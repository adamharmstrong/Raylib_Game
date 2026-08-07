#!/usr/bin/env python3
"""Render the Clocktower Core theme for Power Pulley Panic Level 6."""

from __future__ import annotations

import math
import wave
from pathlib import Path

import numpy as np

from build_level_05_theme import read_stereo_pcm_wav
from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE


BPM = 172.0
LOOP_BARS = 48
SOURCE_PHRASE_BARS = 32
RHYTHM_PHRASE_BARS = 8
FRAMES_PER_BAR = SAMPLE_RATE * 4.0 * 60.0 / BPM
SOURCE_PHRASE_FRAMES = round(SOURCE_PHRASE_BARS * FRAMES_PER_BAR)
RHYTHM_PHRASE_FRAMES = round(RHYTHM_PHRASE_BARS * FRAMES_PER_BAR)
LOOP_FRAMES = round(LOOP_BARS * FRAMES_PER_BAR)
FRAMES_PER_BEAT = FRAMES_PER_BAR / 4.0

STARTER_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 05 170BPM Bmin"
)
STEMS = {
    "arp": "Clark Audio - MERCURY - Song Starter 05 - Arp 170BPM Bmin.wav",
    "bass": "Clark Audio - MERCURY - Song Starter 05 - Bass 170BPM Bmin.wav",
    "lead": "Clark Audio - MERCURY - Song Starter 05 - Lead 170BPM Bmin.wav",
    "pad": "Clark Audio - MERCURY - Song Starter 05 - Pad 170BPM Bmin.wav",
}
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "DnB"
    / "Clark Audio - MERCURY - DnB Drum Loop 02 - 172BPM.wav"
)
HIHAT_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Hihat Loops"
    / "Clark Audio - MERCURY - HiHat Loop 02 - 172BPM.wav"
)
OUTPUT_PATH = (
    PACK_ROOT
    / "processed"
    / "power_pulley_panic_level_06_clocktower_core.wav"
)


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def prepared_stem(name: str) -> np.ndarray:
    phrase = resample_to_frames(
        read_stereo_pcm_wav(STARTER_ROOT / STEMS[name]), SOURCE_PHRASE_FRAMES
    )
    return np.tile(phrase, (2, 1))[:LOOP_FRAMES]


