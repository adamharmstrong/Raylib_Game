#pragma once

#include "Player.h"

#include <vector>
struct Level;
void ResolveTilePassage(Player& player, const Level& level, Rectangle previous, bool horizontal);

void ResolveHorizontal(Player& player, const std::vector<Rectangle>& solids);
void ResolveVertical(Player& player, const std::vector<Rectangle>& solids);
std::vector<Rectangle> BuildLadderMovementSolids(const std::vector<Rectangle>& solids,
    const std::vector<Rectangle>& ladders, Rectangle playerBounds, bool climbing);
