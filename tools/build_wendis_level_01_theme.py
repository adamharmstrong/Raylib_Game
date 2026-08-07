#!/usr/bin/env python3
"""Render Wendi's Three-Step Tumble theme from original synthesized material."""

from __future__ import annotations

import math
import wave
from pathlib import Path

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PATH = (
    PROJECT_ROOT
    / "assets"
    / "first_party"
    / "audio"
    / "power_pulley_panic_wendi_01_three_step_tumble.wav"
)

SAMPLE_RATE = 44_100
BPM = 132.0
LOOP_BARS = 24
FRAMES_PER_BEAT = SAMPLE_RATE * 60.0 / BPM
LOOP_FRAMES = round(LOOP_BARS * 4.0 * FRAMES_PER_BEAT)
RNG = np.random.default_rng(301)


def midi_frequency(note: int) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def add_wrapped(
    destination: np.ndarray,
    signal: np.ndarray,
    start: int,
    gain: float = 1.0,
    pan: float = 0.0,
) -> None:
    """Place a mono sound in the seamless stereo loop, wrapping its tail."""
    if len(signal) == 0:
        return
    start %= LOOP_FRAMES
    left_gain = gain * math.sqrt((1.0 - pan) * 0.5)
    right_gain = gain * math.sqrt((1.0 + pan) * 0.5)
    position = 0
    while position < len(signal):
        count = min(len(signal) - position, LOOP_FRAMES - start)
        destination[start : start + count, 0] += signal[position : position + count] * left_gain
        destination[start : start + count, 1] += signal[position : position + count] * right_gain
        position += count
        start = 0


def beat_frame(bar: int, beat: float) -> int:
    return round((bar * 4.0 + beat) * FRAMES_PER_BEAT)


def shaped_tone(
    frequency: float,
    seconds: float,
    attack: float,
    release: float,
    brightness: float = 0.25,
) -> np.ndarray:
    frames = max(1, round(seconds * SAMPLE_RATE))
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    phase = 2.0 * math.pi * frequency * time
    tone = (
        np.sin(phase)
        + brightness * np.sin(phase * 2.0)
        + brightness * 0.28 * np.sin(phase * 3.0)
    )
    envelope = np.ones(frames, dtype=np.float32)
    attack_frames = min(frames, round(attack * SAMPLE_RATE))
    release_frames = min(frames, round(release * SAMPLE_RATE))
    if attack_frames:
        envelope[:attack_frames] *= np.linspace(0.0, 1.0, attack_frames, dtype=np.float32)
    if release_frames:
        envelope[-release_frames:] *= np.linspace(1.0, 0.0, release_frames, dtype=np.float32)
    return (tone * envelope).astype(np.float32)


