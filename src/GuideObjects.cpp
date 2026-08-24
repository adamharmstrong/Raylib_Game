#include "GuideObjects.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
    constexpr float Pi = 3.14159265358979323846f;

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    Vector2 NormalizeOr(Vector2 value, Vector2 fallback = {1.0f, 0.0f}) {
        float length = sqrtf(value.x * value.x + value.y * value.y);
        return length > 0.0001f ? Vector2{value.x / length, value.y / length} : fallback;
    }

    Vector2 Add(Vector2 a, Vector2 b) {
        return {a.x + b.x, a.y + b.y};
    }

    Vector2 Scale(Vector2 value, float scale) {
        return {value.x * scale, value.y * scale};
    }

    Vector2 Center(Rectangle rect) {
        return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    }

    void ReadOptionalChannel(std::istringstream& stream, GuideObject& object) {
        if (!(stream >> object.power.channel)) {
            stream.clear();
            object.power.channel = 0;
        }
        object.sensor.channel = object.power.channel;
    }

    void ConfigureRectangle(GuideObject& object, float x, float y, float width, float height, BodyType bodyType) {
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.previousPosition = object.transform.position;
        object.collider.shape = ColliderShape::Rectangle;
        object.collider.size = {fmaxf(1.0f, width), fmaxf(1.0f, height)};
        object.body.type = bodyType;
        object.body.affectedByGravity = bodyType == BodyType::Dynamic;
    }

    void ConfigureCircle(GuideObject& object, float x, float y, float radius, BodyType bodyType) {
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.previousPosition = object.transform.position;
        object.collider.shape = ColliderShape::Circle;
        object.collider.radius = fmaxf(1.0f, radius);
        object.body.type = bodyType;
        object.body.affectedByGravity = bodyType == BodyType::Dynamic;
    }

    bool IsKinematicSolid(GuideObjectType type) {
        return type == GuideObjectType::MovingPlatform || type == GuideObjectType::Elevator ||
            type == GuideObjectType::Piston || type == GuideObjectType::HydraulicCylinder ||
            type == GuideObjectType::CrushingBlock;
    }

    bool IsStaticSolid(GuideObjectType type) {
        return type == GuideObjectType::GuideRail || type == GuideObjectType::ConveyorBelt;
    }

    bool IsRoundDynamic(GuideObjectType type) {
        return type == GuideObjectType::Ball;
    }

    bool IsDynamicObject(GuideObjectType type) {
        return type == GuideObjectType::Ball || type == GuideObjectType::Barrel ||
            type == GuideObjectType::BreakableCrate || type == GuideObjectType::ExplosiveBarrel;
    }

    bool IsSpringConstraintType(GuideObjectType type) {
        return type == GuideObjectType::Spring || type == GuideObjectType::CompressionSpring ||
            type == GuideObjectType::ExtensionSpring || type == GuideObjectType::TorsionSpring ||
            type == GuideObjectType::GarterSpring || type == GuideObjectType::VoluteSpring ||
            type == GuideObjectType::SpiralSpring || type == GuideObjectType::ConstantForceSpring ||
            type == GuideObjectType::ConstantTorqueSpring || type == GuideObjectType::LeafSpring ||
            type == GuideObjectType::BeamSpring || type == GuideObjectType::DiscSpring ||
            type == GuideObjectType::WaveSpring || type == GuideObjectType::WaveWasher ||
            type == GuideObjectType::TorsionBar || type == GuideObjectType::RingSpring ||
            type == GuideObjectType::ElastomerSpring || type == GuideObjectType::PneumaticSpring ||
            type == GuideObjectType::GasSpring || type == GuideObjectType::HydropneumaticSpring ||
            type == GuideObjectType::MagneticSpring || type == GuideObjectType::CompositeSpring;
    }

    bool IsRotarySpringType(GuideObjectType type) {
        return type == GuideObjectType::TorsionSpring || type == GuideObjectType::SpiralSpring ||
            type == GuideObjectType::ConstantTorqueSpring || type == GuideObjectType::TorsionBar;
    }

    bool IsFlexuralSpringType(GuideObjectType type) {
        return type == GuideObjectType::LeafSpring || type == GuideObjectType::BeamSpring;
    }

    bool TryGetSpringType(const std::string& command, GuideObjectType& type) {
        if (command == "spring") type = GuideObjectType::Spring;
        else if (command == "compressionSpring") type = GuideObjectType::CompressionSpring;
        else if (command == "extensionSpring") type = GuideObjectType::ExtensionSpring;
        else if (command == "torsionSpring") type = GuideObjectType::TorsionSpring;
        else if (command == "garterSpring") type = GuideObjectType::GarterSpring;
        else if (command == "voluteSpring") type = GuideObjectType::VoluteSpring;
        else if (command == "spiralSpring") type = GuideObjectType::SpiralSpring;
        else if (command == "constantForceSpring") type = GuideObjectType::ConstantForceSpring;
        else if (command == "constantTorqueSpring") type = GuideObjectType::ConstantTorqueSpring;
        else if (command == "leafSpring") type = GuideObjectType::LeafSpring;
        else if (command == "beamSpring") type = GuideObjectType::BeamSpring;
        else if (command == "discSpring") type = GuideObjectType::DiscSpring;
        else if (command == "waveSpring") type = GuideObjectType::WaveSpring;
        else if (command == "waveWasher") type = GuideObjectType::WaveWasher;
        else if (command == "torsionBar") type = GuideObjectType::TorsionBar;
        else if (command == "ringSpring") type = GuideObjectType::RingSpring;
        else if (command == "elastomerSpring") type = GuideObjectType::ElastomerSpring;
        else if (command == "pneumaticSpring") type = GuideObjectType::PneumaticSpring;
        else if (command == "gasSpring") type = GuideObjectType::GasSpring;
        else if (command == "hydropneumaticSpring") type = GuideObjectType::HydropneumaticSpring;
        else if (command == "magneticSpring") type = GuideObjectType::MagneticSpring;
        else if (command == "compositeSpring") type = GuideObjectType::CompositeSpring;
        else return false;
        return true;
    }

    void ConfigureSpringDefaults(GuideObject& object) {
        object.constraint.stiffness = 8.0f;
        object.constraint.damping = 1.0f;
        object.strength = 80.0f;
        switch (object.type) {
            case GuideObjectType::VoluteSpring: object.constraint.stiffness = 14.0f; break;
            case GuideObjectType::TorsionSpring: object.constraint.stiffness = 10.0f; object.constraint.damping = 1.8f; break;
            case GuideObjectType::GarterSpring: object.constraint.stiffness = 9.0f; break;
            case GuideObjectType::SpiralSpring: object.constraint.stiffness = 9.0f; object.constraint.damping = 1.4f; break;
            case GuideObjectType::ConstantForceSpring: object.strength = 65.0f; break;
            case GuideObjectType::ConstantTorqueSpring: object.strength = 85.0f; object.constraint.damping = 1.2f; break;
            case GuideObjectType::LeafSpring: object.constraint.stiffness = 12.0f; object.constraint.damping = 2.0f; break;
            case GuideObjectType::BeamSpring: object.constraint.stiffness = 10.0f; object.constraint.damping = 1.7f; break;
            case GuideObjectType::DiscSpring: object.constraint.stiffness = 18.0f; break;
            case GuideObjectType::WaveSpring: object.constraint.stiffness = 10.0f; break;
            case GuideObjectType::WaveWasher: object.constraint.stiffness = 7.0f; break;
            case GuideObjectType::TorsionBar: object.constraint.stiffness = 15.0f; object.constraint.damping = 2.2f; break;
            case GuideObjectType::RingSpring: object.constraint.stiffness = 13.0f; object.constraint.damping = 4.2f; break;
            case GuideObjectType::ElastomerSpring: object.constraint.stiffness = 9.0f; object.constraint.damping = 4.8f; break;
            case GuideObjectType::PneumaticSpring: object.constraint.stiffness = 11.0f; object.constraint.damping = 2.4f; break;
            case GuideObjectType::GasSpring: object.constraint.stiffness = 10.0f; object.constraint.damping = 3.4f; object.strength = 35.0f; break;
            case GuideObjectType::HydropneumaticSpring: object.constraint.stiffness = 14.0f; object.constraint.damping = 5.2f; object.strength = 45.0f; break;
            case GuideObjectType::MagneticSpring: object.strength = 2200.0f; object.constraint.damping = 0.4f; break;
            case GuideObjectType::CompositeSpring: object.constraint.stiffness = 10.0f; object.constraint.damping = 1.6f; break;
            default: break;
        }
    }

    float GetSpringForce(const GuideObject& spring, float distance, float radialSpeed) {
        float restLength = fmaxf(1.0f, spring.constraint.restLength);
        float displacement = distance - restLength;
        float compression = fmaxf(0.0f, -displacement);
        float extension = fmaxf(0.0f, displacement);
        float compressionRatio = compression / restLength;
        float extensionRatio = extension / restLength;
        float stiffness = spring.constraint.stiffness;
        float damping = spring.constraint.damping;

        switch (spring.type) {
            case GuideObjectType::CompressionSpring:
                return compression > 0.0f ? compression * stiffness - radialSpeed * damping : 0.0f;
            case GuideObjectType::ExtensionSpring:
                return extension > 0.0f ? -extension * stiffness - radialSpeed * damping : 0.0f;
            case GuideObjectType::GarterSpring:
                return extension > 0.0f ? -extension * stiffness * (1.0f + extensionRatio * 1.8f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::VoluteSpring:
                return compression > 0.0f ? compression * stiffness * (1.0f + compressionRatio * 3.2f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::ConstantForceSpring:
                return extension > 0.0f ? -spring.strength - radialSpeed * damping : 0.0f;
            case GuideObjectType::LeafSpring:
                return -displacement * stiffness * (1.0f + fabsf(displacement) / restLength) - radialSpeed * damping;
            case GuideObjectType::BeamSpring:
                return -displacement * stiffness * (1.0f + 0.6f * fabsf(displacement) / restLength) - radialSpeed * damping;
            case GuideObjectType::DiscSpring:
                return compression > 0.0f ? compression * stiffness * (1.0f + compressionRatio * 4.5f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::WaveSpring:
                return compression > 0.0f ? compression * stiffness * (1.0f + compressionRatio * 1.4f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::WaveWasher:
                return compression > 0.0f ? compression * stiffness * (1.0f + compressionRatio * 0.8f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::RingSpring: {
                float friction = radialSpeed == 0.0f ? 0.0f : -copysignf(spring.strength * 0.22f, radialSpeed);
                return -displacement * stiffness - radialSpeed * damping + friction;
            }
            case GuideObjectType::ElastomerSpring:
                return -displacement * stiffness * (1.0f + 2.8f * displacement * displacement / (restLength * restLength)) - radialSpeed * damping;
            case GuideObjectType::PneumaticSpring:
                return compression > 0.0f ? compression * stiffness / fmaxf(0.16f, 1.0f - compressionRatio) - radialSpeed * damping : 0.0f;
            case GuideObjectType::GasSpring:
                return distance <= restLength ? spring.strength + compression * stiffness * (1.0f + compressionRatio * 2.0f) - radialSpeed * damping : 0.0f;
            case GuideObjectType::HydropneumaticSpring:
                return distance <= restLength ? spring.strength + compression * stiffness / fmaxf(0.18f, 1.0f - compressionRatio) - radialSpeed * damping : 0.0f;
            case GuideObjectType::MagneticSpring:
                return -displacement * stiffness * (1.0f + 4.0f * fabsf(displacement) / restLength) - radialSpeed * damping;
            case GuideObjectType::CompositeSpring:
                return -displacement * stiffness * (1.0f + 1.6f * displacement * displacement / (restLength * restLength)) - radialSpeed * damping;
            default:
                return -displacement * stiffness - radialSpeed * damping;
        }
    }

    bool IsPeriodicActive(const GuideObject& object) {
        if (object.interval <= 0.0f) return object.active;
        float cycle = fmodf(fmaxf(0.0f, object.timer), object.interval);
        return object.active && cycle < object.interval * 0.58f;
    }

    bool IsForceEmitterActive(const GuideObject& object) {
        if (object.type == GuideObjectType::RocketThruster) {
            return object.active && (object.power.channel == 0 || object.power.powered);
        }
        return IsPeriodicActive(object);
    }

    Rectangle BeamBounds(const GuideObject& object) {
        Vector2 end = Add(object.transform.position, Scale(object.direction, object.length));
        float margin = fmaxf(2.0f, object.width * 0.5f);
        return {
            fminf(object.transform.position.x, end.x) - margin,
            fminf(object.transform.position.y, end.y) - margin,
            fabsf(end.x - object.transform.position.x) + margin * 2.0f,
            fabsf(end.y - object.transform.position.y) + margin * 2.0f
        };
    }

    void ResolveDynamicObject(GuideObject& object, const std::vector<Rectangle>& solids, float dt) {
        Rectangle bounds = GetGuideObjectBounds(object);
        object.transform.position.x += object.body.velocity.x * dt;
        bounds = GetGuideObjectBounds(object);
        for (Rectangle solid : solids) {
            if (!CheckCollisionRecs(bounds, solid)) continue;
            if (object.body.velocity.x > 0.0f) {
                object.transform.position.x -= bounds.x + bounds.width - solid.x;
            }
            else if (object.body.velocity.x < 0.0f) {
                object.transform.position.x += solid.x + solid.width - bounds.x;
            }
            object.body.velocity.x *= -object.collider.restitution;
            bounds = GetGuideObjectBounds(object);
        }

        object.body.onGround = false;
        object.transform.position.y += object.body.velocity.y * dt;
        bounds = GetGuideObjectBounds(object);
        for (Rectangle solid : solids) {
            if (!CheckCollisionRecs(bounds, solid)) continue;
            if (object.body.velocity.y > 0.0f) {
                object.transform.position.y -= bounds.y + bounds.height - solid.y;
                object.body.onGround = true;
            }
            else if (object.body.velocity.y < 0.0f) {
                object.transform.position.y += solid.y + solid.height - bounds.y;
            }
            object.body.velocity.y *= -object.collider.restitution;
            bounds = GetGuideObjectBounds(object);
        }
    }

    void DrawWheel(Vector2 center, float radius, float rotation, Color rim, int spokes) {
        DrawCircleV(center, radius, rim);
        DrawCircleV(center, radius * 0.72f, Color{41, 45, 48, 255});
        DrawCircleLinesV(center, radius, BLACK);
        for (int i = 0; i < spokes; i++) {
            float angle = (rotation + i * 360.0f / static_cast<float>(spokes)) * DEG2RAD;
            DrawLineEx(center, Add(center, {cosf(angle) * radius * 0.70f, sinf(angle) * radius * 0.70f}), 3.0f, rim);
        }
        DrawCircleV(center, fmaxf(3.0f, radius * 0.18f), DARKGRAY);
    }

    void DrawBolt(Vector2 center, float radius) {
        DrawCircleV(center, radius, GRAY);
        DrawCircleLinesV(center, radius, BLACK);
        DrawLineEx({center.x - radius * 0.55f, center.y}, {center.x + radius * 0.55f, center.y}, 2.0f, BLACK);
    }

    Vector2 LocalPoint(Vector2 origin, Vector2 direction, Vector2 normal, float forward, float side) {
        return {
            origin.x + direction.x * forward + normal.x * side,
            origin.y + direction.y * forward + normal.y * side
        };
    }

    void DrawSolidTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
        DrawTriangle(a, b, c, color);
        DrawTriangle(a, c, b, color);
    }

    void DrawQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
        DrawSolidTriangle(a, b, c, color);
        DrawSolidTriangle(a, c, d, color);
    }

    void DrawCoilBetween(Vector2 start, Vector2 end, float width, int coils, Color color, bool hookedEnds) {
        Vector2 delta{end.x - start.x, end.y - start.y};
        Vector2 normal = NormalizeOr({-delta.y, delta.x});
        Vector2 previous = start;
        for (int i = 1; i <= coils; i++) {
            float amount = static_cast<float>(i) / static_cast<float>(coils);
            Vector2 point = Add(start, Scale(delta, amount));
            if (i < coils) point = Add(point, Scale(normal, (i % 2 == 0 ? -1.0f : 1.0f) * width));
            DrawLineEx(previous, point, 3.0f, BLACK);
            DrawLineEx(previous, point, 1.6f, color);
            previous = point;
        }
        if (hookedEnds) {
            DrawCircleLinesV(start, fmaxf(4.0f, width * 0.62f), color);
            DrawCircleLinesV(end, fmaxf(4.0f, width * 0.62f), color);
        }
        else {
            DrawLineEx(Add(start, Scale(normal, -width)), Add(start, Scale(normal, width)), 3.0f, DARKGRAY);
            DrawLineEx(Add(end, Scale(normal, -width)), Add(end, Scale(normal, width)), 3.0f, DARKGRAY);
        }
    }

    void DrawSpiral(Vector2 center, float radius, float rotation, Color color, int turns) {
        Vector2 previous = center;
        constexpr int segmentsPerTurn = 18;
        int segmentCount = std::max(segmentsPerTurn, turns * segmentsPerTurn);
        for (int i = 1; i <= segmentCount; i++) {
            float amount = static_cast<float>(i) / static_cast<float>(segmentCount);
            float angle = rotation * DEG2RAD + amount * static_cast<float>(turns) * 2.0f * Pi;
            Vector2 point = Add(center, {cosf(angle) * radius * amount, sinf(angle) * radius * amount});
            DrawLineEx(previous, point, 3.0f, BLACK);
            DrawLineEx(previous, point, 1.5f, color);
            previous = point;
        }
    }

    void DrawSpecializedSpring(const GuideObject& object) {
        Vector2 start = object.constraint.anchorA;
        Vector2 end = object.constraint.anchorB;
        Vector2 delta{end.x - start.x, end.y - start.y};
        float distance = fmaxf(1.0f, sqrtf(delta.x * delta.x + delta.y * delta.y));
        Vector2 direction{delta.x / distance, delta.y / distance};
        Vector2 normal{-direction.y, direction.x};
        float width = fmaxf(5.0f, object.width);
        Color springSteel{188, 197, 199, 255};
        Color darkMetal{65, 73, 76, 255};

        switch (object.type) {
            case GuideObjectType::Spring:
                DrawCoilBetween(start, end, width, 10, springSteel, false);
                break;
            case GuideObjectType::CompressionSpring:
                DrawCoilBetween(start, end, width, 12, Color{171, 184, 187, 255}, false);
                break;
            case GuideObjectType::ExtensionSpring:
                DrawCoilBetween(start, end, width, 14, Color{201, 187, 145, 255}, true);
                break;
            case GuideObjectType::CompositeSpring:
                DrawCoilBetween(start, end, width, 9, Color{77, 166, 158, 255}, false);
                DrawLineEx(start, end, 1.0f, Fade(RAYWHITE, 0.42f));
                break;
            case GuideObjectType::TorsionSpring: {
                float radius = fmaxf(11.0f, width * 1.6f);
                DrawRing(start, radius * 0.55f, radius, 0.0f, 360.0f, 30, springSteel);
                DrawLineEx(Add(start, Scale(direction, radius * 0.72f)), end, 4.0f, springSteel);
                DrawLineEx(Add(start, Scale(direction, -radius)), Add(start, Scale(direction, -radius - 16.0f)), 4.0f, springSteel);
                DrawBolt(start, radius * 0.28f);
                break;
            }
            case GuideObjectType::SpiralSpring:
            case GuideObjectType::ConstantTorqueSpring: {
                float radius = std::clamp(distance * 0.28f, 14.0f, 32.0f);
                Color color = object.type == GuideObjectType::ConstantTorqueSpring ? Color{211, 148, 55, 255} : springSteel;
                DrawCircleV(start, radius + 3.0f, darkMetal);
                DrawCircleLinesV(start, radius + 3.0f, BLACK);
                DrawSpiral(start, radius, object.phase, color, object.type == GuideObjectType::ConstantTorqueSpring ? 4 : 3);
                DrawLineEx(Add(start, Scale(direction, radius)), end, 4.0f, color);
                break;
            }
            case GuideObjectType::GarterSpring: {
                DrawRing(start, fmaxf(4.0f, distance - width), distance, 0.0f, 360.0f, 48, Color{167, 180, 182, 255});
                for (int i = 0; i < 14; i++) {
                    float angle = static_cast<float>(i) / 14.0f * 2.0f * Pi;
                    Vector2 marker = Add(start, {cosf(angle) * distance, sinf(angle) * distance});
                    DrawCircleV(marker, 2.0f, BLACK);
                }
                DrawBolt(end, 4.0f);
                break;
            }
            case GuideObjectType::VoluteSpring: {
                constexpr int layers = 7;
                for (int i = 0; i < layers; i++) {
                    float amount = static_cast<float>(i) / static_cast<float>(layers - 1);
                    Vector2 point = Add(start, Scale(delta, amount));
                    float halfWidth = width * (1.35f - amount * 0.75f);
                    DrawLineEx(Add(point, Scale(normal, -halfWidth)), Add(point, Scale(normal, halfWidth)), 5.0f, BLACK);
                    DrawLineEx(Add(point, Scale(normal, -halfWidth + 2.0f)), Add(point, Scale(normal, halfWidth - 2.0f)), 2.0f, Color{148, 117, 67, 255});
                }
                DrawLineEx(start, end, 2.0f, darkMetal);
                break;
            }
            case GuideObjectType::ConstantForceSpring: {
                float spoolRadius = fmaxf(12.0f, width * 1.7f);
                DrawCircleV(start, spoolRadius + 2.0f, BLACK);
                DrawCircleV(start, spoolRadius, Color{83, 91, 94, 255});
                DrawSpiral(start, spoolRadius * 0.78f, object.phase, Color{212, 160, 67, 255}, 3);
                DrawLineEx(Add(start, Scale(direction, spoolRadius)), end, 6.0f, BLACK);
                DrawLineEx(Add(start, Scale(direction, spoolRadius)), end, 3.0f, Color{201, 174, 110, 255});
                break;
            }
            case GuideObjectType::LeafSpring:
            case GuideObjectType::BeamSpring: {
                Vector2 previous = start;
                int layers = object.type == GuideObjectType::LeafSpring ? 3 : 1;
                for (int layer = 0; layer < layers; layer++) {
                    previous = Add(start, Scale(normal, static_cast<float>(layer) * 3.0f));
                    for (int i = 1; i <= 14; i++) {
                        float amount = static_cast<float>(i) / 14.0f;
                        float bow = sinf(amount * Pi) * width * (object.type == GuideObjectType::LeafSpring ? 0.85f : 0.45f);
                        Vector2 point = Add(Add(start, Scale(delta, amount)), Scale(normal, bow + static_cast<float>(layer) * 3.0f));
                        DrawLineEx(previous, point, object.type == GuideObjectType::LeafSpring ? 3.0f : 5.0f,
                            object.type == GuideObjectType::LeafSpring ? Color{101, 112, 116, 255} : Color{151, 162, 165, 255});
                        previous = point;
                    }
                }
                DrawBolt(start, 5.0f);
                DrawBolt(end, 5.0f);
                break;
            }
            case GuideObjectType::DiscSpring:
            case GuideObjectType::WaveSpring:
            case GuideObjectType::WaveWasher:
            case GuideObjectType::RingSpring: {
                int count = object.type == GuideObjectType::WaveWasher ? 3 : (object.type == GuideObjectType::DiscSpring ? 8 : 10);
                for (int i = 0; i < count; i++) {
                    float amount = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
                    Vector2 point = Add(start, Scale(delta, amount));
                    float alternating = i % 2 == 0 ? 1.0f : -1.0f;
                    float halfWidth = object.type == GuideObjectType::RingSpring ? width * 1.25f : width;
                    Color color = object.type == GuideObjectType::RingSpring ? Color{151, 112, 59, 255} : springSteel;
                    DrawLineEx(Add(point, Scale(normal, -halfWidth)), Add(point, Scale(normal, halfWidth)),
                        object.type == GuideObjectType::RingSpring ? 6.0f : 3.0f, BLACK);
                    DrawLineEx(Add(Add(point, Scale(direction, alternating * 2.0f)), Scale(normal, -halfWidth + 1.5f)),
                        Add(Add(point, Scale(direction, -alternating * 2.0f)), Scale(normal, halfWidth - 1.5f)), 2.0f, color);
                }
                DrawLineEx(start, end, 1.5f, darkMetal);
                break;
            }
            case GuideObjectType::TorsionBar:
                DrawLineEx(start, end, width + 5.0f, BLACK);
                DrawLineEx(start, end, width, Color{115, 126, 130, 255});
                for (int i = 1; i < 6; i++) {
                    Vector2 collar = Add(start, Scale(delta, static_cast<float>(i) / 6.0f));
                    DrawLineEx(Add(collar, Scale(normal, -width)), Add(collar, Scale(normal, width)), 2.0f, Color{198, 151, 57, 255});
                }
                DrawBolt(start, width);
                DrawBolt(end, width);
                break;
            case GuideObjectType::ElastomerSpring:
                DrawLineEx(start, end, width * 2.1f + 5.0f, BLACK);
                DrawLineEx(start, end, width * 2.1f, Color{59, 67, 68, 255});
                for (int i = 1; i < 6; i++) {
                    Vector2 band = Add(start, Scale(delta, static_cast<float>(i) / 6.0f));
                    DrawCircleV(band, width * 0.72f, Color{91, 101, 102, 255});
                }
                break;
            case GuideObjectType::PneumaticSpring:
            case GuideObjectType::GasSpring:
            case GuideObjectType::HydropneumaticSpring: {
                float cylinderLength = distance * 0.58f;
                Color bodyColor = object.type == GuideObjectType::PneumaticSpring ? Color{77, 132, 151, 255} :
                    (object.type == GuideObjectType::GasSpring ? Color{93, 105, 109, 255} : Color{52, 105, 121, 255});
                DrawLineEx(start, Add(start, Scale(direction, cylinderLength)), width * 2.0f + 5.0f, BLACK);
                DrawLineEx(start, Add(start, Scale(direction, cylinderLength)), width * 2.0f, bodyColor);
                DrawLineEx(Add(start, Scale(direction, cylinderLength)), end, fmaxf(4.0f, width * 0.55f), LIGHTGRAY);
                DrawLineEx(Add(start, Scale(normal, -width)), Add(start, Scale(normal, width)), 4.0f, BLACK);
                DrawBolt(end, 4.0f);
                if (object.type == GuideObjectType::HydropneumaticSpring) {
                    DrawLineEx(Add(start, Scale(direction, cylinderLength * 0.30f)),
                        Add(start, Scale(direction, cylinderLength * 0.55f)), 3.0f, SKYBLUE);
                }
                break;
            }
            case GuideObjectType::MagneticSpring: {
                float magnetHalf = fmaxf(8.0f, width);
                DrawLineEx(Add(start, Scale(normal, -magnetHalf)), Add(start, Scale(normal, magnetHalf)), 10.0f, RED);
                DrawLineEx(Add(end, Scale(normal, -magnetHalf)), Add(end, Scale(normal, magnetHalf)), 10.0f, RED);
                for (int lane = -1; lane <= 1; lane++) {
                    Vector2 laneOffset = Scale(normal, static_cast<float>(lane) * width * 0.75f);
                    DrawLineEx(Add(Add(start, Scale(direction, 7.0f)), laneOffset),
                        Add(Add(end, Scale(direction, -7.0f)), laneOffset), 1.5f, Fade(SKYBLUE, 0.52f));
                }
                DrawText("N", static_cast<int>(start.x - 5.0f), static_cast<int>(start.y - 7.0f), 14, RAYWHITE);
                DrawText("N", static_cast<int>(end.x - 5.0f), static_cast<int>(end.y - 7.0f), 14, RAYWHITE);
                break;
            }
            default:
                DrawCoilBetween(start, end, width, 10, springSteel, false);
                break;
        }
    }
}

bool ParseGuideObject(const std::string& command, std::istringstream& stream, GuideObject& object) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    GuideObjectType springType = GuideObjectType::Spring;

    if (command == "ball") {
        object.type = GuideObjectType::Ball;
        stream >> x >> y >> object.collider.radius >> object.body.mass;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Dynamic);
        object.collider.restitution = 0.64f;
    }
    else if (command == "barrel" || command == "breakableCrate" || command == "explosiveBarrel") {
        object.type = command == "barrel" ? GuideObjectType::Barrel :
            (command == "breakableCrate" ? GuideObjectType::BreakableCrate : GuideObjectType::ExplosiveBarrel);
        stream >> x >> y >> width >> height >> object.body.mass;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.collider.restitution = object.type == GuideObjectType::Barrel ? 0.18f : 0.05f;
    }
    else if (command == "movingPlatform" || command == "elevator" || command == "crushingBlock") {
        object.type = command == "movingPlatform" ? GuideObjectType::MovingPlatform :
            (command == "elevator" ? GuideObjectType::Elevator : GuideObjectType::CrushingBlock);
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >> object.length >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Kinematic);
        object.direction = NormalizeOr(object.direction, command == "elevator" ? Vector2{0.0f, -1.0f} : Vector2{1.0f, 0.0f});
    }
    else if (command == "pendulumBob" || command == "swingingHammer") {
        object.type = command == "pendulumBob" ? GuideObjectType::PendulumBob : GuideObjectType::SwingingHammer;
        stream >> x >> y >> object.length >> object.collider.radius >> object.phase >> object.speed;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Kinematic);
        object.constraint.kind = ConstraintKind::Hinge;
        object.constraint.anchorA = {x, y};
        object.origin = {x, y};
    }
    else if (command == "oneWayPlatform" || command == "guideRail" || command == "conveyorBelt") {
        object.type = command == "oneWayPlatform" ? GuideObjectType::OneWayPlatform :
            (command == "guideRail" ? GuideObjectType::GuideRail : GuideObjectType::ConveyorBelt);
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        if (object.type == GuideObjectType::ConveyorBelt) stream >> object.speed;
    }
    else if (command == "ceilingHook" || command == "fixedJoint") {
        object.type = command == "ceilingHook" ? GuideObjectType::CeilingHook : GuideObjectType::FixedJoint;
        stream >> x >> y >> object.collider.radius;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
        object.constraint.kind = object.type == GuideObjectType::FixedJoint ? ConstraintKind::Fixed : ConstraintKind::Hinge;
        object.constraint.anchorA = {x, y};
    }
    else if (TryGetSpringType(command, springType) || command == "rod") {
        object.type = command == "rod" ? GuideObjectType::Rod : springType;
        stream >> object.constraint.anchorA.x >> object.constraint.anchorA.y >> object.constraint.anchorB.x >> object.constraint.anchorB.y >> object.width;
        object.transform.position = object.constraint.anchorA;
        object.origin = object.transform.position;
        object.constraint.kind = IsSpringConstraintType(object.type) ? ConstraintKind::Spring : ConstraintKind::Distance;
        object.constraint.restLength = sqrtf(
            powf(object.constraint.anchorB.x - object.constraint.anchorA.x, 2.0f) +
            powf(object.constraint.anchorB.y - object.constraint.anchorA.y, 2.0f));
        if (IsSpringConstraintType(object.type)) {
            ConfigureSpringDefaults(object);
            float stiffness = object.constraint.stiffness;
            float damping = object.constraint.damping;
            if (stream >> stiffness) object.constraint.stiffness = fmaxf(0.0f, stiffness);
            else stream.clear();
            if (stream >> damping) object.constraint.damping = fmaxf(0.0f, damping);
            else stream.clear();
            object.phase = atan2f(
                object.constraint.anchorB.y - object.constraint.anchorA.y,
                object.constraint.anchorB.x - object.constraint.anchorA.x
            ) * RAD2DEG;
        }
    }
    else if (command == "crank" || command == "ratchet" || command == "clutch" || command == "cam" ||
             command == "turntable" || command == "electricMotor") {
        if (command == "crank") object.type = GuideObjectType::Crank;
        else if (command == "ratchet") object.type = GuideObjectType::Ratchet;
        else if (command == "clutch") object.type = GuideObjectType::Clutch;
        else if (command == "cam") object.type = GuideObjectType::Cam;
        else if (command == "turntable") object.type = GuideObjectType::Turntable;
        else object.type = GuideObjectType::ElectricMotor;
        stream >> x >> y >> object.collider.radius >> object.speed;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
        if (object.type == GuideObjectType::Cam) stream >> object.strength;
        if (object.type == GuideObjectType::Clutch) {
            int engaged = 1;
            stream >> engaged;
            object.engaged = engaged != 0;
        }
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.type == GuideObjectType::ElectricMotor ? 0.25f : 0.0f;
    }
    else if (command == "generator") {
        object.type = GuideObjectType::Generator;
        stream >> x >> y >> width >> height >> object.ratedMechanicalSpeed >>
            object.power.maximumPower >> object.generatorEfficiency;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        object.ratedMechanicalSpeed = fmaxf(1.0f, object.ratedMechanicalSpeed);
        object.power.maximumPower = fmaxf(0.0f, object.power.maximumPower);
        object.generatorEfficiency = std::clamp(object.generatorEfficiency, 0.05f, 1.0f);
        ReadOptionalChannel(stream, object);
    }
    else if (command == "brake") {
        object.type = GuideObjectType::Brake;
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "battery" || command == "relay" || command == "fuse" || command == "limitSwitch") {
        if (command == "battery") object.type = GuideObjectType::Battery;
        else if (command == "relay") object.type = GuideObjectType::Relay;
        else if (command == "fuse") object.type = GuideObjectType::Fuse;
        else object.type = GuideObjectType::LimitSwitch;
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        if (object.type == GuideObjectType::Battery || object.type == GuideObjectType::Fuse) stream >> object.strength;
        ReadOptionalChannel(stream, object);
        object.sensor.bounds = GetGuideObjectBounds(object);
        object.power.maximumPower = object.type == GuideObjectType::Battery ? fmaxf(0.0f, object.strength) : 1.0f;
    }
    else if (command == "magnet") {
        object.type = GuideObjectType::Magnet;
        stream >> x >> y >> object.collider.radius >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "piston" || command == "hydraulicCylinder") {
        object.type = command == "piston" ? GuideObjectType::Piston : GuideObjectType::HydraulicCylinder;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >> object.length >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Kinematic);
        object.direction = NormalizeOr(object.direction);
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.type == GuideObjectType::HydraulicCylinder ? 0.4f : 0.15f;
    }
    else if (command == "rocketThruster" || command == "steamVent") {
        object.type = command == "rocketThruster" ? GuideObjectType::RocketThruster : GuideObjectType::SteamVent;
        stream >> x >> y >> object.direction.x >> object.direction.y >> object.length >> object.width >> object.strength;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.direction = NormalizeOr(object.direction, {0.0f, -1.0f});
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        if (object.type == GuideObjectType::SteamVent) stream >> object.interval;
        else ReadOptionalChannel(stream, object);
    }
    else if (command == "sawBlade") {
        object.type = GuideObjectType::SawBlade;
        stream >> x >> y >> object.collider.radius >> object.direction.x >> object.direction.y >> object.length >> object.speed;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Kinematic);
        object.direction = NormalizeOr(object.direction);
    }
    else if (command == "spinnerTrap") {
        object.type = GuideObjectType::SpinnerTrap;
        stream >> x >> y >> object.collider.radius >> object.speed;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Kinematic);
        object.collider.isTrigger = true;
    }
    else if (command == "electricalArc") {
        object.type = GuideObjectType::ElectricalArc;
        stream >> x >> y >> object.constraint.anchorB.x >> object.constraint.anchorB.y >> object.interval;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.length = sqrtf(powf(object.constraint.anchorB.x - x, 2.0f) + powf(object.constraint.anchorB.y - y, 2.0f));
        object.direction = NormalizeOr({object.constraint.anchorB.x - x, object.constraint.anchorB.y - y});
        object.width = 6.0f;
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "oil" || command == "mud") {
        object.type = command == "oil" ? GuideObjectType::Oil : GuideObjectType::Mud;
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "speedSensor" || command == "beamSensor") {
        object.type = command == "speedSensor" ? GuideObjectType::SpeedSensor : GuideObjectType::BeamSensor;
        if (object.type == GuideObjectType::SpeedSensor) {
            stream >> x >> y >> width >> height >> object.sensor.threshold;
            ConfigureRectangle(object, x, y, width, height, BodyType::Static);
            object.sensor.bounds = GetGuideObjectBounds(object);
        }
        else {
            stream >> x >> y >> object.direction.x >> object.direction.y >> object.length;
            object.transform.position = {x, y};
            object.origin = object.transform.position;
            object.direction = NormalizeOr(object.direction);
            object.width = 3.0f;
            object.collider.shape = ColliderShape::Segment;
            object.collider.isTrigger = true;
            object.sensor.bounds = BeamBounds(object);
        }
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "checkpoint") {
        object.type = GuideObjectType::Checkpoint;
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "collectible" || command == "key" || command == "gasMask") {
        object.type = command == "collectible" ? GuideObjectType::Collectible :
            (command == "key" ? GuideObjectType::Key : GuideObjectType::GasMask);
        stream >> x >> y >> object.collider.radius;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else {
        return false;
    }

    InitializeGuideObject(object);
    return true;
}

