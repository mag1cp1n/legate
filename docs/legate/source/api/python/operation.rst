.. _label_operation:

.. currentmodule:: legate.core

Operations
==========

Operations in Legate are by default automatically parallelized. Legate extracts
parallelism from an operation by partitioning its store arguments. Operations
usually require the partitions to be aligned in some way; e.g., partitioning
vectors across multiple addition tasks requires the vectors to be partitioned
in the same way. Legate provides APIs for developers to control how stores are
partitioned via `partitioning constraints`.

When an operation needs a store to be partitioned more than one way, the
operation can create `partition symbols` and use them in partitioning
constraints. In that case, a partition symbol must be passed along with the
store when the store is added. Stores can be partitioned in multiple ways when
they are used only for read accesses or reductions.

AutoTask
--------

``AutoTask`` is a type of tasks that are automatically parallelized. Each
Legate task is associated with a task id that uniquely names a task to invoke.
The actual task implementation resides on the C++ side.

.. autosummary::
   :toctree: generated/

   AutoTask.add_input
   AutoTask.add_output
   AutoTask.add_reduction
   AutoTask.add_scalar_arg
   AutoTask.declare_partition
   AutoTask.add_constraint
   AutoTask.add_alignment
   AutoTask.add_broadcast
   AutoTask.throws_exception
   AutoTask.add_nccl_communicator
   AutoTask.add_cpu_communicator
   AutoTask.set_concurrent
   AutoTask.set_side_effect
   AutoTask.execute


Manually Parallelized Tasks
---------------------------

In some occasions, tasks are unnatural or even impossible to write in the
auto-parallelized style. For those occasions, Legate provides explicit control
on how tasks are parallelized via ``ManualTask``. Each manual task requires the
caller to provide a `launch domain` that determines the degree of parallelism
and also names task instances initiaed by the task. Direct store arguments to a
manual task are assumed to be replicated across task instances, and it's the
developer's responsibility to partition stores. Mapping between points in the
launch domain and colors in the color space of a store partition is assumed to
be an identity mapping by default, but it can be configured with a `projection
function`, a Python function on tuples of coordinates. (See
:ref:`StorePartition <label_store_partition>` for definitions of color,
color space, and store partition.)

.. autosummary::
   :toctree: generated/

   ManualTask.set_concurrent
   ManualTask.set_side_effect
   ManualTask.add_input
   ManualTask.add_output
   ManualTask.add_reduction
   ManualTask.add_scalar_arg
   ManualTask.throws_exception
   ManualTask.add_nccl_communicator
   ManualTask.add_cpu_communicator
   ManualTask.execute
