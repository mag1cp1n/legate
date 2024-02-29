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

#pragma once

// This must go exactly here, because it must come before any Legion/Realm includes. If any of
// those headers come first, then we get extremely confusing errors:
//
// error: implicit instantiation of undefined template 'std::span<const unsigned long, 0>'
//   explicit logical_store(std::span<const std::size_t, 0>) : LogicalStore(logical_store::create())
//   {}
//                                                         ^
// /Users/jfaibussowit/soft/nv/legate.core.internal/build/debug-sanitizer-clang/_deps/span-src/include/tcb/span.hpp:148:7:
// note: template is declared here
// class span;
//       ^
//
// This type *is* complete and defined at that point! However, Realm has its own span
// implementation, and for whatever reason, this is picked up by the compiler, and used
// instead. You can verify this by compiling the following program:
//
// #include "realm/utils.h"
// #include "realm/instance.h"
// #include "tcb/span.hpp"
//
// int main()
// {
//   std::span<const std::size_t, 0> x;
// }
//
// And you will find the familiar:
//
// span_bug.cpp:11:35: error: implicit instantiation of undefined template
// 'std::span<const unsigned long, 0>'
//   std::span<const std::size_t, 0> x;
//                                   ^
// /Users/jfaibussowit/soft/nv/legate.core.internal/build/debug-sanitizer-clang/_deps/span-src/include/tcb/span.hpp:148:7:
// note: template is declared here class span;
//       ^
#include "span.hpp"
//

#include "core/utilities/defined.h"

#include "config.hpp"
#include "legate.h"
#include "mdspan.hpp"
#include "meta.hpp"
#include "ranges.hpp"
#include "type_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

// Include this last:
#include "prefix.hpp"

/// @defgroup data Data

