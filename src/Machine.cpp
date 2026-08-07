#include "Machine.h"

#include "MathUtils.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float ContactSlop = 0.01f;

    float Distance(Vector2 a, Vector2 b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    float Dot(Vector2 a, Vector2 b) {
        return a.x * b.x + a.y * b.y;
    }

    Vector2 RectCenter(Rectangle rect) {
        return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    }

    void GetCenteredPlankTop(
        Vector2 center,
        float length,
        float thickness,
        float angleDegrees,
        Vector2& start,
        Vector2& end
    ) {
        float angle = angleDegrees * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 normal{-axis.y, axis.x};
        float halfLength = length * 0.5f;
        float halfThickness = thickness * 0.5f;
        start = {
            center.x - axis.x * halfLength - normal.x * halfThickness,
            center.y - axis.y * halfLength - normal.y * halfThickness
        };
        end = {
            center.x + axis.x * halfLength - normal.x * halfThickness,
            center.y + axis.y * halfLength - normal.y * halfThickness
        };
    }

    void GetHingedPlankTop(const TrapDoor& trapDoor, Vector2& start, Vector2& end) {
        float angle = trapDoor.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 normal{-axis.y, axis.x};
        float halfThickness = trapDoor.thickness * 0.5f;
        Vector2 ring = GetTrapDoorRingPosition(trapDoor);
        start = {
            trapDoor.hinge.x - normal.x * halfThickness,
            trapDoor.hinge.y - normal.y * halfThickness
        };
        end = {
            ring.x - normal.x * halfThickness,
            ring.y - normal.y * halfThickness
        };
    }

    float GetSegmentYAtX(Vector2 start, Vector2 end, float x) {
        float deltaX = end.x - start.x;
        if (fabsf(deltaX) <= 0.0001f) {
            return fminf(start.y, end.y);
        }

        float amount = std::clamp((x - start.x) / deltaX, 0.0f, 1.0f);
        return LerpFloat(start.y, end.y, amount);
    }

    bool IsXOverSegment(Vector2 start, Vector2 end, float x) {
        constexpr float EdgeTolerance = 0.5f;
        float minX = fminf(start.x, end.x) - EdgeTolerance;
        float maxX = fmaxf(start.x, end.x) + EdgeTolerance;
        return fabsf(end.x - start.x) > 0.0001f && x >= minX && x <= maxX;
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

    bool IsChainPointPinned(const Chain& chain, int index) {
        return (index == 0 && chain.pinStart) ||
            (index == static_cast<int>(chain.points.size()) - 1 && chain.pinEnd);
    }

    void PinChainEnds(Chain& chain) {
        if (chain.points.empty()) {
            return;
        }

        if (chain.pinStart) {
            chain.points.front() = chain.start;
            chain.previousPoints.front() = chain.start;
        }
        if (chain.pinEnd) {
            chain.points.back() = chain.end;
            chain.previousPoints.back() = chain.end;
        }
    }

    void CollideFlexiblePointWithRect(Vector2& point, Vector2& previousPoint, Rectangle rect, float radius) {
        const Vector2 pointBeforeCollision = point;
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

        const Vector2 correction{
            point.x - pointBeforeCollision.x,
            point.y - pointBeforeCollision.y
        };
        previousPoint.x += correction.x;
        previousPoint.y += correction.y;

        const float correctionLength =
            sqrtf(correction.x * correction.x + correction.y * correction.y);
        if (correctionLength > 0.0001f) {
            const Vector2 outwardNormal{
                correction.x / correctionLength,
                correction.y / correctionLength
            };
            const Vector2 movement{
                point.x - previousPoint.x,
                point.y - previousPoint.y
            };
            const float inwardMovement = Dot(movement, outwardNormal);
            if (inwardMovement < 0.0f) {
                previousPoint.x += outwardNormal.x * inwardMovement;
                previousPoint.y += outwardNormal.y * inwardMovement;
            }
        }
    }

    template <typename Body>
    bool IsFixedPhysicsBody(const Body&) {
        return false;
    }

    bool IsFixedPhysicsBody(const Gear& gear) {
        return gear.mounting == GearMounting::Mounted;
    }

    void ResolveContact(
        Vector2& centerA,
        Vector2& velocityA,
        float massA,
        bool& onGroundA,
        Vector2& centerB,
        Vector2& velocityB,
        float massB,
        bool& onGroundB,
        Vector2 normal,
        float penetration,
        bool fixedA = false,
        bool fixedB = false
    ) {
        if (penetration <= 0.0f) {
            return;
        }

        float inverseMassA = fixedA ? 0.0f : 1.0f / fmaxf(0.1f, massA);
        float inverseMassB = fixedB ? 0.0f : 1.0f / fmaxf(0.1f, massB);

        // A grounded lower body should support a stack instead of being driven into the floor.
        if (normal.y > 0.55f && onGroundB) {
            inverseMassB = 0.0f;
        }
        else if (normal.y < -0.55f && onGroundA) {
            inverseMassA = 0.0f;
        }

        float inverseMassSum = inverseMassA + inverseMassB;
        if (inverseMassSum <= 0.0f) {
            return;
        }

        float correction = fmaxf(0.0f, penetration - ContactSlop) * 0.88f / inverseMassSum;
        centerA.x -= normal.x * correction * inverseMassA;
        centerA.y -= normal.y * correction * inverseMassA;
        centerB.x += normal.x * correction * inverseMassB;
        centerB.y += normal.y * correction * inverseMassB;

        if (normal.y > 0.55f && !fixedA) {
            onGroundA = true;
        }
        else if (normal.y < -0.55f && !fixedB) {
            onGroundB = true;
        }

        Vector2 relativeVelocity{
            velocityB.x - velocityA.x,
            velocityB.y - velocityA.y
        };
        float normalSpeed = Dot(relativeVelocity, normal);
        if (normalSpeed >= 0.0f) {
            return;
        }

        constexpr float Restitution = 0.025f;
        float impulseMagnitude = -(1.0f + Restitution) * normalSpeed / inverseMassSum;
        Vector2 impulse{normal.x * impulseMagnitude, normal.y * impulseMagnitude};
        velocityA.x -= impulse.x * inverseMassA;
        velocityA.y -= impulse.y * inverseMassA;
        velocityB.x += impulse.x * inverseMassB;
        velocityB.y += impulse.y * inverseMassB;

        relativeVelocity = {velocityB.x - velocityA.x, velocityB.y - velocityA.y};
        Vector2 tangent{
            relativeVelocity.x - Dot(relativeVelocity, normal) * normal.x,
            relativeVelocity.y - Dot(relativeVelocity, normal) * normal.y
        };
        float tangentLength = sqrtf(Dot(tangent, tangent));
        if (tangentLength <= 0.0001f) {
            return;
        }

        tangent.x /= tangentLength;
        tangent.y /= tangentLength;
        float frictionImpulse = -Dot(relativeVelocity, tangent) / inverseMassSum;
        float maxFriction = impulseMagnitude * 0.32f;
        frictionImpulse = std::clamp(frictionImpulse, -maxFriction, maxFriction);
        velocityA.x -= tangent.x * frictionImpulse * inverseMassA;
        velocityA.y -= tangent.y * frictionImpulse * inverseMassA;
        velocityB.x += tangent.x * frictionImpulse * inverseMassB;
        velocityB.y += tangent.y * frictionImpulse * inverseMassB;
    }

    void ResolveStoneBlockPair(StoneBlock& a, StoneBlock& b) {
        if (a.layer != b.layer) {
            return;
        }

        float overlapX = fminf(a.rect.x + a.rect.width, b.rect.x + b.rect.width) - fmaxf(a.rect.x, b.rect.x);
        float overlapY = fminf(a.rect.y + a.rect.height, b.rect.y + b.rect.height) - fmaxf(a.rect.y, b.rect.y);
        if (overlapX <= 0.0f || overlapY <= 0.0f) {
            return;
        }

        Vector2 centerA = RectCenter(a.rect);
        Vector2 centerB = RectCenter(b.rect);
        Vector2 normal{};
        float penetration = 0.0f;
        if (overlapX < overlapY) {
            normal = {centerB.x >= centerA.x ? 1.0f : -1.0f, 0.0f};
            penetration = overlapX;
        }
        else {
            normal = {0.0f, centerB.y >= centerA.y ? 1.0f : -1.0f};
            penetration = overlapY;
        }

        ResolveContact(
            centerA, a.velocity, a.mass, a.onGround,
            centerB, b.velocity, b.mass, b.onGround,
            normal, penetration
        );
        a.rect.x = centerA.x - a.rect.width * 0.5f;
        a.rect.y = centerA.y - a.rect.height * 0.5f;
        b.rect.x = centerB.x - b.rect.width * 0.5f;
        b.rect.y = centerB.y - b.rect.height * 0.5f;
    }

    template <typename RoundBody>
    void ResolveStoneBlockRoundPair(StoneBlock& block, RoundBody& body, float radius) {
        if (block.layer != body.layer) {
            return;
        }

        float closestX = std::clamp(body.center.x, block.rect.x, block.rect.x + block.rect.width);
        float closestY = std::clamp(body.center.y, block.rect.y, block.rect.y + block.rect.height);
        float dx = body.center.x - closestX;
        float dy = body.center.y - closestY;
        float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > radius * radius) {
            return;
        }

        Vector2 normal{};
        float penetration = 0.0f;
        if (distanceSquared <= 0.0001f) {
            float left = fabsf(body.center.x - block.rect.x);
            float right = fabsf(block.rect.x + block.rect.width - body.center.x);
            float top = fabsf(body.center.y - block.rect.y);
            float bottom = fabsf(block.rect.y + block.rect.height - body.center.y);
            float nearest = fminf(fminf(left, right), fminf(top, bottom));
            if (nearest == left) {
                normal = {-1.0f, 0.0f};
                penetration = radius + left;
            }
            else if (nearest == right) {
                normal = {1.0f, 0.0f};
                penetration = radius + right;
            }
            else if (nearest == top) {
                normal = {0.0f, -1.0f};
                penetration = radius + top;
            }
            else {
                normal = {0.0f, 1.0f};
                penetration = radius + bottom;
            }
        }
        else {
            float distance = sqrtf(distanceSquared);
            normal = {dx / distance, dy / distance};
            penetration = radius - distance;
        }

        Vector2 blockCenter = RectCenter(block.rect);
        ResolveContact(
            blockCenter, block.velocity, block.mass, block.onGround,
            body.center, body.velocity, body.mass, body.onGround,
            normal, penetration, false, IsFixedPhysicsBody(body)
        );
        block.rect.x = blockCenter.x - block.rect.width * 0.5f;
        block.rect.y = blockCenter.y - block.rect.height * 0.5f;
    }

    template <typename RoundBodyA, typename RoundBodyB>
    void ResolveRoundPair(RoundBodyA& a, float radiusA, RoundBodyB& b, float radiusB) {
        if (a.layer != b.layer) {
            return;
        }

        Vector2 delta{b.center.x - a.center.x, b.center.y - a.center.y};
        float distanceSquared = Dot(delta, delta);
        float combinedRadius = radiusA + radiusB;
        if (distanceSquared >= combinedRadius * combinedRadius) {
            return;
        }

        float distance = sqrtf(distanceSquared);
        Vector2 normal = distance > 0.0001f
            ? Vector2{delta.x / distance, delta.y / distance}
            : Vector2{1.0f, 0.0f};
        ResolveContact(
            a.center, a.velocity, a.mass, a.onGround,
            b.center, b.velocity, b.mass, b.onGround,
            normal, combinedRadius - distance,
            IsFixedPhysicsBody(a), IsFixedPhysicsBody(b)
        );
    }

    template <typename RoundBody>
    void ResolveSameRoundBodies(std::vector<RoundBody>& bodies, float radiusScale) {
        for (int i = 0; i < static_cast<int>(bodies.size()); i++) {
            for (int j = i + 1; j < static_cast<int>(bodies.size()); j++) {
                ResolveRoundPair(
                    bodies[i], bodies[i].radius * radiusScale,
                    bodies[j], bodies[j].radius * radiusScale
                );
            }
        }
    }

    template <typename RoundBodyA, typename RoundBodyB>
    void ResolveRoundBodyCollections(
        std::vector<RoundBodyA>& bodiesA,
        float radiusScaleA,
        std::vector<RoundBodyB>& bodiesB,
        float radiusScaleB
    ) {
        for (RoundBodyA& a : bodiesA) {
            for (RoundBodyB& b : bodiesB) {
                ResolveRoundPair(a, a.radius * radiusScaleA, b, b.radius * radiusScaleB);
            }
        }
    }

    template <typename RoundBody>
    void ResolveBlocksAgainstRoundBodies(
        std::vector<StoneBlock>& blocks,
        std::vector<RoundBody>& bodies,
        float radiusScale
    ) {
        for (StoneBlock& block : blocks) {
            for (RoundBody& body : bodies) {
                ResolveStoneBlockRoundPair(block, body, body.radius * radiusScale);
            }
        }
    }

    bool SectorTeethFaceGear(const Gear& sector, const Gear& other) {
        if (sector.visualType != GearVisualType::Sector) {
            return true;
        }

        const float contactAngle = atan2f(other.center.y - sector.center.y,
            other.center.x - sector.center.x) * RAD2DEG;
        const float offset = remainderf(contactAngle - sector.rotation, 360.0f);
        return fabsf(offset) <= 117.5f;
    }

    bool CanGearsMesh(const Gear& a, const Gear& b) {
        if (a.layer != b.layer) {
            return false;
        }

        const bool samePlane = a.orientation == b.orientation;
        const bool bevelTransfer = a.visualType == GearVisualType::Bevel ||
            b.visualType == GearVisualType::Bevel;
        return (samePlane || bevelTransfer) && SectorTeethFaceGear(a, b) && SectorTeethFaceGear(b, a);
    }

    void CoupleTouchingGears(Gear& a, Gear& b) {
        if (!CanGearsMesh(a, b)) {
            return;
        }

        float radiusA = a.radius * GearOuterRadiusScale;
        float radiusB = b.radius * GearOuterRadiusScale;
        if (Distance(a.center, b.center) > radiusA + radiusB + 1.0f) {
            return;
        }

        float inertiaA = 0.5f * fmaxf(0.1f, a.mass) * radiusA * radiusA;
        float inertiaB = 0.5f * fmaxf(0.1f, b.mass) * radiusB * radiusB;
        float inverseInertiaA = a.stopped ? 0.0f : 1.0f / inertiaA;
        float inverseInertiaB = b.stopped ? 0.0f : 1.0f / inertiaB;
        float omegaA = a.angularVelocity * DEG2RAD;
        float omegaB = b.angularVelocity * DEG2RAD;
        float contactSlip = omegaA * radiusA + omegaB * radiusB;
        float denominator = radiusA * radiusA * inverseInertiaA + radiusB * radiusB * inverseInertiaB;
        if (fabsf(contactSlip) <= 0.0001f || denominator <= 0.0001f) {
            return;
        }

        float impulse = contactSlip / denominator * 0.65f;
        omegaA -= impulse * radiusA * inverseInertiaA;
        omegaB -= impulse * radiusB * inverseInertiaB;
        a.angularVelocity = a.stopped ? 0.0f : omegaA * RAD2DEG;
        b.angularVelocity = b.stopped ? 0.0f : omegaB * RAD2DEG;
    }

    bool IsPhysicsRopePointPinned(const PhysicsRope& rope, int index) {
        return (index == 0 && rope.pinStart) ||
            (index == static_cast<int>(rope.points.size()) - 1 && rope.pinEnd);
    }

    void PinPhysicsRopeEnds(PhysicsRope& rope) {
        if (rope.points.empty()) {
            return;
        }

        if (rope.pinStart) {
            rope.points.front() = rope.start;
            rope.previousPoints.front() = rope.start;
        }
        if (rope.pinEnd) {
            rope.points.back() = rope.end;
            rope.previousPoints.back() = rope.end;
        }
    }

    void SolvePhysicsRopeSegments(PhysicsRope& rope) {
        for (int i = 0; i < static_cast<int>(rope.points.size()) - 1; i++) {
            Vector2& a = rope.points[i];
            Vector2& b = rope.points[i + 1];
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance <= 0.0001f) {
                continue;
            }

            bool aPinned = IsPhysicsRopePointPinned(rope, i);
            bool bPinned = IsPhysicsRopePointPinned(rope, i + 1);
            float inverseMassA = aPinned ? 0.0f : 1.0f;
            float inverseMassB = bPinned ? 0.0f : 1.0f;
            float inverseMassSum = inverseMassA + inverseMassB;
            if (inverseMassSum <= 0.0f) {
                continue;
            }

            float error = (distance - rope.segmentLength) / distance;
            a.x += dx * error * (inverseMassA / inverseMassSum);
            a.y += dy * error * (inverseMassA / inverseMassSum);
            b.x -= dx * error * (inverseMassB / inverseMassSum);
            b.y -= dy * error * (inverseMassB / inverseMassSum);
        }
    }

    void SolvePhysicsRopeSelfCollisions(PhysicsRope& rope) {
        float minimumDistance = fmaxf(2.0f, rope.thickness);
        float minimumDistanceSquared = minimumDistance * minimumDistance;
        int pointCount = static_cast<int>(rope.points.size());
        for (int i = 0; i < pointCount; i++) {
            for (int j = i + 3; j < pointCount; j++) {
                Vector2 delta{
                    rope.points[j].x - rope.points[i].x,
                    rope.points[j].y - rope.points[i].y
                };
                float distanceSquared = Dot(delta, delta);
                if (distanceSquared >= minimumDistanceSquared) {
                    continue;
                }

                float distance = sqrtf(distanceSquared);
                Vector2 normal = distance > 0.0001f
                    ? Vector2{delta.x / distance, delta.y / distance}
                    : Vector2{(i + j) % 2 == 0 ? 1.0f : -1.0f, 0.0f};
                bool aPinned = IsPhysicsRopePointPinned(rope, i);
                bool bPinned = IsPhysicsRopePointPinned(rope, j);
                float inverseMassA = aPinned ? 0.0f : 1.0f;
                float inverseMassB = bPinned ? 0.0f : 1.0f;
                float inverseMassSum = inverseMassA + inverseMassB;
                if (inverseMassSum <= 0.0f) {
                    continue;
                }

                float correction = (minimumDistance - distance) / inverseMassSum;
                rope.points[i].x -= normal.x * correction * inverseMassA;
                rope.points[i].y -= normal.y * correction * inverseMassA;
                rope.points[j].x += normal.x * correction * inverseMassB;
                rope.points[j].y += normal.y * correction * inverseMassB;
            }
        }
    }
}

