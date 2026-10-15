#include "GuideObjects.h"
#include "Level.h"

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

    int ParseColorMask(const std::string& value) {
        if (value == "red" || value == "r") return 1;
        if (value == "green" || value == "g") return 2;
        if (value == "blue" || value == "b") return 4;
        if (value == "yellow" || value == "rg") return 3;
        if (value == "magenta" || value == "rb") return 5;
        if (value == "cyan" || value == "gb") return 6;
        if (value == "white" || value == "rgb") return 7;
        return 0;
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
            type == GuideObjectType::CrushingBlock || type == GuideObjectType::SpokePress ||
            type == GuideObjectType::RotoTransConverter;
    }

    bool IsStaticSolid(GuideObjectType type) {
        return type == GuideObjectType::GuideRail || type == GuideObjectType::ConveyorBelt ||
            type == GuideObjectType::Trampoline || type == GuideObjectType::DestructibleWood ||
            type == GuideObjectType::DestructibleBrick || type == GuideObjectType::DestructibleGlass ||
            type == GuideObjectType::GlassPane || type == GuideObjectType::GlassBlock ||
            type == GuideObjectType::IceBlock || type == GuideObjectType::IceSheet ||
            type == GuideObjectType::Heater || type == GuideObjectType::Campfire;
    }

    bool IsRoundDynamic(GuideObjectType type) {
        return type == GuideObjectType::Ball || type == GuideObjectType::ProgrammableBall ||
            type == GuideObjectType::HotAirBalloon;
    }

    bool IsDynamicObject(GuideObjectType type) {
        return type == GuideObjectType::Ball || type == GuideObjectType::Barrel ||
            type == GuideObjectType::BreakableCrate || type == GuideObjectType::ExplosiveBarrel ||
            type == GuideObjectType::Tnt ||
            type == GuideObjectType::LoadCart || type == GuideObjectType::RockHammer ||
            type == GuideObjectType::ProgrammableBall || type == GuideObjectType::HotAirBalloon ||
            type == GuideObjectType::Blimp || type == GuideObjectType::LeakyBucket ||
            type == GuideObjectType::Torch || type == GuideObjectType::WaterBucket;
    }

    bool IsExplosive(GuideObjectType type) {
        return type == GuideObjectType::ExplosiveBarrel || type == GuideObjectType::Tnt;
    }

    float ExplosionRadius(const GuideObject& object) {
        return object.type == GuideObjectType::Tnt
            ? fmaxf(80.0f, object.strength)
            : fmaxf(96.0f, object.collider.size.x * 2.4f);
    }

    Vector2 ExplosionCenter(const GuideObject& object) {
        return Center(GetGuideObjectBounds(object));
    }

    bool IsDestructibleMaterial(GuideObjectType type) {
        return type == GuideObjectType::DestructibleWood || type == GuideObjectType::DestructibleBrick ||
            type == GuideObjectType::DestructibleGlass || type == GuideObjectType::GlassPane ||
            type == GuideObjectType::GlassBlock || type == GuideObjectType::IceBlock ||
            type == GuideObjectType::IceSheet;
    }

    bool IsLightBeam(GuideObjectType type) {
        return type == GuideObjectType::LightSource || type == GuideObjectType::LaserEmitter ||
            type == GuideObjectType::LaserMixer;
    }

    bool IsLeedskalninLifter(GuideObjectType type) {
        return type == GuideObjectType::ChainHoist || type == GuideObjectType::BlockAndTackle ||
            type == GuideObjectType::ComeAlong;
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

    Vector2 TeleforceMuzzle(const GuideObject& object) {
        const Rectangle bounds = GetGuideObjectBounds(object);
        const Vector2 center = Center(bounds);
        const float extent = fabsf(object.direction.x) * bounds.width * 0.5f +
            fabsf(object.direction.y) * bounds.height * 0.5f;
        return Add(center, Scale(object.direction, extent));
    }

    bool RectangleTouchesTeleforceBeam(const GuideObject& object, Rectangle target) {
        const Vector2 muzzle = TeleforceMuzzle(object);
        const Vector2 normal{-object.direction.y, object.direction.x};
        const Vector2 targetCenter = Center(target);
        const Vector2 delta{targetCenter.x - muzzle.x, targetCenter.y - muzzle.y};
        const float along = delta.x * object.direction.x + delta.y * object.direction.y;
        const float side = delta.x * normal.x + delta.y * normal.y;
        const float halfWidth = target.width * 0.5f;
        const float halfHeight = target.height * 0.5f;
        const float alongRadius = fabsf(object.direction.x) * halfWidth +
            fabsf(object.direction.y) * halfHeight;
        const float sideRadius = fabsf(normal.x) * halfWidth + fabsf(normal.y) * halfHeight;
        return along >= -alongRadius && along <= object.length + alongRadius &&
            fabsf(side) <= object.width * 0.5f + sideRadius;
    }

    Vector2 TeslaCoilTerminal(const GuideObject& object) {
        const Rectangle bounds = GetGuideObjectBounds(object);
        return {bounds.x + bounds.width * 0.5f, bounds.y + fmaxf(5.0f, bounds.height * 0.08f)};
    }

    void ResolveDynamicObject(GuideObject& object, const std::vector<Rectangle>& solids, float dt, const Level* tileLevel) {
        Rectangle bounds = GetGuideObjectBounds(object);
        auto horizontalSolids = solids;
        if (tileLevel) AppendDirectionalTileSolids(horizontalSolids, *tileLevel, object.layer, bounds, object.body.velocity, true);
        object.transform.position.x += object.body.velocity.x * dt;
        bounds = GetGuideObjectBounds(object);
        for (Rectangle solid : horizontalSolids) {
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
        auto verticalSolids = solids;
        if (tileLevel) AppendDirectionalTileSolids(verticalSolids, *tileLevel, object.layer, bounds, object.body.velocity, false);
        object.transform.position.y += object.body.velocity.y * dt;
        bounds = GetGuideObjectBounds(object);
        for (Rectangle solid : verticalSolids) {
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

    Color LaserColor(int mask, unsigned char alpha = 255) {
        const unsigned char low = 36;
        return {
            static_cast<unsigned char>((mask & 1) ? 255 : low),
            static_cast<unsigned char>((mask & 2) ? 255 : low),
            static_cast<unsigned char>((mask & 4) ? 255 : low),
            alpha
        };
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

void CycleTernarySwitch(GuideObject& object) {
    if (object.type != GuideObjectType::TernarySwitch) return;
    object.signalMask = object.signalMask < 1 ? object.signalMask + 1 : -1;
    object.colorMask = 15;
}

bool ParseGuideObject(const std::string& command, std::istringstream& stream, GuideObject& object) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    GuideObjectType springType = GuideObjectType::Spring;

    if (command == "rock") {
        object.type = GuideObjectType::Rock;
        if (!(stream >> x >> y >> width >> height) || width <= 0 || height <= 0) return false;
        if (!(stream >> object.transform.rotation)) stream.clear();
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "thornyVines") {
        object.type = GuideObjectType::ThornyVines;
        if (!(stream >> x >> y >> width >> height) || width <= 0.0f || height <= 0.0f) return false;
        object.damage = 0.34f;
        if (!(stream >> object.damage)) stream.clear();
        object.damage = fmaxf(0.0f, object.damage);
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "ball") {
        object.type = GuideObjectType::Ball;
        stream >> x >> y >> object.collider.radius >> object.body.mass;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Dynamic);
        object.collider.restitution = 0.64f;
    }
    else if (command == "barrel" || command == "breakableCrate" || command == "explosiveBarrel" || command == "tnt") {
        object.type = command == "barrel" ? GuideObjectType::Barrel :
            (command == "breakableCrate" ? GuideObjectType::BreakableCrate :
             command == "tnt" ? GuideObjectType::Tnt : GuideObjectType::ExplosiveBarrel);
        stream >> x >> y >> width >> height >> object.body.mass;
        if (object.type == GuideObjectType::Tnt) {
            object.interval = 1.5f;
            object.strength = 180.0f;
            if (!(stream >> object.interval)) stream.clear();
            if (!(stream >> object.strength)) stream.clear();
            object.interval = fmaxf(0.1f, object.interval);
            object.strength = fmaxf(80.0f, object.strength);
        }
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.collider.restitution = object.type == GuideObjectType::Barrel ? 0.18f : 0.05f;
        if (object.type == GuideObjectType::Tnt) object.body.angularDamping = 0.72f;
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
    else if (command == "antiGravityPad") {
        object.type = GuideObjectType::AntiGravityPad;
        stream >> x >> y >> width >> height >> object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        object.length = fmaxf(1.0f, object.length);
        object.strength = fmaxf(0.0f, object.strength);
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.35f;
    }
    else if (command == "teleforce") {
        object.type = GuideObjectType::Teleforce;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.width >> object.strength >> object.capacity >> object.damage >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
        object.length = fmaxf(24.0f, object.length);
        object.width = fmaxf(3.0f, object.width);
        object.strength = fmaxf(0.0f, object.strength);
        object.capacity = fmaxf(0.05f, object.capacity);
        object.damage = fmaxf(0.03f, object.damage);
        object.speed = fmaxf(0.05f, object.speed);
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.9f;
    }
    else if (command == "teslaCoil") {
        object.type = GuideObjectType::TeslaCoil;
        stream >> x >> y >> width >> height >> object.length >> object.strength >>
            object.capacity >> object.damage >> object.speed >> object.power.channel >> object.sensor.channel;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        object.length = fmaxf(16.0f, object.length);
        object.strength = fmaxf(0.0f, object.strength);
        object.capacity = fmaxf(0.05f, object.capacity);
        object.damage = fmaxf(0.03f, object.damage);
        object.speed = fmaxf(0.05f, object.speed);
        object.power.requiredPower = 0.75f;
    }
    else if (command == "tripodDerrick") {
        object.type = GuideObjectType::TripodDerrick;
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "chainHoist" || command == "blockAndTackle") {
        object.type = command == "chainHoist" ? GuideObjectType::ChainHoist : GuideObjectType::BlockAndTackle;
        stream >> x >> y >> object.length >> object.speed >> object.strength;
        ConfigureCircle(object, x, y, 15.0f, BodyType::Static);
        object.collider.isTrigger = true;
        object.constraint.anchorA = {x, y};
        object.constraint.anchorB = {x, y + object.length};
        object.constraint.kind = ConstraintKind::Slider;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.2f;
    }
    else if (command == "comeAlong") {
        object.type = GuideObjectType::ComeAlong;
        stream >> x >> y >> object.constraint.anchorB.x >> object.constraint.anchorB.y >>
            object.speed >> object.strength;
        ConfigureCircle(object, x, y, 13.0f, BodyType::Static);
        object.collider.isTrigger = true;
        object.constraint.anchorA = {x, y};
        object.length = sqrtf(
            powf(object.constraint.anchorB.x - x, 2.0f) +
            powf(object.constraint.anchorB.y - y, 2.0f));
        object.constraint.kind = ConstraintKind::Slider;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.15f;
    }
    else if (command == "manualBenchGrinder") {
        object.type = GuideObjectType::ManualBenchGrinder;
        stream >> x >> y >> object.collider.radius >> object.speed;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "loadCart" || command == "rockHammer") {
        object.type = command == "loadCart" ? GuideObjectType::LoadCart : GuideObjectType::RockHammer;
        stream >> x >> y >> width >> height >> object.body.mass;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.collider.friction = object.type == GuideObjectType::LoadCart ? 0.16f : 0.58f;
        object.collider.restitution = object.type == GuideObjectType::RockHammer ? 0.12f : 0.03f;
    }
    else if (command == "revolvingStoneGate") {
        object.type = GuideObjectType::RevolvingStoneGate;
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        object.strength = std::clamp(object.strength, 0.0f, 1.0f);
    }
    else if (command == "fieldCoil") {
        object.type = GuideObjectType::FieldCoil;
        stream >> x >> y >> object.collider.radius >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.25f;
    }
    else if (command == "spokePress") {
        object.type = GuideObjectType::SpokePress;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Kinematic);
        object.direction = NormalizeOr(object.direction, {0.0f, 1.0f});
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.3f;
    }
    else if (command == "polarisTelescope") {
        object.type = GuideObjectType::PolarisTelescope;
        float angle = 0.0f;
        stream >> x >> y >> object.length >> angle;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.direction = {cosf(angle * DEG2RAD), sinf(angle * DEG2RAD)};
        object.width = 5.0f;
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        object.sensor.bounds = BeamBounds(object);
        ReadOptionalChannel(stream, object);
    }
    else if (command == "leedskalninSundial") {
        object.type = GuideObjectType::Sundial;
        stream >> x >> y >> object.collider.radius >> object.interval;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "perpetualMotionHolder") {
        object.type = GuideObjectType::PerpetualMotionHolder;
        stream >> x >> y >> width >> height >> object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.2f;
    }
    else if (command == "bellows") {
        object.type = GuideObjectType::Bellows;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
    }
    else if (command == "trampoline") {
        object.type = GuideObjectType::Trampoline;
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.strength = fmaxf(100.0f, object.strength);
    }
    else if (command == "boxingGlove") {
        object.type = GuideObjectType::BoxingGlove;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.15f;
    }
    else if (command == "vacuum") {
        object.type = GuideObjectType::Vacuum;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.25f;
    }
    else if (command == "magnifyingGlass") {
        object.type = GuideObjectType::MagnifyingGlass;
        stream >> x >> y >> object.collider.radius >> object.direction.x >> object.direction.y >> object.length;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
    }
    else if (command == "lightSource") {
        object.type = GuideObjectType::LightSource;
        stream >> x >> y >> object.direction.x >> object.direction.y >> object.length >> object.width;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.direction = NormalizeOr(object.direction);
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = 0.15f;
    }
    else if (command == "ignitableFuse") {
        object.type = GuideObjectType::IgnitableFuse;
        stream >> x >> y >> width >> height;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "programmableBall") {
        object.type = GuideObjectType::ProgrammableBall;
        stream >> x >> y >> object.collider.radius >> object.body.mass >>
            object.collider.restitution >> object.collider.friction >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Dynamic);
        object.collider.restitution = Clamp01(object.collider.restitution);
        object.collider.friction = Clamp01(object.collider.friction);
        object.strength = std::clamp(object.strength, -2.0f, 3.0f);
    }
    else if (command == "transRotoConverter") {
        object.type = GuideObjectType::TransRotoConverter;
        stream >> x >> y >> object.collider.radius >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "rotoTransConverter") {
        object.type = GuideObjectType::RotoTransConverter;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Kinematic);
        object.direction = NormalizeOr(object.direction);
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.2f;
    }
    else if (command == "acceleratorTube") {
        object.type = GuideObjectType::AcceleratorTube;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.25f;
    }
    else if (command == "pinballBumper") {
        object.type = GuideObjectType::PinballBumper;
        stream >> x >> y >> object.collider.radius >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "pinballFlipper") {
        object.type = GuideObjectType::PinballFlipper;
        stream >> x >> y >> width >> height >> object.direction.x >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction.x = object.direction.x < 0.0f ? -1.0f : 1.0f;
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.15f;
    }
    else if (command == "laserEmitter") {
        std::string color;
        object.type = GuideObjectType::LaserEmitter;
        stream >> x >> y >> object.direction.x >> object.direction.y >> object.length >> color;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.direction = NormalizeOr(object.direction);
        object.width = 3.0f;
        object.colorMask = ParseColorMask(color);
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.1f;
    }
    else if (command == "laserMixer") {
        object.type = GuideObjectType::LaserMixer;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >> object.length;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        object.collider.isTrigger = true;
    }
    else if (command == "laserDetector") {
        std::string color;
        object.type = GuideObjectType::LaserDetector;
        stream >> x >> y >> width >> height >> color;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.colorMask = ParseColorMask(color);
        object.collider.isTrigger = true;
        ReadOptionalChannel(stream, object);
    }
    else if (command == "hotAirBalloon") {
        object.type = GuideObjectType::HotAirBalloon;
        stream >> x >> y >> object.collider.radius >> object.body.mass >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Dynamic);
        object.collider.restitution = 0.12f;
        object.collider.friction = 0.18f;
    }
    else if (command == "blimp") {
        object.type = GuideObjectType::Blimp;
        stream >> x >> y >> width >> height >> object.body.mass >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.collider.restitution = 0.08f;
        object.collider.friction = 0.12f;
    }
    else if (command == "leakyBucket") {
        object.type = GuideObjectType::LeakyBucket;
        stream >> x >> y >> width >> height >> object.body.mass >> object.capacity >> object.speed;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.capacity = fmaxf(0.0f, object.capacity);
        object.body.mass += object.capacity;
        object.collider.friction = 0.62f;
        object.collider.restitution = 0.04f;
    }
    else if (command == "destructibleWood" || command == "destructibleBrick" || command == "destructibleGlass") {
        object.type = command == "destructibleWood" ? GuideObjectType::DestructibleWood :
            (command == "destructibleBrick" ? GuideObjectType::DestructibleBrick : GuideObjectType::DestructibleGlass);
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.strength = fmaxf(1.0f, object.strength);
    }
    else if (command == "glassPane" || command == "glassBlock") {
        object.type = command == "glassPane" ? GuideObjectType::GlassPane : GuideObjectType::GlassBlock;
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.strength = fmaxf(1.0f, object.strength);
    }
    else if (command == "iceBlock" || command == "iceSheet") {
        object.type = command == "iceBlock" ? GuideObjectType::IceBlock : GuideObjectType::IceSheet;
        stream >> x >> y >> width >> height >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.strength = fmaxf(1.0f, object.strength);
        object.collider.friction = object.type == GuideObjectType::IceSheet ? 0.015f : 0.08f;
    }
    else if (command == "heater") {
        object.type = GuideObjectType::Heater;
        stream >> x >> y >> width >> height >> object.direction.x >> object.direction.y >>
            object.length >> object.width >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.direction = NormalizeOr(object.direction);
        ReadOptionalChannel(stream, object);
        object.power.requiredPower = object.power.channel == 0 ? 0.0f : 0.25f;
    }
    else if (command == "campfire") {
        object.type = GuideObjectType::Campfire;
        stream >> x >> y >> object.collider.radius >> object.length >> object.strength;
        ConfigureCircle(object, x, y, object.collider.radius, BodyType::Static);
        object.collider.isTrigger = true;
    }
    else if (command == "torch") {
        object.type = GuideObjectType::Torch;
        stream >> x >> y >> width >> height >> object.body.mass >> object.length >> object.strength;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.collider.friction = 0.42f;
        object.collider.restitution = 0.08f;
    }
    else if (command == "waterBucket") {
        object.type = GuideObjectType::WaterBucket;
        stream >> x >> y >> width >> height >> object.body.mass >> object.capacity >> object.speed >> object.transform.rotation;
        ConfigureRectangle(object, x, y, width, height, BodyType::Dynamic);
        object.body.mass += object.capacity * 0.08f;
        object.body.inverseMass = 1.0f / fmaxf(0.1f, object.body.mass);
        object.collider.friction = 0.58f;
        object.collider.restitution = 0.04f;
    }
    else if (command == "signalDust") {
        object.type = GuideObjectType::SignalDust;
        Vector2 end{};
        stream >> x >> y >> end.x >> end.y >> object.power.channel;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.constraint.anchorA = object.transform.position;
        object.constraint.anchorB = end;
        const Vector2 delta{end.x - x, end.y - y};
        object.length = sqrtf(delta.x * delta.x + delta.y * delta.y);
        object.direction = NormalizeOr(delta);
        object.width = 7.0f;
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        object.body.type = BodyType::Static;
        object.power.maximumPower = 1.0f;
    }
    else if (command == "ternaryTrace") {
        object.type = GuideObjectType::TernaryTrace;
        Vector2 end{};
        stream >> x >> y >> end.x >> end.y >> object.sensor.channel >> object.power.channel;
        object.transform.position = {x, y};
        object.origin = object.transform.position;
        object.constraint.anchorA = object.transform.position;
        object.constraint.anchorB = end;
        const Vector2 delta{end.x - x, end.y - y};
        object.length = sqrtf(delta.x * delta.x + delta.y * delta.y);
        object.direction = NormalizeOr(delta);
        object.width = 8.0f;
        object.collider.shape = ColliderShape::Segment;
        object.collider.isTrigger = true;
        object.body.type = BodyType::Static;
        object.power.maximumPower = 1.0f;
    }
    else if (command == "ternarySwitch") {
        object.type = GuideObjectType::TernarySwitch;
        stream >> x >> y >> width >> height >> object.signalMask;
        ConfigureRectangle(object, x, y, width, height, BodyType::Static);
        object.collider.isTrigger = true;
        object.signalMask = std::clamp(object.signalMask, -1, 1);
        object.colorMask = 15;
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
    else if (command == "collectible" || command == "key" || command == "gasMask" || command == "scubaGear") {
        object.type = command == "collectible" ? GuideObjectType::Collectible :
            (command == "key" ? GuideObjectType::Key :
             command == "scubaGear" ? GuideObjectType::ScubaGear : GuideObjectType::GasMask);
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

bool IsGuideAttachmentConstraint(GuideObjectType type) {
    return IsSpringConstraintType(type) || type == GuideObjectType::Rod || type == GuideObjectType::FixedJoint;
}

int FindGuideAttachmentTarget(const std::vector<GuideObject>& objects, int constraintIndex) {
    if (constraintIndex < 0 || constraintIndex >= static_cast<int>(objects.size())) return -1;
    const GuideObject& constraint = objects[constraintIndex];
    if (!IsGuideAttachmentConstraint(constraint.type)) return -1;
    const Vector2 attachment = constraint.type == GuideObjectType::FixedJoint
        ? constraint.transform.position : constraint.constraint.anchorB;
    float closestDistanceSq = 42.0f * 42.0f;
    int target = -1;
    for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
        const GuideObject& body = objects[index];
        if (body.layer != constraint.layer || !IsDynamicObject(body.type) || body.broken) continue;
        const Vector2 center = Center(GetGuideObjectBounds(body));
        const float dx = center.x - attachment.x, dy = center.y - attachment.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq <= closestDistanceSq) { closestDistanceSq = distanceSq; target = index; }
    }
    return target;
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

Rectangle GetAntiGravityFieldBounds(const GuideObject& object) {
    if (object.type != GuideObjectType::AntiGravityPad) return {};
    return {
        object.transform.position.x,
        object.transform.position.y - object.length,
        object.collider.size.x,
        object.length
    };
}

bool IsAntiGravityPadActive(const GuideObject& object) {
    return object.type == GuideObjectType::AntiGravityPad && object.active && !object.broken &&
        (object.power.channel == 0 || object.power.powered);
}

bool IsTeleforceFiring(const GuideObject& object) {
    if (object.type != GuideObjectType::Teleforce || !object.active || object.broken ||
        (object.power.channel != 0 && !object.power.powered)) return false;
    const float cycleDuration = object.capacity + object.damage + object.speed;
    const float cycleTime = fmodf(fmaxf(0.0f, object.timer), fmaxf(0.001f, cycleDuration));
    return cycleTime >= object.capacity && cycleTime < object.capacity + object.damage;
}

bool IsTeslaCoilDischarging(const GuideObject& object) {
    if (object.type != GuideObjectType::TeslaCoil || !object.active || object.broken ||
        (object.power.channel != 0 && !object.power.powered)) return false;
    const float cycleDuration = object.capacity + object.damage + object.speed;
    const float cycleTime = fmodf(fmaxf(0.0f, object.timer), fmaxf(0.001f, cycleDuration));
    return cycleTime >= object.capacity && cycleTime < object.capacity + object.damage;
}

bool IsGuideObjectClimbable(const GuideObject& object) {
    return object.type == GuideObjectType::ThornyVines && object.active &&
        !object.broken && !object.collected;
}

float GetGuideObjectContactDamage(const GuideObject& object) {
    return IsGuideObjectClimbable(object) ? fmaxf(0.0f, object.damage) : 0.0f;
}

Vector2 GetAntiGravityPadAcceleration(
    const GuideObject& object,
    Rectangle targetBounds,
    Vector2 targetVelocity
) {
    if (!IsAntiGravityPadActive(object)) return {};
    const Rectangle field = GetAntiGravityFieldBounds(object);
    if (!CheckCollisionRecs(field, targetBounds)) return {};

    const float targetCenterY = targetBounds.y + targetBounds.height * 0.5f;
    const float heightProgress = Clamp01((field.y + field.height - targetCenterY) / field.height);
    const float lift = object.strength * powf(1.0f - heightProgress, 0.75f);
    const float verticalDamping = -targetVelocity.y * 2.2f;
    return {0.0f, -lift + verticalDamping};
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
    Vector2& checkpoint,
    std::vector<FluidField>* fluids,
    FluidSimulationMode fluidMode,
    const Level* tileLevel
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
        if (object.type == GuideObjectType::Tnt && object.triggered && !object.broken && object.timer >= object.interval) {
            object.broken = true;
            object.timer = 0.0f;
        }
        object.sensor.active = false;
        if (object.type != GuideObjectType::TernarySwitch) object.signalMask = 0;
        if (object.type == GuideObjectType::MagnifyingGlass || object.type == GuideObjectType::LaserMixer ||
            object.type == GuideObjectType::LaserDetector) object.triggered = false;

        Rectangle bounds = GetGuideObjectBounds(object);
        if (object.type == GuideObjectType::Sundial) {
            object.sensor.active = fmodf(object.timer, object.interval) < object.interval * 0.5f;
            object.triggered = object.sensor.active;
            if (object.sensor.active) channelPower[object.sensor.channel] = 1.0f;
        }
        else if (object.type == GuideObjectType::LimitSwitch || object.type == GuideObjectType::SpeedSensor ||
            object.type == GuideObjectType::BeamSensor || object.type == GuideObjectType::PolarisTelescope) {
            Rectangle sensedArea = (object.type == GuideObjectType::BeamSensor ||
                object.type == GuideObjectType::PolarisTelescope) ? BeamBounds(object) : bounds;
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

    // Resolve colored light before consumers read channel power. Mixers combine
    // RGB bitmasks and emit the resulting color through their output aperture.
    for (GuideObject& source : objects) {
        if ((source.type != GuideObjectType::LaserEmitter && source.type != GuideObjectType::LightSource) ||
            !source.active || source.broken ||
            (source.power.channel != 0 && !source.power.powered)) continue;
        const Rectangle beam = BeamBounds(source);
        for (GuideObject& target : objects) {
            if (&target == &source || target.layer != source.layer || target.broken) continue;
            if (target.type == GuideObjectType::MagnifyingGlass && CheckCollisionRecs(beam, GetGuideObjectBounds(target))) {
                target.triggered = true;
            }
            if (source.type == GuideObjectType::LaserEmitter &&
                (target.type == GuideObjectType::LaserMixer || target.type == GuideObjectType::LaserDetector) &&
                CheckCollisionRecs(beam, GetGuideObjectBounds(target))) {
                target.signalMask |= source.colorMask;
            }
        }
    }
    for (GuideObject& mixer : objects) {
        if (mixer.type != GuideObjectType::LaserMixer || mixer.signalMask == 0 || mixer.broken) continue;
        mixer.colorMask = mixer.signalMask;
        mixer.triggered = true;
        GuideObject output = mixer;
        output.transform.position = Center(GetGuideObjectBounds(mixer));
        output.width = 3.0f;
        const Rectangle beam = BeamBounds(output);
        for (GuideObject& detector : objects) {
            if (detector.type == GuideObjectType::LaserDetector && detector.layer == mixer.layer &&
                !detector.broken && CheckCollisionRecs(beam, GetGuideObjectBounds(detector))) {
                detector.signalMask |= mixer.colorMask;
            }
        }
    }
    for (GuideObject& lens : objects) {
        if (lens.type != GuideObjectType::MagnifyingGlass || !lens.triggered || lens.broken) continue;
        GuideObject focused = lens;
        focused.transform.position = lens.transform.position;
        focused.width = 2.0f;
        focused.collider.shape = ColliderShape::Segment;
        const Rectangle focusBeam = BeamBounds(focused);
        for (GuideObject& fuse : objects) {
            if (fuse.type == GuideObjectType::IgnitableFuse && fuse.layer == lens.layer && !fuse.broken &&
                CheckCollisionRecs(focusBeam, GetGuideObjectBounds(fuse))) {
                fuse.triggered = true;
            }
        }
    }
    for (GuideObject& object : objects) {
        if (object.type == GuideObjectType::LaserDetector) {
            object.sensor.active = object.colorMask != 0 && object.signalMask == object.colorMask;
            object.triggered = object.sensor.active;
            if (object.sensor.active) channelPower[object.sensor.channel] = 1.0f;
        }
        else if (object.type == GuideObjectType::IgnitableFuse && object.triggered) {
            channelPower[object.sensor.channel] = 1.0f;
        }
    }
    for (const GuideObject& fuse : objects) {
        if (fuse.type != GuideObjectType::IgnitableFuse || !fuse.triggered || fuse.broken) continue;
        Rectangle ignitionBounds = GetGuideObjectBounds(fuse);
        ignitionBounds.x -= 8.0f;
        ignitionBounds.y -= 8.0f;
        ignitionBounds.width += 16.0f;
        ignitionBounds.height += 16.0f;
        for (GuideObject& explosive : objects) {
            if (explosive.type != GuideObjectType::Tnt || explosive.triggered || explosive.broken ||
                explosive.layer != fuse.layer || !CheckCollisionRecs(ignitionBounds, GetGuideObjectBounds(explosive))) continue;
            explosive.triggered = true;
            explosive.timer = 0.0f;
        }
    }

    // Signal dust forms a physical, branching circuit. A touching source seeds
    // strength 15, and every connected trace consumes one step of range.
    auto expandedBounds = [](Rectangle bounds, float amount) {
        return Rectangle{bounds.x - amount, bounds.y - amount,
            bounds.width + amount * 2.0f, bounds.height + amount * 2.0f};
    };
    for (GuideObject& dust : objects) {
        if (dust.type != GuideObjectType::SignalDust) continue;
        dust.signalMask = 0;
        dust.power.currentPower = 0.0f;
        dust.power.powered = false;
        const Rectangle reach = expandedBounds(GetGuideObjectBounds(dust), 10.0f);
        for (const GuideObject& source : objects) {
            if (&source == &dust || source.type == GuideObjectType::SignalDust ||
                source.layer != dust.layer || source.broken || !source.active) continue;
            const bool electricalSource =
                (source.type == GuideObjectType::Battery || source.type == GuideObjectType::Generator) &&
                source.power.powered;
            const bool logicSource = source.sensor.active ||
                (source.type == GuideObjectType::IgnitableFuse && source.triggered);
            if ((electricalSource || logicSource) && CheckCollisionRecs(reach, GetGuideObjectBounds(source))) {
                const float sourcePower = electricalSource
                    ? std::clamp(source.power.currentPower, 0.0f, 1.0f) : 1.0f;
                dust.signalMask = std::max(1, static_cast<int>(ceilf(sourcePower * 15.0f)));
                break;
            }
        }
    }
    for (int pass = 0; pass < 15; ++pass) {
        bool changed = false;
        for (GuideObject& target : objects) {
            if (target.type != GuideObjectType::SignalDust || target.signalMask >= 15) continue;
            const Rectangle targetReach = expandedBounds(GetGuideObjectBounds(target), 7.0f);
            int strongestNeighbor = target.signalMask;
            for (const GuideObject& source : objects) {
                if (&source == &target || source.type != GuideObjectType::SignalDust ||
                    source.layer != target.layer || source.signalMask <= 1) continue;
                if (CheckCollisionRecs(targetReach, GetGuideObjectBounds(source))) {
                    strongestNeighbor = std::max(strongestNeighbor, source.signalMask - 1);
                }
            }
            if (strongestNeighbor > target.signalMask) {
                target.signalMask = strongestNeighbor;
                changed = true;
            }
        }
        if (!changed) break;
    }
    for (GuideObject& dust : objects) {
        if (dust.type != GuideObjectType::SignalDust || dust.signalMask <= 0) continue;
        dust.power.currentPower = static_cast<float>(dust.signalMask) / 15.0f;
        dust.power.powered = true;
        channelPower[dust.power.channel] = fmaxf(channelPower[dust.power.channel], dust.power.currentPower);
    }

    // Ternary traces preserve a signed state while attenuating its strength.
    // Negative and positive states feed separate binary output channels so the
    // existing machine library can branch on either result.
    for (GuideObject& trace : objects) {
        if (trace.type != GuideObjectType::TernaryTrace) continue;
        trace.signalMask = 0;
        trace.colorMask = 0;
        trace.power.currentPower = 0.0f;
        trace.power.powered = false;
        const Rectangle reach = expandedBounds(GetGuideObjectBounds(trace), 10.0f);
        for (const GuideObject& source : objects) {
            if (source.type != GuideObjectType::TernarySwitch || source.layer != trace.layer ||
                source.broken || !source.active || source.signalMask == 0) continue;
            if (!CheckCollisionRecs(reach, GetGuideObjectBounds(source))) continue;
            if (trace.colorMask == 0 || trace.signalMask == source.signalMask) {
                trace.signalMask = source.signalMask;
                trace.colorMask = 15;
            }
            else {
                trace.signalMask = 0;
                trace.colorMask = 15;
            }
        }
    }
    for (int pass = 0; pass < 15; ++pass) {
        bool changed = false;
        for (GuideObject& target : objects) {
            if (target.type != GuideObjectType::TernaryTrace || target.colorMask >= 15) continue;
            const Rectangle targetReach = expandedBounds(GetGuideObjectBounds(target), 7.0f);
            int bestState = target.signalMask;
            int bestStrength = target.colorMask;
            for (const GuideObject& source : objects) {
                if (&source == &target || source.type != GuideObjectType::TernaryTrace ||
                    source.layer != target.layer || source.colorMask <= 1) continue;
                if (!CheckCollisionRecs(targetReach, GetGuideObjectBounds(source))) continue;
                const int candidateStrength = source.colorMask - 1;
                if (candidateStrength > bestStrength) {
                    bestState = source.signalMask;
                    bestStrength = candidateStrength;
                }
                else if (candidateStrength == bestStrength && candidateStrength > 0 &&
                    source.signalMask != bestState) {
                    bestState = 0;
                }
            }
            if (bestStrength > target.colorMask || bestState != target.signalMask) {
                target.signalMask = bestState;
                target.colorMask = bestStrength;
                changed = true;
            }
        }
        if (!changed) break;
    }
    for (GuideObject& trace : objects) {
        if (trace.type != GuideObjectType::TernaryTrace || trace.colorMask <= 0 || trace.signalMask == 0) continue;
        trace.power.currentPower = static_cast<float>(trace.colorMask) / 15.0f;
        trace.power.powered = true;
        const int outputChannel = trace.signalMask < 0 ? trace.sensor.channel : trace.power.channel;
        channelPower[outputChannel] = fmaxf(channelPower[outputChannel], trace.power.currentPower);
    }

    // Tesla coils consume one channel and broadcast a separate wireless channel
    // only during their brief discharge window.
    for (GuideObject& coil : objects) {
        if (coil.type != GuideObjectType::TeslaCoil || !coil.active || coil.broken) continue;
        const float suppliedPower = coil.power.channel == 0 ? 1.0f : channelPower[coil.power.channel];
        coil.power.currentPower = std::clamp(suppliedPower, 0.0f, coil.power.maximumPower);
        coil.power.powered = coil.power.currentPower + 0.0001f >= coil.power.requiredPower;
        if (!coil.power.powered) coil.timer = 0.0f;
        else {
            const float cycleDuration = coil.capacity + coil.damage + coil.speed;
            if (coil.timer >= cycleDuration) coil.timer = fmodf(coil.timer, cycleDuration);
        }
        coil.triggered = IsTeslaCoilDischarging(coil);
        if (coil.triggered && coil.sensor.channel != 0) {
            channelPower[coil.sensor.channel] = fmaxf(channelPower[coil.sensor.channel], 0.85f);
        }
    }

    for (GuideObject& object : objects) {
        if (object.type == GuideObjectType::SignalDust || object.type == GuideObjectType::TernaryTrace ||
            object.type == GuideObjectType::TernarySwitch) continue;
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
        if (object.type == GuideObjectType::PerpetualMotionHolder && object.power.powered) {
            object.triggered = true;
        }
        if (object.type == GuideObjectType::Teleforce) {
            const bool hasPower = object.power.channel == 0 || object.power.powered;
            if (!hasPower) object.timer = 0.0f;
            else {
                const float cycleDuration = object.capacity + object.damage + object.speed;
                if (object.timer >= cycleDuration) object.timer = fmodf(object.timer, cycleDuration);
            }
            object.triggered = IsTeleforceFiring(object);
        }

        bool poweredMotion = object.power.channel == 0 || object.power.powered;
        if ((object.type == GuideObjectType::MovingPlatform || object.type == GuideObjectType::Elevator ||
             object.type == GuideObjectType::CrushingBlock || object.type == GuideObjectType::Piston ||
             object.type == GuideObjectType::HydraulicCylinder || object.type == GuideObjectType::SawBlade ||
             object.type == GuideObjectType::SpokePress || object.type == GuideObjectType::RotoTransConverter) && poweredMotion) {
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
             object.type == GuideObjectType::SpinnerTrap || object.type == GuideObjectType::ManualBenchGrinder) &&
            (poweredMotion || object.type == GuideObjectType::Generator)) {
            object.transform.rotation += rotationSpeed * dt;
        }
        if (object.type == GuideObjectType::RevolvingStoneGate) {
            object.body.angularVelocity *= powf(Clamp01(1.0f - object.strength), dt);
            object.transform.rotation += object.body.angularVelocity * dt;
        }

        if (IsDynamicObject(object.type) && !object.broken) {
            if (object.type == GuideObjectType::LeakyBucket && object.capacity > 0.0f) {
                const float leaked = fminf(object.capacity, fmaxf(0.0f, object.speed) * dt);
                object.capacity -= leaked;
                object.body.mass = fmaxf(0.1f, object.body.mass - leaked);
                object.body.inverseMass = 1.0f / object.body.mass;
            }
            if (object.type == GuideObjectType::HotAirBalloon || object.type == GuideObjectType::Blimp) {
                object.body.force.y -= object.strength * object.body.mass;
            }
            object.body.velocity.x += object.body.force.x * object.body.inverseMass * dt;
            const float gravityScale = object.type == GuideObjectType::ProgrammableBall ? object.strength : 1.0f;
            object.body.velocity.y += (object.body.force.y * object.body.inverseMass +
                (object.body.affectedByGravity ? gravity * gravityScale : 0.0f)) * dt;
            object.body.angularVelocity += object.body.torque * object.body.inverseMass * dt;
            object.body.force = {};
            object.body.torque = 0.0f;
            object.body.velocity.y = fminf(object.body.velocity.y, 900.0f);
            object.body.angularVelocity *= powf(Clamp01(1.0f - object.body.angularDamping * 0.08f), dt * 60.0f);
            if (object.body.onGround) object.body.velocity.x *= powf(Clamp01(1.0f - object.collider.friction), dt);
            Vector2 beforeVelocity = object.body.velocity;
            ResolveDynamicObject(object, worldSolidsByLayer[WorldLayerIndex(object.layer)], dt, tileLevel);
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
            if (object.type == GuideObjectType::Tnt && impact > 360.0f && !object.triggered) {
                object.triggered = true;
                object.timer = 0.0f;
            }
        }
    }

    std::vector<Rectangle> fluidObstacles;
    for (const auto& layerSolids : worldSolidsByLayer) {
        fluidObstacles.insert(fluidObstacles.end(), layerSolids.begin(), layerSolids.end());
    }
    const auto emitWater = [&](Vector2 source, Vector2 velocity, float amount) -> float {
        if (fluids == nullptr || amount <= 0.0f) return 0.0f;
        FluidField* destination = nullptr;
        for (FluidField& fluid : *fluids) {
            if (fluid.type == FluidType::Water && CheckCollisionPointRec(source, fluid.bounds)) {
                destination = &fluid;
                break;
            }
        }
        if (destination == nullptr) {
            FluidField field{};
            field.type = FluidType::Water;
            field.bounds = {source.x - 192.0f, source.y - 48.0f, 384.0f, 320.0f};
            field.particleSpacing = 7.0f;
            field.particleRadius = 3.5f;
            field.interactionRadius = 14.0f;
            field.initialFill = 0.0f;
            field.flowSpeed = 1.0f;
            InitializeFluidField(field, fluidObstacles, fluidMode);
            fluids->push_back(std::move(field));
            destination = &fluids->back();
        }
        if (!destination->cells.empty()) {
            return EmitCellularFluidMass(*destination, source, amount, velocity);
        }
        return static_cast<float>(EmitParticleFluid(*destination, source, velocity,
            std::max(1, static_cast<int>(floorf(amount))), fluidObstacles));
    };

    for (GuideObject& heat : objects) {
        if (heat.broken || !heat.active ||
            (heat.type != GuideObjectType::Heater && heat.type != GuideObjectType::Campfire &&
             heat.type != GuideObjectType::Torch)) continue;
        if (heat.type == GuideObjectType::Heater && heat.power.channel != 0 && !heat.power.powered) continue;
        const Rectangle heatBounds = GetGuideObjectBounds(heat);
        const Vector2 heatCenter = Center(heatBounds);
        for (GuideObject& ice : objects) {
            if (ice.broken || (ice.type != GuideObjectType::IceBlock && ice.type != GuideObjectType::IceSheet)) continue;
            const Rectangle iceBounds = GetGuideObjectBounds(ice);
            const Vector2 iceCenter = Center(iceBounds);
            float exposure = 0.0f;
            if (heat.type == GuideObjectType::Heater) {
                if (CheckCollisionRecs(BeamBounds(heat), iceBounds)) exposure = 1.0f;
            }
            else {
                const float dx = iceCenter.x - heatCenter.x;
                const float dy = iceCenter.y - heatCenter.y;
                const float distance = sqrtf(dx * dx + dy * dy);
                if (distance <= heat.length) exposure = 1.0f - distance / fmaxf(1.0f, heat.length);
            }
            if (exposure <= 0.0f) continue;
            ice.damage += heat.strength * exposure * dt;
            if (ice.damage >= ice.strength) {
                ice.broken = true;
                const float waterAmount = std::clamp(iceBounds.width * iceBounds.height / 180.0f, 3.0f, 28.0f);
                emitWater(iceCenter, {0.0f, 35.0f}, waterAmount);
            }
        }
        for (GuideObject& explosive : objects) {
            if (explosive.type != GuideObjectType::Tnt || explosive.broken || explosive.triggered ||
                explosive.layer != heat.layer) continue;
            const Rectangle explosiveBounds = GetGuideObjectBounds(explosive);
            bool exposed = heat.type == GuideObjectType::Heater
                ? CheckCollisionRecs(BeamBounds(heat), explosiveBounds)
                : CheckCollisionCircleRec(heatCenter, heat.length, explosiveBounds);
            if (exposed) {
                explosive.triggered = true;
                explosive.timer = 0.0f;
            }
        }
    }

    for (GuideObject& bucket : objects) {
        if (bucket.type != GuideObjectType::WaterBucket || bucket.broken || bucket.capacity <= 0.0f) continue;
        const float angle = bucket.transform.rotation * DEG2RAD;
        if (cosf(angle) > -0.35f) continue;
        const Vector2 spout{
            bucket.transform.position.x + sinf(angle) * bucket.collider.size.y * 0.5f,
            bucket.transform.position.y - cosf(angle) * bucket.collider.size.y * 0.5f
        };
        const Vector2 pourVelocity{bucket.body.velocity.x + sinf(angle) * 45.0f,
            bucket.body.velocity.y - cosf(angle) * 90.0f};
        float requested = fminf(bucket.capacity, fmaxf(0.0f, bucket.speed) * dt);
        if (fluidMode == FluidSimulationMode::Advanced) {
            bucket.phase += requested;
            requested = floorf(bucket.phase);
            bucket.phase -= requested;
        }
        if (requested <= 0.0f) continue;
        const float emitted = emitWater(spout, pourVelocity, requested);
        bucket.capacity = fmaxf(0.0f, bucket.capacity - emitted);
        bucket.body.mass = fmaxf(0.1f, bucket.body.mass - emitted * 0.08f);
        bucket.body.inverseMass = 1.0f / bucket.body.mass;
    }

    for (int sourceIndex = 0; sourceIndex < static_cast<int>(objects.size()); ++sourceIndex) {
        GuideObject& lifter = objects[sourceIndex];
        if (!IsLeedskalninLifter(lifter.type) || !lifter.active || lifter.broken ||
            (lifter.power.channel != 0 && !lifter.power.powered)) continue;

        if (lifter.attachedObject < 0) {
            float closestDistanceSq = 72.0f * 72.0f;
            for (int bodyIndex = 0; bodyIndex < static_cast<int>(objects.size()); ++bodyIndex) {
                const GuideObject& body = objects[bodyIndex];
                if (bodyIndex == sourceIndex || body.layer != lifter.layer ||
                    !IsDynamicObject(body.type) || body.broken) continue;
                const Vector2 bodyCenter = Center(GetGuideObjectBounds(body));
                const float dx = bodyCenter.x - lifter.constraint.anchorB.x;
                const float dy = bodyCenter.y - lifter.constraint.anchorB.y;
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq <= closestDistanceSq) {
                    closestDistanceSq = distanceSq;
                    lifter.attachedObject = bodyIndex;
                }
            }
        }
        if (lifter.attachedObject < 0 || lifter.attachedObject >= static_cast<int>(objects.size())) continue;

        GuideObject& body = objects[lifter.attachedObject];
        if (body.broken || !IsDynamicObject(body.type)) {
            lifter.attachedObject = -1;
            continue;
        }
        const Vector2 bodyCenter = Center(GetGuideObjectBounds(body));
        Vector2 target = lifter.constraint.anchorA;
        if (lifter.type != GuideObjectType::ComeAlong) {
            const float travel = lifter.length * (0.52f + 0.38f * sinf(lifter.timer * lifter.speed));
            target = Add(lifter.constraint.anchorA, {0.0f, travel});
        }
        const Vector2 delta{target.x - bodyCenter.x, target.y - bodyCenter.y};
        const float response = lifter.type == GuideObjectType::BlockAndTackle ? 0.55f : 1.0f;
        body.body.velocity.x += delta.x * lifter.strength * response * body.body.inverseMass * dt;
        body.body.velocity.y += delta.y * lifter.strength * response * body.body.inverseMass * dt;
        body.body.velocity.x *= powf(0.45f, dt * 60.0f);
        body.body.velocity.y *= powf(0.45f, dt * 60.0f);
        lifter.constraint.anchorB = bodyCenter;
    }

    for (int i = 0; i < static_cast<int>(objects.size()); i++) {
        GuideObject& constraint = objects[i];
        if (!IsSpringConstraintType(constraint.type) && constraint.type != GuideObjectType::Rod &&
            constraint.type != GuideObjectType::FixedJoint) continue;

        if (constraint.attachedObject < 0) {
            constraint.attachedObject = FindGuideAttachmentTarget(objects, i);
            if (constraint.attachedObject >= 0 && IsRotarySpringType(constraint.type))
                constraint.constraint.minimum = objects[constraint.attachedObject].transform.rotation;
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

    for (GuideObject& explosive : objects) {
        if (!IsExplosive(explosive.type) || !explosive.broken || explosive.timer >= 0.35f) continue;
        const float blastRadius = ExplosionRadius(explosive);
        const Vector2 blastCenter = ExplosionCenter(explosive);
        const bool freshBlast = explosive.timer <= fmaxf(0.025f, dt * 1.5f);
        for (GuideObject& target : objects) {
            if (&target == &explosive || target.layer != explosive.layer) continue;
            const Vector2 targetCenter = Center(GetGuideObjectBounds(target));
            Vector2 delta{targetCenter.x - blastCenter.x, targetCenter.y - blastCenter.y};
            const float distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
            if (distance > blastRadius) continue;
            if (IsDestructibleMaterial(target.type)) target.broken = true;
            if (!freshBlast) continue;
            if (IsExplosive(target.type) && !target.broken) {
                target.broken = true;
                target.triggered = true;
                target.timer = 0.0f;
                continue;
            }
            if (IsDynamicObject(target.type) && !target.broken) {
                const Vector2 direction = NormalizeOr(delta, {0.0f, -1.0f});
                const float falloff = 1.0f - distance / blastRadius;
                const float impulse = (explosive.type == GuideObjectType::Tnt ? 720.0f : 520.0f) * falloff;
                target.body.velocity.x += direction.x * impulse / fmaxf(0.25f, target.body.mass);
                target.body.velocity.y += direction.y * impulse / fmaxf(0.25f, target.body.mass);
            }
        }
    }

    for (GuideObject& source : objects) {
        if (!source.active || source.broken) continue;
        Rectangle sourceBounds = GetGuideObjectBounds(source);
        if (source.type == GuideObjectType::Bellows || source.type == GuideObjectType::BoxingGlove ||
            source.type == GuideObjectType::PinballFlipper) {
            source.phase = fmaxf(0.0f, source.phase - dt * 4.5f);
            if (source.power.channel != 0 && source.power.powered) source.phase = 1.0f;
        }
        GuideObject field = source;
        field.transform.position = Center(sourceBounds);
        Rectangle effectBounds = BeamBounds(field);
        if (source.type == GuideObjectType::Teleforce && IsTeleforceFiring(source)) {
            for (GuideObject& target : objects) {
                if (&target == &source || target.layer != source.layer || target.broken ||
                    !RectangleTouchesTeleforceBeam(source, GetGuideObjectBounds(target))) continue;
                if (IsDestructibleMaterial(target.type)) {
                    target.damage += source.strength * dt * 0.35f;
                    if (target.damage >= target.strength) target.broken = true;
                }
                else if (IsExplosive(target.type) && !target.triggered) {
                    target.triggered = true;
                    target.timer = 0.0f;
                }
            }
        }
        if (source.type == GuideObjectType::TeslaCoil && IsTeslaCoilDischarging(source)) {
            const Vector2 terminal = TeslaCoilTerminal(source);
            for (GuideObject& target : objects) {
                if (&target == &source || target.layer != source.layer || target.broken ||
                    !IsExplosive(target.type)) continue;
                const Vector2 targetCenter = Center(GetGuideObjectBounds(target));
                const float dx = targetCenter.x - terminal.x;
                const float dy = targetCenter.y - terminal.y;
                if (dx * dx + dy * dy > source.length * source.length) continue;
                target.triggered = true;
                target.timer = 0.0f;
            }
        }
        if (IsDestructibleMaterial(source.type)) {
            for (const GuideObject& explosive : objects) {
                if (!IsExplosive(explosive.type) || !explosive.broken || explosive.timer >= 0.35f) continue;
                const Vector2 blastCenter = ExplosionCenter(explosive);
                const Vector2 blastDelta{blastCenter.x - Center(sourceBounds).x,
                    blastCenter.y - Center(sourceBounds).y};
                const float blastRadius = ExplosionRadius(explosive);
                if (blastDelta.x * blastDelta.x + blastDelta.y * blastDelta.y <= blastRadius * blastRadius) {
                    source.broken = true;
                    break;
                }
            }
            if (source.broken) continue;
        }

        for (GuideObject& body : objects) {
            if (&body == &source || body.layer != source.layer ||
                !IsDynamicObject(body.type) || body.broken) continue;
            Rectangle bodyBounds = GetGuideObjectBounds(body);
            Vector2 bodyCenter = Center(bodyBounds);
            if ((source.type == GuideObjectType::Bellows || source.type == GuideObjectType::BoxingGlove ||
                 source.type == GuideObjectType::PinballFlipper) && CheckCollisionRecs(sourceBounds, bodyBounds)) {
                source.phase = 1.0f;
            }
            if ((source.type == GuideObjectType::Magnet || source.type == GuideObjectType::FieldCoil) &&
                (source.power.channel == 0 || source.power.powered)) {
                Vector2 delta{source.transform.position.x - bodyCenter.x, source.transform.position.y - bodyCenter.y};
                float distanceSq = fmaxf(100.0f, delta.x * delta.x + delta.y * delta.y);
                if (distanceSq <= source.collider.radius * source.collider.radius) {
                    Vector2 direction = NormalizeOr(delta);
                    body.body.velocity.x += direction.x * source.strength * dt / sqrtf(distanceSq) * 18.0f;
                    body.body.velocity.y += direction.y * source.strength * dt / sqrtf(distanceSq) * 18.0f;
                }
            }
            else if (source.type == GuideObjectType::PerpetualMotionHolder && source.triggered) {
                const Vector2 holderCenter = Center(sourceBounds);
                Vector2 delta{holderCenter.x - bodyCenter.x, holderCenter.y - bodyCenter.y};
                const float distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
                if (distance <= source.length) {
                    const Vector2 direction = NormalizeOr(delta);
                    body.body.velocity.x += direction.x * source.strength * dt / body.body.mass;
                    body.body.velocity.y += direction.y * source.strength * dt / body.body.mass;
                    if (distance < fmaxf(sourceBounds.width, sourceBounds.height) * 0.65f) {
                        source.attachedObject = static_cast<int>(&body - objects.data());
                        body.body.velocity.x *= powf(0.08f, dt * 60.0f);
                        body.body.velocity.y *= powf(0.08f, dt * 60.0f);
                    }
                }
                else if (source.attachedObject == static_cast<int>(&body - objects.data())) {
                    source.attachedObject = -1;
                    source.triggered = false;
                }
            }
            else if ((source.type == GuideObjectType::RocketThruster || source.type == GuideObjectType::SteamVent) &&
                     IsForceEmitterActive(source) && CheckCollisionRecs(BeamBounds(source), bodyBounds)) {
                body.body.velocity.x += source.direction.x * source.strength * dt / body.body.mass;
                body.body.velocity.y += source.direction.y * source.strength * dt / body.body.mass;
            }
            else if (source.type == GuideObjectType::AntiGravityPad) {
                const Vector2 acceleration = GetAntiGravityPadAcceleration(
                    source, bodyBounds, body.body.velocity);
                body.body.velocity.x += acceleration.x * dt;
                body.body.velocity.y += acceleration.y * dt;
            }
            else if (source.type == GuideObjectType::Brake && CheckCollisionRecs(sourceBounds, bodyBounds)) {
                body.body.velocity.x *= powf(Clamp01(1.0f - source.strength), dt);
                body.body.velocity.y *= powf(Clamp01(1.0f - source.strength), dt);
            }
            else if (source.type == GuideObjectType::Vacuum &&
                (source.power.channel == 0 || source.power.powered) && CheckCollisionRecs(effectBounds, bodyBounds)) {
                const Vector2 nozzle = Center(sourceBounds);
                const Vector2 pull = NormalizeOr({nozzle.x - bodyCenter.x, nozzle.y - bodyCenter.y});
                body.body.velocity.x += pull.x * source.strength * dt / body.body.mass;
                body.body.velocity.y += pull.y * source.strength * dt / body.body.mass;
            }
            else if (source.type == GuideObjectType::AcceleratorTube &&
                (source.power.channel == 0 || source.power.powered) && CheckCollisionRecs(sourceBounds, bodyBounds)) {
                body.body.velocity.x += source.direction.x * source.strength * dt;
                body.body.velocity.y += source.direction.y * source.strength * dt;
            }
            else if (source.type == GuideObjectType::Teleforce && IsTeleforceFiring(source) &&
                RectangleTouchesTeleforceBeam(source, bodyBounds)) {
                const float inverseMass = 1.0f / fmaxf(0.25f, body.body.mass);
                body.body.velocity.x += source.direction.x * source.strength * inverseMass * dt;
                body.body.velocity.y += source.direction.y * source.strength * inverseMass * dt;
            }
            else if (source.type == GuideObjectType::TeslaCoil && IsTeslaCoilDischarging(source)) {
                const Vector2 terminal = TeslaCoilTerminal(source);
                const Vector2 delta{bodyCenter.x - terminal.x, bodyCenter.y - terminal.y};
                const float distanceSq = delta.x * delta.x + delta.y * delta.y;
                if (distanceSq <= source.length * source.length) {
                    const float distance = sqrtf(fmaxf(1.0f, distanceSq));
                    const float falloff = 1.0f - distance / source.length;
                    const Vector2 push = NormalizeOr(delta, {0.0f, -1.0f});
                    const float acceleration = source.strength * (0.35f + falloff * 0.65f) /
                        fmaxf(0.25f, body.body.mass);
                    body.body.velocity.x += push.x * acceleration * dt;
                    body.body.velocity.y += push.y * acceleration * dt;
                }
            }
            else if ((source.type == GuideObjectType::Bellows || source.type == GuideObjectType::BoxingGlove) &&
                source.phase > 0.05f && CheckCollisionRecs(effectBounds, bodyBounds)) {
                const float force = source.type == GuideObjectType::BoxingGlove ? source.strength * 2.2f : source.strength;
                body.body.velocity.x += source.direction.x * force * source.phase * dt / body.body.mass;
                body.body.velocity.y += source.direction.y * force * source.phase * dt / body.body.mass;
            }
            else if (source.type == GuideObjectType::PinballBumper &&
                CheckCollisionCircleRec(source.transform.position, source.collider.radius, bodyBounds)) {
                const Vector2 normal = NormalizeOr({bodyCenter.x - source.transform.position.x,
                    bodyCenter.y - source.transform.position.y}, {0.0f, -1.0f});
                body.body.velocity.x = normal.x * source.strength;
                body.body.velocity.y = normal.y * source.strength;
            }
            else if (source.type == GuideObjectType::PinballFlipper && source.phase > 0.05f &&
                CheckCollisionRecs(sourceBounds, bodyBounds)) {
                body.body.velocity.x += source.direction.x * source.strength * 0.72f;
                body.body.velocity.y = fminf(body.body.velocity.y, -source.strength);
            }
            else if (source.type == GuideObjectType::Trampoline) {
                const float feet = bodyBounds.y + bodyBounds.height;
                if (body.body.velocity.y >= 0.0f && bodyBounds.x + bodyBounds.width > sourceBounds.x &&
                    bodyBounds.x < sourceBounds.x + sourceBounds.width &&
                    feet >= sourceBounds.y - 4.0f && feet <= sourceBounds.y + fmaxf(9.0f, body.body.velocity.y * dt + 4.0f)) {
                    body.body.velocity.y = -source.strength;
                    source.phase = 1.0f;
                }
            }
            else if (source.type == GuideObjectType::TransRotoConverter &&
                CheckCollisionCircleRec(source.transform.position, source.collider.radius * 1.25f, bodyBounds)) {
                const float linearSpeed = sqrtf(body.body.velocity.x * body.body.velocity.x +
                    body.body.velocity.y * body.body.velocity.y);
                source.speed = linearSpeed * source.strength / fmaxf(1.0f, source.collider.radius);
                source.transform.rotation += source.speed * RAD2DEG * dt;
                body.body.velocity.x *= 0.985f;
                body.body.velocity.y *= 0.985f;
            }
            if (IsDestructibleMaterial(source.type)) {
                const Rectangle impactZone{sourceBounds.x - 4.0f, sourceBounds.y - 4.0f,
                    sourceBounds.width + 8.0f, sourceBounds.height + 8.0f};
                if (CheckCollisionRecs(impactZone, bodyBounds)) {
                    const float impactSpeed = sqrtf(body.body.velocity.x * body.body.velocity.x +
                        body.body.velocity.y * body.body.velocity.y);
                    if (impactSpeed > 110.0f) source.damage += impactSpeed * body.body.mass * dt;
                    if (source.damage >= source.strength) source.broken = true;
                }
            }
        }

        if (!IsPlayerCollisionLayer(source.layer)) continue;
        for (Player* player : players) {
            if (player == nullptr) continue;
            Rectangle playerRect = player->rect;
            if ((source.type == GuideObjectType::Bellows || source.type == GuideObjectType::BoxingGlove ||
                 source.type == GuideObjectType::PinballFlipper) && CheckCollisionRecs(sourceBounds, playerRect)) {
                source.phase = 1.0f;
            }
            if (source.type == GuideObjectType::TransRotoConverter &&
                CheckCollisionCircleRec(source.transform.position, source.collider.radius * 1.25f, playerRect)) {
                const float linearSpeed = sqrtf(player->velocity.x * player->velocity.x + player->velocity.y * player->velocity.y);
                source.speed = linearSpeed * source.strength / fmaxf(1.0f, source.collider.radius);
                source.transform.rotation += source.speed * RAD2DEG * dt;
                player->velocity.x *= 0.985f;
                player->velocity.y *= 0.985f;
            }
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
            if (source.type == GuideObjectType::RevolvingStoneGate &&
                CheckCollisionRecs(playerRect, sourceBounds)) {
                source.body.angularVelocity += player->velocity.x * 0.055f;
                source.body.angularVelocity = std::clamp(source.body.angularVelocity, -150.0f, 150.0f);
                player->velocity.x *= 0.72f;
            }
            if (source.type == GuideObjectType::Oil && CheckCollisionRecs(playerRect, sourceBounds)) {
                player->velocity.x *= powf(0.985f, dt * 60.0f);
            }
            if (source.type == GuideObjectType::IceSheet && CheckCollisionRecs(playerRect, sourceBounds)) {
                player->velocity.x *= powf(0.998f, dt * 60.0f);
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
            if (source.type == GuideObjectType::AntiGravityPad) {
                const Vector2 acceleration = GetAntiGravityPadAcceleration(
                    source, playerRect, player->velocity);
                player->velocity.x += acceleration.x * dt;
                player->velocity.y += acceleration.y * dt;
                if (acceleration.y < 0.0f) player->onGround = false;
            }
            if (source.type == GuideObjectType::Vacuum &&
                (source.power.channel == 0 || source.power.powered) && CheckCollisionRecs(effectBounds, playerRect)) {
                const Vector2 nozzle = Center(sourceBounds);
                const Vector2 playerCenter = Center(playerRect);
                const Vector2 pull = NormalizeOr({nozzle.x - playerCenter.x, nozzle.y - playerCenter.y});
                player->velocity.x += pull.x * source.strength * dt;
                player->velocity.y += pull.y * source.strength * dt;
            }
            if (source.type == GuideObjectType::AcceleratorTube &&
                (source.power.channel == 0 || source.power.powered) && CheckCollisionRecs(sourceBounds, playerRect)) {
                player->velocity.x += source.direction.x * source.strength * dt;
                player->velocity.y += source.direction.y * source.strength * dt;
            }
            if (source.type == GuideObjectType::Teleforce && IsTeleforceFiring(source) &&
                RectangleTouchesTeleforceBeam(source, playerRect)) {
                player->velocity.x += source.direction.x * source.strength * dt;
                player->velocity.y += source.direction.y * source.strength * dt;
            }
            if (source.type == GuideObjectType::TeslaCoil && IsTeslaCoilDischarging(source) &&
                CheckCollisionCircleRec(TeslaCoilTerminal(source), source.length, playerRect)) {
                const Vector2 push = NormalizeOr({Center(playerRect).x - TeslaCoilTerminal(source).x,
                    Center(playerRect).y - TeslaCoilTerminal(source).y}, {0.0f, -1.0f});
                player->velocity.x += push.x * source.strength * dt;
                player->velocity.y += push.y * source.strength * dt;
            }
            if ((source.type == GuideObjectType::Bellows || source.type == GuideObjectType::BoxingGlove) &&
                source.phase > 0.05f && CheckCollisionRecs(effectBounds, playerRect)) {
                const float force = source.type == GuideObjectType::BoxingGlove ? source.strength * 2.2f : source.strength;
                player->velocity.x += source.direction.x * force * source.phase * dt;
                player->velocity.y += source.direction.y * force * source.phase * dt;
            }
            if (source.type == GuideObjectType::PinballBumper &&
                CheckCollisionCircleRec(source.transform.position, source.collider.radius, playerRect)) {
                const Vector2 playerCenter = Center(playerRect);
                const Vector2 normal = NormalizeOr({playerCenter.x - source.transform.position.x,
                    playerCenter.y - source.transform.position.y}, {0.0f, -1.0f});
                player->velocity.x = normal.x * source.strength;
                player->velocity.y = normal.y * source.strength;
            }
            if (source.type == GuideObjectType::PinballFlipper && source.phase > 0.05f &&
                CheckCollisionRecs(sourceBounds, playerRect)) {
                player->velocity.x += source.direction.x * source.strength * 0.72f;
                player->velocity.y = fminf(player->velocity.y, -source.strength);
            }
            if (source.type == GuideObjectType::Trampoline) {
                const float feet = playerRect.y + playerRect.height;
                if (player->velocity.y >= 0.0f && playerRect.x + playerRect.width > sourceBounds.x &&
                    playerRect.x < sourceBounds.x + sourceBounds.width &&
                    feet >= sourceBounds.y - 4.0f && feet <= sourceBounds.y + fmaxf(11.0f, player->velocity.y * dt + 4.0f)) {
                    player->rect.y = sourceBounds.y - playerRect.height;
                    player->velocity.y = -source.strength;
                    player->onGround = false;
                    source.phase = 1.0f;
                }
            }
            if (IsDestructibleMaterial(source.type) && CheckCollisionRecs(sourceBounds, playerRect)) {
                const float left = playerRect.x + playerRect.width - sourceBounds.x;
                const float right = sourceBounds.x + sourceBounds.width - playerRect.x;
                const float top = playerRect.y + playerRect.height - sourceBounds.y;
                const float bottom = sourceBounds.y + sourceBounds.height - playerRect.y;
                const float minimum = fminf(fminf(left, right), fminf(top, bottom));
                if (minimum == top && player->velocity.y >= 0.0f) {
                    player->rect.y = sourceBounds.y - playerRect.height;
                    player->velocity.y = 0.0f;
                    player->onGround = true;
                }
                else if (minimum == left) { player->rect.x -= left; player->velocity.x = 0.0f; }
                else if (minimum == right) { player->rect.x += right; player->velocity.x = 0.0f; }
                else { player->rect.y += bottom; player->velocity.y = 0.0f; }
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
                 source.type == GuideObjectType::GasMask || source.type == GuideObjectType::ScubaGear) &&
                !source.collected && CheckCollisionRecs(playerRect, sourceBounds)) {
                source.collected = true;
            }
        }
    }

    for (GuideObject& output : objects) {
        if (output.type != GuideObjectType::RotoTransConverter || output.broken) continue;
        float nearestDistanceSq = 240.0f * 240.0f;
        for (const GuideObject& input : objects) {
            if (input.type != GuideObjectType::TransRotoConverter || input.layer != output.layer || input.broken) continue;
            const float dx = input.transform.position.x - output.origin.x;
            const float dy = input.transform.position.y - output.origin.y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                output.speed = input.speed;
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
    if (object.type == GuideObjectType::SpinnerTrap || object.type == GuideObjectType::ManualBenchGrinder) {
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
    if (object.type == GuideObjectType::Teleforce) {
        return IsTeleforceFiring(object) && RectangleTouchesTeleforceBeam(object, playerRect);
    }
    if (object.type == GuideObjectType::TeslaCoil) {
        return IsTeslaCoilDischarging(object) &&
            CheckCollisionCircleRec(TeslaCoilTerminal(object), object.length, playerRect);
    }
    if (IsExplosive(object.type) && object.broken && object.timer < 0.35f) {
        return CheckCollisionCircleRec(ExplosionCenter(object), ExplosionRadius(object), playerRect);
    }
    if (object.type == GuideObjectType::Campfire) {
        return CheckCollisionCircleRec(object.transform.position, object.collider.radius * 1.25f, playerRect);
    }
    if (object.type == GuideObjectType::Torch) {
        return CheckCollisionCircleRec(object.transform.position, fmaxf(10.0f, object.collider.size.y * 0.45f), playerRect);
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
        case GuideObjectType::AntiGravityPad: return "Anti-Gravity Pad";
        case GuideObjectType::TripodDerrick: return "Tripod Derrick";
        case GuideObjectType::ChainHoist: return "Chain Hoist";
        case GuideObjectType::BlockAndTackle: return "Block and Tackle";
        case GuideObjectType::ComeAlong: return "Come-Along";
        case GuideObjectType::ManualBenchGrinder: return "Manual Bench Grinder";
        case GuideObjectType::LoadCart: return "Heavy-Load Cart";
        case GuideObjectType::RevolvingStoneGate: return "Balanced Stone Gate";
        case GuideObjectType::RockHammer: return "Gear-Headed Rock Hammer";
        case GuideObjectType::FieldCoil: return "Field Coil";
        case GuideObjectType::SpokePress: return "Richmond Spoke Press";
        case GuideObjectType::PolarisTelescope: return "Polaris Telescope";
        case GuideObjectType::Sundial: return "Leedskalnin Sundial";
        case GuideObjectType::PerpetualMotionHolder: return "Perpetual Motion Holder";
        case GuideObjectType::Bellows: return "Bellows";
        case GuideObjectType::Trampoline: return "Trampoline";
        case GuideObjectType::BoxingGlove: return "Boxing Glove";
        case GuideObjectType::Vacuum: return "Vacuum";
        case GuideObjectType::MagnifyingGlass: return "Magnifying Glass";
        case GuideObjectType::LightSource: return "Light Source";
        case GuideObjectType::IgnitableFuse: return "Ignitable Fuse";
        case GuideObjectType::ProgrammableBall: return "Programmable Ball";
        case GuideObjectType::TransRotoConverter: return "Trans-Roto Converter";
        case GuideObjectType::RotoTransConverter: return "Roto-Trans Converter";
        case GuideObjectType::AcceleratorTube: return "Accelerator Tube";
        case GuideObjectType::PinballBumper: return "Pinball Bumper";
        case GuideObjectType::PinballFlipper: return "Pinball Flipper";
        case GuideObjectType::LaserEmitter: return "Laser Emitter";
        case GuideObjectType::LaserMixer: return "Laser Mixer";
        case GuideObjectType::LaserDetector: return "Laser Detector";
        case GuideObjectType::HotAirBalloon: return "Hot-Air Balloon";
        case GuideObjectType::Blimp: return "Blimp";
        case GuideObjectType::LeakyBucket: return "Leaky Bucket";
        case GuideObjectType::DestructibleWood: return "Destructible Wood";
        case GuideObjectType::DestructibleBrick: return "Destructible Brick";
        case GuideObjectType::DestructibleGlass: return "Destructible Glass";
        case GuideObjectType::GlassPane: return "Glass Pane";
        case GuideObjectType::GlassBlock: return "Glass Block";
        case GuideObjectType::IceBlock: return "Ice Block";
        case GuideObjectType::IceSheet: return "Ice Sheet";
        case GuideObjectType::Heater: return "Heater";
        case GuideObjectType::Campfire: return "Campfire";
        case GuideObjectType::Torch: return "Torch";
        case GuideObjectType::WaterBucket: return "Bucket of Water";
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
        case GuideObjectType::ScubaGear: return "Scuba Gear";
        case GuideObjectType::GasMask: return "Gas Mask";
        case GuideObjectType::BreakableCrate: return "Breakable Crate";
        case GuideObjectType::ExplosiveBarrel: return "Explosive Barrel";
        case GuideObjectType::Tnt: return "TNT";
        case GuideObjectType::SignalDust: return "Signal Dust";
        case GuideObjectType::TernaryTrace: return "Ternary Trace";
        case GuideObjectType::TernarySwitch: return "Ternary Switch";
        case GuideObjectType::Teleforce: return "Teleforce Projector";
        case GuideObjectType::Rock: return "Rock";
        case GuideObjectType::TeslaCoil: return "Tesla Coil";
        case GuideObjectType::ThornyVines: return "Thorny Vines";
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
        case GuideObjectType::AntiGravityPad: return "Creates a powered upward field that lifts players and movable physics objects into a stable hover.";
        case GuideObjectType::TripodDerrick: return "A wide three-legged frame that supports lifting machinery and spreads a suspended load.";
        case GuideObjectType::ChainHoist: return "Uses a geared hand-chain mechanism to raise a nearby load vertically.";
        case GuideObjectType::BlockAndTackle: return "A compound pulley set that trades extra rope travel for greater lifting force.";
        case GuideObjectType::ComeAlong: return "A ratcheting portable winch that steadily pulls an attached load toward its anchor.";
        case GuideObjectType::ManualBenchGrinder: return "A hand-cranked abrasive wheel that is dangerous while its exposed stone is spinning.";
        case GuideObjectType::LoadCart: return "A low-friction wheeled platform for transporting heavy physics objects.";
        case GuideObjectType::RevolvingStoneGate: return "A massive stone slab balanced on a centered shaft so a small push can rotate it.";
        case GuideObjectType::RockHammer: return "A heavy movable hammer made from a salvaged differential gear and steel rod.";
        case GuideObjectType::FieldCoil: return "A powered salvage coil that creates an electromagnetic pulling field.";
        case GuideObjectType::SpokePress: return "A powered mechanical press adapted from equipment used to form vehicle wheels.";
        case GuideObjectType::PolarisTelescope: return "A fixed sight aimed toward Polaris that also acts as a line-of-sight sensor.";
        case GuideObjectType::Sundial: return "A celestial timer that alternates its output channel as its shadow crosses the dial.";
        case GuideObjectType::PerpetualMotionHolder: return "A pulsed electromagnetic keeper that remains latched without continuous power until its magnetic circuit is pulled open.";
        case GuideObjectType::Bellows: return "Compresses when struck and sends a short burst of air through its outlet.";
        case GuideObjectType::Trampoline: return "Returns downward motion as a strong upward bounce.";
        case GuideObjectType::BoxingGlove: return "Extends from its housing to punch players and movable objects away.";
        case GuideObjectType::Vacuum: return "Uses electrical power to pull players and movable objects toward its nozzle.";
        case GuideObjectType::MagnifyingGlass: return "Focuses a crossing light beam into a short hot ray that can ignite a fuse.";
        case GuideObjectType::LightSource: return "Projects a powered beam that can be focused by a magnifying glass.";
        case GuideObjectType::IgnitableFuse: return "Latches on when focused light reaches it and then powers its output channel.";
        case GuideObjectType::ProgrammableBall: return "A ball with configurable mass, bounce, friction, and gravity response.";
        case GuideObjectType::TransRotoConverter: return "Converts the movement of a contacting body into rotary motion.";
        case GuideObjectType::RotoTransConverter: return "Converts continuous rotation into repeating straight-line travel.";
        case GuideObjectType::AcceleratorTube: return "Continuously accelerates movable objects through the tube in its facing direction.";
        case GuideObjectType::PinballBumper: return "Kicks touching players and physics objects radially away from its center.";
        case GuideObjectType::PinballFlipper: return "Swings upward to launch players and physics objects across a pinball lane.";
        case GuideObjectType::LaserEmitter: return "Projects red, green, or blue light for color-sensitive logic circuits.";
        case GuideObjectType::LaserMixer: return "Combines incoming RGB laser beams and emits their mixed color.";
        case GuideObjectType::LaserDetector: return "Powers its channel only when it receives the configured laser color combination.";
        case GuideObjectType::HotAirBalloon: return "A buoyant round craft whose lift competes with its weight and carried momentum.";
        case GuideObjectType::Blimp: return "A large buoyant body with gentle lift, low friction, and substantial air resistance.";
        case GuideObjectType::LeakyBucket: return "Starts heavy and steadily loses water mass, changing the balance of attached mechanisms.";
        case GuideObjectType::DestructibleWood: return "A solid wooden barrier that splinters after moderate impacts.";
        case GuideObjectType::DestructibleBrick: return "A tough masonry barrier that requires a heavy impact to break.";
        case GuideObjectType::DestructibleGlass: return "A brittle transparent barrier that shatters under a relatively light impact.";
        case GuideObjectType::GlassPane: return "A thin framed sheet of glass that blocks movement but shatters under a light impact.";
        case GuideObjectType::GlassBlock: return "A thick translucent building block that withstands moderate impacts before shattering.";
        case GuideObjectType::IceBlock: return "A solid frozen block that melts into simulated water when exposed to enough heat.";
        case GuideObjectType::IceSheet: return "A thin slippery frozen surface that melts quickly when heated.";
        case GuideObjectType::Heater: return "Projects powered heat in one chosen direction to melt ice within its beam.";
        case GuideObjectType::Campfire: return "Radiates heat in every direction, melting nearby ice and burning players who get too close.";
        case GuideObjectType::Torch: return "A movable flame that radiates a small amount of heat around itself.";
        case GuideObjectType::WaterBucket: return "Carries dormant water and pours it into the fluid simulation only while turned upside down.";
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
        case GuideObjectType::ScubaGear: return "Allows underwater breathing until enemy contact knocks it off.";
        case GuideObjectType::GasMask: return "Protects the wearer from toxic gas until enemy contact knocks it off.";
        case GuideObjectType::BreakableCrate: return "A movable wooden crate that breaks after a sufficiently hard impact.";
        case GuideObjectType::ExplosiveBarrel: return "A movable hazard that explodes after a sufficiently hard impact.";
        case GuideObjectType::Tnt: return "A movable bundle of dynamite with a timed fuse. Heat, hard impacts, or nearby explosions ignite it.";
        case GuideObjectType::Teleforce: return "A Tesla-inspired particle projector that charges, then launches and destroys anything caught in its brief discharge.";
        case GuideObjectType::Rock: return "A decorative rock by Kiril.";
        case GuideObjectType::TeslaCoil: return "A powered high-voltage coil that periodically arcs through its surroundings, jolts physics objects, and broadcasts wireless power while discharging.";
        case GuideObjectType::ThornyVines: return "A climbable tangle of sharp vines that continuously damages players while they remain in contact.";
        case GuideObjectType::SignalDust: return "A placeable conductive trace that carries a fading 15-step signal from touching power sources into its configured channel.";
        case GuideObjectType::TernaryTrace: return "Carries a fading balanced-ternary signal: cyan for negative, gray for zero, and magenta for positive.";
        case GuideObjectType::TernarySwitch: return "Cycles between negative, zero, and positive states to drive a touching ternary trace network.";
    }
    return "An interactive object in the level.";
}

void DrawGuideObject(const GuideObject& object, Texture2D rockTexture) {
    if (!object.active || object.collected) return;
    Rectangle bounds = GetGuideObjectBounds(object);
    Vector2 center = Center(bounds);
    if (object.type == GuideObjectType::Rock) {
        if (rockTexture.id != 0) {
            DrawTexturePro(rockTexture,
                {0, 0, static_cast<float>(rockTexture.width), static_cast<float>(rockTexture.height)},
                {center.x, center.y, bounds.width, bounds.height},
                {bounds.width * 0.5f, bounds.height * 0.5f}, object.transform.rotation, WHITE);
        }
        return;
    }
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
        case GuideObjectType::Tnt: {
            if (object.broken) {
                if (object.timer < 0.35f) {
                    const float progress = object.timer / 0.35f;
                    const float radius = 18.0f + ExplosionRadius(object) * progress;
                    DrawCircleV(center, radius, Fade(Color{238, 82, 25, 255}, (1.0f - progress) * 0.72f));
                    DrawRing(center, radius * 0.46f, radius * 0.64f, 0.0f, 360.0f, 32,
                        Fade(YELLOW, 1.0f - progress));
                }
                break;
            }
            const float stickWidth = bounds.width / 5.2f;
            const float gap = fmaxf(1.0f, stickWidth * 0.08f);
            for (int index = 0; index < 5; ++index) {
                const Rectangle stick{bounds.x + index * (stickWidth + gap), bounds.y + 2.0f,
                    stickWidth, bounds.height - 4.0f};
                DrawRectangleRounded(stick, 0.38f, 5, Color{184, 38, 31, 255});
                DrawRectangleRoundedLinesEx(stick, 0.38f, 5, 1.5f, Color{72, 21, 19, 255});
                DrawLineEx({stick.x + 2.0f, stick.y + 5.0f}, {stick.x + 2.0f, stick.y + stick.height - 5.0f},
                    1.5f, Color{232, 83, 59, 255});
            }
            const float strapHeight = fmaxf(3.0f, bounds.height * 0.12f);
            DrawRectangleRec({bounds.x - 1.0f, bounds.y + bounds.height * 0.22f, bounds.width + 2.0f, strapHeight},
                Color{50, 53, 55, 255});
            DrawRectangleRec({bounds.x - 1.0f, bounds.y + bounds.height * 0.68f, bounds.width + 2.0f, strapHeight},
                Color{50, 53, 55, 255});
            const int fontSize = std::max(8, static_cast<int>(fminf(bounds.width * 0.24f, bounds.height * 0.30f)));
            const int labelWidth = MeasureText("TNT", fontSize);
            DrawRectangleRec({center.x - labelWidth * 0.5f - 3.0f, center.y - fontSize * 0.5f - 1.0f,
                static_cast<float>(labelWidth + 6), static_cast<float>(fontSize + 2)}, Color{238, 218, 172, 255});
            DrawText("TNT", static_cast<int>(center.x - labelWidth * 0.5f),
                static_cast<int>(center.y - fontSize * 0.5f), fontSize, Color{50, 24, 20, 255});
            const Vector2 fuseStart{bounds.x + bounds.width * 0.78f, bounds.y + 3.0f};
            const Vector2 fuseEnd{bounds.x + bounds.width * 0.94f, bounds.y - fmaxf(7.0f, bounds.height * 0.22f)};
            DrawLineEx(fuseStart, fuseEnd, 3.0f, Color{61, 50, 40, 255});
            if (object.triggered) {
                const float pulse = 2.5f + 1.5f * sinf(object.timer * 22.0f);
                DrawCircleV(fuseEnd, pulse + 2.0f, Fade(ORANGE, 0.65f));
                DrawCircleV(fuseEnd, pulse, YELLOW);
                DrawLineEx({fuseEnd.x - 5.0f, fuseEnd.y}, {fuseEnd.x + 5.0f, fuseEnd.y}, 1.0f, ORANGE);
                DrawLineEx({fuseEnd.x, fuseEnd.y - 5.0f}, {fuseEnd.x, fuseEnd.y + 5.0f}, 1.0f, ORANGE);
            }
            break;
        }
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
        case GuideObjectType::AntiGravityPad: {
            const bool energized = IsAntiGravityPadActive(object);
            const Rectangle field = GetAntiGravityFieldBounds(object);
            const Color fieldColor = energized ? Color{77, 226, 255, 255} : Color{91, 111, 117, 255};

            if (energized) {
                DrawRectangleRec(field, Fade(fieldColor, 0.035f));
                DrawLineEx({field.x, field.y}, {field.x, field.y + field.height}, 1.0f, Fade(fieldColor, 0.22f));
                DrawLineEx({field.x + field.width, field.y}, {field.x + field.width, field.y + field.height}, 1.0f, Fade(fieldColor, 0.22f));
                for (int lane = 0; lane < 3; ++lane) {
                    const float x = field.x + field.width * (static_cast<float>(lane) + 1.0f) / 4.0f;
                    for (int marker = 0; marker < 3; ++marker) {
                        const float travel = fmodf(object.timer * 52.0f + marker * field.height / 3.0f, field.height);
                        const float y = field.y + field.height - travel;
                        const float halfWidth = fminf(7.0f, field.width * 0.09f);
                        DrawLineEx({x - halfWidth, y + 5.0f}, {x, y}, 2.0f, Fade(fieldColor, 0.68f));
                        DrawLineEx({x, y}, {x + halfWidth, y + 5.0f}, 2.0f, Fade(fieldColor, 0.68f));
                    }
                }
            }

            DrawRectangleRec(bounds, Color{42, 50, 55, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            DrawRectangleRec({bounds.x + 4.0f, bounds.y + 3.0f, bounds.width - 8.0f, 4.0f}, fieldColor);
            const float arrowCenterX = bounds.x + bounds.width * 0.5f;
            const float arrowY = bounds.y + bounds.height * 0.58f;
            DrawTriangle(
                {arrowCenterX, arrowY - 5.0f},
                {arrowCenterX - 7.0f, arrowY + 4.0f},
                {arrowCenterX + 7.0f, arrowY + 4.0f},
                energized ? RAYWHITE : Color{126, 139, 143, 255}
            );
            DrawBolt({bounds.x + 7.0f, bounds.y + bounds.height - 5.0f}, 2.5f);
            DrawBolt({bounds.x + bounds.width - 7.0f, bounds.y + bounds.height - 5.0f}, 2.5f);
            break;
        }
        case GuideObjectType::TripodDerrick: {
            const Vector2 apex{center.x, bounds.y + 8.0f};
            const Color timber{113, 76, 46, 255};
            DrawLineEx(apex, {bounds.x + 8.0f, bounds.y + bounds.height}, 11.0f, BLACK);
            DrawLineEx(apex, {bounds.x + bounds.width - 8.0f, bounds.y + bounds.height}, 11.0f, BLACK);
            DrawLineEx(apex, {center.x + bounds.width * 0.18f, bounds.y + bounds.height}, 9.0f, BLACK);
            DrawLineEx(apex, {bounds.x + 8.0f, bounds.y + bounds.height}, 7.0f, timber);
            DrawLineEx(apex, {bounds.x + bounds.width - 8.0f, bounds.y + bounds.height}, 7.0f, timber);
            DrawLineEx(apex, {center.x + bounds.width * 0.18f, bounds.y + bounds.height}, 5.0f, Color{139, 91, 52, 255});
            DrawCircleV(apex, 9.0f, darkSteel);
            DrawCircleLinesV(apex, 9.0f, BLACK);
            break;
        }
        case GuideObjectType::ChainHoist: {
            DrawCircleV(object.constraint.anchorA, 15.0f, darkSteel);
            DrawCircleLinesV(object.constraint.anchorA, 15.0f, BLACK);
            DrawWheel(object.constraint.anchorA, 10.0f, object.timer * object.speed * RAD2DEG, Color{177, 133, 48, 255}, 6);
            const Vector2 load = object.constraint.anchorB;
            for (float y = object.constraint.anchorA.y + 15.0f; y < load.y; y += 9.0f) {
                DrawEllipseLines(static_cast<int>(object.constraint.anchorA.x), static_cast<int>(y), 4.0f, 6.0f, LIGHTGRAY);
            }
            DrawLineEx({load.x - 7.0f, load.y}, {load.x, load.y + 9.0f}, 4.0f, steel);
            DrawLineEx({load.x, load.y + 9.0f}, {load.x + 8.0f, load.y + 2.0f}, 4.0f, steel);
            break;
        }
        case GuideObjectType::BlockAndTackle: {
            const Vector2 top = object.constraint.anchorA;
            const Vector2 bottom = object.constraint.anchorB;
            DrawCircleV(top, 13.0f, Color{123, 92, 43, 255});
            DrawCircleV(bottom, 13.0f, Color{123, 92, 43, 255});
            DrawCircleLinesV(top, 13.0f, BLACK);
            DrawCircleLinesV(bottom, 13.0f, BLACK);
            DrawLineEx({top.x - 7.0f, top.y}, {bottom.x - 7.0f, bottom.y}, 2.0f, Color{188, 153, 91, 255});
            DrawLineEx({top.x + 7.0f, top.y}, {bottom.x + 7.0f, bottom.y}, 2.0f, Color{188, 153, 91, 255});
            DrawLineEx(top, bottom, 2.0f, Color{188, 153, 91, 255});
            DrawLineEx({bottom.x - 7.0f, bottom.y + 10.0f}, {bottom.x, bottom.y + 18.0f}, 4.0f, steel);
            DrawLineEx({bottom.x, bottom.y + 18.0f}, {bottom.x + 8.0f, bottom.y + 11.0f}, 4.0f, steel);
            break;
        }
        case GuideObjectType::ComeAlong: {
            const Vector2 anchor = object.constraint.anchorA;
            const Vector2 load = object.constraint.anchorB;
            DrawLineEx(anchor, load, 3.0f, Color{92, 97, 94, 255});
            DrawCircleV(anchor, 13.0f, Color{164, 109, 42, 255});
            DrawCircleLinesV(anchor, 13.0f, BLACK);
            const float leverAngle = sinf(object.timer * object.speed) * 0.65f - 1.0f;
            DrawLineEx(anchor, {anchor.x + cosf(leverAngle) * 40.0f, anchor.y + sinf(leverAngle) * 40.0f}, 7.0f, darkSteel);
            DrawCircleV(load, 5.0f, steel);
            break;
        }
        case GuideObjectType::ManualBenchGrinder: {
            DrawRectangle(static_cast<int>(center.x - 22.0f), static_cast<int>(center.y + object.collider.radius * 0.48f),
                44, 13, darkSteel);
            DrawCircleV(center, object.collider.radius, Color{104, 101, 91, 255});
            DrawCircleLinesV(center, object.collider.radius, BLACK);
            for (int spoke = 0; spoke < 8; ++spoke) {
                const float angle = (object.transform.rotation + spoke * 45.0f) * DEG2RAD;
                DrawLineEx(center,
                    {center.x + cosf(angle) * object.collider.radius * 0.88f,
                     center.y + sinf(angle) * object.collider.radius * 0.88f},
                    2.0f, Color{57, 58, 57, 255});
            }
            DrawCircleV(center, 5.0f, Color{180, 133, 48, 255});
            DrawLineEx({center.x + object.collider.radius + 2.0f, center.y},
                {center.x + object.collider.radius + 22.0f, center.y + 12.0f}, 4.0f, Color{130, 85, 45, 255});
            break;
        }
        case GuideObjectType::LoadCart:
            DrawRectangleRec({bounds.x, bounds.y, bounds.width, bounds.height * 0.68f}, Color{103, 73, 48, 255});
            DrawRectangleLinesEx({bounds.x, bounds.y, bounds.width, bounds.height * 0.68f}, 3.0f, BLACK);
            DrawLineEx({bounds.x + bounds.width, bounds.y + 5.0f}, {bounds.x + bounds.width + 24.0f, bounds.y - 8.0f}, 5.0f, steel);
            DrawCircleV({bounds.x + bounds.width * 0.23f, bounds.y + bounds.height * 0.78f}, bounds.height * 0.22f, darkSteel);
            DrawCircleV({bounds.x + bounds.width * 0.77f, bounds.y + bounds.height * 0.78f}, bounds.height * 0.22f, darkSteel);
            DrawCircleLinesV({bounds.x + bounds.width * 0.23f, bounds.y + bounds.height * 0.78f}, bounds.height * 0.22f, BLACK);
            DrawCircleLinesV({bounds.x + bounds.width * 0.77f, bounds.y + bounds.height * 0.78f}, bounds.height * 0.22f, BLACK);
            break;
        case GuideObjectType::RevolvingStoneGate: {
            const Rectangle slab{center.x, center.y, bounds.width, bounds.height};
            DrawRectanglePro(slab, {bounds.width * 0.5f, bounds.height * 0.5f}, object.transform.rotation, Color{124, 118, 102, 255});
            DrawCircleV(center, 7.0f, Color{50, 54, 55, 255});
            DrawCircleLinesV(center, 7.0f, BLACK);
            DrawCircleLinesV(center, fminf(bounds.width, bounds.height) * 0.42f, Fade(BLACK, 0.25f));
            break;
        }
        case GuideObjectType::RockHammer: {
            const Vector2 handleStart{bounds.x + 8.0f, center.y};
            const Vector2 head{bounds.x + bounds.width - bounds.height * 0.48f, center.y};
            DrawLineEx(handleStart, head, fmaxf(5.0f, bounds.height * 0.22f), Color{115, 78, 46, 255});
            DrawWheel(head, bounds.height * 0.43f, object.transform.rotation, Color{76, 82, 82, 255}, 10);
            break;
        }
        case GuideObjectType::FieldCoil: {
            const bool powered = object.power.channel == 0 || object.power.powered;
            DrawCircleV(center, object.collider.radius * 0.72f, darkSteel);
            DrawCircleLinesV(center, object.collider.radius * 0.72f, BLACK);
            for (int ring = 0; ring < 5; ++ring) {
                DrawCircleLinesV(center, object.collider.radius * (0.28f + ring * 0.09f),
                    powered ? Color{210, 113, 50, 255} : Color{111, 78, 53, 255});
            }
            DrawCircleV(center, object.collider.radius * 0.18f, BLACK);
            if (powered) DrawCircleLinesV(center, object.collider.radius, Fade(SKYBLUE, 0.38f));
            break;
        }
        case GuideObjectType::SpokePress: {
            DrawRectangleRec(bounds, Color{69, 80, 82, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            for (int tooth = 0; tooth < 4; ++tooth) {
                const float tx = bounds.x + (tooth + 0.5f) * bounds.width / 4.0f;
                DrawLineEx({tx, bounds.y + bounds.height}, {tx, bounds.y + bounds.height + 8.0f}, 4.0f, darkSteel);
            }
            DrawLineEx(object.origin, object.transform.position, 7.0f, Color{173, 125, 45, 255});
            break;
        }
        case GuideObjectType::PolarisTelescope: {
            const Vector2 end = Add(object.transform.position, Scale(object.direction, object.length));
            const Vector2 normal{-object.direction.y, object.direction.x};
            DrawLineEx(object.transform.position, end, 12.0f, BLACK);
            DrawLineEx(object.transform.position, end, 8.0f, Color{78, 91, 96, 255});
            DrawLineEx(Add(end, Scale(normal, -10.0f)), Add(end, Scale(normal, 10.0f)), 4.0f, Color{175, 144, 73, 255});
            DrawLineEx(object.transform.position,
                {object.transform.position.x - 18.0f, object.transform.position.y + 50.0f}, 5.0f, steel);
            DrawLineEx(object.transform.position,
                {object.transform.position.x + 18.0f, object.transform.position.y + 50.0f}, 5.0f, steel);
            DrawLineEx(object.transform.position, end, 1.0f, object.sensor.active ? GREEN : Fade(SKYBLUE, 0.35f));
            break;
        }
        case GuideObjectType::Sundial: {
            DrawCircleV(center, object.collider.radius, Color{181, 164, 119, 255});
            DrawCircleLinesV(center, object.collider.radius, BLACK);
            for (int tick = 0; tick < 12; ++tick) {
                const float angle = (tick * 30.0f - 90.0f) * DEG2RAD;
                DrawLineEx(
                    {center.x + cosf(angle) * object.collider.radius * 0.72f,
                     center.y + sinf(angle) * object.collider.radius * 0.72f},
                    {center.x + cosf(angle) * object.collider.radius * 0.90f,
                     center.y + sinf(angle) * object.collider.radius * 0.90f}, 2.0f, darkSteel);
            }
            const float shadowAngle = (object.timer / object.interval * 360.0f - 90.0f) * DEG2RAD;
            DrawTriangle(center,
                {center.x + cosf(shadowAngle) * object.collider.radius * 0.75f,
                 center.y + sinf(shadowAngle) * object.collider.radius * 0.75f},
                {center.x + 4.0f, center.y + 2.0f}, object.sensor.active ? Color{63, 76, 68, 255} : Color{86, 77, 65, 255});
            break;
        }
        case GuideObjectType::PerpetualMotionHolder: {
            const Color coil = object.triggered ? Color{210, 113, 50, 255} : Color{121, 76, 48, 255};
            const float legWidth = bounds.width * 0.24f;
            DrawRectangleRec({bounds.x, bounds.y + bounds.height * 0.25f, legWidth, bounds.height * 0.75f}, darkSteel);
            DrawRectangleRec({bounds.x + bounds.width - legWidth, bounds.y + bounds.height * 0.25f, legWidth, bounds.height * 0.75f}, darkSteel);
            DrawRectangleRec({bounds.x, bounds.y + bounds.height * 0.72f, bounds.width, bounds.height * 0.28f}, darkSteel);
            for (int winding = 0; winding < 4; ++winding) {
                const float wy = bounds.y + bounds.height * (0.40f + winding * 0.10f);
                DrawLineEx({bounds.x - 2.0f, wy}, {bounds.x + legWidth + 2.0f, wy}, 3.0f, coil);
                DrawLineEx({bounds.x + bounds.width - legWidth - 2.0f, wy}, {bounds.x + bounds.width + 2.0f, wy}, 3.0f, coil);
            }
            DrawRectangleRec({bounds.x - 3.0f, bounds.y + 2.0f, bounds.width + 6.0f, bounds.height * 0.23f},
                object.triggered ? steel : Color{83, 88, 87, 255});
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            if (object.triggered) DrawLineEx({bounds.x + legWidth, bounds.y + bounds.height * 0.22f},
                {bounds.x + bounds.width - legWidth, bounds.y + bounds.height * 0.22f}, 2.0f, SKYBLUE);
            break;
        }
        case GuideObjectType::Bellows: {
            const float squash = 1.0f - object.phase * 0.28f;
            DrawRectangleRec({bounds.x + 4.0f, bounds.y + 4.0f, bounds.width - 8.0f,
                (bounds.height - 8.0f) * squash}, Color{151, 91, 53, 255});
            for (int fold = 1; fold < 4; ++fold) {
                const float fy = bounds.y + fold * bounds.height * squash / 4.0f;
                DrawLineEx({bounds.x + 5.0f, fy}, {bounds.x + bounds.width - 5.0f, fy}, 2.0f, BLACK);
            }
            DrawLineEx(center, Add(center, Scale(object.direction, bounds.width * 0.72f)), 8.0f, darkSteel);
            break;
        }
        case GuideObjectType::Trampoline:
            DrawLineEx({bounds.x + 5.0f, bounds.y + object.phase * 4.0f},
                {bounds.x + bounds.width - 5.0f, bounds.y + object.phase * 4.0f}, 7.0f, Color{51, 161, 201, 255});
            DrawLineEx({bounds.x + 8.0f, bounds.y + 5.0f}, {bounds.x + 2.0f, bounds.y + bounds.height}, 4.0f, darkSteel);
            DrawLineEx({bounds.x + bounds.width - 8.0f, bounds.y + 5.0f},
                {bounds.x + bounds.width - 2.0f, bounds.y + bounds.height}, 4.0f, darkSteel);
            break;
        case GuideObjectType::BoxingGlove: {
            const Vector2 glove = Add(center, Scale(object.direction, object.length * object.phase));
            DrawRectangleRec(bounds, darkSteel);
            DrawLineEx(center, glove, 7.0f, Color{191, 194, 185, 255});
            DrawCircleV(glove, fmaxf(9.0f, bounds.height * 0.42f), Color{188, 46, 48, 255});
            DrawCircleLinesV(glove, fmaxf(9.0f, bounds.height * 0.42f), BLACK);
            break;
        }
        case GuideObjectType::Vacuum: {
            DrawRectangleRec(bounds, Color{55, 112, 129, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            const Vector2 nozzle = Add(center, Scale(object.direction, bounds.width * 0.72f));
            DrawLineEx(center, nozzle, 9.0f, darkSteel);
            DrawCircleV({bounds.x + bounds.width * 0.28f, bounds.y + bounds.height}, 7.0f, BLACK);
            DrawCircleV({bounds.x + bounds.width * 0.72f, bounds.y + bounds.height}, 7.0f, BLACK);
            if (object.power.channel == 0 || object.power.powered) {
                for (int line = -1; line <= 1; ++line) {
                    const Vector2 normal{-object.direction.y, object.direction.x};
                    DrawLineEx(Add(nozzle, Scale(normal, line * 6.0f)),
                        Add(Add(nozzle, Scale(object.direction, object.length * 0.65f)), Scale(normal, line * 16.0f)),
                        1.0f, Fade(SKYBLUE, 0.48f));
                }
            }
            break;
        }
        case GuideObjectType::MagnifyingGlass: {
            DrawCircleV(object.transform.position, object.collider.radius, Fade(SKYBLUE, 0.28f));
            DrawCircleLinesV(object.transform.position, object.collider.radius, object.triggered ? YELLOW : LIGHTGRAY);
            DrawLineEx(Add(object.transform.position, {object.collider.radius * 0.7f, object.collider.radius * 0.7f}),
                Add(object.transform.position, {object.collider.radius * 1.5f, object.collider.radius * 1.5f}), 6.0f, Color{104, 73, 43, 255});
            if (object.triggered) DrawLineEx(object.transform.position,
                Add(object.transform.position, Scale(object.direction, object.length)), 2.0f, YELLOW);
            break;
        }
        case GuideObjectType::LightSource:
            DrawCircleV(object.transform.position, 9.0f, object.power.channel == 0 || object.power.powered ? YELLOW : GRAY);
            DrawCircleLinesV(object.transform.position, 9.0f, BLACK);
            if (object.power.channel == 0 || object.power.powered) DrawLineEx(object.transform.position,
                Add(object.transform.position, Scale(object.direction, object.length)), fmaxf(2.0f, object.width), Fade(YELLOW, 0.62f));
            break;
        case GuideObjectType::IgnitableFuse:
            DrawLineEx({bounds.x, bounds.y + bounds.height}, {bounds.x + bounds.width, bounds.y}, 4.0f,
                object.triggered ? ORANGE : Color{93, 75, 52, 255});
            if (object.triggered) DrawCircleV({bounds.x + bounds.width, bounds.y}, 5.0f, YELLOW);
            break;
        case GuideObjectType::ProgrammableBall:
            DrawCircleV(object.transform.position, object.collider.radius, Color{76, 169, 207, 255});
            DrawCircleLinesV(object.transform.position, object.collider.radius, BLACK);
            DrawText("P", static_cast<int>(object.transform.position.x - 4.0f),
                static_cast<int>(object.transform.position.y - 7.0f), 14, RAYWHITE);
            break;
        case GuideObjectType::TransRotoConverter:
            DrawWheel(object.transform.position, object.collider.radius, object.transform.rotation, Color{190, 141, 45, 255}, 6);
            DrawLineEx({center.x - object.collider.radius, center.y}, {center.x - object.collider.radius - 16.0f, center.y}, 5.0f, steel);
            break;
        case GuideObjectType::RotoTransConverter:
            DrawRectangleRec(bounds, Color{174, 130, 42, 255});
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            DrawLineEx(object.origin, object.transform.position, 5.0f, LIGHTGRAY);
            DrawCircleV(center, fminf(bounds.width, bounds.height) * 0.24f, darkSteel);
            break;
        case GuideObjectType::AcceleratorTube:
            DrawRectangleRec(bounds, Fade(SKYBLUE, 0.18f));
            DrawRectangleLinesEx(bounds, 3.0f, steel);
            for (int marker = 1; marker < 4; ++marker) {
                const Vector2 p{bounds.x + bounds.width * marker / 4.0f, bounds.y + bounds.height * 0.5f};
                DrawTriangle(Add(p, Scale(object.direction, 8.0f)), Add(p, {-object.direction.y * 6.0f, object.direction.x * 6.0f}),
                    Add(p, {object.direction.y * 6.0f, -object.direction.x * 6.0f}), SKYBLUE);
            }
            break;
        case GuideObjectType::PinballBumper:
            DrawCircleV(center, object.collider.radius, Color{211, 55, 76, 255});
            DrawRing(center, object.collider.radius * 0.55f, object.collider.radius * 0.78f, 0.0f, 360.0f, 24, YELLOW);
            DrawCircleLinesV(center, object.collider.radius, BLACK);
            break;
        case GuideObjectType::PinballFlipper: {
            const float angle = object.direction.x * object.phase * -32.0f;
            DrawRectanglePro({bounds.x, bounds.y, bounds.width, bounds.height},
                {object.direction.x > 0.0f ? 0.0f : bounds.width, bounds.height * 0.5f}, angle,
                Color{224, 72, 72, 255});
            DrawCircleV({object.direction.x > 0.0f ? bounds.x : bounds.x + bounds.width, center.y}, bounds.height * 0.42f, darkSteel);
            break;
        }
        case GuideObjectType::LaserEmitter: {
            const Color color = LaserColor(object.colorMask);
            DrawRectangle(static_cast<int>(object.transform.position.x - 8.0f), static_cast<int>(object.transform.position.y - 8.0f), 16, 16, darkSteel);
            if (object.power.channel == 0 || object.power.powered) DrawLineEx(object.transform.position,
                Add(object.transform.position, Scale(object.direction, object.length)), 3.0f, color);
            break;
        }
        case GuideObjectType::LaserMixer: {
            const Color color = LaserColor(object.colorMask, object.triggered ? 255 : 90);
            DrawRectangleRec(bounds, darkSteel);
            DrawRectangleLinesEx(bounds, 2.0f, color);
            DrawCircleV(center, fminf(bounds.width, bounds.height) * 0.25f, color);
            if (object.triggered) DrawLineEx(center, Add(center, Scale(object.direction, object.length)), 3.0f, color);
            break;
        }
        case GuideObjectType::LaserDetector:
            DrawRectangleRec(bounds, darkSteel);
            DrawRectangleLinesEx(bounds, 3.0f, LaserColor(object.colorMask));
            DrawCircleV(center, fminf(bounds.width, bounds.height) * 0.24f,
                object.sensor.active ? LaserColor(object.colorMask) : BLACK);
            break;
        case GuideObjectType::HotAirBalloon:
            DrawCircleV(object.transform.position, object.collider.radius, Color{213, 91, 57, 255});
            DrawRing(object.transform.position, object.collider.radius * 0.78f, object.collider.radius * 0.92f, 0.0f, 360.0f, 24, Color{237, 187, 75, 255});
            DrawLineEx({center.x - 8.0f, center.y + object.collider.radius * 0.75f}, {center.x - 6.0f, center.y + object.collider.radius + 12.0f}, 2.0f, DARKBROWN);
            DrawLineEx({center.x + 8.0f, center.y + object.collider.radius * 0.75f}, {center.x + 6.0f, center.y + object.collider.radius + 12.0f}, 2.0f, DARKBROWN);
            DrawRectangle(static_cast<int>(center.x - 8.0f), static_cast<int>(center.y + object.collider.radius + 10.0f), 16, 10, BROWN);
            break;
        case GuideObjectType::Blimp:
            DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), bounds.width * 0.5f, bounds.height * 0.42f, Color{182, 190, 184, 255});
            DrawEllipseLines(static_cast<int>(center.x), static_cast<int>(center.y), bounds.width * 0.5f, bounds.height * 0.42f, BLACK);
            DrawTriangle({bounds.x, center.y}, {bounds.x - 14.0f, center.y - 11.0f}, {bounds.x - 14.0f, center.y + 11.0f}, Color{182, 190, 184, 255});
            DrawRectangle(static_cast<int>(center.x - 10.0f), static_cast<int>(bounds.y + bounds.height * 0.78f), 20, 8, BROWN);
            break;
        case GuideObjectType::LeakyBucket: {
            DrawRectangleRec(bounds, Color{91, 102, 107, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            const float fill = object.capacity <= 0.0f ? 0.0f : std::clamp(object.capacity / fmaxf(0.1f, object.body.mass), 0.0f, 1.0f);
            if (fill > 0.05f) DrawRectangleRec({bounds.x + 4.0f, bounds.y + bounds.height * (1.0f - fill),
                bounds.width - 8.0f, fmaxf(1.0f, bounds.height * fill - 4.0f)}, Fade(BLUE, 0.72f));
            if (object.capacity > 0.0f) DrawCircleV({center.x, bounds.y + bounds.height + 5.0f + fmodf(object.timer * 34.0f, 10.0f)}, 2.0f, BLUE);
            break;
        }
        case GuideObjectType::DestructibleWood:
        case GuideObjectType::DestructibleBrick:
        case GuideObjectType::DestructibleGlass: {
            if (object.broken) break;
            const Color material = object.type == GuideObjectType::DestructibleWood ? Color{133, 85, 47, 255} :
                (object.type == GuideObjectType::DestructibleBrick ? Color{151, 66, 54, 255} : Color{139, 216, 232, 130});
            DrawRectangleRec(bounds, material);
            DrawRectangleLinesEx(bounds, 2.0f, BLACK);
            const float crack = Clamp01(object.damage / fmaxf(1.0f, object.strength));
            if (crack > 0.2f) DrawLineEx({bounds.x + bounds.width * 0.45f, bounds.y},
                {bounds.x + bounds.width * (0.35f + crack * 0.2f), bounds.y + bounds.height}, 2.0f, BLACK);
            break;
        }
        case GuideObjectType::GlassPane: {
            if (object.broken) break;
            DrawRectangleRec(bounds, Color{150, 224, 239, 72});
            DrawRectangleLinesEx(bounds, 3.0f, Color{112, 148, 158, 255});
            const float inset = fminf(5.0f, fminf(bounds.width, bounds.height) * 0.18f);
            DrawRectangleLinesEx({bounds.x + inset, bounds.y + inset,
                bounds.width - inset * 2.0f, bounds.height - inset * 2.0f}, 1.0f, Color{205, 246, 252, 190});
            DrawLineEx({bounds.x + inset + 2.0f, bounds.y + bounds.height - inset - 2.0f},
                {bounds.x + bounds.width - inset - 2.0f, bounds.y + inset + 2.0f}, 2.0f, Color{231, 253, 255, 180});
            break;
        }
        case GuideObjectType::GlassBlock: {
            if (object.broken) break;
            DrawRectangleRec(bounds, Color{104, 190, 211, 125});
            DrawRectangleLinesEx(bounds, 4.0f, Color{68, 118, 132, 255});
            const Rectangle inner{bounds.x + 7.0f, bounds.y + 7.0f,
                fmaxf(1.0f, bounds.width - 14.0f), fmaxf(1.0f, bounds.height - 14.0f)};
            DrawRectangleLinesEx(inner, 2.0f, Color{202, 243, 248, 205});
            DrawLineEx({inner.x, inner.y}, {inner.x + inner.width, inner.y + inner.height}, 1.0f, Color{79, 147, 166, 160});
            DrawLineEx({inner.x + inner.width, inner.y}, {inner.x, inner.y + inner.height}, 1.0f, Color{230, 252, 255, 130});
            break;
        }
        case GuideObjectType::IceBlock: {
            if (object.broken) break;
            DrawRectangleRec(bounds, Color{116, 205, 235, 220});
            DrawRectangleLinesEx(bounds, 3.0f, Color{210, 248, 255, 255});
            DrawRectangleLinesEx({bounds.x + 5.0f, bounds.y + 5.0f,
                fmaxf(1.0f, bounds.width - 10.0f), fmaxf(1.0f, bounds.height - 10.0f)},
                1.0f, Color{55, 139, 181, 210});
            DrawLineEx({bounds.x + bounds.width * 0.18f, bounds.y + bounds.height * 0.22f},
                {bounds.x + bounds.width * 0.56f, bounds.y + bounds.height * 0.08f}, 3.0f, Fade(WHITE, 0.75f));
            if (object.damage > 0.0f) {
                const float melt = Clamp01(object.damage / fmaxf(1.0f, object.strength));
                DrawLineEx(center, {bounds.x + bounds.width * (0.2f + melt * 0.45f), bounds.y + bounds.height},
                    2.0f, Color{48, 126, 165, 255});
            }
            break;
        }
        case GuideObjectType::IceSheet: {
            if (object.broken) break;
            DrawRectangleRec(bounds, Color{151, 225, 244, 205});
            DrawRectangleLinesEx(bounds, 2.0f, Color{221, 251, 255, 255});
            for (float x = bounds.x + 8.0f; x < bounds.x + bounds.width - 5.0f; x += 24.0f) {
                DrawLineEx({x, bounds.y + bounds.height * 0.68f},
                    {fminf(x + 14.0f, bounds.x + bounds.width - 3.0f), bounds.y + bounds.height * 0.28f},
                    1.0f, Fade(WHITE, 0.72f));
            }
            break;
        }
        case GuideObjectType::Heater: {
            DrawRectangleRec(bounds, Color{74, 78, 82, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            const bool powered = object.power.channel == 0 || object.power.powered;
            for (int bar = 0; bar < 3; ++bar) {
                const float offset = (bar - 1) * bounds.height * 0.22f;
                DrawLineEx({center.x - bounds.width * 0.28f, center.y + offset},
                    {center.x + bounds.width * 0.28f, center.y + offset}, 3.0f,
                    powered ? ORANGE : DARKGRAY);
            }
            const Vector2 end = Add(center, Scale(object.direction, object.length));
            for (int wave = 1; wave <= 3; ++wave) {
                const float progress = fmodf(object.timer * 1.7f + wave * 0.24f, 1.0f);
                const Vector2 point = Add(center, Scale(object.direction, object.length * progress));
                DrawCircleV(point, 2.0f + wave, powered ? Fade(ORANGE, 0.58f) : Fade(GRAY, 0.25f));
            }
            DrawLineEx(center, end, fmaxf(2.0f, object.width), powered ? Fade(ORANGE, 0.16f) : Fade(GRAY, 0.08f));
            break;
        }
        case GuideObjectType::Campfire: {
            DrawLineEx({center.x - 18.0f, center.y + 10.0f}, {center.x + 18.0f, center.y + 20.0f}, 7.0f, DARKBROWN);
            DrawLineEx({center.x + 18.0f, center.y + 10.0f}, {center.x - 18.0f, center.y + 20.0f}, 7.0f, BROWN);
            const float flicker = sinf(object.timer * 13.0f) * 3.0f;
            DrawTriangle({center.x, center.y - object.collider.radius - 12.0f - flicker},
                {center.x - object.collider.radius * 0.72f, center.y + 12.0f},
                {center.x + object.collider.radius * 0.72f, center.y + 12.0f}, ORANGE);
            DrawTriangle({center.x + 2.0f, center.y - object.collider.radius * 0.6f},
                {center.x - object.collider.radius * 0.34f, center.y + 10.0f},
                {center.x + object.collider.radius * 0.38f, center.y + 10.0f}, YELLOW);
            break;
        }
        case GuideObjectType::Torch: {
            const Rectangle shaft{center.x, center.y, bounds.width, bounds.height};
            DrawRectanglePro(shaft, {bounds.width * 0.5f, bounds.height * 0.5f}, object.transform.rotation,
                Color{112, 70, 38, 255});
            const float angle = object.transform.rotation * DEG2RAD;
            const Vector2 flame{center.x + sinf(angle) * bounds.height * 0.55f,
                center.y - cosf(angle) * bounds.height * 0.55f};
            DrawCircleV(flame, fmaxf(6.0f, bounds.width * 0.72f), ORANGE);
            DrawCircleV({flame.x, flame.y + 2.0f}, fmaxf(3.0f, bounds.width * 0.38f), YELLOW);
            break;
        }
        case GuideObjectType::WaterBucket: {
            const Rectangle bucket{center.x, center.y, bounds.width, bounds.height};
            DrawRectanglePro(bucket, {bounds.width * 0.5f, bounds.height * 0.5f}, object.transform.rotation,
                Color{102, 111, 116, 255});
            const float innerWidth = fmaxf(1.0f, bucket.width - 8.0f);
            const float innerHeight = fmaxf(1.0f, bucket.height - 9.0f);
            DrawRectanglePro({center.x, center.y + 2.0f, innerWidth, innerHeight},
                {innerWidth * 0.5f, innerHeight * 0.5f}, object.transform.rotation,
                object.capacity > 0.0f ? Color{46, 151, 213, 210} : Color{58, 64, 67, 255});
            DrawCircleLinesV(center, bounds.width * 0.72f, DARKGRAY);
            break;
        }
        case GuideObjectType::SignalDust: {
            const Vector2 start = object.transform.position;
            const Vector2 end = Add(start, Scale(object.direction, object.length));
            const bool powered = object.signalMask > 0 || object.power.powered;
            const Color shadow{50, 25, 23, 255};
            const Color trace = powered ? Color{255, 91, 36, 255} : Color{119, 45, 39, 255};
            const Color core = powered ? Color{255, 190, 67, 255} : Color{163, 65, 52, 255};
            DrawLineEx(start, end, 7.0f, shadow);
            DrawLineEx(start, end, 4.0f, trace);
            const int grains = std::max(2, static_cast<int>(object.length / 12.0f));
            for (int grain = 0; grain <= grains; ++grain) {
                const Vector2 point = Add(start, Scale(object.direction,
                    object.length * static_cast<float>(grain) / static_cast<float>(grains)));
                DrawCircleV(point, powered ? 2.4f : 1.8f, core);
            }
            DrawCircleLinesV(start, 4.0f, shadow);
            DrawCircleLinesV(end, 4.0f, shadow);
            break;
        }
        case GuideObjectType::TernaryTrace: {
            const Vector2 start = object.transform.position;
            const Vector2 end = Add(start, Scale(object.direction, object.length));
            const Color shadow{25, 29, 35, 255};
            const Color stateColor = object.signalMask < 0 ? Color{48, 220, 238, 255} :
                object.signalMask > 0 ? Color{232, 72, 225, 255} : Color{112, 120, 128, 255};
            const float glow = object.colorMask > 0 ? 1.0f : 0.58f;
            DrawLineEx(start, end, 8.0f, shadow);
            DrawLineEx(start, end, 4.5f, Fade(stateColor, glow));
            const int crystals = std::max(2, static_cast<int>(object.length / 13.0f));
            const Vector2 normal{-object.direction.y, object.direction.x};
            for (int crystal = 0; crystal <= crystals; ++crystal) {
                const float along = object.length * static_cast<float>(crystal) / static_cast<float>(crystals);
                const float side = crystal % 2 == 0 ? -2.0f : 2.0f;
                const Vector2 point = Add(Add(start, Scale(object.direction, along)), Scale(normal, side));
                DrawCircleV(point, object.colorMask > 0 ? 2.5f : 1.8f, stateColor);
            }
            break;
        }
        case GuideObjectType::TernarySwitch: {
            DrawRectangleRounded(bounds, 0.16f, 5, Color{42, 48, 55, 255});
            DrawRectangleRoundedLinesEx(bounds, 0.16f, 5, 2.0f, BLACK);
            const float centerY = bounds.y + bounds.height * 0.5f;
            const float left = bounds.x + bounds.width * 0.22f;
            const float middle = bounds.x + bounds.width * 0.5f;
            const float right = bounds.x + bounds.width * 0.78f;
            DrawLineEx({left, centerY}, {right, centerY}, 3.0f, Color{20, 23, 27, 255});
            DrawCircleV({left, centerY}, 4.0f, Color{48, 220, 238, 255});
            DrawCircleV({middle, centerY}, 4.0f, Color{112, 120, 128, 255});
            DrawCircleV({right, centerY}, 4.0f, Color{232, 72, 225, 255});
            const float knobX = object.signalMask < 0 ? left : object.signalMask > 0 ? right : middle;
            DrawCircleV({knobX, centerY}, 7.0f, RAYWHITE);
            DrawCircleLinesV({knobX, centerY}, 7.0f, BLACK);
            const char* state = object.signalMask < 0 ? "-" : object.signalMask > 0 ? "+" : "0";
            DrawText(state, static_cast<int>(knobX - MeasureText(state, 12) * 0.5f),
                static_cast<int>(centerY - 6.0f), 12, BLACK);
            break;
        }
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
        case GuideObjectType::Teleforce: {
            const bool powered = object.power.channel == 0 || object.power.powered;
            const float cycleDuration = object.capacity + object.damage + object.speed;
            const float cycleTime = powered
                ? fmodf(fmaxf(0.0f, object.timer), fmaxf(0.001f, cycleDuration)) : 0.0f;
            const float charge = powered ? Clamp01(cycleTime / object.capacity) : 0.0f;
            const bool firing = IsTeleforceFiring(object);
            const Vector2 forward = object.direction;
            const Vector2 side{-forward.y, forward.x};
            const Vector2 muzzle = TeleforceMuzzle(object);
            const Vector2 beamEnd = Add(muzzle, Scale(forward, object.length));
            const Color brass{175, 118, 39, 255};
            const Color copper{204, 83, 37, 255};
            const Color energized = firing ? RAYWHITE : Color{255, 175, 38, 255};

            DrawRectangleRec(bounds, Color{36, 43, 49, 255});
            DrawRectangleLinesEx(bounds, 3.0f, BLACK);
            DrawRectangleLinesEx({bounds.x + 4.0f, bounds.y + 4.0f,
                bounds.width - 8.0f, bounds.height - 8.0f}, 2.0f, brass);
            for (int coil = -1; coil <= 1; ++coil) {
                const Vector2 coilCenter = Add(center, Scale(side, coil * fminf(13.0f, bounds.height * 0.22f)));
                DrawCircleV(coilCenter, fminf(9.0f, bounds.height * 0.18f), BLACK);
                DrawRing(coilCenter, 3.0f, fminf(8.0f, bounds.height * 0.16f), 0.0f, 360.0f, 18,
                    Fade(copper, 0.45f + charge * 0.55f));
            }
            const Vector2 barrelRear = Add(center, Scale(forward, 2.0f));
            DrawLineEx(barrelRear, muzzle, fmaxf(10.0f, object.width * 0.55f), BLACK);
            DrawLineEx(barrelRear, muzzle, fmaxf(6.0f, object.width * 0.34f), steel);
            DrawLineEx(Add(muzzle, Scale(side, -object.width * 0.42f)),
                Add(muzzle, Scale(side, object.width * 0.42f)), 5.0f, brass);
            DrawCircleV(muzzle, 4.0f + charge * 3.0f, Fade(energized, 0.35f + charge * 0.55f));
            DrawRectangleRec({bounds.x + 5.0f, bounds.y + bounds.height - 8.0f,
                (bounds.width - 10.0f) * charge, 4.0f}, powered ? ORANGE : DARKGRAY);

            const float halfBeam = object.width * 0.5f;
            if (powered && !firing) {
                DrawLineEx(Add(muzzle, Scale(side, -halfBeam)), beamEnd, 1.0f, Fade(ORANGE, 0.22f + charge * 0.42f));
                DrawLineEx(Add(muzzle, Scale(side, halfBeam)), beamEnd, 1.0f, Fade(ORANGE, 0.22f + charge * 0.42f));
            }
            if (firing) {
                DrawLineEx(muzzle, beamEnd, object.width + 9.0f, Fade(ORANGE, 0.20f));
                DrawLineEx(muzzle, beamEnd, object.width, Color{255, 128, 25, 245});
                DrawLineEx(muzzle, beamEnd, fmaxf(2.0f, object.width * 0.28f), RAYWHITE);
                for (int particle = 1; particle <= 9; ++particle) {
                    const float amount = fmodf(particle * 0.113f + object.timer * 4.8f, 1.0f);
                    const float jitter = sinf(object.timer * 43.0f + particle * 2.7f) * halfBeam * 0.55f;
                    DrawCircleV(Add(Add(muzzle, Scale(forward, object.length * amount)), Scale(side, jitter)),
                        particle % 3 == 0 ? 3.0f : 2.0f, particle % 2 == 0 ? YELLOW : RAYWHITE);
                }
            }
            break;
        }
        case GuideObjectType::TeslaCoil: {
            const bool powered = object.power.channel == 0 || object.power.powered;
            const bool discharging = IsTeslaCoilDischarging(object);
            const float cycleDuration = object.capacity + object.damage + object.speed;
            const float cycleTime = powered
                ? fmodf(fmaxf(0.0f, object.timer), fmaxf(0.001f, cycleDuration)) : 0.0f;
            const float charge = powered ? Clamp01(cycleTime / object.capacity) : 0.0f;
            const Vector2 terminal = TeslaCoilTerminal(object);
            const float baseHeight = fmaxf(14.0f, bounds.height * 0.20f);
            const Rectangle base{bounds.x, bounds.y + bounds.height - baseHeight, bounds.width, baseHeight};
            const float towerBottom = base.y;
            const float towerHeight = fmaxf(12.0f, towerBottom - terminal.y - 8.0f);
            const Color copper{204, 91, 40, 255};
            const Color hotCopper{246, 155, 56, 255};

            DrawRectangleRounded(base, 0.16f, 5, Color{41, 49, 55, 255});
            DrawRectangleRoundedLinesEx(base, 0.16f, 5, 3.0f, BLACK);
            DrawRectangleRec({base.x + 5.0f, base.y + base.height - 6.0f,
                (base.width - 10.0f) * charge, 3.0f}, powered ? ORANGE : DARKGRAY);

            const int turns = std::max(6, static_cast<int>(towerHeight / 8.0f));
            Vector2 previous{center.x, towerBottom};
            for (int turn = 1; turn <= turns; ++turn) {
                const float amount = static_cast<float>(turn) / static_cast<float>(turns);
                const float halfSpan = fmaxf(5.0f, bounds.width * (0.24f - amount * 0.12f));
                const Vector2 point{center.x + (turn % 2 == 0 ? -halfSpan : halfSpan),
                    towerBottom - towerHeight * amount};
                DrawLineEx(previous, point, 5.0f, BLACK);
                DrawLineEx(previous, point, 2.5f, discharging ? hotCopper : copper);
                previous = point;
            }
            DrawLineEx(previous, terminal, 4.0f, discharging ? RAYWHITE : copper);

            const float toroidWidth = fmaxf(20.0f, bounds.width * 0.62f);
            const float toroidHeight = fmaxf(8.0f, bounds.height * 0.09f);
            DrawEllipse(static_cast<int>(terminal.x), static_cast<int>(terminal.y),
                toroidWidth * 0.5f, toroidHeight, BLACK);
            DrawEllipseLines(static_cast<int>(terminal.x), static_cast<int>(terminal.y),
                toroidWidth * 0.5f, toroidHeight, discharging ? RAYWHITE : LIGHTGRAY);
            DrawEllipse(static_cast<int>(terminal.x), static_cast<int>(terminal.y),
                toroidWidth * 0.30f, toroidHeight * 0.42f, Color{41, 49, 55, 255});

            if (powered && !discharging) {
                const float corona = 4.0f + charge * 7.0f;
                DrawCircleV(terminal, corona, Fade(SKYBLUE, 0.12f + charge * 0.28f));
            }
            if (discharging) {
                for (int arc = 0; arc < 6; ++arc) {
                    const float angle = object.timer * 9.0f + arc * (2.0f * Pi / 6.0f);
                    const float reach = object.length * (0.58f + 0.36f *
                        (0.5f + 0.5f * sinf(object.timer * 37.0f + arc * 3.1f)));
                    const Vector2 end = Add(terminal, {cosf(angle) * reach, sinf(angle) * reach});
                    Vector2 previousArc = terminal;
                    for (int segment = 1; segment <= 7; ++segment) {
                        const float amount = static_cast<float>(segment) / 7.0f;
                        Vector2 point = Add(terminal, Scale({end.x - terminal.x, end.y - terminal.y}, amount));
                        if (segment < 7) {
                            const Vector2 normal{-sinf(angle), cosf(angle)};
                            point = Add(point, Scale(normal,
                                sinf(object.timer * 53.0f + arc * 5.0f + segment * 2.4f) * 7.0f));
                        }
                        DrawLineEx(previousArc, point, 5.0f, Fade(SKYBLUE, 0.34f));
                        DrawLineEx(previousArc, point, 2.0f, RAYWHITE);
                        previousArc = point;
                    }
                }
                DrawCircleV(terminal, 8.0f, RAYWHITE);
                DrawCircleV(terminal, 4.0f, SKYBLUE);
            }
            break;
        }
        case GuideObjectType::ThornyVines: {
            const Color shadow{27, 45, 24, 255};
            const Color stem{50, 105, 48, 255};
            const Color highlight{91, 151, 66, 255};
            const Color thorn{224, 205, 151, 255};
            const int stemCount = std::max(2, static_cast<int>(bounds.width / 15.0f));
            const int segments = std::max(4, static_cast<int>(bounds.height / 18.0f));
            for (int vine = 0; vine < stemCount; ++vine) {
                const float lane = (static_cast<float>(vine) + 0.5f) / static_cast<float>(stemCount);
                Vector2 previous{bounds.x + bounds.width * lane, bounds.y + bounds.height};
                for (int segment = 1; segment <= segments; ++segment) {
                    const float amount = static_cast<float>(segment) / static_cast<float>(segments);
                    const float sway = sinf(amount * 8.0f + vine * 2.1f) *
                        fminf(8.0f, bounds.width / static_cast<float>(stemCount + 1));
                    const Vector2 point{bounds.x + bounds.width * lane + sway,
                        bounds.y + bounds.height * (1.0f - amount)};
                    DrawLineEx(previous, point, 5.0f, shadow);
                    DrawLineEx(previous, point, 2.5f, vine % 2 == 0 ? stem : highlight);

                    const Vector2 delta{point.x - previous.x, point.y - previous.y};
                    const float length = fmaxf(0.001f, sqrtf(delta.x * delta.x + delta.y * delta.y));
                    const Vector2 normal{-delta.y / length, delta.x / length};
                    const Vector2 thornBase{(previous.x + point.x) * 0.5f,
                        (previous.y + point.y) * 0.5f};
                    const float side = (segment + vine) % 2 == 0 ? 1.0f : -1.0f;
                    const Vector2 thornTip{thornBase.x + normal.x * 7.0f * side,
                        thornBase.y + normal.y * 7.0f * side};
                    DrawLineEx(thornBase, thornTip, 2.0f, shadow);
                    DrawCircleV(thornTip, 1.6f, thorn);

                    if ((segment + vine) % 2 == 0) {
                        const Vector2 leafCenter{thornBase.x - normal.x * 5.0f * side,
                            thornBase.y - normal.y * 5.0f * side};
                        DrawEllipse(static_cast<int>(leafCenter.x), static_cast<int>(leafCenter.y),
                            5.0f, 3.0f, Color{62, 126, 55, 255});
                    }
                    previous = point;
                }
            }
            DrawRectangleLinesEx(bounds, 1.0f, Fade(Color{143, 198, 96, 255}, 0.28f));
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
        case GuideObjectType::Checkpoint: {
            const float poleX = bounds.x + 5.0f;
            const float flagTop = bounds.y + 2.0f;
            const float flagWidth = fmaxf(12.0f, fminf(bounds.width - 7.0f, bounds.height * 0.58f));
            const float flagHeight = fmaxf(12.0f, fminf(bounds.height * 0.36f, flagWidth * 0.72f));
            DrawLineEx({poleX, bounds.y + bounds.height}, {poleX, bounds.y}, 4.0f, LIGHTGRAY);

            Vector2 flagTip;
            Vector2 flagTopRight;
            Vector2 flagBottom;
            if (object.triggered) {
                const float flutter = sinf(object.timer * 12.0f) * flagHeight * 0.16f;
                flagTopRight = {poleX + flagWidth, flagTop + flagHeight * 0.5f + flutter};
                flagBottom = {poleX + 1.0f, flagTop + flagHeight};
                flagTip = {poleX + 1.0f, flagTop};
            }
            else {
                flagTopRight = {poleX + flagWidth, flagTop};
                flagBottom = {poleX + flagWidth * 0.5f, flagTop + flagHeight};
                flagTip = {poleX + 1.0f, flagTop};
            }
            const Color flagGreen{54, 190, 83, 255};
            DrawTriangle(flagTip, flagTopRight, flagBottom, flagGreen);
            DrawLineEx(flagTip, flagTopRight, 1.5f, Color{23, 103, 48, 255});
            DrawLineEx(flagTopRight, flagBottom, 1.5f, Color{23, 103, 48, 255});
            DrawLineEx(flagBottom, flagTip, 1.5f, Color{23, 103, 48, 255});
            break;
        }
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
        case GuideObjectType::ScubaGear:
            DrawRectangleRounded({center.x - 5, center.y - 14, 10, 28}, 0.5f, 4, YELLOW);
            DrawRectangleRounded({center.x - 13, center.y - 8, 26, 10}, 0.4f, 4, DARKBLUE);
            DrawRectangleRounded({center.x - 10, center.y - 6, 20, 6}, 0.4f, 4, SKYBLUE);
            DrawLineEx({center.x + 10, center.y}, {center.x + 10, center.y + 10}, 3, DARKGRAY);
            break;
        case GuideObjectType::GasMask:
            DrawCircleV(center, object.collider.radius, Color{93, 117, 92, 255});
            DrawCircleV({center.x - object.collider.radius * 0.42f, center.y}, object.collider.radius * 0.32f, Color{31, 40, 34, 255});
            DrawCircleV({center.x + object.collider.radius * 0.42f, center.y}, object.collider.radius * 0.32f, Color{31, 40, 34, 255});
            break;
    }
}
