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

#include <gtest/gtest.h>

#include "legate.h"
#include "tasks/task_simple.h"

namespace multiscalarout {

void test_writer_auto(legate::Library library,
                      legate::LogicalStore scalar1,
                      legate::LogicalStore scalar2)
{
  auto runtime = legate::Runtime::get_runtime();
  auto task    = runtime->create_task(library, task::simple::WRITER);
  auto part1   = task.declare_partition();
  auto part2   = task.declare_partition();
  task.add_output(scalar1, part1);
  task.add_output(scalar2, part2);
  runtime->submit(std::move(task));
}

void test_reducer_auto(legate::Library library,
                       legate::LogicalStore scalar1,
                       legate::LogicalStore scalar2,
                       legate::LogicalStore store)
{
  auto runtime = legate::Runtime::get_runtime();
  auto task    = runtime->create_task(library, task::simple::REDUCER);
  auto part1   = task.declare_partition();
  auto part2   = task.declare_partition();
  auto part3   = task.declare_partition();
  task.add_reduction(scalar1, legate::ReductionOpKind::ADD, part1);
  task.add_reduction(scalar2, legate::ReductionOpKind::MUL, part2);
  task.add_output(store, part3);
  runtime->submit(std::move(task));
}

void test_reducer_manual(legate::Library library,
                         legate::LogicalStore scalar1,
                         legate::LogicalStore scalar2)
{
  auto runtime = legate::Runtime::get_runtime();
  auto task    = runtime->create_task(library, task::simple::REDUCER, legate::Shape({2}));
  task.add_reduction(scalar1, legate::ReductionOpKind::ADD);
  task.add_reduction(scalar2, legate::ReductionOpKind::MUL);
  runtime->submit(std::move(task));
}

void print_stores(legate::LogicalStore scalar1, legate::LogicalStore scalar2)
{
  auto runtime   = legate::Runtime::get_runtime();
  auto p_scalar1 = scalar1.get_physical_store();
  auto p_scalar2 = scalar2.get_physical_store();
  auto acc1      = p_scalar1.read_accessor<int8_t, 2>();
  auto acc2      = p_scalar2.read_accessor<int32_t, 3>();
  std::stringstream ss;
  ss << static_cast<int32_t>(acc1[{0, 0}]) << " " << acc2[{0, 0, 0}];
  task::simple::logger.print() << ss.str();
}

TEST(Integration, MultiScalarOut)
{
  legate::Core::perform_registration<task::simple::register_tasks>();

  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->find_library(task::simple::library_name);

  auto scalar1 = runtime->create_store({1, 1}, legate::int8(), true);
  auto scalar2 = runtime->create_store({1, 1, 1}, legate::int32(), true);
  auto store   = runtime->create_store({10}, legate::int64());
  test_writer_auto(library, scalar1, scalar2);
  print_stores(scalar1, scalar2);
  test_reducer_auto(library, scalar1, scalar2, store);
  print_stores(scalar1, scalar2);
  test_reducer_manual(library, scalar1, scalar2);
  print_stores(scalar1, scalar2);
}

}  // namespace multiscalarout
