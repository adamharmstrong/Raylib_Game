# Levels

Each `.level` file is a standalone text definition. It references only reusable
game systems and assets that already ship with the game; it does not embed art,
audio, or code.

Use `test_level.level` as the sandbox for trying newly added objects before
placing them in campaign levels. Use `tileset_reference.level` to validate
industrial tileset floor, wall, ceiling, and corner placement rules against the
how-to examples.

Read [INDUSTRIAL_TILESET_GUIDE.md](INDUSTRIAL_TILESET_GUIDE.md) before
authoring or rebuilding industrial tile artwork. It documents the three visual
layers, atlas coordinates, collision alignment, renderer-owned hazards, and
the mistakes that must not be reintroduced across campaign levels.

`gatehouse_generator_test.level` is an alternate copy of Level 1 that replaces
the decorative generator behavior with mechanical-to-electrical conversion and
a channel-powered gate motor. The completed `gatehouse.level` is unchanged.

`massive_test_level.level` is the full-catalog test facility. It uses a compact
3600x1800 world, eight labeled rooms, two camera-framed floors, and three ladder
shafts so objects can be tested in focused groups without long traversal times.

`spring_test_level.level` is the dedicated spring laboratory. Its three compact
galleries compare axial, rotary, flexural, frictional, fluid, and noncontact
spring behavior; hover any sample to identify it and read its purpose.

`gear_render_gallery.level` is a two-room visual and physics test gallery. Its
first screen shows all seven reusable gear types without overlap in large
vertical, large horizontal, and small horizontal rows. Crossing the right edge
opens a second fixed-camera screen containing all 22 spring objects in a regular
inspection grid.

Supported records:

- `script <power_pulley_panic|rotary_latch_lab|flooded_foundry|counterweight_row|button_sequence|portal_lift|neurotoxin_maze|clocktower_core|tileset_reference>`
- `bounds <x> <y> <width> <height>` (defaults to the 1600x900 virtual screen)
- `layer <background|middleground|foreground>` (sets the layer for following physics-object records; defaults to `middleground`)
- `cameraZone <x> <y> <width> <height>` (optional camera framing region)
- `playerStart <x> <y>`
- `clockFace <centerX> <centerY> <radius>`
- `label <x> <y> <text...>`
- `labelSized <x> <y> <fontSize> <text...>`
- `visualTile <farBackground|background|foreground> <sheetColumn> <sheetRow> <x> <y>`
- `visualTileRect <farBackground|background|foreground> <sheetColumn> <sheetRow> <x> <y> <columns> <rows>`
- `solid <x> <y> <width> <height>`
- `platform <x> <y> <width> <height>`
- `ladder <x> <y> <width> <height>`
- `spikeHazard <x> <y> <width> <height>`
- `spikePitTop <y>` (optional rendered/collision top edge for nonstandard pit heights)
- `valve <x> <y> <radius>`
- `waterPit <x> <y> <width> <height> <startSurfaceY> <targetSurfaceY> <fillRate>`
- `valveFluidFill <fluidIndex> <targetFill0..1> <riseRatePixelsPerSecond>`
- `toxinLeak <fluidIndex> <sourceX> <sourceY> <massPerSecond> <maximumMass> <exposureRate>`
- `water <x> <y> <width> <height> <cellSize> <initialFill0..1> <flowSpeed>`
- `sand <x> <y> <width> <height> <cellSize> <initialFill0..1> <flowSpeed>`
- `gel <x> <y> <width> <height> <particleSpacing> <initialFill0..1> <flowSpeed>`
- `gas <x> <y> <width> <height> <cellSize> <initialFill0..1> <flowSpeed>`
- `darkness <x> <y> <width> <height>` (repeatable)
- `exit <x> <y> <width> <height>`
- `pulley <x> <y>`
- `weight <pulleyIndex> <pulleyRadius> <phase> <speed> <width> <height>`
- `rotaryLatch <x> <y> <radius> <angle> <targetAngle> <tolerance> <spinSpeed>`
- `stoneBlock <x> <y> <width> <height> <mass>`
- `boulder <x> <y> <radius> <mass>`
- `physicsWheel <x> <y> <radius> <mass>`
- `gear <spur|lantern|ratchet|escape|bevel|sector|count> <dynamic|mounted> <vertical|horizontal> <x> <y> <radius> <mass> <teeth> <rotation> <initialAngularVelocity> <driveSpeed> [none|hour|minute|second]`
- `flywheel <x> <y> <radius> <mass> <initialAngularVelocity>`
- `steeringWheel <x> <y> <radius> <rotationDegrees>`
- `screw <centerX> <centerY> <length> <radius> <angleDegrees> <spinSpeed>`
- `fan <x> <y> <dirX> <dirY> <length> <width> <strength> <power>`
- `pinwheel <x> <y> <radius>`
- `ramp <centerX> <centerY> <length> <thickness> <angleDegrees> [segmentCount]`
- `trapDoor <hingeX> <hingeY> <length> <thickness> <angleDegrees>`
- `seeSaw <pivotX> <pivotY> <length> <thickness> <minAngle> <maxAngle> <response>`
- `chain <startX> <startY> <endX> <endY> <spacing> <scale> [pinStart] [pinEnd]`
- `physicsRope <startX> <startY> <endX> <endY> <length> <thickness> <pinStart> <pinEnd>`

