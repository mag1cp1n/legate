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

import multiprocessing
import queue
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING, Any, NoReturn, Protocol

from rich.console import Group
from rich.rule import Rule

from ...util.types import ArgList, EnvDict
from ...util.ui import section, summary
from .. import FeatureType
from ..config import Config
from ..runner import Runner, TestSpec
from ..test_system import ProcessResult, TestSystem
from .util import Shard, StageResult, StageSpec, log_proc

if TYPE_CHECKING:
    from rich.panel import Panel


class TestStage(Protocol):
    """Encapsulate running configured test files using specific features.

    Parameters
    ----------
    config: Config
        Test runner configuration

    system: TestSystem
        Process execution wrapper

    """

    kind: FeatureType

    #: The computed specification for processes to launch to run the
    #: configured test files.
    spec: StageSpec

    #: The computed sharding id sets to use for job runs
    shards: queue.Queue[Any]

    #: After the stage completes, results will be stored here
    result: StageResult

    #: Any fixed stage-specific command-line args to pass
    args: ArgList

    runner: Runner

    # --- Protocol methods

    def __init__(self, config: Config, system: TestSystem) -> None:
        pass

    def env(self, config: Config, system: TestSystem) -> EnvDict:
        """Generate stage-specific customizations to the process env

        Parameters
        ----------
        config: Config
            Test runner configuration

        system: TestSystem
            Process execution wrapper

        """
        ...

    def delay(self, shard: Shard, config: Config, system: TestSystem) -> None:
        """Wait any delay that should be applied before running the next
        test.

        Parameters
        ----------
        shard: Shard
            The shard to be used for the next test that is run

        config: Config
            Test runner configuration

        system: TestSystem
            Process execution wrapper

        """
        return

    def shard_args(self, shard: Shard, config: Config) -> ArgList:
        """Generate the command line arguments necessary to launch
        the next test process on the given shard.

        Parameters
        ----------
        shard: Shard
            The shard to be used for the next test that is run

        config: Config
            Test runner configuration

        """
        ...

    def compute_spec(self, config: Config, system: TestSystem) -> StageSpec:
        """Compute the number of worker processes to launch and stage shards
        to use for running the configured test files.

        Parameters
        ----------
        config: Config
            Test runner configuration

        system: TestSystem
            Process execution wrapper

        """
        ...

    # --- Shared implementation methods

    def execute(self, config: Config, system: TestSystem) -> None:
        """Execute this test stage.

        Parameters
        ----------
        config: Config
            Test runner configuration

        system: TestSystem
            Process execution wrapper

        """
        if config.other.gdb:
            self._launch_gdb_and_exit(config, system)

        t0 = datetime.now()
        procs = self._launch(config, system)
        t1 = datetime.now()

        self.result = StageResult(procs, t1 - t0)

    @property
    def name(self) -> str:
        """A stage name to display for tests in this stage."""
        return self.__class__.__name__

    @property
    def intro(self) -> Panel:
        """An informative banner to display at stage end."""
        num_workers = self.spec.workers
        workers = f"with {num_workers} worker{'s' if num_workers > 1 else ''}"
        return section(f"Entering stage: [cyan]{self.name}[/] ({workers})")

    @property
    def outro(self) -> Group:
        """An informative banner to display at stage end."""
        total, passed = self.result.total, self.result.passed

        return Group(
            section(
                Group(
                    f"Exiting stage: [cyan]{self.name}[/]\n",
                    summary(total, passed, self.result.time),
                )
            ),
            Rule(style="white"),
        )

    def _run(
        self,
        test_spec: TestSpec,
        config: Config,
        system: TestSystem,
        *,
        custom_args: ArgList | None = None,
    ) -> ProcessResult:
        """Execute a single test within gtest with appropriate environment and
        command-line options for a feature test stage.

        Parameters
        ----------
        test_spec : TestSpec
            Specification for the test to execute

        config: Config
            Test runner configuration

        system: TestSystem
            Process execution wrapper

        """

        shard = self.shards.get()

        stage_args = self.args + self.shard_args(shard, config)

        cmd = self.runner.cmd(
            test_spec,
            config,
            stage_args,
            custom_args=custom_args,
        )

        self.delay(shard, config, system)

        result = system.run(
            cmd,
            test_spec.display,
            env=self._env(config, system),
            timeout=config.execution.timeout,
        )

        log_proc(self.name, result, config, verbose=config.info.verbose)

        self.shards.put(shard)

        return result

    def _env(self, config: Config, system: TestSystem) -> EnvDict:
        env = dict(config.env)
        env.update(self.env(config, system))

        # special case for LEGATE_CONFIG -- if users have specified this on
        # their own we still want to see the value since it will affect the
        # test invocation directly.
        if "LEGATE_CONFIG" in system.env:
            env["LEGATE_CONFIG"] = system.env["LEGATE_CONFIG"]

        return env

    def _init(self, config: Config, system: TestSystem) -> None:
        self.runner = Runner.create(config)
        self.spec = self.compute_spec(config, system)
        self.shards = system.manager.Queue(len(self.spec.shards))
        for shard in self.spec.shards:
            self.shards.put(shard)

    @staticmethod
    def handle_multi_node_args(config: Config) -> ArgList:
        args: ArgList = []

        if config.multi_node.launcher != "none":
            args += ["--launcher", str(config.multi_node.launcher)]

        if config.multi_node.ranks_per_node > 1:
            args += [
                "--ranks-per-node",
                str(config.multi_node.ranks_per_node),
            ]

        if config.multi_node.nodes > 1:
            args += [
                "--nodes",
                str(config.multi_node.nodes),
            ]

        for extra in config.multi_node.launcher_extra:
            args += ["--launcher-extra=" + str(extra)]

        return args

    @staticmethod
    def handle_cpu_pin_args(config: Config, shard: Shard) -> ArgList:
        args: ArgList = []
        if config.execution.cpu_pin != "none":
            args += [
                "--cpu-bind",
                str(shard),
            ]

        return args

    def _launch(
        self, config: Config, system: TestSystem
    ) -> list[ProcessResult]:
        pool = multiprocessing.pool.ThreadPool(self.spec.workers)

        assert not config.other.gdb

        # make sure we get any updated values from downstream test scripts
        import legate.tester as lt

        custom_paths_map = {Path(x.file): x for x in lt.CUSTOM_FILES}

        specs = self.runner.test_specs(config)

        sharded_specs = (s for s in specs if s.path not in custom_paths_map)
        jobs = [
            pool.apply_async(self._run, (spec, config, system))
            for spec in sharded_specs
        ]
        pool.close()

        sharded_results = [job.get() for job in jobs]

        unsharded_specs = [s for s in specs if s.path in custom_paths_map]
        unsharded_results = []
        for spec in unsharded_specs:
            kind = custom_paths_map[spec.path].kind or lt.FEATURES
            args = custom_paths_map[spec.path].args or []
            if self.kind == kind or self.kind in kind:
                result = self._run(spec, config, system, custom_args=args)
                unsharded_results.append(result)

        return sharded_results + unsharded_results

    def _launch_gdb_and_exit(
        self, config: Config, system: TestSystem
    ) -> NoReturn:
        import os
        import sys
        from subprocess import run

        cmd = self.runner.cmd_gdb(config)
        env = os.environ | self._env(config, system)

        run(cmd, env=env)

        sys.exit()
