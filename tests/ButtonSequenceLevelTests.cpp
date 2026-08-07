#include "Level.h"

#include <cassert>
#include <cmath>
#include <iostream>

#ifndef BUTTON_SEQUENCE_LEVEL_PATH
#define BUTTON_SEQUENCE_LEVEL_PATH "game_data/levels/wendis_level_1.level"
#endif

namespace {
    bool NearlyEqual(float a, float b, float tolerance = 0.001f) {
        return std::fabs(a - b) <= tolerance;
    }
}

int main() {
    Level level = LoadLevelFromFile(BUTTON_SEQUENCE_LEVEL_PATH, {});

    assert(level.script == LevelScript::ButtonSequence);
    assert(NearlyEqual(level.worldBounds.width, 1600.0f));
    assert(NearlyEqual(level.worldBounds.height, 900.0f));
    assert(level.labels.empty());
    assert(level.buttons.size() == 3);
    assert(level.boulders.size() == 3);
    assert(level.trapDoors.size() == 1);
    assert(level.buttonTrapDoorLinks.size() == 1);
    assert(level.buttonLadderLinks.size() == 1);

    const ButtonTrapDoorLink& trapLink = level.buttonTrapDoorLinks.front();
    assert(trapLink.buttonIndex == 0);
    assert(trapLink.trapDoorIndex == 0);
    assert(NearlyEqual(trapLink.openAngle, 78.0f));
    assert(NearlyEqual(trapLink.speed, 115.0f));

    const ButtonLadderLink& ladderLink = level.buttonLadderLinks.front();
    assert(ladderLink.buttonIndex == 1);
    assert(NearlyEqual(ladderLink.ladder.x, 1110.0f));
    assert(NearlyEqual(ladderLink.ladder.height, 618.0f));
    assert(!ladderLink.activated);
    assert(NearlyEqual(ladderLink.revealProgress, 0.0f));
    Rectangle ladderOpening{
        ladderLink.ladder.x,
        ladderLink.ladder.y,
        ladderLink.ladder.width,
        32.0f
    };
    for (Rectangle solid : level.baseSolids) {
        assert(!CheckCollisionRecs(ladderOpening, solid));
    }
    assert(level.buttonExitLink.buttonIndex == 2);

    const Rectangle buttonOneStop{715.0f, 474.0f, 14.0f, 36.0f};
    bool foundButtonOneStop = false;
    for (Rectangle solid : level.baseSolids) {
        if (NearlyEqual(solid.x, buttonOneStop.x) &&
            NearlyEqual(solid.y, buttonOneStop.y) &&
            NearlyEqual(solid.width, buttonOneStop.width) &&
            NearlyEqual(solid.height, buttonOneStop.height)) {
            foundButtonOneStop = true;
        }
        assert(!(NearlyEqual(solid.x, 170.0f) && NearlyEqual(solid.y, 184.0f)));
    }
    assert(foundButtonOneStop);

    std::cout << "Button-sequence level tests passed.\n";
    return 0;
}