void InitializeGuideObject(GuideObject& object) {
    object.body.mass = fmaxf(0.1f, object.body.mass);
    object.body.inverseMass = object.body.type == BodyType::Dynamic ? 1.0f / object.body.mass : 0.0f;
    object.length = fmaxf(0.0f, object.length);
    object.width = fmaxf(0.0f, object.width);
    object.interval = fmaxf(0.08f, object.interval);
    object.ratedMechanicalSpeed = fmaxf(1.0f, object.ratedMechanicalSpeed);
    object.generatorEfficiency = std::clamp(object.generatorEfficiency, 0.05f, 1.0f);
    object.mechanicalLoad = std::clamp(object.mechanicalLoad, 0.0f, 1.0f);
    object.previousPosition = object.transform.position;
}

Rectangle GetGuideObjectBounds(const GuideObject& object) {
    if (IsSpringConstraintType(object.type) || object.type == GuideObjectType::Rod) {
        if (object.type == GuideObjectType::GarterSpring) {
            float radius = fmaxf(object.constraint.restLength, object.width);
            return {object.constraint.anchorA.x - radius, object.constraint.anchorA.y - radius,
                radius * 2.0f, radius * 2.0f};
        }
        float margin = fmaxf(10.0f, object.width * 2.0f);
        return {
            fminf(object.constraint.anchorA.x, object.constraint.anchorB.x) - margin,
            fminf(object.constraint.anchorA.y, object.constraint.anchorB.y) - margin,
            fabsf(object.constraint.anchorB.x - object.constraint.anchorA.x) + margin * 2.0f,
            fabsf(object.constraint.anchorB.y - object.constraint.anchorA.y) + margin * 2.0f
        };
    }
    if (object.collider.shape == ColliderShape::Circle) {
        return {
            object.transform.position.x - object.collider.radius,
            object.transform.position.y - object.collider.radius,
            object.collider.radius * 2.0f,
            object.collider.radius * 2.0f
        };
    }
    if (object.collider.shape == ColliderShape::Segment) return BeamBounds(object);
    return {object.transform.position.x, object.transform.position.y, object.collider.size.x, object.collider.size.y};
}

