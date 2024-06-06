# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES.
# All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from typing import Any

from ..type.type_info import Type
from ..utilities.typedefs import Domain
from .physical_store import PhysicalStore

class PhysicalArray:
    @property
    def nullable(self) -> bool: ...
    @property
    def ndim(self) -> int: ...
    @property
    def type(self) -> Type: ...
    @property
    def nested(self) -> bool: ...
    def data(self) -> PhysicalStore: ...
    def null_mask(self) -> PhysicalStore: ...
    def child(self, index: int) -> PhysicalArray: ...
    def domain(self) -> Domain: ...
    @property
    def __array_interface__(self) -> dict[str, Any]: ...
    @property
    def __cuda_array_interface__(self) -> dict[str, Any]: ...
