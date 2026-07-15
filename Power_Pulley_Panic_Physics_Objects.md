# Power Pulley Panic — Physics Object Design Guide

## Purpose

This document defines the physics-related objects that could appear in **Power Pulley Panic**, explains what each object contributes to the game, and recommends how each one should be implemented in a Raylib-based C++ project.

The game should prioritize physics that are:

- Predictable enough for puzzle solving
- Easy for the player to read visually
- Stable at common frame rates
- Flexible enough to support new puzzle combinations
- Fun rather than perfectly realistic

A component-based architecture is recommended. Most gameplay objects should be assembled from reusable systems such as rigid bodies, colliders, constraints, sensors, motors, and power connections rather than being implemented as completely separate physics classes.

---

# 1. Core Architecture

## 1.1 Recommended Components

### `Transform2D`

Stores an object's position, rotation, and scale.

Suggested data:

```cpp
struct Transform2D
{
    Vector2 position;
    float rotation;
    Vector2 scale;
};
```

Use this component for every visible object.

---

### `RigidBody2D`

Stores physical motion data.

Suggested data:

```cpp
enum class BodyType
{
    Static,
    Kinematic,
    Dynamic
};

struct RigidBody2D
{
    BodyType type;

    Vector2 velocity;
    Vector2 accumulatedForce;

    float angularVelocity;
    float accumulatedTorque;

    float mass;
    float inverseMass;
    float momentOfInertia;
    float inverseInertia;

    float linearDamping;
    float angularDamping;

    bool affectedByGravity;
    bool sleeping;
};
```

Use:

- **Static bodies** for floors, walls, hooks, and fixed machinery.
- **Kinematic bodies** for scripted platforms, doors, and mechanisms.
- **Dynamic bodies** for crates, weights, barrels, and loose machinery.

---

### `Collider2D`

Defines the physical collision shape.

Recommended first shapes:

- Axis-aligned rectangle
- Oriented rectangle
- Circle
- Line segment
- Capsule

Avoid complex polygon collisions early in development. Approximate detailed artwork with several simple colliders.

Suggested data:

```cpp
enum class ColliderShape
{
    Rectangle,
    Circle,
    Capsule,
    Segment
};

struct Collider2D
{
    ColliderShape shape;
    Vector2 offset;
    Vector2 size;
    float radius;

    float friction;
    float restitution;

    bool isTrigger;
    unsigned int collisionLayer;
    unsigned int collisionMask;
};
```

---

### `Constraint2D`

Maintains a physical relationship between two bodies.

Useful constraint types:

- Rope
- Distance
- Spring
- Hinge
- Slider
- Fixed
- Gear
- Pulley

Constraints should be solved repeatedly during each physics update to reduce stretching and instability.

---

### `SensorComponent`

Detects conditions without necessarily blocking movement.

Examples:

- A crate is resting on a pressure plate.
- A pulley is rotating fast enough.
- A counterweight reached the bottom.
- A player entered a trigger region.
- A generator has produced enough energy.

---

### `PowerComponent`

Represents gameplay power rather than full electrical simulation.

Suggested values:

```cpp
struct PowerComponent
{
    float currentPower;
    float requiredPower;
    float maximumPower;
    bool powered;
};
```

Use a simplified signal-and-capacity model instead of simulating voltage, amperage, and resistance unless those systems become central to the puzzles.

---

# 2. Core Physics Bodies

## 2.1 Player

### Gameplay role

The player is the primary force that interacts with machinery, crates, levers, ropes, and switches.

### Recommended behavior

Use a **custom character controller**, not a fully free rigid body.

The player should:

- Walk and jump reliably
- Push and pull designated objects
- Grab ropes or handles
- Climb ladders
- Stand on moving platforms
- Avoid falling over or rotating uncontrollably

### Best implementation

Use:

- A capsule or rectangle collider
- Manually controlled horizontal movement
- Gravity and vertical velocity
- Ground detection using collision normals or small downward probes
- Separate interaction logic for pushing and pulling
- Limited or disabled rotation

The player should transfer force to objects, but the character controller should remain responsive even when the surrounding physics becomes chaotic.

### Priority

**Essential**

---

## 2.2 Crate

### Gameplay role

Crates are the most versatile movable puzzle objects.

They can:

- Hold down pressure plates
- Block hazards
- Become counterweights
- Create steps
- Interrupt beams
- Be attached to ropes
- Be pushed onto conveyors

### Best implementation

Use a dynamic rectangular rigid body.

Important properties:

- Moderate friction
- Very little bounce
- Stable rotation
- Optional grab points
- Optional maximum push speed

Create multiple crate variants by changing mass rather than creating entirely different physics code.

Examples:

- Light wooden crate
- Heavy metal crate
- Fragile crate
- Magnetic crate

### Priority

**Essential**

---

## 2.3 Counterweight

### Gameplay role

Counterweights power pulley puzzles and create balanced or unbalanced mechanical systems.