void AppendGuideObjectSolids(std::vector<Rectangle>& solids, const std::vector<GuideObject>& objects) {
    for (const GuideObject& object : objects) {
        if (!object.active || object.broken || object.collider.isTrigger ||
            !IsPlayerCollisionLayer(object.layer)) continue;
        if (IsKinematicSolid(object.type) || IsStaticSolid(object.type)) solids.push_back(GetGuideObjectBounds(object));
    }
}

void UpdateGuideObjects(
    std::vector<GuideObject>& objects,
    const std::array<Player*, 4>& players,
    const std::array<std::vector<Rectangle>, WorldLayerCount>& worldSolidsByLayer,
    float gravity,
    float dt,
    Vector2& checkpoint
) {
    std::unordered_map<int, float> channelPower;
    std::unordered_map<int, float> channelDemand;
    for (const GuideObject& object : objects) {
        if (!object.active || object.broken || object.power.requiredPower <= 0.0f ||
            object.type == GuideObjectType::Battery || object.type == GuideObjectType::Generator) continue;
        channelDemand[object.power.channel] += object.power.requiredPower;
    }

    for (GuideObject& object : objects) {
        if (object.type == GuideObjectType::Battery && object.active && !object.broken) {
            object.power.currentPower = object.power.maximumPower;
            object.power.powered = object.power.currentPower > 0.01f;
            channelPower[object.power.channel] = fmaxf(channelPower[object.power.channel], object.power.currentPower);
        }
        else if (object.type == GuideObjectType::Generator && object.active && !object.broken) {
            const float speedRatio = std::clamp(
                fabsf(object.mechanicalInputSpeed) / object.ratedMechanicalSpeed,
                0.0f,
                1.0f
            );
            object.power.currentPower = speedRatio * object.power.maximumPower * object.generatorEfficiency;
            object.power.powered = object.power.currentPower > 0.01f;
            channelPower[object.power.channel] = fmaxf(channelPower[object.power.channel], object.power.currentPower);

            const float usableCapacity = fmaxf(0.001f,
                object.power.maximumPower * object.generatorEfficiency);
            const float deliveredLoad = fminf(object.power.currentPower, channelDemand[object.power.channel]);
            const float electricalLoad = deliveredLoad / usableCapacity;
            object.mechanicalLoad = std::clamp(speedRatio * 0.06f + electricalLoad * 0.82f, 0.0f, 1.0f);
        }
    }

    for (GuideObject& object : objects) {
        object.previousPosition = object.transform.position;
        object.timer += dt;
        object.sensor.active = false;

        Rectangle bounds = GetGuideObjectBounds(object);
        if (object.type == GuideObjectType::LimitSwitch || object.type == GuideObjectType::SpeedSensor ||
            object.type == GuideObjectType::BeamSensor) {
            Rectangle sensedArea = object.type == GuideObjectType::BeamSensor ? BeamBounds(object) : bounds;
            for (Player* player : players) {
                if (!IsPlayerCollisionLayer(object.layer) || player == nullptr ||
                    !CheckCollisionRecs(sensedArea, player->rect)) continue;
                if (object.type != GuideObjectType::SpeedSensor ||
                    sqrtf(player->velocity.x * player->velocity.x + player->velocity.y * player->velocity.y) >= object.sensor.threshold) {
                    object.sensor.active = true;
                }
            }
            for (const GuideObject& body : objects) {
                if (&body == &object || body.layer != object.layer ||
                    !IsDynamicObject(body.type) || body.broken) continue;
                if (!CheckCollisionRecs(sensedArea, GetGuideObjectBounds(body))) continue;
                float speed = sqrtf(body.body.velocity.x * body.body.velocity.x + body.body.velocity.y * body.body.velocity.y);
                if (object.type != GuideObjectType::SpeedSensor || speed >= object.sensor.threshold) object.sensor.active = true;
            }
            object.triggered = object.sensor.active;
            if (object.sensor.active) channelPower[object.sensor.channel] = 1.0f;
        }
    }

    for (GuideObject& object : objects) {
        float suppliedPower = object.power.channel == 0 ? 1.0f : channelPower[object.power.channel];
        if (object.type != GuideObjectType::Battery && object.type != GuideObjectType::Generator) {
            object.power.currentPower = std::clamp(suppliedPower, 0.0f, object.power.maximumPower);
            object.power.powered = object.power.currentPower + 0.0001f >= object.power.requiredPower;
        }
        if (object.type == GuideObjectType::Fuse && suppliedPower > fmaxf(0.05f, object.strength)) {
            object.broken = true;
            object.power.powered = false;
        }
        if (object.type == GuideObjectType::Relay && object.power.powered) {
            channelPower[object.power.channel] = object.power.currentPower;
        }

        bool poweredMotion = object.power.channel == 0 || object.power.powered;
        if ((object.type == GuideObjectType::MovingPlatform || object.type == GuideObjectType::Elevator ||
             object.type == GuideObjectType::CrushingBlock || object.type == GuideObjectType::Piston ||
             object.type == GuideObjectType::HydraulicCylinder || object.type == GuideObjectType::SawBlade) && poweredMotion) {
            float distance = object.length * (0.5f + 0.5f * sinf(object.timer * object.speed + object.phase));
            object.transform.position = Add(object.origin, Scale(object.direction, distance));
        }
        else if (object.type == GuideObjectType::PendulumBob || object.type == GuideObjectType::SwingingHammer) {
            float angle = sinf(object.timer * object.speed + object.phase) * (object.type == GuideObjectType::SwingingHammer ? 1.05f : 0.78f);
            object.transform.rotation = angle * RAD2DEG;
            object.transform.position = Add(object.origin, {sinf(angle) * object.length, cosf(angle) * object.length});
        }

        float rotationSpeed = object.type == GuideObjectType::Generator
            ? object.mechanicalInputSpeed
            : object.speed;
        if (object.type == GuideObjectType::Ratchet) rotationSpeed = fmaxf(0.0f, rotationSpeed);
        if (object.type == GuideObjectType::Clutch && !object.engaged) rotationSpeed = 0.0f;
        if ((object.type == GuideObjectType::Crank || object.type == GuideObjectType::Ratchet ||
             object.type == GuideObjectType::Clutch || object.type == GuideObjectType::Cam ||
             object.type == GuideObjectType::Turntable || object.type == GuideObjectType::ElectricMotor ||
             object.type == GuideObjectType::Generator || object.type == GuideObjectType::SawBlade ||
             object.type == GuideObjectType::SpinnerTrap) &&
            (poweredMotion || object.type == GuideObjectType::Generator)) {
            object.transform.rotation += rotationSpeed * dt;
        }

        if (IsDynamicObject(object.type) && !object.broken) {
            object.body.velocity.x += object.body.force.x * object.body.inverseMass * dt;
            object.body.velocity.y += (object.body.force.y * object.body.inverseMass + (object.body.affectedByGravity ? gravity : 0.0f)) * dt;
            object.body.angularVelocity += object.body.torque * object.body.inverseMass * dt;
            object.body.force = {};
            object.body.torque = 0.0f;
            object.body.velocity.y = fminf(object.body.velocity.y, 900.0f);
            object.body.angularVelocity *= powf(Clamp01(1.0f - object.body.angularDamping * 0.08f), dt * 60.0f);
            if (object.body.onGround) object.body.velocity.x *= powf(Clamp01(1.0f - object.collider.friction), dt);
            Vector2 beforeVelocity = object.body.velocity;
            ResolveDynamicObject(object, worldSolidsByLayer[WorldLayerIndex(object.layer)], dt);
            object.transform.rotation += object.body.angularVelocity * dt;
            if (IsRoundDynamic(object.type)) {
                object.transform.rotation += object.body.velocity.x / fmaxf(1.0f, object.collider.radius) * RAD2DEG * dt;
            }
            float impact = fabsf(beforeVelocity.x - object.body.velocity.x) + fabsf(beforeVelocity.y - object.body.velocity.y);
            if (object.type == GuideObjectType::BreakableCrate && impact > 440.0f) object.broken = true;
            if (object.type == GuideObjectType::ExplosiveBarrel && impact > 520.0f) {
                object.broken = true;
                object.timer = 0.0f;
            }
        }
    }

    for (int i = 0; i < static_cast<int>(objects.size()); i++) {
        GuideObject& constraint = objects[i];
        if (!IsSpringConstraintType(constraint.type) && constraint.type != GuideObjectType::Rod &&
            constraint.type != GuideObjectType::FixedJoint) continue;

        if (constraint.attachedObject < 0) {
            Vector2 attachment = constraint.type == GuideObjectType::FixedJoint ?
                constraint.transform.position : constraint.constraint.anchorB;
            float closestDistanceSq = 42.0f * 42.0f;
            for (int bodyIndex = 0; bodyIndex < static_cast<int>(objects.size()); bodyIndex++) {
                if (objects[bodyIndex].layer != constraint.layer ||
                    !IsDynamicObject(objects[bodyIndex].type) || objects[bodyIndex].broken) continue;
                Vector2 bodyCenter = Center(GetGuideObjectBounds(objects[bodyIndex]));
                float dx = bodyCenter.x - attachment.x;
                float dy = bodyCenter.y - attachment.y;
                float distanceSq = dx * dx + dy * dy;
                if (distanceSq <= closestDistanceSq) {
                    closestDistanceSq = distanceSq;
                    constraint.attachedObject = bodyIndex;
                    if (IsRotarySpringType(constraint.type)) {
                        constraint.constraint.minimum = objects[bodyIndex].transform.rotation;
                    }
                }
            }
        }
        if (constraint.attachedObject < 0 || constraint.attachedObject >= static_cast<int>(objects.size())) continue;

        GuideObject& body = objects[constraint.attachedObject];
        if (body.broken || !IsDynamicObject(body.type)) {
            constraint.attachedObject = -1;
            continue;
        }
        Vector2 bodyCenter = Center(GetGuideObjectBounds(body));
        Vector2 delta{bodyCenter.x - constraint.constraint.anchorA.x, bodyCenter.y - constraint.constraint.anchorA.y};
        float distance = fmaxf(0.001f, sqrtf(delta.x * delta.x + delta.y * delta.y));
        Vector2 direction{delta.x / distance, delta.y / distance};
        if (IsSpringConstraintType(constraint.type)) {
            float radialSpeed = body.body.velocity.x * direction.x + body.body.velocity.y * direction.y;
            if (IsRotarySpringType(constraint.type)) {
                float angleError = body.transform.rotation - constraint.constraint.minimum;
                float torque = -angleError * constraint.constraint.stiffness -
                    body.body.angularVelocity * constraint.constraint.damping;
                if (constraint.type == GuideObjectType::ConstantTorqueSpring) {
                    torque = -constraint.strength - body.body.angularVelocity * constraint.constraint.damping;
                }
                body.body.torque += torque;
                if (constraint.type == GuideObjectType::TorsionBar) {
                    float tangentialSpeed = body.body.velocity.x * -direction.y + body.body.velocity.y * direction.x;
                    float tangentialForce = -angleError * 0.12f * constraint.constraint.stiffness -
                        tangentialSpeed * constraint.constraint.damping;
                    body.body.velocity.x += -direction.y * tangentialForce * body.body.inverseMass * dt;
                    body.body.velocity.y += direction.x * tangentialForce * body.body.inverseMass * dt;
                }
                float supportForce = -(distance - constraint.constraint.restLength) * constraint.constraint.stiffness -
                    radialSpeed * constraint.constraint.damping;
                body.body.velocity.x += direction.x * supportForce * body.body.inverseMass * dt;
                body.body.velocity.y += direction.y * supportForce * body.body.inverseMass * dt;
            }
            else {
                float force = GetSpringForce(constraint, distance, radialSpeed);
                Vector2 forceDirection = direction;
                if (IsFlexuralSpringType(constraint.type)) {
                    Vector2 restAxis = NormalizeOr({
                        cosf(constraint.phase * DEG2RAD), sinf(constraint.phase * DEG2RAD)
                    });
                    forceDirection = {-restAxis.y, restAxis.x};
                    float side = delta.x * forceDirection.x + delta.y * forceDirection.y;
                    force = -side * constraint.constraint.stiffness -
                        (body.body.velocity.x * forceDirection.x + body.body.velocity.y * forceDirection.y) * constraint.constraint.damping;
                }
                body.body.velocity.x += forceDirection.x * force * body.body.inverseMass * dt;
                body.body.velocity.y += forceDirection.y * force * body.body.inverseMass * dt;
            }
            constraint.constraint.anchorB = bodyCenter;
        }
        else if (constraint.type == GuideObjectType::Rod) {
            Vector2 target = Add(constraint.constraint.anchorA, Scale(direction, constraint.constraint.restLength));
            body.transform.position.x += target.x - bodyCenter.x;
            body.transform.position.y += target.y - bodyCenter.y;
            float radialSpeed = body.body.velocity.x * direction.x + body.body.velocity.y * direction.y;
            body.body.velocity.x -= direction.x * radialSpeed;
            body.body.velocity.y -= direction.y * radialSpeed;
            constraint.constraint.anchorB = target;
        }
        else {
            body.transform.position.x += constraint.transform.position.x - bodyCenter.x;
            body.transform.position.y += constraint.transform.position.y - bodyCenter.y;
            body.body.velocity = {};
        }
    }

    for (int i = 0; i < static_cast<int>(objects.size()); i++) {
        GuideObject& a = objects[i];
        if (!IsDynamicObject(a.type) || a.broken) continue;
        for (int j = i + 1; j < static_cast<int>(objects.size()); j++) {
            GuideObject& b = objects[j];
            if (a.layer != b.layer || !IsDynamicObject(b.type) || b.broken) continue;
            Rectangle aBounds = GetGuideObjectBounds(a);
            Rectangle bBounds = GetGuideObjectBounds(b);
            if (!CheckCollisionRecs(aBounds, bBounds)) continue;

            float overlapX = fminf(aBounds.x + aBounds.width, bBounds.x + bBounds.width) - fmaxf(aBounds.x, bBounds.x);
            float overlapY = fminf(aBounds.y + aBounds.height, bBounds.y + bBounds.height) - fmaxf(aBounds.y, bBounds.y);
            float totalInverseMass = fmaxf(0.001f, a.body.inverseMass + b.body.inverseMass);
            if (overlapX < overlapY) {
                float sign = Center(aBounds).x < Center(bBounds).x ? -1.0f : 1.0f;
                a.transform.position.x += sign * overlapX * a.body.inverseMass / totalInverseMass;
                b.transform.position.x -= sign * overlapX * b.body.inverseMass / totalInverseMass;
                float momentum = a.body.velocity.x * a.body.mass + b.body.velocity.x * b.body.mass;
                float sharedVelocity = momentum / (a.body.mass + b.body.mass);
                a.body.velocity.x = sharedVelocity;
                b.body.velocity.x = sharedVelocity;
            }
            else {
                float sign = Center(aBounds).y < Center(bBounds).y ? -1.0f : 1.0f;
                a.transform.position.y += sign * overlapY * a.body.inverseMass / totalInverseMass;
                b.transform.position.y -= sign * overlapY * b.body.inverseMass / totalInverseMass;
                if (sign < 0.0f) a.body.onGround = true;
                else b.body.onGround = true;
                float momentum = a.body.velocity.y * a.body.mass + b.body.velocity.y * b.body.mass;
                float sharedVelocity = momentum / (a.body.mass + b.body.mass);
                a.body.velocity.y = sharedVelocity;
                b.body.velocity.y = sharedVelocity;
            }
        }
    }

    for (GuideObject& source : objects) {
        if (!source.active || source.broken) continue;
        Rectangle sourceBounds = GetGuideObjectBounds(source);

        for (GuideObject& body : objects) {
            if (&body == &source || body.layer != source.layer ||
                !IsDynamicObject(body.type) || body.broken) continue;
            Rectangle bodyBounds = GetGuideObjectBounds(body);
            Vector2 bodyCenter = Center(bodyBounds);
            if (source.type == GuideObjectType::Magnet && (source.power.channel == 0 || source.power.powered)) {
                Vector2 delta{source.transform.position.x - bodyCenter.x, source.transform.position.y - bodyCenter.y};
                float distanceSq = fmaxf(100.0f, delta.x * delta.x + delta.y * delta.y);
                if (distanceSq <= source.collider.radius * source.collider.radius) {
                    Vector2 direction = NormalizeOr(delta);
                    body.body.velocity.x += direction.x * source.strength * dt / sqrtf(distanceSq) * 18.0f;
                    body.body.velocity.y += direction.y * source.strength * dt / sqrtf(distanceSq) * 18.0f;
                }
            }
            else if ((source.type == GuideObjectType::RocketThruster || source.type == GuideObjectType::SteamVent) &&
                     IsForceEmitterActive(source) && CheckCollisionRecs(BeamBounds(source), bodyBounds)) {
                body.body.velocity.x += source.direction.x * source.strength * dt / body.body.mass;
                body.body.velocity.y += source.direction.y * source.strength * dt / body.body.mass;
            }
            else if (source.type == GuideObjectType::Brake && CheckCollisionRecs(sourceBounds, bodyBounds)) {
                body.body.velocity.x *= powf(Clamp01(1.0f - source.strength), dt);
                body.body.velocity.y *= powf(Clamp01(1.0f - source.strength), dt);
            }
        }

        if (!IsPlayerCollisionLayer(source.layer)) continue;
        for (Player* player : players) {
            if (player == nullptr) continue;
            Rectangle playerRect = player->rect;
            if (source.type == GuideObjectType::OneWayPlatform && player->velocity.y >= 0.0f) {
                float feet = playerRect.y + playerRect.height;
                if (playerRect.x + playerRect.width > sourceBounds.x && playerRect.x < sourceBounds.x + sourceBounds.width &&
                    feet >= sourceBounds.y && feet <= sourceBounds.y + fmaxf(12.0f, player->velocity.y * dt + 4.0f)) {
                    player->rect.y = sourceBounds.y - playerRect.height;
                    player->velocity.y = 0.0f;
                    player->onGround = true;
                }
            }
            if (IsKinematicSolid(source.type)) {
                float oldTop = source.previousPosition.y;
                float feet = playerRect.y + playerRect.height;
                if (playerRect.x + playerRect.width > sourceBounds.x && playerRect.x < sourceBounds.x + sourceBounds.width &&
                    feet >= oldTop - 4.0f && feet <= oldTop + 10.0f && player->velocity.y >= 0.0f) {
                    player->rect.x += source.transform.position.x - source.previousPosition.x;
                    player->rect.y = sourceBounds.y - playerRect.height;
                    player->velocity.y = 0.0f;
                    player->onGround = true;
                }
            }
            if (source.type == GuideObjectType::ConveyorBelt) {
                float feet = playerRect.y + playerRect.height;
                if (playerRect.x + playerRect.width > sourceBounds.x && playerRect.x < sourceBounds.x + sourceBounds.width &&
                    fabsf(feet - sourceBounds.y) < 10.0f) {
                    player->rect.x += source.speed * dt;
                    player->velocity.x += source.speed * 0.08f;
                }
            }
            if (source.type == GuideObjectType::Turntable &&
                CheckCollisionCircleRec(source.transform.position, source.collider.radius, playerRect)) {
                Vector2 radial{Center(playerRect).x - source.transform.position.x, Center(playerRect).y - source.transform.position.y};
                Vector2 tangent = NormalizeOr({-radial.y, radial.x}, {1.0f, 0.0f});
                player->velocity.x += tangent.x * source.speed * DEG2RAD * source.collider.radius * dt;
                player->velocity.y += tangent.y * source.speed * DEG2RAD * source.collider.radius * dt;
            }
            if (source.type == GuideObjectType::Oil && CheckCollisionRecs(playerRect, sourceBounds)) {
                player->velocity.x *= powf(0.985f, dt * 60.0f);
            }
            if (source.type == GuideObjectType::Mud && CheckCollisionRecs(playerRect, sourceBounds)) {
                player->velocity.x *= powf(0.82f, dt * 60.0f);
                player->velocity.y *= powf(0.94f, dt * 60.0f);
            }
            if ((source.type == GuideObjectType::RocketThruster || source.type == GuideObjectType::SteamVent) &&
                IsForceEmitterActive(source) && CheckCollisionRecs(playerRect, BeamBounds(source))) {
                player->velocity.x += source.direction.x * source.strength * dt;
                player->velocity.y += source.direction.y * source.strength * dt;
            }
            if (source.type == GuideObjectType::Magnet && (source.power.channel == 0 || source.power.powered)) {
                Vector2 playerCenter = Center(playerRect);
                Vector2 delta{source.transform.position.x - playerCenter.x, source.transform.position.y - playerCenter.y};
                float distanceSq = fmaxf(100.0f, delta.x * delta.x + delta.y * delta.y);
                if (distanceSq <= source.collider.radius * source.collider.radius) {
                    Vector2 direction = NormalizeOr(delta);
                    player->velocity.x += direction.x * source.strength * dt / sqrtf(distanceSq) * 12.0f;
                    player->velocity.y += direction.y * source.strength * dt / sqrtf(distanceSq) * 12.0f;
                }
            }
            if (source.type == GuideObjectType::Checkpoint && CheckCollisionRecs(playerRect, sourceBounds)) {
                source.triggered = true;
                checkpoint = {sourceBounds.x + sourceBounds.width * 0.5f - playerRect.width * 0.5f, sourceBounds.y - playerRect.height};
            }
            if ((source.type == GuideObjectType::Collectible || source.type == GuideObjectType::Key ||
                 source.type == GuideObjectType::GasMask) &&
                !source.collected && CheckCollisionRecs(playerRect, sourceBounds)) {
                source.collected = true;
            }
        }
    }

    for (GuideObject& body : objects) {
        if (!IsPlayerCollisionLayer(body.layer) || !IsDynamicObject(body.type) || body.broken) continue;
        Rectangle bodyBounds = GetGuideObjectBounds(body);
        for (Player* player : players) {
            if (player == nullptr || !CheckCollisionRecs(player->rect, bodyBounds)) continue;
            float pushScale = 1.0f / sqrtf(body.body.mass);
            if (player->velocity.x > 0.0f) {
                body.body.velocity.x = fmaxf(body.body.velocity.x, player->velocity.x * pushScale);
                player->rect.x = bodyBounds.x - player->rect.width;
            }
            else if (player->velocity.x < 0.0f) {
                body.body.velocity.x = fminf(body.body.velocity.x, player->velocity.x * pushScale);
                player->rect.x = bodyBounds.x + bodyBounds.width;
            }
        }
    }
}

