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

    void Step(std::vector<GuideObject>& objects, Player* player, float dt) {
        std::array<Player*, 4> players{player, nullptr, nullptr, nullptr};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, 0.0f, dt, checkpoint);
    }
}

int main() {
    GuideObject projector = Parse("teleforce", "100 100 80 50 1 0 300 20 1200 1.0 0.25 0.75 4");
    if (projector.type != GuideObjectType::Teleforce || !projector.collider.isTrigger ||
        projector.power.channel != 4 || fabsf(projector.power.requiredPower - 0.9f) > 0.001f ||
        std::string(GetGuideObjectName(projector.type)) != "Teleforce Projector") {
        std::cerr << "Teleforce record did not preserve its authored configuration.\n";
        return 1;
    }

    std::vector<GuideObject> unpowered{projector, Parse("ball", "260 115 12 1")};
    Step(unpowered, nullptr, 1.1f);
    if (IsTeleforceFiring(unpowered[0]) || fabsf(unpowered[1].body.velocity.x) > 0.001f) {
        std::cerr << "Unpowered Teleforce discharged.\n";
        return 1;
    }

    std::vector<GuideObject> powered{
        Parse("battery", "20 20 40 24 1 4"),
        projector,
        Parse("ball", "260 115 12 1")
    };
    Step(powered, nullptr, 0.5f);
    if (IsTeleforceFiring(powered[1]) || powered[2].body.velocity.x > 0.001f) {
        std::cerr << "Teleforce fired before its charge interval elapsed.\n";
        return 1;
    }
    Step(powered, nullptr, 0.55f);
    if (!IsTeleforceFiring(powered[1]) || powered[2].body.velocity.x <= 100.0f) {
        std::cerr << "Charged Teleforce did not launch a body in its firing corridor.\n";
        return 1;
    }

    Player player{};
    player.rect = {260.0f, 112.0f, 22.0f, 32.0f};
    if (!IsGuideObjectHazardTouchingPlayer(powered[1], player.rect)) {
        std::cerr << "Active Teleforce beam was not recognized as a player hazard.\n";
        return 1;
    }
    player.rect.y = 180.0f;
    if (IsGuideObjectHazardTouchingPlayer(powered[1], player.rect)) {
        std::cerr << "Teleforce hazard extended outside its visible beam width.\n";
        return 1;
    }

    std::cout << "All Teleforce physics checks passed.\n";
    return 0;
}
