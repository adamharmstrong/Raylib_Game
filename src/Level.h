#pragma once

#include "Machine.h"
#include "raylib.h"

#include <string>
#include <vector>

enum class LevelScript {
    PowerPulleyPanic,
    RotaryLatchLab
};

struct Level {
    LevelScript script{LevelScript::PowerPulleyPanic};

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
Level CreateRotaryLatchLabLevel();
Level LoadLevelFromFile(const std::string& path, Level fallback);
std::vector<Rectangle> BuildSolids(const Level& level);
