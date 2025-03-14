# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: Apache-2.0


import math

import numpy as np

from libc.stdint cimport int32_t, uintptr_t

from ..type.types cimport Type
from ..utilities.typedefs cimport _Domain, _DomainPoint
from .physical_store cimport PhysicalStore


cdef class InlineAllocation:
    @staticmethod
    cdef InlineAllocation create(
        PhysicalStore store, _InlineAllocation handle
    ):
        cdef InlineAllocation result = InlineAllocation.__new__(
            InlineAllocation
        )
        result._handle = handle
        result._store = store
        result._shape = None
        return result

    @property
    def ptr(self) -> uintptr_t:
        r"""
        Access the raw pointer to the allocation.

        :returns: The raw pointer to the allocation.
        :rtype: int
        """
        return <uintptr_t>(self._handle.ptr)

    @property
    def strides(self) -> tuple[size_t, ...]:
        r"""
        Retrieve the strides of the allocation.

        If the allocation has dimension 0, an empty tuple is returned.

        :returns: The strides of the allocation.
        :rtype: tuple[int, ...]
        """
        return () if self._store.ndim == 0 else tuple(self._handle.strides)

    @property
    def shape(self) -> tuple[size_t, ...]:
        r"""
        Retrieve the shape of the allocation.

        :returns: The shape of the allocation.
        :rtype: tuple[int, ...]
        """
        if self._store.ndim == 0:
            return ()

        if self._shape is not None:
            return self._shape

        cdef _Domain domain = self._store._handle.domain()
        cdef _DomainPoint lo = domain.lo()
        cdef _DomainPoint hi = domain.hi()
        cdef int32_t ndim = domain.get_dim()

        self._shape = tuple(max(hi[i] - lo[i] + 1, 0) for i in range(ndim))
        return self._shape

    cdef dict _get_array_interface(self):
        cdef Type ty = self._store.type
        cdef tuple shape = self.shape

        if math.prod(shape) == 0:
            # For some reason NumPy doesn't like a null pointer even when the
            # array size is 0, so we just make an empty ndarray and return its
            # array interface object
            return np.empty(
                shape, dtype=ty.to_numpy_dtype()
            ).__array_interface__

        return {
            "version": 3,
            "shape": shape,
            "typestr": ty.to_numpy_dtype().str,
            "data": (self.ptr, False),
            "strides": self.strides,
        }

    @property
    def __array_interface__(self) -> dict[str, Any]:
        r"""
        Retrieve the numpy-compatible array representation of the allocation.

        :returns: The numpy array interface dict.
        :rtype: dict[str, Any]

        :raises ValueError: If the allocation is allocated on the GPU.
        """
        if self._store.target == StoreTarget.FBMEM:
            raise ValueError(
                "Physical store in a framebuffer memory does not support "
                "the array interface"
            )
        return self._get_array_interface()

    @property
    def __cuda_array_interface__(self) -> dict[str, Any]:
        r"""
        Retrieve the cupy-compatible array representation of the allocation.

        :returns: The cupy array interface dict.
        :rtype: dict[str, Any]

        :raises ValueError: If the array is in host-only memory
        """
        if self._store.target not in (StoreTarget.FBMEM, StoreTarget.ZCMEM):
            raise ValueError(
                "Physical store in a host-only memory does not support "
                "the CUDA array interface"
            )
        # TODO(wonchanl): We should add a Legate-managed stream to the returned
        # interface object
        return self._get_array_interface()

    def __str__(self) -> str:
        r"""
        Return a human-readable string representation of the allocation.

        Returns
        -------
        str
            The string representation.
        """
        return f"InlineAllocation({self.ptr}, {self.strides})"

    def __repr__(self) -> str:
        r"""
        Return a human-readable string representation of the allocation.

        Returns
        -------
        str
            The string representation.
        """
        return str(self)
