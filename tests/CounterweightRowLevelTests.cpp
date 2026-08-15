#include "Level.h"
#include "Machine.h"

#include <cassert>
#include <cmath>
#include <iostream>

#ifndef COUNTERWEIGHT_ROW_LEVEL_PATH
#define COUNTERWEIGHT_ROW_LEVEL_PATH "game_data/levels/counterweight_row.level"
#endif

namespace {
    bool NearlyEqual(float first, float second, float tolerance = 0.05f) {
        return std::fabs(first - second) <= tolerance;
    }

    void GetRampTopEndpoints(const Ramp& ramp, Vector2& left, Vector2& right) {
        const float angle = ramp.angle * DEG2RAD;
        const Vector2 axis{std::cos(angle), std::sin(angle)};
        const Vector2 normal{-axis.y, axis.x};
        const float halfLength = ramp.length * 0.5f;
        const float halfThickness = ramp.thickness * 0.5f;
        left = {
            ramp.center.x - axis.x * halfLength - normal.x * halfThickness,
            ramp.center.y - axis.y * halfLength - normal.y * halfThickness
        };
        right = {
            ramp.center.x + axis.x * halfLength - normal.x * halfThickness,
            ramp.center.y + axis.y * halfLength - normal.y * halfThickness
        };
    }
}

int main() {
    const Level level = LoadLevelFromFile(COUNTERWEIGHT_ROW_LEVEL_PATH, {});

    assert(level.script == LevelScript::CounterweightRow);
    assert(level.ramps.size() == 1);

    Vector2 rampLeft{};
    Vector2 rampRight{};
    GetRampTopEndpoints(level.ramps.front(), rampLeft, rampRight);

    // The Level 4 slide must meet the balcony and landing at one unambiguous
    // point. Horizontal overlap creates competing floor heights and makes the
    // player alternate between the ramp and rectangular collision solvers.
    assert(NearlyEqual(rampLeft.x, 370.0f));
    assert(NearlyEqual(rampLeft.y, 180.0f));
    assert(NearlyEqual(rampRight.x, 680.0f));
    assert(NearlyEqual(rampRight.y, 360.0f));
    assert(NearlyEqual(GetRampSurfaceY(level.ramps.front(), 370.0f), 180.0f));
    assert(NearlyEqual(GetRampSurfaceY(level.ramps.front(), 680.0f), 360.0f));

    std::cout << "Counterweight Row level tests passed.\n";
    return 0;
}
