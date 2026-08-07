#!/usr/bin/env python3
"""Render the Gatehouse Generator theme for Power Pulley Panic Level 1."""

from __future__ import annotations

import math
import wave

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE, read_pcm_wav


BPM = 135.0
BEAT_FRAMES = int(SAMPLE_RATE * 60.0 / BPM)
BAR_FRAMES = BEAT_FRAMES * 4
LOOP_BARS = 32
LOOP_FRAMES = BAR_FRAMES * LOOP_BARS

CHORD_PATH = (
    PACK_ROOT
    / "Synths"
    / "Chords"
    / "Clark Audio - MERCURY - Chord Loop 01 - 135BPM Gmin.wav"
)
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "Garage"
    / "Clark Audio - MERCURY - Garage Drum Loop 03 - 135BPM.wav"
)
HIHAT_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Hihat Loops"
    / "Clark Audio - MERCURY - HiHat Loop 04 - 135BPM.wav"
)
STARTER_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 03 140BPM Gmin"
)
STAB_PATH = STARTER_ROOT / "Clark Audio - MERCURY - Song Starter 03 - Stab 140BPM Gmin.wav"
GROWL_PATH = STARTER_ROOT / "Clark Audio - MERCURY - Song Starter 03 - Growl 140BPM Gmin.wav"
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_level_01_gatehouse.wav"


def tiled_exact(path, repeats: int, expected_frames: int) -> np.ndarray:
    source = read_pcm_wav(path)
    tiled = np.tile(source, (repeats, 1))
    if len(tiled) != expected_frames:
        raise ValueError(
            f"Unexpected loop length for {path.name}: {len(tiled)} frames, "
            f"expected {expected_frames}"
        )
    return tiled


def section_gain(points: list[tuple[float, float]]) -> np.ndarray:
    frames = np.arange(LOOP_FRAMES, dtype=np.float32)
    point_frames = np.asarray([bar * BAR_FRAMES for bar, _ in points], dtype=np.float32)
    point_gains = np.asarray([gain for _, gain in points], dtype=np.float32)
    return np.interp(frames, point_frames, point_gains).astype(np.float32)


def synthesize_bass() -> np.ndarray:
    """Build the track's G-minor gear-cycle bass motif."""
    bass = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    notes = {
        "G1": 49.0,
        "Bb1": 58.2705,
        "D2": 73.4162,
        "Eb2": 77.7817,
    }

    for bar in range(LOOP_BARS):
        last_note = "Bb1" if bar % 4 == 3 else "D2"
        pattern = [
            (0.00, 0.72, "G1", 0.30),
            (1.50, 0.34, "D2", 0.19),
            (2.50, 0.36, "Eb2", 0.20),
            (3.25, 0.56, last_note, 0.22),
        ]
        for beat, duration_beats, note, amplitude in pattern:
            start = int((bar * 4.0 + beat) * BEAT_FRAMES)
            frames = min(int(duration_beats * BEAT_FRAMES), LOOP_FRAMES - start)
            if frames <= 0:
                continue

            time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
            frequency = notes[note]
            oscillator = (
                np.sin(2.0 * math.pi * frequency * time)
                + 0.22 * np.sin(4.0 * math.pi * frequency * time)
                + 0.07 * np.sin(6.0 * math.pi * frequency * time)
            )
            attack = min(int(0.012 * SAMPLE_RATE), frames)
            release = min(int(0.075 * SAMPLE_RATE), frames)
            envelope = np.ones(frames, dtype=np.float32)
            if attack > 0:
                envelope[:attack] = np.linspace(0.0, 1.0, attack, dtype=np.float32)
            if release > 0:
                envelope[-release:] *= np.linspace(1.0, 0.0, release, dtype=np.float32)
            note_audio = np.tanh(oscillator * 0.9) * envelope * amplitude
            bass[start : start + frames, 0] += note_audio
            bass[start : start + frames, 1] += note_audio

    return bass


