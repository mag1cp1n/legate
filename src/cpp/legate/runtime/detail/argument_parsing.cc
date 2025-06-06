/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <legate/runtime/detail/argument_parsing.h>

#include <legate_defines.h>

#include <legate/cuda/detail/cuda_driver_api.h>
#include <legate/mapping/detail/base_mapper.h>
#include <legate/runtime/detail/argument_parsing/logging.h>
#include <legate/runtime/detail/argument_parsing/util.h>
#include <legate/runtime/detail/config.h>
#include <legate/runtime/runtime.h>
#include <legate/utilities/detail/env.h>
#include <legate/utilities/detail/traced_exception.h>
#include <legate/utilities/detail/zstring_view.h>
#include <legate/utilities/macros.h>
#include <legate/version.h>

#include <legion.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace legate::detail {

namespace {

constexpr std::int64_t MB = 1 << 20;

// Simple wrapper for variables with default values
template <typename T>
class ScaledVar {
 public:
  template <typename U>
  explicit ScaledVar(U default_val, T scale = T{1});

  using value_type = T;

  [[nodiscard]] T raw_value() const;
  [[nodiscard]] T value() const;
  [[nodiscard]] constexpr T default_value() const;
  [[nodiscard]] T& ref();
  void set(T value);

 private:
  static constexpr T UNSET{std::numeric_limits<T>::max()};

  T default_value_{};
  T scale_{};
  T value_{UNSET};
};

template <typename T>
template <typename U>
ScaledVar<T>::ScaledVar(U default_val, T scale) : default_value_{default_val}, scale_{scale}
{
  static_assert(std::is_same_v<T, U>);
}

template <typename T>
T ScaledVar<T>::raw_value() const
{
  return (value_ == UNSET ? default_value() : value_);
}

template <typename T>
T ScaledVar<T>::value() const
{
  return raw_value() * scale_;
}

template <typename T>
constexpr T ScaledVar<T>::default_value() const
{
  return default_value_;
}

template <typename T>
T& ScaledVar<T>::ref()
{
  return value_;
}

template <typename T>
void ScaledVar<T>::set(T value)
{
  value_ = std::move(value);
}

// ==========================================================================================

template <typename T>
class Arg {
 public:
  Arg() = delete;
  explicit Arg(std::string_view flag);
  explicit Arg(std::string_view flag, T init);

  using value_type = T;

