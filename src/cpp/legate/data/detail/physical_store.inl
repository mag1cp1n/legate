/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <legate/data/detail/physical_store.h>
#include <legate/utilities/machine.h>

#include <utility>

namespace legate::detail {

inline UnboundRegionField::UnboundRegionField(const Legion::OutputRegion& out,
                                              Legion::FieldID fid,
                                              bool partitioned)
  : partitioned_{partitioned},
    num_elements_{sizeof(std::size_t),
                  find_memory_kind_for_executing_processor(),
                  nullptr /*init_value*/,
                  alignof(std::size_t)},
    out_{out},
    fid_{fid}
{
}

inline UnboundRegionField::UnboundRegionField(UnboundRegionField&& other) noexcept
  : bound_{std::exchange(other.bound_, false)},
    partitioned_{std::exchange(other.partitioned_, false)},
    num_elements_{std::exchange(other.num_elements_, Legion::UntypedDeferredValue{})},
    out_{std::exchange(other.out_, Legion::OutputRegion{})},
    fid_{std::exchange(other.fid_, -1)}
{
}

inline bool UnboundRegionField::is_partitioned() const { return partitioned_; }

inline bool UnboundRegionField::bound() const { return bound_; }

inline void UnboundRegionField::set_bound(bool bound) { bound_ = bound; }

inline const Legion::OutputRegion& UnboundRegionField::get_output_region() const { return out_; }

inline Legion::FieldID UnboundRegionField::get_field_id() const { return fid_; }

// ==========================================================================================

inline PhysicalStore::PhysicalStore(std::int32_t dim,
                                    InternalSharedPtr<Type> type,
                                    GlobalRedopID redop_id,
                                    FutureWrapper future,
                                    InternalSharedPtr<detail::TransformStack> transform)
  : is_future_{true},
    dim_{dim},
    type_{std::move(type)},
    redop_id_{redop_id},
    future_{std::move(future)},
    transform_{std::move(transform)},
    readable_{future_.valid()},
    writable_{!future_.is_read_only()},
    reducible_{writable_}
{
}

inline PhysicalStore::PhysicalStore(std::int32_t dim,
                                    InternalSharedPtr<Type> type,
                                    GlobalRedopID redop_id,
                                    RegionField&& region_field,
                                    InternalSharedPtr<detail::TransformStack> transform)
  : dim_{dim},
    type_{std::move(type)},
    redop_id_{redop_id},
    region_field_{std::move(region_field)},
    transform_{std::move(transform)},
    readable_{region_field_.is_readable()},
    writable_{region_field_.is_writable()},
    reducible_{region_field_.is_reducible()}
{
}

inline PhysicalStore::PhysicalStore(std::int32_t dim,
                                    InternalSharedPtr<Type> type,
                                    UnboundRegionField&& unbound_field,
                                    InternalSharedPtr<detail::TransformStack> transform)
  : is_unbound_store_{true},
    dim_{dim},
    type_{std::move(type)},
    unbound_field_{std::move(unbound_field)},
    transform_{std::move(transform)}
{
}

inline std::int32_t PhysicalStore::dim() const { return dim_; }

inline const InternalSharedPtr<Type>& PhysicalStore::type() const { return type_; }

inline bool PhysicalStore::is_readable() const { return readable_; }

inline bool PhysicalStore::is_writable() const { return writable_; }

inline bool PhysicalStore::is_reducible() const { return reducible_; }

inline bool PhysicalStore::is_future() const { return is_future_; }

inline bool PhysicalStore::is_unbound_store() const { return is_unbound_store_; }

inline ReturnValue PhysicalStore::pack() const { return future_.pack(type_); }

inline ReturnValue PhysicalStore::pack_weight() const { return unbound_field_.pack_weight(); }

inline GlobalRedopID PhysicalStore::get_redop_id_() const { return redop_id_; }

}  // namespace legate::detail
