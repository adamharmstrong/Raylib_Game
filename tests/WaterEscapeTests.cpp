#include "WaterEscape.h"
#include "Constants.h"
#include "Collision.h"
#include <iostream>
#include <cstdlib>

static void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    Level level = LoadLevelFromFile(WATER_ESCAPE_LEVEL_PATH, {});
    Check(level.script == LevelScript::WaterEscape && level.author == "Kiril", "Wrong level loaded");
    Check(level.trapDoors[0].kind == HingedDoorKind::SimpleHingedDoor &&
        level.trapDoors[1].kind == HingedDoorKind::SimpleHingedDoor, "Kiril doors must be distinct objects");
    Check(level.buttons.size() == 5 && level.buttonTrapDoorLinks.size() == 4 &&
        level.trapDoors.size() == 2 && level.enemies.size() == 4, "Missing authored encounters");
    Check(level.fluids.size() == 1 && GetFluidMass(level.fluids[0]) == 0.0f, "Flood must start dry");
    Check(level.trapDoors[0].angle == 90 && level.trapDoors[1].angle == 0, "Wrong initial door positions");
    for (const TrapDoor& door : level.trapDoors) {
        bool edgeMounted = false;
        for (Rectangle platform : level.pitPlatforms)
            if (door.hinge.x >= platform.x && door.hinge.x <= platform.x + platform.width &&
                std::fabs(door.hinge.y - platform.y - platform.height) < 0.001f) edgeMounted = true;
        Check(edgeMounted, "A hanging door hinge is embedded instead of mounted on the ceiling edge");
    }
    Check(level.buttonTrapDoorLinks[1].openAngle == 90 && level.buttonTrapDoorLinks[3].openAngle == 90,
        "Door targets must retain the full quarter turn when loaded");
    const auto jumpReaches = [](float rise, float gap) {
        const float speed = -Constants::PlayerJumpVelocity;
        const float discriminant = speed * speed - 2.0f * Constants::Gravity * rise;
        return discriminant >= 0 && gap <= Constants::PlayerSpeed *
            (speed + std::sqrt(discriminant)) / Constants::Gravity;
    };
    const float boulderTop = level.boulders[0].center.y - level.boulders[0].radius;
    Check(!jumpReaches(level.pitPlatforms[1].y - level.pitPlatforms[2].y, 0) &&
        jumpReaches(boulderTop - level.pitPlatforms[2].y, 50),
        "Floor four must require the boulder's extra height");
    Check(jumpReaches(level.pitPlatforms[7].y - level.pitPlatforms[9].y, 460 - 362) &&
        jumpReaches(level.pitPlatforms[9].y - level.pitPlatforms[11].y, 500 - 362),
        "Upper shelves exceed the player's jump arc");
    // Test every enemy lane against its actual ceiling, including moving enemies.
    for (size_t enemyIndex = 0; enemyIndex < level.enemies.size(); ++enemyIndex) {
        const Enemy& enemy = level.enemies[enemyIndex];
        const size_t floorIndex = enemyIndex == 0 ? 0 : (enemyIndex == 3 ? 3 : 1);
        Player jumper{};
        jumper.rect = {enemy.rect.x, level.pitPlatforms[floorIndex].y - 40, 31, 40};
        jumper.velocity.y = Constants::PlayerJumpVelocity;
        float clearSeconds = 0;
        constexpr float step = 1.0f / 240.0f;
        for (int i = 0; i < 240; ++i) {
            jumper.velocity.y += Constants::Gravity * step;
            jumper.rect.y += jumper.velocity.y * step;
            ResolveVertical(jumper, level.pitPlatforms);
            if (jumper.rect.y + jumper.rect.height <= enemy.rect.y - 4) clearSeconds += step;
            if (jumper.onGround) break;
        }
        Check(clearSeconds * (Constants::PlayerSpeed - enemy.speed) >=
            jumper.rect.width + enemy.rect.width + 8,
            "An enemy lane has too little ceiling clearance to jump over its enemy");
    }
    WaterEscapeState state;
    UpdateWaterEscape(level, state, {{520, 810, 31, 40}}, 0.1f);
    Check(!state.flooding && !level.buttonExitLink.activated, "Flood or exit started early");
    UpdateWaterEscape(level, state, {{800, 810, 31, 40}}, 0.1f);
    Check(state.flooding, "Crossing floor-one trigger did not start flood");
    UpdateWaterEscape(level, state, {}, 0.1f);
    Check(state.flooding, "Flood trigger failed to latch");

    level.buttons[0].pressed = true;
    UpdateWaterEscape(level, state, {}, 1.0f);
    Check(level.trapDoors[0].angle == 0.0f, "Right button did not raise first door");
    level.buttons[0].pressed = false;
    level.buttons[1].pressed = true;
    UpdateWaterEscape(level, state, {}, 1.0f);
    Check(level.trapDoors[0].angle == 90.0f, "Left button did not drop first door");
    Check(WaterEscapeLeakMultiplier(level, state) > 0.0f &&
        WaterEscapeLeakMultiplier(level, state) < 1.0f, "Barrier must slow but never stop flood");
    for (int i = 0; i < 6; ++i) UpdateWaterEscape(level, state, {}, 1.0f);
    Check(WaterEscapeLeakMultiplier(level, state) == 1.0f, "Held button extended barrier indefinitely");
    level.buttons[1].pressed = false;
    UpdateWaterEscape(level, state, {}, 0.1f);
    level.buttons[1].pressed = true;
    level.buttons[3].pressed = true;
    UpdateWaterEscape(level, state, {}, 1.0f);
    Check(WaterEscapeLeakMultiplier(level, state) < 0.55f, "Both barriers should work independently");
    level.buttons[2].pressed = true;
    UpdateWaterEscape(level, state, {}, 1.0f);
    Check(level.trapDoors[1].angle == 0 && state.barrierSeconds[1] == 0, "Raising a barrier must cancel its effect");

    level.buttons[4].pressed = true;
    UpdateWaterEscape(level, state, {}, 0.1f);
    Check(level.buttonExitLink.activated && state.exitSeconds == 6.0f, "Exit release failed");
    for (int i = 0; i < 7; ++i) UpdateWaterEscape(level, state, {}, 1.0f);
    Check(!level.buttonExitLink.activated, "Held release kept exit open indefinitely");
    level.buttons[4].pressed = false;
    UpdateWaterEscape(level, state, {}, 0.1f);
    level.buttons[4].pressed = true;
    UpdateWaterEscape(level, state, {}, 0.1f);
    Check(level.buttonExitLink.activated, "Exit cannot be retried");
    level = LoadLevelFromFile(WATER_ESCAPE_LEVEL_PATH, {});
    state = {};
    UpdateWaterEscape(level, state, {{520, 810, 31, 40}}, 0.1f);
    Check(!state.flooding && state.exitSeconds == 0 && WaterEscapeLeakMultiplier(level, state) == 1,
        "Restart did not restore initial state");
    Level mixed;
    mixed.buttons.resize(2);
    TrapDoor oldDoor{}; oldDoor.angle = 12;
    TrapDoor newDoor{}; newDoor.kind = HingedDoorKind::SimpleHingedDoor;
    mixed.trapDoors = {oldDoor, newDoor};
    mixed.buttonTrapDoorLinks = {{0, 0, 76, 150}, {1, 1, 76, 30}};
    mixed.buttons[0].pressed = mixed.buttons[1].pressed = true;
    state = {};
    UpdateSimpleHingedDoors(mixed, state, 1.0f);
    Check(mixed.trapDoors[0].angle == 12 && mixed.trapDoors[1].angle == 30,
        "New controls must respect speed and leave old trap doors untouched");
    std::cout << "Water escape checks passed.\n";
}
