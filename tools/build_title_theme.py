#!/usr/bin/env python3
"""Render the original long-form theme from licensed Mercury stems.

The source files are 24-bit, 44.1 kHz stereo PCM WAVs. The rendered runtime
asset is 16-bit stereo PCM so raylib can stream it without another codec.
"""

from __future__ import annotations

import argparse
import math
import wave
from pathlib import Path

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACK_ROOT = (
    PROJECT_ROOT
    / "assets"
    / "third_party"
    / "Clark Audio"
    / "Clark Audio - MERCURY Beta"
)
STEM_ROOT = (
    PACK_ROOT
    / "Song Starters With Stems"
    / "Stems"
    / "Song Starter 01 120BPM Gbm"
)
OUTPUT_PATH = PACK_ROOT / "processed" / "power_pulley_panic_level_theme_01.wav"

SAMPLE_RATE = 44_100
CHANNELS = 2
SOURCE_DURATION_SECONDS = 64.0

STEMS = {
    "drone": "Clark Audio - MERCURY - Song Starter 01 -  Drone Atmos 120BPM Gbmin.wav",
    "choir": "Clark Audio - MERCURY - Song Starter 01 -  Choir 120BPM Gbmin.wav",
    "keys": "Clark Audio - MERCURY - Song Starter 01 -  Keys 120BPM Gbmin.wav",
    "melody": "Clark Audio - MERCURY - Song Starter 01 -  Melody Dry 120BPM Gbmin.wav",
    "bass_sub": "Clark Audio - MERCURY - Song Starter 01 -  Bass Sub 120BPM Gbmin.wav",
    "bass_top": "Clark Audio - MERCURY - Song Starter 01 -  Bass Top 120BPM Gbmin.wav",
    "percussion": "Clark Audio - MERCURY - Song Starter 01 -  Percussion 120BPM Gbmin.wav",
}

# Gain automation is expressed at musical section boundaries. At 120 BPM each
# four-second span is two bars. Matching gains at 0 and 64 seconds lets the
# source arrangement circle back into its opening texture. This mix is now kept
# as a level-theme candidate; the hook-first title mix has a separate builder.
AUTOMATION = {
    "drone": [
        (0, 0.62), (8, 0.68), (16, 0.74), (28, 0.72),
        (32, 0.80), (40, 0.72), (52, 0.68), (60, 0.62), (64, 0.62),
    ],
    "choir": [
        (0, 0.34), (8, 0.42), (16, 0.46), (28, 0.50),
        (32, 0.60), (40, 0.42), (52, 0.50), (60, 0.36), (64, 0.34),
    ],
    "keys": [
        (0, 0.22), (8, 0.34), (16, 0.42), (28, 0.38),
        (32, 0.28), (40, 0.40), (52, 0.44), (60, 0.26), (64, 0.22),
    ],
    "melody": [
        (0, 0.00), (8, 0.00), (12, 0.16), (24, 0.25),
        (28, 0.00), (36, 0.00), (40, 0.20), (52, 0.28), (60, 0.00), (64, 0.00),
    ],
    "bass_sub": [
        (0, 0.00), (8, 0.00), (12, 0.18), (24, 0.24),
        (28, 0.00), (36, 0.00), (40, 0.20), (56, 0.25), (60, 0.00), (64, 0.00),
    ],
    "bass_top": [
        (0, 0.00), (12, 0.00), (16, 0.12), (24, 0.15),
        (28, 0.00), (40, 0.00), (44, 0.13), (56, 0.16), (60, 0.00), (64, 0.00),
    ],
    "percussion": [
        (0, 0.00), (8, 0.00), (12, 0.16), (16, 0.32),
        (24, 0.38), (28, 0.00), (36, 0.00), (40, 0.18),
        (44, 0.38), (56, 0.42), (60, 0.00), (64, 0.00),
    ],
}


def read_pcm_wav(path: Path) -> np.ndarray:
    """Read 16- or 24-bit PCM WAV into float32 frames by channel."""
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        sample_rate = source.getframerate()
        sample_width = source.getsampwidth()
        frame_count = source.getnframes()
        compression = source.getcomptype()
        raw = source.readframes(frame_count)

    if compression != "NONE":
        raise ValueError(f"Unsupported compressed WAV: {path}")
    if channels != CHANNELS or sample_rate != SAMPLE_RATE:
        raise ValueError(
            f"Expected {CHANNELS} channels at {SAMPLE_RATE} Hz, got "
            f"{channels} channels at {sample_rate} Hz: {path}"
        )

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


def gain_envelope(points: list[tuple[float, float]], frame_count: int) -> np.ndarray:
    times = np.arange(frame_count, dtype=np.float32) / SAMPLE_RATE
    point_times = np.asarray([point[0] for point in points], dtype=np.float32)
    point_gains = np.asarray([point[1] for point in points], dtype=np.float32)
    return np.interp(times, point_times, point_gains).astype(np.float32)


