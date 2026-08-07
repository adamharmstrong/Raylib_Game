#!/usr/bin/env python3
"""Render The Neurotoxin Annex theme for Power Pulley Panic Level 5."""

from __future__ import annotations

import math
import wave
from pathlib import Path

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE


BPM = 137.0
LOOP_BARS = 32
SOURCE_PHRASE_BARS = 8
SOURCE_PHRASE_FRAMES = round(
    SOURCE_PHRASE_BARS * 4.0 * 60.0 / BPM * SAMPLE_RATE
)
LOOP_FRAMES = SOURCE_PHRASE_FRAMES * (LOOP_BARS // SOURCE_PHRASE_BARS)
FRAMES_PER_BAR = SOURCE_PHRASE_FRAMES / SOURCE_PHRASE_BARS
FRAMES_PER_BEAT = FRAMES_PER_BAR / 4.0

ARP_PATH = (
    PACK_ROOT
    / "Synths"
    / "Arps"
    / "Clark Audio - MERCURY - Arp Loop 02 - 137BPM Fmin.wav"
)
DRUM_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Full Drum Loops"
    / "House"
    / "Clark Audio - MERCURY - House Drum Loop 01 - 136BPM.wav"
)
HIHAT_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Hihat Loops"
    / "Clark Audio - MERCURY - HiHat Loop 05 - 135BPM.wav"
)
OUTPUT_PATH = (
    PACK_ROOT
    / "processed"
    / "power_pulley_panic_level_05_neurotoxin_annex.wav"
)