float GetMachinePower(const Winch& winch) {
    return Clamp01((winch.rect.x - winch.startX) / (winch.maxX - winch.startX));
}

float UpdateWinch(Winch& winch, Player& player, float moveInput, bool interactHeld, float dt) {
    float oldX = winch.rect.x;
    winch.grabbed = NearRect(player.rect, winch.rect, 18.0f) && interactHeld;

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

void AdvanceRotaryLatch(RotaryLatch& latch, float machinePower, float dt) {
    latch.powered = machinePower > 0.05f;

    if (!latch.latched && latch.powered) {
        latch.angle = NormalizeAngle(latch.angle + latch.spinSpeed * machinePower * dt);
    }
}

void TryLockRotaryLatch(RotaryLatch& latch, const Player& player, bool interactPressed) {
    bool playerNear = CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player.rect);
    if (playerNear && interactPressed && IsRotaryLatchAligned(latch)) {
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
    Vector2 start{};
    Vector2 end{};
    GetCenteredPlankTop(seeSaw.pivot, seeSaw.length, seeSaw.thickness, seeSaw.angle, start, end);
    return GetSegmentYAtX(start, end, x);
}

bool IsPointOverSeeSaw(const SeeSaw& seeSaw, float x) {
    Vector2 start{};
    Vector2 end{};
    GetCenteredPlankTop(seeSaw.pivot, seeSaw.length, seeSaw.thickness, seeSaw.angle, start, end);
    return IsXOverSegment(start, end, x);
}

float GetRampSurfaceY(const Ramp& ramp, float x) {
    Vector2 start{};
    Vector2 end{};
    GetCenteredPlankTop(ramp.center, ramp.length, ramp.thickness, ramp.angle, start, end);
    return GetSegmentYAtX(start, end, x);
}

bool IsPointOverRamp(const Ramp& ramp, float x) {
    Vector2 start{};
    Vector2 end{};
    GetCenteredPlankTop(ramp.center, ramp.length, ramp.thickness, ramp.angle, start, end);
    return IsXOverSegment(start, end, x);
}

Vector2 GetTrapDoorRingPosition(const TrapDoor& trapDoor) {
    float angle = trapDoor.angle * DEG2RAD;
    return {
        trapDoor.hinge.x + cosf(angle) * trapDoor.length,
        trapDoor.hinge.y + sinf(angle) * trapDoor.length
    };
}

std::array<Vector2, 2> GetTrapDoorRingPositions(const TrapDoor& trapDoor) {
    float angle = trapDoor.angle * DEG2RAD;
    Vector2 axis{cosf(angle), sinf(angle)};
    Vector2 normal{-axis.y, axis.x};
    float inset = std::clamp(trapDoor.length * 0.12f, trapDoor.thickness * 0.7f, trapDoor.length * 0.3f);
    // Keep each ring fully clear of its door face, with a small readable gap.
    float faceOffset = trapDoor.thickness * 1.10f;
    Vector2 center{
        trapDoor.hinge.x + axis.x * (trapDoor.length - inset),
        trapDoor.hinge.y + axis.y * (trapDoor.length - inset)
    };

    return {{
        {center.x - normal.x * faceOffset, center.y - normal.y * faceOffset},
        {center.x + normal.x * faceOffset, center.y + normal.y * faceOffset}
    }};
}

float GetTrapDoorSurfaceY(const TrapDoor& trapDoor, float x) {
    Vector2 start{};
    Vector2 end{};
    GetHingedPlankTop(trapDoor, start, end);
    return GetSegmentYAtX(start, end, x);
}

bool IsPointOverTrapDoor(const TrapDoor& trapDoor, float x) {
    Vector2 start{};
    Vector2 end{};
    GetHingedPlankTop(trapDoor, start, end);
    return IsXOverSegment(start, end, x);
}

void ApplyScrewGearCoupling(Gear& gear, const std::vector<Screw>& screws, float dt) {
    float gearOuterRadius = gear.radius * GearOuterRadiusScale;
    for (const Screw& screw : screws) {
        if (gear.layer != screw.layer) {
            continue;
        }

        float angle = screw.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 normal{-axis.y, axis.x};
        Vector2 offset{gear.center.x - screw.center.x, gear.center.y - screw.center.y};
        float along = std::clamp(
            Dot(offset, axis),
            -screw.length * 0.5f,
            screw.length * 0.5f
        );
        Vector2 closest{
            screw.center.x + axis.x * along,
            screw.center.y + axis.y * along
        };
        Vector2 separation{gear.center.x - closest.x, gear.center.y - closest.y};
        float distance = sqrtf(Dot(separation, separation));
        float contactDistance = gearOuterRadius + screw.radius;
        if (distance > contactDistance + 3.0f) {
            continue;
        }

        float side = Dot(offset, normal) < 0.0f ? -1.0f : 1.0f;
        float inertiaScale = 1.0f / sqrtf(fmaxf(1.0f, gear.mass));
        float angularCoupling = std::clamp(8.0f * inertiaScale * dt, 0.0f, 1.0f);
        const float screwSurfaceSpeed = screw.spinSpeed * DEG2RAD * screw.radius;
        const float targetAngularVelocity = -side * screwSurfaceSpeed /
            fmaxf(1.0f, gearOuterRadius) * RAD2DEG;
        gear.angularVelocity = LerpFloat(gear.angularVelocity, targetAngularVelocity, angularCoupling);

        float pitch = fmaxf(20.0f, screw.radius * 1.85f);
        float targetAxialSpeed = screw.spinSpeed / 360.0f * pitch;
        float currentAxialSpeed = Dot(gear.velocity, axis);
        float axialCoupling = std::clamp(3.2f * inertiaScale * dt, 0.0f, 1.0f);
        float axialChange = (targetAxialSpeed - currentAxialSpeed) * axialCoupling;
        gear.velocity.x += axis.x * axialChange;
        gear.velocity.y += axis.y * axialChange;
    }
}

void ResolveDynamicBodyCollisions(
    std::vector<StoneBlock>& stoneBlocks,
    std::vector<Boulder>& boulders,
    std::vector<PhysicsWheel>& physicsWheels,
    std::vector<Gear>& gears,
    std::vector<Flywheel>& flywheels
) {
    constexpr int SolverIterations = 6;
    for (int iteration = 0; iteration < SolverIterations; iteration++) {
        for (int i = 0; i < static_cast<int>(stoneBlocks.size()); i++) {
            for (int j = i + 1; j < static_cast<int>(stoneBlocks.size()); j++) {
                ResolveStoneBlockPair(stoneBlocks[i], stoneBlocks[j]);
            }
        }

        ResolveBlocksAgainstRoundBodies(stoneBlocks, boulders, 1.0f);
        ResolveBlocksAgainstRoundBodies(stoneBlocks, physicsWheels, 1.0f);
        ResolveBlocksAgainstRoundBodies(stoneBlocks, gears, GearOuterRadiusScale);
        ResolveBlocksAgainstRoundBodies(stoneBlocks, flywheels, 1.0f);

        ResolveSameRoundBodies(boulders, 1.0f);
        ResolveSameRoundBodies(physicsWheels, 1.0f);
        ResolveSameRoundBodies(gears, GearOuterRadiusScale);
        ResolveSameRoundBodies(flywheels, 1.0f);

        ResolveRoundBodyCollections(boulders, 1.0f, physicsWheels, 1.0f);
        ResolveRoundBodyCollections(boulders, 1.0f, gears, GearOuterRadiusScale);
        ResolveRoundBodyCollections(boulders, 1.0f, flywheels, 1.0f);
        ResolveRoundBodyCollections(physicsWheels, 1.0f, gears, GearOuterRadiusScale);
        ResolveRoundBodyCollections(physicsWheels, 1.0f, flywheels, 1.0f);
        ResolveRoundBodyCollections(gears, GearOuterRadiusScale, flywheels, 1.0f);
    }

    for (int i = 0; i < static_cast<int>(gears.size()); i++) {
        for (int j = i + 1; j < static_cast<int>(gears.size()); j++) {
            CoupleTouchingGears(gears[i], gears[j]);
        }
    }
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

    chain.previousTimeStep = 1.0f / 60.0f;
    PinChainEnds(chain);
}

void UpdateChainPhysics(
    Chain& chain,
    const std::vector<Rectangle>& colliders,
    const std::vector<Vector2>& externalAccelerations,
    float dt
) {
    if (chain.points.size() < 2 || chain.previousPoints.size() != chain.points.size()) {
        InitializeChain(chain);
    }
    if (dt <= 0.0f) {
        return;
    }

    float radius = fmaxf(2.0f, chain.collisionRadius * chain.scale);
    float segmentLength = GetChainSegmentLength(chain);
    float simulatedTime = std::clamp(dt, 0.0f, 1.0f / 15.0f);
    int substepCount = std::max(1, static_cast<int>(ceilf(simulatedTime / (1.0f / 120.0f))));
    float substep = simulatedTime / static_cast<float>(substepCount);

    for (int step = 0; step < substepCount; step++) {
        PinChainEnds(chain);

        float timeScale = substep / fmaxf(1.0f / 1000.0f, chain.previousTimeStep);
        float damping = powf(0.985f, substep * 60.0f);
        for (int i = 0; i < static_cast<int>(chain.points.size()); i++) {
            if (IsChainPointPinned(chain, i)) {
                continue;
            }
            if (!std::isfinite(chain.points[i].x) ||
                !std::isfinite(chain.points[i].y) ||
                !std::isfinite(chain.previousPoints[i].x) ||
                !std::isfinite(chain.previousPoints[i].y)) {
                const float amount = static_cast<float>(i) /
                    static_cast<float>(chain.points.size() - 1);
                chain.points[i] = LerpVector(chain.start, chain.end, amount);
                chain.previousPoints[i] = chain.points[i];
            }

            Vector2 point = chain.points[i];
            Vector2 displacement{
                (chain.points[i].x - chain.previousPoints[i].x) * timeScale * damping,
                (chain.points[i].y - chain.previousPoints[i].y) * timeScale * damping
            };
            const float displacementLength =
                sqrtf(displacement.x * displacement.x + displacement.y * displacement.y);
            const float maximumDisplacement = fmaxf(4.0f, segmentLength * 0.75f);
            if (displacementLength > maximumDisplacement) {
                const float scale = maximumDisplacement / displacementLength;
                displacement.x *= scale;
                displacement.y *= scale;
            }

            Vector2 external{};
            if (i < static_cast<int>(externalAccelerations.size())) {
                external = externalAccelerations[i];
            }
            if (!std::isfinite(external.x) || !std::isfinite(external.y)) {
                external = {};
            }
            const float externalLength =
                sqrtf(external.x * external.x + external.y * external.y);
            constexpr float MaximumExternalAcceleration = 5000.0f;
            if (externalLength > MaximumExternalAcceleration) {
                const float scale = MaximumExternalAcceleration / externalLength;
                external.x *= scale;
                external.y *= scale;
            }

            chain.previousPoints[i] = point;
            chain.points[i].x += displacement.x + external.x * substep * substep;
            chain.points[i].y += displacement.y + (900.0f + external.y) * substep * substep;

            for (const Rectangle& collider : colliders) {
                CollideFlexiblePointWithRect(chain.points[i], chain.previousPoints[i], collider, radius);
            }
        }

        for (int iteration = 0; iteration < 10; iteration++) {
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
                bool aPinned = IsChainPointPinned(chain, i);
                bool bPinned = IsChainPointPinned(chain, i + 1);
                float aWeight = aPinned ? 0.0f : (bPinned ? 1.0f : 0.5f);
                float bWeight = bPinned ? 0.0f : (aPinned ? 1.0f : 0.5f);
                Vector2 correctionA{
                    dx * difference * aWeight,
                    dy * difference * aWeight
                };
                Vector2 correctionB{
                    -dx * difference * bWeight,
                    -dy * difference * bWeight
                };
                const auto clampCorrection = [&](Vector2& correction) {
                    const float correctionLength = sqrtf(
                        correction.x * correction.x +
                        correction.y * correction.y
                    );
                    const float maximumCorrection = segmentLength * 0.55f;
                    if (correctionLength > maximumCorrection) {
                        const float scale = maximumCorrection / correctionLength;
                        correction.x *= scale;
                        correction.y *= scale;
                    }
                };
                clampCorrection(correctionA);
                clampCorrection(correctionB);
                a.x += correctionA.x;
                a.y += correctionA.y;
                b.x += correctionB.x;
                b.y += correctionB.y;

                // Most constraint projection is geometric cleanup, not real
                // acceleration. Mirror it into Verlet history so a stretched
                // or newly pinned chain cannot turn that correction into an
                // explosive velocity on the following substep.
                constexpr float HistoryCorrection = 0.85f;
                if (!aPinned) {
                    chain.previousPoints[i].x += correctionA.x * HistoryCorrection;
                    chain.previousPoints[i].y += correctionA.y * HistoryCorrection;
                }
                if (!bPinned) {
                    chain.previousPoints[i + 1].x += correctionB.x * HistoryCorrection;
                    chain.previousPoints[i + 1].y += correctionB.y * HistoryCorrection;
                }
            }

            for (int i = 0; i < static_cast<int>(chain.points.size()); i++) {
                if (IsChainPointPinned(chain, i)) {
                    continue;
                }
                for (const Rectangle& collider : colliders) {
                    CollideFlexiblePointWithRect(chain.points[i], chain.previousPoints[i], collider, radius);
                }
            }
        }

        chain.previousTimeStep = substep;
    }

    PinChainEnds(chain);
}

