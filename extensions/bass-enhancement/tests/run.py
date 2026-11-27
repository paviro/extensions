#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the host tests with a staged SDK: python3 tests/run.py SDK.

The reference gains at 100 % are those of Rockbox lib/rbcodec/dsp/pbe.c
(Copyright (C) 2014 by Chiwen Chang). Modified by the RevolaOS authors, 2026.
Rockbox is GPL-2.0-or-later; this version is GPL-3.0-only.
"""

import ctypes
import math
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent
dsp = root.parents[1] / "dsp"
shared = root.parents[1] / "shared"
sys.path.insert(0, str(dsp / "tests"))
from harness import build, strings  # noqa: E402
from model import SCALE, bishelf, peaking, power  # noqa: E402

RATES = (44100, 48000)
STAGES, BANKS, STRENGTHS = 5, 2, 5
OFF = 0
FULL = STRENGTHS - 1
# pbe.c's gains in tenths of a decibel: the shelf's high side and overall
# gain, then the four bells as (hz, Q tenths, gain).
SHELF_HZ, SHELF_GAIN, OVERALL = 16000, 53, -5
BELLS = ((64, 28, 53), (2000, 28, 58), (7500, 43, -82), (10000, 43, -29))
Biquad = ctypes.c_int32 * 5
Bank = Biquad * STAGES


class Set(ctypes.Structure):
    _fields_ = [("strength", ctypes.c_uint8), ("coefficients", Bank * BANKS)]


def scaled(tenths, percent):
    """design.c's rounding of a gain at a strength."""
    value = tenths * percent
    return int((value + (-50 if value < 0 else 50)) / 100)


def reference(strength, rate):
    """The five filters at a strength before the precut, b0, b1, b2, a1, a2
    over a0."""
    percent = 25 * strength
    shelf = bishelf(
        20, SHELF_HZ, 0, scaled(SHELF_GAIN, percent), scaled(OVERALL, percent), rate
    )
    return [shelf] + [
        peaking(hz, q, scaled(gain, percent), rate) for hz, q, gain in BELLS
    ]


def dense_peak(bank, rate):
    """The largest response over 20 Hz to 20 kHz in tenths of a decibel."""
    points = (20 * 1000 ** (i / 6000) for i in range(6001))
    return 100 * math.log10(max(power(bank, hz, rate) for hz in points))


def designed(dll, strength, rate):
    out, precut = Bank(), ctypes.c_uint()
    assert dll.bass_design(out, precut, strength, rate) == 1
    return out, precut.value


def design_tests(dll):
    design = dll.bass_design
    design.argtypes = [ctypes.POINTER(Biquad), ctypes.POINTER(ctypes.c_uint)]
    design.argtypes += [ctypes.c_uint, ctypes.c_uint]
    for rate in RATES:
        for strength in range(STRENGTHS):
            out, precut = designed(dll, strength, rate)
            expected = reference(strength, rate)
            if strength == OFF:
                assert precut == 0
                for stage in out:
                    assert list(stage) == [SCALE, 0, 0, 0, 0]
                continue
            # The precut is the peak of the cascade, rounded up: at most a
            # tenth above it, and the cut cascade stays at or below 0 dB.
            peak = dense_peak(expected, rate)
            assert peak > 0
            assert peak - 0.01 <= precut <= math.ceil(peak) + 1, (
                rate,
                strength,
                precut,
                peak,
            )
            cut = 10 ** (-precut / 200)
            for i, (stage, ref) in enumerate(zip(out, expected, strict=True)):
                scale = cut if i == 0 else 1
                for k, (a, b) in enumerate(zip(stage, ref, strict=True)):
                    b = b * scale if k < 3 else b
                    assert abs(a / SCALE - b) <= 2 / SCALE, (
                        rate,
                        strength,
                        i,
                        list(stage),
                        ref,
                    )
            quantized = [[c / SCALE for c in stage] for stage in out]
            assert dense_peak(quantized, rate) <= 0.05
            # The bells sit where the design says, scaled by the strength.
            lift = 100 * math.log10(power(expected, 64, rate))
            assert abs(lift - scaled(53, 25 * strength)) < 8, (strength, lift)
            cut_7k5 = 100 * math.log10(power(expected, 7500, rate))
            assert abs(cut_7k5 - scaled(-82, 25 * strength)) < 12, (strength, cut_7k5)
        # 100 % is Rockbox's gains, 25 % a quarter of them in decibels.
        assert reference(FULL, rate)[1] == peaking(64, 28, 53, rate)
        assert reference(1, rate)[1] == peaking(64, 28, 13, rate)
        assert reference(1, rate)[3] == peaking(7500, 43, -21, rate)
    precut = ctypes.c_uint()
    assert design(Bank(), precut, STRENGTHS, 44100) == 0
    assert design(Bank(), precut, 1, 32000) == 0


