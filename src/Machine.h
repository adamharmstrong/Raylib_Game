#pragma once

#include "Player.h"
#include "raylib.h"

#include <vector>

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

struct SeeSaw {
    Vector2 pivot;
    float length{220.0f};
    float thickness{16.0f};
    float angle{0.0f};
    float minAngle{-18.0f};
    float maxAngle{18.0f};
    float response{8.0f};
};

struct Chain {
    Vector2 start;
    Vector2 end;
    float spacing{12.0f};
    float scale{1.0f};
    float collisionRadius{7.0f};
    std::vector<Vector2> points;
    std::vector<Vector2> previousPoints;
};

float GetMachinePower(const Winch& winch);
float UpdateWinch(Winch& winch, Player& player, float moveInput, KeyboardKey interactKey, float dt);
void UpdateHangingWeights(std::vector<HangingWeight>& weights, float machinePower, float machinePhase);
void ResetRotaryLatch(RotaryLatch& latch);
void UpdateRotaryLatch(RotaryLatch& latch, const Player& player, KeyboardKey interactKey, float machinePower, float dt);
bool IsRotaryLatchAligned(const RotaryLatch& latch);
bool AreAllRotaryLatchesLatched(const std::vector<RotaryLatch>& latches);
float GetSeeSawSurfaceY(const SeeSaw& seeSaw, float x);
bool IsPointOverSeeSaw(const SeeSaw& seeSaw, float x);
void InitializeChain(Chain& chain);
void UpdateChainPhysics(Chain& chain, const std::vector<Rectangle>& colliders, float dt);
