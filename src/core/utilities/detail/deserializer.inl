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

// Useful for IDEs
#include "core/utilities/detail/deserializer.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace legate::detail {

template <typename T>
std::pair<void*, std::size_t> align_for_unpack(void* ptr,
                                               std::size_t capacity,
                                               std::size_t bytes,
                                               std::size_t align)
{
  const auto orig_avail_space = std::min(bytes + align - 1, capacity);
  auto avail_space            = orig_avail_space;

  if (!std::align(align, bytes, ptr, avail_space)) {
    std::stringstream ss;

    // If we get here, it means that someone did not pack the value correctly, likely without
    // first aligning the pointer!
    ss << "Failed to align buffer " << ptr << " (of size: " << bytes << ") to " << align
       << "-byte alignment (remaining capacity: " << capacity << ')';
    throw std::runtime_error{std::move(ss).str()};
  }
  return {ptr, orig_avail_space - avail_space};
}

template <typename Deserializer>
BaseDeserializer<Deserializer>::BaseDeserializer(const void* args, std::size_t arglen)
  : args_{static_cast<const std::int8_t*>(args), arglen}
{
}

template <typename Deserializer>
template <typename T>
inline T BaseDeserializer<Deserializer>::unpack()
{
  T value;
  static_cast<Deserializer*>(this)->unpack_impl(value);
  return value;
}

template <typename Deserializer>
template <typename T, std::enable_if_t<type_code_of_v<T> != Type::Code::NIL>*>
void BaseDeserializer<Deserializer>::unpack_impl(T& value)
{
  const auto vptr          = static_cast<void*>(const_cast<std::int8_t*>(args_.ptr()));
  auto [ptr, align_offset] = align_for_unpack<T>(vptr, args_.size());

  // We need to align-up the incoming args_.ptr() since the value was stored according to
  // alignof(T). So we ultimately get 2 pointers:
  //
  //      ____ vptr (args_.ptr() on entry)
  //     /
  //    /           ___ ptr                            args_.ptr() on exit
  //   /           /                                          |
  //  v           v                                           v
  //  X --------- X ========================================= X
  //   ^~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
  //        |                        |
  //   align_offset               sizeof(T)
  //
  //
  // Note align_offset may be zero if vptr was already properly aligned.
  value = *static_cast<const T*>(ptr);
  args_ = args_.subspan(align_offset + sizeof(T));
}

template <typename Deserializer>
template <typename T>
void BaseDeserializer<Deserializer>::unpack_impl(std::vector<T>& values)
{
  auto size = unpack<std::uint32_t>();
  values.reserve(size);
  for (std::uint32_t idx = 0; idx < size; ++idx) {
    values.emplace_back(unpack<T>());
  }
}

template <typename Deserializer>
template <typename T1, typename T2>
void BaseDeserializer<Deserializer>::unpack_impl(std::pair<T1, T2>& values)
{
  values.first  = unpack<T1>();
  values.second = unpack<T2>();
}

template <typename Deserializer>
std::vector<legate::Scalar> BaseDeserializer<Deserializer>::unpack_scalars()
{
  std::vector<legate::Scalar> values;
  auto size = unpack<std::uint32_t>();

  values.reserve(size);
  for (std::uint32_t idx = 0; idx < size; ++idx) {
    values.emplace_back(unpack_scalar());
  }
  return values;
}

