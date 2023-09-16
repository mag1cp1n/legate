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

#include "core/operation/detail/projection.h"

#include "core/runtime/detail/runtime.h"

namespace legate::detail {

template <>
void BaseProjectionInfo::populate_requirement<true>(Legion::RegionRequirement& requirement,
                                                    const Legion::LogicalRegion& region,
                                                    const std::vector<Legion::FieldID>& fields,
                                                    Legion::PrivilegeMode privilege,
                                                    bool is_key) const
{
  auto parent = Runtime::get_runtime()->find_parent_region(region);
  auto tag    = is_key ? LEGATE_CORE_KEY_STORE_TAG : 0;
  if (LEGION_REDUCE == privilege) {
    new (&requirement) Legion::RegionRequirement(region, redop, LEGION_EXCLUSIVE, parent, tag);
  } else {
    new (&requirement) Legion::RegionRequirement(region, privilege, LEGION_EXCLUSIVE, parent, tag);
  }
  requirement.add_fields(fields);
}

template <>
void BaseProjectionInfo::populate_requirement<false>(Legion::RegionRequirement& requirement,
                                                     const Legion::LogicalRegion& region,
                                                     const std::vector<Legion::FieldID>& fields,
                                                     Legion::PrivilegeMode privilege,
                                                     bool is_key) const
{
  if (Legion::LogicalPartition::NO_PART == partition) {
    populate_requirement<true>(requirement, region, fields, privilege, is_key);
    return;
  }

  auto parent = Runtime::get_runtime()->find_parent_region(region);
  auto tag    = is_key ? LEGATE_CORE_KEY_STORE_TAG : 0;
  if (LEGION_REDUCE == privilege) {
    new (&requirement)
      Legion::RegionRequirement(partition, proj_id, redop, LEGION_EXCLUSIVE, parent, tag);
  } else {
    new (&requirement)
      Legion::RegionRequirement(partition, proj_id, privilege, LEGION_EXCLUSIVE, parent, tag);
  }
  requirement.add_fields(fields);
}

BaseProjectionInfo::BaseProjectionInfo(Legion::LogicalPartition _partition,
                                       Legion::ProjectionID _proj_id)
  : partition(_partition), proj_id(_proj_id)
{
}

bool BaseProjectionInfo::operator<(const BaseProjectionInfo& other) const
{
  if (partition < other.partition)
    return true;
  else if (other.partition < partition)
    return false;
  if (proj_id < other.proj_id)
    return true;
  else if (proj_id > other.proj_id)
    return false;
  if (redop < other.redop)
    return true;
  else
    return false;
}

bool BaseProjectionInfo::operator==(const BaseProjectionInfo& other) const
{
  return partition == other.partition && proj_id == other.proj_id && redop == other.redop;
}

ProjectionInfo::ProjectionInfo(Legion::LogicalPartition partition, Legion::ProjectionID proj_id)
  : BaseProjectionInfo(partition, proj_id)
{
}

template <>
void ProjectionInfo::populate_requirement<true>(Legion::RegionRequirement& requirement,
                                                const Legion::LogicalRegion& region,
                                                const std::vector<Legion::FieldID>& fields,
                                                Legion::PrivilegeMode privilege) const
{
  populate_requirement<true>(requirement, region, fields, privilege, is_key);
}

template <>
void ProjectionInfo::populate_requirement<false>(Legion::RegionRequirement& requirement,
                                                 const Legion::LogicalRegion& region,
                                                 const std::vector<Legion::FieldID>& fields,
                                                 Legion::PrivilegeMode privilege) const
{
  populate_requirement<false>(requirement, region, fields, privilege, is_key);
}

}  // namespace legate::detail