bool IsGuideObjectHazardTouchingPlayer(const GuideObject& object, Rectangle playerRect) {
    if (!object.active || !IsPlayerCollisionLayer(object.layer)) return false;
    if (object.type == GuideObjectType::SpinnerTrap) {
        return CheckCollisionCircleRec(object.transform.position, object.collider.radius * 1.24f, playerRect);
    }
    if (object.type == GuideObjectType::CrushingBlock || object.type == GuideObjectType::SwingingHammer ||
        object.type == GuideObjectType::SawBlade) {
        return CheckCollisionRecs(playerRect, GetGuideObjectBounds(object));
    }
    if (object.type == GuideObjectType::ElectricalArc) {
        return (object.power.channel == 0 || object.power.powered) && IsPeriodicActive(object) &&
            CheckCollisionRecs(playerRect, BeamBounds(object));
    }
    if (object.type == GuideObjectType::ExplosiveBarrel && object.broken && object.timer < 0.35f) {
        return CheckCollisionCircleRec(object.transform.position, object.collider.size.x * 2.4f, playerRect);
    }
    return false;
}

const char* GetGuideObjectName(GuideObjectType type) {
    switch (type) {
        case GuideObjectType::Ball: return "Ball";
        case GuideObjectType::Barrel: return "Barrel";
        case GuideObjectType::MovingPlatform: return "Moving Platform";
        case GuideObjectType::Elevator: return "Elevator";
        case GuideObjectType::PendulumBob: return "Pendulum Bob";
        case GuideObjectType::OneWayPlatform: return "One-Way Platform";
        case GuideObjectType::CeilingHook: return "Ceiling Hook";
        case GuideObjectType::GuideRail: return "Guide Rail";
        case GuideObjectType::Spring: return "Spring";
        case GuideObjectType::CompressionSpring: return "Compression Spring";
        case GuideObjectType::ExtensionSpring: return "Extension Spring";
        case GuideObjectType::TorsionSpring: return "Torsion Spring";
        case GuideObjectType::GarterSpring: return "Garter Spring";
        case GuideObjectType::VoluteSpring: return "Volute Spring";
        case GuideObjectType::SpiralSpring: return "Spiral Spring";
        case GuideObjectType::ConstantForceSpring: return "Constant-Force Spring";
        case GuideObjectType::ConstantTorqueSpring: return "Constant-Torque Spring";
        case GuideObjectType::LeafSpring: return "Leaf Spring";
        case GuideObjectType::BeamSpring: return "Beam Spring";
        case GuideObjectType::DiscSpring: return "Disc Spring";
        case GuideObjectType::WaveSpring: return "Wave Spring";
        case GuideObjectType::WaveWasher: return "Wave Washer";
        case GuideObjectType::TorsionBar: return "Torsion Bar";
        case GuideObjectType::RingSpring: return "Ring Spring";
        case GuideObjectType::ElastomerSpring: return "Elastomer Spring";
        case GuideObjectType::PneumaticSpring: return "Pneumatic Spring";
        case GuideObjectType::GasSpring: return "Gas Spring";
        case GuideObjectType::HydropneumaticSpring: return "Hydropneumatic Spring";
        case GuideObjectType::MagneticSpring: return "Magnetic Spring";
        case GuideObjectType::CompositeSpring: return "Composite Spring";
        case GuideObjectType::Rod: return "Rod";
        case GuideObjectType::FixedJoint: return "Fixed Joint";
        case GuideObjectType::Crank: return "Crank";
        case GuideObjectType::Ratchet: return "Ratchet";
        case GuideObjectType::Clutch: return "Clutch";
        case GuideObjectType::Brake: return "Brake";
        case GuideObjectType::Cam: return "Cam";
        case GuideObjectType::ConveyorBelt: return "Conveyor Belt";
        case GuideObjectType::Turntable: return "Turntable";
        case GuideObjectType::Battery: return "Battery";
        case GuideObjectType::ElectricMotor: return "Electric Motor";
        case GuideObjectType::Generator: return "Generator";
        case GuideObjectType::LimitSwitch: return "Limit Switch";
        case GuideObjectType::Relay: return "Relay";
        case GuideObjectType::Fuse: return "Fuse";
        case GuideObjectType::Magnet: return "Magnet";
        case GuideObjectType::Piston: return "Piston";
        case GuideObjectType::HydraulicCylinder: return "Hydraulic Cylinder";
        case GuideObjectType::RocketThruster: return "Rocket Thruster";
        case GuideObjectType::CrushingBlock: return "Crushing Block";
        case GuideObjectType::SwingingHammer: return "Swinging Hammer";
        case GuideObjectType::SawBlade: return "Saw Blade";
        case GuideObjectType::SpinnerTrap: return "Spinner Trap";
        case GuideObjectType::SteamVent: return "Steam Vent";
        case GuideObjectType::ElectricalArc: return "Electrical Arc";
        case GuideObjectType::Oil: return "Oil";
        case GuideObjectType::Mud: return "Mud";
        case GuideObjectType::SpeedSensor: return "Speed Sensor";
        case GuideObjectType::BeamSensor: return "Beam Sensor";
        case GuideObjectType::Checkpoint: return "Checkpoint";
        case GuideObjectType::Collectible: return "Collectible";
        case GuideObjectType::Key: return "Key";
        case GuideObjectType::GasMask: return "Gas Mask";
        case GuideObjectType::BreakableCrate: return "Breakable Crate";
        case GuideObjectType::ExplosiveBarrel: return "Explosive Barrel";
    }
    return "Object";
}

