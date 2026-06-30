#include "Level.h"

Level CreatePowerPulleyPanicLevel() {
    Level level;

    level.ladder = {255, 250, 45, 400};
    level.spikeHazard = {300, 773, 960, 32};
    level.darknessArea = {300, 275, 960, 625};
    level.rightDarknessArea = {1260, 0, 340, 900};
    level.exitTrigger = {1485, 430, 85, 220};

    level.baseSolids = {
        {0, 650, 300, 40},
        {300, 250, 960, 25},
        {1238, 0, 22, 275},
        {1260, 650, 340, 40},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    level.pitPlatforms = {
        {420, 570, 70, 18},
        {590, 520, 70, 18},
        {760, 570, 70, 18},
        {930, 515, 70, 18},
        {1100, 570, 70, 18}
    };

    level.pulleys = {
        {650, 210},
        {820, 210},
        {990, 210},
        {1160, 210},
        {1430, 515}
    };

    level.weights = {
        {level.pulleys[1], 45.0f, 0.0f, 1.00f, {0, 0, 44, 55}},
        {level.pulleys[2], 45.0f, 1.6f, 1.25f, {0, 0, 44, 55}},
        {level.pulleys[3], 45.0f, 3.2f, 1.10f, {0, 0, 44, 55}}
    };

    return level;
}

std::vector<Rectangle> BuildSolids(const Level& level) {
    std::vector<Rectangle> solids = level.baseSolids;

    for (Rectangle platform : level.pitPlatforms) {
        solids.push_back(platform);
    }

    return solids;
}
