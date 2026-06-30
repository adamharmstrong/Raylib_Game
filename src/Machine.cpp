#include "Machine.h"

#include "MathUtils.h"

#include <cmath>

float GetMachinePower(const Winch& winch) {
    return Clamp01((winch.rect.x - winch.startX) / (winch.maxX - winch.startX));
}

float UpdateWinch(Winch& winch, Player& player, float moveInput, float dt) {
    float oldX = winch.rect.x;
    winch.grabbed = NearRect(player.rect, winch.rect, 18.0f) && IsKeyDown(KEY_E);

    if (winch.grabbed) {
        winch.rect.x += moveInput * 120.0f * dt;

        if (winch.rect.x < winch.startX) winch.rect.x = winch.startX;
        if (winch.rect.x > winch.maxX) winch.rect.x = winch.maxX;

        if (moveInput != 0.0f) {
            player.rect.x = winch.rect.x - player.rect.width - 4.0f;
        }
    }
    else {
        winch.rect.x -= winch.returnSpeed * dt;
        if (winch.rect.x < winch.startX) {
            winch.rect.x = winch.startX;
        }
    }

    return winch.rect.x - oldX;
}

void UpdateHangingWeights(std::vector<HangingWeight>& weights, float machinePower, float machinePhase) {
    for (HangingWeight& weight : weights) {
        float travel = LerpFloat(28.0f, 88.0f, machinePower);
        float y = 505.0f + sinf(machinePhase * weight.speed + weight.phase) * travel;
        float ropeX = weight.pulley.x + weight.pulleyRadius;
        weight.rect.x = ropeX - weight.rect.width / 2.0f;
        weight.rect.y = y;
    }
}

void ResetRotaryLatch(RotaryLatch& latch) {
    latch.angle = NormalizeAngle(latch.angle);
    latch.powered = false;
    latch.latched = false;
}

void UpdateRotaryLatch(RotaryLatch& latch, const Player& player, float machinePower, float dt) {
    latch.powered = machinePower > 0.05f;

    if (!latch.latched && latch.powered) {
        latch.angle = NormalizeAngle(latch.angle + latch.spinSpeed * machinePower * dt);
    }

    bool playerNear = CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player.rect);
    if (playerNear && IsKeyPressed(KEY_E) && IsRotaryLatchAligned(latch)) {
        latch.latched = true;
    }
}

bool IsRotaryLatchAligned(const RotaryLatch& latch) {
    return IsAngleAligned(latch.angle, latch.targetAngle, latch.tolerance);
}

bool AreAllRotaryLatchesLatched(const std::vector<RotaryLatch>& latches) {
    for (const RotaryLatch& latch : latches) {
        if (!latch.latched) {
            return false;
        }
    }

    return true;
}
