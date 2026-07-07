#include "Player.h"

void ResetPlayer(Player& player) {
    player.rect = {80, 600, 31, 40};
    player.velocity = {0, 0};
    player.coyoteTimer = 0.0f;
    player.jumpBufferTimer = 0.0f;
    player.animationTimer = 0.0f;
    player.onGround = false;
    player.climbing = false;
    player.facingRight = true;
    player.walking = false;
}