They can:

- Lift platforms
- Open gates
- Pull ropes
- Store gravitational potential energy
- Drop to power generators
- Create timed puzzle sequences

### Best implementation

Use a dynamic rigid body attached to:

- A rope constraint
- A pulley constraint
- A guide rail or slider constraint when lateral movement is undesirable

Counterweights should usually move along predictable vertical paths.

Add minimum and maximum travel distances to prevent ropes or mechanisms from moving outside the intended puzzle area.

### Priority

**Essential**

---

## 2.4 Ball

### Gameplay role

Balls introduce rolling momentum and can create timing puzzles.

They can:

- Roll down slopes
- Trigger switches
- Transfer momentum
- Enter sockets
- Move through pipes
- Act as cannon or launcher ammunition

### Best implementation

Use a circular collider with dynamic linear and angular movement.

Important properties:

- Radius
- Mass
- Friction
- Rolling resistance
- Restitution

Use restrained bounce unless the puzzle specifically depends on ricochets.

### Priority

**Useful**

---

## 2.5 Boulder

### Gameplay role

Boulders are heavy rolling hazards and large momentum-based puzzle objects.

They can:

- Break weak barriers
- Chase the player
- Power impact switches
- Turn machinery
- Crush obstacles

### Best implementation

Use the ball system with:

- Greater mass
- Lower acceleration from player contact
- Stronger collision impulse
- Lower bounce
- Optional damage component

Boulders should be visually and mechanically distinct from ordinary balls.

### Priority

**Useful**

---

## 2.6 Barrel

### Gameplay role

Barrels combine rolling behavior with cargo or hazard properties.

Possible variants:

- Empty barrel
- Water barrel
- Oil barrel
- Explosive barrel
- Weighted barrel

### Best implementation

Use either:

- A circular collider for arcade-style rolling, or
- A capsule/rounded rectangle approximation

Avoid simulating the exact shape of a barrel unless necessary.

Attach optional components such as:

- Fluid weight
- Flammability
- Explosion
- Leakage
- Breakability

### Priority

**Optional**

---

## 2.7 Moving Platform

### Gameplay role

Moving platforms transport the player and objects through the level.

They may be:

- Powered by counterweights
- Driven by gears
- Mounted on rails
- Controlled by switches
- Temporarily powered by generators

### Best implementation

Use a kinematic body for most platforms.

A platform should:

- Follow a rail or path
- Carry riders by adding platform displacement to objects standing on it
- Stop at defined endpoints
- Respond to power or mechanical input

Use a dynamic platform only when the puzzle requires the platform itself to react physically.

### Priority

**Essential**

---

## 2.8 Elevator

### Gameplay role

An elevator is a specialized moving platform, often linked to pulley and counterweight systems.

### Best implementation

Use:

- A kinematic or constrained dynamic platform
- Vertical guide rails
- A pulley or motor connection
- Endpoint sensors
- A braking or locking state

The elevator should never drift sideways.

For puzzle readability, show the cable, counterweight, and power source whenever possible.

### Priority

**Essential**

---

## 2.9 Pendulum Bob

### Gameplay role

Pendulums create timing, momentum, and swinging hazards.

They can:

- Knock objects away
- Break obstacles
- Carry the player
- Activate impact switches
- Create periodic danger zones

### Best implementation

Use a dynamic body connected to a fixed anchor by:

- A distance constraint, or
- A hinge plus rigid rod

A rigid rod is more stable than a chain of many rope segments.

Use a chain only if visible flexibility is part of the intended effect.

### Priority

**Useful**

---

## 2.10 Rope Segment

### Gameplay role

Rope segments allow ropes to bend, swing, and react to forces.

### Best implementation

There are two practical options.

#### Option A: Constraint-only rope

Represent the rope as a line between endpoints with one or more constraints.

Advantages:

- Fast
- Stable
- Easy to control

Best for:

- Pulley cables
- Elevator ropes
- Mostly straight ropes

#### Option B: Particle-chain rope

Represent the rope as several particles connected by distance constraints.

Advantages:

- Flexible
- Visually expressive
- Can wrap or swing

Disadvantages:

- More expensive
- More difficult to stabilize
- More likely to stretch or jitter

Best for:

- Climbable ropes
- Loose dangling cables
- Swinging rope bridges

### Priority

**Essential**, but begin with constraint-only ropes.

---

## 2.11 Chain Link

### Gameplay role

Chains can visually communicate strength, weight, and industrial machinery.

### Best implementation

Do not simulate every visible link unless the chain is a major interactive object.

Recommended approach:

- Use a rope or distance-constraint simulation
- Render repeated chain-link sprites along the rope path

Use actual linked rigid bodies only for short chains used in close-up puzzles.

### Priority

**Optional**

---

# 3. Static Environment Objects

## 3.1 Floor

### Gameplay role

Provides stable ground for the player and physics objects.

### Best implementation

