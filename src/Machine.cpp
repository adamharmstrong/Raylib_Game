#include "Machine.h"

#include "MathUtils.h"

#include <algorithm>
#include <cmath>

namespace {
    float Distance(Vector2 a, Vector2 b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    Vector2 LerpVector(Vector2 start, Vector2 end, float amount) {
        return {
            LerpFloat(start.x, end.x, amount),
            LerpFloat(start.y, end.y, amount)
        };
    }

    float GetChainSegmentLength(const Chain& chain) {
        return fmaxf(1.0f, chain.spacing * chain.scale);
    }

    void PinChainEnds(Chain& chain) {
        if (chain.points.empty()) {
            return;
        }

        chain.points.front() = chain.start;
        chain.points.back() = chain.end;
    }

    void CollideChainPointWithRect(Vector2& point, Vector2& previousPoint, Rectangle rect, float radius) {
        float closestX = std::clamp(point.x, rect.x, rect.x + rect.width);
        float closestY = std::clamp(point.y, rect.y, rect.y + rect.height);
        float dx = point.x - closestX;
        float dy = point.y - closestY;
        float distanceSquared = dx * dx + dy * dy;

        if (distanceSquared > radius * radius) {
            return;
        }

        if (distanceSquared <= 0.0001f) {
            float left = fabsf(point.x - rect.x);
            float right = fabsf((rect.x + rect.width) - point.x);
            float top = fabsf(point.y - rect.y);
            float bottom = fabsf((rect.y + rect.height) - point.y);
            float nearest = fminf(fminf(left, right), fminf(top, bottom));

            if (nearest == left) {
                point.x = rect.x - radius;
            }
            else if (nearest == right) {
                point.x = rect.x + rect.width + radius;
            }
            else if (nearest == top) {
                point.y = rect.y - radius;
            }
            else {
                point.y = rect.y + rect.height + radius;
            }
        }
        else {
            float distance = sqrtf(distanceSquared);
            float push = radius - distance;
            point.x += (dx / distance) * push;
            point.y += (dy / distance) * push;
        }

        previousPoint.x = LerpFloat(previousPoint.x, point.x, 0.18f);
        previousPoint.y = LerpFloat(previousPoint.y, point.y, 0.18f);
    }
}

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

float GetSeeSawSurfaceY(const SeeSaw& seeSaw, float x) {
    float halfLength = seeSaw.length * 0.5f;
    float localX = x - seeSaw.pivot.x;
    if (localX < -halfLength) localX = -halfLength;
    if (localX > halfLength) localX = halfLength;

    return seeSaw.pivot.y + tanf(seeSaw.angle * DEG2RAD) * localX - seeSaw.thickness * 0.5f;
}

bool IsPointOverSeeSaw(const SeeSaw& seeSaw, float x) {
    float halfLength = seeSaw.length * 0.5f;
    return x >= seeSaw.pivot.x - halfLength && x <= seeSaw.pivot.x + halfLength;
}

void InitializeChain(Chain& chain) {
    float segmentLength = GetChainSegmentLength(chain);
    float spanLength = Distance(chain.start, chain.end);
    int pointCount = std::max(2, static_cast<int>(ceilf(spanLength / segmentLength)) + 1);

    chain.points.clear();
    chain.previousPoints.clear();
    chain.points.reserve(pointCount);
    chain.previousPoints.reserve(pointCount);

    for (int i = 0; i < pointCount; i++) {
        float amount = pointCount == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(pointCount - 1);
        Vector2 point = LerpVector(chain.start, chain.end, amount);
        chain.points.push_back(point);
        chain.previousPoints.push_back(point);
    }

    PinChainEnds(chain);
}

void UpdateChainPhysics(Chain& chain, const std::vector<Rectangle>& colliders, float dt) {
    if (chain.points.size() < 2 || chain.previousPoints.size() != chain.points.size()) {
        InitializeChain(chain);
    }

    float radius = fmaxf(2.0f, chain.collisionRadius * chain.scale);
    float segmentLength = GetChainSegmentLength(chain);
    float accelerationY = 900.0f * dt * dt;

    PinChainEnds(chain);

    for (int i = 1; i < static_cast<int>(chain.points.size()) - 1; i++) {
        Vector2 point = chain.points[i];
        Vector2 velocity{
            (chain.points[i].x - chain.previousPoints[i].x) * 0.985f,
            (chain.points[i].y - chain.previousPoints[i].y) * 0.985f
        };

        chain.previousPoints[i] = point;
        chain.points[i].x += velocity.x;
        chain.points[i].y += velocity.y + accelerationY;

        for (const Rectangle& collider : colliders) {
            CollideChainPointWithRect(chain.points[i], chain.previousPoints[i], collider, radius);
        }
    }

    for (int iteration = 0; iteration < 8; iteration++) {
        PinChainEnds(chain);

        for (int i = 0; i < static_cast<int>(chain.points.size()) - 1; i++) {
            Vector2& a = chain.points[i];
            Vector2& b = chain.points[i + 1];
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance <= 0.0001f) {
                continue;
            }

            float difference = (distance - segmentLength) / distance;
            Vector2 correction{
                dx * 0.5f * difference,
                dy * 0.5f * difference
            };

            bool aPinned = i == 0;
            bool bPinned = i + 1 == static_cast<int>(chain.points.size()) - 1;
            if (!aPinned) {
                a.x += correction.x;
                a.y += correction.y;
            }
            if (!bPinned) {
                b.x -= correction.x;
                b.y -= correction.y;
            }
        }
    }

    PinChainEnds(chain);
}