void InitializePhysicsRope(PhysicsRope& rope) {
    rope.thickness = fmaxf(1.0f, rope.thickness);
    float spanLength = Distance(rope.start, rope.end);
    if (rope.pinStart && rope.pinEnd) {
        rope.length = fmaxf(spanLength, rope.length);
    }
    else {
        rope.length = fmaxf(1.0f, rope.length);
    }

    float targetSegmentLength = std::clamp(rope.thickness * 1.35f, 4.0f, 8.0f);
    int segmentCount = std::clamp(
        static_cast<int>(ceilf(rope.length / targetSegmentLength)),
        2,
        256
    );
    rope.segmentLength = rope.length / static_cast<float>(segmentCount);
    rope.points.assign(segmentCount + 1, {});
    rope.previousPoints.assign(segmentCount + 1, {});

    Vector2 direction = spanLength > 0.0001f
        ? Vector2{(rope.end.x - rope.start.x) / spanLength, (rope.end.y - rope.start.y) / spanLength}
        : Vector2{0.0f, 1.0f};

    if (rope.pinStart && rope.pinEnd) {
        Vector2 sagNormal{-direction.y, direction.x};
        if (sagNormal.y < 0.0f || (fabsf(sagNormal.y) <= 0.0001f && sagNormal.x < 0.0f)) {
            sagNormal.x = -sagNormal.x;
            sagNormal.y = -sagNormal.y;
        }

        auto curvePoint = [&](float amount, float amplitude) {
            float sag = sinf(PI * amount) * amplitude;
            return Vector2{
                LerpFloat(rope.start.x, rope.end.x, amount) + sagNormal.x * sag,
                LerpFloat(rope.start.y, rope.end.y, amount) + sagNormal.y * sag
            };
        };
        auto curveLength = [&](float amplitude) {
            float total = 0.0f;
            Vector2 previous = curvePoint(0.0f, amplitude);
            for (int i = 1; i <= segmentCount; i++) {
                Vector2 point = curvePoint(static_cast<float>(i) / static_cast<float>(segmentCount), amplitude);
                total += Distance(previous, point);
                previous = point;
            }
            return total;
        };

        float low = 0.0f;
        float high = fmaxf(1.0f, rope.length);
        for (int i = 0; i < 4 && curveLength(high) < rope.length; i++) {
            high *= 2.0f;
        }
        for (int i = 0; i < 18; i++) {
            float midpoint = (low + high) * 0.5f;
            if (curveLength(midpoint) < rope.length) {
                low = midpoint;
            }
            else {
                high = midpoint;
            }
        }

        float sagAmplitude = (low + high) * 0.5f;
        for (int i = 0; i <= segmentCount; i++) {
            float amount = static_cast<float>(i) / static_cast<float>(segmentCount);
            rope.points[i] = curvePoint(amount, sagAmplitude);
        }
    }
    else if (rope.pinStart) {
        for (int i = 0; i <= segmentCount; i++) {
            float distance = rope.segmentLength * static_cast<float>(i);
            rope.points[i] = {
                rope.start.x + direction.x * distance,
                rope.start.y + direction.y * distance
            };
        }
    }
    else if (rope.pinEnd) {
        for (int i = 0; i <= segmentCount; i++) {
            float distanceFromEnd = rope.segmentLength * static_cast<float>(segmentCount - i);
            rope.points[i] = {
                rope.end.x - direction.x * distanceFromEnd,
                rope.end.y - direction.y * distanceFromEnd
            };
        }
    }
    else {
        Vector2 center{
            (rope.start.x + rope.end.x) * 0.5f,
            (rope.start.y + rope.end.y) * 0.5f
        };
        for (int i = 0; i <= segmentCount; i++) {
            float distance = (static_cast<float>(i) / static_cast<float>(segmentCount) - 0.5f) * rope.length;
            rope.points[i] = {
                center.x + direction.x * distance,
                center.y + direction.y * distance
            };
        }
    }

    rope.previousPoints = rope.points;
    rope.previousTimeStep = 1.0f / 60.0f;
    PinPhysicsRopeEnds(rope);
}