def label_tests(dll):
    labels = strings(dll, "strength_labels", STRENGTHS)
    assert labels == ["Off", "25 %", "50 %", "75 %", "100 %"]


def tone_tests(dll):
    """At 100 % a full-scale 2 kHz tone, the strongest lift, stays inside full
    scale under the precut, and comes out at the design's level once the
    filters and the limiter have settled."""
    publish = dll.bass_publish
    publish.argtypes = [ctypes.POINTER(Set)]
    publish.restype = None
    process = dll.bass_process
    process.argtypes = [ctypes.POINTER(ctypes.c_int16)] + [ctypes.c_uint32] * 2
    process.restype = None
    chosen = Set(strength=FULL)
    for bank, rate in enumerate(RATES):
        bank_out, _ = designed(dll, FULL, rate)
        ctypes.memmove(chosen.coefficients[bank], bank_out, ctypes.sizeof(Bank))
    publish(chosen)
    frames = 8192
    tone = [
        round(32767 * math.sin(2 * math.pi * 2000 * i / 44100)) for i in range(frames)
    ]
    buffer = (ctypes.c_int16 * (2 * frames))(*[s for s in tone for _ in range(2)])
    process(buffer, frames, 44100)
    peak = max(abs(buffer[2 * i]) for i in range(frames // 2, frames))
    assert peak <= 32767
    bank_out, _ = designed(dll, FULL, 44100)
    quantized = [[c / SCALE for c in stage] for stage in bank_out]
    designed_tenths = 100 * math.log10(power(quantized, 2000, 44100))
    assert abs(200 * math.log10(peak / 32767) - designed_tenths) < 3, (
        peak,
        designed_tenths,
    )


def response_tests(dll):
    """The editor's curve of each strength is the cut design's response, so it
    peaks at 0 dB or just below."""
    response = dll.rv_dsp_response
    response.argtypes = [ctypes.POINTER(Biquad), ctypes.c_uint, ctypes.c_uint]
    response.argtypes += [ctypes.POINTER(ctypes.c_int16), ctypes.c_uint]
    response.restype = None
    points = 261
    hertz = [20 * 1000 ** (i / (points - 1)) for i in range(points)]
    for strength in range(1, STRENGTHS):
        bank, precut = designed(dll, strength, 44100)
        curve = (ctypes.c_int16 * points)()
        response(bank, STAGES, 44100, curve, points)
        expected = reference(strength, 44100)
        for hz, tenths in zip(hertz, curve, strict=True):
            model = 100 * math.log10(power(expected, hz, 44100)) - precut
            assert abs(tenths - model) <= 1, (hz, tenths, model)
        assert -5 <= max(curve) <= 0 and min(curve) >= -240


sdk = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="bass-enhancement-tests-") as name:
    work = Path(name)
    sources = (
        [root / "host.c", sdk / "rv_libc.c"]
        + [dsp / name for name in ("math.c", "limiter.c", "filter.c", "cascade.c")]
        + [shared / "bank.c"]
        + [root.parent / name for name in ("design.c", "modes.c")]
    )
    includes = [sdk, dsp, shared, root.parent]
    dll = build(work, "bass-enhancement.so", sources, includes, "-DBANK_STAGES=5")
    design_tests(dll)
    label_tests(dll)
    response_tests(dll)
    tone_tests(dll)
print("Bass Enhancement tests passed")