Additional physics-guide records:

- `ball <centerX> <centerY> <radius> <mass>`
- `barrel|breakableCrate|explosiveBarrel <x> <y> <width> <height> <mass>`
- `movingPlatform|elevator|crushingBlock <x> <y> <width> <height> <dirX> <dirY> <travel> <speed>`
- `pendulumBob|swingingHammer <anchorX> <anchorY> <length> <radius> <phase> <speed>`
- `oneWayPlatform|guideRail <x> <y> <width> <height>`
- `ceilingHook|fixedJoint <x> <y> <radius>`
- `rod <startX> <startY> <endX> <endY> <thickness>`
- `spring|compressionSpring|extensionSpring|torsionSpring|garterSpring|voluteSpring|spiralSpring <startX> <startY> <endX> <endY> <thickness> [stiffness] [damping]`
- `constantForceSpring|constantTorqueSpring|leafSpring|beamSpring|discSpring|waveSpring|waveWasher <startX> <startY> <endX> <endY> <thickness> [stiffness] [damping]`
- `torsionBar|ringSpring|elastomerSpring|pneumaticSpring|gasSpring|hydropneumaticSpring|magneticSpring|compositeSpring <startX> <startY> <endX> <endY> <thickness> [stiffness] [damping]`
- `crank|ratchet <x> <y> <radius> <speed> [powerChannel]`
- `clutch <x> <y> <radius> <speed> <engaged0or1> [powerChannel]`
- `brake <x> <y> <width> <height> <strength0to1> [powerChannel]`
- `cam <x> <y> <radius> <speed> <eccentricity> [powerChannel]`
- `conveyorBelt <x> <y> <width> <height> <speed>`
- `turntable|electricMotor <x> <y> <radius> <speed> [powerChannel]`
- `generator <x> <y> <width> <height> <ratedShaftSpeed> <maximumOutput> <efficiency0to1> <powerChannel>`
- `battery <x> <y> <width> <height> <output> <powerChannel>`
- `limitSwitch|relay <x> <y> <width> <height> <powerChannel>`
- `fuse <x> <y> <width> <height> <capacity> <powerChannel>`
- `magnet <x> <y> <range> <strength> [powerChannel]`
- `piston|hydraulicCylinder <x> <y> <width> <height> <dirX> <dirY> <travel> <speed> [powerChannel]`
- `rocketThruster <x> <y> <dirX> <dirY> <length> <width> <strength> [powerChannel]`
- `sawBlade <x> <y> <radius> <dirX> <dirY> <travel> <speed>`
- `spinnerTrap <centerX> <centerY> <radius> <rotationDegreesPerSecond>`
- `steamVent <x> <y> <dirX> <dirY> <length> <width> <strength> <interval>`
- `electricalArc <startX> <startY> <endX> <endY> <interval> [powerChannel]`
- `oil|mud <x> <y> <width> <height>`
- `speedSensor <x> <y> <width> <height> <minimumSpeed> <powerChannel>`
- `beamSensor <x> <y> <dirX> <dirY> <length> <powerChannel>`
- `checkpoint <x> <y> <width> <height>`
- `collectible|key <centerX> <centerY> <radius>`

Objects on power channel `0` operate independently. Batteries, generators, and
active sensors energize matching nonzero channels. Generator output scales with
shaft speed and efficiency, while electrical demand produces mechanical load.
Dynamic guide objects collide with
the original movable bodies and can press existing buttons. Springs, rods, and
fixed joints attach to the nearest loose guide body at their attachment point.
All gear designs use the same physics object. Dynamic gears fall, roll, collide,
mesh with compatible gears, and engage screws. Mounted gears retain a fixed axle
position but have physical angular velocity and inertia, collide as anchored
bodies, and transfer torque through touching teeth. A nonzero `driveSpeed` acts
as a motor target; use `0` for an unpowered freely rotating gear. Gear orientation
is fixed by the level record and cannot be changed by players. Gears in the same
orientation mesh directly; a bevel gear allows torque transfer between vertical
and horizontal gear planes. A sector gear only meshes while its toothed arc faces
the contact. In `clocktower_core`, gears assigned to a clock hand can be stopped
and restarted to set that hand.