def prepared_rhythm(path: Path) -> np.ndarray:
    phrase = resample_to_frames(read_stereo_pcm_wav(path), RHYTHM_PHRASE_FRAMES)
    return np.tile(
        phrase, (LOOP_BARS // RHYTHM_PHRASE_BARS, 1)
    )[:LOOP_FRAMES]


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray(
        [bar * FRAMES_PER_BAR for bar, _ in points], dtype=np.float32
    )
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def add_panned(
    destination: np.ndarray, signal: np.ndarray, start: int, pan: float
) -> None:
    end = min(start + len(signal), LOOP_FRAMES)
    if end <= start:
        return
    left_gain = math.sqrt((1.0 - pan) * 0.5)
    right_gain = math.sqrt((1.0 + pan) * 0.5)
    destination[start:end, 0] += signal[: end - start] * left_gain
    destination[start:end, 1] += signal[: end - start] * right_gain


def synthesize_escapement() -> np.ndarray:
    """Add alternating clock ticks on every beat from the opening frame."""
    escapement = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    tick_frames = int(0.035 * SAMPLE_RATE)
    tock_frames = int(0.060 * SAMPLE_RATE)
    tick_time = np.arange(tick_frames, dtype=np.float32) / SAMPLE_RATE
    tock_time = np.arange(tock_frames, dtype=np.float32) / SAMPLE_RATE
    tick = (
        np.sin(2.0 * math.pi * 2760.0 * tick_time)
        + 0.34 * np.sin(2.0 * math.pi * 4215.0 * tick_time)
    ) * np.exp(-tick_time * 105.0) * 0.021
    tock = (
        np.sin(2.0 * math.pi * 1040.0 * tock_time)
        + 0.30 * np.sin(2.0 * math.pi * 1685.0 * tock_time)
    ) * np.exp(-tock_time * 62.0) * 0.027

    for beat in range(LOOP_BARS * 4):
        start = round(beat * FRAMES_PER_BEAT)
        if beat % 2 == 0:
            add_panned(escapement, tick.astype(np.float32), start, -0.32)
        else:
            add_panned(escapement, tock.astype(np.float32), start, 0.32)
    return escapement


def bell_note(frequency: float, gain: float = 1.0) -> np.ndarray:
    duration = 1.05
    frames = int(duration * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    phase = 2.0 * math.pi * frequency * time
    tone = (
        np.sin(phase)
        + 0.42 * np.sin(phase * 2.01)
        + 0.19 * np.sin(phase * 3.97)
        + 0.09 * np.sin(phase * 6.08)
    )
    attack = np.minimum(time / 0.003, 1.0)
    envelope = attack * (
        0.75 * np.exp(-time * 3.9) + 0.25 * np.exp(-time * 12.0)
    )
    return (tone * envelope * 0.031 * gain).astype(np.float32)


def synthesize_belfry_hook() -> np.ndarray:
    """Add the immediate B-minor 'XII' hook at every eight-bar landing."""
    hook = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    notes = (987.767, 1174.659, 1479.978, 1975.533)
    beat_offsets = (0.0, 0.75, 1.5, 3.0)
    pans = (-0.26, 0.10, 0.28, 0.0)
    for landing_index, bar in enumerate(range(0, LOOP_BARS, 8)):
        landing_gain = 0.92 + 0.055 * landing_index
        for frequency, beat, pan in zip(notes, beat_offsets, pans):
            start = round(bar * FRAMES_PER_BAR + beat * FRAMES_PER_BEAT)
            add_panned(hook, bell_note(frequency, landing_gain), start, pan)
    return hook


def synthesize_hand_locks() -> np.ndarray:
    """Mark the three hand stations with progressively higher metal strikes."""
    impacts = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    duration = 0.95
    frames = int(duration * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    for index, (bar, frequency) in enumerate(
        ((8, 123.471), (16, 146.832), (32, 184.997))
    ):
        phase = 2.0 * math.pi * frequency * time
        metal = (
            np.sin(phase)
            + 0.31 * np.sin(phase * 2.73)
            + 0.16 * np.sin(phase * 5.19)
        ) * np.exp(-time * 4.1)
        transient = (
            np.sin(2.0 * math.pi * 630.0 * time) * np.exp(-time * 34.0)
        )
        strike = (metal * 0.064 + transient * 0.028).astype(np.float32)
        pan = (-0.2, 0.2, 0.0)[index]
        add_panned(impacts, strike, round(bar * FRAMES_PER_BAR), pan)
    return impacts


def render() -> None:
    tracks = {name: prepared_stem(name) for name in STEMS}
    drums = prepared_rhythm(DRUM_PATH)
    hihat = prepared_rhythm(HIHAT_PATH)
    escapement = synthesize_escapement()
    belfry_hook = synthesize_belfry_hook()
    hand_locks = synthesize_hand_locks()

    gains = {
        "arp": [(0, 0.90), (8, 0.96), (16, 1.02), (24, 0.62),
                (28, 0.74), (32, 1.06), (40, 1.14), (48, 0.90)],
        "bass": [(0, 0.48), (8, 0.55), (16, 0.62), (24, 0.31),
                 (28, 0.43), (32, 0.66), (40, 0.73), (48, 0.48)],
        "lead": [(0, 0.30), (8, 0.49), (16, 0.72), (24, 0.24),
                 (28, 0.38), (32, 0.86), (40, 0.98), (48, 0.30)],
        "pad": [(0, 0.56), (8, 0.72), (16, 0.85), (24, 1.05),
                (28, 0.74), (32, 0.91), (40, 1.01), (48, 0.56)],
    }

    melodic = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    for name, audio in tracks.items():
        melodic += audio * section_gain(gains[name])[:, None]
    melodic += belfry_hook
    melodic += hand_locks

    drum_gain = section_gain(
        [(0, 0.46), (8, 0.51), (16, 0.56), (24, 0.24),
         (28, 0.34), (32, 0.59), (40, 0.65), (48, 0.46)]
    )
    hihat_gain = section_gain(
        [(0, 0.08), (8, 0.14), (16, 0.20), (24, 0.04),
         (28, 0.09), (32, 0.22), (40, 0.27), (48, 0.08)]
    )
    escapement_gain = section_gain(
        [(0, 0.92), (16, 1.0), (24, 1.32), (28, 1.14),
         (32, 1.02), (48, 0.92)]
    )

    beat_phase = (
        np.arange(LOOP_FRAMES, dtype=np.float32) % FRAMES_PER_BEAT
    ) / FRAMES_PER_BEAT
    duck = 0.74 + 0.26 * np.sin(math.pi * beat_phase) ** 0.70
    melodic *= duck[:, None]

    mix = melodic
    mix += drums * drum_gain[:, None]
    mix += hihat * hihat_gain[:, None]
    mix += escapement * escapement_gain[:, None]
    mix -= np.mean(mix, axis=0, keepdims=True)

    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.94
    mix[:, 1] = mid - side * 0.94

    mix = np.tanh(mix * 1.10) / math.tanh(1.10)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.82:
        mix *= 0.82 / peak

    fade_frames = int(0.008 * SAMPLE_RATE)
    fade = np.sin(
        np.linspace(0.0, math.pi * 0.5, fade_frames, dtype=np.float32)
    )
    mix[:fade_frames] *= fade[:, None]
    mix[-fade_frames:] *= fade[::-1, None]

    pcm = np.clip(np.rint(mix * 32_767.0), -32_768, 32_767).astype("<i2")
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUTPUT_PATH), "wb") as destination:
        destination.setnchannels(2)
        destination.setsampwidth(2)
        destination.setframerate(SAMPLE_RATE)
        destination.writeframes(pcm.tobytes())

    rms = float(np.sqrt(np.mean(np.square(mix), dtype=np.float64)))
    print(
        f"Rendered {OUTPUT_PATH.relative_to(PROJECT_ROOT)}\n"
        f"  bars={LOOP_BARS} bpm={BPM:.0f} duration={LOOP_FRAMES / SAMPLE_RATE:.3f}s "
        f"peak={np.max(np.abs(mix)):.3f} rms={rms:.3f} "
        f"size={OUTPUT_PATH.stat().st_size / (1024 * 1024):.1f} MiB"
    )


if __name__ == "__main__":
    render()