Use static rectangle or polygon-like collections of rectangles.

Use moderate-to-high friction and no restitution.

Combine adjacent tiles into larger collision rectangles where possible to reduce collision checks.

### Priority

**Essential**

---

## 3.2 Wall

### Gameplay role

Restricts movement and defines puzzle chambers.

### Best implementation

Use static colliders.

Walls may contain:

- Grab points
- Breakable sections
- Embedded machinery
- Rope anchors
- Climbable surfaces

Keep decorative detail separate from collision geometry.

### Priority

**Essential**

---

## 3.3 Slope

### Gameplay role

Allows rolling objects, gravity puzzles, ramps, and changing movement speed.

### Best implementation

Use line-segment colliders or simple convex ramp colliders.

Collision response must use the surface normal so objects move along the slope rather than being forced directly upward.

Start with a small set of fixed slope angles.

### Priority

**Useful**

---

## 3.4 One-Way Platform

### Gameplay role

Allows the player to jump upward through a platform and land on top.

### Best implementation

Ignore collision when:

- The object is moving upward, or
- The object's previous bottom position was below the platform top

Enable collision only when the object approaches from above.

Do not use one-way behavior for heavy machinery unless specifically designed for it.

### Priority

**Useful**

---

## 3.5 Anchor

### Gameplay role

Anchors provide fixed attachment points for ropes, springs, hinges, and pulleys.

### Best implementation

Represent anchors as static world points or static bodies.

Each anchor should expose:

- World position
- Allowed constraint types
- Maximum load, if breakable
- Visual attachment marker

### Priority

**Essential**

---

## 3.6 Ceiling Hook

### Gameplay role

A visible anchor used for hanging ropes, weights, pulleys, and pendulums.

### Best implementation

Use the anchor system with a hook-specific sprite and optional break strength.

Hooks can create puzzles where the player must choose what to attach.

### Priority

**Useful**

---

## 3.7 Guide Rail

### Gameplay role

Restricts an object to a line or track.

Examples:

- Elevator rails
- Sliding counterweights
- Horizontal doors
- Piston tracks
- Conveyor-mounted machinery

### Best implementation

Use a slider constraint or manually project movement onto the rail axis.

Include minimum and maximum positions.

Guide rails are extremely useful for keeping machinery predictable.

### Priority

**Essential**

---

# 4. Constraints and Connections

## 4.1 Rope Constraint

### Gameplay role

Maintains a maximum or fixed distance between two attachment points.

### Best implementation

Store:

- Body A
- Body B
- Local attachment points
- Rope length
- Maximum tension
- Whether the rope can go slack
- Whether it can break

A rope should pull but not push.

Solve rope constraints several times per physics step.

### Priority

**Essential**

---

## 4.2 Cable

### Gameplay role

A cable is mechanically similar to a rope but may also carry power or signals.

### Best implementation

Use the rope constraint for physical behavior and add a separate power or signal connection.

Possible properties:

- Conductive
- Insulated
- Maximum tension
- Maximum power
- Damaged state

### Priority

**Useful**

---

## 4.3 Chain

### Gameplay role

A chain behaves like a strong, heavy rope.

### Best implementation

Use the rope constraint with:

- Higher visual thickness
- Greater apparent weight
- Lower elasticity
- Optional clanking sound
- Greater break strength

Only simulate multiple chain bodies when close interaction requires it.

### Priority

**Optional**

---

## 4.4 Spring

### Gameplay role

Springs store energy and create launching, recoil, suspension, and timing mechanics.

### Best implementation

Use a spring force based on displacement from a rest length.

Important properties:

- Rest length
- Stiffness
- Damping
- Maximum extension
- Maximum compression
- Break threshold

Springs need damping to prevent endless oscillation.

### Priority

**Useful**

---

## 4.5 Rod

### Gameplay role

A rod maintains a fixed distance and can either rotate freely or remain rigidly oriented.

### Best implementation

Use a distance constraint for a freely rotating rod.

Use a fixed joint for a rigid assembly.

Rods are more stable than ropes for levers, pendulums, and linkage mechanisms.

### Priority

**Useful**

---

## 4.6 Hinge

### Gameplay role

Allows rotation around a fixed point.

Applications:

- Doors
- Levers
- Trapdoors
- Pendulums
- Rotating arms
- Crane booms

### Best implementation

Constrain two attachment points to remain coincident while allowing relative rotation.

Useful properties:

- Minimum angle
- Maximum angle
- Motor speed
- Maximum motor torque
- Friction
- Lock state

### Priority

**Essential**

---

## 4.7 Slider Joint

### Gameplay role

Restricts movement to a single axis.

Applications:

- Elevators
- Pistons
- Sliding gates
- Guided counterweights
- Mechanical presses

### Best implementation

Project the body's allowed movement onto a normalized axis.

Prevent movement perpendicular to that axis.

Include optional:

- Position limits
- Motor force
- Spring behavior
- Locking

