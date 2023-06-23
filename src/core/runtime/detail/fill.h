/* Copyright 2023 NVIDIA Corporation
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

#include "core/runtime/operation.h"

namespace legate {

class Fill : public Operation {
 private:
  friend class detail::Runtime;
  Fill(LogicalStore lhs, LogicalStore value, int64_t unique_id, mapping::MachineDesc&& machine);

 public:
  void launch(detail::Strategy* strategy) override;

 public:
  std::string to_string() const override;

 public:
  void add_to_solver(detail::ConstraintSolver& solver) override;

 private:
  const Variable* lhs_var_;
  std::shared_ptr<detail::LogicalStore> lhs_;
  std::shared_ptr<detail::LogicalStore> value_;
};

}  // namespace legate
