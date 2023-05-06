/* Copyright 2021 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <numeric>

#include "core/data/logical_region_field.h"
#include "core/data/logical_store.h"
#include "core/data/logical_store_detail.h"
#include "core/data/store.h"
#include "core/partitioning/partition.h"
#include "core/runtime/req_analyzer.h"
#include "core/runtime/runtime.h"
#include "core/type/type_traits.h"
#include "core/utilities/buffer_builder.h"
#include "core/utilities/dispatch.h"
#include "legate_defines.h"

using namespace Legion;

namespace legate {

extern Logger log_legate;

LogicalRegionField::LogicalRegionField(const LogicalRegion& lr, FieldID fid) : lr_(lr), fid_(fid) {}

int32_t LogicalRegionField::dim() const { return lr_.get_dim(); }

Domain LogicalRegionField::domain() const
{
  return Runtime::get_runtime()->get_index_space_domain(lr_.get_index_space());
}

LogicalStore::LogicalStore(std::shared_ptr<detail::LogicalStore>&& impl)
  : impl_(std::forward<decltype(impl)>(impl))
{
}

int32_t LogicalStore::dim() const { return impl_->dim(); }

Type::Code LogicalStore::code() const { return impl_->code(); }

const Shape& LogicalStore::extents() const { return impl_->extents(); }

size_t LogicalStore::volume() const { return impl_->volume(); }

bool LogicalStore::unbound() const { return impl_->unbound(); }

bool LogicalStore::transformed() const { return impl_->transformed(); }

LogicalStore LogicalStore::promote(int32_t extra_dim, size_t dim_size) const
{
  return LogicalStore(impl_->promote(extra_dim, dim_size));
}

LogicalStore LogicalStore::project(int32_t dim, int64_t index) const
{
  return LogicalStore(impl_->project(dim, index));
}

LogicalStorePartition LogicalStore::partition_by_tiling(std::vector<size_t> tile_shape) const
{
  return LogicalStorePartition(impl_->partition_by_tiling(Shape(std::move(tile_shape))));
}

LogicalStore LogicalStore::slice(int32_t dim, std::slice sl) const
{
  return LogicalStore(impl_->slice(dim, sl));
}

LogicalStore LogicalStore::transpose(std::vector<int32_t>&& axes) const
{
  return LogicalStore(impl_->transpose(std::forward<decltype(axes)>(axes)));
}

LogicalStore LogicalStore::delinearize(int32_t dim, std::vector<int64_t>&& sizes) const
{
  return LogicalStore(impl_->delinearize(dim, std::forward<decltype(sizes)>(sizes)));
}

std::shared_ptr<Store> LogicalStore::get_physical_store(LibraryContext* context)
{
  return impl_->get_physical_store(context);
}

LogicalStorePartition::LogicalStorePartition(std::shared_ptr<detail::LogicalStorePartition>&& impl)
  : impl_(std::forward<decltype(impl_)>(impl))
{
}

LogicalStore LogicalStorePartition::store() const { return LogicalStore(impl_->store()); }

std::shared_ptr<Partition> LogicalStorePartition::partition() const
{
  return impl_->storage_partition()->partition();
}

}  // namespace legate
