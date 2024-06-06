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

#pragma once

#include "core/cuda/cuda.h"
#include "core/utilities/debug.h"

#include <sstream>

namespace legate {

template <typename T, int DIM>
[[nodiscard]] std::string print_dense_array(const T* base,
                                            const Point<DIM>& extents,
                                            std::size_t strides[DIM])
{
  T* buf                            = nullptr;
  constexpr auto is_device_only_ptr = [](const void* ptr) {
    cudaPointerAttributes attrs;
    auto res = cudaPointerGetAttributes(&attrs, ptr);
    return res == cudaSuccess && attrs.type == cudaMemoryTypeDevice;
  };

  if (is_device_only_ptr(base)) {
    const auto max_different_types = [](const auto& lhs, const auto& rhs) {
      return lhs < rhs ? rhs : lhs;
    };
    std::size_t num_elems = 0;
    for (std::size_t dim = 0; dim < DIM; ++dim) {
      num_elems = max_different_types(num_elems, strides[dim] * extents[dim]);
    }
    buf      = new T[num_elems];
    auto res = cudaMemcpy(buf, base, num_elems * sizeof(T), cudaMemcpyDeviceToHost);
    LEGATE_CHECK(res == cudaSuccess);
    base = buf;
  }
  std::stringstream ss;

  for (int dim = 0; dim < DIM; ++dim) {
    if (strides[dim] != 0) {
      ss << "[";
    }
  }
  ss << *base;

  coord_t offset   = 0;
  Point<DIM> point = Point<DIM>::ZEROES();
  int dim;
  do {
    for (dim = DIM - 1; dim >= 0; --dim) {
      if (strides[dim] == 0) {
        continue;
      }
      if (point[dim] + 1 < extents[dim]) {
        ++point[dim];
        offset += strides[dim];
        ss << ", ";

        for (auto i = dim + 1; i < DIM; ++i) {
          if (strides[i] != 0) {
            ss << "[";
          }
        }
        ss << base[offset];
        break;
      }
      offset -= point[dim] * strides[dim];
      point[dim] = 0;
      ss << "]";
    }
  } while (dim >= 0);
  if (LEGATE_DEFINED(LEGATE_USE_CUDA)) {
    delete[] buf;  // LEGATE_USE_CUDA
  }
  return ss.str();
}

template <int DIM, typename ACC>
[[nodiscard]] std::string print_dense_array(ACC accessor, const Rect<DIM>& rect)
{
  const Point<DIM> extents = rect.hi - rect.lo + Point<DIM>::ONES();
  std::size_t strides[DIM];
  const typename ACC::value_type* base = accessor.ptr(rect, strides);
  return print_dense_array(base, extents, strides);
}

}  // namespace legate
