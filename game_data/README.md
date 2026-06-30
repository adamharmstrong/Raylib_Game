# Game Data

This folder is for authored data files that define the game outside C++ code.

- `levels/`: level files, likely Tiled JSON/TMJ or a compatible object-layer format.
- `campaigns/`: level ordering, unlock flow, and campaign metadata.

Runtime builds copy this folder next to the executable as `game_data/`.
