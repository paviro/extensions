#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the host tests with a staged SDK: python3 tests/run.py SDK.

The reference filters follow Rockbox lib/rbcodec/dsp/afr.c (Copyright (C)
2014 by Chiwen Chang). Modified by the RevolaOS authors, 2026. Rockbox is
GPL-2.0-or-later; this version is GPL-3.0-only.
"""

import ctypes
import math
import sys
import tempfile
from itertools import pairwise
from pathlib import Path

root = Path(__file__).resolve().parent
dsp = root.parents[1] / "dsp"
shared = root.parents[1] / "shared"
sys.path.insert(0, str(dsp / "tests"))
from harness import build, strings  # noqa: E402
from model import SCALE, bishelf, peaking, power  # noqa: E402

RATES = (44100, 48000)
STAGES, BANKS, STRENGTHS = 4, 2, 4
OFF, WEAK, MODERATE, STRONG = range(4)
# hs, ls, drop3k in afr.c, tenths of a decibel.
GAINS = {WEAK: (-29, 0, -13), MODERATE: (-58, -5, -29), STRONG: (-72, -5, -48)}
Biquad = ctypes.c_int32 * 5
Bank = Biquad * STAGES


class Set(ctypes.Structure):
    _fields_ = [("strength", ctypes.c_uint8), ("coefficients", Bank * BANKS)]


def reference(strength, rate):
    """afr.c's four filters at a strength, b0, b1, b2, a1, a2 over a0."""
    if strength == OFF:
        return [[1, 0, 0, 0, 0]] * STAGES
    hs, ls, drop3k = GAINS[strength]
    return [
        bishelf(8000, 20000, 0, int(hs / 2), 0, rate),
        peaking(8000, 28, hs, rate),
        peaking(3150, 28, drop3k, rate),
        bishelf(200, 1280, ls, 0, 0, rate),
    ]


def designed(dll, strength, rate):
    out = Bank()
    assert dll.fatigue_design(out, strength, rate) == 1
    return out


def design_tests(dll):
    design = dll.fatigue_design
    design.argtypes = [ctypes.POINTER(Biquad), ctypes.c_uint, ctypes.c_uint]
    for rate in RATES:
        curves = {}
        for strength in range(STRENGTHS):
            out = designed(dll, strength, rate)
            expected = reference(strength, rate)
            for stage, ref in zip(out, expected, strict=True):
                for a, b in zip(stage, ref, strict=True):
                    assert abs(a / SCALE - b) <= 2 / SCALE, (
                        rate,
                        strength,
                        list(stage),
                        ref,
                    )
            scaled = [[c / SCALE for c in stage] for stage in out]
            curves[strength] = [
                100 * math.log10(power(scaled, hz, rate))
                for hz in (50, 1000, 3150, 8000)
            ]
        # Off is flat; the cuts deepen with the strength at 3.15 and 8 kHz, and
        # the bass shelf's cut shows at 50 Hz from Moderate.
        assert curves[OFF] == [0, 0, 0, 0]
        for a, b in pairwise(range(STRENGTHS)):
            assert curves[b][2] < curves[a][2] - 5 and curves[b][3] < curves[a][3] - 5
        assert abs(curves[WEAK][0]) < 1 and -8 < curves[MODERATE][0] < -3
        assert abs(curves[MODERATE][2] - GAINS[MODERATE][2]) < 5
        assert all(abs(curves[s][1]) < 20 for s in range(STRENGTHS))
    assert design(Bank(), STRENGTHS, 44100) == 0
    assert design(Bank(), WEAK, 32000) == 0


def label_tests(dll):
    labels = strings(dll, "strength_labels", STRENGTHS)
    assert labels == ["Off", "Weak", "Moderate", "Strong"]


def tone_tests(dll):
    """A quiet tone at Strong comes out quieter by the design at 3.15 kHz, once
    the filters and the limiter have settled."""
    publish = dll.fatigue_publish
    publish.argtypes = [ctypes.POINTER(Set)]
    publish.restype = None
    process = dll.fatigue_process
    process.argtypes = [ctypes.POINTER(ctypes.c_int16)] + [ctypes.c_uint32] * 2
    process.restype = None
    chosen = Set(strength=STRONG)
    for bank, rate in enumerate(RATES):
        bank_out = designed(dll, STRONG, rate)
        ctypes.memmove(chosen.coefficients[bank], bank_out, ctypes.sizeof(Bank))
    publish(chosen)
    frames = 8192
    tone = [
        round(8000 * math.sin(2 * math.pi * 3150 * i / 44100)) for i in range(frames)
    ]
    buffer = (ctypes.c_int16 * (2 * frames))(*[s for s in tone for _ in range(2)])
    process(buffer, frames, 44100)
    peak = max(abs(buffer[2 * i]) for i in range(frames // 2, frames))
    scaled = [[c / SCALE for c in stage] for stage in designed(dll, STRONG, 44100)]
    designed_tenths = 100 * math.log10(power(scaled, 3150, 44100))
    assert abs(200 * math.log10(peak / 8000) - designed_tenths) < 3, (
        peak,
        designed_tenths,
    )


def response_tests(dll):
    """The editor's curve of each strength is the design's response."""
    response = dll.rv_dsp_response
    response.argtypes = [ctypes.POINTER(Biquad), ctypes.c_uint, ctypes.c_uint]
    response.argtypes += [ctypes.POINTER(ctypes.c_int16), ctypes.c_uint]
    response.restype = None
    points = 261
    hertz = [20 * 1000 ** (i / (points - 1)) for i in range(points)]
    for strength in (WEAK, MODERATE, STRONG):
        bank = designed(dll, strength, 44100)
        curve = (ctypes.c_int16 * points)()
        response(bank, STAGES, 44100, curve, points)
        expected = reference(strength, 44100)
        for hz, tenths in zip(hertz, curve, strict=True):
            assert abs(tenths - 100 * math.log10(power(expected, hz, 44100))) <= 1, (
                hz,
                tenths,
            )
        assert min(curve) >= -120 and max(curve) <= 0


sdk = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="fatigue-reduction-tests-") as name:
    work = Path(name)
    sources = (
        [root / "host.c", sdk / "rv_libc.c"]
        + [dsp / name for name in ("math.c", "limiter.c", "filter.c", "cascade.c")]
        + [shared / "bank.c"]
        + [root.parent / name for name in ("design.c", "modes.c")]
    )
    includes = [sdk, dsp, shared, root.parent]
    dll = build(work, "fatigue-reduction.so", sources, includes, "-DBANK_STAGES=4")
    design_tests(dll)
    label_tests(dll)
    response_tests(dll)
    tone_tests(dll)
print("Fatigue Reduction tests passed")