const char* GetGuideObjectDescription(GuideObjectType type) {
    switch (type) {
        case GuideObjectType::Ball: return "A light rolling body that transfers motion and momentum.";
        case GuideObjectType::Barrel: return "A movable cylinder that rolls, collides, and can carry momentum.";
        case GuideObjectType::MovingPlatform: return "A powered platform that travels back and forth along a fixed path.";
        case GuideObjectType::Elevator: return "A powered platform that carries players and objects vertically.";
        case GuideObjectType::PendulumBob: return "A hanging weight that swings around a fixed pivot.";
        case GuideObjectType::OneWayPlatform: return "Supports objects from above while allowing movement through it from below.";
        case GuideObjectType::CeilingHook: return "A fixed anchor point for hanging mechanisms and constraints.";
        case GuideObjectType::GuideRail: return "A rigid track that supports or guides moving objects.";
        case GuideObjectType::Spring: return "Stores energy when stretched or compressed and pulls an attached object back.";
        case GuideObjectType::CompressionSpring: return "Pushes outward only while compressed below its resting length.";
        case GuideObjectType::ExtensionSpring: return "Pulls inward only after it is stretched beyond its resting length.";
        case GuideObjectType::TorsionSpring: return "Applies restoring torque to return an attached body to its starting angle.";
        case GuideObjectType::GarterSpring: return "A circular tension spring that tightens progressively as it expands.";
        case GuideObjectType::VoluteSpring: return "A heavy conical strip spring with progressive compression resistance.";
        case GuideObjectType::SpiralSpring: return "A flat clock-style coil that stores and returns rotational energy.";
        case GuideObjectType::ConstantForceSpring: return "Pulls with nearly the same force throughout its extension travel.";
        case GuideObjectType::ConstantTorqueSpring: return "Delivers continuous torque in one rotary direction.";
        case GuideObjectType::LeafSpring: return "A curved flexible leaf that supports a load by bending.";
        case GuideObjectType::BeamSpring: return "A flexible beam that resists displacement perpendicular to its length.";
        case GuideObjectType::DiscSpring: return "A compact conical disc that strongly resists short compression travel.";
        case GuideObjectType::WaveSpring: return "A compact axial spring formed from stacked waves of flat wire.";
        case GuideObjectType::WaveWasher: return "A low-profile single-turn washer that cushions short axial movement.";
        case GuideObjectType::TorsionBar: return "A straight bar that stores energy by twisting around its long axis.";
        case GuideObjectType::RingSpring: return "Nested rings combine spring force with strong frictional damping.";
        case GuideObjectType::ElastomerSpring: return "A rubber-like spring with progressive force and heavy damping.";
        case GuideObjectType::PneumaticSpring: return "Compressed air produces rapidly increasing resistance near full compression.";
        case GuideObjectType::GasSpring: return "A preloaded gas strut pushes outward and damps compression.";
        case GuideObjectType::HydropneumaticSpring: return "Gas supports the load while hydraulic fluid heavily damps movement.";
        case GuideObjectType::MagneticSpring: return "A noncontact magnetic field restores an attached body toward equilibrium.";
        case GuideObjectType::CompositeSpring: return "A lightweight composite member with progressive axial response.";
        case GuideObjectType::Rod: return "Keeps an attached object at a fixed distance from its anchor.";
        case GuideObjectType::FixedJoint: return "Locks an attached physics object to a fixed point.";
        case GuideObjectType::Crank: return "Converts rotary motion into motion that can drive another mechanism.";
        case GuideObjectType::Ratchet: return "Allows rotation in one direction while resisting reverse motion.";
        case GuideObjectType::Clutch: return "Engages or disengages the transfer of rotary power.";
        case GuideObjectType::Brake: return "Slows physics objects that enter its contact area.";
        case GuideObjectType::Cam: return "Uses an off-center rotating profile to create repeating motion.";
        case GuideObjectType::ConveyorBelt: return "Moves players and physics objects across its surface.";
        case GuideObjectType::Turntable: return "A powered rotating platform that pushes objects around its center.";
        case GuideObjectType::Battery: return "Supplies electrical power to every object on its channel.";
        case GuideObjectType::ElectricMotor: return "Uses electrical power to produce continuous rotary motion.";
        case GuideObjectType::Generator: return "Converts mechanical shaft rotation into electrical power on its output channel.";
        case GuideObjectType::LimitSwitch: return "Activates its channel when a player or physics object presses it.";
        case GuideObjectType::Relay: return "Passes an electrical signal onward when its input channel is powered.";
        case GuideObjectType::Fuse: return "Carries power until the supplied load exceeds its rating and breaks it.";
        case GuideObjectType::Magnet: return "Pulls nearby players and movable physics objects toward its center.";
        case GuideObjectType::Piston: return "Extends and retracts along a straight powered path.";
        case GuideObjectType::HydraulicCylinder: return "A strong powered actuator that pushes along a straight path.";
        case GuideObjectType::RocketThruster: return "Produces a continuous high-force exhaust stream while powered.";
        case GuideObjectType::CrushingBlock: return "A heavy moving hazard that repeatedly travels along a fixed path.";
        case GuideObjectType::SwingingHammer: return "A heavy pendulum hazard that strikes anything in its arc.";
        case GuideObjectType::SawBlade: return "A powered rotating hazard that damages players on contact.";
        case GuideObjectType::SpinnerTrap: return "A fast exposed propeller hazard that damages players on contact.";
        case GuideObjectType::SteamVent: return "Releases timed bursts of steam that push players and physics objects.";
        case GuideObjectType::ElectricalArc: return "A periodically active electrical hazard spanning the marked gap.";
        case GuideObjectType::Oil: return "A slippery patch that reduces traction and makes stopping difficult.";
        case GuideObjectType::Mud: return "A thick patch that slows horizontal and vertical movement.";
        case GuideObjectType::SpeedSensor: return "Activates its channel when something crosses it above the set speed.";
        case GuideObjectType::BeamSensor: return "Activates its channel when a player or physics object breaks its beam.";
        case GuideObjectType::Checkpoint: return "Updates the player respawn point when touched.";
        case GuideObjectType::Collectible: return "A pickup that disappears when collected.";
        case GuideObjectType::Key: return "A collectible key intended to unlock a matching mechanism.";
        case GuideObjectType::GasMask: return "Protects the wearer from toxic gas until enemy contact knocks it off.";
        case GuideObjectType::BreakableCrate: return "A movable wooden crate that breaks after a sufficiently hard impact.";
        case GuideObjectType::ExplosiveBarrel: return "A movable hazard that explodes after a sufficiently hard impact.";
    }
    return "An interactive object in the level.";
}

