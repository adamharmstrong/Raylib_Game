# Levels

Each `.level` file is a standalone text definition. It references only reusable
game systems and assets that already ship with the game; it does not embed art,
audio, or code.

Supported records:

- `script <power_pulley_panic|rotary_latch_lab>`
- `solid <x> <y> <width> <height>`
- `platform <x> <y> <width> <height>`
- `ladder <x> <y> <width> <height>`
- `spikeHazard <x> <y> <width> <height>`
- `darkness <x> <y> <width> <height>`
- `rightDarkness <x> <y> <width> <height>`
- `exit <x> <y> <width> <height>`
- `pulley <x> <y>`
- `weight <pulleyIndex> <pulleyRadius> <phase> <speed> <width> <height>`
- `rotaryLatch <x> <y> <radius> <angle> <targetAngle> <tolerance> <spinSpeed>`

Physics collision is explicit geometry, not inferred from visual art.
