#pragma once

#include "Machine.h"
#include "raylib.h"

#include <string>
#include <vector>

enum class LevelScript {
    PowerPulleyPanic,
    RotaryLatchLab,
    FloodedFoundry
};

struct Enemy {
    Rectangle rect{};
    Vector2 velocity{};
    float patrolMinX{0.0f};
    float patrolMaxX{0.0f};
    float speed{80.0f};
    bool onGround{false};
    bool facingRight{false};
    bool walking{true};
};

struct Valve {
    Vector2 center{};
    float radius{34.0f};
    bool opened{false};
};

struct WaterPit {
    Rectangle bounds{};
    float surfaceY{0.0f};
    float targetSurfaceY{0.0f};
    float fillRate{95.0f};
    bool filling{false};
};

struct Level {
    LevelScript script{LevelScript::PowerPulleyPanic};

    Rectangle ladder{};
    Rectangle spikeHazard{};
    Rectangle darknessArea{};
    Rectangle rightDarknessArea{};
    Rectangle exitTrigger{};
    Valve valve{};
    WaterPit waterPit{};

    std::vector<Rectangle> baseSolids;
    std::vector<Rectangle> pitPlatforms;
    std::vector<Vector2> pulleys;
    std::vector<HangingWeight> weights;
    std::vector<RotaryLatch> rotaryLatches;
    std::vector<StoneBlock> stoneBlocks;
    std::vector<SeeSaw> seeSaws;
    std::vector<Chain> chains;
    std::vector<Enemy> enemies;
};

Level CreatePowerPulleyPanicLevel();
Level CreateRotaryLatchLabLevel();
Level CreateFloodedFoundryLevel();
Level LoadLevelFromFile(const std::string& path, Level fallback);
std::vector<Rectangle> BuildSolids(const Level& level);
