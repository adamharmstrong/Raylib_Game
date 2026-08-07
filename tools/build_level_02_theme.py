#!/usr/bin/env python3
"""Render the Rotary Latch Lab theme for Power Pulley Panic Level 2."""

from __future__ import annotations

import math
import wave

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE, read_pcm_wav


BPM = 126.0
BEAT_FRAMES = int(SAMPLE_RATE * 60.0 / BPM)
BAR_FRAMES = BEAT_FRAMES * 4
LOOP_BARS = 32
LOOP_FRAMES = BAR_FRAMES * LOOP_BARS

STAB_PATH = (
    PACK_ROOT
    / "Instruments"
    / "Stab Loops"
    / "Clark Audio - MERCURY - Stab Loop 03 - 126BPM Dmin.wav"
)
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "Garage"
    / "Clark Audio - MERCURY - Garage Drum Loop 02 - 127BPM.wav"
)
CRASH_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "Garage"
    / "Stems"
    / "Full Drum Loop 02 127BPM"
    / "Clark Audio - MERCURY - Full Drum Loop 03 - Crash 127BPM.wav"
)
PERCUSSION_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Percussion Loops"
    / "Clark Audio - MERCURY - Percussion Loop 01 - 125BPM.wav"
)
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_level_02_rotary_latch_lab.wav"


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def prepared_loop(path, phrase_bars: int, repeats: int) -> np.ndarray:
    source = read_pcm_wav(path)
    phrase_frames = BAR_FRAMES * phrase_bars
    if len(source) != phrase_frames:
        source = resample_to_frames(source, phrase_frames)
    tiled = np.tile(source, (repeats, 1))
    if len(tiled) != LOOP_FRAMES:
        raise ValueError(f"Unexpected prepared length for {path.name}: {len(tiled)}")
    return tiled


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray([bar * BAR_FRAMES for bar, _ in points], dtype=np.float32)
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def add_synth_note(
    destination: np.ndarray,
    start: int,
    frames: int,
    frequency: float,
    amplitude: float,
) -> None:
    frames = min(frames, LOOP_FRAMES - start)
    if frames <= 0:
        return
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    oscillator = (
        np.sin(2.0 * math.pi * frequency * time)
        + 0.20 * np.sin(4.0 * math.pi * frequency * time)
        + 0.05 * np.sin(6.0 * math.pi * frequency * time)
    )
    envelope = np.ones(frames, dtype=np.float32)
    attack = min(int(0.010 * SAMPLE_RATE), frames)
    release = min(int(0.065 * SAMPLE_RATE), frames)
    envelope[:attack] = np.linspace(0.0, 1.0, attack, dtype=np.float32)
    envelope[-release:] *= np.linspace(1.0, 0.0, release, dtype=np.float32)
    note = np.tanh(oscillator * 0.92) * envelope * amplitude
    destination[start : start + frames, 0] += note
    destination[start : start + frames, 1] += note


def synthesize_bass() -> np.ndarray:
    bass = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    notes = {
        "D1": 36.7081,
        "F1": 43.6535,
        "A1": 55.0,
        "Bb1": 58.2705,
        "C2": 65.4064,
    }
    for bar in range(LOOP_BARS):
        if bar % 2 == 0:
            pattern = [(0.0, 0.82, "D1", 0.30), (2.0, 0.46, "A1", 0.19), (3.25, 0.42, "C2", 0.18)]
        else:
            last_note = "Bb1" if bar % 8 == 7 else "A1"
            pattern = [
                (0.0, 0.72, "D1", 0.28),
                (1.5, 0.40, "F1", 0.18),
                (2.75, 0.36, "C2", 0.18),
                (3.5, 0.28, last_note, 0.16),
            ]
        for beat, duration, note, amplitude in pattern:
            start = int((bar * 4.0 + beat) * BEAT_FRAMES)
            add_synth_note(bass, start, int(duration * BEAT_FRAMES), notes[note], amplitude)
    return bass


