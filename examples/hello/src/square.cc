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

#include "hello_world.h"
#include "legate_library.h"

namespace hello {

class SquareTask : public Task<SquareTask, SQUARE> {
 public:
  static void cpu_variant(legate::TaskContext context)
  {
    legate::Store output         = context.output(0).data();
    legate::Rect<1> output_shape = output.shape<1>();
    auto out                     = output.write_accessor<float, 1>();

    legate::Store input         = context.input(0).data();
    legate::Rect<1> input_shape = input.shape<1>();  // should be a 1-Dim array
    auto in                     = input.read_accessor<float, 1>();

    assert(input_shape == output_shape);

    logger.info() << "Elementwise square [" << output_shape.lo << "," << output_shape.hi << "]";

    // i is a global index for the complete array
    for (size_t i = input_shape.lo; i <= input_shape.hi; ++i) {
      out[i] = in[i] * in[i];
    }
  }
};

}  // namespace hello

namespace  // unnamed
{

static void __attribute__((constructor)) register_tasks(void)
{
  hello::SquareTask::register_variants();
}

}  // namespace
