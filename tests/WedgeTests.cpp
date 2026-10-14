#include "Level.h"
#include "ScriptWorld.h"
#include <cmath>
#include <stdexcept>
int main() {
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("Wedge regression"); };
    Level level = LoadLevelFromFile(WEDGE_FIXTURE_PATH, {});
    check(level.ramps.size() == 3);
    const Ramp& wedge = level.ramps[0];
    check(wedge.wedge && !level.ramps[1].wedge && level.ramps[2].wedge);
    check(GetRampSurfaceY(wedge, 180) > GetRampSurfaceY(wedge, 220));
    check(IsPointOverRamp(wedge, 200) && !IsPointOverRamp(wedge, 400));
    check(level.ramps[2].length == 1 && level.ramps[2].thickness == 1);
    check(level.ramps[2].angle == 80 && level.ramps[2].segmentCount == 16);
    Rectangle bounds{};
    Vector2 move{10, 15};
    check(ScriptTarget(level, "wedge", 2, bounds, &move));
    check(level.ramps[2].center.x == 610 && level.ramps[0].center.x == 200);
    check(ScriptTarget(level, "ramp", 1, bounds, nullptr) && bounds.x == 320);
    check(!ScriptTarget(level, "wedge", 3, bounds, nullptr));
}