def synthesize_latch_motif() -> np.ndarray:
    """Place five pitched metallic ticks across every two-bar cycle."""
    motif = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    hit_beats = [0.0, 1.5, 2.75, 4.25, 6.5]
    frequencies = [587.33, 440.0, 523.25, 698.46, 587.33]  # D5, A4, C5, F5, D5
    pans = [-0.64, -0.32, 0.0, 0.32, 0.64]
    hit_frames = int(0.095 * SAMPLE_RATE)
    time = np.arange(hit_frames, dtype=np.float32) / SAMPLE_RATE

    for phrase_bar in range(0, LOOP_BARS, 2):
        phrase_scale = 0.80 if 16 <= phrase_bar < 20 else 1.0
        for beat, frequency, pan in zip(hit_beats, frequencies, pans):
            start = int((phrase_bar * 4.0 + beat) * BEAT_FRAMES)
            end = min(start + hit_frames, LOOP_FRAMES)
            frames = end - start
            if frames <= 0:
                continue
            ring = (
                np.sin(2.0 * math.pi * frequency * time[:frames])
                + 0.42 * np.sin(2.0 * math.pi * frequency * 1.4142 * time[:frames])
                + 0.18 * np.sin(2.0 * math.pi * frequency * 2.09 * time[:frames])
            )
            ring *= np.exp(-time[:frames] * 34.0) * 0.075 * phrase_scale
            left_gain = math.sqrt((1.0 - pan) * 0.5)
            right_gain = math.sqrt((1.0 + pan) * 0.5)
            motif[start:end, 0] += ring * left_gain
            motif[start:end, 1] += ring * right_gain
    return motif


def synthesize_rotor_drone() -> np.ndarray:
    time = np.arange(LOOP_FRAMES, dtype=np.float32) / SAMPLE_RATE
    loop_seconds = LOOP_FRAMES / SAMPLE_RATE
    slow_phase = 2.0 * math.pi * time / loop_seconds
    pulse = 0.72 + 0.28 * np.sin(slow_phase * 4.0) ** 2
    base = (
        np.sin(2.0 * math.pi * 73.4162 * time)
        + 0.24 * np.sin(2.0 * math.pi * 110.0 * time)
    ) * 0.040 * pulse
    side = np.sin(slow_phase) * 0.22
    drone = np.empty((LOOP_FRAMES, 2), dtype=np.float32)
    drone[:, 0] = base * (1.0 + side)
    drone[:, 1] = base * (1.0 - side)
    return drone


def render() -> None:
    stabs = prepared_loop(STAB_PATH, 4, 8)
    drums = prepared_loop(DRUM_PATH, 8, 4)
    crashes = prepared_loop(CRASH_PATH, 8, 4)
    percussion = prepared_loop(PERCUSSION_PATH, 16, 2)
    bass = synthesize_bass()
    latch_motif = synthesize_latch_motif()
    rotor_drone = synthesize_rotor_drone()

    stab_gain = section_gain(
        [(0, 0.44), (8, 0.50), (16, 0.31), (20, 0.42), (24, 0.56), (32, 0.44)]
    )
    drum_gain = section_gain(
        [(0, 0.52), (8, 0.58), (16, 0.28), (20, 0.44), (24, 0.62), (32, 0.52)]
    )
    percussion_gain = section_gain(
        [(0, 0.10), (8, 0.18), (16, 0.08), (20, 0.20), (24, 0.24), (32, 0.10)]
    )
    bass_gain = section_gain(
        [(0, 0.86), (8, 0.98), (16, 0.55), (20, 0.76), (24, 1.05), (32, 0.86)]
    )
    crash_gain = section_gain(
        [(0, 0.34), (8, 0.38), (16, 0.22), (24, 0.42), (32, 0.34)]
    )

    # Precise eighth-note breathing reinforces the latch alignment mechanic.
    eighth_phase = (np.arange(LOOP_FRAMES, dtype=np.float32) % (BEAT_FRAMES // 2)) / (BEAT_FRAMES // 2)
    stab_pulse = 0.66 + 0.34 * np.sin(math.pi * eighth_phase) ** 0.8

    mix = (
        stabs * (stab_gain * stab_pulse)[:, None]
        + drums * drum_gain[:, None]
        + percussion * percussion_gain[:, None]
        + crashes * crash_gain[:, None]
        + bass * bass_gain[:, None]
        + latch_motif
        + rotor_drone
    )

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
