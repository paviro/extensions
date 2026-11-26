#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the host tests with a staged SDK: python3 tests/run.py SDK."""

import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent
dsp = root.parents[1] / "dsp"


def run_engine_test(work):
    """The engine against a double-precision cascade, in tests/engine_test.c."""
    program = work / "engine_test"
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-g"]
        + [
            "-fsanitize=undefined",
            "-fno-sanitize-recover=undefined",
            f"-I{sdk}",
            f"-I{dsp}",
        ]
        + ["-D_FORTIFY_SOURCE=0"]  # macOS string.h would wrap what rv_libc.h declares
        + [str(root / "engine_test.c"), str(dsp / "cascade.c")]
        + [
            str(dsp / "filter.c"),
            str(dsp / "math.c"),
            str(dsp / "limiter.c"),
            "-lm",
            "-o",
            str(program),
        ],
        check=True,
    )
    subprocess.run([str(program)], check=True)


sdk = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="equalizer-tests-") as name:
    run_engine_test(Path(name))
print("Equalizer tests passed")
