/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <legate/data/detail/scalar.h>
#include <legate/mapping/detail/machine.h>
#include <legate/operation/detail/launcher_arg.h>
#include <legate/utilities/detail/core_ids.h>
#include <legate/utilities/detail/zstring_view.h>

#include <memory>
#include <vector>

namespace legate::detail {

class Library;
class Runtime;

class TaskLauncher {
 public:
  TaskLauncher(const Library& library,
               const mapping::detail::Machine& machine,
               ZStringView provenance,
               LocalTaskID task_id,
               Legion::MappingTagID tag = 0);

  TaskLauncher(const Library& library,
               const mapping::detail::Machine& machine,
               LocalTaskID task_id,
               Legion::MappingTagID tag = 0);

  [[nodiscard]] GlobalTaskID legion_task_id() const;

  void add_input(std::unique_ptr<Analyzable> arg);
  void add_output(std::unique_ptr<Analyzable> arg);
  void add_reduction(std::unique_ptr<Analyzable> arg);
  void add_scalar(InternalSharedPtr<Scalar> scalar);

  void add_future(Legion::Future future);
  void add_future_map(Legion::FutureMap future_map);
  void add_communicator(Legion::FutureMap communicator);

  void set_priority(std::int32_t priority);
  void set_side_effect(bool has_side_effect);
  void set_concurrent(bool is_concurrent);
  void set_insert_barrier(bool insert_barrier);
  void set_future_size(std::size_t future_size);
  void throws_exception(bool can_throw_exception);
  void can_elide_device_ctx_sync(bool can_elide_sync);
  void relax_interference_checks(bool relax);

  Legion::FutureMap execute(const Legion::Domain& launch_domain);
  Legion::Future execute_single();
  [[nodiscard]] ZStringView provenance() const;

 private:
  void pack_mapper_arg_(BufferBuilder& buffer);
  void import_output_regions_(Runtime& runtime,
                              const std::vector<Legion::OutputRequirement>& output_requirements);
  void post_process_unbound_stores_(
    const std::vector<Legion::OutputRequirement>& output_requirements);
  void post_process_unbound_stores_(
    const Legion::FutureMap& result,
    const Legion::Domain& launch_domain,
    const std::vector<Legion::OutputRequirement>& output_requirements);

  void report_interfering_stores_() const;
  [[nodiscard]] std::size_t get_future_size_including_exception_() const;

  std::reference_wrapper<const Library> library_;
  LocalTaskID task_id_{};
  Legion::MappingTagID tag_{};
  const mapping::detail::Machine& machine_;
  ZStringView provenance_{};
  std::int32_t priority_{static_cast<std::int32_t>(TaskPriority::DEFAULT)};

  bool has_side_effect_{true};
  bool concurrent_{};
  bool insert_barrier_{};
  bool can_throw_exception_{};
  bool can_elide_device_ctx_sync_{};
  bool relax_interference_checks_{};
  std::size_t future_size_{};

  std::vector<std::unique_ptr<Analyzable>> inputs_{};
  std::vector<std::unique_ptr<Analyzable>> outputs_{};
  std::vector<std::unique_ptr<Analyzable>> reductions_{};
  std::vector<ScalarArg> scalars_{};
  std::vector<Legion::Future> futures_{};
  std::vector<const OutputRegionArg*> unbound_stores_{};
  std::vector<Legion::FutureMap> future_maps_{};
  std::vector<Legion::FutureMap> communicators_{};
};

}  // namespace legate::detail

#include <legate/operation/detail/task_launcher.inl>
