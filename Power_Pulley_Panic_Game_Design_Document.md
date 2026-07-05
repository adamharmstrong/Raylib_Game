# Power Pulley Panic

## Game Design Document

### Elevator Pitch

**Power Pulley Panic** is a 2D puzzle-platformer built around simple
machines. Players operate mechanical winches that spin large pulley
wheels connected to electrical generators. While the pulleys spin,
generators provide electricity that powers lights and machinery. As the
spring-loaded winches slowly unwind, generators lose power, forcing
players to race through obstacle courses before darkness returns.

Created for the **"Spin to Win"** game jam, every major puzzle revolves
around making machinery spin.

------------------------------------------------------------------------

# Genre

-   Puzzle Platformer
-   Physics-inspired
-   Mechanical Puzzle Game

# Theme

**Spin to Win**

Every puzzle is solved by winding up a mechanical system and keeping its
pulleys spinning.

# Core Gameplay Loop

1.  Discover a puzzle.
2.  Climb to the raised platform.
3.  Hold **E** to grab the winch.
4.  Pull the winch along its rail.
5.  Release it and let the spring slowly unwind.
6.  Spinning pulleys drive generators.
7.  Generators power lights and machinery.
8.  Cross the illuminated hazard room before the machine winds down.
9.  Reach the exit.

# Story

The player explores an abandoned industrial fortress powered entirely by
gears, ropes, pulleys, flywheels, generators, and counterweights. Every
obstacle has a mechanical explanation.

# Controls

  Action       Key
  ------------ --------
  Select/Start Enter / Space
  Map Select   A / D or Arrow Keys
  Pause        Esc / Enter
  Move         A / D
  Jump         Space
  Climb        W / S
  Grab Winch   Hold E
  Toggle FPS   F
  Console      ` / ~

# Mechanics

## Title Screen

The game opens on a title screen with animated pulleys and basic control
prompts. Pressing Enter or Space opens the level select map. The title
screen provides menu buttons for New Game, Continue, Load Custom,
Settings, and Quit Game. Quit Game opens a confirmation prompt before
exiting.

## Level Select / Overworld

The first version of the overworld is a shape-drawn factory district map
with connected level nodes. New Game and Continue both open this map.
Players can select nodes with A/D, arrow keys, or the mouse. Enter,
Space, or clicking an unlocked node starts that level. For now, only the
first node launches the current puzzle. Clearing a level marks its node
complete, unlocks the next connected node, and returns the player to the
map.

## Pause Screen

During gameplay, Esc or Enter opens a pause overlay instead of exiting
the application. Esc or Enter resumes, and T returns to the title screen.
The pause screen provides menu buttons for
Resume, Restart Level, Controls, Settings, Return to Map, Return to Title Screen, and Quit Game. Quit
Game opens a confirmation prompt before exiting. Controls opens a popup
window listing the current keyboard inputs.

## Developer Console

Pressing the backtick/tilde key opens an in-game command console. During
development, it supports fast debug commands such as reset, teleport,
machine power control, and collision visualization. Later, the same
command system can expose safe cheat-code commands in release builds.

## Winch

The player grabs a spring-loaded winch and drags it along a rail.
Releasing it allows the spring to slowly return it to its starting
position.

## Pulley Network

The winch drives ropes wrapped around large pulley wheels. The spinning
wheels are the visual centerpiece of the game.

## Rotary Latch

A rotary latch is a reusable machine part that spins while powered. The
player can latch it only when its rotating spoke aligns with its target
notch. Rotary latches can be used to unlock gates, complete circuits,
stabilize machinery, or force timing-based synchronization puzzles.

## Generator

A pulley drives an electrical generator. Power is only produced while
the pulley rotates.

## Electrical System

Visible blue wires connect generators to lights, gate motors, and future
machinery.

## Lighting

The hazard room is covered in darkness until the generator powers the
lights. Platforms are always present physically, but become difficult to
see when the lights go out.

# Hazard Room

The player crosses a deep spike pit using floating platforms while
weaving around moving counterweights attached to independent pulleys.

# Final Puzzle

A second winch powers a gate motor. The player must keep the machinery
spinning long enough to open the castle gate and escape.

# Visual Style

-   Medieval industrial
-   Stone fortress
-   Wooden and iron pulleys
-   Thick ropes
-   Brass generators
-   Blue electrical wiring
-   Warm hanging lamps

# Audio

-   Rope creaks
-   Turning pulleys
-   Clicking gears
-   Generator hum
-   Electrical buzz
-   Spring unwinding

# Future Ideas

-   Multiple generators
-   Gear ratios
-   Rotary latch synchronization puzzles
-   Conveyor belts
-   Elevators
-   Rotating bridges
-   Split-power puzzles
-   Data-driven level files for large level sets

# Win Condition

Reach the castle gate before the machinery winds down. Clearing a level
briefly shows a level clear message, then returns to the overworld map
with the completed node marked and the next node unlocked.

# Failure Conditions

-   Fall into the spike pit.
-   Become trapped by moving counterweights.
-   Lose electrical power before reaching safety.

When the player dies, the game-over screen provides direct Restart
Level and Quit Game buttons. Quit Game opens a confirmation prompt before
exiting.

# Design Goals

-   Clear cause-and-effect.
-   Consistent mechanical rules.
-   Spinning machinery as the core gameplay.
-   Intuitive puzzle design built around simple machines.