def section_rms(audio: np.ndarray, seconds: float = 4.0) -> list[float]:
    section_frames = int(seconds * SAMPLE_RATE)
    values: list[float] = []
    for start in range(0, len(audio), section_frames):
        section = audio[start : start + section_frames]
        values.append(float(np.sqrt(np.mean(np.square(section), dtype=np.float64))))
    return values


def arrange_stem(name: str, audio: np.ndarray) -> np.ndarray:
    """Fill deliberately sparse starter sections into the title arrangement."""
    half = int(32.0 * SAMPLE_RATE)
    quarter = int(16.0 * SAMPLE_RATE)
    arranged = audio.copy()

    if name == "drone":
        # The source drone occupies the first 16 bars. Repeat it under the
        # busier second half so the fortress atmosphere never disappears.
        arranged[half:] = audio[:half]
    elif name == "choir":
        # Reprise the four-bar choir phrase in the final build.
        arranged[3 * quarter : 4 * quarter] = audio[quarter : 2 * quarter]
    elif name == "keys":
        # Introduce the closing keys phrase once in the quieter first build.
        arranged[quarter : 2 * quarter] = audio[3 * quarter : 4 * quarter]

    return arranged


def print_analysis() -> None:
    print("Source stem RMS by four-second section:")
    for name, filename in STEMS.items():
        audio = read_pcm_wav(STEM_ROOT / filename)
        rms = " ".join(f"{value:.3f}" for value in section_rms(audio))
        print(f"  {name:10s} peak={np.max(np.abs(audio)):.3f} rms=[{rms}]")


def render(output_path: Path) -> None:
    expected_frames = int(SOURCE_DURATION_SECONDS * SAMPLE_RATE)
    mix = np.zeros((expected_frames, CHANNELS), dtype=np.float32)

    for name, filename in STEMS.items():
        stem_path = STEM_ROOT / filename
        audio = arrange_stem(name, read_pcm_wav(stem_path))
        if len(audio) != expected_frames:
            raise ValueError(
                f"Expected {expected_frames} frames, got {len(audio)}: {stem_path}"
            )

        envelope = gain_envelope(AUTOMATION[name], expected_frames)
        if name == "percussion":
            # Slightly narrow the drums so menu text and title effects retain a
            # broad stereo bed without the beat pulling to the sides.
            mono = np.mean(audio, axis=1)
            audio = audio * 0.72 + mono[:, None] * 0.28
        elif name == "drone":
            # Widen the drone by gently reducing its center component.
            mid = np.mean(audio, axis=1)
            side = (audio[:, 0] - audio[:, 1]) * 0.5
            audio[:, 0] = mid + side * 1.12
            audio[:, 1] = mid - side * 1.12

        mix += audio * envelope[:, None]

    # Remove any source DC bias, then use subtle saturation as a soft bus
    # compressor. A final peak ceiling leaves headroom for menu sound effects.
    mix -= np.mean(mix, axis=0, keepdims=True)
    mix = np.tanh(mix * 1.08) / math.tanh(1.08)
    peak = float(np.max(np.abs(mix)))
    if peak > 0.82:
        mix *= 0.82 / peak

    # Short equal-power fades prevent a click at playback start/stop. The source
    # stems and automation remain aligned to the full 32-bar loop.
    fade_frames = int(0.015 * SAMPLE_RATE)
    fade = np.sin(np.linspace(0.0, math.pi * 0.5, fade_frames, dtype=np.float32))
    mix[:fade_frames] *= fade[:, None]
    mix[-fade_frames:] *= fade[::-1, None]

    pcm = np.clip(np.rint(mix * 32_767.0), -32_768, 32_767).astype("<i2")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output_path), "wb") as destination:
        destination.setnchannels(CHANNELS)
        destination.setsampwidth(2)
        destination.setframerate(SAMPLE_RATE)
        destination.writeframes(pcm.tobytes())

    rms = float(np.sqrt(np.mean(np.square(mix), dtype=np.float64)))
    size_mib = output_path.stat().st_size / (1024 * 1024)
    print(
        f"Rendered {output_path.relative_to(PROJECT_ROOT)}\n"
        f"  duration={SOURCE_DURATION_SECONDS:.1f}s sample_rate={SAMPLE_RATE} "
        f"channels={CHANNELS} peak={np.max(np.abs(mix)):.3f} rms={rms:.3f} "
        f"size={size_mib:.1f} MiB"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analyze", action="store_true", help="print source stem levels")
    parser.add_argument("--output", type=Path, default=OUTPUT_PATH, help="render destination")
    args = parser.parse_args()

    if args.analyze:
        print_analysis()
    render(args.output.resolve())


if __name__ == "__main__":
    main()
