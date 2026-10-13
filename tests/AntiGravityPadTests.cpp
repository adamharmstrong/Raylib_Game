#include "GuideObjects.h"
#include "Level.h"

#include <array>
#include <cmath>
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
            std::cerr << "Failed to parse guide object: " << command << '\n';
            std::exit(1);
        }
        return object;
    }

    void Step(std::vector<GuideObject>& objects, Player* player, float gravity, float dt) {
        std::array<Player*, 4> players{player, nullptr, nullptr, nullptr};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, gravity, dt, checkpoint);
    }
}

int main() {
    GuideObject pad = Parse("antiGravityPad", "100 300 80 16 180 1800");
    const Rectangle field = GetAntiGravityFieldBounds(pad);
    if (pad.type != GuideObjectType::AntiGravityPad || pad.power.channel != 0 ||
        fabsf(field.x - 100.0f) > 0.001f || fabsf(field.y - 120.0f) > 0.001f ||
        fabsf(field.width - 80.0f) > 0.001f || fabsf(field.height - 180.0f) > 0.001f) {
        std::cerr << "Anti-gravity pad record did not preserve its lift field dimensions.\n";
        return 1;
    }

    Player player{};
    player.rect = {124.0f, 260.0f, 24.0f, 32.0f};
    player.velocity = {0.0f, 120.0f};
    std::vector<GuideObject> playerSystem{pad};
    Step(playerSystem, &player, 980.0f, 1.0f / 60.0f);
    if (player.velocity.y >= 120.0f || player.onGround) {
        std::cerr << "Active anti-gravity field did not lift the player.\n";
        return 1;
    }

    std::vector<GuideObject> bodySystem{
        pad,
        Parse("ball", "140 265 10 1.0")
    };
    Step(bodySystem, nullptr, 980.0f, 1.0f / 60.0f);
    Step(bodySystem, nullptr, 980.0f, 1.0f / 60.0f);
    if (bodySystem[1].body.velocity.y >= 0.0f) {
        std::cerr << "Active anti-gravity field did not overcome gravity for a movable body.\n";
        return 1;
    }

    std::vector<GuideObject> unpoweredSystem{
        Parse("antiGravityPad", "100 300 80 16 180 1800 9"),
        Parse("ball", "140 265 10 1.0")
    };
    Step(unpoweredSystem, nullptr, 980.0f, 1.0f / 60.0f);
    if (unpoweredSystem[1].body.velocity.y <= 0.0f || IsAntiGravityPadActive(unpoweredSystem[0])) {
        std::cerr << "Unpowered anti-gravity pad still applied lift.\n";
        return 1;
    }

    std::vector<GuideObject> poweredSystem{
        Parse("battery", "20 20 40 24 1.0 9"),
        Parse("antiGravityPad", "100 300 80 16 180 1800 9"),
        Parse("ball", "140 265 10 1.0")
    };
    Step(poweredSystem, nullptr, 980.0f, 1.0f / 60.0f);
    Step(poweredSystem, nullptr, 980.0f, 1.0f / 60.0f);
    if (!IsAntiGravityPadActive(poweredSystem[1]) || poweredSystem[2].body.velocity.y >= 0.0f) {
        std::cerr << "Powered anti-gravity pad did not lift its movable body.\n";
        return 1;
    }

    const Level testLevel = LoadLevelFromFile(TEST_LEVEL_PATH, {});
    int padCount = 0;
    for (const GuideObject& object : testLevel.guideObjects) {
        if (object.type == GuideObjectType::AntiGravityPad) ++padCount;
    }
    if (padCount != 1) {
        std::cerr << "Test Level did not load exactly one anti-gravity pad.\n";
        return 1;
    }

    std::cout << "All anti-gravity pad physics checks passed.\n";
    return 0;
}