### Priority

**Essential**

---

## 4.8 Pulley

### Gameplay role

Changes the direction of rope force and visually defines the game's central mechanic.

### Best implementation

At minimum, a pulley should:

- Define a wheel center and radius
- Connect two rope paths
- Preserve total rope length
- Transfer movement from one side to the other
- Optionally rotate visually based on rope movement

A simple fixed pulley changes force direction but does not create mechanical advantage.

For early prototypes, the rope may be mathematically constrained without simulating rope contact around the wheel.

### Priority

**Essential**

---

## 4.9 Compound Pulley

### Gameplay role

Creates mechanical advantage by using multiple supporting rope segments.

### Best implementation

Model the system using a constraint equation rather than trying to calculate every rope collision.

Useful data:

- Number of supporting rope segments
- Input rope displacement
- Load displacement ratio
- Input force requirement
- Total rope length

Keep the mechanical relationship visually understandable.

### Priority

**Highly recommended after basic pulleys work**

---

## 4.10 Winch

### Gameplay role

Reels rope in or out.

Applications:

- Lifting objects
- Pulling bridges
- Tightening cables
- Moving elevators
- Retracting hooks

### Best implementation

A winch modifies the target rope length over time.

Important properties:

- Reel speed
- Maximum torque
- Minimum rope length
- Maximum rope length
- Direction
- Powered state
- Brake state

If the attached load is too heavy, the winch should slow, stall, or reverse depending on the puzzle design.

### Priority

**Essential**

---

## 4.11 Fixed Joint

### Gameplay role

Attaches two bodies so they act as one assembly.

Applications:

- Temporary construction
- Locked machinery
- Welded components
- Objects held by clamps

### Best implementation

Preserve both relative position and relative rotation.

Fixed joints can be breakable when the force exceeds a threshold.

### Priority

**Useful**

---

## 4.12 Gear Constraint

### Gameplay role

Links the angular movement of two gears.

### Best implementation

Use the relationship between gear radii or tooth counts.

Store:

- Gear A
- Gear B
- Ratio
- Rotation direction
- Maximum transmitted torque
- Whether the gears are engaged

External gears rotate in opposite directions.

Do not rely only on physical tooth collision. A mathematical gear constraint will be much more stable.

### Priority

**Highly recommended**

---

# 5. Mechanical Objects

## 5.1 Pulley Wheel

### Gameplay role

Guides rope and communicates the direction of force.

### Best implementation

Use:

- A static or rotating rigid body
- A circular visual representation
- A pulley connection component
- Angular rotation derived from rope movement

The wheel itself does not need detailed collision unless loose objects can hit it.

### Priority

**Essential**

---

## 5.2 Gear

### Gameplay role

Transfers rotational motion and changes direction or speed.

### Best implementation

Use a rotating body with:

- Radius
- Tooth count
- Angular velocity
- Maximum torque
- Gear connections

The teeth may be decorative. Use gear constraints for actual transmission.

### Priority

**Highly recommended**

---

## 5.3 Gear Train

### Gameplay role

Combines several gears to alter speed, direction, and torque.

### Best implementation

Create a graph of connected gears.

When one gear turns, propagate angular motion through the graph using each gear ratio.

Prevent cycles from applying the same update repeatedly.

Use visual markings to show rotation direction.

### Priority

**Useful after basic gears**

---

## 5.4 Crank

### Gameplay role

Lets the player manually create rotational motion.

### Best implementation

Use a rotating body with a handle interaction point.

Possible controls:

- Hold the interaction button
- Move left/right to rotate
- Press repeatedly to turn
- Use mouse or analog-stick circular motion

Convert player input into torque or direct angular displacement.

### Priority

**Highly recommended**

---

## 5.5 Flywheel

### Gameplay role

Stores rotational momentum and smooths intermittent power.

### Best implementation

Use a rotating rigid body with high moment of inertia and low angular damping.

It can:

- Keep a generator running briefly
- Smooth crank input
- Power a timed mechanism
- Resist rapid speed changes

### Priority

**Useful**

---

## 5.6 Lever

### Gameplay role

Lets the player trade force for distance and control switches or machinery.

### Best implementation

Use a rigid rod attached to a hinge.

Important properties:

- Pivot position
- Handle position
- Load position
- Angle limits
- Mass
- Interaction force

For puzzle clarity, exaggerate lever motion and provide clear stop positions.

### Priority

**Highly recommended**

---

## 5.7 Seesaw

### Gameplay role

Creates balance and weight-comparison puzzles.

### Best implementation

Use a long rigid body attached to a center hinge.

Add angle limits so it cannot rotate completely around.

Objects resting on each side apply torque based on their mass and distance from the pivot.

### Priority

**Useful**

---

## 5.8 Ratchet

### Gameplay role

Allows rotation in one direction while preventing reverse movement.

### Best implementation

Track the connected wheel's angle.

When motion attempts to reverse:

- Lock rotation, or
- Snap back to the previous tooth position

Ratchets are useful for preserving player progress and creating safer lifting puzzles.

### Priority

**Useful**

---

## 5.9 Clutch

### Gameplay role

Connects or disconnects rotational power.

### Best implementation

Treat the clutch as a switchable gear or shaft constraint.

States:

- Disengaged
- Partially engaged
- Fully engaged

Partial engagement may gradually match angular velocity, but a simple on/off state is sufficient for most puzzles.

### Priority

**Optional**

---

## 5.10 Brake

### Gameplay role

Slows or locks mechanical motion.

### Best implementation

Apply angular damping or opposing torque.

Brake types:

- Passive friction brake
- Player-controlled brake
- Power-dependent brake
- Emergency locking brake

### Priority

**Useful**

---

## 5.11 Cam

### Gameplay role

Converts rotational motion into repeating linear motion.

Applications:

- Moving platforms
- Hammer traps
- Pumps
- Timed switches
- Automatic doors

### Best implementation

Use a rotating cam angle to calculate a follower's target position.

The visible cam can rotate physically, but the follower should often use a guided kinematic or slider-controlled movement for stability.

### Priority

**Optional but valuable for advanced puzzles**

---

## 5.12 Conveyor Belt

### Gameplay role

Moves objects horizontally or along an incline.

### Best implementation

Keep the belt body static or kinematic.

When an object contacts the top surface, apply tangential surface velocity or a controlled force.

Properties:

- Direction
- Speed
- Maximum force
- Powered state
- Reversible state

Avoid simply teleporting objects each frame.

### Priority

**Highly recommended**

---

## 5.13 Turntable

### Gameplay role

Rotates objects, redirects conveyors, or changes puzzle layout.

### Best implementation

Use a rotating kinematic platform.

Objects on top should inherit tangential motion from the platform.

Include angle stops or discrete rotation positions when precise alignment is required.

### Priority

**Optional**

---

# 6. Power and Logic Objects

## 6.1 Generator

### Gameplay role

Converts mechanical motion into puzzle power.

Possible inputs:

- Falling counterweight
- Crank
- Water wheel
- Conveyor
- Flywheel
- Gear train

### Best implementation

Measure the connected shaft's angular speed and torque.

A simplified model can calculate generated power from rotational speed:

```cpp
generatedPower = efficiency * fabsf(angularVelocity) * torqueFactor;
```

Use a threshold or charge meter rather than a full electrical simulation.

The generator should visibly respond with:

- Spinning animation
- Lights
- Sparks
- Sound pitch
- Meter movement

### Priority

**Essential**

---

## 6.2 Battery

### Gameplay role

Stores generated power and lets a mechanism remain active after the generator stops.

### Best implementation

Track:

- Current charge
- Maximum charge
- Charge rate
- Discharge rate
- Leakage rate
- Connected devices

A battery can create puzzles involving preparation and delayed use.

### Priority

**Useful**

---

## 6.3 Electric Motor

### Gameplay role

Converts power into mechanical rotation or linear motion.

### Best implementation

Apply torque to a shaft when powered.

Properties:

- Target speed
- Maximum torque
- Power consumption
- Direction
- Stall state

Motors should slow under heavy loads instead of maintaining impossible speed.

### Priority

**Highly recommended**

---

## 6.4 Switch

### Gameplay role

Directly changes the state of machinery or power connections.

### Best implementation

Use a simple boolean or enumerated state.

Possible switch types:

- Toggle
- Momentary
- Timed
- Keyed
- Directional
- Mechanical

A switch should send an event rather than directly containing all device logic.

### Priority

**Essential**

---

## 6.5 Pressure Plate

### Gameplay role

Detects weight placed on a surface.

### Best implementation

Use a trigger region and sum the mass or downward force of overlapping objects.

Possible activation rules:

- Any object
- Minimum total mass
- Specific object type
- Player only
- Continuous hold
- Toggle when pressed

### Priority

**Essential**

---

## 6.6 Limit Switch

### Gameplay role

Detects when a mechanism reaches an endpoint.

### Best implementation

Attach a sensor to the end of a rail, door path, elevator shaft, or piston.

Use it to:

- Stop a motor
- Reverse movement
- Unlock another system
- Trigger sound and animation

### Priority

**Highly recommended**

---

## 6.7 Relay

### Gameplay role

Allows one signal to control another system.

### Best implementation

Use a logic node that accepts one or more input signals and controls one or more outputs.

Possible relay behaviors:

- Normally open
- Normally closed
- Delayed
- Latched
- Power-dependent

### Priority

**Optional**

---

## 6.8 Fuse

### Gameplay role

Protects a circuit or creates overload puzzles.

### Best implementation

Track incoming power or current as a simplified value.

If it exceeds the fuse rating for too long, the fuse changes to a broken state.

The player may need to:

- Replace it
- Reset it
- Reduce the load
- Bypass it

