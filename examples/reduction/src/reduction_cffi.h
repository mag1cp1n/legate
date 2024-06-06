/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef __REDUCTION_C_H__
#define __REDUCTION_C_H__

enum OpCode {
  _OP_CODE_BASE = 0,
  BINCOUNT,
  CATEGORIZE,
  HISTOGRAM,
  MATMUL,
  MUL,
  SUM_OVER_AXIS,
  UNIQUE,
};

#endif  // __REDUCTION_C_H__
