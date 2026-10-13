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
    GuideObject coil = Parse("teslaCoil", "100 100 70 120 180 500 1.0 0.3 0.8 4 7");
    if (coil.type != GuideObjectType::TeslaCoil || !coil.collider.isTrigger ||
        coil.power.channel != 4 || coil.sensor.channel != 7 ||
        fabsf(coil.power.requiredPower - 0.75f) > 0.001f ||
        std::string(GetGuideObjectName(coil.type)) != "Tesla Coil") {
        std::cerr << "Tesla coil record did not preserve its authored configuration.\n";
        return 1;
    }

    std::vector<GuideObject> unpowered{coil, Parse("ball", "205 105 12 1")};
    Step(unpowered, nullptr, 1.1f);
    if (IsTeslaCoilDischarging(unpowered[0]) || fabsf(unpowered[1].body.velocity.x) > 0.001f) {
        std::cerr << "Unpowered Tesla coil discharged.\n";
        return 1;
    }

    std::vector<GuideObject> powered{
        Parse("battery", "20 20 40 24 1 4"),
        coil,
        Parse("ball", "205 105 12 1"),
        Parse("electricMotor", "360 120 20 90 7")
    };
    Step(powered, nullptr, 0.55f);
    if (IsTeslaCoilDischarging(powered[1]) || powered[3].power.powered) {
        std::cerr << "Tesla coil discharged or broadcast before it finished charging.\n";
        return 1;
    }
    Step(powered, nullptr, 0.5f);
    if (!IsTeslaCoilDischarging(powered[1]) || powered[2].body.velocity.x <= 0.0f ||
        !powered[3].power.powered) {
        std::cerr << "Tesla coil pulse did not jolt a body and broadcast wireless power.\n";
        return 1;
    }

    Player player{};
    player.rect = {210.0f, 105.0f, 22.0f, 32.0f};
    if (!IsGuideObjectHazardTouchingPlayer(powered[1], player.rect)) {
        std::cerr << "Active Tesla coil arc was not recognized as a player hazard.\n";
        return 1;
    }
    player.rect = {420.0f, 300.0f, 22.0f, 32.0f};
    if (IsGuideObjectHazardTouchingPlayer(powered[1], player.rect)) {
        std::cerr << "Tesla coil hazard extended beyond its visible arc radius.\n";
        return 1;
    }

    std::cout << "All Tesla coil physics checks passed.\n";
    return 0;
}
