# Game Data

This folder is for authored data files that define the game outside C++ code.

- `levels/`: level files, likely Tiled JSON/TMJ or a compatible object-layer format.
- `campaigns/`: level ordering, unlock flow, and campaign metadata.
- `credits.txt`: credits screen entries, one person per line as `"Job title" "Real name" "Username"`. Lines starting with `#` are comments. All three fields are required; entries appear in file order, five per page.
- `achievements.txt`: achievement ids, display names, and descriptions.

Runtime builds copy this folder next to the executable as `game_data/`.
