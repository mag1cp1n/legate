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
from __future__ import annotations

import json
import os
from typing import TypedDict

from ._io import vprint
from ._legate_config import get_legate_config


class CMakeSpec(TypedDict):
    CMAKE_GENERATOR: str
    CMAKE_COMMANDS: list[str]


class CMakeConfig:
    def __init__(self) -> None:
        # We wish to configure the build using exactly the same arguments as
        # the C++ lib so that ./configure options are respected.
        cmd_spec_path = (
            get_legate_config().LEGATE_CMAKE_DIR
            / "aedifix_cmake_command_spec.json"
        )
        vprint(f"Using cmake_command file: {cmd_spec_path}")
        with cmd_spec_path.open() as fd:
            cmake_spec: CMakeSpec = json.load(fd)

        cmake_args = self._read_cmake_args(cmake_spec)
        cmake_args, cmake_defines = self._split_out_cmake_defines(cmake_args)

        try:
            build_type = cmake_defines["CMAKE_BUILD_TYPE"]
        except KeyError:
            build_type = "Release"
            vprint(f"Using default build type: {build_type}")
        else:
            vprint(f"Found build type: {build_type}")

        self._build_type = build_type
        self._cmake_args = cmake_args
        self._cmake_defines = cmake_defines
        self._generator = cmake_spec["CMAKE_GENERATOR"]

    @staticmethod
    def _read_cmake_args(cmake_spec: CMakeSpec) -> list[str]:
        def read_env_args(name: str) -> list[str]:
            args = (x.strip() for x in os.environ.get(name, "").split(";"))
            return [a for a in args if a]

        cmake_args = [
            arg
            for arg in cmake_spec["CMAKE_COMMANDS"]
            if "CMAKE_INSTALL_PREFIX" not in arg
        ]
        vprint(f"Initialized cmake args from command spec: {cmake_args}")

        for name in ("CMAKE_ARGS", "SKBUILD_CMAKE_ARGS"):
            env_args = read_env_args(name)
            vprint(f"Adding {name} to cmake_args: {env_args}")
            cmake_args.extend(env_args)

        return cmake_args

    @staticmethod
    def _split_out_cmake_defines(
        cmake_args: list[str],
    ) -> tuple[list[str], dict[str, str]]:
        cmake_defines = {}
        new_cmake_args = []

        for arg in cmake_args:
            if arg.startswith("-D"):
                name, _, value = arg.partition("=")
                name = name.removeprefix("-D").rpartition(":")[0]
                cmake_defines[name] = value
            else:
                new_cmake_args.append(arg)
        return new_cmake_args, cmake_defines

    @property
    def cmake_defines(self) -> dict[str, str]:
        return self._cmake_defines

    @property
    def cmake_args(self) -> list[str]:
        return self._cmake_args

    @property
    def build_type(self) -> str:
        return self._build_type

    @property
    def generator(self) -> str:
        return self._generator
