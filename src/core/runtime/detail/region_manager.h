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

#include "core/utilities/typedefs.h"

namespace legate::detail {

class Runtime;

class RegionManager {
 private:
  struct ManagerEntry {
    static constexpr Legion::FieldID FIELD_ID_BASE = 10000;
    static constexpr int32_t MAX_NUM_FIELDS = LEGION_MAX_FIELDS - LEGION_DEFAULT_LOCAL_FIELDS;

    ManagerEntry(const Legion::LogicalRegion& _region)
      : region(_region), next_field_id(FIELD_ID_BASE)
    {
    }
    bool has_space() const { return next_field_id - FIELD_ID_BASE < MAX_NUM_FIELDS; }
    Legion::FieldID get_next_field_id() { return next_field_id++; }

    void destroy(Runtime* runtime, bool unordered);

    Legion::LogicalRegion region;
    Legion::FieldID next_field_id;
  };

 public:
  RegionManager(Runtime* runtime, const Domain& shape);
  void destroy(bool unordered = false);

 private:
  const ManagerEntry& active_entry() const { return entries_.back(); }
  ManagerEntry& active_entry() { return entries_.back(); }
  void push_entry();

 public:
  bool has_space() const;
  std::pair<Legion::LogicalRegion, Legion::FieldID> allocate_field(size_t field_size);
  void import_region(const Legion::LogicalRegion& region);

 private:
  Runtime* runtime_;
  Domain shape_;
  std::vector<ManagerEntry> entries_{};
};

}  // namespace legate::detail
