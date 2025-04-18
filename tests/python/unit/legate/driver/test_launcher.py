# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import pytest

import legate.driver.launcher as m
from legate.util.shared_args import LAUNCHERS
from legate.util.system import System

from ...util import powerset_nonempty

if TYPE_CHECKING:
    from legate.util.types import LauncherType

    from .util import GenConfig

SYSTEM = System()


def test_RANK_ENV_VARS() -> None:
    assert m.RANK_ENV_VARS == (
        "OMPI_COMM_WORLD_RANK",
        "PMI_RANK",
        "MV2_COMM_WORLD_RANK",
        "SLURM_PROCID",
    )


def test_LAUNCHER_VAR_PREFIXES() -> None:
    assert m.LAUNCHER_VAR_PREFIXES == (
        "CONDA_",
        "CUTENSOR_",
        "LEGATE_",
        "LEGION_",
        "LG_",
        "REALM_",
        "GASNET_",
        "PYTHON",
        "UCX_",
        "NCCL_",
        "CUPYNUMERIC_",
        "NVIDIA_",
        "LD_",
    )


class TestLauncher:
    @pytest.mark.parametrize("prefix", m.LAUNCHER_VAR_PREFIXES)
    def test_is_launcher_var_good_prefix(self, prefix: str) -> None:
        assert m.Launcher.is_launcher_var(f"{prefix}abcxyz")

    def test_is_launcher_var_good_path_suffix(self) -> None:
        assert m.Launcher.is_launcher_var("abcxyzPATH")

    @pytest.mark.parametrize("prefix", m.LAUNCHER_VAR_PREFIXES)
    def test_is_launcher_var_bad_not_prefix(self, prefix: str) -> None:
        assert not m.Launcher.is_launcher_var(f"qq{prefix}abcxyz")
        assert not m.Launcher.is_launcher_var(f"qq{prefix}")

    def test_is_launcher_var_bad_path_not_suffix(self) -> None:
        assert not m.Launcher.is_launcher_var("abcxyzPATHqq")
        assert not m.Launcher.is_launcher_var("PATHqq")

    @pytest.mark.parametrize("value", ("a123", "_ABC", "ABC_"))
    def test_is_launcher_var_bad_junk(self, value: str) -> None:
        assert not m.Launcher.is_launcher_var(value)

    def test_create_launcher_none(self, genconfig: GenConfig) -> None:
        config = genconfig([])

        launcher = m.Launcher.create(config, SYSTEM)

        assert isinstance(launcher, m.SimpleLauncher)

    def test_create_launcher_mpirun(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "mpirun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert isinstance(launcher, m.MPILauncher)

    def test_create_launcher_jsrun(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "jsrun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert isinstance(launcher, m.JSRunLauncher)

    def test_create_launcher_srun(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "srun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert isinstance(launcher, m.SRunLauncher)


@pytest.mark.parametrize("launch", LAUNCHERS)
class TestLauncher__eq__:
    def test_identity(
        self, genconfig: GenConfig, launch: LauncherType
    ) -> None:
        config = genconfig(["--launcher", launch])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher == launcher  # noqa: PLR0124

    def test_identical_config(
        self, genconfig: GenConfig, launch: LauncherType
    ) -> None:
        config1 = genconfig(["--launcher", launch])
        config2 = genconfig(["--launcher", launch])

        launcher1 = m.Launcher.create(config1, SYSTEM)
        launcher2 = m.Launcher.create(config2, SYSTEM)

        assert launcher1 == launcher2
        assert launcher1.kind == launcher2.kind
        assert launcher1.detected_rank_id == launcher2.detected_rank_id
        assert launcher1.cmd == launcher2.cmd
        assert launcher1.env == launcher2.env


@pytest.mark.parametrize("launch", LAUNCHERS)
class TestLauncherEnv:
    def test_no_bytecode(
        self, genconfig: GenConfig, launch: LauncherType
    ) -> None:
        config = genconfig(["--launcher", launch])

        env = m.Launcher.create(config, SYSTEM).env

        assert env["PYTHONDONTWRITEBYTECODE"] == "1"

    def test_pythonpath(
        self,
        genconfig: GenConfig,
        monkeypatch: pytest.MonkeyPatch,
        launch: LauncherType,
    ) -> None:
        monkeypatch.setenv("PYTHONPATH", "/foo/bar")
        system = System()
        config = genconfig(["--launcher", launch])

        env = m.Launcher.create(config, system).env

        assert env["PYTHONPATH"].split(os.pathsep)[0] == "/foo/bar"

    def test_gasnet_trace(
        self, genconfig: GenConfig, launch: LauncherType
    ) -> None:
        config = genconfig(["--launcher", launch, "--gasnet-trace"])

        env = m.Launcher.create(config, SYSTEM).env

        assert env["GASNET_TRACEFILE"] == str(
            config.logging.logdir / "gasnet_%.log"
        )


class TestSimpleLauncher:
    def test_single_rank(self, genconfig: GenConfig) -> None:
        config = genconfig([])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == ()

    def test_single_rank_launcher_extra_ignored(
        self, genconfig: GenConfig
    ) -> None:
        config = genconfig(
            ["--launcher-extra", "foo", "--launcher-extra", "bar"]
        )

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == ()

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(multi_rank=(100, 2))
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == ()

    def test_multi_rank_bad(self, genconfig: GenConfig) -> None:
        config = genconfig([], multi_rank=(2, 2))

        msg = (
            "Could not detect rank ID on multi-rank "
            "run with no --launcher provided."
        )
        with pytest.raises(RuntimeError, match=msg):
            m.Launcher.create(config, SYSTEM)

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank_launcher_extra_ignored(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(
            ["--launcher-extra", "foo", "--launcher-extra", "bar"],
            multi_rank=(100, 2),
        )
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == ()


class TestMPILauncher:
    XARGS = (
        "-x",
        "DEFAULTS_PATH",
        "-x",
        "XDG_SESSION_PATH",
        "-x",
        "MANDATORY_PATH",
        "-x",
        "XDG_SEAT_PATH",
        "-x",
        "PATH",
        "-x",
        "PKG_CONFIG_PATH",
        "-x",
        "CONDA_EXE",
        "-x",
        "CONDA_PYTHON_EXE",
        "-x",
        "CONDA_SHLVL",
        "-x",
        "CONDA_PREFIX",
        "-x",
        "CONDA_DEFAULT_ENV",
        "-x",
        "CONDA_PROMPT_MODIFIER",
        "-x",
        "NODE_BIN_PATH",
        "-x",
        "NODE_INCLUDE_PATH",
        "-x",
        "CONDA_PREFIX_1",
        "-x",
        "CONDA_BACKUP_HOST",
        "-x",
        "CONDA_TOOLCHAIN_HOST",
        "-x",
        "CONDA_TOOLCHAIN_BUILD",
        "-x",
        "CMAKE_PREFIX_PATH",
        "-x",
        "CONDA_BUILD_SYSROOT",
        "-x",
        "PYTHONDONTWRITEBYTECODE",
        "-x",
        "PYTHONPATH",
        "-x",
        "NCCL_LAUNCH_MODE",
        "-x",
        "UCC_TLS",
        "-x",
        "GASNET_MPI_THREAD",
        "-x",
        "LD_LIBRARY_PATH",
        "-x",
        "LEGATE_MAX_DIM",
        "-x",
        "LEGATE_MAX_FIELDS",
        "-x",
        "REALM_BACKTRACE",
    )

    def test_single_rank(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "mpirun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"

        # TODO (bv) -x env args currently too fragile to test
        assert launcher.cmd[:10] == (
            (
                "mpirun",
                "-n",
                "1",
                "--npernode",
                "1",
                "--bind-to",
                "none",
                "--mca",
                "mpi_warn_on_fork",
                "0",
            )
            # + self.XARGS
        )

        # at least make sure LEGATE_CONFIG is forwarded
        assert "LEGATE_CONFIG" in launcher.cmd
        assert launcher.cmd[launcher.cmd.index("LEGATE_CONFIG") - 1] == "-x"

    def test_single_rank_launcher_extra(self, genconfig: GenConfig) -> None:
        config = genconfig(
            [
                "--launcher",
                "mpirun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ]
        )

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"

        # TODO (bv) -x env args currently too fragile to test
        assert launcher.cmd[:10] == (
            (
                "mpirun",
                "-n",
                "1",
                "--npernode",
                "1",
                "--bind-to",
                "none",
                "--mca",
                "mpi_warn_on_fork",
                "0",
            )
            # + self.XARGS
            # + ("foo", "bar")
        )

        # at least make sure LEGATE_CONFIG is forwarded
        assert "LEGATE_CONFIG" in launcher.cmd
        assert launcher.cmd[launcher.cmd.index("LEGATE_CONFIG") - 1] == "-x"

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(["--launcher", "mpirun"], multi_rank=(100, 2))
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"

        # TODO (bv) -x env args currently too fragile to test
        assert launcher.cmd[:10] == (
            (
                "mpirun",
                "-n",
                "200",
                "--npernode",
                "2",
                "--bind-to",
                "none",
                "--mca",
                "mpi_warn_on_fork",
                "0",
            )
            # + self.XARGS
        )

        # at least make sure LEGATE_CONFIG is forwarded
        assert "LEGATE_CONFIG" in launcher.cmd
        assert launcher.cmd[launcher.cmd.index("LEGATE_CONFIG") - 1] == "-x"

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank_launcher_extra(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(
            [
                "--launcher",
                "mpirun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ],
            multi_rank=(100, 2),
        )
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"

        # TODO (bv) -x env args currently too fragile to test
        assert launcher.cmd[:10] == (
            (
                "mpirun",
                "-n",
                "200",
                "--npernode",
                "2",
                "--bind-to",
                "none",
                "--mca",
                "mpi_warn_on_fork",
                "0",
            )
            # + self.XARGS
            # + ("foo", "bar")
        )

        # at least make sure LEGATE_CONFIG is forwarded
        assert "LEGATE_CONFIG" in launcher.cmd
        assert launcher.cmd[launcher.cmd.index("LEGATE_CONFIG") - 1] == "-x"


class TestJSRunLauncher:
    def test_single_rank(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "jsrun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == (
            (
                "jsrun",
                "-n",
                "1",
                "-r",
                "1",
                "-a",
                "1",
                "-c",
                "ALL_CPUS",
                "-g",
                "ALL_GPUS",
                "-b",
                "none",
            )
        )

    def test_single_rank_launcher_extra(self, genconfig: GenConfig) -> None:
        config = genconfig(
            [
                "--launcher",
                "jsrun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ]
        )

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == (
            (
                "jsrun",
                "-n",
                "1",
                "-r",
                "1",
                "-a",
                "1",
                "-c",
                "ALL_CPUS",
                "-g",
                "ALL_GPUS",
                "-b",
                "none",
                "foo",
                "bar",
            )
        )

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(["--launcher", "jsrun"], multi_rank=(100, 2))
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == (
            (
                "jsrun",
                "-n",
                "100",
                "-r",
                "1",
                "-a",
                "2",
                "-c",
                "ALL_CPUS",
                "-g",
                "ALL_GPUS",
                "-b",
                "none",
            )
        )

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank_launcher_extra(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(
            [
                "--launcher",
                "jsrun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ],
            multi_rank=(100, 2),
        )
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == (
            (
                "jsrun",
                "-n",
                "100",
                "-r",
                "1",
                "-a",
                "2",
                "-c",
                "ALL_CPUS",
                "-g",
                "ALL_GPUS",
                "-b",
                "none",
                "foo",
                "bar",
            )
        )


class TestSRunLauncher:
    def test_single_rank(self, genconfig: GenConfig) -> None:
        config = genconfig(["--launcher", "srun"])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == ("srun", "-n", "1", "--ntasks-per-node", "1")

    def test_single_rank_launcher_extra(self, genconfig: GenConfig) -> None:
        config = genconfig(
            [
                "--launcher",
                "srun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ]
        )

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == (
            "srun",
            "-n",
            "1",
            "--ntasks-per-node",
            "1",
            "foo",
            "bar",
        )

    @pytest.mark.parametrize(
        "debugger", powerset_nonempty(("--gdb", "--cuda-gdb")), ids=str
    )
    def test_single_rank_debugging(
        self, genconfig: GenConfig, debugger: str
    ) -> None:
        config = genconfig(["--launcher", "srun", *list(debugger)])

        launcher = m.Launcher.create(config, SYSTEM)

        assert launcher.detected_rank_id == "0"
        assert launcher.cmd == (
            "srun",
            "-n",
            "1",
            "--ntasks-per-node",
            "1",
            "--pty",
        )

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(["--launcher", "srun"], multi_rank=(100, 2))
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == ("srun", "-n", "200", "--ntasks-per-node", "2")

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    def test_multi_rank_launcher_extra(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(
            [
                "--launcher",
                "srun",
                "--launcher-extra",
                "foo",
                "--launcher-extra",
                "bar",
            ],
            multi_rank=(100, 2),
        )
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == (
            "srun",
            "-n",
            "200",
            "--ntasks-per-node",
            "2",
            "foo",
            "bar",
        )

    @pytest.mark.parametrize("rank_var", m.RANK_ENV_VARS)
    @pytest.mark.parametrize(
        "debugger", powerset_nonempty(("--gdb", "--cuda-gdb")), ids=str
    )
    def test_multi_rank_debugging(
        self,
        monkeypatch: pytest.MonkeyPatch,
        genconfig: GenConfig,
        debugger: str,
        rank_var: str,
    ) -> None:
        for name in m.RANK_ENV_VARS:
            monkeypatch.delenv(name, raising=False)
        monkeypatch.setenv(name, "123")

        config = genconfig(
            ["--launcher", "srun", *list(debugger)], multi_rank=(100, 2)
        )
        system = System()
        launcher = m.Launcher.create(config, system)

        assert launcher.detected_rank_id == "123"
        assert launcher.cmd == (
            "srun",
            "-n",
            "200",
            "--ntasks-per-node",
            "2",
            "--pty",
        )
