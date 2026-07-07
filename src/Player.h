#pragma once

#include "raylib.h"

struct Player {
    Rectangle rect{80, 600, 31, 40};
    Vector2 velocity{0, 0};
    float coyoteTimer{0.0f};
    float jumpBufferTimer{0.0f};
    float animationTimer{0.0f};
    bool onGround{false};
    bool climbing{false};
    bool facingRight{true};
    bool walking{false};
};

void ResetPlayer(Player& player);
