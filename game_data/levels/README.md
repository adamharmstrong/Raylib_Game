# Levels

Each `.level` file is a standalone text definition. It references only reusable
game systems and assets that already ship with the game; it does not embed art,
audio, or code.

Use `test_level.level` as the sandbox for trying newly added objects before
placing them in campaign levels. Use `tileset_reference.level` to validate
industrial tileset floor, wall, ceiling, and corner placement rules against the
how-to examples.

Supported records:

- `script <power_pulley_panic|rotary_latch_lab|flooded_foundry|counterweight_row|tileset_reference>`
- `playerStart <x> <y>`
- `visualTile <farBackground|background|foreground> <sheetColumn> <sheetRow> <x> <y>`
- `visualTileRect <farBackground|background|foreground> <sheetColumn> <sheetRow> <x> <y> <columns> <rows>`
- `solid <x> <y> <width> <height>`
- `platform <x> <y> <width> <height>`
- `ladder <x> <y> <width> <height>`
- `spikeHazard <x> <y> <width> <height>`
- `valve <x> <y> <radius>`
- `waterPit <x> <y> <width> <height> <startSurfaceY> <targetSurfaceY> <fillRate>`
- `valveFluidFill <fluidIndex> <targetFill0..1> <riseRatePixelsPerSecond>`
- `water <x> <y> <width> <height> <cellSize> <initialFill0..1> <flowSpeed>`
- `sand <x> <y> <width> <height> <cellSize> <initialFill0..1> <flowSpeed>`
- `gel <x> <y> <width> <height> <particleSpacing> <initialFill0..1> <flowSpeed>`
- `gas <x> <y> <width> <height> <particleSpacing> <initialFill0..1> <flowSpeed>`
- `darkness <x> <y> <width> <height>` (repeatable)
- `exit <x> <y> <width> <height>`
- `pulley <x> <y>`
- `weight <pulleyIndex> <pulleyRadius> <phase> <speed> <width> <height>`
- `rotaryLatch <x> <y> <radius> <angle> <targetAngle> <tolerance> <spinSpeed>`
- `stoneBlock <x> <y> <width> <height> <mass>`
- `boulder <x> <y> <radius> <mass>`
- `physicsWheel <x> <y> <radius> <mass>`
- `gear <x> <y> <radius> <mass>`
- `flywheel <x> <y> <radius> <mass> <initialAngularVelocity>`
- `steeringWheel <x> <y> <radius> <rotationDegrees>`
- `screw <centerX> <centerY> <length> <radius> <angleDegrees> <spinSpeed>`
- `fan <x> <y> <dirX> <dirY> <length> <width> <strength> <power>`
- `pinwheel <x> <y> <radius>`
- `ramp <centerX> <centerY> <length> <thickness> <angleDegrees>`
- `trapDoor <hingeX> <hingeY> <length> <thickness> <angleDegrees>`
- `seeSaw <pivotX> <pivotY> <length> <thickness> <minAngle> <maxAngle> <response>`
- `chain <startX> <startY> <endX> <endY> <spacing> <scale> [pinStart] [pinEnd]`
- `physicsRope <startX> <startY> <endX> <endY> <length> <thickness> <pinStart> <pinEnd>`

Pinned rope and chain endpoints can be picked up with a player's interact key. Releasing near an available anchor attaches the endpoint; releasing elsewhere drops it.
- `button <x> <y> <width> <height>`
- `arrowTrap <x> <y> <dirX> <dirY> <interval> <speed>`
- `breakableTile <x> <y> <width> <height> <breakDelaySeconds>`

`water` uses compressible cellular pressure to fall, seek a common level, form
thin streams, and transmit disturbances through a pool. `sand` uses discrete
gravity-driven grains with friction, diagonal falling, and surface avalanching
that settles near a dry-sand angle of repose. `cellSize` controls their pixel
resolution (1-12 pixels). `gel` combines particle pressure with persistent
viscoelastic bonds, so it deforms, wobbles, and recovers its shape. `gas` uses
buoyancy, low viscosity, turbulence, and neighbor pressure to expand and diffuse,
while remaining strongly advected by fans.
`flowSpeed` controls transfer or pressure response (0.1-4.0). Movable bodies,
players, enemies, chains, and physics ropes exchange forces with every fluid.
`valveFluidFill` connects the level valve to a cellular fluid record. Its rise
rate is resolution-independent, and filling stops at the configured fraction.
The settings menu can switch between `Advanced` fluid simulation and `Simple`
fluid simulation. Advanced keeps the high-detail one-pixel cellular water/sand
and particle gel/gas behavior. Simple mode reinitializes cellular fluids onto
coarser six-pixel tiles and uses a simpler flow solver for lower-end hardware.

Physics collision is normally explicit geometry. The `tileset_reference` script
instead derives 32px collision cells from non-void foreground visual tiles so its
collision map stays aligned while the tileset rules are being audited.

Spike shafts, spike rows, and pit foundations remain renderer-driven. They are
shared hazard presentation, and the flooded pit must preserve its dynamic water
layering. Do not duplicate them with `visualTile` records unless the renderer is
changed at the same time.
