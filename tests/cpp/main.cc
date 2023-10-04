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

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  DefaultFixture::init(argc, argv);
  DeathTestFixture::init(argc, argv);

  return RUN_ALL_TESTS();
}
