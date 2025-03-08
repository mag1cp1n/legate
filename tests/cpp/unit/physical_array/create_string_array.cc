/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <legate.h>

#include <gtest/gtest.h>

#include <utilities/utilities.h>

namespace physical_array_create_string_test {

namespace {

class StringArrayTask : public legate::LegateTask<StringArrayTask> {
 public:
  static inline const auto TASK_CONFIG =  // NOLINT(cert-err58-cpp)
    legate::TaskConfig{legate::LocalTaskID{0}};

  static constexpr auto CPU_VARIANT_OPTIONS = legate::VariantOptions{}.with_has_allocations(true);

  static void cpu_variant(legate::TaskContext);
};

class Config {
 public:
  static constexpr std::string_view LIBRARY_NAME = "test_create_string_physical_array";

  static void registration_callback(legate::Library library)
  {
    StringArrayTask::register_variants(library);
  }
};

class CreateStringPhysicalArrayUnit : public RegisterOnceFixture<Config> {};

class NullableCreateStringArrayTest : public CreateStringPhysicalArrayUnit,
                                      public ::testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(CreateStringPhysicalArrayUnit,
                         NullableCreateStringArrayTest,
                         ::testing::Values(true, false));

void test_array_data(legate::PhysicalStore& store,
                     bool is_unbound,
                     legate::Type::Code code,
                     std::int32_t dim)
{
  ASSERT_EQ(store.is_unbound_store(), is_unbound);
  ASSERT_EQ(store.dim(), dim);
  ASSERT_EQ(store.type().code(), code);
  if (is_unbound) {
    ASSERT_THROW(static_cast<void>(store.shape<1>()), std::invalid_argument);
    ASSERT_THROW(static_cast<void>(store.domain()), std::invalid_argument);
  }
}

/*static*/ void StringArrayTask::cpu_variant(legate::TaskContext context)
{
  auto array                        = context.output(0);
  auto nullable                     = context.scalar(0).value<bool>();
  auto unbound                      = context.scalar(1).value<bool>();
  auto string_array                 = array.as_string_array();
  auto ranges_store                 = string_array.ranges().data();
  auto chars_store                  = string_array.chars().data();
  static constexpr std::int32_t DIM = 1;

  ASSERT_NO_THROW(static_cast<void>(
    chars_store.create_output_buffer<std::int8_t, DIM>(legate::Point<DIM>{10}, true)));
  if (unbound) {
    ASSERT_NO_THROW(ranges_store.bind_empty_data());
  }

  ASSERT_EQ(array.nullable(), nullable);
  ASSERT_EQ(array.dim(), DIM);
  ASSERT_EQ(array.type(), legate::string_type());
  ASSERT_TRUE(array.nested());
  if (unbound) {
    ASSERT_THROW(static_cast<void>(array.shape<DIM>()), std::invalid_argument);
    ASSERT_THROW(static_cast<void>(array.domain()), std::invalid_argument);
  }

  if (nullable) {
    auto null_mask = array.null_mask();

    if (null_mask.is_unbound_store()) {
      ASSERT_NO_THROW(null_mask.bind_empty_data());
      ASSERT_THROW(static_cast<void>(null_mask.shape<DIM>()), std::invalid_argument);
      ASSERT_THROW(static_cast<void>(null_mask.domain()), std::invalid_argument);
    }
    ASSERT_EQ(null_mask.type(), legate::bool_());
    ASSERT_EQ(null_mask.dim(), array.dim());
  } else {
    ASSERT_THROW(static_cast<void>(array.null_mask()), std::invalid_argument);
  }

  test_array_data(ranges_store, unbound, legate::Type::Code::STRUCT, DIM);
  test_array_data(chars_store, true, legate::Type::Code::INT8, DIM);

  auto ranges = array.child(0).data();
  auto chars  = array.child(1).data();

  test_array_data(ranges, unbound, legate::Type::Code::STRUCT, DIM);
  test_array_data(chars, true, legate::Type::Code::INT8, DIM);

  // cast to ListArray
  auto list_array       = array.as_list_array();
  auto descriptor_store = list_array.descriptor().data();
  auto vardata_store    = list_array.vardata().data();

  test_array_data(descriptor_store, unbound, legate::Type::Code::STRUCT, DIM);
  test_array_data(vardata_store, true, legate::Type::Code::INT8, DIM);

  ASSERT_THROW(static_cast<void>(array.child(2)), std::out_of_range);
  ASSERT_THROW(static_cast<void>(array.child(-1)), std::out_of_range);
}

void test_create_string_array_task(legate::LogicalArray& logical_array, bool nullable, bool unbound)
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(Config::LIBRARY_NAME);
  auto task    = runtime->create_task(context, StringArrayTask::TASK_CONFIG.task_id());
  auto part    = task.declare_partition();

  task.add_output(logical_array, std::move(part));
  task.add_scalar_arg(legate::Scalar{nullable});
  task.add_scalar_arg(legate::Scalar{unbound});
  runtime->submit(std::move(task));
}

}  // namespace

TEST_P(NullableCreateStringArrayTest, BoundStringArray)
{
  const auto nullable                       = GetParam();
  auto runtime                              = legate::Runtime::get_runtime();
  static constexpr std::int32_t SHAPE_BOUND = 5;
  auto logical_array = runtime->create_array({SHAPE_BOUND}, legate::string_type(), nullable);

  test_create_string_array_task(logical_array, nullable, false);
}

TEST_P(NullableCreateStringArrayTest, UnboundStringArray)
{
  const auto nullable = GetParam();
  auto runtime        = legate::Runtime::get_runtime();
  auto logical_array  = runtime->create_array(legate::string_type(), 1, nullable);

  test_create_string_array_task(logical_array, nullable, true);
}

}  // namespace physical_array_create_string_test