void DrawGuideObject(const GuideObject& object) {
    if (!object.active || object.collected) return;
    Rectangle bounds = GetGuideObjectBounds(object);
    Vector2 center = Center(bounds);
    Color steel{102, 112, 118, 255};
    Color darkSteel{50, 57, 62, 255};

    switch (object.type) {
        case GuideObjectType::Ball:
            DrawCircleV(object.transform.position, object.collider.radius, Color{190, 74, 56, 255});
            DrawCircleLinesV(object.transform.position, object.collider.radius, BLACK);
            DrawLineEx(object.transform.position, Add(object.transform.position, {
                cosf(object.transform.rotation * DEG2RAD) * object.collider.radius,
                sinf(object.transform.rotation * DEG2RAD) * object.collider.radius}), 2.0f, RAYWHITE);
            break;
        case GuideObjectType::Barrel:
        case GuideObjectType::ExplosiveBarrel:
            if (object.broken) {
                if (object.type == GuideObjectType::ExplosiveBarrel && object.timer < 0.35f) {
                    DrawCircleV(center, 20.0f + object.timer * 150.0f, Fade(ORANGE, 1.0f - object.timer / 0.35f));
                    DrawRing(center, 8.0f + object.timer * 110.0f, 13.0f + object.timer * 125.0f,
                        0.0f, 360.0f, 28, Fade(YELLOW, 1.0f - object.timer / 0.35f));
                }
                break;
            }
            if (object.type == GuideObjectType::ExplosiveBarrel) {
                Color explosiveRed{145, 39, 31, 255};
                DrawRectangleRec(bounds, explosiveRed);
                DrawRectangleRec({bounds.x, bounds.y, bounds.width * 0.18f, bounds.height}, Color{91, 28, 25, 255});
                DrawRectangleRec({bounds.x + bounds.width * 0.82f, bounds.y, bounds.width * 0.18f, bounds.height}, Color{91, 28, 25, 255});
                DrawEllipse(static_cast<int>(center.x), static_cast<int>(bounds.y + 3.0f), bounds.width * 0.48f, 5.0f, Color{188, 61, 43, 255});
                DrawEllipse(static_cast<int>(center.x), static_cast<int>(bounds.y + bounds.height - 3.0f), bounds.width * 0.48f, 5.0f, Color{77, 27, 25, 255});
                DrawRectangleLinesEx(bounds, 3.0f, BLACK);
                DrawLineEx({bounds.x, bounds.y + 8.0f}, {bounds.x + bounds.width, bounds.y + 8.0f}, 4.0f, darkSteel);
                DrawLineEx({bounds.x, bounds.y + bounds.height - 8.0f}, {bounds.x + bounds.width, bounds.y + bounds.height - 8.0f}, 4.0f, darkSteel);

                float signRadius = fminf(bounds.width, bounds.height) * 0.27f;
                Vector2 signTop{center.x, center.y - signRadius};
                Vector2 signRight{center.x + signRadius, center.y};
                Vector2 signBottom{center.x, center.y + signRadius};
                Vector2 signLeft{center.x - signRadius, center.y};
                DrawSolidTriangle(signTop, signRight, signBottom, BLACK);
                DrawSolidTriangle(signTop, signBottom, signLeft, BLACK);
                float inset = signRadius * 0.78f;
                DrawSolidTriangle({center.x, center.y - inset}, {center.x + inset, center.y},
                    {center.x, center.y + inset}, Color{239, 176, 43, 255});
                DrawSolidTriangle({center.x, center.y - inset}, {center.x, center.y + inset},
                    {center.x - inset, center.y}, Color{239, 176, 43, 255});
                DrawCircleV({center.x, center.y + 3.0f}, signRadius * 0.25f, BLACK);
                DrawSolidTriangle({center.x - signRadius * 0.20f, center.y + 2.0f},
                    {center.x + signRadius * 0.08f, center.y - signRadius * 0.48f},
                    {center.x + signRadius * 0.24f, center.y + 3.0f}, BLACK);
            }
            else {
                DrawRectangleRec(bounds, Color{126, 83, 53, 255});
                DrawRectangleLinesEx(bounds, 2.0f, BLACK);
                DrawLineEx({bounds.x, bounds.y + 7.0f}, {bounds.x + bounds.width, bounds.y + 7.0f}, 3.0f, steel);
                DrawLineEx({bounds.x, bounds.y + bounds.height - 7.0f}, {bounds.x + bounds.width, bounds.y + bounds.height - 7.0f}, 3.0f, steel);
            }
            break;
        case GuideObjectType::BreakableCrate:
            if (!object.broken) {
                DrawRectangleRec(bounds, BROWN);
                DrawRectangleLinesEx(bounds, 3.0f, BLACK);
                DrawLineEx({bounds.x, bounds.y}, {bounds.x + bounds.width, bounds.y + bounds.height}, 3.0f, DARKBROWN);
                DrawLineEx({bounds.x + bounds.width, bounds.y}, {bounds.x, bounds.y + bounds.height}, 3.0f, DARKBROWN);
            }
            break;
        case GuideObjectType::MovingPlatform:
        case GuideObjectType::Elevator:
            DrawRectangleRec(bounds, steel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            for (float x = bounds.x + 10.0f; x < bounds.x + bounds.width; x += 22.0f) DrawBolt({x, bounds.y + bounds.height * 0.5f}, 3.0f);
            if (object.type == GuideObjectType::Elevator) DrawLineEx(object.origin, object.transform.position, 3.0f, DARKGRAY);
            break;
        case GuideObjectType::PendulumBob:
            DrawLineEx(object.origin, object.transform.position, 4.0f, darkSteel);
            DrawCircleV(object.transform.position, object.collider.radius, steel);
            DrawCircleLinesV(object.transform.position, object.collider.radius, BLACK);
            DrawBolt(object.origin, 7.0f);
            break;
        case GuideObjectType::SwingingHammer: {
            Vector2 rod = NormalizeOr({object.transform.position.x - object.origin.x, object.transform.position.y - object.origin.y}, {0.0f, 1.0f});
            Vector2 headAxis{-rod.y, rod.x};
            float radius = object.collider.radius;
            DrawLineEx(object.origin, object.transform.position, 10.0f, BLACK);
            DrawLineEx(object.origin, object.transform.position, 6.0f, Color{73, 80, 83, 255});
            for (float amount = 0.18f; amount < 0.88f; amount += 0.22f) {
                Vector2 collar = Add(object.origin, Scale({object.transform.position.x - object.origin.x,
                    object.transform.position.y - object.origin.y}, amount));
                DrawLineEx(Add(collar, Scale(headAxis, -5.0f)), Add(collar, Scale(headAxis, 5.0f)), 2.0f, LIGHTGRAY);
            }

            float halfWidth = radius * 0.86f;
            float halfHeight = radius * 0.48f;
            Vector2 topLeft = LocalPoint(object.transform.position, rod, headAxis, -halfHeight, -halfWidth);
            Vector2 topRight = LocalPoint(object.transform.position, rod, headAxis, -halfHeight, halfWidth);
            Vector2 bottomRight = LocalPoint(object.transform.position, rod, headAxis, halfHeight, halfWidth);
            Vector2 bottomLeft = LocalPoint(object.transform.position, rod, headAxis, halfHeight, -halfWidth);
            DrawQuad(topLeft, topRight, bottomRight, bottomLeft, BLACK);
            DrawQuad(LocalPoint(object.transform.position, rod, headAxis, -halfHeight + 3.0f, -halfWidth + 3.0f),
                LocalPoint(object.transform.position, rod, headAxis, -halfHeight + 3.0f, halfWidth - 3.0f),
                LocalPoint(object.transform.position, rod, headAxis, halfHeight - 3.0f, halfWidth - 3.0f),
                LocalPoint(object.transform.position, rod, headAxis, halfHeight - 3.0f, -halfWidth + 3.0f), Color{78, 85, 88, 255});
            DrawLineEx(LocalPoint(object.transform.position, rod, headAxis, 0.0f, -halfWidth * 0.62f),
                LocalPoint(object.transform.position, rod, headAxis, 0.0f, halfWidth * 0.62f), 5.0f, Color{222, 145, 34, 255});
            DrawBolt(LocalPoint(object.transform.position, rod, headAxis, 0.0f, -halfWidth * 0.58f), 3.0f);
            DrawBolt(LocalPoint(object.transform.position, rod, headAxis, 0.0f, halfWidth * 0.58f), 3.0f);
            DrawSolidTriangle(topLeft, bottomLeft,
                LocalPoint(object.transform.position, rod, headAxis, 0.0f, -radius * 1.12f), Color{46, 51, 53, 255});
            DrawSolidTriangle(topRight, LocalPoint(object.transform.position, rod, headAxis, 0.0f, radius * 1.12f),
                bottomRight, Color{46, 51, 53, 255});
            DrawBolt(object.origin, 8.0f);
            break;
        }
        case GuideObjectType::OneWayPlatform:
            DrawRectangleRec(bounds, darkSteel);
            DrawLineEx({bounds.x, bounds.y}, {bounds.x + bounds.width, bounds.y}, 4.0f, SKYBLUE);
            for (float x = bounds.x + 12.0f; x < bounds.x + bounds.width; x += 24.0f) DrawTriangle({x - 5.0f, bounds.y + 9.0f}, {x + 5.0f, bounds.y + 9.0f}, {x, bounds.y + 3.0f}, SKYBLUE);
            break;
        case GuideObjectType::CeilingHook:
            DrawRing(object.transform.position, object.collider.radius * 0.58f, object.collider.radius, 0.0f, 300.0f, 20, steel);
            DrawLineEx({object.transform.position.x, object.transform.position.y - object.collider.radius}, {object.transform.position.x, object.transform.position.y - object.collider.radius * 2.0f}, 5.0f, steel);
            break;
        case GuideObjectType::GuideRail:
            DrawRectangleRec(bounds, darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawLineEx({bounds.x + 5.0f, center.y}, {bounds.x + bounds.width - 5.0f, center.y}, 3.0f, LIGHTGRAY);
            break;
        case GuideObjectType::Spring:
        case GuideObjectType::CompressionSpring:
        case GuideObjectType::ExtensionSpring:
        case GuideObjectType::TorsionSpring:
        case GuideObjectType::GarterSpring:
        case GuideObjectType::VoluteSpring:
        case GuideObjectType::SpiralSpring:
        case GuideObjectType::ConstantForceSpring:
        case GuideObjectType::ConstantTorqueSpring:
        case GuideObjectType::LeafSpring:
        case GuideObjectType::BeamSpring:
        case GuideObjectType::DiscSpring:
        case GuideObjectType::WaveSpring:
        case GuideObjectType::WaveWasher:
        case GuideObjectType::TorsionBar:
        case GuideObjectType::RingSpring:
        case GuideObjectType::ElastomerSpring:
        case GuideObjectType::PneumaticSpring:
        case GuideObjectType::GasSpring:
        case GuideObjectType::HydropneumaticSpring:
        case GuideObjectType::MagneticSpring:
        case GuideObjectType::CompositeSpring:
            DrawSpecializedSpring(object);
            break;
        case GuideObjectType::Rod:
            DrawLineEx(object.constraint.anchorA, object.constraint.anchorB, fmaxf(3.0f, object.width), steel);
            DrawBolt(object.constraint.anchorA, 5.0f);
            DrawBolt(object.constraint.anchorB, 5.0f);
            break;
        case GuideObjectType::FixedJoint:
            DrawRectangle(static_cast<int>(center.x - 9), static_cast<int>(center.y - 9), 18, 18, steel);
            DrawBolt(center, 5.0f);
            break;
        case GuideObjectType::Crank: {
            DrawWheel(object.transform.position, object.collider.radius, object.transform.rotation, steel, 4);
            float angle = object.transform.rotation * DEG2RAD;
            Vector2 handle = Add(object.transform.position, {cosf(angle) * object.collider.radius * 1.25f, sinf(angle) * object.collider.radius * 1.25f});
            DrawLineEx(object.transform.position, handle, 5.0f, LIGHTGRAY);
            DrawCircleV(handle, 6.0f, BROWN);
            break;
        }
        case GuideObjectType::Ratchet:
            DrawWheel(object.transform.position, object.collider.radius, object.transform.rotation, steel, 8);
            DrawTriangle({center.x, center.y - object.collider.radius - 8.0f}, {center.x + 13.0f, center.y - object.collider.radius + 4.0f}, {center.x - 4.0f, center.y - object.collider.radius + 7.0f}, ORANGE);
            break;
        case GuideObjectType::Clutch:
            DrawWheel(object.transform.position, object.collider.radius, object.transform.rotation, object.engaged ? GREEN : steel, 6);
            DrawCircleLinesV(object.transform.position, object.collider.radius * 0.48f, object.engaged ? LIME : RED);
            break;
        case GuideObjectType::Brake:
            DrawRectangleRec(bounds, Color{93, 59, 47, 255});
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawText("BRAKE", static_cast<int>(bounds.x + 4.0f), static_cast<int>(center.y - 7.0f), 12, RAYWHITE);
            break;
        case GuideObjectType::Cam: {
            float angle = object.transform.rotation * DEG2RAD;
            Vector2 offset{cosf(angle) * object.strength, sinf(angle) * object.strength};
            DrawCircleV(Add(object.transform.position, offset), object.collider.radius, steel);
            DrawCircleLinesV(Add(object.transform.position, offset), object.collider.radius, BLACK);
            DrawBolt(object.transform.position, 5.0f);
            break;
        }
        case GuideObjectType::ConveyorBelt:
            DrawRectangleRounded(bounds, 0.15f, 4, darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            for (float x = bounds.x + 8.0f; x < bounds.x + bounds.width - 4.0f; x += 18.0f) {
                float offset = fmodf(object.timer * object.speed, 18.0f);
                DrawLineEx({x + offset, bounds.y + 3.0f}, {x + offset, bounds.y + bounds.height - 3.0f}, 2.0f, LIGHTGRAY);
            }
            break;
        case GuideObjectType::Turntable:
            DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), object.collider.radius, object.collider.radius * 0.35f, steel);
            DrawLineEx(center, Add(center, {cosf(object.transform.rotation * DEG2RAD) * object.collider.radius, sinf(object.transform.rotation * DEG2RAD) * object.collider.radius * 0.35f}), 3.0f, ORANGE);
            break;
        case GuideObjectType::Battery:
            DrawRectangleRec(bounds, object.broken ? DARKGRAY : Color{44, 68, 58, 255});
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawRectangle(static_cast<int>(bounds.x + bounds.width * 0.25f), static_cast<int>(bounds.y - 5.0f), 8, 5, LIGHTGRAY);
            DrawRectangle(static_cast<int>(bounds.x + bounds.width * 0.68f), static_cast<int>(bounds.y - 5.0f), 8, 5, LIGHTGRAY);
            DrawText("+", static_cast<int>(bounds.x + 5.0f), static_cast<int>(center.y - 8.0f), 16, YELLOW);
            DrawText("-", static_cast<int>(bounds.x + bounds.width - 14.0f), static_cast<int>(center.y - 8.0f), 16, RAYWHITE);
            break;
        case GuideObjectType::ElectricMotor:
            DrawCircleV(center, object.collider.radius, object.power.powered ? Color{53, 116, 132, 255} : darkSteel);
            DrawCircleLinesV(center, object.collider.radius, BLACK);
            DrawText("M", static_cast<int>(center.x - 8.0f), static_cast<int>(center.y - 10.0f), 20, RAYWHITE);
            break;
        case GuideObjectType::Generator: {
            const float powerAmount = object.power.maximumPower > 0.001f
                ? std::clamp(object.power.currentPower / object.power.maximumPower, 0.0f, 1.0f)
                : 0.0f;
            const Color housing{48, 58, 61, 255};
            const Color copper{190, 105, 48, 255};
            const Color energized = ColorLerp(Color{76, 82, 79, 255}, Color{76, 199, 116, 255}, powerAmount);
            DrawRectangleRec(bounds, housing);
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            DrawRectangleLinesEx(
                {bounds.x + 4.0f, bounds.y + 4.0f, bounds.width - 8.0f, bounds.height - 8.0f},
                1.0f,
                Color{116, 128, 129, 255}
            );

            const Vector2 rotor{
                bounds.x + bounds.width * 0.42f,
                bounds.y + bounds.height * 0.48f
            };
            const float rotorRadius = fmaxf(10.0f, fminf(bounds.width, bounds.height) * 0.31f);
            DrawCircleV(rotor, rotorRadius + 3.0f, BLACK);
            DrawCircleV(rotor, rotorRadius, Color{98, 106, 107, 255});
            DrawRing(rotor, rotorRadius * 0.62f, rotorRadius * 0.88f,
                0.0f, 360.0f, 28, copper);
            for (int spoke = 0; spoke < 4; ++spoke) {
                const float angle = (object.transform.rotation + spoke * 90.0f) * DEG2RAD;
                DrawLineEx(rotor,
                    {rotor.x + cosf(angle) * rotorRadius * 0.72f,
                     rotor.y + sinf(angle) * rotorRadius * 0.72f},
                    3.0f,
                    Color{31, 37, 39, 255});
            }
            DrawCircleV(rotor, rotorRadius * 0.22f + 1.0f, BLACK);
            DrawCircleV(rotor, rotorRadius * 0.22f, Color{204, 165, 74, 255});

            const float coilX = bounds.x + bounds.width * 0.77f;
            for (int winding = -2; winding <= 2; ++winding) {
                DrawLineEx(
                    {coilX - 9.0f, center.y + winding * 5.0f},
                    {coilX + 9.0f, center.y + winding * 5.0f},
                    3.0f,
                    copper
                );
            }
            DrawCircleV({bounds.x + bounds.width - 9.0f, bounds.y + 9.0f}, 4.0f, BLACK);
            DrawCircleV({bounds.x + bounds.width - 9.0f, bounds.y + 9.0f}, 2.5f, energized);
            DrawRectangleRec(
                {bounds.x + bounds.width * 0.66f, bounds.y + bounds.height - 13.0f,
                 bounds.width * 0.27f * powerAmount, 5.0f},
                energized
            );
            break;
        }
        case GuideObjectType::LimitSwitch:
            DrawRectangleRec(bounds, darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawLineEx(center, {bounds.x + bounds.width + 10.0f, bounds.y - 7.0f}, 3.0f, object.sensor.active ? GREEN : LIGHTGRAY);
            break;
        case GuideObjectType::Relay:
            DrawRectangleRec(bounds, object.power.powered ? Color{58, 105, 75, 255} : darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawText("R", static_cast<int>(center.x - 6.0f), static_cast<int>(center.y - 9.0f), 18, RAYWHITE);
            break;
        case GuideObjectType::Fuse:
            DrawRectangleRec(bounds, darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawLineEx({bounds.x + 6.0f, center.y}, {bounds.x + bounds.width - 6.0f, center.y}, 3.0f, object.broken ? RED : ORANGE);
            break;
        case GuideObjectType::Magnet:
            DrawRing(center, object.collider.radius * 0.52f, object.collider.radius * 0.82f, 30.0f, 150.0f, 20, RED);
            DrawRing(center, object.collider.radius * 0.52f, object.collider.radius * 0.82f, 210.0f, 330.0f, 20, BLUE);
            DrawCircleLinesV(center, object.collider.radius, Fade(SKYBLUE, 0.35f));
            break;
        case GuideObjectType::Piston:
        case GuideObjectType::HydraulicCylinder:
            DrawRectangleRec(bounds, object.type == GuideObjectType::HydraulicCylinder ? Color{45, 90, 105, 255} : steel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawLineEx(object.origin, object.transform.position, fmaxf(4.0f, bounds.height * 0.35f), LIGHTGRAY);
            break;
        case GuideObjectType::RocketThruster: {
            Vector2 direction = object.direction;
            Vector2 normal{-direction.y, direction.x};
            float bodyHalf = std::clamp(object.width * 0.25f, 10.0f, 22.0f);
            float bellHalf = std::clamp(object.width * 0.38f, 14.0f, 30.0f);

            Vector2 rearLeft = LocalPoint(center, direction, normal, -50.0f, -bodyHalf);
            Vector2 rearRight = LocalPoint(center, direction, normal, -50.0f, bodyHalf);
            Vector2 bodyLeft = LocalPoint(center, direction, normal, -13.0f, -bodyHalf);
            Vector2 bodyRight = LocalPoint(center, direction, normal, -13.0f, bodyHalf);
            DrawQuad(rearLeft, rearRight, bodyRight, bodyLeft, Color{66, 72, 78, 255});
            DrawLineEx(rearLeft, rearRight, 3.0f, BLACK);
            DrawLineEx(rearRight, bodyRight, 3.0f, BLACK);
            DrawLineEx(bodyLeft, rearLeft, 3.0f, BLACK);

            Vector2 bandLeft = LocalPoint(center, direction, normal, -37.0f, -bodyHalf - 2.0f);
            Vector2 bandRight = LocalPoint(center, direction, normal, -37.0f, bodyHalf + 2.0f);
            DrawLineEx(bandLeft, bandRight, 6.0f, Color{192, 76, 31, 255});
            DrawLineEx(LocalPoint(center, direction, normal, -29.0f, -bodyHalf),
                LocalPoint(center, direction, normal, -29.0f, bodyHalf), 2.0f, LIGHTGRAY);

            Vector2 throatLeft = LocalPoint(center, direction, normal, -13.0f, -bodyHalf);
            Vector2 throatRight = LocalPoint(center, direction, normal, -13.0f, bodyHalf);
            Vector2 mouthLeft = LocalPoint(center, direction, normal, 6.0f, -bellHalf);
            Vector2 mouthRight = LocalPoint(center, direction, normal, 6.0f, bellHalf);
            DrawQuad(throatLeft, throatRight, mouthRight, mouthLeft, steel);
            DrawLineEx(throatLeft, mouthLeft, 3.0f, BLACK);
            DrawLineEx(throatRight, mouthRight, 3.0f, BLACK);
            DrawLineEx(mouthLeft, mouthRight, 5.0f, darkSteel);
            DrawLineEx(mouthLeft, mouthRight, 2.0f, Color{222, 142, 54, 255});

            if (IsForceEmitterActive(object)) {
                float flameLength = std::clamp(object.length * 0.48f, 42.0f, 112.0f);
                float flicker = sinf(object.timer * 23.0f) * 5.0f + sinf(object.timer * 41.0f) * 2.5f;
                Vector2 outerTip = LocalPoint(center, direction, normal, 7.0f + flameLength + flicker, 0.0f);
                Vector2 outerLeft = LocalPoint(center, direction, normal, 8.0f, -bellHalf * 0.72f);
                Vector2 outerRight = LocalPoint(center, direction, normal, 8.0f, bellHalf * 0.72f);
                DrawSolidTriangle(outerLeft, outerTip, outerRight, Color{211, 53, 25, 235});

                Vector2 midTip = LocalPoint(center, direction, normal, 7.0f + flameLength * 0.76f - flicker * 0.25f, 0.0f);
                DrawSolidTriangle(LocalPoint(center, direction, normal, 8.0f, -bellHalf * 0.51f), midTip,
                    LocalPoint(center, direction, normal, 8.0f, bellHalf * 0.51f), ORANGE);
                Vector2 coreTip = LocalPoint(center, direction, normal, 7.0f + flameLength * 0.43f + flicker * 0.12f, 0.0f);
                DrawSolidTriangle(LocalPoint(center, direction, normal, 8.0f, -bellHalf * 0.26f), coreTip,
                    LocalPoint(center, direction, normal, 8.0f, bellHalf * 0.26f), Color{255, 244, 167, 255});

                for (int i = 0; i < 3; i++) {
                    float trail = flameLength + 22.0f + static_cast<float>(i) * 17.0f;
                    float side = sinf(object.timer * 8.0f + static_cast<float>(i) * 2.1f) * (4.0f + i * 2.0f);
                    Vector2 start = LocalPoint(center, direction, normal, trail, side);
                    Vector2 end = LocalPoint(center, direction, normal, trail + 13.0f, side * 1.15f);
                    DrawLineEx(start, end, 2.0f, Fade(LIGHTGRAY, 0.22f - i * 0.045f));
                }
            }
            break;
        }
        case GuideObjectType::SteamVent: {
            Vector2 direction = object.direction;
            Vector2 normal{-direction.y, direction.x};
            float grateHalf = std::clamp(object.width * 0.45f, 14.0f, 36.0f);
            float grateDepth = std::clamp(object.width * 0.22f, 8.0f, 17.0f);
            Vector2 backLeft = LocalPoint(center, direction, normal, -grateDepth, -grateHalf);
            Vector2 backRight = LocalPoint(center, direction, normal, -grateDepth, grateHalf);
            Vector2 frontRight = LocalPoint(center, direction, normal, 5.0f, grateHalf);
            Vector2 frontLeft = LocalPoint(center, direction, normal, 5.0f, -grateHalf);

            DrawQuad(backLeft, backRight, frontRight, frontLeft, Color{49, 55, 59, 255});
            DrawLineEx(backLeft, backRight, 3.0f, BLACK);
            DrawLineEx(backRight, frontRight, 3.0f, BLACK);
            DrawLineEx(frontRight, frontLeft, 3.0f, BLACK);
            DrawLineEx(frontLeft, backLeft, 3.0f, BLACK);
            for (int i = -3; i <= 3; i++) {
                float side = grateHalf * static_cast<float>(i) / 4.0f;
                DrawLineEx(LocalPoint(center, direction, normal, -grateDepth + 3.0f, side),
                    LocalPoint(center, direction, normal, 2.0f, side), 3.0f, Color{121, 132, 137, 255});
            }
            DrawBolt(LocalPoint(center, direction, normal, -grateDepth * 0.35f, -grateHalf + 5.0f), 2.5f);
            DrawBolt(LocalPoint(center, direction, normal, -grateDepth * 0.35f, grateHalf - 5.0f), 2.5f);

            bool venting = IsForceEmitterActive(object);
            int puffCount = venting ? 10 : 2;
            float visibleLength = std::clamp(object.length * 0.68f, 58.0f, 130.0f);
            for (int i = 0; i < puffCount; i++) {
                float phase = fmodf(object.timer * (venting ? 0.52f : 0.13f) +
                    static_cast<float>(i) / static_cast<float>(puffCount), 1.0f);
                float spread = object.width * (0.08f + phase * 0.24f);
                float side = sinf(object.timer * 3.7f + static_cast<float>(i) * 2.17f) * spread;
                float forward = 9.0f + phase * visibleLength;
                float radius = (venting ? 5.0f : 3.0f) + phase * (venting ? 13.0f : 5.0f);
                float alpha = (venting ? 0.42f : 0.16f) * (1.0f - phase);
                Vector2 puff = LocalPoint(center, direction, normal, forward, side);
                DrawCircleV(puff, radius, Fade(Color{205, 216, 219, 255}, alpha));
                DrawCircleV(LocalPoint(puff, direction, normal, -radius * 0.18f, -radius * 0.18f),
                    radius * 0.48f, Fade(RAYWHITE, alpha * 0.72f));
            }
            break;
        }
        case GuideObjectType::CrushingBlock: {
            DrawRectangleRec(bounds, Color{63, 69, 72, 255});
            DrawRectangleLinesEx(bounds, 4.0f, BLACK);
            Rectangle warningBand{bounds.x + 6.0f, bounds.y + bounds.height * 0.34f,
                fmaxf(1.0f, bounds.width - 12.0f), bounds.height * 0.32f};
            DrawRectangleRec(warningBand, Color{222, 148, 34, 255});
            int chevronCount = std::max(1, static_cast<int>(warningBand.width / 18.0f));
            float chevronWidth = warningBand.width / static_cast<float>(chevronCount);
            for (int i = 0; i < chevronCount; i += 2) {
                DrawRectangleRec({warningBand.x + i * chevronWidth, warningBand.y, chevronWidth,
                    warningBand.height}, Color{39, 43, 45, 255});
            }
            for (Vector2 bolt : std::initializer_list<Vector2>{
                {bounds.x + 7.0f, bounds.y + 7.0f}, {bounds.x + bounds.width - 7.0f, bounds.y + 7.0f},
                {bounds.x + 7.0f, bounds.y + bounds.height - 7.0f},
                {bounds.x + bounds.width - 7.0f, bounds.y + bounds.height - 7.0f}}) {
                DrawBolt(bolt, 3.0f);
            }

            Vector2 direction = NormalizeOr(object.direction, {0.0f, 1.0f});
            Vector2 normal{-direction.y, direction.x};
            float faceDistance = fabsf(direction.x) > fabsf(direction.y) ? bounds.width * 0.5f : bounds.height * 0.5f;
            float faceSpan = fabsf(direction.x) > fabsf(direction.y) ? bounds.height : bounds.width;
            Vector2 faceCenter = Add(center, Scale(direction, faceDistance));
            int toothCount = std::max(2, static_cast<int>(faceSpan / 14.0f));
            float toothSpan = faceSpan / static_cast<float>(toothCount);
            for (int i = 0; i < toothCount; i++) {
                float side = -faceSpan * 0.5f + (static_cast<float>(i) + 0.5f) * toothSpan;
                Vector2 left = Add(faceCenter, Scale(normal, side - toothSpan * 0.40f));
                Vector2 right = Add(faceCenter, Scale(normal, side + toothSpan * 0.40f));
                Vector2 tip = Add(Add(faceCenter, Scale(normal, side)), Scale(direction, 8.0f));
                DrawSolidTriangle(left, tip, right, BLACK);
                DrawSolidTriangle(Add(left, Scale(direction, 1.0f)), Add(tip, Scale(direction, -2.0f)),
                    Add(right, Scale(direction, 1.0f)), Color{171, 179, 180, 255});
            }
            break;
        }
        case GuideObjectType::SawBlade: {
            constexpr int toothCount = 12;
            float radius = object.collider.radius;
            Color toothSteel{157, 164, 166, 255};
            Color bladeSteel{105, 114, 118, 255};
            Color warningRed{177, 47, 36, 255};

            // Broad, forward-raked teeth give the blade a readable cutting silhouette.
            for (int i = 0; i < toothCount; i++) {
                float toothAngle = object.transform.rotation + static_cast<float>(i) * (360.0f / toothCount);
                float leadingAngle = (toothAngle - 12.5f) * DEG2RAD;
                float tipAngle = (toothAngle + 5.5f) * DEG2RAD;
                float trailingAngle = (toothAngle + 13.5f) * DEG2RAD;
                Vector2 outlineLeading = Add(center, {cosf(leadingAngle) * radius * 0.66f, sinf(leadingAngle) * radius * 0.66f});
                Vector2 outlineTip = Add(center, {cosf(tipAngle) * radius * 1.16f, sinf(tipAngle) * radius * 1.16f});
                Vector2 outlineTrailing = Add(center, {cosf(trailingAngle) * radius * 0.73f, sinf(trailingAngle) * radius * 0.73f});
                DrawSolidTriangle(outlineLeading, outlineTip, outlineTrailing, BLACK);

                Vector2 leading = Add(center, {cosf(leadingAngle) * radius * 0.71f, sinf(leadingAngle) * radius * 0.71f});
                Vector2 tip = Add(center, {cosf(tipAngle) * radius * 1.08f, sinf(tipAngle) * radius * 1.08f});
                Vector2 trailing = Add(center, {cosf(trailingAngle) * radius * 0.77f, sinf(trailingAngle) * radius * 0.77f});
                DrawSolidTriangle(leading, tip, trailing, toothSteel);

                Vector2 cuttingEdge = Add(center, {cosf((toothAngle + 1.0f) * DEG2RAD) * radius * 0.94f,
                    sinf((toothAngle + 1.0f) * DEG2RAD) * radius * 0.94f});
                DrawLineEx(cuttingEdge, tip, 1.5f, Fade(RAYWHITE, 0.65f));
            }

            DrawCircleV(center, radius * 0.75f, BLACK);
            DrawCircleV(center, radius * 0.69f, bladeSteel);
            DrawRing(center, radius * 0.49f, radius * 0.64f, 0.0f, 360.0f, 32, Color{71, 78, 81, 255});
            DrawCircleLinesV(center, radius * 0.64f, Fade(BLACK, 0.72f));

            for (int i = 0; i < 6; i++) {
                float angle = (object.transform.rotation + 15.0f + static_cast<float>(i) * 60.0f) * DEG2RAD;
                Vector2 slot = Add(center, {cosf(angle) * radius * 0.52f, sinf(angle) * radius * 0.52f});
                DrawCircleV(slot, fmaxf(2.5f, radius * 0.075f), Color{31, 36, 38, 255});
            }

            DrawCircleV(center, radius * 0.31f, BLACK);
            DrawRing(center, radius * 0.16f, radius * 0.28f, 0.0f, 360.0f, 24, warningRed);
            DrawBolt(center, radius * 0.13f);
            DrawLineEx(Add(center, {-radius * 0.18f, -radius * 0.18f}),
                Add(center, {radius * 0.18f, radius * 0.18f}), 2.0f, Fade(RAYWHITE, 0.55f));
            break;
        }
        case GuideObjectType::SpinnerTrap: {
            constexpr int bladeCount = 4;
            constexpr int perimeterToothCount = 12;
            const float radius = object.collider.radius;
            const Color bladeEdge{17, 20, 22, 255};
            const Color bladeSteel{156, 166, 171, 255};
            const Color bladeShadow{75, 84, 89, 255};
            const Color bladeHighlight{238, 243, 244, 255};
            const Color warningOrange{247, 91, 24, 255};
            const Color warningRed{158, 29, 24, 255};

            // A toothed triangular perimeter gives the stationary housing the
            // same dangerous silhouette as the exposed blades.
            DrawCircleV(center, radius * 1.02f, bladeEdge);
            for (int i = 0; i < perimeterToothCount; ++i) {
                const float angle =
                    static_cast<float>(i) * (360.0f / perimeterToothCount) * DEG2RAD;
                const float halfBase = 8.5f * DEG2RAD;
                const Vector2 baseA = Add(center, {
                    cosf(angle - halfBase) * radius * 0.96f,
                    sinf(angle - halfBase) * radius * 0.96f
                });
                const Vector2 point = Add(center, {
                    cosf(angle) * radius * 1.24f,
                    sinf(angle) * radius * 1.24f
                });
                const Vector2 baseB = Add(center, {
                    cosf(angle + halfBase) * radius * 0.96f,
                    sinf(angle + halfBase) * radius * 0.96f
                });
                DrawSolidTriangle(
                    baseA,
                    point,
                    baseB,
                    i % 2 == 0 ? warningOrange : warningRed
                );
                DrawLineEx(baseA, point, 1.5f, bladeEdge);
                DrawLineEx(point, baseB, 1.5f, bladeEdge);
            }

            DrawCircleV(center, radius * 0.86f, Color{40, 47, 51, 255});
            for (int i = 0; i < bladeCount; ++i) {
                const float angle =
                    (object.transform.rotation + static_cast<float>(i) * 90.0f) * DEG2RAD;
                const Vector2 forward{cosf(angle), sinf(angle)};
                const Vector2 side{-forward.y, forward.x};
                const Vector2 rootLeft = Add(
                    Add(center, Scale(forward, radius * 0.14f)),
                    Scale(side, -radius * 0.10f)
                );
                const Vector2 rootRight = Add(
                    Add(center, Scale(forward, radius * 0.14f)),
                    Scale(side, radius * 0.10f)
                );
                const Vector2 shoulderLeft = Add(
                    Add(center, Scale(forward, radius * 0.61f)),
                    Scale(side, -radius * 0.24f)
                );
                const Vector2 shoulderRight = Add(
                    Add(center, Scale(forward, radius * 0.73f)),
                    Scale(side, radius * 0.11f)
                );
                const Vector2 tip = Add(
                    Add(center, Scale(forward, radius * 1.19f)),
                    Scale(side, -radius * 0.12f)
                );

                // Black under-blades preserve a hard outline at game scale.
                DrawSolidTriangle(rootLeft, shoulderLeft, tip, bladeEdge);
                DrawSolidTriangle(rootLeft, tip, rootRight, bladeEdge);
                DrawSolidTriangle(rootRight, tip, shoulderRight, bladeEdge);

                const Vector2 insetRoot = Add(
                    Add(center, Scale(forward, radius * 0.20f)),
                    Scale(side, -radius * 0.055f)
                );
                const Vector2 insetShoulder = Add(
                    Add(center, Scale(forward, radius * 0.62f)),
                    Scale(side, -radius * 0.18f)
                );
                const Vector2 insetTip = Add(
                    Add(center, Scale(forward, radius * 1.10f)),
                    Scale(side, -radius * 0.105f)
                );
                const Vector2 insetTrailing = Add(
                    Add(center, Scale(forward, radius * 0.66f)),
                    Scale(side, radius * 0.07f)
                );
                DrawSolidTriangle(insetRoot, insetShoulder, insetTip, bladeSteel);
                DrawSolidTriangle(insetRoot, insetTip, insetTrailing, bladeShadow);
                DrawLineEx(insetShoulder, insetTip, 1.6f, bladeHighlight);
            }

            // An angular hub reinforces the trap's triangular design language.
            DrawCircleV(center, radius * 0.31f, bladeEdge);
            DrawPoly(center, 6, radius * 0.25f, object.transform.rotation + 30.0f, warningRed);
            DrawPoly(center, 3, radius * 0.17f, object.transform.rotation, warningOrange);
            DrawCircleV(center, radius * 0.075f, bladeHighlight);
            break;
        }
        case GuideObjectType::ElectricalArc: {
            Vector2 end = Add(object.transform.position, Scale(object.direction, object.length));
            Vector2 normal{-object.direction.y, object.direction.x};
            auto drawElectrode = [&](Vector2 position, float facing) {
                Vector2 rear = Add(position, Scale(object.direction, -facing * 13.0f));
                DrawLineEx(Add(rear, Scale(normal, -9.0f)), Add(rear, Scale(normal, 9.0f)), 9.0f, BLACK);
                DrawLineEx(Add(rear, Scale(normal, -8.0f)), Add(rear, Scale(normal, 8.0f)), 5.0f, Color{88, 96, 100, 255});
                DrawCircleV(position, 8.0f, BLACK);
                DrawCircleV(position, 5.0f, Color{199, 169, 67, 255});
                Vector2 prong = Add(position, Scale(object.direction, facing * 8.0f));
                DrawLineEx(position, prong, 4.0f, LIGHTGRAY);
            };
            drawElectrode(object.transform.position, 1.0f);
            drawElectrode(end, -1.0f);

            if (!IsPeriodicActive(object)) {
                float warningPulse = 0.55f + sinf(object.timer * 8.0f) * 0.18f;
                DrawCircleV(object.transform.position, 11.0f, Fade(ORANGE, warningPulse * 0.28f));
                DrawCircleV(end, 11.0f, Fade(ORANGE, warningPulse * 0.28f));
                DrawLineEx(Add(object.transform.position, Scale(object.direction, 9.0f)),
                    Add(object.transform.position, Scale(object.direction, 17.0f)), 2.0f, Fade(SKYBLUE, 0.52f));
                DrawLineEx(Add(end, Scale(object.direction, -9.0f)),
                    Add(end, Scale(object.direction, -17.0f)), 2.0f, Fade(SKYBLUE, 0.52f));
                break;
            }

            Vector2 previous = object.transform.position;
            constexpr int arcSegments = 10;
            for (int i = 1; i <= arcSegments; i++) {
                float t = static_cast<float>(i) / static_cast<float>(arcSegments);
                Vector2 point = Add(object.transform.position, Scale({end.x - object.transform.position.x, end.y - object.transform.position.y}, t));
                if (i < arcSegments) {
                    float jitter = sinf(object.timer * 31.0f + static_cast<float>(i) * 4.7f) * 7.0f;
                    point = Add(point, Scale(normal, jitter));
                }
                DrawLineEx(previous, point, 8.0f, Fade(SKYBLUE, 0.24f));
                DrawLineEx(previous, point, 4.0f, Color{80, 185, 255, 255});
                DrawLineEx(previous, point, 1.5f, RAYWHITE);
                if (i == 3 || i == 7) {
                    Vector2 branchEnd = Add(point, Scale(normal, i == 3 ? 15.0f : -15.0f));
                    branchEnd = Add(branchEnd, Scale(object.direction, 8.0f));
                    DrawLineEx(point, branchEnd, 2.0f, Fade(SKYBLUE, 0.82f));
                }
                previous = point;
            }
            break;
        }
        case GuideObjectType::Oil:
            DrawRectangleRec(bounds, Fade(Color{31, 27, 36, 255}, 0.86f));
            for (float x = bounds.x + 7.0f; x < bounds.x + bounds.width; x += 17.0f) DrawCircleV({x, bounds.y + 3.0f}, 3.0f, Fade(PURPLE, 0.55f));
            break;
        case GuideObjectType::Mud:
            DrawRectangleRec(bounds, Color{83, 66, 47, 220});
            for (float x = bounds.x + 8.0f; x < bounds.x + bounds.width; x += 20.0f) DrawCircleV({x, bounds.y + 4.0f}, 4.0f, DARKBROWN);
            break;
        case GuideObjectType::SpeedSensor:
            DrawRectangleRec(bounds, object.sensor.active ? Color{52, 121, 78, 255} : darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawText(">", static_cast<int>(center.x - 6.0f), static_cast<int>(center.y - 9.0f), 20, RAYWHITE);
            break;
        case GuideObjectType::BeamSensor: {
            Vector2 end = Add(object.transform.position, Scale(object.direction, object.length));
            DrawCircleV(object.transform.position, 7.0f, darkSteel);
            DrawLineEx(object.transform.position, end, 2.0f, object.sensor.active ? GREEN : RED);
            break;
        }
        case GuideObjectType::Checkpoint:
            DrawLineEx({bounds.x + 5.0f, bounds.y + bounds.height}, {bounds.x + 5.0f, bounds.y}, 4.0f, LIGHTGRAY);
            DrawTriangle({bounds.x + 7.0f, bounds.y}, {bounds.x + bounds.width, bounds.y + bounds.height * 0.25f}, {bounds.x + 7.0f, bounds.y + bounds.height * 0.5f}, object.triggered ? GREEN : ORANGE);
            break;
        case GuideObjectType::Collectible:
            DrawCircleV(center, object.collider.radius, GOLD);
            DrawCircleLinesV(center, object.collider.radius, BLACK);
            DrawCircleV(center, object.collider.radius * 0.42f, YELLOW);
            break;
        case GuideObjectType::Key:
            DrawCircleLinesV(center, object.collider.radius * 0.48f, GOLD);
            DrawLineEx({center.x + object.collider.radius * 0.45f, center.y}, {center.x + object.collider.radius * 1.35f, center.y}, 5.0f, GOLD);
            DrawLineEx({center.x + object.collider.radius, center.y}, {center.x + object.collider.radius, center.y + object.collider.radius * 0.55f}, 4.0f, GOLD);
            break;
        case GuideObjectType::GasMask:
            DrawCircleV(center, object.collider.radius, Color{93, 117, 92, 255});
            DrawCircleV({center.x - object.collider.radius * 0.42f, center.y}, object.collider.radius * 0.32f, Color{31, 40, 34, 255});
            DrawCircleV({center.x + object.collider.radius * 0.42f, center.y}, object.collider.radius * 0.32f, Color{31, 40, 34, 255});
            break;
    }
}
