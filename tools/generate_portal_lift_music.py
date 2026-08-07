"""Render the original Portal Lift level theme as a loop-ready stereo WAV."""

from __future__ import annotations

import math
import pathlib
import struct
import wave


SAMPLE_RATE = 44_100
BPM = 112.0
BEAT_SECONDS = 60.0 / BPM
BAR_COUNT = 32
DURATION = BAR_COUNT * 4.0 * BEAT_SECONDS
TAU = math.tau

OUTPUT = (
    pathlib.Path(__file__).resolve().parents[1]
    / "assets"
    / "first_party"
    / "music"
    / "power_pulley_panic_wendi_02_portal_lift.wav"
)

# Dm - Bb - F - C | Dm - Gm - Bb - A, voiced as MIDI root notes.
CHORD_ROOTS = (38, 34, 41, 36, 38, 43, 34, 33)
CHORD_QUALITIES = ("minor", "major", "major", "major", "minor", "minor", "major", "major")
BASS_PATTERN = (0, 0, 7, 0, 12, 7, 3, 7)
LEAD_PATTERN = (12, 15, 17, 19, 17, 15, 22, 19, 17, 15, 12, 10, 12, 15, 17, 19)


def midi_frequency(note: int) -> float:
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def oscillator(phase: float, brightness: float = 0.25) -> float:
    """A softened industrial synth voice with controlled upper harmonics."""
    return (
        math.sin(phase)
        + brightness * math.sin(phase * 2.0)
        + brightness * 0.32 * math.sin(phase * 3.0)
    ) / (1.0 + brightness * 1.32)


def triangle(phase: float) -> float:
    return 2.0 * abs(2.0 * ((phase / TAU) % 1.0) - 1.0) - 1.0


def soft_clip(value: float) -> float:
    return math.tanh(value * 1.18) / math.tanh(1.18)


def render() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    frame_count = int(round(DURATION * SAMPLE_RATE))
    rng = 0x504F5254
    wind_state = 0.0
    metal_state = 0.0

    with wave.open(str(OUTPUT), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)

        chunk = bytearray()
        for sample_index in range(frame_count):
            time = sample_index / SAMPLE_RATE
            beat = time / BEAT_SECONDS
            bar = int(beat // 4.0) % BAR_COUNT
            beat_in_bar = beat % 4.0
            chord_index = (bar // 2) % len(CHORD_ROOTS)
            root = CHORD_ROOTS[chord_index]
            quality = CHORD_QUALITIES[chord_index]

            # Deterministic noise keeps the render reproducible.
            rng = (1664525 * rng + 1013904223) & 0xFFFFFFFF
            noise = ((rng >> 8) / 8_388_607.5) - 1.0
            wind_state += (noise - wind_state) * 0.015
            metal_state += (noise - metal_state) * 0.11

            # Slow machine-room bed and chord pad.
            third = 3 if quality == "minor" else 4
            pad_notes = (root + 12, root + 12 + third, root + 19)
            pad = 0.0
            for voice, note in enumerate(pad_notes):
                frequency = midi_frequency(note)
                pad += math.sin(TAU * frequency * time + voice * 1.7)
            pad *= 0.037
            pad += wind_state * (0.075 if 16 <= bar < 24 else 0.043)

            # Ratcheting eighth-note bass suggests the circulating rail.
            eighth = int(beat * 2.0)
            eighth_phase = (beat * 2.0) % 1.0
            bass_interval = BASS_PATTERN[eighth % len(BASS_PATTERN)]
            bass_frequency = midi_frequency(root + bass_interval)
            bass_envelope = math.exp(-eighth_phase * 3.2)
            bass = oscillator(TAU * bass_frequency * time, 0.16) * bass_envelope * 0.20

            # Portal motif: a glassy two-octave figure that opens up by section.
            lead_step = int(beat * 4.0)
            lead_phase = (beat * 4.0) % 1.0
            motif_interval = LEAD_PATTERN[lead_step % len(LEAD_PATTERN)]
            lead_note = root + motif_interval
            lead_frequency = midi_frequency(lead_note)
            lead_envelope = math.exp(-lead_phase * (5.8 if bar < 16 else 4.5))
            lead_gate = 0.0
            if 4 <= bar < 16:
                lead_gate = 0.12
            elif 16 <= bar < 24 and lead_step % 2 == 0:
                lead_gate = 0.095
            elif bar >= 24:
                lead_gate = 0.155
            lead = (
                math.sin(TAU * lead_frequency * time)
                + 0.34 * math.sin(TAU * lead_frequency * 2.01 * time)
            ) * lead_envelope * lead_gate

            # Four-on-the-floor machinery with a restrained puzzle-game pulse.
            kick_phase = beat % 1.0
            kick_frequency = 48.0 + 72.0 * math.exp(-kick_phase * 15.0)
            kick_level = 0.30 if bar >= 8 and not 16 <= bar < 20 else 0.20
            kick = math.sin(TAU * kick_frequency * (kick_phase * BEAT_SECONDS))
            kick *= math.exp(-kick_phase * 10.5) * kick_level

            snare_phase = (beat_in_bar - 1.0) % 2.0
            snare = 0.0
            if snare_phase < 0.42:
                snare_envelope = math.exp(-snare_phase * 14.0)
                snare = (
                    noise * 0.19
                    + math.sin(TAU * 174.0 * snare_phase * BEAT_SECONDS) * 0.08
                ) * snare_envelope

            hat_phase = (beat * 2.0) % 1.0
            hat = (noise - wind_state) * math.exp(-hat_phase * 24.0)
            hat *= 0.065 if bar >= 8 else 0.038

            # A pitched steel strike marks the mechanism's two-beat cycle.
            clang_phase = (beat % 2.0)
            clang = 0.0
            if clang_phase < 0.5:
                clang_envelope = math.exp(-clang_phase * 8.0)
                clang = (
                    triangle(TAU * 618.0 * time) * 0.032
                    + metal_state * 0.022
                ) * clang_envelope

            # A rising airy signal every eight bars evokes portal transport.
            phrase = beat % 32.0
            rise = 0.0
            if phrase >= 28.0:
                rise_progress = (phrase - 28.0) / 4.0
                rise_frequency = 310.0 + rise_progress * 520.0
                rise = (
                    math.sin(TAU * rise_frequency * time) * 0.025
                    + wind_state * 0.08
                ) * rise_progress

            mono = pad + bass + lead + kick + snare + hat + clang + rise

            # Modest stereo motion gives the portal lead width without making
            # positional gameplay sounds difficult to read.
            pan = math.sin(TAU * beat / 16.0) * 0.18
            left = mono - lead * pan + pad * 0.07
            right = mono + lead * pan - pad * 0.07

            # Short boundary fades prevent clicks when raylib loops the stream.
            edge_seconds = min(time, DURATION - time)
            edge_gain = min(1.0, max(0.0, edge_seconds / 0.025))
            left = soft_clip(left * 1.32) * edge_gain * 0.78
            right = soft_clip(right * 1.32) * edge_gain * 0.78

            left_sample = int(max(-1.0, min(1.0, left)) * 32767.0)
            right_sample = int(max(-1.0, min(1.0, right)) * 32767.0)
            chunk.extend(struct.pack("<hh", left_sample, right_sample))

            if len(chunk) >= 262_144:
                output.writeframesraw(chunk)
                chunk.clear()

        if chunk:
            output.writeframesraw(chunk)

    print(f"Rendered {OUTPUT}")
    print(f"{BAR_COUNT} bars, {BPM:g} BPM, {DURATION:.2f} seconds, {SAMPLE_RATE} Hz stereo")


if __name__ == "__main__":
    render()