  std::string_view flag{};
  T value{};
};

template <typename T>
Arg<T>::Arg(std::string_view f) : flag{f}
{
}

template <typename T>
Arg<T>::Arg(std::string_view f, T init) : flag{f}, value{std::move(init)}
{
}

// ==========================================================================================

[[nodiscard]] std::filesystem::path normalize_log_dir(std::string log_dir)
{
  namespace fs = std::filesystem;

  auto log_path = fs::path{std::move(log_dir)};

  if (log_path.empty()) {
    log_path = fs::current_path();  // cwd
  }
  return log_path;
}

// ==========================================================================================

template <typename T>
void try_set_property(Realm::Runtime* runtime,
                      const std::string& module_name,
                      const std::string& property_name,
                      const argparse::ArgumentParser& parser,
                      const Arg<ScaledVar<T>>& var)
{
  const auto value = var.value.value();

  LEGATE_CHECK(value >= 0);

  auto* const config = runtime->get_module_config(module_name);

  if (nullptr == config) {
    // If the variable doesn't have a value, we don't care if the module is nonexistent
    if (!parser.is_used(var.flag)) {
      return;
    }

    throw TracedException<ConfigurationError>{
      fmt::format("Unable to set {} (the {} module is not available).", var.flag, module_name)};
  }

  if (!config->set_property(property_name, value)) {
    throw TracedException<ConfigurationError>{fmt::format("Unable to set {}.", var.flag)};
  }
}

// ==========================================================================================

constexpr std::int64_t MINIMAL_MEM = 256;  // MB
constexpr double SYSMEM_FRACTION   = 0.8;

void autoconfigure_gpus(const Realm::ModuleConfig* cuda, ScaledVar<std::int32_t>* gpus)
{
  if (gpus->value() >= 0) {
    return;
  }

  std::decay_t<decltype(*gpus)>::value_type auto_gpus = 0;

  if (Config::get_config().auto_config() && cuda != nullptr) {
    // use all available GPUs
    if (!cuda->get_resource("gpu", auto_gpus)) {
      throw TracedException<AutoConfigurationError>{
        "CUDA Realm module could not determine the number of GPUs."};
    }
  }  // otherwise don't allocate any GPUs
  gpus->set(auto_gpus);
}

void autoconfigure_fbmem(const Realm::ModuleConfig* cuda,
                         std::int32_t gpus,
                         ScaledVar<std::int64_t>* fbmem)
{
  if (fbmem->value() >= 0) {
    return;
  }

  if (gpus <= 0) {
    fbmem->set(0);
    return;
  }

  if (Config::get_config().auto_config() && cuda) {
    constexpr double FBMEM_FRACTION = 0.95;
    std::size_t res_fbmem           = 0;

    auto&& driver = cuda::detail::get_cuda_driver_api();
    // Currently, we assume all GPUs are identical, so we can just use the first one
    {
      legate::cuda::detail::AutoPrimaryContext const ctx{0};
      std::tie(res_fbmem, std::ignore) = driver->mem_get_info();
    }

    using T = std::decay_t<decltype(*fbmem)>::value_type;

    const auto auto_fbmem =
      static_cast<T>(std::floor(FBMEM_FRACTION * static_cast<double>(res_fbmem) / MB));

    fbmem->set(auto_fbmem);
  } else {
    fbmem->set(MINIMAL_MEM);
  }
}

void autoconfigure_omps(const Realm::ModuleConfig* openmp,
                        const std::vector<std::size_t>& numa_mems,
                        std::int32_t gpus,
                        ScaledVar<std::int32_t>* omps)
{
  if (omps->value() >= 0) {
    return;
  }

  using T = std::decay_t<decltype(*omps)>::value_type;

  const auto auto_omps = [&]() -> T {
    if (!Config::get_config().auto_config() || !openmp) {
      return 0;  // don't allocate any OpenMP groups
    }
    if (gpus > 0) {
      // match the number of GPUs, to ensure host offloading does not repartition
      return gpus;
    }
    // create one OpenMP group per NUMA node (or a single group, if no NUMA info is available)
    return std::max(static_cast<T>(numa_mems.size()), T{1});
  }();

  omps->set(auto_omps);
}

void autoconfigure_numamem(const std::vector<std::size_t>& numa_mems,
                           std::int32_t omps,
                           ScaledVar<std::int64_t>* numamem)
{
  if (numamem->value() >= 0) {
    return;
  }

  if (omps <= 0 || numa_mems.empty() || omps % numa_mems.size() != 0) {
    numamem->set(0);
    return;
  }

  if (!Config::get_config().auto_config()) {
    numamem->set(MINIMAL_MEM);
    return;
  }

  // TODO(mpapadakis): Assuming that all NUMA domains have the same size
  const auto numa_mem_size = numa_mems.front();
  const auto num_numa_mems = numa_mems.size();
  const auto omps_per_numa = (omps + num_numa_mems - 1) / num_numa_mems;
  using T                  = std::decay_t<decltype(*numamem)>::value_type;
  const auto auto_numamem =
    static_cast<T>(std::floor(SYSMEM_FRACTION * static_cast<double>(numa_mem_size) / MB /
                              static_cast<double>(omps_per_numa)));

  numamem->set(auto_numamem);
}

void autoconfigure_cpus(const Realm::ModuleConfig* core,
                        std::int32_t omps,
                        std::int32_t util,
                        std::int32_t gpus,
                        ScaledVar<std::int32_t>* cpus)
{
  if (cpus->value() >= 0) {
    return;
  }

  if (!Config::get_config().auto_config() || (omps > 0)) {
    // leave one core available for profiling meta-tasks, and other random uses
    cpus->set(1);
    return;
  }

  if (gpus > 0) {
    // match the number of GPUs, to ensure host offloading does not repartition
    cpus->set(gpus);
    return;
  }

  // use all unallocated cores
  int res_num_cpus{};

  if (!core->get_resource("cpu", res_num_cpus)) {
    throw TracedException<AutoConfigurationError>{
      "Core Realm module could not determine the number of CPU cores."};
  }
  if (res_num_cpus == 0) {
    throw TracedException<AutoConfigurationError>{
      "Core Realm module detected 0 CPU cores while configuring CPUs."};
  }

  using T = std::decay_t<decltype(*cpus)>::value_type;

  const T auto_cpus = res_num_cpus - util - gpus;

  if (auto_cpus <= 0) {
    throw TracedException<AutoConfigurationError>{
      fmt::format("No CPU cores left to allocate to CPU processors. Have {}, but need {} for "
                  "utility processors, and {} for GPU processors.",
                  res_num_cpus,
                  util,
                  gpus)};
  }
  cpus->set(auto_cpus);
}

void autoconfigure_sysmem(const Realm::ModuleConfig* core,
                          std::int64_t numamem,
                          ScaledVar<std::int64_t>* sysmem)
{
  if (sysmem->value() >= 0) {
    return;
  }

  if (!Config::get_config().auto_config() || (numamem > 0)) {
    // don't allocate much memory to --sysmem; leave most to be used for --numamem
    sysmem->set(MINIMAL_MEM);
    return;
  }

  using T = std::decay_t<decltype(*sysmem)>::value_type;

  std::size_t res_sysmem_size{};

  if (!core->get_resource("sysmem", res_sysmem_size)) {
    throw TracedException<AutoConfigurationError>{
      "Core Realm module could not determine the available system memory."};
  }

  const auto auto_sysmem =
    static_cast<T>(std::floor(SYSMEM_FRACTION * static_cast<double>(res_sysmem_size) / MB));

  sysmem->set(auto_sysmem);
}

void autoconfigure_ompthreads(const Realm::ModuleConfig* core,
                              std::int32_t util,
                              std::int32_t cpus,
                              std::int32_t gpus,
                              std::int32_t omps,
                              ScaledVar<std::int32_t>* ompthreads)
{
  if (ompthreads->value() >= 0) {
    return;
  }

  if (omps <= 0) {
    ompthreads->set(0);
    return;
  }

  if (!Config::get_config().auto_config()) {
    ompthreads->set(1);
    return;
  }

  using T = std::decay_t<decltype(*ompthreads)>::value_type;

  int res_num_cpus{};

  if (!core->get_resource("cpu", res_num_cpus)) {
    throw TracedException<AutoConfigurationError>{
      "Core Realm module could not determine the number of CPU cores."};
  }
  if (res_num_cpus == 0) {
    throw TracedException<AutoConfigurationError>{
      "Core Realm module detected 0 CPU cores while configuring the number of OpenMP threads."};
  }

  const auto auto_ompthreads =
    static_cast<T>(std::floor((res_num_cpus - cpus - util - gpus) / omps));

  if (auto_ompthreads <= 0) {
    throw TracedException<AutoConfigurationError>{
      fmt::format("Not enough CPU cores to split across {} OpenMP processor(s). Have {}, but need "
                  "{} for CPU processors, {} for utility processors, {} for GPU processors, and at "
                  "least {} for OpenMP processors (1 core each).",
                  omps,
                  res_num_cpus,
                  cpus,
                  util,
                  gpus,
                  omps)};
  }
  ompthreads->set(auto_ompthreads);
}

void autoconfigure(Realm::Runtime* rt,
                   Arg<ScaledVar<std::int32_t>>* util,
                   Arg<ScaledVar<std::int32_t>>* cpus,
                   Arg<ScaledVar<std::int32_t>>* gpus,
                   Arg<ScaledVar<std::int32_t>>* omps,
                   Arg<ScaledVar<std::int32_t>>* ompthreads,
                   Arg<ScaledVar<std::int64_t>>* sysmem,
                   Arg<ScaledVar<std::int64_t>>* fbmem,
                   Arg<ScaledVar<std::int64_t>>* numamem)
{
  const auto* core = rt->get_module_config("core");

  // ensure core module
  LEGATE_CHECK(core != nullptr);

  const auto* cuda   = rt->get_module_config("cuda");
  const auto* openmp = rt->get_module_config("openmp");
  const auto* numa   = rt->get_module_config("numa");
  std::vector<std::size_t> numa_mems{};
  if (numa && !numa->get_resource("numa_mems", numa_mems)) {
    numa_mems.clear();
  }

  // auto-configure --gpus
  autoconfigure_gpus(cuda, &gpus->value);

  // auto-configure --fbmem
  autoconfigure_fbmem(cuda, gpus->value.value(), &fbmem->value);

  // auto-configure --omps
  autoconfigure_omps(openmp, numa_mems, gpus->value.value(), &omps->value);

  // auto-configure --numamem
  autoconfigure_numamem(numa_mems, omps->value.value(), &numamem->value);

  // auto-configure --cpus
  autoconfigure_cpus(
    core, omps->value.value(), util->value.value(), gpus->value.value(), &cpus->value);

  // auto-configure --sysmem
  autoconfigure_sysmem(core, numamem->value.value(), &sysmem->value);

  // auto-configure --ompthreads
  autoconfigure_ompthreads(core,
                           util->value.value(),
                           cpus->value.value(),
                           gpus->value.value(),
                           omps->value.value(),
                           &ompthreads->value);
}

// ==========================================================================================

void set_core_config_properties(Realm::Runtime* rt,
                                const argparse::ArgumentParser& parser,
                                const Arg<ScaledVar<std::int32_t>>& cpus,
                                const Arg<ScaledVar<std::int32_t>>& util,
                                const Arg<ScaledVar<std::int64_t>>& sysmem,
                                const Arg<ScaledVar<std::int64_t>>& regmem)
{
  constexpr std::size_t SYSMEM_LIMIT_FOR_IPC_REG = 1024 * MB;

  try_set_property(rt, "core", "cpu", parser, cpus);
  try_set_property(rt, "core", "util", parser, util);
  try_set_property(rt, "core", "sysmem", parser, sysmem);
  try_set_property(rt, "core", "regmem", parser, regmem);

  // Don't register sysmem for intra-node IPC if it's above a certain size, as it can take forever.
  auto* const config = rt->get_module_config("core");
  LEGATE_CHECK(config != nullptr);
  static_cast<void>(config->set_property("sysmem_ipc_limit", SYSMEM_LIMIT_FOR_IPC_REG));
}

void set_cuda_config_properties(Realm::Runtime* rt,
                                const argparse::ArgumentParser& parser,
                                const Arg<ScaledVar<std::int32_t>>& gpus,
                                const Arg<ScaledVar<std::int64_t>>& fbmem,
                                const Arg<ScaledVar<std::int64_t>>& zcmem)
{
  try {
    try_set_property(rt, "cuda", "gpu", parser, gpus);
    try_set_property(rt, "cuda", "fbmem", parser, fbmem);
    try_set_property(rt, "cuda", "zcmem", parser, zcmem);
  } catch (...) {
    // If we have CUDA, but failed above, then rethrow, otherwise silently gobble the error
    if (LEGATE_DEFINED(LEGATE_USE_CUDA)) {
      throw;
    }
  }
  if (gpus.value.value() > 0) {
    Config::get_config_mut().set_need_cuda(true);
  }
}

void set_openmp_config_properties(Realm::Runtime* rt,
                                  const argparse::ArgumentParser& parser,
                                  const Arg<ScaledVar<std::int32_t>>& omps,
                                  const Arg<ScaledVar<std::int32_t>>& ompthreads,
                                  const Arg<ScaledVar<std::int64_t>>& numamem)
{
  if (omps.value.value() > 0) {
    const auto num_threads = ompthreads.value.value();

    LEGATE_CHECK(num_threads > 0);

    auto& config = Config::get_config_mut();

    config.set_need_openmp(true);
    config.set_num_omp_threads(num_threads);
  }
  try {
    try_set_property(rt, "openmp", "ocpu", parser, omps);
    try_set_property(rt, "openmp", "othr", parser, ompthreads);
    try_set_property(rt, "numa", "numamem", parser, numamem);
  } catch (...) {
    // If we have OpenMP, but failed above, then rethrow, otherwise silently gobble the error
    if (LEGATE_DEFINED(LEGATE_USE_OPENMP)) {
      throw;
    }
  }
}

void set_legion_default_args(std::string log_dir,
                             std::string log_levels,
                             bool profile,
                             bool spy,
                             bool freeze_on_error,
                             bool log_to_file,
                             std::int32_t omps,
                             std::int64_t numamem)
{
  auto log_path = normalize_log_dir(std::move(log_dir));

  std::stringstream args_ss;

  // some values have to be passed via env var
  args_ss << "-lg:local 0 ";

  if (omps >= 1 && numamem <= 0) {
    // Realm will try to allocate OpenMP groups in a NUMA-aligned way, even if NUMA detection
    // failed (in which case the auto-configuration system set --numamem 0), resulting in a warning.
    // Just tell it to not bother, so we suppress the warning.
    // Technically speaking it might be useful to enable NUMA-aligned OpenMP group instantiation
    // in cases where NUMA is available, but we're explicitly requesting no NUMA-aligned memory,
    // i.e. the user set --numamem 0.
    args_ss << "-ll:onuma 0 ";
  }

  const auto add_logger = [&](std::string_view logger, std::string_view level = "info") {
    if (!log_levels.empty()) {
      log_levels += ',';
    }
    log_levels += logger;
    log_levels += '=';
    log_levels += level;
  };

  LEGATE_ASSERT(Config::get_config().parsed());
  if (Config::get_config().log_mapping_decisions()) {
    add_logger(mapping::detail::BaseMapper::LOGGER_NAME);
  }
  if (Config::get_config().log_partitioning_decisions()) {
    add_logger(log_legate_partitioner().get_name(), "debug");
  }

  if (spy) {
    if (!log_to_file && !log_levels.empty()) {
      // Spy output is dumped to the same place as other logging, so we must redirect all logging to
      // a file, even if the user didn't ask for it.
      // NOLINTNEXTLINE(performance-avoid-endl) endl is deliberate, we want a newline and flush
      std::cout << "Logging output is being redirected to a file in --logdir" << std::endl;
    }
    args_ss << "-lg:spy ";
    add_logger("legion_spy");
    log_to_file = true;
  }

  // Do this after the --spy w/o --logdir check above, as the logging level legion_prof=2 doesn't
  // actually print anything to the logs, so don't consider that a conflict.
  if (profile) {
    args_ss << "-lg:prof 1 -lg:prof_logfile " << log_path / "legate_%.prof" << " ";
    add_logger("legion_prof");
  }

  if (freeze_on_error) {
    constexpr detail::EnvironmentVariable<std::uint32_t> LEGION_FREEZE_ON_ERROR{
      "LEGION_FREEZE_ON_ERROR"};

    LEGION_FREEZE_ON_ERROR.set(1);
    args_ss << "-ll:force_kthreads ";
  }

  if (!log_levels.empty()) {
    args_ss << "-level " << convert_log_levels(log_levels) << " ";
  }

  if (log_to_file) {
    args_ss << "-logfile " << log_path / "legate_%.log" << " -errlevel 4 ";
  }

  if (LEGATE_DEFINED(LEGATE_HAS_ASAN)) {
    // TODO (wonchanl, jfaibussowit) Sanitizers can raise false alarms if the code does
    // user-level threading, so we turn it off for sanitizer-enabled tests
    args_ss << "-ll:force_kthreads ";
  }

  if (const auto existing_default_args = LEGION_DEFAULT_ARGS.get();
      existing_default_args.has_value()) {
    args_ss << *existing_default_args;
  }

  LEGION_DEFAULT_ARGS.set(args_ss.str());
}

// ==========================================================================================

template <typename T>
void add_argument_base(argparse::ArgumentParser* parser,
                       std::string_view flag,
                       std::string help,
                       bool hidden,
                       const T* default_val,
                       T* val)
{
  using RawT = std::decay_t<T>;
  auto& parg = parser->add_argument(flag);

  parg.help(std::move(help));
  if (default_val) {
    parg.default_value(*default_val);
  }
  if (hidden) {
    parg.hidden();
  }
  parg.store_into(*val);
  if constexpr (std::is_same_v<RawT, bool>) {
    parg.metavar("BOOL");
  } else {
    parg.nargs(1);
    if constexpr (std::is_integral_v<RawT>) {
      parg.metavar("INT");
    } else if constexpr (std::is_same_v<RawT, std::string>) {
      parg.metavar("STRING");
    }
  }
}

template <typename T>
[[nodiscard]] Arg<ScaledVar<T>> add_argument(argparse::ArgumentParser* parser,
                                             std::string_view flag,
                                             std::string help,
                                             ScaledVar<T> val,
                                             bool hidden = false)
{
  auto arg                = Arg<ScaledVar<T>>{flag, std::move(val)};
  const auto& default_val = arg.value.default_value();

  add_argument_base(parser, flag, std::move(help), hidden, &default_val, &arg.value.ref());
  return arg;
}

template <typename T>
[[nodiscard]] Arg<T> add_argument(argparse::ArgumentParser* parser,
                                  std::string_view flag,
                                  std::string help,
                                  T val,
                                  bool hidden = false)
{
  auto arg = Arg<T>{flag, std::move(val)};

  add_argument_base(parser, flag, std::move(help), hidden, &arg.value, &arg.value);
  return arg;
}

template <typename T>
[[nodiscard]] Arg<T> add_argument(argparse::ArgumentParser* parser,
                                  std::string_view flag,
                                  std::string help,
                                  bool hidden = false)
{
  auto arg = Arg<T>{flag};

  add_argument_base(
    parser, flag, std::move(help), hidden, static_cast<const T*>(nullptr), &arg.value);
  return arg;
}

}  // namespace

