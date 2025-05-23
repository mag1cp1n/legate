# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: Apache-2.0

"""Consolidate test configuration from command-line and environment."""

from __future__ import annotations

import pytest

from legate.tester.config import Config
from legate.tester.defaults import SMALL_SYSMEM
from legate.tester.project import Project
from legate.tester.stages._linux import gpu as m
from legate.tester.stages.util import Shard

from .. import FakeSystem

PROJECT = Project()


def test_default() -> None:
    c = Config([], project=PROJECT)
    s = FakeSystem()
    stage = m.GPU(c, s)
    assert stage.kind == "cuda"
    assert stage.args == []
    assert stage.stage_env(c, s) == {}
    assert stage.spec.workers > 0


class TestSingleRank:
    @pytest.mark.parametrize(
        ("shard", "expected"), [[(2,), "2"], [(1, 2, 3), "1,2,3"]]
    )
    def test_shard_args(self, shard: tuple[int, ...], expected: str) -> None:
        c = Config([], project=PROJECT)
        s = FakeSystem()
        stage = m.GPU(c, s)
        result = stage.shard_args(Shard([shard]), c)
        assert result == [
            "--fbmem",
            "4096",
            "--gpus",
            f"{len(shard)}",
            "--gpu-bind",
            expected,
            "--sysmem",
            str(SMALL_SYSMEM),
            "--cpus",
            "1",
            "--utility",
            f"{c.core.utility}",
        ]

    def test_spec_with_gpus_1(self) -> None:
        c = Config(["test.py", "--gpus", "1"], project=PROJECT)
        s = FakeSystem()
        stage = m.GPU(c, s)
        assert stage.spec.workers == 24
        assert (
            stage.spec.shards
            == [
                Shard([(0,)]),
                Shard([(1,)]),
                Shard([(2,)]),
                Shard([(3,)]),
                Shard([(4,)]),
                Shard([(5,)]),
            ]
            * stage.spec.workers
        )

    def test_spec_with_gpus_2(self) -> None:
        c = Config(["test.py", "--gpus", "2"], project=PROJECT)
        s = FakeSystem()
        stage = m.GPU(c, s)
        assert stage.spec.workers == 12
        assert (
            stage.spec.shards
            == [Shard([(0, 1)]), Shard([(2, 3)]), Shard([(4, 5)])]
            * stage.spec.workers
        )

    def test_spec_with_requested_workers(self) -> None:
        c = Config(["test.py", "--gpus", "1", "-j", "2"], project=PROJECT)
        s = FakeSystem()
        stage = m.GPU(c, s)
        assert stage.spec.workers == 2
        assert (
            stage.spec.shards
            == [
                Shard([(0,)]),
                Shard([(1,)]),
                Shard([(2,)]),
                Shard([(3,)]),
                Shard([(4,)]),
                Shard([(5,)]),
            ]
            * stage.spec.workers
        )

    def test_spec_with_requested_workers_zero(self) -> None:
        s = FakeSystem()
        c = Config(["test.py", "-j", "0"], project=PROJECT)
        assert c.execution.workers == 0
        with pytest.raises(RuntimeError):
            m.GPU(c, s)

    def test_spec_with_requested_workers_bad(self) -> None:
        s = FakeSystem()
        c = Config(["test.py", "-j", f"{len(s.gpus) + 100}"], project=PROJECT)
        requested_workers = c.execution.workers
        assert requested_workers is not None
        assert requested_workers > len(s.gpus)
        with pytest.raises(RuntimeError):
            m.GPU(c, s)

    def test_spec_with_verbose(self) -> None:
        args = ["test.py", "--gpus", "2"]
        c = Config(args, project=PROJECT)
        cv = Config([*args, "--verbose"], project=PROJECT)
        s = FakeSystem()

        spec, vspec = m.GPU(c, s).spec, m.GPU(cv, s).spec
        assert vspec == spec


class TestMultiRank:
    @pytest.mark.parametrize(
        ("shard", "expected"), [[(2,), "2"], [(1, 2, 3), "1,2,3"]]
    )
    def test_shard_args(self, shard: tuple[int, ...], expected: str) -> None:
        c = Config([], project=PROJECT)
        s = FakeSystem()
        stage = m.GPU(c, s)
        result = stage.shard_args(Shard([shard]), c)
        assert result == [
            "--fbmem",
            "4096",
            "--gpus",
            f"{len(shard)}",
            "--gpu-bind",
            expected,
            "--sysmem",
            str(SMALL_SYSMEM),
            "--cpus",
            "1",
            "--utility",
            f"{c.core.utility}",
        ]

    def test_spec_with_gpus_1(self) -> None:
        # use any valid launcher
        c = Config(
            [
                "test.py",
                "--gpus",
                "1",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ],
            project=PROJECT,
        )
        s = FakeSystem(gpus=4)
        stage = m.GPU(c, s)
        assert stage.spec.workers == 8
        assert (
            stage.spec.shards == [Shard([(0,), (1,)]), Shard([(2,), (3,)])] * 4
        )

    def test_spec_with_gpus_2(self) -> None:
        # use any valid launcher
        c = Config(
            [
                "test.py",
                "--gpus",
                "2",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ],
            project=PROJECT,
        )
        s = FakeSystem(gpus=4)
        stage = m.GPU(c, s)
        assert stage.spec.workers == 4
        assert stage.spec.shards == [Shard([(0, 1), (2, 3)])] * 4
