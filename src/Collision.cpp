#include "Collision.h"
#include "Level.h"
#include <algorithm>

void ResolveTilePassage(Player& player, const Level& level, Rectangle previous, bool horizontal) {
    for (const auto& tile : level.visualTiles) {
        if (!TileCollidesWithLayer(tile.layer, WorldLayer::Middleground)) continue;
        const int mask = GetTileBlockedFaces(level, tile);
        if (mask == 0 || mask == 15) continue;
        const Rectangle solid{tile.position.x, tile.position.y, 32, 32};
        if (!CheckCollisionRecs(player.rect, solid)) continue;
        if (horizontal) {
            if (player.velocity.x > 0 && (mask & 8) && previous.x + previous.width <= solid.x + 0.01f) {
                player.rect.x = solid.x - player.rect.width;
                player.velocity.x = 0;
            } else if (player.velocity.x < 0 && (mask & 2) && previous.x >= solid.x + solid.width - 0.01f) {
                player.rect.x = solid.x + solid.width;
                player.velocity.x = 0;
            }
        } else {
            if (player.velocity.y > 0 && (mask & 1) && previous.y + previous.height <= solid.y + 0.01f) {
                player.rect.y = solid.y - player.rect.height;
                player.velocity.y = 0;
                player.onGround = true;
            } else if (player.velocity.y < 0 && (mask & 4) && previous.y >= solid.y + solid.height - 0.01f) {
                player.rect.y = solid.y + solid.height;
                player.velocity.y = 0;
            }
        }
    }
}

std::vector<Rectangle> BuildLadderMovementSolids(const std::vector<Rectangle>& solids,
    const std::vector<Rectangle>& ladders, Rectangle playerBounds, bool climbing) {
    auto movementSolids = solids;
    if (!climbing) return movementSolids;
    for (const auto& ladder : ladders) {
        if (!CheckCollisionRecs(playerBounds, ladder)) continue;
        // The player's body can overhang a narrow ladder. Open enough space for
        // that body, rather than letting either floor edge block the climb.
        const float left = std::min(ladder.x, playerBounds.x) - 1.0f;
        const float right = std::max(ladder.x + ladder.width, playerBounds.x + playerBounds.width) + 1.0f;
        std::vector<Rectangle> carved;
        for (const auto& solid : movementSolids) {
            if (!CheckCollisionRecs(solid, ladder)) { carved.push_back(solid); continue; }
            const float leftWidth = std::min(left, solid.x + solid.width) - solid.x;
            if (leftWidth > 0) carved.push_back({solid.x, solid.y, leftWidth, solid.height});
            const float rightX = std::max(right, solid.x);
            const float rightWidth = solid.x + solid.width - rightX;
            if (rightWidth > 0) carved.push_back({rightX, solid.y, rightWidth, solid.height});
        }
        movementSolids = std::move(carved);
    }
    return movementSolids;
}

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
