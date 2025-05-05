/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <legate/mapping/detail/store.h>

namespace legate::mapping::detail {

inline RegionField::RegionField(const Legion::RegionRequirement& req,
                                std::int32_t dim,
                                std::uint32_t idx,
                                Legion::FieldID fid,
                                bool unbound)
  : req_{&req}, dim_{dim}, idx_{idx}, fid_{fid}, unbound_{unbound}
{
}

inline bool RegionField::valid() const
{
  return get_requirement().region != Legion::LogicalRegion::NO_REGION;
}

inline RegionField::Id RegionField::unique_id() const { return {unbound(), index(), field_id()}; }

inline std::int32_t RegionField::dim() const { return dim_; }

inline std::uint32_t RegionField::index() const { return idx_; }

inline Legion::FieldID RegionField::field_id() const { return fid_; }

inline bool RegionField::unbound() const { return unbound_; }

inline const Legion::RegionRequirement& RegionField::get_requirement() const { return *req_; }

// ==========================================================================================

// Silence pass-by-value since Legion::Domain is POD, and the move ctor just does the copy
// anyways. Unfortunately there is no way to check this programmatically (e.g. via a
// static_assert).
inline FutureWrapper::FutureWrapper(std::uint32_t idx,
                                    const Legion::Domain& domain  // NOLINT(modernize-pass-by-value)
                                    )
  : idx_{idx}, domain_{domain}
{
}

inline std::int32_t FutureWrapper::dim() const { return domain().dim; }

inline std::uint32_t FutureWrapper::index() const { return idx_; }

inline const Legion::Domain& FutureWrapper::domain() const { return domain_; }

// ==========================================================================================

inline bool Store::is_future() const { return is_future_; }

inline bool Store::unbound() const { return is_unbound_store_; }

inline std::int32_t Store::dim() const { return dim_; }

inline const InternalSharedPtr<legate::detail::Type>& Store::type() const { return type_; }

inline bool Store::is_reduction() const { return redop() > GlobalRedopID{0}; }

inline GlobalRedopID Store::redop() const { return redop_id_; }

}  // namespace legate::mapping::detail
