/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <legate/utilities/detail/traced_exception.h>

#include <cerrno>
#include <cstdlib>
#include <system_error>

namespace legate::detail {

template <typename T = long long>  // NOLINT(google-runtime-int) default to match strtoll()
[[nodiscard]] T safe_strtoll(const char* env_value, char** end_ptr = nullptr)
{
  constexpr auto radix = 10;

  // must reset errno before calling std::strtoll()
  errno    = 0;
  auto ret = std::strtoll(env_value, end_ptr, radix);
  if (const auto eval = errno) {
    throw TracedException<std::system_error>{
      eval, std::generic_category(), "error occurred calling std::strtol()"};
  }
  return static_cast<T>(ret);
}

}  // namespace legate::detail
