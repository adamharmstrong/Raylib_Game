#!/usr/bin/env python3
"""Render the Flooded Lower Works theme for Power Pulley Panic Level 3."""

from __future__ import annotations

import math
import wave

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE, read_pcm_wav


BPM = 122.0
LOOP_BARS = 32
PHRASE_BARS = 8

STARTER_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 04 122BPM Fmaj"
)
ARP_PATH = STARTER_ROOT / "Clark Audio - MERCURY - Song Starter 04 - Arp 122BPM Fmaj.wav"
BASS_PATH = STARTER_ROOT / "Clark Audio - MERCURY - Song Starter 04 - Bass 122BPM Fmaj.wav"
PIANO_PATH = STARTER_ROOT / "Clark Audio - MERCURY - Song Starter 04 - Piano 122BPM Fmaj.wav"
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "House"
    / "Clark Audio - MERCURY - House Drum Loop 02 - 115BPM.wav"
)
PERCUSSION_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Percussion Loops"
    / "Clark Audio - MERCURY - Percussion Loop 03 - 120BPM.wav"
)
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_level_03_flooded_lower_works.wav"

# The pack's 122 BPM stems contain the exact sample-aligned eight-bar phrase.
# Derive all longer timing from that phrase so repeated layers never drift.
PHRASE_FRAMES = 694_033
LOOP_FRAMES = PHRASE_FRAMES * (LOOP_BARS // PHRASE_BARS)
FRAMES_PER_BAR = PHRASE_FRAMES / PHRASE_BARS
FRAMES_PER_BEAT = PHRASE_FRAMES / (PHRASE_BARS * 4)


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def prepared_loop(path, source_bars: int, repeats: int) -> np.ndarray:
    target_frames = round(PHRASE_FRAMES * source_bars / PHRASE_BARS)
    source = read_pcm_wav(path)
    if len(source) != target_frames:
        source = resample_to_frames(source, target_frames)
    tiled = np.tile(source, (repeats, 1))
    if len(tiled) != LOOP_FRAMES:
        raise ValueError(f"Unexpected prepared length for {path.name}: {len(tiled)}")
    return tiled


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray([bar * FRAMES_PER_BAR for bar, _ in points], dtype=np.float32)
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def beat_to_frame(beat: float) -> int:
    return round(beat * FRAMES_PER_BEAT)


def synthesize_rising_water_motif() -> np.ndarray:
    """Create a six-droplet F-major figure that rises and travels left-to-right."""
    motif = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    relative_beats = [0.50, 1.25, 2.25, 3.50, 4.75, 6.25]
    frequencies = [698.46, 880.0, 1046.50, 880.0, 783.99, 1046.50]  # F5 A5 C6 A5 G5 C6
    pans = [-0.70, -0.42, -0.14, 0.14, 0.42, 0.70]
    hit_frames = int(0.18 * SAMPLE_RATE)
    time = np.arange(hit_frames, dtype=np.float32) / SAMPLE_RATE

    for phrase_bar in range(0, LOOP_BARS, 2):
        phrase_gain = 0.72 if 16 <= phrase_bar < 20 else 1.0
        for relative_beat, frequency, pan in zip(relative_beats, frequencies, pans):
            start = beat_to_frame(phrase_bar * 4.0 + relative_beat)
            end = min(start + hit_frames, LOOP_FRAMES)
            frames = end - start
            if frames <= 0:
                continue
            local_time = time[:frames]
            start_frequency = frequency * 0.72
            sweep = (frequency - start_frequency) / max(local_time[-1], 0.001)
            phase = 2.0 * math.pi * (
                start_frequency * local_time + 0.5 * sweep * local_time * local_time
            )
            droplet = (
                np.sin(phase) + 0.18 * np.sin(phase * 2.01)
            ) * np.exp(-local_time * 20.0) * 0.036 * phrase_gain
            attack = min(int(0.006 * SAMPLE_RATE), frames)
            droplet[:attack] *= np.linspace(0.0, 1.0, attack, dtype=np.float32)
            left_gain = math.sqrt((1.0 - pan) * 0.5)
            right_gain = math.sqrt((1.0 + pan) * 0.5)
            motif[start:end, 0] += droplet * left_gain
            motif[start:end, 1] += droplet * right_gain
    return motif


def synthesize_pressure_hum() -> np.ndarray:
    time = np.arange(LOOP_FRAMES, dtype=np.float32) / SAMPLE_RATE
    loop_seconds = LOOP_FRAMES / SAMPLE_RATE
    cycle = 2.0 * math.pi * time / loop_seconds
    pressure = 0.65 + 0.35 * np.sin(cycle * 4.0 - math.pi * 0.5) ** 2
    hum = (
        np.sin(2.0 * math.pi * 43.6535 * time)
        + 0.28 * np.sin(2.0 * math.pi * 65.4064 * time)
    ) * 0.032 * pressure
    stereo_motion = np.sin(cycle * 2.0) * 0.18
    result = np.empty((LOOP_FRAMES, 2), dtype=np.float32)
    result[:, 0] = hum * (1.0 + stereo_motion)
    result[:, 1] = hum * (1.0 - stereo_motion)
    return result


def render() -> None:
    arp = prepared_loop(ARP_PATH, 8, 4)
    bass = prepared_loop(BASS_PATH, 8, 4)
    piano = prepared_loop(PIANO_PATH, 8, 4)
    drums = prepared_loop(DRUM_PATH, 8, 4)
    percussion = prepared_loop(PERCUSSION_PATH, 16, 2)
    water_motif = synthesize_rising_water_motif()
    pressure_hum = synthesize_pressure_hum()

    arp_gain = section_gain(
        [(0, 1.28), (8, 1.62), (16, 1.86), (20, 1.52), (24, 1.90), (32, 1.28)]
    )
    bass_gain = section_gain(
        [(0, 0.42), (8, 0.50), (16, 0.25), (20, 0.38), (24, 0.54), (32, 0.42)]
    )
    piano_gain = section_gain(
        [(0, 0.62), (8, 0.70), (16, 0.78), (20, 0.66), (24, 0.75), (32, 0.62)]
    )
    drum_gain = section_gain(
        [(0, 0.49), (8, 0.55), (16, 0.25), (20, 0.42), (24, 0.60), (30, 0.52), (32, 0.49)]
    )
    percussion_gain = section_gain(
        [(0, 0.10), (8, 0.17), (16, 0.07), (20, 0.16), (24, 0.22), (32, 0.10)]
    )

    beat_phase = (np.arange(LOOP_FRAMES, dtype=np.float32) % FRAMES_PER_BEAT) / FRAMES_PER_BEAT
    pump = 0.58 + 0.42 * np.sin(math.pi * beat_phase) ** 0.75
    melodic = (
        arp * arp_gain[:, None]
        + bass * bass_gain[:, None]
        + piano * piano_gain[:, None]
    )
    melodic *= pump[:, None]

    mix = (
        melodic
        + drums * drum_gain[:, None]
        + percussion * percussion_gain[:, None]
        + water_motif
        + pressure_hum
    )

    # Preserve the flowing stereo motion while keeping the kick and bass stable.
    mix -= np.mean(mix, axis=0, keepdims=True)
    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 1.04
    mix[:, 1] = mid - side * 1.04

    mix = np.tanh(mix * 1.08) / math.tanh(1.08)
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
