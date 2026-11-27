#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the host tests with a staged SDK: python3 tests/run.py SDK.

The reference models follow Rockbox lib/rbcodec/dsp/compressor.c
(Copyright (C) 2009 Jeffrey Goode). Modified by the RevolaOS authors, 2026.
Rockbox is GPL-2.0-or-later; this version is GPL-3.0-only.
"""

import copy
import ctypes
import math
import sys
import tempfile
from itertools import pairwise
from pathlib import Path

root = Path(__file__).resolve().parent
dsp = root.parents[1] / "dsp"
sys.path.insert(0, str(dsp / "tests"))
from harness import build, labeller  # noqa: E402
from harness import noise as stereo_noise  # noqa: E402
from model import Limiter  # noqa: E402

UNITY = 1 << 24
SHIFT = 13
RATES = (44100, 48000, 32000, 22050, 8000)
RATIOS = (2, 4, 6, 10, 0)
OVER_DB = 0xC0A8C / 65536
HEADROOM = 0x4AC4 / 65536


class Set(ctypes.Structure):
    _fields_ = [
        ("on", ctypes.c_uint8),
        ("threshold", ctypes.c_int8),
        ("makeup", ctypes.c_uint8),
        ("ratio", ctypes.c_uint8),
        ("knee", ctypes.c_uint8),
        ("attack", ctypes.c_uint16),
        ("release", ctypes.c_uint16),
    ]


class Points(ctypes.Structure):
    _fields_ = [
        ("db", ctypes.c_int32 * 5),
        ("offset", ctypes.c_int32 * 5),
        ("makeup", ctypes.c_int32),
    ]


class Design(ctypes.Structure):
    _fields_ = [("gain", ctypes.c_int32 * 66), ("makeup", ctypes.c_int32)] + [
        (name, ctypes.c_int32)
        for name in (
            "attack_a",
            "attack_b",
            "release_a",
            "release_b",
            "hp1",
            "hp2",
        )
    ]


def reference_points(threshold, makeup, ratio, knee):
    """Rockbox's curve in floating point: input levels in dB, the gain offset
    at each, and the makeup gain."""
    r = RATIOS[ratio]

    def slope(db):
        return -db * (r - 1) / r if r else -db

    half = 3 if knee else 0
    at = [threshold - half, threshold, threshold + half, 0, OVER_DB]
    top, full = slope(half), slope(-threshold)
    offset = [0, top / 2, top, full, slope(OVER_DB) + full]
    return at, offset, (-full - HEADROOM) if makeup else 0


def reference_offset(db, points, knee):
    """The gain offset in dB for an input level, as Rockbox fills its table."""
    at, offset, _ = points
    if db <= at[0]:
        return 0
    if knee and db <= at[2]:
        return (db - at[0]) / 6 * offset[2]
    return (at[1] - db) / at[1] * offset[3]


def to_db(gain):
    return 20 * math.log10(gain / UNITY)


def every_set():
    for threshold in range(1, 9):
        for ratio in range(5):
            for knee in range(2):
                for makeup in range(2):
                    yield Set(
                        on=1,
                        threshold=-3 * threshold,
                        makeup=makeup,
                        ratio=ratio,
                        knee=knee,
                        attack=5,
                        release=500,
                    )


def coefficient(rate, ms):
    return UNITY // rate * 1152 // ms


def design_tests(dll):
    points = dll.compressor_points
    points.argtypes = [ctypes.POINTER(Set), ctypes.POINTER(Points)]
    points.restype = None
    design = dll.compressor_design
    design.argtypes = [ctypes.POINTER(Design), ctypes.POINTER(Set), ctypes.c_uint]
    design.restype = None

    for chosen in every_set():
        reference = reference_points(
            chosen.threshold, chosen.makeup, chosen.ratio, chosen.knee
        )
        out = Points()
        points(chosen, out)
        for i in range(5):
            assert abs(out.db[i] / 65536 - reference[0][i]) < 2 / 65536, (
                chosen.threshold,
                i,
            )
            assert abs(out.offset[i] / 65536 - reference[1][i]) < 2 / 65536, (
                chosen.ratio,
                i,
            )
        assert abs(out.makeup / 65536 - reference[2]) < 2 / 65536

        d = Design()
        design(d, chosen, 44100)
        assert d.gain[0] == UNITY
        for i in range(1, 64):
            level = 20 * math.log10(i / 64)
            expected = reference_offset(level, reference, chosen.knee)
            assert abs(to_db(d.gain[i]) - expected) < 0.01, (
                chosen.threshold,
                chosen.ratio,
                i,
            )
            assert d.gain[i] <= UNITY
        assert abs(to_db(d.gain[64]) - reference[1][3]) < 0.005
        assert abs(to_db(d.gain[65]) - reference[1][4]) < 0.005
        assert abs(to_db(d.makeup) - reference[2]) < 0.005
        # Monotonic: louder input, no more gain.
        assert all(a >= b for a, b in zip(d.gain, d.gain[1:]))

    # Coefficients are Rockbox's integer formulas, and near the real values.
    for rate in RATES:
        for attack, release in ((0, 100), (5, 500), (30, 1000)):
            chosen = Set(
                on=1,
                threshold=-12,
                makeup=1,
                ratio=1,
                knee=1,
                attack=attack,
                release=release,
            )
            d = Design()
            design(d, chosen, rate)
            assert d.release_a == coefficient(rate, release)
            assert d.release_b == UNITY - d.release_a
            assert abs(d.release_a / UNITY - 1152 / (rate * release)) < 0.02 * 1152 / (
                rate * release
            )
            if attack:
                assert d.attack_a == coefficient(rate, attack)
                assert d.attack_b == UNITY - d.attack_a
            else:
                assert d.attack_a == UNITY and d.attack_b == 0
            assert d.hp1 == UNITY // (rate + 0x3C1) * rate
            assert d.hp2 == UNITY // (rate + 0x2065) * rate
            assert abs(d.hp1 / UNITY - rate / (rate + 961)) < 3e-3
            assert abs(d.hp2 / UNITY - rate / (rate + 8293)) < 3e-3
    # Makeup Off is exactly unity.
    d = Design()
    design(
        d,
        Set(on=1, threshold=-12, makeup=0, ratio=4, knee=0, attack=5, release=500),
        44100,
    )
    assert d.makeup == UNITY


def label_tests(dll):
    labels = labeller(dll.compressor_label)
    ON, THRESHOLD, RATIO, KNEE, MAKEUP, ATTACK, RELEASE = range(7)
    assert labels(ON, 2) == ["Off", "On"]
    assert labels(THRESHOLD, 8) == [f"{-3 * i} dB" for i in range(1, 9)]
    assert labels(MAKEUP, 2) == ["Off", "Auto"]
    assert labels(RATIO, 5) == ["2:1", "4:1", "6:1", "10:1", "Limit"]
    assert labels(KNEE, 2) == ["Hard", "Soft"]
    assert labels(ATTACK, 7) == [f"{5 * i} ms" for i in range(7)]
    assert labels(RELEASE, 10) == [f"{100 + 100 * i} ms" for i in range(10)]
    defaults = (0, 3, 1, 1, 1, 1, 4)
    counts = (2, 8, 5, 2, 2, 7, 10)
    chosen = [labels(p, c)[d] for p, (c, d) in enumerate(zip(counts, defaults))]
    assert chosen == ["Off", "-12 dB", "4:1", "Soft", "Auto", "5 ms", "500 ms"]


def wrap(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def scale(a, b):
    return wrap((a * b) >> 24)


def fracmul(a, b):
    return wrap((a * b) >> 31)


def magnitude(sample):
    return -(sample + 1) if sample < 0 else sample


class Reference:
    """The ported loop, integer for integer, on S16 raised by SHIFT bits, with
    its output wide for the limiter."""

    def __init__(self, rate):
        self.length = 3 * rate // 1000
        self.delay = [(0, 0)] * self.length
        self.index = 0
        self.gain = UNITY
        self.holdoff = self.length
        self.x1 = self.y1 = self.y2 = 0

    def curve_gain(self, design, level):
        gain = design.gain
        level = magnitude(level) >> SHIFT
        if level < 1 << 15:
            i = level >> 9
            return wrap(gain[i] - fracmul((level & 0x1FF) << 22, gain[i] - gain[i + 1]))
        if level < 1 << 17:
            return wrap(
                gain[64]
                - fracmul(((level - (1 << 15)) // 3) << 16, gain[64] - gain[65])
            )
        return -1

    def process(self, design, frames):
        out = []
        for s0, s1 in frames:
            left, right = s0 << SHIFT, s1 << SHIFT
            x = (left + right) >> 1
            self.y1 = scale(design.hp1, x - self.x1 + self.y1)
            self.y2 = scale(design.hp2, x - self.x1 + self.y2)
            self.x1 = x
            level = wrap((x >> 1) + self.y1 + self.y2 * 2)
            level >>= 1
            level = wrap(level + (level >> 1))
            wanted = self.curve_gain(design, level)
            if wanted <= self.gain and wanted > 0:
                if design.attack_a == UNITY:
                    self.gain = wanted
                else:
                    self.gain = wrap(
                        scale(self.gain, design.attack_b)
                        + scale(wanted, design.attack_a)
                    )
                self.holdoff = self.length
            elif self.holdoff > 0:
                self.holdoff -= 1
            else:
                self.gain = wrap(
                    scale(self.gain, design.release_b) + scale(wanted, design.release_a)
                )
            total = scale(self.gain, design.makeup)
            d0, d1 = self.delay[self.index]
            self.delay[self.index] = (s0, s1)
            self.index = (self.index + 1) % self.length
            if total != UNITY:
                out.append(
                    (
                        wrap((total * d0 + (1 << 23)) >> 24),
                        wrap((total * d1 + (1 << 23)) >> 24),
                    )
                )
            else:
                out.append((d0, d1))
        return out


def noise(count, seed, scale_bits=0):
    return [
        (left >> scale_bits, right >> scale_bits)
        for left, right in stereo_noise(count, seed)
    ]


def sine(count, hz, rate, amplitude):
    return [
        (round(amplitude * math.sin(2 * math.pi * hz * i / rate)),) * 2
        for i in range(count)
    ]


FADE = 128


def process_tests(dll):
    publish = dll.compressor_publish
    publish.argtypes = [ctypes.POINTER(Set)]
    publish.restype = None
    process = dll.compressor_process
    process.argtypes = [
        ctypes.POINTER(ctypes.c_int16),
        ctypes.c_uint32,
        ctypes.c_uint32,
    ]
    process.restype = None
    design = dll.compressor_design
    design.argtypes = [ctypes.POINTER(Design), ctypes.POINTER(Set), ctypes.c_uint]
    design.restype = None
    count = 512

    def designed(chosen, rate):
        d = Design()
        design(d, chosen, rate)
        return d

    class Player:
        """One instance as the engine keeps it: a set at a rate over a Reference,
        or a pass-through while Off."""

        def __init__(self, rate, chosen):
            self.rate, self.chosen, self.on = rate, chosen, chosen.on != 0
            self.reference = Reference(rate)
            self.design = designed(chosen, rate) if self.on else None

        def play(self, frames):
            return (
                self.reference.process(self.design, frames) if self.on else list(frames)
            )

    # What plays, what fades out and the frames left of that fade, a set left
    # while an on/off fade lasts, and the limiter behind both.
    engine = {
        "new": Player(44100, Set(on=0, threshold=-12)),
        "old": None,
        "remaining": 0,
    }
    engine["pending"] = None
    limiter = Limiter()

    def switching():
        old = engine["old"]
        return bool(
            engine["remaining"] and old is not None and old.on != engine["new"].on
        )

    def take(chosen, rate):
        """At the same rate the engine fades from what played, and new values
        while on keep the state; turning on rests the limiter and fills both
        delay lines first; a new rate starts over without a fade."""
        old, new = engine["new"], Player(rate, chosen)
        if rate == old.rate and old.on and new.on:
            new.reference = copy.deepcopy(old.reference)
        turns_on = new.on and not old.on
        if turns_on:
            limiter.rest()
        engine["new"] = new
        if rate == old.rate:
            lead = new.reference.length + Limiter.FRAMES if turns_on else 0
            engine["old"], engine["remaining"] = old, FADE + lead
        else:
            engine["old"], engine["remaining"] = None, 0

    def switch(chosen):
        """Publishes a set, which the next process call takes."""
        publish(chosen)
        engine["pending"] = chosen

    def expected(frames, rate):
        """One process call: a pending set is taken unless an on/off fade
        lasts, a new rate takes it at once, then rv_fade_run_limited's blend."""
        pending = engine["pending"]
        if (pending is not None and not switching()) or rate != engine["new"].rate:
            take(pending if pending is not None else engine["new"].chosen, rate)
            engine["pending"] = None
        new, old = engine["new"], engine["old"]
        new_out = new.play(frames) if new.on else None
        running = old is not None and engine["remaining"] and old.on
        old_out = old.play(frames) if running else None
        out = []
        for i, frame in enumerate(frames):
            remaining = engine["remaining"]
            if not remaining:
                out.append(frame if new_out is None else limiter.run(new_out[i]))
                continue
            weight = 0 if remaining > FADE else FADE + 1 - remaining
            if old_out is not None and new_out is not None:
                mixed = tuple(
                    (Limiter.clamp(a) * (FADE - weight) + Limiter.clamp(b) * weight)
                    >> 7
                    for a, b in zip(old_out[i], new_out[i], strict=True)
                )
                out.append(limiter.run(mixed))
            elif old_out is not None or new_out is not None:
                alone = limiter.run(old_out[i] if old_out is not None else new_out[i])
                if old_out is not None:
                    weight = FADE - weight
                out.append(
                    tuple(
                        (x * (FADE - weight) + y * weight) >> 7
                        for x, y in zip(frame, alone, strict=True)
                    )
                )
            else:
                out.append(frame)
            engine["remaining"] -= 1
        if not engine["remaining"]:
            engine["old"] = None
        return out

    def run(frames, rate, *splits):
        """Processes the frames in one buffer, or in buffers cut at the splits.
        A rate the engine has not seen starts its state over without a fade."""
        buffer = (ctypes.c_int16 * (2 * len(frames)))(
            *[s for frame in frames for s in frame]
        )
        edges = [0, *splits, len(frames)]
        for first, last in pairwise(edges):
            process(
                ctypes.cast(ctypes.byref(buffer, 4 * first), process.argtypes[0]),
                last - first,
                rate,
            )
        return [(buffer[2 * i], buffer[2 * i + 1]) for i in range(len(frames))]

    def play(frames, rate, *splits):
        """Runs the frames and checks them against the model, returning them."""
        got = run(frames, rate, *splits)
        edges = [0, *splits, len(frames)]
        model = [f for a, b in pairwise(edges) for f in expected(frames[a:b], rate)]
        assert got == model, (rate, splits)
        return got

    def interrupts():
        dll.test_irq_calls.restype = ctypes.c_uint32
        return dll.test_irq_calls()

    def turned_off():
        """Off, with the fade out of the delayed samples over."""
        switch(Set(on=0, threshold=-12))
        play(noise(FADE, 1), 44100)

    # Off leaves the stream alone.
    switch(Set(on=0, threshold=-12))
    frames = noise(count, 1)
    assert play(frames, 44100) == frames
    assert interrupts() == 2

    # On at three rates, bit for bit against the model across blocks. The
    # first look-ahead's worth of frames is the silence the delay line held;
    # turning on at the rate that plays hides it under the input, and fades in
    # the delayed samples after it.
    chosen = Set(on=1, threshold=-12, makeup=1, ratio=1, knee=1, attack=5, release=500)
    for rate in (44100, 48000, 8000):
        switch(chosen)
        length = 3 * rate // 1000  # the look-ahead
        for block in range(3):
            frames = noise(count, block + rate)
            got = play(frames, rate)
            if block == 0 and rate == 44100:
                assert got[:length] == frames[:length]
            elif block == 0:
                # The limiter plays out what it held from the old rate first.
                late = got[Limiter.FRAMES :]
                assert all(frame == (0, 0) for frame in late[:length])
                assert late[length] != (0, 0)

    # New values keep the state and fade in; Off fades the delayed samples out
    # into the input; on again fills the line, then fades in; a new rate during
    # a fade ends it and starts over.
    switch(chosen)
    play(noise(count, 20), 44100)
    louder = Set(on=1, threshold=-6, makeup=0, ratio=4, knee=0, attack=0, release=100)
    switch(louder)
    play(noise(count, 21), 44100)
    switch(Set(on=0, threshold=-12))
    frames = noise(count, 22)
    got = play(frames, 44100)
    assert got[FADE:] == frames[FADE:] and got[:FADE] != frames[:FADE]
    switch(louder)
    play(noise(64, 23), 44100)
    play(noise(count, 24), 32000)

    # The lead and the fade span buffer edges and end after their frames
    # whatever their size; a set taken during a fade restarts the fade from
    # the set that plays.
    turned_off()
    switch(chosen)
    play(noise(count, 25), 32000, 17, 100, 300)
    switch(louder)
    play(noise(64, 26), 32000)
    switch(chosen)
    play(noise(count, 27), 32000)

    # A set left while an on/off fade lasts waits for its end: on, then new
    # values 50 frames into the lead, which wait for the blend to end, across a
    # buffer edge, and are taken on the buffer after it.
    turned_off()
    switch(chosen)
    play(noise(50, 28), 44100)
    assert switching()
    switch(louder)
    play(noise(count, 29), 44100, 200)
    assert engine["pending"] is not None and not switching()
    play(noise(count, 30), 44100)
    assert engine["pending"] is None and engine["new"].chosen is louder

    # Attack 0 ms, every ratio and both knees: the model agrees at the edges.
    for ratio in range(5):
        for knee in range(2):
            chosen = Set(
                on=1,
                threshold=-24,
                makeup=1,
                ratio=ratio,
                knee=knee,
                attack=0,
                release=1000,
            )
            turned_off()
            switch(chosen)
            frames = noise(count, 30 + ratio) + noise(count, 40 + ratio, 6)
            play(frames, 44100)

    # A full-scale tone above a Limit threshold comes out well below full scale.
    chosen = Set(on=1, threshold=-24, makeup=0, ratio=4, knee=0, attack=0, release=100)
    turned_off()
    switch(chosen)
    for _ in range(10):
        got = play(sine(4410, 1000, 44100, 32767), 44100)
    peak = max(abs(s) for frame in got for s in frame)
    assert 0 < peak < 0.3 * 32767, peak

    # Auto makeup over a quiet start then full scale: the makeup lifts the
    # first loud frames past full scale, and the limiter holds them there.
    chosen = Set(on=1, threshold=-24, makeup=1, ratio=4, knee=1, attack=30, release=500)
    turned_off()
    switch(chosen)
    frames = noise(count, 50, 8) + noise(count, 51)
    wide = Player(44100, chosen).play(frames)
    assert max(abs(s) for frame in wide for s in frame) > 40000
    got = play(frames, 44100)
    assert all(-32767 <= s <= 32767 for frame in got for s in frame)


sdk = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="compressor-tests-") as name:
    work = Path(name)
    dll = build(
        work,
        "compressor.so",
        [
            root / "host.c",
            sdk / "rv_libc.c",
            dsp / "limiter.c",
            root.parent / "compressor.c",
            root.parent / "curve.c",
            root.parent / "design.c",
            root.parent / "labels.c",
        ],
        [sdk, dsp, root.parent],
    )
    design_tests(dll)
    label_tests(dll)
    process_tests(dll)
print("Compressor tests passed")
