#
# Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#

set(prefix ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(exec_prefix "${prefix}")

add_library(ucc::ucc SHARED IMPORTED)

set_target_properties(ucc::ucc PROPERTIES
  IMPORTED_LOCATION "${exec_prefix}/lib/libucc.so"
  INTERFACE_INCLUDE_DIRECTORIES "${prefix}/include"
)
