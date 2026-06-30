#include "Player.h"

void ResetPlayer(Player& player) {
    player.rect = {80, 600, 31, 40};
    player.velocity = {0, 0};
    player.onGround = false;
    player.facingRight = true;
    player.walking = false;
}
