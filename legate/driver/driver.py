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

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from shlex import quote
from subprocess import run
from textwrap import indent
from typing import TYPE_CHECKING

from ..util.system import System
from ..util.types import DataclassMixin
from ..util.ui import kvtable, rule, section, value, warn
from .command import CMD_PARTS_CANONICAL, CMD_PARTS_LEGION
from .config import ConfigProtocol
from .launcher import Launcher, SimpleLauncher

if TYPE_CHECKING:
    from ..util.types import Command, EnvDict

__all__ = ("LegateDriver", "CanonicalDriver", "print_verbose")

_DARWIN_GDB_WARN = """\
You must start the debugging session with the following command,
as LLDB no longer forwards the environment to subprocesses for security
reasons:

(lldb) process launch -v LIB_PATH={libpath} -v PYTHONPATH={pythonpath}

"""


@dataclass(frozen=True)
class LegateVersions(DataclassMixin):
    """Collect package versions relevant to Legate."""

    legate_version: str


class LegateDriver:
    """Coordinate the system, user-configuration, and launcher to appropriately
    execute the Legate process.

    Parameters
    ----------
        config : Config

        system : System

    """

    def __init__(self, config: ConfigProtocol, system: System) -> None:
        self.config = config
        self.system = system
        self.launcher = Launcher.create(config, system)

    @property
    def cmd(self) -> Command:
        """The full command invocation that should be used to start Legate."""
        config = self.config
        launcher = self.launcher
        system = self.system

        parts = (part(config, system, launcher) for part in CMD_PARTS_LEGION)
        return launcher.cmd + sum(parts, ())

    @property
    def env(self) -> EnvDict:
        """The system environment that should be used when started Legate."""
        # in case we want to augment the launcher env we could do it here
        return self.launcher.env

    @property
    def custom_env_vars(self) -> set[str]:
        """The names of environment variables that we have explicitly set
        for the system environment.

        """
        # in case we want to augment the launcher env we could do it here
        return self.launcher.custom_env_vars

    @property
    def dry_run(self) -> bool:
        """Check verbose and dry run.

        Returns
        -------
            bool : whether dry run is enabled

        """
        if self.config.info.verbose:
            # we only want to print verbose output on a "head" node
            if (
                self.launcher.kind != "none"
                or self.launcher.detected_rank_id == "0"
            ):
                print_verbose(self.system, self)

        self._darwin_gdb_warn()

        return self.config.other.dry_run

    def run(self) -> int:
        """Run the Legate process.

        Returns
        -------
            int : process return code

        """
        if self.dry_run:
            return 0

        if self.config.other.timing:
            print(f"Legate start: {datetime.now()}")

        ret = run(self.cmd, env=self.env).returncode

        if self.config.other.timing:
            print(f"Legate end: {datetime.now()}")

        log_dir = self.config.logging.logdir

        if self.config.profiling.profile:
            print(
                f"Profiles have been generated under {log_dir}, run "
                f"legion_prof --view {log_dir}/legate_*.prof to view them"
            )

        if self.config.debugging.spy:
            print(
                f"Legion Spy logs have been generated under {log_dir}, run "
                f"legion_spy.py {log_dir}/legate_*.log to process them"
            )

        return ret

    def _darwin_gdb_warn(self) -> None:
        gdb = self.config.debugging.gdb

        if gdb and self.system.os == "Darwin":
            libpath = self.env[self.system.LIB_PATH]
            pythonpath = self.env["PYTHONPATH"]
            print(
                warn(
                    _DARWIN_GDB_WARN.format(
                        libpath=libpath, pythonpath=pythonpath
                    )
                )
            )


class CanonicalDriver(LegateDriver):
    """Coordinate the system, user-configuration, and launcher to appropriately
    execute the Legate process.

    Parameters
    ----------
        config : Config

        system : System

    """

    def __init__(self, config: ConfigProtocol, system: System) -> None:
        self.config = config
        self.system = system
        self.launcher = SimpleLauncher(config, system)

    @property
    def cmd(self) -> Command:
        """The full command invocation that should be used to start Legate."""
        config = self.config
        launcher = self.launcher
        system = self.system

        parts = (
            part(config, system, launcher) for part in CMD_PARTS_CANONICAL
        )
        return sum(parts, ())

    def run(self) -> int:
        """Run the Legate process.

        Returns
        -------
            int : process return code

        """
        assert False, "This function should not be invoked."


def get_versions() -> LegateVersions:
    from legate import __version__ as lg_version

    return LegateVersions(legate_version=lg_version)


def print_verbose(
    system: System,
    driver: LegateDriver | None = None,
) -> None:
    """Print system and driver configuration values.

    Parameters
    ----------
    system : System
        A System instance to obtain Legate and Legion paths from

    driver : Driver or None, optional
        If not None, a Driver instance to obtain command invocation and
        environment from (default: None)

    Returns
    -------
        None

    """

    print(f"\n{rule('Legion Python Configuration')}")

    print(section("\nLegate paths:"))
    print(indent(str(system.legate_paths), prefix="  "))

    print(section("\nLegion paths:"))
    print(indent(str(system.legion_paths), prefix="  "))

    print(section("\nVersions:"))
    print(indent(str(get_versions()), prefix="  "))

    if driver:
        print(section("\nCommand:"))
        cmd = " ".join(quote(t) for t in driver.cmd)
        print(f"  {value(cmd)}")

        if keys := sorted(driver.custom_env_vars):
            print(section("\nCustomized Environment:"))
            print(
                indent(
                    kvtable(driver.env, delim="=", align=False, keys=keys),
                    prefix="  ",
                )
            )

    print(f"\n{rule()}")

    print(flush=True)