template <typename Deserializer>
std::unique_ptr<Scalar> BaseDeserializer<Deserializer>::unpack_scalar()
{
  // this unpack_type call must be in a separate line from the following one because they both
  // read and update the buffer location.
  auto type = unpack_type_();

  const auto unpack_scalar_value = [&](const auto& ty,
                                       const signed char* vptr,
                                       std::size_t capacity) -> std::pair<void*, std::size_t> {
    const auto ptr = static_cast<void*>(const_cast<signed char*>(vptr));

    switch (ty->code) {
      case Type::Code::NIL:
        return {nullptr, 0};

#define LEGATE_CORE_CASE_TYPE_CODE(CODE) \
  case Type::Code::CODE:                 \
    return align_for_unpack<type_of_t<Type::Code::CODE>>(ptr, capacity, ty->size(), ty->alignment())

        LEGATE_CORE_CASE_TYPE_CODE(BOOL);
        LEGATE_CORE_CASE_TYPE_CODE(INT8);
        LEGATE_CORE_CASE_TYPE_CODE(INT16);
        LEGATE_CORE_CASE_TYPE_CODE(INT32);
        LEGATE_CORE_CASE_TYPE_CODE(INT64);
        LEGATE_CORE_CASE_TYPE_CODE(UINT8);
        LEGATE_CORE_CASE_TYPE_CODE(UINT16);
        LEGATE_CORE_CASE_TYPE_CODE(UINT32);
        LEGATE_CORE_CASE_TYPE_CODE(UINT64);
        LEGATE_CORE_CASE_TYPE_CODE(FLOAT16);
        LEGATE_CORE_CASE_TYPE_CODE(FLOAT32);
        LEGATE_CORE_CASE_TYPE_CODE(FLOAT64);
        LEGATE_CORE_CASE_TYPE_CODE(COMPLEX64);
        LEGATE_CORE_CASE_TYPE_CODE(COMPLEX128);

#undef LEGATE_CORE_CASE_TYPE_CODE

      case Type::Code::BINARY:       // fall-through
      case Type::Code::FIXED_ARRAY:  // fall-through
      case Type::Code::STRUCT:
        return align_for_unpack<std::byte>(ptr, capacity, ty->size(), ty->alignment());
      case Type::Code::STRING:
        // The size is an approximation here. We cannot know the true size of the string until
        // we have aligned the pointer, but we cannot align the pointer without knowing the
        // true size of the string... so we give a lower bound
        return align_for_unpack<std::byte>(
          ptr, capacity, sizeof(std::uint32_t) + sizeof(char), alignof(std::max_align_t));
      case Type::Code::LIST:
        // don't know how to handle these yet
        break;
        // do not add a default clause! compilers will warn about missing enum values if there is
        // ever a new value added to Type::Code. We want to catch that!
    }
    LEGATE_ABORT("unhandled type code: " << legate::traits::detail::to_underlying(ty->code));
    return {nullptr, 0};
  };

  auto [ptr, align_offset] = unpack_scalar_value(type, args_.ptr(), args_.size());
  auto result              = std::make_unique<Scalar>(type, ptr, false /*copy*/);

  args_ = args_.subspan(align_offset + result->size());
  return result;
}

template <typename Deserializer>
void BaseDeserializer<Deserializer>::unpack_impl(mapping::TaskTarget& value)
{
  value = static_cast<mapping::TaskTarget>(unpack<std::int32_t>());
}

template <typename Deserializer>
void BaseDeserializer<Deserializer>::unpack_impl(mapping::ProcessorRange& value)
{
  value.low            = unpack<std::uint32_t>();
  value.high           = unpack<std::uint32_t>();
  value.per_node_count = unpack<std::uint32_t>();
}

template <typename Deserializer>
void BaseDeserializer<Deserializer>::unpack_impl(mapping::detail::Machine& value)
{
  value.preferred_target =
    static_cast<mapping::TaskTarget>(unpack<std::underlying_type_t<mapping::TaskTarget>>());
  auto num_ranges = unpack<std::uint32_t>();
  for (std::uint32_t idx = 0; idx < num_ranges; ++idx) {
    auto kind =
      static_cast<mapping::TaskTarget>(unpack<std::underlying_type_t<mapping::TaskTarget>>());
    auto range = unpack<mapping::ProcessorRange>();
    if (!range.empty()) {
      value.processor_ranges.insert({kind, range});
    }
  }
}

template <typename Deserializer>
void BaseDeserializer<Deserializer>::unpack_impl(Domain& domain)
{
  domain.dim = unpack<std::uint32_t>();
  for (std::int32_t idx = 0; idx < domain.dim; ++idx) {
    auto coord                         = unpack<std::int64_t>();
    domain.rect_data[idx]              = 0;
    domain.rect_data[idx + domain.dim] = coord - 1;
  }
}

template <typename Deserializer>
Span<const std::int8_t> BaseDeserializer<Deserializer>::current_args() const
{
  return args_;
}

