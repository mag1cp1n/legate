/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <legate/data/shape.h>
#include <legate/partitioning/detail/constraint.h>
#include <legate/utilities/hash.h>
#include <legate/utilities/internal_shared_ptr.h>
#include <legate/utilities/span.h>

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace legate::detail {

class ConstraintSolver;
class Partition;
class Partitioner;

class Strategy {
  friend class Partitioner;

 public:
  [[nodiscard]] bool parallel(const Operation* op) const;
  [[nodiscard]] const Domain& launch_domain(const Operation* op) const;
  void set_launch_domain(const Operation* op, const Domain& domain);

  void insert(const Variable* partition_symbol, InternalSharedPtr<Partition> partition);
  void insert(const Variable* partition_symbol,
              InternalSharedPtr<Partition> partition,
              Legion::FieldSpace field_space,
              Legion::FieldID field_id);
  [[nodiscard]] bool has_assignment(const Variable* partition_symbol) const;
  [[nodiscard]] const InternalSharedPtr<Partition>& operator[](
    const Variable* partition_symbol) const;
  [[nodiscard]] const std::pair<Legion::FieldSpace, Legion::FieldID>& find_field_for_unbound_store(
    const Variable* partition_symbol) const;
  [[nodiscard]] bool is_key_partition(const Variable* partition_symbol) const;

  void dump() const;

 private:
  void compute_launch_domains_(const ConstraintSolver& solver);
  void record_key_partition_(const Variable* partition_symbol);

  std::unordered_map<Variable, InternalSharedPtr<Partition>> assignments_{};
  std::unordered_map<Variable, std::pair<Legion::FieldSpace, Legion::FieldID>>
    fields_for_unbound_stores_{};
  std::unordered_map<const Operation*, Domain> launch_domains_{};
  std::optional<const Variable*> key_partition_{};
};

class Partitioner {
 public:
  explicit Partitioner(Span<const InternalSharedPtr<Operation>> operations);

  [[nodiscard]] std::unique_ptr<Strategy> partition_stores();

 private:
  // Populates solutions for unbound stores in the `strategy` and returns remaining partition
  // symbols
  [[nodiscard]] static std::vector<const Variable*> handle_unbound_stores_(
    Strategy* strategy,
    std::vector<const Variable*> partition_symbols,
    const ConstraintSolver& solver);

  Span<const InternalSharedPtr<Operation>> operations_{};
};

}  // namespace legate::detail

#include <legate/partitioning/detail/partitioner.inl>
