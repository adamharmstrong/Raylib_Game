#!/usr/bin/env python3
"""Render the Power Pulley Panic level-select theme."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

import numpy as np

from build_title_theme import PACK_ROOT, PROJECT_ROOT, SAMPLE_RATE


BPM = 128.0
LOOP_BARS = 24
FRAMES_PER_BAR = SAMPLE_RATE * 4.0 * 60.0 / BPM
FRAMES_PER_BEAT = FRAMES_PER_BAR / 4.0
LOOP_FRAMES = round(LOOP_BARS * FRAMES_PER_BAR)

ARP_PATH = (
    PACK_ROOT
    / "Synths"
    / "Arps"
    / "Clark Audio - MERCURY - Arp Loop 01 - 128BPM Ebmaj.wav"
)
HIHAT_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Hihat Loops"
    / "Clark Audio - MERCURY - HiHat Loop 01 - 120BPM.wav"
)
PERCUSSION_PATH = (
    PACK_ROOT
    / "Drum Loops"
    / "Percussion Loops"
    / "Clark Audio - MERCURY - Percussion Loop 02 - 120BPM.wav"
)
OUTPUT_PATH = (
    PACK_ROOT
    / "processed"
    / "power_pulley_panic_level_select_theme.wav"
)


def read_stereo_wav(path: Path) -> np.ndarray:
    """Read stereo PCM or 32-bit IEEE-float RIFF/WAV into float32 frames."""
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Unsupported WAV container: {path}")

    format_chunk = None
    audio_data = None
    offset = 12
    while offset + 8 <= len(data):
        chunk_id = data[offset : offset + 4]
        chunk_size = struct.unpack_from("<I", data, offset + 4)[0]
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_size
        if chunk_id == b"fmt ":
            format_chunk = data[chunk_start:chunk_end]
        elif chunk_id == b"data":
            audio_data = data[chunk_start:chunk_end]
        offset = chunk_end + (chunk_size & 1)

    if format_chunk is None or audio_data is None or len(format_chunk) < 16:
        raise ValueError(f"Missing WAV format or data chunk: {path}")

    format_tag, channels, _, _, _, bits = struct.unpack_from(
        "<HHIIHH", format_chunk
    )
    if channels != 2:
        raise ValueError(f"Expected stereo audio, got {channels} channels: {path}")

    if format_tag == 3 and bits == 32:
        samples = np.frombuffer(audio_data, dtype="<f4").astype(np.float32)
    elif format_tag == 1 and bits == 16:
        samples = np.frombuffer(audio_data, dtype="<i2").astype(np.float32)
        samples /= 32_768.0
    elif format_tag == 1 and bits == 24:
        bytes_24 = np.frombuffer(audio_data, dtype=np.uint8).reshape(-1, 3)
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
        raise ValueError(f"Unsupported WAV format {format_tag}/{bits}-bit: {path}")

    if not np.all(np.isfinite(samples)):
        raise ValueError(f"Non-finite samples in WAV: {path}")
    return samples.reshape(-1, channels)


def resample_to_frames(audio: np.ndarray, target_frames: int) -> np.ndarray:
    positions = np.arange(target_frames, dtype=np.float64) * len(audio) / target_frames
    left = np.floor(positions).astype(np.int64)
    right = np.minimum(left + 1, len(audio) - 1)
    amount = (positions - left).astype(np.float32)
    return audio[left] * (1.0 - amount[:, None]) + audio[right] * amount[:, None]


def prepared_loop(path: Path, source_bars: int) -> np.ndarray:
    phrase_frames = round(source_bars * FRAMES_PER_BAR)
    phrase = resample_to_frames(read_stereo_wav(path), phrase_frames)
    repeats = math.ceil(LOOP_FRAMES / phrase_frames)
    return np.tile(phrase, (repeats, 1))[:LOOP_FRAMES]


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


def synthesize_menu_beat() -> np.ndarray:
    beat = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)

    kick_frames = int(0.25 * SAMPLE_RATE)
    kick_time = np.arange(kick_frames, dtype=np.float32) / SAMPLE_RATE
    start_frequency = 96.0
    end_frequency = 45.0
    sweep = (end_frequency - start_frequency) / 0.25
    kick_phase = 2.0 * math.pi * (
        start_frequency * kick_time + 0.5 * sweep * kick_time * kick_time
    )
    kick = np.sin(kick_phase) * np.exp(-kick_time * 13.0) * 0.090
    kick += np.sin(2.0 * math.pi * 1120.0 * kick_time) * np.exp(-kick_time * 75.0) * 0.012

    rim_frames = int(0.075 * SAMPLE_RATE)
    rim_time = np.arange(rim_frames, dtype=np.float32) / SAMPLE_RATE
    rim = (
        np.sin(2.0 * math.pi * 1840.0 * rim_time)
        + 0.32 * np.sin(2.0 * math.pi * 2710.0 * rim_time)
    ) * np.exp(-rim_time * 58.0) * 0.027

    for bar in range(LOOP_BARS):
        for local_beat in (0.0, 2.0):
            start = round(bar * FRAMES_PER_BAR + local_beat * FRAMES_PER_BEAT)
            add_panned(beat, kick.astype(np.float32), start, 0.0)
        for local_beat, pan in ((1.0, -0.18), (3.0, 0.18)):
            start = round(bar * FRAMES_PER_BAR + local_beat * FRAMES_PER_BEAT)
            add_panned(beat, rim.astype(np.float32), start, pan)
    return beat


def synthesize_map_bass() -> np.ndarray:
    bass = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    progression = (
        (82.4069, 116.5409),
        (58.2705, 87.3071),
        (65.4064, 97.9989),
        (51.9131, 77.7817),
    )
    note_frames = int(0.30 * SAMPLE_RATE)
    time = np.arange(note_frames, dtype=np.float32) / SAMPLE_RATE
    for bar in range(LOOP_BARS):
        root, fifth = progression[bar % len(progression)]
        for index, (local_beat, frequency) in enumerate(
            ((0.0, root), (1.5, fifth), (2.0, root), (3.5, fifth))
        ):
            phase = 2.0 * math.pi * frequency * time
            tone = np.sin(phase) + 0.24 * np.sin(phase * 2.0)
            envelope = np.minimum(time / 0.008, 1.0) * np.exp(-time * 6.8)
            note = (tone * envelope * 0.036).astype(np.float32)
            start = round(bar * FRAMES_PER_BAR + local_beat * FRAMES_PER_BEAT)
            add_panned(bass, note, start, -0.05 if index % 2 == 0 else 0.05)
    return bass


def waypoint_note(frequency: float) -> np.ndarray:
    frames = int(0.32 * SAMPLE_RATE)
    time = np.arange(frames, dtype=np.float32) / SAMPLE_RATE
    phase = 2.0 * math.pi * frequency * time
    tone = (
        np.sin(phase)
        + 0.28 * np.sin(phase * 2.02)
        + 0.09 * np.sin(phase * 4.08)
    )
    envelope = np.minimum(time / 0.004, 1.0) * np.exp(-time * 10.0)
    return (tone * envelope * 0.029).astype(np.float32)


def synthesize_six_waypoint_hook() -> np.ndarray:
    hook = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    frequencies = (622.254, 783.991, 932.328, 1046.502, 932.328, 783.991)
    beat_offsets = (0.0, 0.5, 1.0, 1.5, 2.5, 3.0)
    pans = (-0.42, -0.25, -0.08, 0.10, 0.27, 0.44)
    for bar in range(0, LOOP_BARS, 4):
        for frequency, beat, pan in zip(frequencies, beat_offsets, pans):
            start = round(bar * FRAMES_PER_BAR + beat * FRAMES_PER_BEAT)
            add_panned(hook, waypoint_note(frequency), start, pan)
    return hook


def synthesize_map_pad() -> np.ndarray:
    pad = np.zeros((LOOP_FRAMES, 2), dtype=np.float32)
    chords = (
        (155.5635, 195.9977, 233.0819),
        (116.5409, 146.8324, 174.6141),
        (130.8128, 155.5635, 195.9977),
        (103.8262, 130.8128, 155.5635),
    )
    bar_frames = round(FRAMES_PER_BAR)
    time = np.arange(bar_frames, dtype=np.float32) / SAMPLE_RATE
    envelope = np.maximum(
        np.sin(np.linspace(0.0, math.pi, bar_frames, dtype=np.float32)),
        0.0,
    ) ** 0.35
    for bar in range(LOOP_BARS):
        chord = chords[bar % len(chords)]
        left = np.zeros(bar_frames, dtype=np.float32)
        right = np.zeros(bar_frames, dtype=np.float32)
        for index, frequency in enumerate(chord):
            voice = np.sin(2.0 * math.pi * frequency * time + index * 0.35)
            left += voice * (0.0046 if index != 1 else 0.0038)
            right += voice * (0.0046 if index != 0 else 0.0038)
        start = round(bar * FRAMES_PER_BAR)
        end = min(start + bar_frames, LOOP_FRAMES)
        pad[start:end, 0] += left[: end - start] * envelope[: end - start]
        pad[start:end, 1] += right[: end - start] * envelope[: end - start]
    return pad


def render() -> None:
    arp = prepared_loop(ARP_PATH, 8)
    hihat = prepared_loop(HIHAT_PATH, 8)
    percussion = prepared_loop(PERCUSSION_PATH, 16)
    menu_beat = synthesize_menu_beat()
    bass = synthesize_map_bass()
    waypoint_hook = synthesize_six_waypoint_hook()
    pad = synthesize_map_pad()

    arp_gain = section_gain(
        [(0, 0.69), (4, 0.74), (8, 0.61), (12, 0.47),
         (16, 0.73), (20, 0.80), (24, 0.69)]
    )
    hihat_gain = section_gain(
        [(0, 0.07), (4, 0.12), (8, 0.18), (12, 0.05),
         (16, 0.16), (20, 0.21), (24, 0.07)]
    )
    percussion_gain = section_gain(
        [(0, 0.14), (4, 0.18), (8, 0.22), (12, 0.09),
         (16, 0.23), (20, 0.27), (24, 0.14)]
    )
    bass_gain = section_gain(
        [(0, 0.92), (8, 1.0), (12, 0.68), (16, 1.02),
         (20, 1.08), (24, 0.92)]
    )
    pad_gain = section_gain(
        [(0, 0.74), (8, 0.88), (12, 1.16), (16, 0.92),
         (20, 0.84), (24, 0.74)]
    )

    melodic = arp * arp_gain[:, None]
    melodic += bass * bass_gain[:, None]
    melodic += waypoint_hook
    melodic += pad * pad_gain[:, None]

    beat_phase = (
        np.arange(LOOP_FRAMES, dtype=np.float32) % FRAMES_PER_BEAT
    ) / FRAMES_PER_BEAT
    duck = 0.86 + 0.14 * np.sin(math.pi * beat_phase) ** 0.72
    melodic *= duck[:, None]

    mix = melodic
    mix += menu_beat
    mix += hihat * hihat_gain[:, None]
    mix += percussion * percussion_gain[:, None]
    mix -= np.mean(mix, axis=0, keepdims=True)

    mid = np.mean(mix, axis=1)
    side = (mix[:, 0] - mix[:, 1]) * 0.5
    mix[:, 0] = mid + side * 0.90
    mix[:, 1] = mid - side * 0.90

    mix = np.tanh(mix * 1.75) / math.tanh(1.75)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.80:
        mix *= 0.80 / peak

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