template <typename Deserializer>
InternalSharedPtr<TransformStack> BaseDeserializer<Deserializer>::unpack_transform_()
{
  auto code = unpack<std::int32_t>();
  switch (code) {
    case -1: {
      return make_internal_shared<TransformStack>();
    }
    case LEGATE_CORE_TRANSFORM_SHIFT: {
      auto dim    = unpack<std::int32_t>();
      auto offset = unpack<std::int64_t>();
      auto parent = unpack_transform_();
      return make_internal_shared<TransformStack>(std::make_unique<Shift>(dim, offset),
                                                  std::move(parent));
    }
    case LEGATE_CORE_TRANSFORM_PROMOTE: {
      auto extra_dim = unpack<std::int32_t>();
      auto dim_size  = unpack<std::int64_t>();
      auto parent    = unpack_transform_();
      return make_internal_shared<TransformStack>(std::make_unique<Promote>(extra_dim, dim_size),
                                                  std::move(parent));
    }
    case LEGATE_CORE_TRANSFORM_PROJECT: {
      auto dim    = unpack<std::int32_t>();
      auto coord  = unpack<std::int64_t>();
      auto parent = unpack_transform_();
      return make_internal_shared<TransformStack>(std::make_unique<Project>(dim, coord),
                                                  std::move(parent));
    }
    case LEGATE_CORE_TRANSFORM_TRANSPOSE: {
      auto axes   = unpack<std::vector<std::int32_t>>();
      auto parent = unpack_transform_();
      return make_internal_shared<TransformStack>(std::make_unique<Transpose>(std::move(axes)),
                                                  std::move(parent));
    }
    case LEGATE_CORE_TRANSFORM_DELINEARIZE: {
      auto dim    = unpack<std::int32_t>();
      auto sizes  = unpack<std::vector<std::uint64_t>>();
      auto parent = unpack_transform_();
      return make_internal_shared<TransformStack>(
        std::make_unique<Delinearize>(dim, std::move(sizes)), std::move(parent));
    }
  }
  LEGATE_ABORT("Unhandled transform code: " << code);
  return nullptr;
}

template <typename Deserializer>
InternalSharedPtr<Type> BaseDeserializer<Deserializer>::unpack_type_()
{
  auto code = static_cast<Type::Code>(unpack<std::underlying_type_t<Type::Code>>());
  switch (code) {
    case Type::Code::FIXED_ARRAY: {
      auto uid  = unpack<std::uint32_t>();
      auto n    = unpack<std::uint32_t>();
      auto type = unpack_type_();

      return make_internal_shared<FixedArrayType>(uid, std::move(type), n);
    }
    case Type::Code::STRUCT: {
      auto uid        = unpack<std::uint32_t>();
      auto num_fields = unpack<std::uint32_t>();

      std::vector<InternalSharedPtr<Type>> field_types;

      field_types.reserve(num_fields);
      for (std::uint32_t idx = 0; idx < num_fields; ++idx) {
        field_types.emplace_back(unpack_type_());
      }

      auto align = unpack<bool>();

      return make_internal_shared<StructType>(uid, std::move(field_types), align);
    }
    case Type::Code::LIST: {
      auto uid  = unpack<std::uint32_t>();
      auto type = unpack_type_();
      return make_internal_shared<ListType>(uid, std::move(type));
    }
    case Type::Code::NIL: {
      return null_type();
    }
    case Type::Code::BOOL: {
      return bool_();
    }
    case Type::Code::INT8: {
      return int8();
    }
    case Type::Code::INT16: {
      return int16();
    }
    case Type::Code::INT32: {
      return int32();
    }
    case Type::Code::INT64: {
      return int64();
    }
    case Type::Code::UINT8: {
      return uint8();
    }
    case Type::Code::UINT16: {
      return uint16();
    }
    case Type::Code::UINT32: {
      return uint32();
    }
    case Type::Code::UINT64: {
      return uint64();
    }
    case Type::Code::FLOAT16: {
      return float16();
    }
    case Type::Code::FLOAT32: {
      return float32();
    }
    case Type::Code::FLOAT64: {
      return float64();
    }
    case Type::Code::COMPLEX64: {
      return complex64();
    }
    case Type::Code::COMPLEX128: {
      return complex128();
    }
    case Type::Code::BINARY: {
      auto size = unpack<std::uint32_t>();
      return binary_type(size);
    }
    case Type::Code::STRING: {
      return make_internal_shared<StringType>();
    }
    default: break;
  }
  LEGATE_ABORT("unhandled type code: " << legate::traits::detail::to_underlying(code));
  return {};
}

}  // namespace legate::detail
