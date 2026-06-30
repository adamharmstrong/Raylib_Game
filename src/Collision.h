#pragma once

#include "Player.h"

#include <vector>

void ResolveHorizontal(Player& player, const std::vector<Rectangle>& solids);
void ResolveVertical(Player& player, const std::vector<Rectangle>& solids);
