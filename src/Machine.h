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

float GetMachinePower(const Winch& winch);
float UpdateWinch(Winch& winch, Player& player, float moveInput, float dt);
void UpdateHangingWeights(std::vector<HangingWeight>& weights, float machinePower, float machinePhase);
void ResetRotaryLatch(RotaryLatch& latch);
void UpdateRotaryLatch(RotaryLatch& latch, const Player& player, float machinePower, float dt);
bool IsRotaryLatchAligned(const RotaryLatch& latch);
bool AreAllRotaryLatchesLatched(const std::vector<RotaryLatch>& latches);
