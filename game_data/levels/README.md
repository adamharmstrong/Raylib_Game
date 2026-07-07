# Levels

Each `.level` file is a standalone text definition. It references only reusable
game systems and assets that already ship with the game; it does not embed art,
audio, or code.

Use `test_level.level` as the sandbox for trying newly added objects before
placing them in campaign levels.

Supported records:

- `script <power_pulley_panic|rotary_latch_lab|flooded_foundry>`
- `solid <x> <y> <width> <height>`
- `platform <x> <y> <width> <height>`
- `ladder <x> <y> <width> <height>`
- `spikeHazard <x> <y> <width> <height>`
- `valve <x> <y> <radius>`
- `waterPit <x> <y> <width> <height> <startSurfaceY> <targetSurfaceY> <fillRate>`
- `darkness <x> <y> <width> <height>`
- `rightDarkness <x> <y> <width> <height>`
- `exit <x> <y> <width> <height>`
- `pulley <x> <y>`
- `weight <pulleyIndex> <pulleyRadius> <phase> <speed> <width> <height>`
- `rotaryLatch <x> <y> <radius> <angle> <targetAngle> <tolerance> <spinSpeed>`
- `stoneBlock <x> <y> <width> <height> <mass>`
- `seeSaw <pivotX> <pivotY> <length> <thickness> <minAngle> <maxAngle> <response>`
- `chain <startX> <startY> <endX> <endY> <spacing> <scale>`

Physics collision is explicit geometry, not inferred from visual art.