namespace legate::stl {
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace tags {

inline namespace obj {
}

}  // namespace tags

// Fully qualify the namespace to ensure that the compiler doesn't pick some other random one
// NOLINTNEXTLINE(google-build-using-namespace)
using namespace ::legate::stl::tags::obj;

////////////////////////////////////////////////////////////////////////////////////////////////////
using extents                              = const std::size_t[];
inline constexpr std::int32_t dynamic_dims = -1;

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ElementType, std::int32_t Dim = dynamic_dims>
class logical_store;

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace detail {

template <typename Store>
struct value_type_of_;

template <typename ElementType, typename Extents, typename Layout, typename Accessor>
class value_type_of_<std::mdspan<ElementType, Extents, Layout, Accessor>> {
 public:
  using type = ElementType;
};

template <typename Storage>
using has_dim_ = meta::constant<!(std::int32_t{Storage::dim()} < 0)>;

template <typename Storage>
inline constexpr bool has_dim_v =
  meta::eval<meta::quote_or<has_dim_, std::false_type>, std::remove_reference_t<Storage>>::value;

}  // namespace detail

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief An alias for the value type of a `legate::stl::logical_store_like` type. A
 *        store's value type is its element type stripped of any `const` qualification.
 *
 * @tparam Storage A type that satisfies the `legate::stl::logical_store_like` concept.
 */
template <typename Storage>
using value_type_of_t =
  LEGATE_STL_IMPLEMENTATION_DEFINED(typename detail::value_type_of_<remove_cvref_t<Storage>>::type);

/**
 * @brief An alias for the element type of a `legate::stl::logical_store_like` type. A
 *        store's element type is `const` qualified if the store is read-only.
 *
 * @tparam Storage A type that satisfies the `legate::stl::logical_store_like` concept.
 * @hideinitializer
 */
template <typename Storage>
using element_type_of_t = LEGATE_STL_IMPLEMENTATION_DEFINED(
  const_if_t<std::is_const_v<std::remove_reference_t<Storage>>, value_type_of_t<Storage>>);

/**
 * @brief A constexpr variable constant for the dimensionality of a
 *        `legate::stl::logical_store_like` type.
 *
 * @tparam Storage A type that satisfies the `legate::stl::logical_store_like` concept.
 * @hideinitializer
 */
template <typename Storage>
  requires(detail::has_dim_v<Storage>)
inline constexpr std::int32_t dim_of_v = std::remove_reference_t<Storage>::dim();

/** @cond */
template <typename Storage>
inline constexpr std::int32_t dim_of_v<Storage&> = dim_of_v<Storage>;

template <typename Storage>
inline constexpr std::int32_t dim_of_v<Storage&&> = dim_of_v<Storage>;

template <typename Storage>
inline constexpr std::int32_t dim_of_v<const Storage> = dim_of_v<Storage>;

template <typename ElementType, std::int32_t Dim>
inline constexpr std::int32_t dim_of_v<logical_store<ElementType, Dim>> = Dim;
/** @endcond */

/*************************************************************************************************
 * @brief Given an untyped `legate::LogicalStore`, return a strongly-typed
 *        `legate::stl::logical_store`.
 *
 * @tparam ElementType The element type of the `LogicalStore`.
 * @tparam Dim The dimensionality of the `LogicalStore`.
 * @param store The `LogicalStore` to convert.
 * @return logical_store<ElementType, Dim>
 * @pre The element type of the `LogicalStore` must be the same as `ElementType`,
 *      and the dimensionality of the `LogicalStore` must be the same as `Dim`.
 */
template <typename ElementType, std::int32_t Dim = dynamic_dims>
logical_store<ElementType, Dim> as_typed(const legate::LogicalStore& store);

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace detail {

template <typename Function, typename... InputSpans>
class elementwise_accessor;

class default_accessor;

template <typename Op, bool Exclusive>
class reduction_accessor;

template <typename ElementType, std::int32_t Dim, typename Accessor /*= default_accessor*/>
class mdspan_accessor;

}  // namespace detail

template <typename Input>
using mdspan_for_t = mdspan_t<element_type_of_t<Input>, dim_of_v<Input>>;

/** @cond */
void as_mdspan(const PhysicalStore&&) = delete;
/** @endcond */

/*************************************************************************************************
 * @brief Given an untyped `legate::PhysicalStore`, return a strongly-typed
 *        `legate::stl::logical_store`.
 *
 * @tparam ElementType The element type of the `PhysicalStore`.
 * @tparam Dim The dimensionality of the `PhysicalStore`.
 * @param store The `PhysicalStore` to convert.
 * @return mdspan_t<ElementType, Dim>
 * @pre The element type of the `PhysicalStore` must be the same as
 *      `ElementType`, and the dimensionality of the `Store` must be the same
 *      as `Dim`.
 */
template <typename ElementType, std::int32_t Dim>
LEGATE_STL_ATTRIBUTE((host, device))  //
[[nodiscard]] mdspan_t<ElementType, Dim> as_mdspan(const legate::PhysicalStore& store);

/** @overload */
template <typename ElementType, std::int32_t Dim>
LEGATE_STL_ATTRIBUTE((host, device))  //
[[nodiscard]] mdspan_t<ElementType, Dim> as_mdspan(const legate::LogicalStore& store);

template <typename ElementType, std::int32_t Dim, template <typename, std::int32_t> typename StoreT>
  requires(same_as<logical_store<ElementType, Dim>, StoreT<ElementType, Dim>>)
LEGATE_STL_ATTRIBUTE((host, device))  //
  [[nodiscard]] mdspan_t<ElementType, Dim> as_mdspan(const StoreT<ElementType, Dim>& store);
/** @endcond */

/** @overload */
template <typename ElementType, std::int32_t Dim>
LEGATE_STL_ATTRIBUTE((host, device))  //
[[nodiscard]] mdspan_t<ElementType, Dim> as_mdspan(const legate::PhysicalArray& array);

namespace detail {

struct iteration_kind {};

struct reduction_kind {};

template <typename... Types>
void ignore_all(Types&&...);

////////////////////////////////////////////////////////////////////////////////////////////////
template <typename StoreLike>
auto logical_store_like_concept_impl(StoreLike& storeish,
                                     LogicalStore& lstore,
                                     mdspan_for_t<StoreLike> span,
                                     PhysicalStore& pstore)  //
  -> decltype(detail::ignore_all(                            //
    StoreLike::policy::logical_view(lstore),
    StoreLike::policy::physical_view(span),
    StoreLike::policy::size(pstore),
    StoreLike::policy::partition_constraints(iteration_kind{}),
    StoreLike::policy::partition_constraints(reduction_kind{}),
    get_logical_store(storeish)))
{
}

template <typename StoreLike, typename Ptr = decltype(&logical_store_like_concept_impl<StoreLike>)>
constexpr bool is_logical_store_like(int)
{
  return true;
}

template <typename StoreLike>
constexpr bool is_logical_store_like(std::int64_t)
{
  return false;
}

static_assert(!std::is_same_v<int, std::int64_t>);

////////////////////////////////////////////////////////////////////////////////////////////////
template <typename Reduction>
auto legate_reduction_concept_impl(Reduction,
                                   typename Reduction::LHS lhs,  //
                                   typename Reduction::RHS rhs)  //
  -> decltype(detail::ignore_all((                               //
    Reduction::template apply<true>(lhs, std::move(rhs)),        //
    Reduction::template apply<false>(lhs, std::move(rhs)),       //
    Reduction::template fold<true>(rhs, std::move(rhs)),         //
    Reduction::template fold<false>(rhs, std::move(rhs)),        //
    std::integral_constant<typename Reduction::LHS, Reduction::identity>{},
    std::integral_constant<int, Reduction::REDOP_ID>{})))
{
}

template <typename Reduction, typename Ptr = decltype(&legate_reduction_concept_impl<Reduction>)>
constexpr bool is_legate_reduction(int)
{
  return true;
}

template <typename StoreLike>
constexpr bool is_legate_reduction(std::int64_t)
{
  return false;
}

static_assert(!std::is_same_v<int, std::int64_t>);

}  // namespace detail

#if LegateDefined(LEGATE_STL_DOXYGEN)
/**
 * @brief True when `StoreLike` is a type that implements the `get_logical_store`
 *        customization point.
 */
template <typename StoreLike>
concept logical_store_like = /* see below */;

/**
 * @brief True when `Reduction` has static `fold` and `apply` member functions.
 */
template <typename Reduction>
concept legate_reduction = /* see below */;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename StoreLike>
inline constexpr bool logical_store_like =
  detail::is_logical_store_like<remove_cvref_t<StoreLike>>(0);

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename Reduction>
inline constexpr bool legate_reduction = detail::is_legate_reduction<remove_cvref_t<Reduction>>(0);

}  // namespace legate::stl

#include "suffix.hpp"