### Priority

**Optional**

---

## 6.9 Light Bulb

### Gameplay role

Provides visual feedback and may illuminate hidden platforms or hazards.

### Best implementation

Use a powered state and optional brightness value.

Brightness may depend on supplied power.

In **Power Pulley Panic**, lights can directly affect gameplay by revealing platforms, spike pits, or machinery hidden in darkness.

### Priority

**Essential**

---

## 6.10 Door Lock

### Gameplay role

Prevents a door or gate from moving until conditions are met.

### Best implementation

Use a lock state on the door's hinge, slider, or motor.

Possible unlock requirements:

- Sufficient power
- Correct switch combination
- Physical key
- Pressure plate
- Mechanical alignment

### Priority

**Essential**

---

## 6.11 Powered Gate

### Gameplay role

Acts as a major obstacle or level goal.

### Best implementation

Use:

- A slider or hinge constraint
- A motor component
- A lock state
- Limit switches
- Power requirement

The gate should have clear open, closed, moving, and stalled states.

### Priority

**Essential**

---

# 7. Force Sources

## 7.1 Gravity Field

### Gameplay role

Provides the primary downward force.

### Best implementation

Use a global gravity vector:

```cpp
Vector2 gravity = { 0.0f, 1200.0f };
```

Apply it to dynamic bodies with `affectedByGravity == true`.

Different areas may optionally use altered gravity, but standard gravity should remain consistent in most levels.

### Priority

**Essential**

---

## 7.2 Wind

### Gameplay role

Pushes light objects, hanging ropes, particles, and the player.

### Best implementation

Use a trigger volume that applies force based on:

- Wind direction
- Wind strength
- Object area
- Object mass
- Exposure

Use steady wind first. Gusting wind can be added later.

### Priority

**Optional**

---

## 7.3 Water Current

### Gameplay role

Moves floating or submerged objects.

### Best implementation

Within a water trigger volume:

- Apply buoyancy
- Apply drag
- Apply current force

A simplified current can push bodies toward a constant velocity.

### Priority

**Optional**

---

## 7.4 Fan

### Gameplay role

Creates a localized wind force.

### Best implementation

Use a directional trigger region extending from the fan.

Force should decrease with distance or remain constant within a clearly defined area.

Fans can be powered, reversible, or blocked by objects.

### Priority

**Useful**

---

## 7.5 Magnet

### Gameplay role

Attracts or repels designated metal objects.

### Best implementation

Apply force only to objects with a magnetic component.

Possible model:

- Constant force within range
- Inverse-distance force
- Directional electromagnet

For stable puzzles, clamp the maximum magnetic force.

### Priority

**Optional**

---

## 7.6 Piston

### Gameplay role

Produces controlled linear force.

Applications:

- Pushers
- Lifts
- Crushers
- Pumps
- Launchers

### Best implementation

Use a slider joint with a motor.

Properties:

- Extension speed
- Retraction speed
- Maximum force
- Stroke length
- Powered state

### Priority

**Highly recommended**

---

## 7.7 Hydraulic Cylinder

### Gameplay role

Functions like a powerful piston and can emphasize force multiplication.

### Best implementation

Use the piston system with slower movement and greater force.

Hydraulic fluid does not need to be fully simulated. Use pressure as a gameplay resource or state if needed.

### Priority

**Optional**

---

## 7.8 Rocket Thruster

### Gameplay role

Applies force opposite its facing direction.

### Best implementation

Apply continuous force at the thruster's position, which may also create torque.

Useful for:

- Experimental machinery
- Unstable platforms
- Launch puzzles
- Comedic failures

### Priority

**Optional and probably late-game**

---

# 8. Hazards

## 8.1 Crushing Block

### Gameplay role

Creates timing hazards and can destroy objects.

### Best implementation

Use a kinematic or slider-controlled body.

Detect when an object is trapped between the block and another solid surface.

Avoid relying only on raw collision force to determine crushing.

### Priority

**Useful**

---

## 8.2 Spike Platform

### Gameplay role

Damages or resets the player when contacted.

### Best implementation

Use:

- A solid collider for the platform
- A separate damage trigger for the spikes

Spikes may retract through a piston or mechanical linkage.

### Priority

**Essential**

---

## 8.3 Falling Weight

### Gameplay role

Acts as both a physics object and a hazard.

### Best implementation

Use a heavy counterweight body with:

- Damage on high-speed impact
- Guide rails
- Rope connection
- Reset logic

### Priority

**Highly recommended**

---

## 8.4 Swinging Hammer

### Gameplay role

Creates a periodic impact hazard.

### Best implementation

Use a hinge and rigid rod with a heavy hammer head.

The hammer may be:

- Gravity-driven
- Motor-driven
- Cam-driven
- Released by a trigger

### Priority

**Useful**

---

## 8.5 Saw Blade

### Gameplay role

Creates a rotating or moving hazard.

### Best implementation

