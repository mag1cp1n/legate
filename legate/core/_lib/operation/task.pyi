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

from typing import TYPE_CHECKING, Any, Iterable, Optional, Union

if TYPE_CHECKING:
    from ..data.logical_array import LogicalArray
    from ..data.logical_store import LogicalStore, LogicalStorePartition
    from ..partitioning.constraint import Constraint, Variable
    from ..type.type_info import Type
    from .projection import SymbolicPoint

class AutoTask:
    def lock(self) -> None: ...
    def add_input(
        self,
        array_or_store: Union[LogicalArray, LogicalStore],
        variable: Union[Variable, None] = None,
    ) -> Variable: ...
    def add_output(
        self,
        array_or_store: Union[LogicalArray, LogicalStore],
        variable: Union[Variable, None] = None,
    ) -> Variable: ...
    def add_reduction(
        self,
        array_or_store: Union[LogicalArray, LogicalStore],
        redop: int,
        variable: Union[Variable, None] = None,
    ) -> Variable: ...
    def add_scalar_arg(
        self,
        value: Any,
        dtype: Union[Type, tuple[Type, ...], None] = None,
    ) -> None: ...
    def add_constraint(self, constraint: Constraint) -> None: ...
    def find_or_declare_partition(self, array: LogicalArray) -> Variable: ...
    def declare_partition(self) -> Variable: ...
    def provenance(self) -> str: ...
    def set_concurrent(self, concurrent: bool) -> None: ...
    def set_side_effect(self, has_side_effect: bool) -> None: ...
    def throws_exception(self, exception_type: type) -> None: ...
    def exception_types(self) -> tuple[type]: ...
    def add_communicator(self, name: str) -> None: ...
    def execute(self) -> None: ...
    def add_alignment(
        self,
        array_or_store1: Union[LogicalArray, LogicalStore],
        array_or_store2: Union[LogicalArray, LogicalStore],
    ) -> None: ...
    def add_broadcast(
        self,
        array_or_store: Union[LogicalArray, LogicalStore],
        axes: Union[None, int, Iterable[int]] = None,
    ) -> None: ...
    def add_nccl_communicator(self) -> None: ...
    def add_cpu_communicator(self) -> None: ...
    def add_cal_communicator(self) -> None: ...
    @property
    def raw_handle(self) -> int: ...

class ManualTask:
    def add_input(
        self,
        arg: Union[LogicalStore, LogicalStorePartition],
        projection: Optional[SymbolicPoint] = None,
    ) -> None: ...
    def add_output(
        self,
        arg: Union[LogicalStore, LogicalStorePartition],
        projection: Optional[SymbolicPoint] = None,
    ) -> None: ...
    def add_reduction(
        self,
        arg: Union[LogicalStore, LogicalStorePartition],
        redop: int,
        projection: Optional[SymbolicPoint] = None,
    ) -> None: ...
    def add_scalar_arg(
        self,
        value: Any,
        dtype: Union[Type, tuple[Type, ...], None] = None,
    ) -> None: ...
    def provenance(self) -> str: ...
    def set_concurrent(self, concurrent: bool) -> None: ...
    def set_side_effect(self, has_side_effect: bool) -> None: ...
    def throws_exception(self, exception_type: type) -> None: ...
    def exception_types(self) -> tuple[type]: ...
    def add_communicator(self, name: str) -> None: ...
    def execute(self) -> None: ...
    def add_nccl_communicator(self) -> None: ...
    def add_cpu_communicator(self) -> None: ...
    def add_cal_communicator(self) -> None: ...
    @property
    def raw_handle(self) -> int: ...
