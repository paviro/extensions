#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the host tests with a staged SDK: python3 tests/run.py SDK."""

import cmath
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
from harness import build, labeller  # noqa: E402
from harness import noise as stereo_noise  # noqa: E402
from model import Limiter, saturate  # noqa: E402

SHIFT = 12
Q30 = 1 << 30
UNITY = 1 << 15
RATES = (44100, 48000, 32000, 22050, 8000)
DELAYS = (5, 8, 10, 15, 30)
MAX_FRAMES = 30 * 48000 // 1000
FADE = 128


class Set(ctypes.Structure):
    _fields_ = [
        ("on", ctypes.c_uint8),
        ("delay_ms", ctypes.c_uint8),
        ("emphasis", ctypes.c_uint8),
        ("side_only", ctypes.c_uint8),
        ("mix", ctypes.c_uint8),
        ("low_hz", ctypes.c_uint16),
        ("high_hz", ctypes.c_uint16),
    ]


def coefficient_reference(hz, rate):
    """2 tan(pi f / rate) / (1 + tan), the cutoff held below 0.45 of the rate."""
    x = min(math.pi * min(hz, rate - 1) / rate, 0.45 * math.pi)
    t = math.tan(x)
    return 2 * t / (1 + t)


def coefficient_tests(dll):
    coefficient = dll.surround_coefficient
    for rate in RATES:
        for hz in list(range(40, 401, 40)) + list(range(600, 8001, 200)):
            got = coefficient(hz, rate) / Q30
            expected = coefficient_reference(hz, rate)
            assert abs(got - expected) <= 1e-4 * expected, (rate, hz, got, expected)
        # The -3 dB point of the one-pole sits at the cutoff.
        for hz in (320, 3400):
            if hz * 2.5 > rate:
                continue
            k = coefficient(hz, rate) / Q30
            z = cmath.exp(-2j * math.pi * hz / rate)
            h = k / 2 * (1 + z) / (1 - (1 - k) * z)
            assert abs(20 * math.log10(abs(h)) + 3.01) < 0.02, (rate, hz, abs(h))
    assert coefficient(8000, 8000) == coefficient(7999, 8000)
    assert coefficient(100, 0) == 0


def label_tests(dll):
    labels = labeller(dll.surround_label)
    ON, DELAY, MIX, EMPHASIS, LOW, HIGH, SIDE = range(7)

    assert labels(ON, 2) == ["Off", "On"]
    assert labels(DELAY, 5) == ["5 ms", "8 ms", "10 ms", "15 ms", "30 ms"]
    assert labels(MIX, 21) == [f"{5 * i} %" for i in range(21)]
    assert labels(EMPHASIS, 100) == [f"{i} %" for i in range(100)]
    assert labels(LOW, 10) == [f"{40 + 40 * i} Hz" for i in range(10)]
    highs = labels(HIGH, 38)
    assert highs[0] == "600 Hz" and highs[14] == "3.4k Hz" and highs[-1] == "8k Hz"
    assert labels(SIDE, 2) == ["Off", "On"]


def response_tests(dll):
    """The editor's curves: the band between Low and High near 0 dB and falling
    off either side, the late part the complement, nothing late while Off."""
    response = dll.surround_response
    response.argtypes = [ctypes.POINTER(Set), ctypes.c_uint]
    response.argtypes += [ctypes.POINTER(ctypes.c_int16)] * 2 + [ctypes.c_uint]
    response.restype = None
    points = 261
    hertz = [20 * 1000 ** (i / (points - 1)) for i in range(points)]

    def curves(chosen):
        kept = (ctypes.c_int16 * points)()
        late = (ctypes.c_int16 * points)()
        response(chosen, 44100, kept, late, points)
        return list(kept), list(late)

    def model(low_hz, high_hz, hz):
        out = []
        for cutoff in (low_hz, high_hz):
            k = coefficient_reference(cutoff, 44100)
            z = cmath.exp(-2j * math.pi * hz / 44100)
            out.append(k / 2 * (1 + z) / (1 - (1 - k) * z))
        kept = out[1] - out[0]
        return kept, 1 - kept

    for low_hz, high_hz in ((320, 3400), (40, 600), (400, 8000)):
        chosen = Set(1, 10, 35, 0, 50, low_hz, high_hz)
        kept, late = curves(chosen)
        for hz, k, d in zip(hertz, kept, late, strict=True):
            mk, md = model(low_hz, high_hz, hz)
            assert abs(k - 200 * math.log10(abs(mk))) <= 1, (hz, k, mk)
            assert abs(d - 200 * math.log10(abs(md))) <= 1, (hz, d, md)
        middle = next(
            i for i, hz in enumerate(hertz) if hz >= math.sqrt(low_hz * high_hz)
        )
        assert kept[middle] > -40
        assert kept[0] < -60 and kept[-1] < kept[middle]
        assert late[0] > -15 and late[-1] > -10
    kept, late = curves(Set(0, 10, 35, 0, 50, 320, 3400))
    assert kept == [0] * points and late == [-32768] * points
    kept, late = curves(Set(1, 10, 35, 0, 0, 320, 3400))
    assert kept == [0] * points and late == [-32768] * points


