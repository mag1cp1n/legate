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

from libc.stdint cimport int32_t
from libcpp.vector cimport vector as std_vector

from ..mapping.mapping cimport StoreTarget
from ..type.type_info cimport _Type
from ..utilities.typedefs cimport _Domain
from ..utilities.unconstructable cimport Unconstructable
from .inline_allocation cimport InlineAllocation, _InlineAllocation


cdef extern from "core/data/physical_store.h" namespace "legate" nogil:
    cdef cppclass _PhysicalStore "legate::PhysicalStore":
        int32_t dim()
        _Type type()
        _Domain domain()
        _InlineAllocation get_inline_allocation() except+
        StoreTarget target()


cdef class PhysicalStore(Unconstructable):
    cdef _PhysicalStore _handle

    @staticmethod
    cdef PhysicalStore from_handle(_PhysicalStore)

    cpdef InlineAllocation get_inline_allocation(self)
