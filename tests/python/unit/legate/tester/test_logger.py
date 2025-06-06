# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: Apache-2.0

"""Consolidate test configuration from command-line and environment."""

from __future__ import annotations

from rich.text import Text

from legate.tester import logger as m

TEST_IN_LINES = (
    "line 1",
    Text.from_markup("[red]foo[/]"),
    "bar",
    "last line",
    "with\nnewlines",
)

TEST_OUT_LINES = ("line 1", "foo", "bar", "last line", "with", "newlines")

SCRUBBED_TEST_LINES = ("line 1", "foo", "bar", "last line", "with", "newlines")


class TestLogger:
    def test_init(self) -> None:
        log = m.Log()
        assert log.lines == ()
        assert log.dump() == ""

    def test_render_lines(self) -> None:
        log = m.Log()
        log.render(*TEST_IN_LINES)
        assert log.lines == TEST_OUT_LINES
        assert log.dump() == "\n".join(SCRUBBED_TEST_LINES)

    def test_call(self) -> None:
        log = m.Log()
        log(*TEST_IN_LINES)
        assert log.lines == TEST_OUT_LINES
        assert log.dump() == "\n".join(SCRUBBED_TEST_LINES)

    def test_clear(self) -> None:
        log = m.Log()
        log.render(*TEST_IN_LINES)
        assert len(log.lines) > 0
        log.clear()
        assert len(log.lines) == 0


def test_LOG() -> None:
    assert isinstance(m.LOG, m.Log)
