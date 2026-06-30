#include "Collision.h"

void ResolveHorizontal(Player& player, const std::vector<Rectangle>& solids) {
    for (const Rectangle& solid : solids) {
        if (CheckCollisionRecs(player.rect, solid)) {
            if (player.velocity.x > 0) {
                player.rect.x = solid.x - player.rect.width;
            }
            else if (player.velocity.x < 0) {
                player.rect.x = solid.x + solid.width;
            }

            player.velocity.x = 0;
        }
    }
}

void ResolveVertical(Player& player, const std::vector<Rectangle>& solids) {
    player.onGround = false;

    for (const Rectangle& solid : solids) {
        if (CheckCollisionRecs(player.rect, solid)) {
            if (player.velocity.y > 0) {
                player.rect.y = solid.y - player.rect.height;
                player.velocity.y = 0;
                player.onGround = true;
            }
            else if (player.velocity.y < 0) {
                player.rect.y = solid.y + solid.height;
                player.velocity.y = 0;
            }
        }
    }
}
