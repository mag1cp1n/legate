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

#include "core/utilities/compiler.h"
#include "core/utilities/macros.h"

#if __has_include(<version>)
#include <version>
#else
#include <ciso646>  // For stdlib feature test macros
#endif

#define LEGATE_STL_DETAIL_CONFIG_INCLUDED

#if defined(__cpp_concepts)
#define LEGATE_STL_CONCEPTS() 1
#else
#define LEGATE_STL_CONCEPTS() 0
#endif

#define LEGATE_STL_EAT(...)
#define LEGATE_STL_EVAL(M, ...) M(__VA_ARGS__)
#define LEGATE_STL_EXPAND(...) __VA_ARGS__

#define LEGATE_STL_CHECK(...) LEGATE_STL_EXPAND(LEGATE_STL_CHECK_(__VA_ARGS__, 0, ))
// NOLINTNEXTLINE(readability-identifier-naming)
#define LEGATE_STL_CHECK_(XP, NP, ...) NP
#define LEGATE_STL_PROBE(...) LEGATE_STL_PROBE_(__VA_ARGS__, 1)
// NOLINTNEXTLINE(readability-identifier-naming)
#define LEGATE_STL_PROBE_(XP, NP, ...) XP, NP,

////////////////////////////////////////////////////////////////////////////////////////////////////
// Concepts emulation and portability macros.
//   Usage:
//
//   template <typename A, typename B>
//     LEGATE_STL_REQUIRES(Fooable<A> && Barable<B>)
//   void foobar(A a, B b) { ... }
//
#if LEGATE_DEFINED(LEGATE_STL_CONCEPTS())
#define LEGATE_STL_REQUIRES requires
#else
#define LEGATE_STL_REQUIRES LEGATE_STL_EAT
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Before clang-16, clang did not like libstdc++'s ranges implementation
#if __has_include(<ranges>) && \
  (defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L) && \
  (!LEGATE_DEFINED(LEGATE_CLANG) || __clang_major__ >= 16 || defined(_LIBCPP_VERSION))
#define LEGATE_STL_HAS_STD_RANGES() 1
#else
#define LEGATE_STL_HAS_STD_RANGES() 0
#endif

#ifndef LEGATE_STL_IMPLEMENTATION_DEFINED
#if LEGATE_DEFINED(LEGATE_DOXYGEN)
#define LEGATE_STL_IMPLEMENTATION_DEFINED(...) implementation - defined
#else
#define LEGATE_STL_IMPLEMENTATION_DEFINED(...) __VA_ARGS__
#endif
#endif