void UpdatePhysicsRope(
    PhysicsRope& rope,
    const std::vector<Rectangle>& colliders,
    const std::vector<Vector2>& externalAccelerations,
    float dt
) {
    if (rope.points.size() < 3 || rope.previousPoints.size() != rope.points.size()) {
        InitializePhysicsRope(rope);
    }
    if (dt <= 0.0f) {
        return;
    }

    float radius = fmaxf(1.0f, rope.thickness * 0.5f);
    float simulatedTime = std::clamp(dt, 0.0f, 1.0f / 15.0f);
    int substepCount = std::max(1, static_cast<int>(ceilf(simulatedTime / (1.0f / 120.0f))));
    float substep = simulatedTime / static_cast<float>(substepCount);

    for (int step = 0; step < substepCount; step++) {
        PinPhysicsRopeEnds(rope);
        float timeScale = substep / fmaxf(1.0f / 1000.0f, rope.previousTimeStep);
        float damping = powf(0.992f, substep * 60.0f);

        for (int i = 0; i < static_cast<int>(rope.points.size()); i++) {
            if (IsPhysicsRopePointPinned(rope, i)) {
                continue;
            }

            Vector2 point = rope.points[i];
            Vector2 displacement{
                (rope.points[i].x - rope.previousPoints[i].x) * timeScale * damping,
                (rope.points[i].y - rope.previousPoints[i].y) * timeScale * damping
            };
            Vector2 external{};
            if (i < static_cast<int>(externalAccelerations.size())) {
                external = externalAccelerations[i];
            }

            rope.previousPoints[i] = point;
            rope.points[i].x += displacement.x + external.x * substep * substep;
            rope.points[i].y += displacement.y + (900.0f + external.y) * substep * substep;
            for (const Rectangle& collider : colliders) {
                CollideFlexiblePointWithRect(rope.points[i], rope.previousPoints[i], collider, radius);
            }
        }

        constexpr int ConstraintIterations = 12;
        for (int iteration = 0; iteration < ConstraintIterations; iteration++) {
            PinPhysicsRopeEnds(rope);
            SolvePhysicsRopeSegments(rope);

            if (iteration == 5 || iteration == 9) {
                SolvePhysicsRopeSelfCollisions(rope);
            }

            if (iteration % 2 == 1 || iteration == ConstraintIterations - 1) {
                for (int i = 0; i < static_cast<int>(rope.points.size()); i++) {
                    if (IsPhysicsRopePointPinned(rope, i)) {
                        continue;
                    }
                    for (const Rectangle& collider : colliders) {
                        CollideFlexiblePointWithRect(rope.points[i], rope.previousPoints[i], collider, radius);
                    }
                }
            }
        }

        PinPhysicsRopeEnds(rope);
        rope.previousTimeStep = substep;
    }
}
