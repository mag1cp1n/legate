# SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: Apache-2.0
from cpython.bytes cimport PyBytes_AS_STRING, PyBytes_Check, PyBytes_GET_SIZE
from cpython.unicode cimport (
    PyUnicode_AsUTF8AndSize,
    PyUnicode_Check,
    PyUnicode_FromStringAndSize,
)


cdef extern from "<string_view>" namespace "std::string_view" nogil:
    const size_t npos

cdef extern from "<string_view>" namespace "std" nogil:
    cdef cppclass std_string_view "std::string_view":
        ctypedef char value_type

        # these should really be allocator_type.size_type and
        # allocator_type.difference_type to be true to the C++ definition
        # but cython doesn't support deferred access on template arguments
        ctypedef size_t size_type
        ctypedef ptrdiff_t difference_type

        cppclass const_iterator
        cppclass iterator:
            iterator()
            iterator(iterator&)
            value_type& operator*()
            iterator operator++()
            iterator operator--()
            iterator operator++(int)
            iterator operator--(int)
            iterator operator+(size_type)
            iterator operator-(size_type)
            difference_type operator-(iterator)
            difference_type operator-(const_iterator)
            bint operator==(iterator)
            bint operator==(const_iterator)
            bint operator!=(iterator)
            bint operator!=(const_iterator)
            bint operator<(iterator)
            bint operator<(const_iterator)
            bint operator>(iterator)
            bint operator>(const_iterator)
            bint operator<=(iterator)
            bint operator<=(const_iterator)
            bint operator>=(iterator)
            bint operator>=(const_iterator)
        cppclass const_iterator:
            const_iterator()
            const_iterator(iterator&)
            const_iterator(const_iterator&)
            operator=(iterator&)
            const value_type& operator*()
            const_iterator operator++()
            const_iterator operator--()
            const_iterator operator++(int)
            const_iterator operator--(int)
            const_iterator operator+(size_type)
            const_iterator operator-(size_type)
            difference_type operator-(iterator)
            difference_type operator-(const_iterator)
            bint operator==(iterator)
            bint operator==(const_iterator)
            bint operator!=(iterator)
            bint operator!=(const_iterator)
            bint operator<(iterator)
            bint operator<(const_iterator)
            bint operator>(iterator)
            bint operator>(const_iterator)
            bint operator<=(iterator)
            bint operator<=(const_iterator)
            bint operator>=(iterator)
            bint operator>=(const_iterator)

        cppclass const_reverse_iterator
        cppclass reverse_iterator:
            reverse_iterator()
            reverse_iterator(reverse_iterator&)
            value_type& operator*()
            reverse_iterator operator++()
            reverse_iterator operator--()
            reverse_iterator operator++(int)
            reverse_iterator operator--(int)
            reverse_iterator operator+(size_type)
            reverse_iterator operator-(size_type)
            difference_type operator-(iterator)
            difference_type operator-(const_iterator)
            bint operator==(reverse_iterator)
            bint operator==(const_reverse_iterator)
            bint operator!=(reverse_iterator)
            bint operator!=(const_reverse_iterator)
            bint operator<(reverse_iterator)
            bint operator<(const_reverse_iterator)
            bint operator>(reverse_iterator)
            bint operator>(const_reverse_iterator)
            bint operator<=(reverse_iterator)
            bint operator<=(const_reverse_iterator)
            bint operator>=(reverse_iterator)
            bint operator>=(const_reverse_iterator)
        cppclass const_reverse_iterator:
            const_reverse_iterator()
            const_reverse_iterator(reverse_iterator&)
            operator=(reverse_iterator&)
            const value_type& operator*()
            const_reverse_iterator operator++()
            const_reverse_iterator operator--()
            const_reverse_iterator operator++(int)
            const_reverse_iterator operator--(int)
            const_reverse_iterator operator+(size_type)
            const_reverse_iterator operator-(size_type)
            difference_type operator-(iterator)
            difference_type operator-(const_iterator)
            bint operator==(reverse_iterator)
            bint operator==(const_reverse_iterator)
            bint operator!=(reverse_iterator)
            bint operator!=(const_reverse_iterator)
            bint operator<(reverse_iterator)
            bint operator<(const_reverse_iterator)
            bint operator>(reverse_iterator)
            bint operator>(const_reverse_iterator)
            bint operator<=(reverse_iterator)
            bint operator<=(const_reverse_iterator)
            bint operator>=(reverse_iterator)
            bint operator>=(const_reverse_iterator)

        std_string_view()
        std_string_view(const std_string_view& s)
        std_string_view(const char* s, size_t n) except +
        std_string_view(const char* s) except +
        # Since C++20
        # string_view[It, End](It first, End last) except +
        # Since C++23
        # string_view[R](R&&) except +
        # string_view(nullptr_t) = delete

        iterator begin()
        const_iterator const_begin "begin"()
        const_iterator cbegin()
        iterator end()
        const_iterator const_end "end"()
        const_iterator cend()
        reverse_iterator rbegin()
        const_reverse_iterator const_rbegin "rbegin"()
        const_reverse_iterator crbegin()
        reverse_iterator rend()
        const_reverse_iterator const_rend "rend"()
        const_reverse_iterator crend()

        char& operator[](size_t pos)
        char& at(size_t pos) except +
        char& front()
        char& back()
        const char* data()

        size_t size()
        size_t length()
        size_t max_size()
        bint empty()

        void remove_prefix(size_type)
        void remove_suffix(size_type)
        void swap(std_string_view& other)

        size_t copy(char* s, size_t len, size_t pos) except +
        size_t copy(char* s, size_t len) except +

        std_string_view substr(size_t pos, size_t len) except +
        std_string_view substr(size_t pos) except +
        std_string_view substr()

        int compare(const std_string_view& s)
        int compare(size_t pos, size_t len, const std_string_view& s) except +
        int compare(
            size_t pos,
            size_t len,
            const std_string_view& s,
            size_t subpos,
            size_t sublen
        ) except +
        int compare(const char* s) except +
        int compare(size_t pos, size_t len, const char* s) except +
        int compare(size_t pos, size_t len, const char* s , size_t n) except +

        size_t find(const std_string_view& s, size_t pos)
        size_t find(const std_string_view& s)
        size_t find(const char* s, size_t pos, size_t n)
        size_t find(const char* s, size_t pos)
        size_t find(const char* s)
        size_t find(char c, size_t pos)
        size_t find(char c)

        size_t rfind(const std_string_view&, size_t pos)
        size_t rfind(const std_string_view&)
        size_t rfind(const char* s, size_t pos, size_t n)
        size_t rfind(const char* s, size_t pos)
        size_t rfind(const char* s)
        size_t rfind(char c, size_t pos)
        size_t rfind(char c)

        size_t find_first_of(const std_string_view&, size_t pos)
        size_t find_first_of(const std_string_view&)
        size_t find_first_of(const char* s, size_t pos, size_t n)
        size_t find_first_of(const char* s, size_t pos)
        size_t find_first_of(const char* s)
        size_t find_first_of(char c, size_t pos)
        size_t find_first_of(char c)

        size_t find_first_not_of(const std_string_view& s, size_t pos)
        size_t find_first_not_of(const std_string_view& s)
        size_t find_first_not_of(const char* s, size_t pos, size_t n)
        size_t find_first_not_of(const char* s, size_t pos)
        size_t find_first_not_of(const char*)
        size_t find_first_not_of(char c, size_t pos)
        size_t find_first_not_of(char c)

        size_t find_last_of(const std_string_view& s, size_t pos)
        size_t find_last_of(const std_string_view& s)
        size_t find_last_of(const char* s, size_t pos, size_t n)
        size_t find_last_of(const char* s, size_t pos)
        size_t find_last_of(const char* s)
        size_t find_last_of(char c, size_t pos)
        size_t find_last_of(char c)

        size_t find_last_not_of(const std_string_view& s, size_t pos)
        size_t find_last_not_of(const std_string_view& s)
        size_t find_last_not_of(const char* s, size_t pos, size_t n)
        size_t find_last_not_of(const char* s, size_t pos)
        size_t find_last_not_of(const char* s)
        size_t find_last_not_of(char c, size_t pos)
        size_t find_last_not_of(char c)

        bint operator==(const std_string_view&)
        bint operator==(const char*)

        bint operator!= (const std_string_view&)
        bint operator!= (const char*)

        bint operator< (const std_string_view&)
        bint operator< (const char*)

        bint operator> (const std_string_view&)
        bint operator> (const char*)

        bint operator<= (const std_string_view&)
        bint operator<= (const char*)

        bint operator>= (const std_string_view&)
        bint operator>= (const char*)


ctypedef fused string_like:
    str
    bytes

cdef inline std_string_view std_string_view_from_py(string_like obj):
    cdef const char* data = NULL
    cdef Py_ssize_t ssize = 0

    if string_like is str:
        data = PyUnicode_AsUTF8AndSize(obj, &ssize)
        if not data:
            raise RuntimeError("error unpacking string as utf-8")
    else:
        data = PyBytes_AS_STRING(obj)
        ssize = PyBytes_GET_SIZE(obj)
    return std_string_view(data, <size_t>ssize)

cdef inline str str_from_string_view(std_string_view sv):
    return PyUnicode_FromStringAndSize(sv.data(), sv.size())
