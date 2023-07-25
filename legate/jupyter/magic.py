# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from __future__ import annotations

import os
from textwrap import indent
from typing import TYPE_CHECKING

from IPython.core.magic import Magics, line_magic, magics_class
from jupyter_client.kernelspec import KernelSpecManager, NoSuchKernel

from legate.jupyter.kernel import (
    LEGATE_JUPYTER_KERNEL_SPEC_KEY,
    LEGATE_JUPYTER_METADATA_KEY,
    LegateMetadata,
)
from legate.util.colors import scrub
from legate.util.ui import kvtable

if TYPE_CHECKING:
    from IPython import InteractiveShell


core = {
    "cpus": "CPUs to use per rank",
    "gpus": "GPUs to use per rank",
    "openmp": "OpenMP groups to use per rank",
    "ompthreads": "Threads per OpenMP group",
    "utility": "Utility processors per rank",
}

memory = {
    "sysmem": "DRAM memory per rank (in MBs)",
    "numamem": "DRAM memory per NUMA domain per rank (in MBs)",
    "fbmem": "Framebuffer memory per GPU (in MBs)",
    "zcmem": "Zero-copy memory per rank (in MBs)",
    "regmem": "Registered CPU-side pinned memory per rank (in MBs)",
}


class LegateInfo:
    config: LegateMetadata

    def __init__(self) -> None:
        if LEGATE_JUPYTER_KERNEL_SPEC_KEY not in os.environ:
            raise RuntimeError("Cannot determine currently running kernel")

        spec_name = os.environ[LEGATE_JUPYTER_KERNEL_SPEC_KEY]

        try:
            spec = KernelSpecManager().get_kernel_spec(spec_name)
        except NoSuchKernel:
            raise RuntimeError(
                f"Cannot find a Legate Jupyter kernel named {spec_name!r}"
            )

        self.spec_name = spec_name
        self.config = spec.metadata[LEGATE_JUPYTER_METADATA_KEY]

    def __str__(self) -> str:
        nodes = self.config["multi_node"]["nodes"]
        header = f"Kernel {self.spec_name!r} configured for {nodes} node(s)"
        core_table = {
            desc: self.config["core"][field] for field, desc in core.items()
        }
        memory_table = {
            desc: self.config["memory"][field]
            for field, desc in memory.items()
        }

        out = f"""{header}

Cores:
{indent(kvtable(core_table, align=False),  prefix='  ')}

Memory:
{indent(kvtable(memory_table, align=False), prefix='  ')}
"""
        # remove any text colors in notebook
        return scrub(out)


@magics_class
class LegateInfoMagics(Magics):
    def __init__(self, shell: InteractiveShell | None = None) -> None:
        super().__init__(shell=shell)
        self.info = LegateInfo()

    @line_magic
    def legate_info(self, line: str) -> None:
        print(self.info)
