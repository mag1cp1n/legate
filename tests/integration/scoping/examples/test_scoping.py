# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.


import pytest
from scoping import user_context, user_lib

import legate.core.types as ty
from legate.core import ProcessorKind, Rect, get_machine


def test_scoping():
    m = get_machine()
    store = user_context.create_store(ty.int32, shape=(5, 5))

    # This task gets parallelized for the machine's preferred processor kind
    task = user_context.create_auto_task(user_lib.shared_object.MULTI_VARIANT)
    task.add_output(store)
    task.add_scalar_arg(len(m), ty.int32)
    task.execute()

    if m.count(ProcessorKind.GPU) > 0:
        with m.only(ProcessorKind.GPU):
            # This task gets parallelized for GPUs
            task = user_context.create_auto_task(
                user_lib.shared_object.MULTI_VARIANT
            )
            task.add_output(store)
            task.add_scalar_arg(m.count(ProcessorKind.GPU), ty.int32)
            task.execute()

    if m.count(ProcessorKind.OMP) > 0:
        with m.only(ProcessorKind.OMP):
            # This task gets parallelized for OMPs
            task = user_context.create_auto_task(
                user_lib.shared_object.MULTI_VARIANT
            )
            task.add_output(store)
            task.add_scalar_arg(m.count(ProcessorKind.OMP), ty.int32)
            task.execute()

    with m.only(ProcessorKind.CPU):
        # This task gets parallelized for CPUs
        task = user_context.create_auto_task(
            user_lib.shared_object.MULTI_VARIANT
        )
        task.add_output(store)
        task.add_scalar_arg(m.count(ProcessorKind.CPU), ty.int32)
        task.execute()

    store.get_inline_allocation()


def test_cpu_only():
    # All tasks in the function get parallelized for CPUs,
    # as the task only has a CPU variant
    m = get_machine()
    store = user_context.create_store(ty.int32, shape=(5, 5))

    task = user_context.create_auto_task(
        user_lib.shared_object.CPU_VARIANT_ONLY
    )
    task.add_output(store)
    task.add_scalar_arg(m.count(ProcessorKind.CPU), ty.int32)
    task.execute()

    with m.only(ProcessorKind.CPU):
        task = user_context.create_auto_task(
            user_lib.shared_object.CPU_VARIANT_ONLY
        )
        task.add_output(store)
        task.add_scalar_arg(m.count(ProcessorKind.CPU), ty.int32)
        task.execute()

    if m.count(ProcessorKind.GPU) > 0:
        with m.only(ProcessorKind.GPU):
            with pytest.raises(ValueError):
                user_context.create_auto_task(
                    user_lib.shared_object.CPU_VARIANT_ONLY
                )

    store.get_inline_allocation()


def test_shifted_slices():
    m = get_machine()
    num_nodes = len(m.get_node_range())
    per_node_count = int((len(m) + num_nodes - 1) / num_nodes)
    # this will test processor slicing of the machine
    for i in range(len(m)):
        for j in range(i + 1, len(m)):
            with m[slice(i, j)]:
                num_tasks = (j - i) * 2
                task = user_context.create_manual_task(
                    user_lib.shared_object.MAP_CHECK,
                    launch_domain=Rect(
                        [
                            num_tasks,
                        ]
                    ),
                )
                task.add_scalar_arg(per_node_count, ty.int32)
                task.add_scalar_arg(j - i, ty.int32)  # proc_count
                task.add_scalar_arg(i, ty.int32)  # start_proc_id
                task.execute()


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main(sys.argv))