def output(sample):
    return (sample + (1 << (SHIFT - 1))) >> SHIFT


class Reference:
    """surround.c's loop in Python integers: one state at one rate."""

    def __init__(self, dll, chosen, rate):
        self.chosen = chosen
        self.on = chosen.on != 0 and chosen.mix != 0
        self.k_low = dll.surround_coefficient(chosen.low_hz, rate)
        self.k_high = dll.surround_coefficient(chosen.high_hz, rate)
        self.lead = UNITY - UNITY * chosen.emphasis // 200
        self.lag = UNITY + UNITY * chosen.emphasis // 200
        self.wet = UNITY * chosen.mix // 100
        self.length = min(chosen.delay_ms * rate // 1000, MAX_FRAMES)
        self.low = self.high = self.previous = 0
        self.delay = [0] * max(self.length, 1)
        self.index = 0

    def copy_filters(self, other):
        self.low, self.high, self.previous = other.low, other.high, other.previous

    def copy_line(self, other):
        self.delay, self.index = list(other.delay), other.index

    def play(self, frames):
        out = []
        side_only = self.chosen.side_only
        for left, right in frames:
            left <<= SHIFT
            right <<= SHIFT
            u = (left - right) >> 1 if side_only else right
            half = (u + self.previous) >> 1
            self.previous = u
            self.low += (self.k_low * (half - self.low)) >> 30
            self.high += (self.k_high * (half - self.high)) >> 30
            kept = self.high - self.low
            band = u - kept
            late = self.delay[self.index] << SHIFT
            self.delay[self.index] = saturate(output(band))
            self.index = (self.index + 1) % self.length
            processed = kept + late
            if side_only:
                mid = (left + right) >> 1
                wet_l = mid + ((u * self.lead) >> 15)
                wet_r = mid - ((processed * self.lag) >> 15)
            else:
                wet_l = (left * self.lead) >> 15
                wet_r = (processed * self.lag) >> 15
            out.append(
                (
                    output(left + (((wet_l - left) * self.wet) >> 15)),
                    output(right + (((wet_r - right) * self.wet) >> 15)),
                )
            )
        return out


def noise(count, seed, panned=False):
    frames = stereo_noise(count, seed)
    if panned:
        frames = [(left, 0) for left, _ in frames]
        frames[0] = (0, 32767)
    return frames


def process_tests(dll):
    publish = dll.surround_publish
    publish.argtypes = [ctypes.POINTER(Set)]
    publish.restype = None
    process = dll.surround_process
    process.argtypes = [ctypes.POINTER(ctypes.c_int16)] + [ctypes.c_uint32] * 2
    process.restype = None
    dll.test_irq_calls.restype = ctypes.c_uint32
    count = 512

    def make(
        on=1, delay_ms=10, emphasis=35, side_only=0, mix=50, low_hz=320, high_hz=3400
    ):
        return Set(on, delay_ms, emphasis, side_only, mix, low_hz, high_hz)

    engine = {"new": Reference(dll, make(on=0), 44100), "rate": 44100}
    engine.update(old=None, remaining=0, pending=None)
    limiter = Limiter()

    def switching():
        old = engine["old"]
        return bool(
            engine["remaining"] and old is not None and old.on != engine["new"].on
        )

    def take(chosen, rate):
        old, new = engine["new"], Reference(dll, chosen, rate)
        same_rate = rate == engine["rate"]
        was = old.chosen
        new_input = not old.on or not same_rate or chosen.side_only != was.side_only
        restart = new_input or chosen.delay_ms != was.delay_ms
        if not new_input:
            new.copy_filters(old)
        if not restart:
            new.copy_line(old)
        turns_on = new.on and not old.on
        if turns_on:
            limiter.rest()
        engine["new"] = new
        engine["rate"] = rate
        if same_rate:
            if turns_on:
                lead = new.length + Limiter.FRAMES
            elif restart and new.on:
                lead = new.length
            else:
                lead = 0
            engine["old"], engine["remaining"] = old, FADE + lead
        else:
            engine["old"], engine["remaining"] = None, 0

    def switch(chosen):
        publish(chosen)
        engine["pending"] = chosen

    def expected(frames, rate):
        pending = engine["pending"]
        lead = engine["remaining"] > FADE
        held = switching() or lead
        if (pending is not None and not held) or rate != engine["rate"]:
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
        got = run(frames, rate, *splits)
        edges = [0, *splits, len(frames)]
        model = [f for a, b in pairwise(edges) for f in expected(frames[a:b], rate)]
        assert got == model, (rate, splits)
        return got

    # Off and Mix 0 leave the stream alone; one publish masks interrupts once.
    switch(make(on=0))
    frames = noise(count, 1)
    assert play(frames, 44100) == frames
    assert dll.test_irq_calls() == 2
    switch(make(mix=0))
    frames = noise(count, 2)
    assert play(frames, 44100) == frames

    # Every delay at every rate, bit for bit, across buffer edges; a right-only
    # impulse never reaches the left ear at Emphasis 0, its kept band passes at
    # once and its late part arrives after the line.
    for rate in RATES:
        for delay in DELAYS:
            chosen = make(delay_ms=delay, emphasis=0, mix=100)
            switch(chosen)
            for block in range(2):
                play(noise(count, block + rate + delay, panned=True), rate, 17, 100)
            alone = Reference(dll, chosen, rate)
            frames = [(0, 32767)] + [(0, 0)] * (MAX_FRAMES + 8)
            out = alone.play(frames)
            length = delay * rate // 1000
            assert all(left == 0 for left, _ in out)
            assert out[0][1] != 0
            assert abs(out[length][1]) > abs(out[length - 1][1]) + 100, (rate, delay)

    # Mono input in Side Only comes out unchanged at any Emphasis: it moves
    # the side parts only.
    for emphasis in (0, 99):
        chosen = make(side_only=1, emphasis=emphasis, mix=100)
        switch(chosen)
        frames = [(s, s) for s, _ in noise(count, 9 + emphasis)]
        play(frames, 44100)
        assert Reference(dll, chosen, 44100).play(frames) == frames

    # New cutoffs, emphasis or mix keep the line and filters and fade; a new
    # delay or Side Only starts them over behind a fade as long as the line.
    switch(make(low_hz=80, high_hz=6000))
    play(noise(count, 20), 44100)
    switch(make(low_hz=80, high_hz=6000, emphasis=99))
    play(noise(count, 21), 44100)
    switch(make(low_hz=80, high_hz=6000, emphasis=99, mix=25))
    play(noise(count, 22), 44100, 50)
    switch(make(delay_ms=30))
    play(noise(4 * count, 23), 44100)
    switch(make(delay_ms=30, side_only=1))
    play(noise(4 * count, 24), 44100, 33)

    # A rate change ends a fade and starts over; a set left during an on/off
    # fade waits for its end.
    switch(make(delay_ms=5))
    play(noise(64, 25), 44100)
    play(noise(count, 26), 48000)
    switch(make(on=0))
    play(noise(count, 27), 48000)
    switch(make(delay_ms=8))
    play(noise(50, 28), 48000)
    assert switching()
    switch(make(delay_ms=15))
    play(noise(2 * count, 29), 48000, 40)
    assert engine["pending"] is not None and not switching()
    play(noise(2 * count, 30), 48000)
    assert engine["pending"] is None

    # A set left while a new line fills waits for the fill, so a fade never
    # carries over a half-filled line.
    switch(make(delay_ms=10))
    play(noise(count, 31), 44100)
    switch(make(delay_ms=30))
    play(noise(100, 32), 44100)
    assert not switching() and engine["remaining"] > FADE
    switch(make(delay_ms=30, mix=100))
    play(noise(count, 33), 44100)
    assert engine["pending"] is not None
    play(noise(2 * count, 34), 44100, 300)
    assert engine["pending"] is not None and not engine["remaining"]
    play(noise(count, 35), 44100)
    assert engine["pending"] is None and engine["new"].chosen.mix == 100

    # A full-scale anti-phase square at Emphasis 99 goes over, which the limiter
    # holds at full scale instead of wrapping.
    chosen = make(emphasis=99, mix=100)
    switch(chosen)
    square = [
        (32767, -32767) if i // 50 % 2 else (-32767, 32767) for i in range(4 * count)
    ]
    wide = Reference(dll, chosen, 48000).play(square)
    assert max(abs(s) for frame in wide for s in frame) > 32767
    got = play(square, 48000)
    assert 32000 < max(abs(s) for frame in got for s in frame) <= 32767


sdk = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="surround-tests-") as name:
    work = Path(name)
    sources = [root / "host.c", sdk / "rv_libc.c"]
    sources += [dsp / file for file in ("math.c", "limiter.c")]
    sources += [
        root.parent / file
        for file in ("surround.c", "design.c", "labels.c", "response.c")
    ]
    dll = build(work, "surround.so", sources, [sdk, dsp, shared, root.parent])
    dll.surround_coefficient.argtypes = [ctypes.c_uint, ctypes.c_uint]
    dll.surround_coefficient.restype = ctypes.c_uint32
    coefficient_tests(dll)
    label_tests(dll)
    response_tests(dll)
    process_tests(dll)
print("Surround tests passed")
