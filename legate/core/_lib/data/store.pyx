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


from libc.stdint cimport int32_t, uintptr_t

from ..type.type_info cimport Type
from ..utilities.typedefs cimport Domain, DomainPoint
from .store cimport _Store


cdef class Store:
    @staticmethod
    cdef Store from_handle(_Store handle):
        cdef Store result = Store.__new__(Store)
        result._handle = handle
        return result

    @property
    def ndim(self) -> int32_t:
        return self._handle.dim()

    @property
    def type(self) -> Type:
        return Type.from_handle(self._handle.type())

    @property
    def domain(self) -> Domain:
        return Domain.from_handle(self._handle.domain())

    def get_inline_allocation(self) -> InlineAllocation:
        return InlineAllocation.create(
            self,
            self._handle.get_inline_allocation()
        )


cdef class InlineAllocation:
    @staticmethod
    cdef InlineAllocation create(Store store, _InlineAllocation handle):
        cdef InlineAllocation result = InlineAllocation.__new__(
            InlineAllocation
        )
        result._handle = handle
        result._store = store
        return result

    @property
    def ptr(self) -> uintptr_t:
        return <long>(self._handle.ptr)

    @property
    def strides(self) -> tuple[size_t]:
        return () if self._store.ndim == 0 else tuple(self._handle.strides)

    @property
    def shape(self) -> tuple[size_t, ...]:
        result = ()
        if self._store.ndim == 0:
            return result

        cdef Domain domain = self._store.domain
        cdef DomainPoint lo = domain.lo
        cdef DomainPoint hi = domain.hi
        cdef int32_t ndim = domain.dim

        for i in range(ndim):
            result = (*result, hi[i] - lo[i] + 1)

        return result

    @property
    def __array_interface__(self):
        ty = self._store.type
        if ty.variable_size:
            raise ValueError(
                "Stores with variable size types don't support "
                "array interface"
            )
        return {
            "version": 3,
            "shape": self.shape,
            "typestr": ty.to_numpy_dtype().str,
            "data": (self.ptr, False),
            "strides": self.strides,
        }

    def __str__(self) -> str:
        return f"InlineAllocation({self.ptr}, {self.strides})"

    def __repr__(self) -> str:
        return str(self)
