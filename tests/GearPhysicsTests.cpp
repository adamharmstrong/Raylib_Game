#include "Machine.h"
#include "Level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
    constexpr float TestRadius = 24.0f;
    constexpr float ContactDistance = TestRadius * GearOuterRadiusScale * 2.0f;

    Gear MakeGear(GearVisualType type, GearMounting mounting, GearOrientation orientation,
                  Vector2 center, float angularVelocity) {
        Gear gear{};
        gear.center = center;
        gear.radius = TestRadius;
        gear.mass = 2.0f;
        gear.visualType = type;
        gear.mounting = mounting;
        gear.orientation = orientation;
        gear.toothCount = 24;
        gear.angularVelocity = angularVelocity;
        return gear;
    }

    void Solve(std::vector<Gear>& gears, std::vector<StoneBlock>& blocks) {
        std::vector<Boulder> boulders;
        std::vector<PhysicsWheel> wheels;
        std::vector<Flywheel> flywheels;
        ResolveDynamicBodyCollisions(blocks, boulders, wheels, gears, flywheels);
    }

    bool NearlyEqual(float a, float b, float tolerance = 0.001f) {
        return fabsf(a - b) <= tolerance;
    }
}

int main() {
    const std::array<GearVisualType, 7> types{
        GearVisualType::Spur,
        GearVisualType::LanternPinion,
        GearVisualType::Ratchet,
        GearVisualType::Escape,
        GearVisualType::Bevel,
        GearVisualType::Sector,
        GearVisualType::Count
    };

    for (GearVisualType type : types) {
        std::vector<Gear> gears{
            MakeGear(GearVisualType::Spur, GearMounting::Mounted,
                GearOrientation::Vertical, {0.0f, 0.0f}, 100.0f),
            MakeGear(type, GearMounting::Dynamic, GearOrientation::Vertical,
                {ContactDistance, 0.0f}, 0.0f)
        };
        if (type == GearVisualType::Sector) {
            gears[1].rotation = 180.0f;
        }
        std::vector<StoneBlock> blocks;
        Solve(gears, blocks);
        if (gears[1].angularVelocity >= -0.01f) {
            std::cerr << "Gear type " << static_cast<int>(type)
                      << " did not transfer torque to a touching physics gear.\n";
            return 1;
        }
    }

    {
        const Level clocktower = LoadLevelFromFile(CLOCKTOWER_LEVEL_PATH, {});
        if (clocktower.gears.size() != 28) {
            std::cerr << "Clocktower did not load all 28 unified gear objects.\n";
            return 1;
        }

        std::array<bool, 7> foundTypes{};
        int clockHandGears = 0;
        int movementContacts = 0;
        int centerArborGears = 0;
        const auto distanceBetween = [](Vector2 a, Vector2 b) {
            const float deltaX = a.x - b.x;
            const float deltaY = a.y - b.y;
            return sqrtf(deltaX * deltaX + deltaY * deltaY);
        };
        for (const Gear& gear : clocktower.gears) {
            if (gear.mounting != GearMounting::Mounted) {
                std::cerr << "Clocktower contains a gear without its mounted physics constraint.\n";
                return 1;
            }
            if (gear.layer != WorldLayer::Background) {
                std::cerr << "Clocktower gear was not migrated to the background physics layer.\n";
                return 1;
            }
            foundTypes[static_cast<size_t>(gear.visualType)] = true;
            if (gear.clockHand != ClockHandType::None) ++clockHandGears;
            if (distanceBetween(gear.center, clocktower.clockFaceCenter) <= 1.0f) {
                ++centerArborGears;
            }
        }
        for (size_t first = 0; first < clocktower.gears.size(); ++first) {
            const Gear& a = clocktower.gears[first];
            if (a.clockHand != ClockHandType::None) continue;
            for (size_t second = first + 1; second < clocktower.gears.size(); ++second) {
                const Gear& b = clocktower.gears[second];
                if (b.clockHand != ClockHandType::None) continue;
                const bool compatiblePlanes = a.orientation == b.orientation ||
                    a.visualType == GearVisualType::Bevel || b.visualType == GearVisualType::Bevel;
                const float contactDistance = (a.radius + b.radius) * GearOuterRadiusScale + 1.0f;
                const float arborDistance = distanceBetween(a.center, b.center);
                if (compatiblePlanes && arborDistance > 1.0f && arborDistance <= contactDistance) {
                    ++movementContacts;
                }
            }
        }
        for (bool found : foundTypes) {
            if (!found) {
                std::cerr << "Clocktower parser coverage is missing a reusable gear type.\n";
                return 1;
            }
        }
        if (clockHandGears != 3) {
            std::cerr << "Clocktower did not preserve its three physics-linked hand brakes.\n";
            return 1;
        }
        if (movementContacts < 15) {
            std::cerr << "Clocktower movement is no longer planted as compact meshing gear trains.\n";
            return 1;
        }
        if (centerArborGears < 2) {
            std::cerr << "Clocktower motion work is no longer coaxial with the hand arbor.\n";
            return 1;
        }
    }

    {
        const Level gallery = LoadLevelFromFile(GEAR_RENDER_GALLERY_PATH, {});
        if (gallery.gears.size() != 21) {
            std::cerr << "Gear render gallery did not load its 21 isolated samples.\n";
            return 1;
        }

        std::array<int, 7> typeCounts{};
        int verticalSamples = 0;
        int horizontalSamples = 0;
        int smallSamples = 0;
        for (const Gear& gear : gallery.gears) {
            if (gear.mounting != GearMounting::Mounted || gear.layer != WorldLayer::Background) {
                std::cerr << "Gear render gallery contains a moving or player-colliding sample.\n";
                return 1;
            }
            ++typeCounts[static_cast<size_t>(gear.visualType)];
            if (gear.orientation == GearOrientation::Vertical) ++verticalSamples;
            else ++horizontalSamples;
            if (gear.radius <= 30.0f) ++smallSamples;
        }
        for (int count : typeCounts) {
            if (count != 3) {
                std::cerr << "Gear render gallery does not show each reusable type three times.\n";
                return 1;
            }
        }
        if (verticalSamples != 7 || horizontalSamples != 14 || smallSamples != 7) {
            std::cerr << "Gear render gallery row coverage is incorrect.\n";
            return 1;
        }
        if (gallery.labels.empty() || std::any_of(gallery.labels.begin(), gallery.labels.end(),
                [](const LevelLabel& label) { return label.fontSize >= 24; })) {
            std::cerr << "Gear render gallery labels are no longer using the compact label style.\n";
            return 1;
        }

        for (size_t first = 0; first < gallery.gears.size(); ++first) {
            for (size_t second = first + 1; second < gallery.gears.size(); ++second) {
                const Gear& a = gallery.gears[first];
                const Gear& b = gallery.gears[second];
                const float deltaX = a.center.x - b.center.x;
                const float deltaY = a.center.y - b.center.y;
                const float distance = sqrtf(deltaX * deltaX + deltaY * deltaY);
                if (distance < a.radius + b.radius + 24.0f) {
                    std::cerr << "Gear render gallery samples overlap each other.\n";
                    return 1;
                }
            }
        }
    }

    {
        Gear backgroundDriver = MakeGear(GearVisualType::Spur, GearMounting::Mounted,
            GearOrientation::Vertical, {0.0f, 0.0f}, 100.0f);
        backgroundDriver.layer = WorldLayer::Background;
        Gear middlegroundGear = MakeGear(GearVisualType::Spur, GearMounting::Dynamic,
            GearOrientation::Vertical, {ContactDistance - 8.0f, 0.0f}, 0.0f);
        middlegroundGear.layer = WorldLayer::Middleground;
        const Vector2 originalCenter = middlegroundGear.center;
        std::vector<Gear> gears{backgroundDriver, middlegroundGear};
        std::vector<StoneBlock> blocks;
        Solve(gears, blocks);
        if (!NearlyEqual(gears[1].angularVelocity, 0.0f) ||
            !NearlyEqual(gears[1].center.x, originalCenter.x) ||
            !NearlyEqual(gears[1].center.y, originalCenter.y)) {
            std::cerr << "Physics bodies on different world layers collided or transferred torque.\n";
            return 1;
        }
        if (IsPlayerCollisionLayer(WorldLayer::Background) ||
            !IsPlayerCollisionLayer(WorldLayer::Middleground) ||
            IsPlayerCollisionLayer(WorldLayer::Foreground)) {
            std::cerr << "Player collision layer policy is incorrect.\n";
            return 1;
        }
    }

    {
        std::vector<Gear> gears{
            MakeGear(GearVisualType::Spur, GearMounting::Mounted, GearOrientation::Vertical,
                {0.0f, 0.0f}, 100.0f),
            MakeGear(GearVisualType::Spur, GearMounting::Dynamic, GearOrientation::Horizontal,
                {ContactDistance, 0.0f}, 0.0f)
        };
        std::vector<StoneBlock> blocks;
        Solve(gears, blocks);
        if (!NearlyEqual(gears[1].angularVelocity, 0.0f)) {
            std::cerr << "Perpendicular spur gears coupled without a bevel gear.\n";
            return 1;
        }

        gears[0] = MakeGear(GearVisualType::Bevel, GearMounting::Mounted, GearOrientation::Vertical,
            {0.0f, 0.0f}, 100.0f);
        gears[1].angularVelocity = 0.0f;
        Solve(gears, blocks);
        if (gears[1].angularVelocity >= -0.01f) {
            std::cerr << "Bevel gear did not transfer torque between perpendicular planes.\n";
            return 1;
        }
    }

    {
        Gear brake = MakeGear(GearVisualType::Spur, GearMounting::Mounted,
            GearOrientation::Vertical, {0.0f, 0.0f}, 0.0f);
        brake.stopped = true;
        std::vector<Gear> gears{
            brake,
            MakeGear(GearVisualType::Spur, GearMounting::Dynamic, GearOrientation::Vertical,
                {ContactDistance, 0.0f}, 90.0f)
        };
        std::vector<StoneBlock> blocks;
        Solve(gears, blocks);
        if (!NearlyEqual(gears[0].angularVelocity, 0.0f) || fabsf(gears[1].angularVelocity) >= 90.0f) {
            std::cerr << "A stopped mounted gear did not brake the touching gear.\n";
            return 1;
        }
    }

    {
        const Vector2 anchor{100.0f, 100.0f};
        std::vector<Gear> gears{
            MakeGear(GearVisualType::Count, GearMounting::Mounted,
                GearOrientation::Vertical, anchor, 0.0f)
        };
        std::vector<StoneBlock> blocks{{{120.0f, 90.0f, 20.0f, 20.0f}, {0.0f, 0.0f}, 2.0f, false}};
        Solve(gears, blocks);
        if (!NearlyEqual(gears[0].center.x, anchor.x) || !NearlyEqual(gears[0].center.y, anchor.y)) {
            std::cerr << "Mounted gear axle moved during a body collision.\n";
            return 1;
        }
        if (NearlyEqual(blocks[0].rect.x, 120.0f)) {
            std::cerr << "Mounted gear did not collide with a dynamic body.\n";
            return 1;
        }
    }

    std::cout << "All reusable gear physics checks passed.\n";
    return 0;
}
