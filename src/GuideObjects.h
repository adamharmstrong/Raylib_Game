#pragma once

#include "Player.h"
#include "WorldLayer.h"
#include "raylib.h"

#include <array>
#include <sstream>
#include <string>
#include <vector>

enum class BodyType {
    Static,
    Kinematic,
    Dynamic
};

enum class ColliderShape {
    None,
    Rectangle,
    Circle,
    Segment
};

enum class ConstraintKind {
    None,
    Distance,
    Spring,
    Hinge,
    Slider,
    Fixed
};

struct Transform2D {
    Vector2 position{};
    float rotation{0.0f};
    Vector2 scale{1.0f, 1.0f};
};

struct RigidBody2D {
    BodyType type{BodyType::Static};
    Vector2 velocity{};
    Vector2 force{};
    float angularVelocity{0.0f};
    float torque{0.0f};
    float mass{1.0f};
    float inverseMass{0.0f};
    float linearDamping{0.4f};
    float angularDamping{0.4f};
    bool affectedByGravity{false};
    bool onGround{false};
};

struct Collider2D {
    ColliderShape shape{ColliderShape::None};
    Vector2 size{};
    float radius{0.0f};
    float friction{0.7f};
    float restitution{0.0f};
    bool isTrigger{false};
    unsigned int layer{1};
    unsigned int mask{0xffffffffu};
};

struct Constraint2D {
    ConstraintKind kind{ConstraintKind::None};
    Vector2 anchorA{};
    Vector2 anchorB{};
    float restLength{0.0f};
    float stiffness{8.0f};
    float damping{1.0f};
    float minimum{0.0f};
    float maximum{0.0f};
    bool enabled{true};
};

struct SensorComponent {
    Rectangle bounds{};
    float threshold{0.0f};
    int channel{0};
    bool active{false};
};

struct PowerComponent {
    int channel{0};
    float currentPower{0.0f};
    float requiredPower{0.0f};
    float maximumPower{1.0f};
    bool powered{false};
};

enum class GuideObjectType {
    Ball,
    Barrel,
    MovingPlatform,
    Elevator,
    PendulumBob,
    OneWayPlatform,
    CeilingHook,
    GuideRail,
    Spring,
    CompressionSpring,
    ExtensionSpring,
    TorsionSpring,
    GarterSpring,
    VoluteSpring,
    SpiralSpring,
    ConstantForceSpring,
    ConstantTorqueSpring,
    LeafSpring,
    BeamSpring,
    DiscSpring,
    WaveSpring,
    WaveWasher,
    TorsionBar,
    RingSpring,
    ElastomerSpring,
    PneumaticSpring,
    GasSpring,
    HydropneumaticSpring,
    MagneticSpring,
    CompositeSpring,
    Rod,
    FixedJoint,
    Crank,
    Ratchet,
    Clutch,
    Brake,
    Cam,
    ConveyorBelt,
    Turntable,
    Battery,
    ElectricMotor,
    Generator,
    LimitSwitch,
    Relay,
    Fuse,
    Magnet,
    Piston,
    HydraulicCylinder,
    RocketThruster,
    CrushingBlock,
    SwingingHammer,
    SawBlade,
    SpinnerTrap,
    SteamVent,
    ElectricalArc,
    Oil,
    Mud,
    SpeedSensor,
    BeamSensor,
    Checkpoint,
    Collectible,
    Key,
    BreakableCrate,
    ExplosiveBarrel
};

struct GuideObject {
    GuideObjectType type{GuideObjectType::Ball};
    Transform2D transform{};
    RigidBody2D body{};
    Collider2D collider{};
    Constraint2D constraint{};
    SensorComponent sensor{};
    PowerComponent power{};
    float mechanicalInputSpeed{0.0f};
    float ratedMechanicalSpeed{180.0f};
    float generatorEfficiency{0.85f};
    float mechanicalLoad{0.0f};
    WorldLayer layer{WorldLayer::Middleground};
    Vector2 origin{};
    Vector2 direction{1.0f, 0.0f};
    Vector2 previousPosition{};
    float length{0.0f};
    float width{0.0f};
    float speed{0.0f};
    float strength{0.0f};
    float phase{0.0f};
    float timer{0.0f};
    float interval{1.0f};
    int attachedObject{-1};
    bool active{true};
    bool triggered{false};
    bool collected{false};
    bool broken{false};
    bool engaged{true};
};

bool ParseGuideObject(const std::string& command, std::istringstream& stream, GuideObject& object);
void InitializeGuideObject(GuideObject& object);
Rectangle GetGuideObjectBounds(const GuideObject& object);
void AppendGuideObjectSolids(std::vector<Rectangle>& solids, const std::vector<GuideObject>& objects);
void UpdateGuideObjects(
    std::vector<GuideObject>& objects,
    const std::array<Player*, 4>& players,
    const std::array<std::vector<Rectangle>, WorldLayerCount>& worldSolidsByLayer,
    float gravity,
    float dt,
    Vector2& checkpoint
);
bool IsGuideObjectHazardTouchingPlayer(const GuideObject& object, Rectangle playerRect);
void DrawGuideObject(const GuideObject& object);
const char* GetGuideObjectName(GuideObjectType type);
const char* GetGuideObjectDescription(GuideObjectType type);
