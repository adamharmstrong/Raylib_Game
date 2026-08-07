#include "Level.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

#ifndef PORTAL_LIFT_LEVEL_PATH
#define PORTAL_LIFT_LEVEL_PATH "game_data/levels/wendis_level_2.level"
#endif

namespace {
    bool ContainsRectangle(const std::vector<Rectangle>& rectangles, Rectangle expected) {
        return std::any_of(rectangles.begin(), rectangles.end(), [&](Rectangle rect) {
            return rect.x == expected.x && rect.y == expected.y &&
                rect.width == expected.width && rect.height == expected.height;
        });
    }
}

int main() {
    Level level = LoadLevelFromFile(PORTAL_LIFT_LEVEL_PATH, {});

    assert(level.script == LevelScript::PortalLift);
    assert(level.worldBounds.width == 1600.0f);
    assert(level.worldBounds.height == 900.0f);
    assert(level.portalPairs.size() == 1);
    assert(level.boulders.size() == 1);
    assert(level.buttons.size() == 2);
    assert(level.fans.size() == 1);
    assert(level.fans.front().power == 0.0f);
    assert(level.buttonFanLinks.size() == 1);
    assert(level.buttonFanLinks.front().buttonIndex == 1);
    assert(level.buttonFanLinks.front().fanIndex == 0);
    assert(level.buttonExitLink.buttonIndex == 1);
    assert(level.buttonPlatformLinks.empty());
    assert(level.buttonSpikeLinks.empty());
    assert(level.buttonPlatformLoops.size() == 1);
    assert(level.buttonPlatformLoops.front().platformCount == 6);
    assert(level.buttonPlatformLoops.front().platforms.size() == 6);
    assert(level.buttonPlatformLoops.front().buttonIndex < 0);
    assert(level.buttonPlatformLoops.front().active);
    assert(level.platformLoopButtonLinks.size() == 1);
    assert(level.platformLoopButtonLinks.front().buttonIndex == 1);
    assert(level.platformLoopButtonLinks.front().loopIndex == 0);
    assert(level.platformLoopButtonLinks.front().platformIndex == 2);
    assert(level.directionalSpikeHazards.size() == 1);
    assert(level.directionalSpikeHazards.front().direction == SpikeDirection::Down);
    assert(level.darknessAreas.size() == 1);
    assert(level.pitPlatforms.size() == 7);
    assert(level.enemies.size() == 1);
    assert(level.ladders.front().x + level.ladders.front().width <=
        level.pitPlatforms[1].x);

    const PortalPair& portal = level.portalPairs.front();
    const Ramp& arrivalRamp = level.ramps.front();
    const float portalExitCenterX = portal.exit.x + portal.exit.width * 0.5f;
    const Rectangle chamberCeiling{458.0f, 160.0f, 302.0f, 28.0f};
    assert(ContainsRectangle(level.baseSolids, chamberCeiling));
    assert(level.darknessAreas.front().x == chamberCeiling.x);
    assert(level.darknessAreas.front().y == chamberCeiling.y + chamberCeiling.height);
    assert(level.darknessAreas.front().width == chamberCeiling.width);
    assert(arrivalRamp.segmentCount == 3);
    const float rampAngle = arrivalRamp.angle * DEG2RAD;
    const float rampLeft = arrivalRamp.center.x -
        cosf(rampAngle) * arrivalRamp.length * 0.5f;
    const float rampRight = arrivalRamp.center.x + arrivalRamp.length * 0.5f;
    assert(portalExitCenterX > rampLeft + level.boulders.front().radius);
    assert(portalExitCenterX < rampRight - level.boulders.front().radius);
    assert(rampLeft < chamberCeiling.x + chamberCeiling.width);
    assert(rampLeft > chamberCeiling.x + chamberCeiling.width - 30.0f);
    assert(level.buttons.front().rect.x - rampRight > 150.0f);

    ButtonPlatformLoop& loop = level.buttonPlatformLoops.front();
    const Rectangle temporaryPlatform = loop.platforms.front();
    assert(ContainsRectangle(BuildSolids(level), temporaryPlatform));
    loop.phase += 90.0f;
    UpdateButtonPlatformLoopPositions(loop);
    const Rectangle oldMovingButton = level.buttons[1].rect;
    UpdatePlatformLoopButtonPositions(level);
    assert(!ContainsRectangle(loop.platforms, temporaryPlatform));
    assert(level.buttons[1].rect.x != oldMovingButton.x ||
        level.buttons[1].rect.y != oldMovingButton.y);

    std::cout << "Portal Lift level tests passed.\n";
    return 0;
}
