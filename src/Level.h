#pragma once

#include "Fluid.h"
#include "GuideObjects.h"
#include "Machine.h"
#include "raylib.h"

#include <string>
#include <vector>

enum class LevelScript {
    PowerPulleyPanic,
    RotaryLatchLab,
    FloodedFoundry,
    CounterweightRow,
    ButtonSequence,
    PortalLift,
    WaterEscape,
    NeurotoxinMaze,
    ClocktowerCore,
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

struct ToxinLeak {
    int fluidIndex{-1};
    Vector2 source{};
    float massPerSecond{0.0f};
    float maximumMass{0.0f};
    float exposureRate{0.10f};
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

struct LevelLabel {
    Vector2 position{};
    std::string text{};
    int fontSize{24};
};

struct ButtonTrapDoorLink {
    int buttonIndex{-1};
    int trapDoorIndex{-1};
    float openAngle{75.0f};
    float speed{120.0f};
    bool activated{false};
};

struct ButtonLadderLink {
    int buttonIndex{-1};
    Rectangle ladder{};
    bool activated{false};
    float revealProgress{0.0f};
};

struct ButtonExitLink {
    int buttonIndex{-1};
    bool activated{false};
};

enum class SpikeDirection {
    Up,
    Down,
    Left,
    Right
};

struct DirectionalSpikeHazard {
    Rectangle rect{};
    SpikeDirection direction{SpikeDirection::Up};
};

struct PortalPair {
    Rectangle entrance{};
    Rectangle exit{};
};

struct ButtonFanLink {
    int buttonIndex{-1};
    int fanIndex{-1};
    float poweredAmount{1.0f};
};

struct ButtonPlatformLink {
    int buttonIndex{-1};
    Rectangle platform{};
    bool active{false};
};

struct ButtonSpikeLink {
    int buttonIndex{-1};
    DirectionalSpikeHazard hazard{};
    bool active{false};
};

struct ButtonPlatformLoop {
    int buttonIndex{-1};
    Vector2 center{};
    Vector2 radius{120.0f, 200.0f};
    Vector2 platformSize{80.0f, 28.0f};
    float speed{28.0f};
    float phase{0.0f};
    int platformCount{4};
    bool active{false};
    std::vector<Rectangle> platforms;
};

struct PlatformLoopButtonLink {
    int buttonIndex{-1};
    int loopIndex{-1};
    int platformIndex{-1};
    bool activated{false};
};

struct Level {
    LevelScript script{LevelScript::PowerPulleyPanic};

    Rectangle worldBounds{0.0f, 0.0f, 1600.0f, 900.0f};
    std::vector<Rectangle> cameraZones;

    std::vector<Rectangle> ladders;
    Rectangle spikeHazard{};
    float spikePitTopY{682.0f};
    Rectangle exitTrigger{};
    Vector2 playerStart{80.0f, 600.0f};
    Valve valve{};
    WaterPit waterPit{};
    ValveFluidFill valveFluidFill{};
    ToxinLeak toxinLeak{};
    Vector2 clockFaceCenter{};
    float clockFaceRadius{0.0f};

    std::vector<FluidField> fluids;
    std::vector<Rectangle> darknessAreas;
    std::vector<VisualTile> visualTiles;
    std::vector<LevelLabel> labels;
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
    std::vector<ButtonTrapDoorLink> buttonTrapDoorLinks;
    std::vector<ButtonLadderLink> buttonLadderLinks;
    ButtonExitLink buttonExitLink{};
    std::vector<PortalPair> portalPairs;
    std::vector<ButtonFanLink> buttonFanLinks;
    std::vector<ButtonPlatformLink> buttonPlatformLinks;
    std::vector<ButtonPlatformLoop> buttonPlatformLoops;
    std::vector<PlatformLoopButtonLink> platformLoopButtonLinks;
    std::vector<DirectionalSpikeHazard> directionalSpikeHazards;
    std::vector<ButtonSpikeLink> buttonSpikeLinks;
    std::vector<ArrowTrap> arrowTraps;
    std::vector<BreakableTile> breakableTiles;
    std::vector<GuideObject> guideObjects;
    std::vector<Enemy> enemies;
};

Level CreatePowerPulleyPanicLevel();
Level CreateRotaryLatchLabLevel();
Level CreateFloodedFoundryLevel();
Level LoadLevelFromFile(const std::string& path, Level fallback);
std::vector<Rectangle> BuildSolids(const Level& level);
Vector2 GetButtonPlatformLoopPoint(const ButtonPlatformLoop& loop, float progress);
void UpdateButtonPlatformLoopPositions(ButtonPlatformLoop& loop);
void UpdatePlatformLoopButtonPositions(Level& level);
