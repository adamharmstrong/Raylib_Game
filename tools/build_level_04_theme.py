#!/usr/bin/env python3
"""Render the Counterweight Row theme for Power Pulley Panic Level 4."""

from __future__ import annotations

import math
import wave

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE, read_pcm_wav


BPM = 170.0
LOOP_BARS = 48
SOURCE_PHRASE_BARS = 16
SOURCE_PHRASE_FRAMES = 996_465
LOOP_FRAMES = SOURCE_PHRASE_FRAMES * (LOOP_BARS // SOURCE_PHRASE_BARS)
FRAMES_PER_BAR = SOURCE_PHRASE_FRAMES / SOURCE_PHRASE_BARS

STARTER_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 06 150BPM Cmin"
)
STEMS = {
    "ambient_guitar": "Clark Audio - MERCURY - Song Starter 06 - Ambient Guitar 170BPM Cmin.wav",
    "bass": "Clark Audio - MERCURY - Song Starter 06 - Bass 170BPM Cmin.wav",
    "break": "Clark Audio - MERCURY - Song Starter 06 - Break 170BPM Cmin.wav",
    "guitar": "Clark Audio - MERCURY - Song Starter 06 - Guitar 170BPM Cmin.wav",
    "key": "Clark Audio - MERCURY - Song Starter 06 - Key 170BPM Cmin.wav",
    "pad": "Clark Audio - MERCURY - Song Starter 06 - Pad 170BPM Cmin.wav",
    "piano": "Clark Audio - MERCURY - Song Starter 06 - Piano 170BPM Cmin.wav",
    "reverse_guitar": "Clark Audio - MERCURY - Song Starter 06 - Reverse Guitar 170BPM Cmin.wav",
}
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "DnB"
    / "Clark Audio - MERCURY - DnB Drum Loop 01 - 170BPM.wav"
)
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_level_04_counterweight_row.wav"


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def arranged_stem(name: str) -> np.ndarray:
    source = read_pcm_wav(STARTER_ROOT / STEMS[name])
    if len(source) != SOURCE_PHRASE_FRAMES:
        source = resample_to_frames(source, SOURCE_PHRASE_FRAMES)
    arranged = np.tile(source, (3, 1))

    # The starter alternates key-led and guitar-led eight-bar halves. In the
    # final 16 bars, fill each stem's silent half to combine both colors.
    midpoint = SOURCE_PHRASE_FRAMES // 2
    final_start = SOURCE_PHRASE_FRAMES * 2
    if name in {"ambient_guitar", "guitar", "pad"}:
        active = source[midpoint:]
        arranged[final_start : final_start + len(active)] = active
    elif name == "key":
        active = source[: SOURCE_PHRASE_FRAMES - midpoint]
        arranged[final_start + midpoint : final_start + SOURCE_PHRASE_FRAMES] = active

    return arranged


