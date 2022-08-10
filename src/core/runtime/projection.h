/* Copyright 2021-2022 NVIDIA Corporation
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

#pragma once

#include "legion.h"

#include "core/runtime/context.h"
#include "core/utilities/tuple.h"

namespace legate {

namespace proj {

class SymbolicExpr {
 public:
  SymbolicExpr(int32_t dim = -1, int32_t weight = 1, int32_t offset = 0);

 public:
  int32_t dim() const { return dim_; }
  int32_t weight() const { return weight_; }
  int32_t offset() const { return offset_; }

 public:
  bool is_identity(int32_t dim) const;

 public:
  bool operator==(const SymbolicExpr& other) const;
  bool operator<(const SymbolicExpr& other) const;

 public:
  SymbolicExpr operator*(int32_t other) const;
  SymbolicExpr operator+(int32_t other) const;

 private:
  int32_t dim_{-1};
  int32_t weight_{1};
  int32_t offset_{0};
};

std::ostream& operator<<(std::ostream& out, const SymbolicExpr& expr);

using SymbolicPoint   = tuple<SymbolicExpr>;
using SymbolicFunctor = SymbolicPoint (*)(const SymbolicPoint&);

SymbolicPoint create_symbolic_point(int32_t ndim);

bool is_identity(int32_t ndim, const SymbolicPoint& point);

}  // namespace proj

// Interface for Legate projection functors
class LegateProjectionFunctor : public Legion::ProjectionFunctor {
 public:
  LegateProjectionFunctor(Legion::Runtime* runtime);

 public:
  using Legion::ProjectionFunctor::project;
  virtual Legion::LogicalRegion project(Legion::LogicalPartition upper_bound,
                                        const Legion::DomainPoint& point,
                                        const Legion::Domain& launch_domain);

 public:
  // legate projection functors are almost always functional and don't traverse the region tree
  virtual bool is_functional(void) const { return true; }
  virtual bool is_exclusive(void) const { return true; }
  virtual unsigned get_depth(void) const { return 0; }

 public:
  virtual Legion::DomainPoint project_point(const Legion::DomainPoint& point,
                                            const Legion::Domain& launch_domain) const = 0;
};

void register_legate_core_projection_functors(Legion::Runtime* runtime,
                                              const LibraryContext& context);

LegateProjectionFunctor* find_legate_projection_functor(Legion::ProjectionID proj_id);

}  // namespace legate
