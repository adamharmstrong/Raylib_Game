#!/usr/bin/env python3
"""Render the short, hook-first Power Pulley Panic title-screen theme."""

from __future__ import annotations

import math
import wave
from pathlib import Path

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE, read_pcm_wav


BPM = 140.0
BEATS_PER_BAR = 4
BAR_FRAMES = int(SAMPLE_RATE * 60.0 * BEATS_PER_BAR / BPM)
LOOP_BARS = 16
LOOP_FRAMES = BAR_FRAMES * LOOP_BARS
HALF_LOOP_FRAMES = LOOP_FRAMES // 2

STARTER_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 03 140BPM Gmin"
)
DRUM_ROOT = PACK_ROOT / "Drum Loops"
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_title_theme.wav"

STARTER_STEMS = {
    "atmo": "Clark Audio - MERCURY - Song Starter 03 - Atmo 140BPM Gmin.wav",
    "downlifter": "Clark Audio - MERCURY - Song Starter 03 - Downlifter 140BPM Gmin.wav",
    "lead_1": "Clark Audio - MERCURY - Song Starter 03 - Lead 1 140BPM Gmin.wav",
    "lead_2": "Clark Audio - MERCURY - Song Starter 03 - Lead 2 140BPM Gmin.wav",
    "reese": "Clark Audio - MERCURY - Song Starter 03 - Reese 140BPM Gmin.wav",
    "stab": "Clark Audio - MERCURY - Song Starter 03 - Stab 140BPM Gmin.wav",
    "sub": "Clark Audio - MERCURY - Song Starter 03 - Sub 140BPM Gmin.wav",
}

GARAGE_DRUM_PATH = (
    DRUM_ROOT
    / "Full Drum Loops"
    / "Garage"
    / "Clark Audio - MERCURY - Garage Drum Loop 01 - 135BPM.wav"
)
PERCUSSION_PATH = (
    DRUM_ROOT
    / "Percussion Loops"
    / "Clark Audio - MERCURY - Percussion Loop 04 - 135BPM.wav"
)


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    """Resample a short rhythmic loop with linear interpolation."""
    source_positions = np.arange(target_frames, dtype=np.float64)
    source_positions *= len(audio) / target_frames
    left = np.floor(source_positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (source_positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray([bar * BAR_FRAMES for bar, _ in points], dtype=np.float32)
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def starter_slice(name: str) -> np.ndarray:
    audio = read_pcm_wav(STARTER_ROOT / STARTER_STEMS[name])
    # Lead 1, Reese, atmosphere, and the downlifter occupy the source's first
    # 16-bar phrase. The source delays Lead 2, Stab, and Sub until its next
    # phrase, so pull that harmonically matching phrase forward for bar-one
    # impact in the title arrangement.
    offset = LOOP_FRAMES if name in {"lead_2", "stab", "sub"} else 0
    arranged = audio[offset : offset + LOOP_FRAMES]
    if len(arranged) != LOOP_FRAMES:
        raise ValueError(f"Unexpected source length for {name}: {len(arranged)} frames")
    return arranged


def eight_bar_drum_loop(path: Path) -> np.ndarray:
    source = read_pcm_wav(path)
    accelerated = resample_to_frames(source, HALF_LOOP_FRAMES)
    return np.concatenate((accelerated, accelerated), axis=0)


def render() -> None:
    drums = eight_bar_drum_loop(GARAGE_DRUM_PATH)
    percussion = eight_bar_drum_loop(PERCUSSION_PATH)

    # The hook, bass, and full beat all begin at frame zero. Later automation
    # provides contrast without hiding the title's identity behind an intro.
    gains = {
        "atmo": [(0, 0.24), (4, 0.18), (8, 0.27), (12, 0.18), (16, 0.24)],
        "downlifter": [(0, 0.34), (4, 0.05), (8, 0.16), (12, 0.04), (16, 0.34)],
        "lead_1": [(0, 0.68), (4, 0.58), (8, 0.42), (10, 0.62), (14, 0.70), (16, 0.68)],
        "lead_2": [(0, 0.12), (4, 0.24), (8, 0.36), (12, 0.22), (16, 0.12)],
        "reese": [(0, 0.43), (4, 0.38), (8, 0.30), (10, 0.43), (14, 0.48), (16, 0.43)],
        "stab": [(0, 0.22), (4, 0.34), (8, 0.18), (12, 0.38), (16, 0.22)],
        "sub": [(0, 0.27), (4, 0.25), (8, 0.18), (10, 0.27), (14, 0.30), (16, 0.27)],
    }

    melodic_mix = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    for name in STARTER_STEMS:
        stem = starter_slice(name)
        melodic_mix += stem * section_gain(gains[name])[:, None]

    # A short beat-synchronous duck leaves room for each kick and makes the
    # machinery-like pulse more obvious even at low title-screen volume.
    duck = np.ones(LOOP_FRAMES, dtype=np.float32)
    beat_frames = int(SAMPLE_RATE * 60.0 / BPM)
    release_frames = int(0.11 * SAMPLE_RATE)
    release = np.linspace(0.72, 1.0, release_frames, dtype=np.float32)
    for beat_start in range(0, LOOP_FRAMES, beat_frames):
        end = min(beat_start + release_frames, LOOP_FRAMES)
        duck[beat_start:end] = np.minimum(duck[beat_start:end], release[: end - beat_start])
    melodic_mix *= duck[:, None]

    drum_gain = section_gain(
        [(0, 0.58), (4, 0.54), (8, 0.48), (9, 0.60), (12, 0.62), (16, 0.58)]
    )
    percussion_gain = section_gain(
        [(0, 0.20), (4, 0.25), (8, 0.14), (10, 0.22), (14, 0.27), (16, 0.20)]
    )
    mix = melodic_mix + drums * drum_gain[:, None] + percussion * percussion_gain[:, None]

    # Center the low end slightly and retain width in the hook and atmosphere.
    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.92
    mix[:, 1] = mid - side * 0.92
    mix -= np.mean(mix, axis=0, keepdims=True)

    mix = np.tanh(mix * 1.12) / math.tanh(1.12)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.84:
        mix *= 0.84 / peak

    # The musical sources are cut on exact bar boundaries. Tiny endpoint fades
    # guarantee a zero-delta stream wrap without softening the immediate hit.
    fade_frames = int(0.008 * SAMPLE_RATE)
    fade = np.sin(np.linspace(0.0, math.pi * 0.5, fade_frames, dtype=np.float32))
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