def read_stereo_pcm_wav(path: Path) -> np.ndarray:
    """Read a stereo 16- or 24-bit PCM WAV at any sample rate."""
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        sample_width = source.getsampwidth()
        compression = source.getcomptype()
        frame_count = source.getnframes()
        raw = source.readframes(frame_count)

    if compression != "NONE":
        raise ValueError(f"Unsupported compressed WAV: {path}")
    if channels != 2:
        raise ValueError(f"Expected stereo audio, got {channels} channels: {path}")

    if sample_width == 2:
        samples = np.frombuffer(raw, dtype="<i2").astype(np.float32)
        samples /= 32_768.0
    elif sample_width == 3:
        bytes_24 = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        samples_32 = (
            bytes_24[:, 0].astype(np.int32)
            | (bytes_24[:, 1].astype(np.int32) << 8)
            | (bytes_24[:, 2].astype(np.int32) << 16)
        )
        samples_32 = np.where(
            samples_32 & 0x0080_0000,
            samples_32 - 0x0100_0000,
            samples_32,
        )
        samples = samples_32.astype(np.float32) / 8_388_608.0
    else:
        raise ValueError(f"Unsupported {sample_width * 8}-bit WAV: {path}")

    return samples.reshape(-1, channels)


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    """Linearly resample audio to a precise musical phrase length."""
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def looped_source(path: Path) -> np.ndarray:
    phrase = resample_to_frames(read_stereo_pcm_wav(path), SOURCE_PHRASE_FRAMES)
    return np.tile(phrase, (LOOP_BARS // SOURCE_PHRASE_BARS, 1))


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


def synthesize_machine_bass() -> np.ndarray:
    """Make the repeating eight-note F-minor machinery pulse."""
    bass = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    frequencies = (
        43.6535, 43.6535, 65.4064, 77.7817,
        43.6535, 51.9131, 65.4064, 77.7817,
    )
    event_frames = int(FRAMES_PER_BEAT * 0.47)
    time = np.arange(event_frames, dtype=np.float32) / SAMPLE_RATE
    for bar in range(LOOP_BARS):
        for step, frequency in enumerate(frequencies):
            phase = 2.0 * math.pi * frequency * time
            tone = (
                np.sin(phase)
                + 0.34 * np.sin(phase * 2.0)
                + 0.15 * np.sin(phase * 3.01)
            )
            envelope = np.minimum(time / 0.006, 1.0) * np.exp(-time * 7.0)
            accent = 1.18 if step in {0, 4} else 0.88
            note = (tone * envelope * 0.055 * accent).astype(np.float32)
            start = round(bar * FRAMES_PER_BAR + step * FRAMES_PER_BEAT * 0.5)
            pan = -0.08 if step % 2 == 0 else 0.08
            add_panned(bass, note, start, pan)
    return bass


def synthesize_warning_signal() -> np.ndarray:
    """Make the short three-tone alarm figure heard from the first bar."""
    signal = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    frequency_pattern = (698.456, 1046.502, 830.609, 1244.508, 1046.502, 830.609)
    beat_offsets = (0.25, 0.75, 1.25, 1.75, 2.75, 3.25)
    note_frames = int(0.145 * SAMPLE_RATE)
    time = np.arange(note_frames, dtype=np.float32) / SAMPLE_RATE

    for phrase_bar in range(0, LOOP_BARS, 2):
        for index, (frequency, beat) in enumerate(
            zip(frequency_pattern, beat_offsets)
        ):
            phase = 2.0 * math.pi * frequency * time
            tone = (
                np.sin(phase)
                + 0.22 * np.sin(phase * 1.997)
                + 0.10 * np.sin(phase * 3.03)
            )
            envelope = np.minimum(time / 0.004, 1.0) * np.exp(-time * 22.0)
            note = (tone * envelope * 0.037).astype(np.float32)
            start = round(phrase_bar * FRAMES_PER_BAR + beat * FRAMES_PER_BEAT)
            pan = (-0.38, 0.28, -0.16, 0.38, 0.16, -0.28)[index]
            add_panned(signal, note, start, pan)
    return signal


def synthesize_toxin_breath() -> np.ndarray:
    """Make deterministic filtered hiss and a low respirator-like swell."""
    rng = np.random.default_rng(5_051_905)
    noise = rng.standard_normal((LOOP_FRAMES, 2), dtype=np.float32)
    noise[1:] -= noise[:-1] * 0.985
    noise[0] = 0.0

    frame = np.arange(LOOP_FRAMES, dtype=np.float32)
    breath_phase = (frame % (FRAMES_PER_BAR * 2.0)) / (FRAMES_PER_BAR * 2.0)
    inhale = np.sin(math.pi * breath_phase) ** 2.3
    hiss_level = section_gain(
        [(0, 0.006), (8, 0.009), (16, 0.018), (20, 0.023),
         (24, 0.011), (28, 0.009), (32, 0.006)]
    )
    hiss = noise * (hiss_level * (0.28 + 0.72 * inhale))[:, None]

    time = frame / SAMPLE_RATE
    hum = (
        np.sin(2.0 * math.pi * 43.6535 * time)
        + 0.27 * np.sin(2.0 * math.pi * 65.4064 * time)
    )
    hum *= (0.008 + 0.012 * inhale) * section_gain(
        [(0, 0.55), (12, 0.68), (16, 0.92), (20, 1.0),
         (24, 0.72), (32, 0.55)]
    )
    breath = hiss
    breath[:, 0] += hum * 0.76
    breath[:, 1] += hum * 0.70
    return breath.astype(np.float32)


def render() -> None:
    arp = looped_source(ARP_PATH)
    drums = looped_source(DRUM_PATH)
    hihat = looped_source(HIHAT_PATH)
    bass = synthesize_machine_bass()
    warning = synthesize_warning_signal()
    breath = synthesize_toxin_breath()

    arp_gain = section_gain(
        [(0, 0.62), (8, 0.69), (14, 0.58), (16, 0.38),
         (20, 0.44), (24, 0.70), (28, 0.76), (32, 0.62)]
    )
    drum_gain = section_gain(
        [(0, 0.45), (8, 0.50), (14, 0.44), (16, 0.23),
         (20, 0.29), (24, 0.52), (28, 0.56), (32, 0.45)]
    )
    hihat_gain = section_gain(
        [(0, 0.07), (4, 0.14), (8, 0.23), (14, 0.19),
         (16, 0.05), (20, 0.11), (24, 0.25), (28, 0.30), (32, 0.07)]
    )
    bass_gain = section_gain(
        [(0, 0.92), (8, 1.0), (14, 0.88), (16, 0.52),
         (20, 0.66), (24, 1.04), (28, 1.10), (32, 0.92)]
    )
    warning_gain = section_gain(
        [(0, 1.0), (12, 1.08), (16, 0.72), (20, 0.90),
         (24, 1.12), (28, 1.18), (32, 1.0)]
    )

    melodic = arp * arp_gain[:, None]
    melodic += bass * bass_gain[:, None]
    melodic += warning * warning_gain[:, None]
    melodic += breath

    # A quick quarter-note recovery leaves the house kick audible through the
    # dense arp while giving the track a respirator-like pumping motion.
    beat_phase = (np.arange(LOOP_FRAMES, dtype=np.float32) % FRAMES_PER_BEAT) / FRAMES_PER_BEAT
    duck = 0.73 + 0.27 * np.sin(math.pi * beat_phase) ** 0.72
    melodic *= duck[:, None]

    mix = melodic + drums * drum_gain[:, None] + hihat * hihat_gain[:, None]
    mix -= np.mean(mix, axis=0, keepdims=True)

    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.90
    mix[:, 1] = mid - side * 0.90

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