Use a kinematic rotating body or animated hazard.

Only use physical angular momentum if the blade must interact with other objects.

For ordinary player damage, visual rotation plus a trigger collider is enough.

### Priority

**Optional**

---

## 8.6 Rolling Boulder

### Gameplay role

Acts as a mobile crushing hazard.

### Best implementation

Use the boulder object with:

- Path design based on slopes
- Damage based on speed
- Destruction or reset at endpoints

### Priority

**Useful**

---

## 8.7 Steam Vent

### Gameplay role

Pushes or damages objects periodically.

### Best implementation

Use a timed force trigger and damage region.

Visual particles should be separate from the actual force calculation.

### Priority

**Optional**

---

## 8.8 Electrical Arc

### Gameplay role

Creates a powered hazard that may be disabled by cutting power.

### Best implementation

Use a trigger volume between two electrodes.

States:

- Off
- Charging
- Active
- Overloaded

The arc can react to conductive objects if desired, but begin with simple on/off behavior.

### Priority

**Optional**

---

# 9. Fluids and Granular Materials

## 9.1 Water

### Gameplay role

Introduces buoyancy, drag, currents, floating objects, and water-driven machinery.

### Best implementation

Use a rectangular water region.

For each submerged body:

- Estimate submerged fraction
- Apply upward buoyancy
- Apply linear drag
- Apply angular drag
- Apply current velocity

Avoid full fluid simulation.

### Priority

**Optional**

---

## 9.2 Oil

### Gameplay role

Changes friction and may interact with fire or machinery.

### Best implementation

Oil may be represented as:

- A floor trigger that reduces friction
- A fluid region with low drag
- A leaking hazard
- A power-system resource

Do not simulate individual liquid particles.

### Priority

**Optional**

---

## 9.3 Sand

### Gameplay role

Can act as flowing weight, a timer, or material that fills containers.

### Best implementation

Possible simplified approaches:

- Discrete sand units
- A fill-level value inside containers
- A moving region or pile approximation

A full granular simulation is likely too expensive and unstable for the initial game.

### Priority

**Late optional feature**

---

## 9.4 Mud

### Gameplay role

Slows movement and increases drag.

### Best implementation

Use a trigger region that:

- Reduces horizontal speed
- Increases damping
- Reduces jump strength
- Applies suction when leaving

### Priority

**Optional**

---

# 10. Sensors

## 10.1 Weight Sensor

### Gameplay role

Measures mass or force.

### Best implementation

Use a pressure plate trigger and sum supported mass.

Can output:

- Boolean activation
- Continuous weight value
- Weight category

### Priority

**Highly recommended**

---

## 10.2 Rotation Sensor

### Gameplay role

Detects gear, wheel, or generator rotation.

### Best implementation

Measure:

- Current angle
- Angular velocity
- Rotation direction
- Number of completed turns

Useful for generators and timed mechanical puzzles.

### Priority

**Essential**

---

## 10.3 Speed Sensor

### Gameplay role

Detects how fast an object is moving.

### Best implementation

Read linear or angular velocity and compare it with thresholds.

Use hysteresis so the sensor does not rapidly switch on and off near the threshold.

### Priority

**Useful**

---

## 10.4 Position Sensor

### Gameplay role

Detects whether an object reached a location.

### Best implementation

Use:

- Trigger region
- Distance check
- Rail position check
- Endpoint switch

### Priority

**Essential**

---

## 10.5 Beam Sensor

### Gameplay role

Detects whether a line of sight is blocked.

### Best implementation

Use a raycast or line-versus-collider test.

Beam sensors can detect:

- Player
- Crates
- Moving machinery
- Smoke or steam, if desired

### Priority

**Useful**

---

## 10.6 Proximity Sensor

### Gameplay role

Detects nearby bodies without requiring contact.

### Best implementation

Use a circular or rectangular trigger region.

Filter detected objects by collision layer or tag.

### Priority

**Useful**

---

## 10.7 Timer

### Gameplay role

Creates delayed, periodic, or temporary behavior.

### Best implementation

Use a logic component with:

- Duration
- Remaining time
- Repeat setting
- Start condition
- Pause state

Timers should use the same fixed physics timestep as gameplay systems when precise synchronization matters.

### Priority

**Essential**

---

# 11. General Gameplay Objects

## 11.1 Checkpoint

### Gameplay role

Defines where the player returns after failure.

### Best implementation

Store:

- Player respawn position
- Puzzle state policy
- Objects that reset
- Objects that remain changed

Decide early whether the entire puzzle resets or only the player resets.

### Priority

**Essential**

---

## 11.2 Goal Switch

### Gameplay role

Marks completion of a puzzle condition.

### Best implementation

Use a sensor or logic node that activates when all required conditions are satisfied.

The goal may open the final gate or complete the level.

### Priority

**Essential**

---

## 11.3 Collectible

### Gameplay role

Rewards exploration and optional puzzle solutions.

