/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "core/mapping/mapping.h"
#include "legate.h"
#include "scoping_cffi.h"

namespace scoping {

static const char* const library_name = "scoping";

Legion::Logger log_scoping(library_name);

template <typename T, int ID>
struct Task : public legate::LegateTask<T> {
  static constexpr int TASK_ID = ID;
};

namespace {

void validate(legate::TaskContext context)
{
  if (context.is_single_task()) return;

  int32_t num_tasks = context.get_launch_domain().get_volume();
  auto to_compare   = context.scalars().at(0).value<int32_t>();
  if (to_compare != num_tasks) {
    log_scoping.error("Test failed: expected %d tasks, but got %d tasks", to_compare, num_tasks);
    LEGATE_ABORT;
  }
}

void map_check(legate::TaskContext& context)
{
  int32_t task_count          = context.get_launch_domain().get_volume();
  int32_t shard_id            = legate::Processor::get_executing_processor().address_space();
  int32_t task_id             = context.get_task_index()[0];
  int32_t per_node_count      = context.scalars().at(0).value<int32_t>();
  int32_t proc_count          = context.scalars().at(1).value<int32_t>();
  int32_t start_proc_id       = context.scalars().at(2).value<int32_t>();
  int32_t global_proc_id      = task_id * proc_count / task_count + start_proc_id;
  int32_t calculated_shard_id = global_proc_id / per_node_count;
  if (shard_id != calculated_shard_id) {
    log_scoping.error(
      "Test failed: expected %d shard, but got %d shard", shard_id, calculated_shard_id);
    LEGATE_ABORT;
  }
}

}  // namespace

class MultiVariantTask : public Task<MultiVariantTask, MULTI_VARIANT> {
 public:
  static void cpu_variant(legate::TaskContext context) { validate(context); }
#if LegateDefined(USE_OPENMP)
  static void omp_variant(legate::TaskContext context) { validate(context); }
#endif
#if LegateDefined(USE_CUDA)
  static void gpu_variant(legate::TaskContext context) { validate(context); }
#endif
};

class CpuVariantOnlyTask : public Task<CpuVariantOnlyTask, CPU_VARIANT_ONLY> {
 public:
  static void cpu_variant(legate::TaskContext context) { validate(context); }
};

class MapCheckTask : public Task<MapCheckTask, MAP_CHECK> {
 public:
  static void cpu_variant(legate::TaskContext& context) { map_check(context); }
#ifdef LEGATE_USE_OPENMP
  static void omp_variant(legate::TaskContext& context) { map_check(context); }
#endif
#ifdef LEGATE_USE_CUDA
  static void gpu_variant(legate::TaskContext& context) { map_check(context); }
#endif
};

void registration_callback()
{
  auto context = legate::Runtime::get_runtime()->create_library(library_name);

  MultiVariantTask::register_variants(context);
  CpuVariantOnlyTask::register_variants(context);
  MapCheckTask::register_variants(context);
}

}  // namespace scoping

extern "C" {

void perform_registration(void)
{
  legate::Core::perform_registration<scoping::registration_callback>();
}
}
