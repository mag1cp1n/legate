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

#pragma once

#include <memory>
#include <unordered_map>

#include "legion.h"

namespace legate {

class LogicalStore;
class Operation;
class Partition;
class Projection;
class Runtime;

class Strategy {
 public:
  Strategy();

 public:
  bool parallel(const Operation* op) const;
  Legion::Domain launch_domain(const Operation* op) const;
  void set_launch_domain(const Operation* op, const Legion::Domain& launch_domain);

 public:
  void insert(const LogicalStore* store, std::shared_ptr<Partition> partition);
  std::shared_ptr<Partition> find(const LogicalStore* store) const;

 public:
  std::unique_ptr<Projection> get_projection(LogicalStore* store) const;

 private:
  std::unordered_map<const LogicalStore*, std::shared_ptr<Partition>> assignments_;
  std::unordered_map<const Operation*, Legion::Domain> launch_domains_;
};

class Partitioner {
 public:
  Partitioner(Runtime* runtime, std::vector<Operation*>&& operations);

 public:
  std::unique_ptr<Strategy> partition_stores();

 private:
  Runtime* runtime_;
  std::vector<Operation*> operations_;
};

}  // namespace legate
