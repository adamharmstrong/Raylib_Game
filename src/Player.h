#pragma once

#include "raylib.h"

struct Player {
    Rectangle rect{80, 600, 31, 40};
    Vector2 velocity{0, 0};
    bool onGround{false};
    bool facingRight{true};
    bool walking{false};
};

void ResetPlayer(Player& player);
