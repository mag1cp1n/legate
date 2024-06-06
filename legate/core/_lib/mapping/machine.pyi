# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from collections.abc import Iterable
from dataclasses import dataclass
from typing import Any

from .mapping import TaskTarget

class EmptyMachineError(Exception):
    pass

@dataclass(frozen=True)
class ProcessorSlice:
    target: TaskTarget
    slice: slice

PROC_RANGE_KEY = slice | int
MACHINE_KEY = TaskTarget | slice | int | ProcessorSlice

class ProcessorRange:
    @staticmethod
    def create(low: int, high: int, per_node_count: int) -> ProcessorRange: ...
    @staticmethod
    def create_empty() -> ProcessorRange: ...
    @property
    def low(self) -> int: ...
    @property
    def high(self) -> int: ...
    @property
    def per_node_count(self) -> int: ...
    @property
    def count(self) -> int: ...
    def __len__(self) -> int: ...
    @property
    def empty(self) -> bool: ...
    def slice(self, sl: slice) -> ProcessorRange: ...
    def __getitem__(self, key: PROC_RANGE_KEY) -> ProcessorRange: ...
    def get_node_range(self) -> tuple[int, ...]: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...
    def __and__(self, other: object) -> ProcessorRange: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __lt__(self, other: object) -> bool: ...

class Machine:
    def __init__(
        self,
        ranges: dict[TaskTarget, ProcessorRange] | None = None,
    ): ...
    @property
    def preferred_target(self) -> TaskTarget: ...
    def get_processor_range(
        self, target: TaskTarget | None = None
    ) -> ProcessorRange: ...
    def get_node_range(
        self, target: TaskTarget | None = None
    ) -> tuple[int, ...]: ...
    def processor_range(self, target: TaskTarget | None) -> ProcessorRange: ...
    @property
    def valid_targets(self) -> tuple[int]: ...
    @property
    def empty(self) -> bool: ...
    def count(self, target: TaskTarget | None = None) -> int: ...
    def __len__(self) -> int: ...
    def only(self, targets: TaskTarget | Iterable[TaskTarget]) -> Machine: ...
    def slice(
        self,
        sl: slice,
        target: TaskTarget | None = None,
    ) -> Machine: ...
    def __getitem__(self, slicer: MACHINE_KEY) -> Machine: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __and__(self, other: object) -> Machine: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...
    def __enter__(self) -> None: ...
    def __exit__(self, _: Any, __: Any, ___: Any) -> None: ...
