#pragma once

#include "Machine.h"
#include "raylib.h"

#include <vector>

struct Level {
    Rectangle ladder{};
    Rectangle spikeHazard{};
    Rectangle darknessArea{};
    Rectangle rightDarknessArea{};
    Rectangle exitTrigger{};

    std::vector<Rectangle> baseSolids;
    std::vector<Rectangle> pitPlatforms;
    std::vector<Vector2> pulleys;
    std::vector<HangingWeight> weights;
    std::vector<RotaryLatch> rotaryLatches;
};

Level CreatePowerPulleyPanicLevel();
std::vector<Rectangle> BuildSolids(const Level& level);
