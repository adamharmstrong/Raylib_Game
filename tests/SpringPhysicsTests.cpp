#include "GuideObjects.h"
#include "Level.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    GuideObject Parse(const std::string& command, const std::string& arguments) {
        GuideObject object{};
        std::istringstream stream(arguments);
        if (!ParseGuideObject(command, stream, object)) {
            std::cerr << "Failed to parse spring record: " << command << '\n';
            std::exit(1);
        }
        return object;
    }

    void Step(std::vector<GuideObject>& objects, float dt = 0.1f) {
        std::array<Player*, 4> players{};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, 0.0f, dt, checkpoint);
    }
}

int main() {
    const std::array<const char*, 21> commands{
        "compressionSpring", "extensionSpring", "torsionSpring", "garterSpring",
        "voluteSpring", "spiralSpring", "constantForceSpring", "constantTorqueSpring",
        "leafSpring", "beamSpring", "discSpring", "waveSpring", "waveWasher",
        "torsionBar", "ringSpring", "elastomerSpring", "pneumaticSpring",
        "gasSpring", "hydropneumaticSpring", "magneticSpring", "compositeSpring"
    };

    for (const char* command : commands) {
        GuideObject spring = Parse(command, "0 0 100 0 7 12 2");
        if (spring.constraint.kind != ConstraintKind::Spring ||
            spring.constraint.restLength < 99.9f || spring.constraint.restLength > 100.1f ||
            spring.constraint.stiffness != 12.0f || spring.constraint.damping != 2.0f ||
            std::string(GetGuideObjectName(spring.type)) == "Object") {
            std::cerr << "Spring family was not fully configured: " << command << '\n';
            return 1;
        }
        Rectangle bounds = GetGuideObjectBounds(spring);
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
            std::cerr << "Spring family has no hover/render bounds: " << command << '\n';
            return 1;
        }
    }

    {
        Level springLab = LoadLevelFromFile(SPRING_LEVEL_PATH, {});
        int springCount = 0;
        for (const GuideObject& object : springLab.guideObjects) {
            if (object.constraint.kind == ConstraintKind::Spring) ++springCount;
        }
        if (springCount != static_cast<int>(commands.size())) {
            std::cerr << "Spring Laboratory did not load all 21 specialized spring families.\n";
            return 1;
        }
    }

    {
        Level gallery = LoadLevelFromFile(GEAR_GALLERY_LEVEL_PATH, {});
        int springCount = 0;
        for (const GuideObject& object : gallery.guideObjects) {
            if (object.constraint.kind == ConstraintKind::Spring) ++springCount;
        }
        if (springCount != static_cast<int>(commands.size()) + 1) {
            std::cerr << "Gear gallery spring room did not load all 22 spring objects.\n";
            return 1;
        }
        if (gallery.cameraZones.size() != 2 || gallery.worldBounds.width != 3200.0f ||
            gallery.cameraZones[0].width != 1600.0f || gallery.cameraZones[1].x != 1600.0f) {
            std::cerr << "Gear gallery is not divided into two screen-sized camera rooms.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("compressionSpring", "0 0 100 0 7"),
            Parse("ball", "80 0 10 1")
        };
        Step(objects);
        if (objects[1].body.velocity.x <= 0.0f) {
            std::cerr << "Compression spring did not push a compressed body outward.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("extensionSpring", "0 0 100 0 7"),
            Parse("ball", "120 0 10 1")
        };
        Step(objects);
        if (objects[1].body.velocity.x >= 0.0f) {
            std::cerr << "Extension spring did not pull an extended body inward.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("constantForceSpring", "0 0 100 0 7"),
            Parse("ball", "120 0 10 1")
        };
        Step(objects);
        if (objects[1].body.velocity.x >= 0.0f) {
            std::cerr << "Constant-force spring did not retract its strip.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("leafSpring", "0 0 100 0 7"),
            Parse("ball", "100 20 10 1")
        };
        Step(objects);
        if (objects[1].body.velocity.y >= 0.0f) {
            std::cerr << "Leaf spring did not resist transverse deflection.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("magneticSpring", "0 0 100 0 7"),
            Parse("ball", "120 0 10 1")
        };
        Step(objects);
        if (objects[1].body.velocity.x >= 0.0f) {
            std::cerr << "Magnetic spring did not restore an offset body without contact.\n";
            return 1;
        }
    }

    {
        std::vector<GuideObject> objects{
            Parse("torsionSpring", "0 0 100 0 7"),
            Parse("ball", "100 0 10 1")
        };
        Step(objects);
        objects[1].transform.rotation += 35.0f;
        Step(objects);
        Step(objects);
        if (objects[1].body.angularVelocity >= 0.0f) {
            std::cerr << "Torsion spring did not apply restoring torque.\n";
            return 1;
        }
    }

    std::cout << "All reusable spring physics checks passed.\n";
    return 0;
}
