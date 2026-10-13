#pragma once
#include "Level.h"

inline void TransformSelectedTile(VisualTile& tile, int action, int left, int top, int right, int bottom) {
    const int x = static_cast<int>(tile.position.x) / 32;
    const int y = static_cast<int>(tile.position.y) / 32;
    int targetX = x;
    int targetY = y;
    if (action == 4) {
        targetX = left + (y - top);
        targetY = top + (right - x);
        tile.quarterTurns = (tile.quarterTurns + 3) % 4;
    }
    else if (action == 5) {
        targetX = left + (bottom - y);
        targetY = top + (x - left);
        tile.quarterTurns = (tile.quarterTurns + 1) % 4;
    }
    else if (action == 6) { targetX = left + right - x; tile.flipX = !tile.flipX; }
    else if (action == 7) { targetY = top + bottom - y; tile.flipY = !tile.flipY; }
    // Put the entire copy beside the source, preserving every original cell.
    else if (action == 8) targetX += right - left + 1;
    else if (action == 9) --targetX;
    else if (action == 10) ++targetX;
    else if (action == 11) --targetY;
    else if (action == 12) ++targetY;
    tile.position = {static_cast<float>(targetX * 32), static_cast<float>(targetY * 32)};
}