void handle_legate_args()
{
  // values with -1 defaults will be auto-configured via the Realm API
  constexpr std::int32_t DEFAULT_CPUS       = -1;
  constexpr std::int32_t DEFAULT_GPUS       = -1;
  constexpr std::int32_t DEFAULT_OMPS       = -1;
  constexpr std::int32_t DEFAULT_OMPTHREADS = -1;
  constexpr std::int32_t DEFAULT_UTILITY    = 2;
  constexpr std::int64_t DEFAULT_SYSMEM     = -1;
  constexpr std::int64_t DEFAULT_NUMAMEM    = -1;
  constexpr std::int64_t DEFAULT_FBMEM      = -1;
  constexpr std::int64_t DEFAULT_ZCMEM      = 128;  // MB
  constexpr std::int64_t DEFAULT_REGMEM     = 0;    // MB
  const auto test                           = LEGATE_TEST.get(/* defualt_value */ false);
  const auto default_or_test                = [&](auto default_val, auto test_val) {
    return test ? test_val : default_val;
  };

  auto parser = argparse::ArgumentParser{
    "LEGATE_CONFIG can contain:",
    std::string{LEGATE_STRINGIZE(LEGATE_VERSION_MAJOR) "." LEGATE_STRINGIZE(
      LEGATE_VERSION_MINOR) "." LEGATE_STRINGIZE(LEGATE_VERSION_PATCH)}};

  auto auto_config = add_argument(&parser,
                                  "--auto-config",
                                  "Automatically detect a suitable configuration. This attempts to "
                                  "detect a reasonable default for most options listed hereafter "
                                  "and is recommended for most users.",
                                  true);
  auto show_config =
    add_argument(&parser, "--show-config", "Print the configuration to stdout.", false);
  auto show_progress = add_argument(
    &parser, "--show-progress", "Print a progress summary before each task is executed.", false);
  auto empty_task =
    add_argument(&parser,
                 "--use-empty-task",
                 "Execute an empty dummy task in place of each task execution. This "
                 "is primarily a developer feature for use in debugging runtime or "
                 "scheduling inconsistencies and is not recommended for external use.",
                 false);
  auto warmup_nccl = add_argument(
    &parser,
    "--warmup-nccl",
    "Perform a warmup for NCCL on startup. This is useful when doing performance benchmarks.",
    false);
  auto inline_task_launch = add_argument(&parser,
                                         "--inline-task-launch",
                                         "Enable inline task launch",
                                         false,
                                         /* hidden */ true);
  auto max_exception_size =
    add_argument(&parser,
                 "--max-exception-size",
                 "Maximum size (in bytes) to allocate for exception messages.",
                 default_or_test(std::size_t{LEGATE_MAX_EXCEPTION_SIZE_DEFAULT},
                                 std::size_t{LEGATE_MAX_EXCEPTION_SIZE_TEST}));
  auto min_cpu_chunk = add_argument(&parser,
                                    "--min-cpu-chunk",
                                    "Minimum CPU chunk size (in bytes).",
                                    default_or_test(std::size_t{LEGATE_MIN_CPU_CHUNK_DEFAULT},
                                                    std::size_t{LEGATE_MIN_CPU_CHUNK_TEST}));
  auto min_gpu_chunk = add_argument(&parser,
                                    "--min-gpu-chunk",
                                    "Minimum GPU chunk size (in bytes).",
                                    default_or_test(std::size_t{LEGATE_MIN_GPU_CHUNK_DEFAULT},
                                                    std::size_t{LEGATE_MIN_GPU_CHUNK_TEST}));
  auto min_omp_chunk = add_argument(&parser,
                                    "--min-omp-chunk",
                                    "Minimum OpenMP chunk size (in bytes).",
                                    default_or_test(std::size_t{LEGATE_MIN_OMP_CHUNK_DEFAULT},
                                                    std::size_t{LEGATE_MIN_OMP_CHUNK_TEST}));
  auto window_size =
    add_argument(&parser,
                 "--window-size",
                 "Maximum size of the submitted operation queue before forced flush.",
                 default_or_test(std::uint32_t{LEGATE_WINDOW_SIZE_DEFAULT},
                                 std::uint32_t{LEGATE_WINDOW_SIZE_TEST}));
  auto field_reuse_frac =
    add_argument(&parser,
                 "--field-reuse-fraction",
                 "Field reuse fraction",
                 default_or_test(std::uint32_t{LEGATE_FIELD_REUSE_FRAC_DEFAULT},
                                 std::uint32_t{LEGATE_FIELD_REUSE_FRAC_TEST}));
  auto field_reuse_freq =
    add_argument(&parser,
                 "--field-reuse-frequency",
                 "Field reuse frequency",
                 default_or_test(std::uint32_t{LEGATE_FIELD_REUSE_FREQ_DEFAULT},
                                 std::uint32_t{LEGATE_FIELD_REUSE_FREQ_TEST}));
  auto consensus =
    add_argument(&parser,
                 "--consensus",
                 "Consensus",
                 default_or_test(bool{LEGATE_CONSENSUS_DEFAULT}, bool{LEGATE_CONSENSUS_TEST}));

  auto cpus = add_argument(&parser,
                           "--cpus",
                           "Number of standalone CPU cores to reserve, must be >=0",
                           ScaledVar<std::int32_t>{DEFAULT_CPUS});
  auto gpus = add_argument(&parser,
                           "--gpus",
                           "Number of GPUs to reserve, must be >=0",
                           ScaledVar<std::int32_t>{DEFAULT_GPUS});
  auto omps = add_argument(&parser,
                           "--omps",
                           "Number of OpenMP groups to use, must be >=0",
                           ScaledVar<std::int32_t>{DEFAULT_OMPS});

  auto ompthreads =
    add_argument(&parser,
                 "--ompthreads",
                 "Number of threads / reserved CPU cores per OpenMP group, must be >=0",
                 ScaledVar<std::int32_t>{DEFAULT_OMPTHREADS});

  auto util   = add_argument(&parser,
                           "--utility",
                           "Number of threads to use for runtime meta-work, must be >=0",
                           ScaledVar<std::int32_t>{DEFAULT_UTILITY});
  auto sysmem = add_argument(&parser,
                             "--sysmem",
                             "Size (in MiB) of DRAM memory to reserve per rank",
                             ScaledVar<std::int64_t>{DEFAULT_SYSMEM, MB});
  auto numamem =
    add_argument(&parser,
                 "--numamem",
                 "Size (in MiB) of NUMA-specific DRAM memory to reserve per NUMA domain",
                 ScaledVar<std::int64_t>{DEFAULT_NUMAMEM, MB});
  auto fbmem = add_argument(&parser,
                            "--fbmem",
                            "Size (in MiB) of GPU (or \"framebuffer\") memory to reserve per GPU",
                            ScaledVar<std::int64_t>{DEFAULT_FBMEM, MB});
  auto zcmem = add_argument(
    &parser,
    "--zcmem",
    "Size (in MiB) of GPU-registered (or \"zero-copy\") DRAM memory to reserve per GPU",
    ScaledVar<std::int64_t>{DEFAULT_ZCMEM, MB});
  auto regmem = add_argument(&parser,
                             "--regmem",
                             "Size (in MiB) of NIC-registered DRAM memory to reserve",
                             ScaledVar<std::int64_t>{DEFAULT_REGMEM, MB});

  const auto profile =
    add_argument(&parser, "--profile", "Whether to collect profiling logs", false);
  const auto spy =
    add_argument(&parser, "--spy", "Whether to collect dataflow & task graph logs", false);
  auto log_levels = add_argument<std::string>(&parser, "--logging", logging_help_str());
  auto log_dir    = add_argument<std::string>(
    &parser, "--logdir", "Directory to emit logfiles to, defaults to current directory");
  const auto log_to_file = add_argument(
    &parser, "--log-to-file", "Redirect logging output to a file inside --logdir", false);
  const auto freeze_on_error = add_argument(
    &parser,
    "--freeze-on-error",
    "If the program crashes, freeze execution right before exit so a debugger can be attached",
    false);

  const auto legate_config_env = LEGATE_CONFIG.get(/* default_value */ "");
  auto&& args                  = string_split(legate_config_env);

  // Needed to satisfy argparse, which expects an argv-like interface where argv[0] is the
  // program name.
  args.insert(args.begin(), "LEGATE");

  try {
    parser.parse_args(args);
  } catch (const std::exception& exn) {
    std::cerr << "== LEGATE ERROR:\n";
    std::cerr << "== LEGATE ERROR: " << exn.what() << "\n";
    std::cerr << "== LEGATE ERROR:\n";
    std::cerr << parser;
    std::exit(EXIT_FAILURE);
  }

  {
    const auto set_config_value = [&](const auto& flag, auto&& config_mem_function) {
      if (parser.is_used(flag.flag)) {
        (Config::get_config_mut().*config_mem_function)(flag.value);
      }
    };

    set_config_value(auto_config, &Config::set_auto_config);
    set_config_value(show_config, &Config::set_show_config);
    set_config_value(show_progress, &Config::set_show_progress_requested);
    set_config_value(empty_task, &Config::set_use_empty_task);
    set_config_value(warmup_nccl, &Config::set_warmup_nccl);
    set_config_value(inline_task_launch, &Config::set_enable_inline_task_launch);
    set_config_value(max_exception_size, &Config::set_max_exception_size);
    set_config_value(min_cpu_chunk, &Config::set_min_cpu_chunk);
    set_config_value(min_gpu_chunk, &Config::set_min_gpu_chunk);
    set_config_value(min_omp_chunk, &Config::set_min_omp_chunk);
    set_config_value(window_size, &Config::set_window_size);
    set_config_value(field_reuse_frac, &Config::set_field_reuse_frac);
    set_config_value(field_reuse_freq, &Config::set_field_reuse_freq);
    set_config_value(consensus, &Config::set_consensus);
  }

  // Disable MPI in legate if the network bootstrap is p2p
  if (REALM_UCP_BOOTSTRAP_MODE.get() == "p2p") {
    Config::get_config_mut().set_disable_mpi(true);
  }

  auto rt = Realm::Runtime::get_runtime();

  // ensure sensible utility
  util.value.set(std::max(1, util.value.value()));

  autoconfigure(&rt, &util, &cpus, &gpus, &omps, &ompthreads, &sysmem, &fbmem, &numamem);

  if (Config::get_config().show_config()) {
    // Can't use a logger, since Realm hasn't been initialized yet.
    std::cout << "Legate hardware configuration:";
    std::cout << " " << cpus.flag << "=" << cpus.value.raw_value();
    std::cout << " " << gpus.flag << "=" << gpus.value.raw_value();
    std::cout << " " << omps.flag << "=" << omps.value.raw_value();
    std::cout << " " << ompthreads.flag << "=" << ompthreads.value.raw_value();
    std::cout << " " << util.flag << "=" << util.value.raw_value();
    std::cout << " " << sysmem.flag << "=" << sysmem.value.raw_value();
    std::cout << " " << numamem.flag << "=" << numamem.value.raw_value();
    std::cout << " " << fbmem.flag << "=" << fbmem.value.raw_value();
    std::cout << " " << zcmem.flag << "=" << zcmem.value.raw_value();
    std::cout << " " << regmem.flag << "=" << regmem.value.raw_value();
    // Use of endl is deliberate, we want a newline and flush
    std::cout << std::endl;  // NOLINT(performance-avoid-endl)
  }

  // Set core configuration properties
  set_core_config_properties(&rt, parser, cpus, util, sysmem, regmem);

  // Set CUDA configuration properties
  set_cuda_config_properties(&rt, parser, gpus, fbmem, zcmem);

  // Set OpenMP configuration properties
  set_openmp_config_properties(&rt, parser, omps, ompthreads, numamem);

  set_legion_default_args(std::move(log_dir.value),
                          std::move(log_levels.value),
                          profile.value,
                          spy.value,
                          freeze_on_error.value,
                          log_to_file.value,
                          omps.value.value(),
                          numamem.value.value());

  // These config flags are set by the set_*_config_properties calls above, so check them now.
  if (!LEGATE_DEFINED(LEGATE_USE_CUDA) && Config::get_config().need_cuda()) {
    throw TracedException<std::runtime_error>{
      "Legate was run with GPUs but was not built with GPU support. Please "
      "install Legate again with the \"--with-cuda\" flag"};
  }
  if (!LEGATE_DEFINED(LEGATE_USE_OPENMP) && Config::get_config().need_openmp()) {
    throw TracedException<std::runtime_error>{
      "Legate was run with OpenMP enabled, but was not built with OpenMP "
      "support. Please install Legate again with the \"--with-openmp\" "
      "flag"};
  }
}

}  // namespace legate::detail