def prepared_drums() -> np.ndarray:
    source = read_pcm_wav(DRUM_PATH)
    repeated = np.tile(source, (6, 1))
    return resample_to_frames(repeated, LOOP_FRAMES)


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray([bar * FRAMES_PER_BAR for bar, _ in points], dtype=np.float32)
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def synthesize_counterweight_impacts() -> np.ndarray:
    impacts = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    duration = 0.72
    frames = int(duration * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    start_frequency = 82.4069
    end_frequency = 32.7032
    sweep = (end_frequency - start_frequency) / duration
    phase = 2.0 * math.pi * (
        start_frequency * time + 0.5 * sweep * time * time
    )
    body = (
        np.sin(phase)
        + 0.30 * np.sin(phase * 2.03)
        + 0.12 * np.sin(phase * 3.91)
    ) * np.exp(-time * 4.2)
    transient = np.sin(2.0 * math.pi * 188.0 * time) * np.exp(-time * 28.0)
    impact = (body * 0.090 + transient * 0.035).astype(np.float32)

    for index, bar in enumerate(range(0, LOOP_BARS, 8)):
        start = round(bar * FRAMES_PER_BAR)
        end = min(start + frames, LOOP_FRAMES)
        pan = -0.24 if index % 2 == 0 else 0.24
        left_gain = math.sqrt((1.0 - pan) * 0.5)
        right_gain = math.sqrt((1.0 + pan) * 0.5)
        impacts[start:end, 0] += impact[: end - start] * left_gain
        impacts[start:end, 1] += impact[: end - start] * right_gain
    return impacts


def synthesize_arrow_ticks() -> np.ndarray:
    """Add a quiet alternating tick that suggests the firing gallery."""
    ticks = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    frames = int(0.045 * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    tick = (
        np.sin(2.0 * math.pi * 1780.0 * time)
        + 0.35 * np.sin(2.0 * math.pi * 2517.0 * time)
    ) * np.exp(-time * 70.0) * 0.021
    beats_per_bar = 4.0
    frames_per_beat = FRAMES_PER_BAR / beats_per_bar
    for bar in range(LOOP_BARS):
        for beat, pan in ((1.50, -0.45), (3.25, 0.45)):
            start = round((bar * beats_per_bar + beat) * frames_per_beat)
            end = min(start + frames, LOOP_FRAMES)
            left_gain = math.sqrt((1.0 - pan) * 0.5)
            right_gain = math.sqrt((1.0 + pan) * 0.5)
            ticks[start:end, 0] += tick[: end - start] * left_gain
            ticks[start:end, 1] += tick[: end - start] * right_gain
    return ticks


def render() -> None:
    tracks = {name: arranged_stem(name) for name in STEMS}
    drums = prepared_drums()
    impacts = synthesize_counterweight_impacts()
    arrow_ticks = synthesize_arrow_ticks()

    gains = {
        "ambient_guitar": [(0, 0.34), (8, 0.46), (16, 0.38), (24, 0.58), (32, 0.64), (48, 0.34)],
        "bass": [(0, 0.34), (8, 0.40), (16, 0.28), (24, 0.38), (32, 0.44), (48, 0.34)],
        "break": [(0, 0.46), (8, 0.52), (16, 0.30), (24, 0.48), (32, 0.56), (48, 0.46)],
        "guitar": [(0, 0.42), (8, 0.56), (16, 0.38), (24, 0.64), (32, 0.70), (48, 0.42)],
        "key": [(0, 0.62), (8, 0.54), (16, 0.72), (24, 0.50), (32, 0.68), (48, 0.62)],
        "pad": [(0, 0.48), (8, 0.56), (16, 0.76), (24, 0.60), (32, 0.72), (48, 0.48)],
        "piano": [(0, 0.58), (8, 0.66), (16, 0.72), (24, 0.62), (32, 0.74), (48, 0.58)],
        "reverse_guitar": [(0, 0.36), (8, 0.48), (16, 0.52), (24, 0.56), (32, 0.62), (48, 0.36)],
    }

    melodic = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    for name, audio in tracks.items():
        melodic += audio * section_gain(gains[name])[:, None]

    drum_gain = section_gain(
        [(0, 0.48), (8, 0.56), (16, 0.27), (20, 0.34), (24, 0.53), (32, 0.62), (44, 0.56), (48, 0.48)]
    )

    # Fast half-beat breathing keeps the dense jungle layers from masking the
    # heavy boulder and counterweight register.
    frames_per_beat = FRAMES_PER_BAR / 4.0
    half_beat_phase = (np.arange(LOOP_FRAMES, dtype=np.float32) % (frames_per_beat * 0.5)) / (frames_per_beat * 0.5)
    duck = 0.70 + 0.30 * np.sin(math.pi * half_beat_phase) ** 0.75
    melodic *= duck[:, None]

    mix = melodic + drums * drum_gain[:, None] + impacts + arrow_ticks
    mix -= np.mean(mix, axis=0, keepdims=True)

    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.93
    mix[:, 1] = mid - side * 0.93

    mix = np.tanh(mix * 1.10) / math.tanh(1.10)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.82:
        mix *= 0.82 / peak

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