def extract_accent(path, beats: int) -> np.ndarray:
    source = read_pcm_wav(path)
    source_beat_frames = int(SAMPLE_RATE * 60.0 / 140.0)
    active_phrase_start = source_beat_frames * 4 * 16
    accent = source[
        active_phrase_start : active_phrase_start + source_beat_frames * beats
    ].copy()
    fade_frames = min(int(0.025 * SAMPLE_RATE), len(accent))
    accent[-fade_frames:] *= np.linspace(1.0, 0.0, fade_frames, dtype=np.float32)[:, None]
    return accent


def place_accent(
    destination: np.ndarray,
    accent: np.ndarray,
    bar: int,
    beat: float,
    gain: float,
    pan: float = 0.0,
) -> None:
    start = int((bar * 4.0 + beat) * BEAT_FRAMES)
    end = min(start + len(accent), LOOP_FRAMES)
    if end <= start:
        return
    left_gain = gain * math.sqrt((1.0 - pan) * 0.5)
    right_gain = gain * math.sqrt((1.0 + pan) * 0.5)
    destination[start:end, 0] += accent[: end - start, 0] * left_gain
    destination[start:end, 1] += accent[: end - start, 1] * right_gain


def render() -> None:
    chords = tiled_exact(CHORD_PATH, 4, LOOP_FRAMES)
    drums = tiled_exact(DRUM_PATH, 2, LOOP_FRAMES)
    hihats = tiled_exact(HIHAT_PATH, 4, LOOP_FRAMES)
    bass = synthesize_bass()

    # Sidechain-style motion turns the sustained G-minor chord into a repeating
    # mechanical breath without borrowing the title theme's lead melody.
    beat_phase = (np.arange(LOOP_FRAMES, dtype=np.float32) % BEAT_FRAMES) / BEAT_FRAMES
    chord_pump = 0.54 + 0.46 * np.sin(math.pi * beat_phase) ** 0.7

    chord_gain = section_gain(
        [(0, 0.43), (8, 0.48), (16, 0.54), (20, 0.46), (24, 0.50), (32, 0.43)]
    )
    drum_gain = section_gain(
        [(0, 0.55), (8, 0.61), (16, 0.34), (18, 0.42), (20, 0.54), (24, 0.64), (32, 0.55)]
    )
    hihat_gain = section_gain(
        [(0, 0.12), (4, 0.18), (8, 0.24), (16, 0.07), (20, 0.17), (24, 0.28), (32, 0.12)]
    )
    bass_gain = section_gain(
        [(0, 0.88), (8, 1.00), (16, 0.58), (20, 0.78), (24, 1.06), (32, 0.88)]
    )

    mix = (
        chords * (chord_gain * chord_pump)[:, None]
        + drums * drum_gain[:, None]
        + hihats * hihat_gain[:, None]
        + bass * bass_gain[:, None]
    )

    accents = np.zeros_like(mix)
    stab = extract_accent(STAB_PATH, 1)
    growl = extract_accent(GROWL_PATH, 2)

    # A three-hit "pulley tooth" figure is the track's recognizable motif.
    for phrase in range(0, LOOP_BARS, 4):
        phrase_gain = 0.26 if phrase < 16 else 0.31
        place_accent(accents, stab, phrase, 1.50, phrase_gain, -0.28)
        place_accent(accents, stab, phrase, 2.75, phrase_gain * 0.86, 0.28)
        place_accent(accents, stab, phrase + 1, 0.50, phrase_gain * 0.94, 0.0)

    for bar in (7, 15, 23, 31):
        place_accent(accents, growl, bar, 2.0, 0.16, 0.0)

    mix += accents
    mix -= np.mean(mix, axis=0, keepdims=True)

    # Slightly rein in stereo width for a centered, machinery-room presentation.
    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.88
    mix[:, 1] = mid - side * 0.88

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
