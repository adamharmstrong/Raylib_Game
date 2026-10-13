#include "Collision.h"
#include <stdexcept>
#include <iostream>
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    const std::vector<Rectangle> floors{{0, 100, 400, 32}, {500, 100, 200, 32}};
    const std::vector<Rectangle> ladders{{100, 40, 16, 200}, {550, 40, 32, 200}};
    for (float x : {85.0f, 100.0f, 110.0f}) {
        Player player{}; player.rect = {x, 110, 31, 40}; player.velocity.y = -100;
        auto climb = BuildLadderMovementSolids(floors, ladders, player.rect, true);
        ResolveVertical(player, climb);
        Check(player.rect.y == 110 && player.velocity.y == -100, "Ascending a narrow ladder must pass through its floor even with body overhang.");
        player.velocity.y = 100; ResolveVertical(player, climb);
        Check(player.rect.y == 110 && player.velocity.y == 100 && !player.onGround, "Descending must also pass through the floor.");
        player.velocity.x = 50; ResolveHorizontal(player, climb);
        Check(player.rect.x == x && player.velocity.x == 50, "Floor overlap must not push a climbing player sideways.");
        Player other{}; other.rect = {540, 110, 31, 40}; other.velocity.y = -100;
        ResolveVertical(other, climb);
        Check(other.rect.y == 132 && other.velocity.y == 0, "An unused ladder must not open another floor.");
    }
    Player walking{}; walking.rect = {100, 110, 31, 40}; walking.velocity.y = -100;
    auto normal = BuildLadderMovementSolids(floors, ladders, walking.rect, false);
    ResolveVertical(walking, normal);
    Check(walking.rect.y == 132 && walking.velocity.y == 0, "Being near a ladder without climbing must retain ceiling collision.");
    walking.rect = {100, 80, 31, 40}; walking.velocity.y = 100;
    ResolveVertical(walking, normal);
    Check(walking.rect.y == 60 && walking.onGround, "Ordinary landing must retain floor collision.");
    Player away{}; away.rect = {300, 110, 31, 40}; away.velocity.y = -100;
    ResolveVertical(away, BuildLadderMovementSolids(floors, ladders, away.rect, true));
    Check(away.rect.y == 132, "The exception must end once the player leaves the ladder.");
    Player revealed{}; revealed.rect = {100, 110, 31, 40}; revealed.velocity.y = -100;
    ResolveVertical(revealed, BuildLadderMovementSolids(floors, {{100, 40, 16, 200}}, revealed.rect, true));
    Check(revealed.velocity.y == -100, "A revealed button-controlled ladder must allow the same pass-through.");
    revealed.rect = {100, 110, 31, 40}; revealed.velocity.y = -100;
    ResolveVertical(revealed, BuildLadderMovementSolids(floors, {{100, 140, 16, 100}}, revealed.rect, true));
    Check(revealed.rect.y == 132, "A partly revealed ladder must not open a floor it has not reached.");
    revealed.rect = {100, 110, 31, 40}; revealed.velocity.y = -100;
    ResolveVertical(revealed, BuildLadderMovementSolids(floors, {}, revealed.rect, true));
    Check(revealed.rect.y == 132, "An inactive ladder must not grant pass-through.");
    Check(floors.size() == 2 && floors[0].width == 400, "Player collision filtering must never change level floors.");
    std::cout << "Ladder pass-through tests passed: ascent, descent, overhang, side motion, ordinary collision, local scope.\n";
}
