#pragma once

#include "Player.h"
#include "raylib.h"

#include <array>
#include <vector>

inline constexpr int GearToothCount = 8;
inline constexpr float GearOuterRadiusScale = 1.12f;

struct Winch {
    Rectangle rect;
    float startX;
    float maxX;
    float returnSpeed;
    bool grabbed{false};
};

struct HangingWeight {
    Vector2 pulley;
    float pulleyRadius;
    float phase;
    float speed;
    Rectangle rect;
};

struct RotaryLatch {
    Vector2 center;
    float radius;
    float angle;
    float targetAngle;
    float tolerance;
    float spinSpeed;
    bool powered{false};
    bool latched{false};
};

struct StoneBlock {
    Rectangle rect;
    Vector2 velocity{0, 0};
    float mass{2.5f};
    bool onGround{false};
};

struct Boulder {
    Vector2 center;
    Vector2 velocity{0, 0};
    float radius{24.0f};
    float mass{2.5f};
    float rotation{0.0f};
    float angularVelocity{0.0f};
    bool onGround{false};
};

struct PhysicsWheel {
    Vector2 center;
    Vector2 velocity{0, 0};
    float radius{24.0f};
    float mass{1.4f};
    float rotation{0.0f};
    float angularVelocity{0.0f};
    bool onGround{false};
};

struct Gear {
    Vector2 center;
    Vector2 velocity{0, 0};
    float radius{24.0f};
    float mass{1.8f};
    float rotation{0.0f};
    float angularVelocity{0.0f};
    bool onGround{false};
};

struct Flywheel {
    Vector2 center;
    Vector2 velocity{0, 0};
    float radius{34.0f};
    float mass{4.0f};
    float rotation{0.0f};
    float angularVelocity{0.0f};
    bool onGround{false};
};

struct SteeringWheel {
    Vector2 center;
    float radius{28.0f};
    float rotation{0.0f};
};

struct Screw {
    Vector2 center;
    float length{160.0f};
    float radius{14.0f};
    float angle{0.0f};
    float spinSpeed{180.0f};
    float rotation{0.0f};
};

struct Fan {
    Vector2 center;
    Vector2 direction{1.0f, 0.0f};
    float length{220.0f};
    float width{90.0f};
    float strength{360.0f};
    float power{1.0f};
    float rotation{0.0f};
};

struct Pinwheel {
    Vector2 center;
    float radius{24.0f};
    float rotation{0.0f};
    float angularVelocity{0.0f};
};

struct SeeSaw {
    Vector2 pivot;
    float length{220.0f};
    float thickness{16.0f};
    float angle{0.0f};
    float angularVelocity{0.0f};
    float minAngle{-18.0f};
    float maxAngle{18.0f};
    float response{8.0f};
};

struct Ramp {
    Vector2 center;
    float length{160.0f};
    float thickness{18.0f};
    float angle{0.0f};
};

struct TrapDoor {
    Vector2 hinge;
    float length{120.0f};
    float thickness{16.0f};
    float angle{0.0f};
};

enum class FlexibleAnchorType {
    None,
    TrapDoorRing
};

struct FlexibleEndpointBinding {
    FlexibleAnchorType anchorType{FlexibleAnchorType::None};
    int objectIndex{-1};
    int pointIndex{-1};
    int carriedByPlayer{-1};
};

struct Chain {
    Vector2 start;
    Vector2 end;
    float spacing{12.0f};
    float scale{1.0f};
    float collisionRadius{7.0f};
    bool pinStart{true};
    bool pinEnd{true};
    FlexibleEndpointBinding startBinding{};
    FlexibleEndpointBinding endBinding{};
    float previousTimeStep{1.0f / 60.0f};
    std::vector<Vector2> points;
    std::vector<Vector2> previousPoints;
};

struct PhysicsRope {
    Vector2 start;
    Vector2 end;
    float length{120.0f};
    float thickness{4.0f};
    bool pinStart{true};
    bool pinEnd{true};
    FlexibleEndpointBinding startBinding{};
    FlexibleEndpointBinding endBinding{};
    float segmentLength{6.0f};
    float previousTimeStep{1.0f / 60.0f};
    std::vector<Vector2> points;
    std::vector<Vector2> previousPoints;
};

struct Button {
    Rectangle rect;
    bool pressed{false};
};

struct ArrowProjectile {
    Rectangle rect;
    Vector2 velocity{0, 0};
    bool active{false};
};

struct ArrowTrap {
    Vector2 position;
    Vector2 direction{1.0f, 0.0f};
    float interval{1.5f};
    float speed{420.0f};
    float timer{0.0f};
    std::vector<ArrowProjectile> arrows;
};

struct BreakableDebris {
    Rectangle rect;
    Vector2 velocity{0.0f, 0.0f};
    float life{0.8f};
    float maxLife{0.8f};
};

struct BreakableTile {
    Rectangle rect;
    float breakDelay{2.0f};
    float crackTimer{0.0f};
    bool cracking{false};
    bool broken{false};
    std::vector<BreakableDebris> debris;
};

float GetMachinePower(const Winch& winch);
float UpdateWinch(Winch& winch, Player& player, float moveInput, bool interactHeld, float dt);
void UpdateHangingWeights(std::vector<HangingWeight>& weights, float machinePower, float machinePhase);
void ResetRotaryLatch(RotaryLatch& latch);
void AdvanceRotaryLatch(RotaryLatch& latch, float machinePower, float dt);
void TryLockRotaryLatch(RotaryLatch& latch, const Player& player, bool interactPressed);
bool IsRotaryLatchAligned(const RotaryLatch& latch);
bool AreAllRotaryLatchesLatched(const std::vector<RotaryLatch>& latches);
float GetSeeSawSurfaceY(const SeeSaw& seeSaw, float x);
bool IsPointOverSeeSaw(const SeeSaw& seeSaw, float x);
float GetRampSurfaceY(const Ramp& ramp, float x);
bool IsPointOverRamp(const Ramp& ramp, float x);
Vector2 GetTrapDoorRingPosition(const TrapDoor& trapDoor);
std::array<Vector2, 2> GetTrapDoorRingPositions(const TrapDoor& trapDoor);
float GetTrapDoorSurfaceY(const TrapDoor& trapDoor, float x);
bool IsPointOverTrapDoor(const TrapDoor& trapDoor, float x);
void ApplyScrewGearCoupling(Gear& gear, const std::vector<Screw>& screws, float dt);
void ResolveDynamicBodyCollisions(
    std::vector<StoneBlock>& stoneBlocks,
    std::vector<Boulder>& boulders,
    std::vector<PhysicsWheel>& physicsWheels,
    std::vector<Gear>& gears,
    std::vector<Flywheel>& flywheels
);
void InitializeChain(Chain& chain);
void UpdateChainPhysics(
    Chain& chain,
    const std::vector<Rectangle>& colliders,
    const std::vector<Vector2>& externalAccelerations,
    float dt
);
void InitializePhysicsRope(PhysicsRope& rope);
void UpdatePhysicsRope(
    PhysicsRope& rope,
    const std::vector<Rectangle>& colliders,
    const std::vector<Vector2>& externalAccelerations,
    float dt
);
