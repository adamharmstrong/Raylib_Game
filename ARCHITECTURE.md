# Power Pulley Panic Architecture

The game is organized around reusable parts rather than one-off level scripts. This keeps the current jam prototype playable while making room for many future levels built from the same mechanical vocabulary.

## Active Code Layout

- `src/main.cpp` starts the game and stays intentionally small.
- `src/Game.*` owns the main loop, current state, update order, win/loss checks, and high-level drawing sequence.
- `src/Level.*` owns level layout data: solids, platforms, pulleys, hazards, exit triggers, and machine parts.
- `src/Machine.*` owns reusable mechanical components such as winches, hanging weights, and rotary latches.
- `src/Player.*` owns player state and reset behavior.
- `src/Collision.*` owns collision resolution helpers.
- `src/Render.*` owns drawing helpers for machines, hazards, the player, and environment pieces.
- `src/DevConsole.*` owns the in-game command console input, output history, and overlay rendering.
- `src/MathUtils.*` owns small shared math helpers.
- `src/Constants.h` owns tuning values shared across systems.
- `prototypes/main_0.cpp` preserves the old raylib alignment prototype outside the active build.
- `assets/first_party` contains art, audio, and source files created by the project team.
- `assets/third_party` contains vendor assets kept separate by vendor and pack.
- `assets/generated` contains generated atlases, conversions, and temporary pipeline outputs.
- `game_data` contains level, campaign, and other data files.

## Reusable Machine Parts

Machine parts should expose simple behavior and state so levels can compose them in many combinations. A later data-driven level format can create these parts from JSON, TMX, or another editor-friendly format.

Current reusable parts:

- `Winch`: produces machine power while pulled and slowly unwinds.
- `HangingWeight`: moves from pulley power and acts as a hazard.
- `RotaryLatch`: spins while powered and can latch only when its spoke aligns with its target notch.
- `StoneBlock`: a heavy pushable body that can stand on platforms, blocks, and see-saws.
- `SeeSaw`: a tilting beam that reacts to player and stone block weight.
- `Chain`: a repeated two-link visual span for machine rigging and future mechanical connections.

## Developer Console

The tilde/backtick console is intended to serve as both a development tool and a future cheat-code layer. `DevConsole` handles text entry, command history, and drawing. `Game` owns command execution so command handlers can safely change game state without exposing every subsystem publicly.

Initial commands:

- `help`
- `clear`
- `reset`
- `win`
- `kill`
- `fps`
- `debug_collision [on|off]`
- `teleport <x> <y>`
- `power <0..1>`
- `player`
- `machine`

## Game Modes

`Game` starts in `GameMode::Title`. Pressing Enter or Space opens `GameMode::Overworld`, a first-pass level select map drawn with raylib shapes. The overworld owns a small node/path model so the current level can be launched from an unlocked node while campaign progress marks completed nodes and unlocks the next node. During gameplay, Esc or Enter switches to `GameMode::Paused`; the pause screen draws over the current gameplay frame and freezes gameplay updates. Clearing a level briefly shows a clear message, records progress on the overworld, and returns to the map. The title, overworld, and pause screens use simple clickable menu buttons, including a Quit Game action that opens a confirmation modal before setting `shouldQuit`. Menu and map states are kept separate from gameplay update/draw code so future menus, save slots, level select data, and settings screens can grow without crowding the level logic.

Current state flow:

- `Title`: main menu, title art, and application-level buttons.
- `Overworld`: campaign map and level node selection.
- `Playing`: active puzzle-platformer level.
- `Paused`: gameplay overlay with resume, settings placeholders, title return, and quit.

Good future parts:

- `Generator`
- `Gate`
- `Lamp`
- `Lift`
- `PressurePlate`
- `Conveyor`
- `GearTrain`
- `SignalConnection`

## Level Scaling Direction

For hundreds of levels, C++ should define the rules and parts. Level data should define placement, tuning, physics shapes, visual layers, and connections. The likely long-term path is Tiled JSON/TMJ or a similar object-layer format:

```json
{
  "physics": [
    {
      "type": "solid",
      "x": 0,
      "y": 650,
      "width": 300,
      "height": 40
    }
  ],
  "parts": [
    {
      "id": "gate_latch_a",
      "type": "rotary_latch",
      "x": 1375,
      "y": 603,
      "targetAngle": 270,
      "tolerance": 5,
      "spinSpeed": 115
    }
  ],
  "connections": [
    {
      "from": "gate_latch_a.latched",
      "to": "exit_gate.unlock"
    }
  ]
}
```

That keeps level design focused on arranging machines and physics shapes instead of writing new C++ for every room. Visual tile art should stay separate from gameplay collision so physics remains predictable.
