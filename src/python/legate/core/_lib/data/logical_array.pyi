# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from collections.abc import Collection
from typing import Any

from ...data_interface import LegateDataInterfaceItem
from ..mapping.mapping import StoreTarget
from ..type.types import Type
from ..utilities.unconstructable import Unconstructable
from .logical_store import LogicalStore
from .physical_array import PhysicalArray
from .shape import Shape

class LogicalArray(Unconstructable):
    @staticmethod
    def from_store(store: LogicalStore) -> LogicalArray: ...
    @staticmethod
    def from_raw_handle(raw_handle: int) -> LogicalArray: ...
    @property
    def shape(self) -> Shape: ...
    @property
    def ndim(self) -> int: ...
    @property
    def type(self) -> Type: ...
    @property
    def extents(self) -> tuple[int, ...]: ...
    @property
    def volume(self) -> int: ...
    @property
    def size(self) -> int: ...
    @property
    def unbound(self) -> bool: ...
    @property
    def nullable(self) -> bool: ...
    @property
    def nested(self) -> bool: ...
    @property
    def num_children(self) -> int: ...
    @property
    def __legate_data_interface__(self) -> LegateDataInterfaceItem: ...
    def __getitem__(
        self, indices: int | slice | tuple[int | slice, ...]
    ) -> LogicalArray: ...
    def promote(self, extra_dim: int, dim_size: int) -> LogicalArray: ...
    def project(self, dim: int, index: int) -> LogicalArray: ...
    def slice(self, dim: int, sl: slice) -> LogicalArray: ...
    def transpose(self, axes: Collection[int]) -> LogicalArray: ...
    def delinearize(
        self, dim: int, shape: Collection[int]
    ) -> LogicalArray: ...
    def fill(self, value: Any) -> None: ...
    @property
    def data(self) -> LogicalStore: ...
    @property
    def null_mask(self) -> LogicalStore: ...
    def child(self, index: int) -> LogicalArray: ...
    def get_physical_array(self) -> PhysicalArray: ...
    @property
    def raw_handle(self) -> int: ...
    def offload_to(self, target_mem: StoreTarget) -> None: ...