Legacy `gear <x> <y> <radius> <mass> [orientation]` and `mountedGear` records are
still accepted by the loader, but new levels should use the unified `gear` form.

Every level has three world layers. `background` and `foreground` bodies continue
to simulate gravity, motors, constraints, collisions, and same-layer object
interactions, but they never directly collide with or push the player. Only
`middleground` bodies occupy the player collision plane. The `layer` record is
stateful and applies to following `stoneBlock`, `boulder`, `physicsWheel`, `gear`,
`flywheel`, `screw`, and physics-guide object records until another `layer` record
changes it. Terrain remains shared by all three layers. Cross-layer gameplay is
implemented with non-solid triggers; for example, the clock-tower's background
hand gears expose middleground interaction proxies so players can operate their
brakes without colliding with the gear bodies.

World layers are separate from the layer token embedded in `visualTile` records;
tile layers select artwork sheets, while world layers control simulated object
depth, object-to-object contacts, rendering order, and player collision.

Pinned rope and chain endpoints can be picked up with a player's interact key. Releasing near an available anchor attaches the endpoint; releasing elsewhere drops it.

Spring objects attach their second endpoint to the nearest loose guide body on
the same world layer. Compression, extension, progressive, constant-output,
rotary, flexural, friction-damped, pneumatic, gas, hydropneumatic, magnetic, and
composite families each use a distinct response model. The optional stiffness
and damping values override each family's tuned defaults.
- `button <x> <y> <width> <height>`
- `buttonTrapDoor <buttonIndex> <trapDoorIndex> <openAngle> <speed>` (latched activation)
- `buttonLadder <buttonIndex> <x> <y> <width> <height>` (reveals and latches a ladder)
- `buttonExit <buttonIndex>` (opens and latches the exit)
- `portalPair <entranceX> <entranceY> <entranceWidth> <entranceHeight> <exitX> <exitY> <exitWidth> <exitHeight>` (one-way; preserves downward momentum)
- `buttonFan <buttonIndex> <fanIndex> <poweredAmount0..1>`
- `buttonPlatform <buttonIndex> <x> <y> <width> <height>`
- `buttonPlatformLoop <buttonIndex> <centerX> <centerY> <radiusX> <radiusY> <platformWidth> <platformHeight> <speedDegreesPerSecond> <platformCount>` (use a negative button index for an always-running rail)
- `platformLoopButton <loopIndex> <platformIndex> <width> <height>` (moves with its platform and latches when pressed)
- `directionalSpikeHazard <up|down|left|right> <x> <y> <width> <height>`
- `buttonSpikeHazard <buttonIndex> <up|down|left|right> <x> <y> <width> <height>`
- `arrowTrap <x> <y> <dirX> <dirY> <interval> <speed>`
- `breakableTile <x> <y> <width> <height> <breakDelaySeconds>`

`water` uses compressible cellular pressure to fall, seek a common level, form
thin streams, and transmit disturbances through a pool. `sand` uses discrete
gravity-driven grains with friction, diagonal falling, and surface avalanching
that settles near a dry-sand angle of repose. `cellSize` controls their pixel
resolution (1-12 pixels). `gel` combines particle pressure with persistent
viscoelastic bonds, so it deforms, wobbles, and recovers its shape. `gas` uses a
12-32 pixel concentration grid with buoyant advection and diffusion, producing a
continuous volume that remains strongly influenced by fans.
`flowSpeed` controls transfer or pressure response (0.1-4.0). Movable bodies,
players, enemies, chains, and physics ropes exchange forces with every fluid.
`valveFluidFill` connects the level valve to a cellular fluid record. Its rise
rate is resolution-independent, and filling stops at the configured fraction.
`toxinLeak` connects a valve-controlled hazard to a tile-density gas record. It
emits gas at the source until the valve is fully turned, tracks player exposure,
and uses the valve as an exit-door safety interlock in `neurotoxin_maze` levels.
The settings menu can switch between `Advanced` fluid simulation and `Simple`
fluid simulation. Advanced keeps the high-detail one-pixel cellular water/sand
and particle gel behavior. Gas remains tile-density based in both modes. Simple mode reinitializes cellular fluids onto
coarser six-pixel tiles and uses a simpler flow solver for lower-end hardware.

Physics collision is normally explicit geometry. The `tileset_reference` script
instead derives 32px collision cells from non-void foreground visual tiles so its
collision map stays aligned while the tileset rules are being audited.

Spike shafts, spike rows, and pit foundations remain renderer-driven. They are
shared hazard presentation, and the flooded pit must preserve its dynamic water
layering. Do not duplicate them with `visualTile` records unless the renderer is
changed at the same time.