### Best implementation

Use a trigger collider and persistent collected state.

Collectibles generally do not require full rigid-body physics.

### Priority

**Optional**

---

## 11.4 Key

### Gameplay role

Unlocks a specific mechanism.

### Best implementation

Possible forms:

- Inventory item
- Physical object carried to a lock
- Gear-shaped mechanical key
- Power cell
- Fuse

A physical key can use a lightweight dynamic body and socket interaction.

### Priority

**Optional**

---

## 11.5 Lock

### Gameplay role

Prevents use of an object until a condition is met.

### Best implementation

A lock should disable a constraint, motor, switch, or interaction.

It should not duplicate the entire behavior of the object it controls.

### Priority

**Essential as a reusable state**

---

## 11.6 Breakable Crate

### Gameplay role

Creates destructible obstacles and temporary weight.

### Best implementation

Use a normal crate with:

- Health
- Impact threshold
- Break effect
- Spawned debris
- Optional contents

Debris should be limited or short-lived for performance.

### Priority

**Useful**

---

## 11.7 Explosive Barrel

### Gameplay role

Creates chain reactions and force-based destruction.

### Best implementation

Use a barrel with an explosion component.

On detonation:

- Apply radial impulse
- Damage nearby objects
- Trigger breakable joints
- Spawn visual effects
- Remove or replace the barrel

Clamp the impulse to prevent extreme velocities and physics instability.

### Priority

**Optional**

---

# 12. Recommended Development Order

## Phase 1: Essential Prototype

Implement these first:

1. Fixed-timestep physics loop
2. Player controller
3. Static floors and walls
4. Dynamic crates
5. Basic collision detection and resolution
6. Gravity
7. Triggers and sensors
8. Pressure plates
9. Moving platforms
10. Basic ropes
11. Counterweights
12. Fixed pulleys
13. Powered gates
14. Checkpoints and reset logic

This phase is enough to build the game's central loop.

---

## Phase 2: Mechanical Depth

Add:

1. Hinges
2. Slider joints
3. Winches
4. Cranks
5. Generators
6. Electric motors
7. Lights
8. Gears and gear constraints
9. Levers
10. Limit switches
11. Falling-weight hazards
12. Conveyor belts

This phase creates the distinctive identity of **Power Pulley Panic**.

---

## Phase 3: Advanced Puzzle Tools

Add:

1. Compound pulleys
2. Springs
3. Flywheels
4. Ratchets
5. Brakes
6. Pistons
7. Beam sensors
8. Batteries
9. Pendulums
10. Breakable joints
11. Fans and wind
12. Water and buoyancy

---

## Phase 4: Optional Complexity

Add only when a level specifically requires them:

- Chains made from individual links
- Hydraulic systems
- Clutches
- Cams
- Oil
- Sand
- Magnets
- Electrical arcs
- Rocket thrusters
- Detailed fluid simulation

---

# 13. Physics Update Recommendation

Use a fixed timestep so puzzles behave consistently.

Example:

```cpp
constexpr float FIXED_TIME_STEP = 1.0f / 120.0f;

float accumulator = 0.0f;

while (!WindowShouldClose())
{
    float frameTime = GetFrameTime();
    frameTime = fminf(frameTime, 0.25f);

    accumulator += frameTime;

    HandleInput();

    while (accumulator >= FIXED_TIME_STEP)
    {
        ApplyForces(FIXED_TIME_STEP);
        IntegrateVelocities(FIXED_TIME_STEP);

        DetectCollisions();
        SolveConstraints();
        ResolveCollisions();

        IntegratePositions(FIXED_TIME_STEP);
        UpdateSensors(FIXED_TIME_STEP);
        UpdatePowerSystems(FIXED_TIME_STEP);

        accumulator -= FIXED_TIME_STEP;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawGameWorld();

    EndDrawing();
}
```

For ropes, pulleys, and stacked crates, perform several constraint iterations per physics step.

Example:

```cpp
for (int i = 0; i < 8; ++i)
{
    SolveRopeConstraints();
    SolvePulleyConstraints();
    SolveJointConstraints();
    ResolveCollisionContacts();
}
```

---

# 14. Final Design Principle

The best object system for **Power Pulley Panic** is not a collection of isolated classes. It is a library of reusable physical and logical components.

For example:

```text
Elevator
├── Transform2D
├── RigidBody2D or KinematicBody2D
├── RectangleCollider
├── SliderConstraint
├── RopeAttachment
├── MotorComponent
├── PowerConsumer
└── EndpointSensor
```

```text
Generator
├── Transform2D
├── RotatingBody
├── RotationSensor
├── GeneratorComponent
├── PowerOutput
└── SoundComponent
```

```text
Counterweight
├── Transform2D
├── DynamicRigidBody2D
├── RectangleCollider
├── RopeAttachment
└── DamageComponent
```

Building objects from components will make it easier to create new puzzle machinery without rewriting the physics engine every time.
