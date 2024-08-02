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
#include "core/task/registrar.h"
#include "core/task/task.h"
#include "core/utilities/compiler.h"

namespace legate {

template <typename T>
/*static*/ void LegateTask<T>::register_variants(std::map<VariantCode, VariantOptions> all_options)
{
  T::Registrar::get_registrar().record_task(
    TaskRegistrar::RecordTaskKey{},
    T::TASK_ID,
    [callsite_options = std::move(all_options)](const Library& lib) {
      return create_task_info_(lib, callsite_options);
    });
}

template <typename T>
/*static*/ void LegateTask<T>::register_variants(
  Library library, const std::map<VariantCode, VariantOptions>& all_options)
{
  register_variants(std::move(library), static_cast<LocalTaskID>(T::TASK_ID), all_options);
}

template <typename T>
/*static*/ void LegateTask<T>::register_variants(
  Library library, LocalTaskID task_id, const std::map<VariantCode, VariantOptions>& all_options)
{
  auto task_info = create_task_info_(library, all_options);
  library.register_task(task_id, std::move(task_info));
}

template <typename T>
/*static*/ std::unique_ptr<TaskInfo> LegateTask<T>::create_task_info_(
  const Library& lib, const std::map<VariantCode, VariantOptions>& all_options)
{
  auto task_info = std::make_unique<TaskInfo>(task_name_().to_string());
  detail::VariantHelper<T, detail::CPUVariant>::record(lib, task_info.get(), all_options);
  detail::VariantHelper<T, detail::OMPVariant>::record(lib, task_info.get(), all_options);
  detail::VariantHelper<T, detail::GPUVariant>::record(lib, task_info.get(), all_options);
  return task_info;
}

template <typename T>
/*static*/ detail::ZStringView LegateTask<T>::task_name_()
{
  static const std::string result = detail::demangle_type(typeid(T));
  return result;
}

template <typename T>
template <VariantImpl variant_fn, VariantCode variant_kind>
/*static*/ void LegateTask<T>::task_wrapper_(const void* args,
                                             std::size_t arglen,
                                             const void* userdata,
                                             std::size_t userlen,
                                             Legion::Processor p)
{
  detail::task_wrapper(
    variant_fn, variant_kind, task_name_(), args, arglen, userdata, userlen, std::move(p));
}

}  // namespace legate