def pluck(frequency: float, seconds: float = 0.42) -> np.ndarray:
    frames = round(seconds * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    phase = 2.0 * math.pi * frequency * time
    metallic = (
        np.sin(phase)
        + 0.44 * np.sin(phase * 2.01)
        + 0.20 * np.sin(phase * 3.98)
        + 0.09 * np.sin(phase * 7.03)
    )
    envelope = (1.0 - np.exp(-time * 150.0)) * np.exp(-time * 7.8)
    return (metallic * envelope).astype(np.float32)


def kick() -> np.ndarray:
    seconds = 0.34
    frames = round(seconds * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    phase = 2.0 * math.pi * (45.0 * time + 48.0 * (1.0 - np.exp(-time * 24.0)) / 24.0)
    body = np.sin(phase) * np.exp(-time * 12.5)
    click = RNG.normal(0.0, 1.0, frames).astype(np.float32) * np.exp(-time * 90.0)
    return (body * 0.94 + click * 0.08).astype(np.float32)


def snare() -> np.ndarray:
    seconds = 0.24
    frames = round(seconds * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    noise = RNG.normal(0.0, 1.0, frames).astype(np.float32)
    noise = noise - np.concatenate(([0.0], noise[:-1])) * 0.72
    body = np.sin(2.0 * math.pi * 176.0 * time) * np.exp(-time * 22.0)
    return (noise * np.exp(-time * 19.0) * 0.31 + body * 0.42).astype(np.float32)


def hat(open_hat: bool = False) -> np.ndarray:
    seconds = 0.20 if open_hat else 0.07
    frames = round(seconds * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    noise = RNG.normal(0.0, 1.0, frames).astype(np.float32)
    high = noise - np.concatenate(([0.0], noise[:-1])) * 0.94
    decay = 18.0 if open_hat else 54.0
    return (high * np.exp(-time * decay) * 0.16).astype(np.float32)


def chain_tick(pitch: float = 1.0) -> np.ndarray:
    seconds = 0.095
    frames = round(seconds * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    signal = (
        np.sin(2.0 * math.pi * 1840.0 * pitch * time)
        + 0.55 * np.sin(2.0 * math.pi * 2910.0 * pitch * time)
        + 0.24 * np.sin(2.0 * math.pi * 4330.0 * pitch * time)
    )
    return (signal * np.exp(-time * 47.0) * 0.18).astype(np.float32)


def add_circular_delay(mix: np.ndarray, beats: float, gain: float, cross_pan: bool) -> None:
    delay = round(beats * FRAMES_PER_BEAT)
    delayed = np.roll(mix, delay, axis=0)
    if cross_pan:
        delayed = delayed[:, ::-1]
    mix += delayed * gain


def render() -> None:
    drums = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    harmony = np.zeros_like(drums)
    bass = np.zeros_like(drums)
    melody = np.zeros_like(drums)
    machinery = np.zeros_like(drums)

    chord_progression = [
        (52, 55, 59),  # E minor
        (48, 52, 55),  # C major
        (43, 47, 50),  # G major
        (50, 54, 57),  # D major
    ]
    bass_roots = [28, 24, 31, 26]
    bass_steps = [
        (0.00, 0),
        (1.50, 7),
        (2.50, 12),
        (3.25, 7),
    ]

    kick_sound = kick()
    snare_sound = snare()
    closed_hat = hat()
    open_hat = hat(True)

    for bar in range(LOOP_BARS):
        section = bar // 8
        chord = chord_progression[bar % 4]
        root = bass_roots[bar % 4]

        # Warm, breathing machinery-room harmony.
        for voice, note in enumerate(chord):
            pad = shaped_tone(midi_frequency(note), 2.05, 0.20, 0.42, 0.13)
            add_wrapped(harmony, pad, beat_frame(bar, 0.0), 0.052, (-0.42, 0.0, 0.42)[voice])

        for step, (beat, interval) in enumerate(bass_steps):
            if section == 0 and step == 2 and bar % 2:
                continue
            tone = shaped_tone(midi_frequency(root + interval), 0.34, 0.008, 0.09, 0.32)
            add_wrapped(bass, np.tanh(tone * 1.25), beat_frame(bar, beat), 0.20, 0.0)

        for beat in (0.0, 2.0):
            add_wrapped(drums, kick_sound, beat_frame(bar, beat), 0.46)
        if section >= 1 or bar % 2 == 0:
            add_wrapped(drums, kick_sound, beat_frame(bar, 2.75), 0.22)
        for beat in (1.0, 3.0):
            add_wrapped(drums, snare_sound, beat_frame(bar, beat), 0.30, 0.05)

        hat_spacing = 1.0 if section == 0 else 0.5
        for hat_index in range(round(4.0 / hat_spacing)):
            beat = hat_index * hat_spacing
            pan = -0.24 if hat_index % 2 == 0 else 0.24
            add_wrapped(drums, closed_hat, beat_frame(bar, beat), 0.32, pan)
        if section == 2 and bar % 2:
            add_wrapped(drums, open_hat, beat_frame(bar, 3.5), 0.36, 0.30)

        # Alternating chain links provide a quiet mechanical clock.
        for beat in (0.75, 1.75, 2.75, 3.75):
            pan = -0.55 if int(beat) % 2 == 0 else 0.55
            add_wrapped(
                machinery,
                chain_tick(0.96 + 0.025 * section),
                beat_frame(bar, beat),
                0.24 + section * 0.035,
                pan,
            )

        # Three rising notes are the level's Button 1 -> 2 -> 3 identity.
        if bar % 2 == 0:
            motif = (71, 76, 79) if section < 2 else (76, 79, 83)
            for index, note in enumerate(motif):
                add_wrapped(
                    melody,
                    pluck(midi_frequency(note)),
                    beat_frame(bar, 0.5 + index * 0.75),
                    0.16 + section * 0.025,
                    (-0.45, 0.0, 0.45)[index],
                )

        # A rolling three-hit fill announces each new eight-bar stage.
        if bar in (7, 15, 23):
            for index, note in enumerate((45, 43, 40)):
                tom = shaped_tone(midi_frequency(note), 0.30, 0.003, 0.17, 0.20)
                add_wrapped(
                    drums,
                    tom,
                    beat_frame(bar, 3.0 + index * 0.25),
                    0.23 + index * 0.04,
                    -0.32 + index * 0.32,
                )

    add_circular_delay(melody, 0.75, 0.18, True)
    add_circular_delay(machinery, 0.25, 0.10, True)

    mix = drums + harmony + bass + melody + machinery
    mix -= np.mean(mix, axis=0, keepdims=True)

    # Gentle bus saturation keeps impacts controlled while retaining dynamics.
    mix = np.tanh(mix * 1.18) / math.tanh(1.18)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.86:
        mix *= 0.86 / peak

    # Meet the final sample back to the first over a tiny inaudible window so
    # raylib can restart the stream without a waveform discontinuity click.
    seam_frames = round(0.012 * SAMPLE_RATE)
    seam_delta = mix[0] - mix[-1]
    seam_curve = 0.5 - 0.5 * np.cos(
        np.linspace(0.0, math.pi, seam_frames, dtype=np.float32)
    )
    mix[-seam_frames:] += seam_curve[:, None] * seam_delta
    peak = float(np.max(np.abs(mix)))
    if peak > 0.86:
        mix *= 0.86 / peak

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
