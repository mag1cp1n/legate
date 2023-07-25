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

#include "core/data/scalar.h"
#include "core/data/detail/scalar.h"

namespace legate {

Scalar::Scalar(std::unique_ptr<detail::Scalar> impl) : impl_(impl.release()) {}

Scalar::Scalar(const Scalar& other) : impl_(new detail::Scalar(*other.impl_)) {}

Scalar::Scalar(Scalar&& other) : impl_(other.impl_) { other.impl_ = nullptr; }

Scalar::~Scalar() { delete impl_; }

Scalar::Scalar(Type type, const void* data, bool copy) : impl_(create_impl(type, data, copy)) {}

Scalar::Scalar(const std::string& string) : impl_(new detail::Scalar(string)) {}

Scalar& Scalar::operator=(const Scalar& other)
{
  *impl_ = *other.impl_;
  return *this;
}

Type Scalar::type() const { return Type(impl_->type()); }

size_t Scalar::size() const { return impl_->size(); }

const void* Scalar::ptr() const { return impl_->data(); }

/*static*/ detail::Scalar* Scalar::create_impl(Type type, const void* data, bool copy)
{
  return new detail::Scalar(std::move(type.impl()), data, copy);
}

}  // namespace legate
