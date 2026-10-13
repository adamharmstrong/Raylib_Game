#include "GuideObjects.h"
#include "Level.h"

#include <algorithm>
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
            std::cerr << "Failed to parse Leedskalnin device: " << command << '\n';
            std::exit(1);
        }
        return object;
    }

    void Step(std::vector<GuideObject>& objects, float dt = 1.0f / 60.0f) {
        std::array<Player*, 4> players{};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, 980.0f, dt, checkpoint);
    }
}

int main() {
    const std::array<std::pair<const char*, const char*>, 13> records{{
        {"tripodDerrick", "10 10 220 300"},
        {"chainHoist", "100 40 180 1.0 240"},
        {"blockAndTackle", "100 40 180 0.7 420"},
        {"comeAlong", "50 200 190 200 1.5 8"},
        {"manualBenchGrinder", "100 200 30 280"},
        {"loadCart", "100 200 70 36 3.0"},
        {"revolvingStoneGate", "100 100 60 220 0.3"},
        {"rockHammer", "100 200 60 24 2.4"},
        {"fieldCoil", "100 200 60 700"},
        {"spokePress", "100 100 70 40 0 1 90 1.2"},
        {"polarisTelescope", "100 200 160 -30 4"},
        {"leedskalninSundial", "100 200 45 4.0 5"},
        {"perpetualMotionHolder", "100 200 80 70 120 900 6"}
    }};
    for (const auto& record : records) {
        const GuideObject object = Parse(record.first, record.second);
        if (std::string(GetGuideObjectName(object.type)) == "Object" ||
            std::string(GetGuideObjectDescription(object.type)) == "An interactive object in the level.") {
            std::cerr << "Missing catalog metadata for " << record.first << ".\n";
            return 1;
        }
    }

    std::vector<GuideObject> hoistSystem{
        Parse("chainHoist", "100 40 180 1.0 240"),
        Parse("loadCart", "70 235 60 36 2.0")
    };
    Step(hoistSystem);
    if (hoistSystem[0].attachedObject != 1 || hoistSystem[1].body.velocity.y >= 0.0f) {
        std::cerr << "Chain hoist did not attach to and begin lifting its nearby load.\n";
        return 1;
    }

    std::vector<GuideObject> sundialSystem{
        Parse("leedskalninSundial", "100 100 40 2.0 5"),
        Parse("electricMotor", "200 100 20 120 5")
    };
    Step(sundialSystem, 0.1f);
    if (!sundialSystem[0].sensor.active || !sundialSystem[1].power.powered) {
        std::cerr << "Sundial did not energize its output channel during daylight phase.\n";
        return 1;
    }

    std::vector<GuideObject> holderSystem{
        Parse("battery", "10 10 30 20 1.0 6"),
        Parse("perpetualMotionHolder", "100 200 80 70 120 900 6"),
        Parse("ball", "140 190 12 1.0")
    };
    Step(holderSystem);
    holderSystem[0].active = false;
    Step(holderSystem);
    if (!holderSystem[1].triggered || holderSystem[2].body.velocity.y >= 20.0f) {
        std::cerr << "Perpetual motion holder did not retain its magnetic latch after the pulse ended.\n";
        return 1;
    }

    const Level workshop = LoadLevelFromFile(LEEDSKALNIN_WORKSHOP_PATH, {});
    int deviceCount = 0;
    for (const GuideObject& object : workshop.guideObjects) {
        if (object.type >= GuideObjectType::TripodDerrick &&
            object.type <= GuideObjectType::PerpetualMotionHolder) ++deviceCount;
    }
    if (deviceCount < 13) {
        std::cerr << "Leedskalnin Workshop is missing catalog devices.\n";
        return 1;
    }

    const bool hasSecondFloorLadder = std::any_of(workshop.ladders.begin(), workshop.ladders.end(),
        [](Rectangle ladder) {
            return ladder.x <= 900.0f && ladder.x + ladder.width >= 900.0f &&
                ladder.y <= 470.0f && ladder.y + ladder.height >= 850.0f;
        });
    const Rectangle spawnBounds{45.0f, 805.0f, 24.0f, 32.0f};
    const auto grinder = std::find_if(workshop.guideObjects.begin(), workshop.guideObjects.end(),
        [](const GuideObject& object) { return object.type == GuideObjectType::ManualBenchGrinder; });
    if (!hasSecondFloorLadder || grinder == workshop.guideObjects.end() ||
        grinder->transform.position.y >= 470.0f || IsGuideObjectHazardTouchingPlayer(*grinder, spawnBounds)) {
        std::cerr << "Workshop ladder or safe grinder placement is missing.\n";
        return 1;
    }

    std::cout << "All Leedskalnin device checks passed.\n";
    return 0;
}
