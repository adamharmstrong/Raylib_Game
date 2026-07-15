#pragma once

#include "Fluid.h"
#include "Machine.h"
#include "raylib.h"

#include <string>
#include <vector>

enum class LevelScript {
    PowerPulleyPanic,
    RotaryLatchLab,
    FloodedFoundry,
    CounterweightRow,
    TilesetReference
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
    float turnDegrees{0.0f};
    float turnSpeed{120.0f};
    bool opened{false};
};

struct WaterPit {
    Rectangle bounds{};
    float surfaceY{0.0f};
    float targetSurfaceY{0.0f};
    float fillRate{95.0f};
    bool filling{false};
};

struct ValveFluidFill {
    int fluidIndex{-1};
    float targetFill{1.0f};
    float riseRate{0.0f};
};

enum class TileLayer {
    FarBackground,
    Background,
    Foreground
};

struct VisualTile {
    TileLayer layer{TileLayer::Foreground};
    int column{0};
    int row{0};
    Vector2 position{};
};

struct Level {
    LevelScript script{LevelScript::PowerPulleyPanic};

    Rectangle ladder{};
    Rectangle spikeHazard{};
    Rectangle exitTrigger{};
    Vector2 playerStart{80.0f, 600.0f};
    Valve valve{};
    WaterPit waterPit{};
    ValveFluidFill valveFluidFill{};

    std::vector<FluidField> fluids;
    std::vector<Rectangle> darknessAreas;
    std::vector<VisualTile> visualTiles;
    std::vector<Rectangle> baseSolids;
    std::vector<Rectangle> pitPlatforms;
    std::vector<Vector2> pulleys;
    std::vector<HangingWeight> weights;
    std::vector<RotaryLatch> rotaryLatches;
    std::vector<StoneBlock> stoneBlocks;
    std::vector<Boulder> boulders;
    std::vector<PhysicsWheel> physicsWheels;
    std::vector<Gear> gears;
    std::vector<Flywheel> flywheels;
    std::vector<SteeringWheel> steeringWheels;
    std::vector<Screw> screws;
    std::vector<Fan> fans;
    std::vector<Pinwheel> pinwheels;
    std::vector<Ramp> ramps;
    std::vector<TrapDoor> trapDoors;
    std::vector<SeeSaw> seeSaws;
    std::vector<Chain> chains;
    std::vector<PhysicsRope> physicsRopes;
    std::vector<Button> buttons;
    std::vector<ArrowTrap> arrowTraps;
    std::vector<BreakableTile> breakableTiles;
    std::vector<Enemy> enemies;
};

Level CreatePowerPulleyPanicLevel();
Level CreateRotaryLatchLabLevel();
Level CreateFloodedFoundryLevel();
Level LoadLevelFromFile(const std::string& path, Level fallback);
std::vector<Rectangle> BuildSolids(const Level& level);
