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
#include "utilities/utilities.h"

namespace tunable {

using Tunable = DefaultFixture;

static const char* library_name = "test_tunable";

const std::vector<legate::Scalar> TUNABLES = {
  legate::Scalar{false},
  legate::Scalar{int8_t{12}},
  legate::Scalar{int32_t{456}},
  legate::Scalar{uint16_t{78}},
  legate::Scalar{uint64_t{91011}},
  legate::Scalar{double{123.0}},
  legate::Scalar{complex<float>{10.0F, 20.0F}},
};

class LibraryMapper : public legate::mapping::Mapper {
  void set_machine(const legate::mapping::MachineQueryInterface* machine) override {}
  legate::mapping::TaskTarget task_target(
    const legate::mapping::Task& task,
    const std::vector<legate::mapping::TaskTarget>& options) override
  {
    return options.front();
  }
  std::vector<legate::mapping::StoreMapping> store_mappings(
    const legate::mapping::Task& task,
    const std::vector<legate::mapping::StoreTarget>& options) override
  {
    return {};
  }
  legate::Scalar tunable_value(legate::TunableID tunable_id) override
  {
    return (tunable_id >= 0 && tunable_id < TUNABLES.size()) ? TUNABLES[tunable_id]
                                                             : legate::Scalar{};
  }
};

void prepare()
{
  static bool prepared = false;
  if (prepared) { return; }
  prepared     = true;
  auto runtime = legate::Runtime::get_runtime();
  runtime->create_library(
    library_name, legate::ResourceConfig{}, std::make_unique<LibraryMapper>());
}

struct scalar_eq_fn {
  template <legate::Type::Code CODE>
  bool operator()(const legate::Scalar& lhs, const legate::Scalar& rhs)
  {
    using VAL = legate::legate_type_of<CODE>;
    return lhs.value<VAL>() == rhs.value<VAL>();
  }
};

TEST_F(Tunable, Valid)
{
  prepare();
  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->find_library(library_name);

  int64_t tunable_id = 0;
  for (const auto& to_compare : TUNABLES) {
    auto dtype = to_compare.type();
    EXPECT_TRUE(legate::type_dispatch(
      dtype.code(), scalar_eq_fn{}, library.get_tunable(tunable_id++, dtype), to_compare));
  }
}

TEST_F(Tunable, Invalid)
{
  prepare();
  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->find_library(library_name);

  EXPECT_THROW(library.get_tunable(0, legate::string_type()), std::invalid_argument);
  EXPECT_THROW(library.get_tunable(0, legate::int64()), std::invalid_argument);
  EXPECT_THROW(library.get_tunable(TUNABLES.size(), legate::bool_()), std::invalid_argument);
}

}  // namespace tunable
