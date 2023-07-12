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

#include "core/operation/detail/copy.h"

#include "core/operation/detail/copy_launcher.h"
#include "core/partitioning/constraint.h"
#include "core/partitioning/constraint_solver.h"
#include "core/partitioning/partition.h"
#include "core/partitioning/partitioner.h"

namespace legate::detail {

Copy::Copy(std::shared_ptr<LogicalStore> target,
           std::shared_ptr<LogicalStore> source,
           int64_t unique_id,
           mapping::MachineDesc&& machine)
  : Operation(unique_id, std::move(machine)),
    target_{target.get(), declare_partition()},
    source_{source.get(), declare_partition()},
    constraint_(legate::align(target_.variable, source_.variable))
{
  record_partition(target_.variable, std::move(target));
  record_partition(source_.variable, std::move(source));
}

void Copy::validate()
{
  if (source_.store->type() != target_.store->type()) {
    throw std::invalid_argument("Source and targets must have the same type");
  }
  auto validate_store = [](auto* store) {
    if (store->unbound() || store->has_scalar_storage() || store->transformed()) {
      throw std::invalid_argument("Copy accepts only normal, untransformed, region-backed stores");
    }
  };
  validate_store(target_.store);
  validate_store(source_.store);
  constraint_->validate();
}

void Copy::launch(Strategy* p_strategy)
{
  auto& strategy = *p_strategy;
  CopyLauncher launcher(machine_);
  auto launch_domain = strategy.launch_domain(this);

  launcher.add_input(source_.store, create_projection_info(strategy, launch_domain, source_));
  launcher.add_output(target_.store, create_projection_info(strategy, launch_domain, target_));

  if (launch_domain != nullptr) {
    return launcher.execute(*launch_domain);
  } else {
    return launcher.execute_single();
  }
}

void Copy::add_to_solver(ConstraintSolver& solver)
{
  solver.add_constraint(constraint_.get());
  solver.add_partition_symbol(target_.variable);
  solver.add_partition_symbol(source_.variable);
}

std::string Copy::to_string() const { return "Copy:" + std::to_string(unique_id_); }

}  // namespace legate::detail
