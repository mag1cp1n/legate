#=============================================================================
# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#=============================================================================

include_guard(GLOBAL)

function(find_or_configure_mdspan)
  list(APPEND CMAKE_MESSAGE_CONTEXT "mdspan")

  if(CMAKE_CXX_STANDARD GREATER_EQUAL 23)
    include(CheckIncludeFileCXX)

    check_include_file_cxx("mdspan" have_std_mdspan)
    if(have_std_mdspan)
      return()
    endif()
  endif()

  include("${rapids-cmake-dir}/cpm/detail/package_details.cmake")
  rapids_cpm_package_details(mdspan version git_url git_tag git_shallow exclude_from_all)

  # MDSpan is strictly header-only so our dependency install is a little unorthodox.
  # Instead of using EXCLUDE_FROM_ALL + custom install to ensure we put it in the right
  # place, we must install() it normally, but point the default CMake install locations to
  # our private directories.
  #
  # The reason for this is that install(TARGETS) (which is what
  # legate_install_dependencies() uses) would not install anything unless the package
  # declares PUBLIC_HEADER or PRIVATE_HEADER. MDSpan does neither of these, so we need to
  # do things this way.
  if(exclude_from_all)
    message(FATAL_ERROR "mdspan must NOT have a truthy EXCLUDE_FROM_ALL (have "
                        "${exclude_from_all}). Setting this to true has no effect, and "
                        "will ensure that the installed package is ill-formed!")
  endif()
  rapids_cpm_find(mdspan "${version}"
                  BUILD_EXPORT_SET legate-exports
                  INSTALL_EXPORT_SET legate-exports
                  CPM_ARGS
                  GIT_REPOSITORY "${git_url}"
                  GIT_SHALLOW "${git_shallow}" SYSTEM TRUE
                  GIT_TAG "${git_tag}"
                  OPTIONS # Got to set this, otherwise mdspan tries to guess a C++
                          # standard
                          "MDSPAN_CXX_STANDARD ${CMAKE_CXX_STANDARD}"
                          "CMAKE_INSTALL_INCLUDEDIR ${legate_DEP_INSTALL_INCLUDEDIR}"
                          "CMAKE_INSTALL_LIBDIR ${legate_DEP_INSTALL_LIBDIR}")

  include("${rapids-cmake-dir}/export/find_package_root.cmake")
  rapids_export_find_package_root(INSTALL mdspan
                                  [=[${CMAKE_CURRENT_LIST_DIR}/../../legate/deps/cmake]=]
                                  EXPORT_SET legate-exports)
  cpm_export_variables(mdspan)
endfunction()
