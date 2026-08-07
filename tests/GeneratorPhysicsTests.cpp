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

    void Step(std::vector<GuideObject>& objects, float dt = 0.1f) {
        std::array<Player*, 4> players{};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, 0.0f, dt, checkpoint);
    }

    int CountGenerators(const Level& level) {
        int count = 0;
        for (const GuideObject& object : level.guideObjects) {
            if (object.type == GuideObjectType::Generator) ++count;
        }
        return count;
    }
}

int main() {
    GuideObject generator = Parse("generator", "0 0 90 70 220 1.0 0.92 7");
    if (generator.type != GuideObjectType::Generator || generator.power.channel != 7 ||
        generator.ratedMechanicalSpeed != 220.0f || generator.power.maximumPower != 1.0f ||
        fabsf(generator.generatorEfficiency - 0.92f) > 0.001f) {
        std::cerr << "Generator record did not preserve its mechanical and electrical ratings.\n";
        return 1;
    }

    std::vector<GuideObject> system{
        generator,
        Parse("electricMotor", "140 35 24 196 7")
    };
    system[0].mechanicalInputSpeed = 110.0f;
    Step(system);
    if (fabsf(system[0].power.currentPower - 0.46f) > 0.001f || !system[1].power.powered) {
        std::cerr << "Half-speed generator output did not power a compatible motor.\n";
        return 1;
    }
    if (system[0].mechanicalLoad <= 0.20f || system[0].transform.rotation <= 0.0f ||
        system[1].transform.rotation <= 0.0f) {
        std::cerr << "Generator demand did not create shaft load and visible rotation.\n";
        return 1;
    }

    system[0].mechanicalInputSpeed = 20.0f;
    Step(system);
    if (system[1].power.powered) {
        std::cerr << "Motor remained powered below the generator's required output voltage.\n";
        return 1;
    }

    system[0].mechanicalInputSpeed = -220.0f;
    Step(system);
    if (fabsf(system[0].power.currentPower - 0.92f) > 0.001f || !system[1].power.powered) {
        std::cerr << "Reverse shaft rotation did not produce rated generator output.\n";
        return 1;
    }

    const Level completedGatehouse = LoadLevelFromFile(COMPLETED_GATEHOUSE_PATH, {});
    const Level generatorTest = LoadLevelFromFile(GENERATOR_TEST_LEVEL_PATH, {});
    if (CountGenerators(completedGatehouse) != 0) {
        std::cerr << "Completed Gatehouse was modified to contain the test generator.\n";
        return 1;
    }
    if (CountGenerators(generatorTest) != 1) {
        std::cerr << "Alternate Gatehouse did not load exactly one generator.\n";
        return 1;
    }

    std::cout << "All reusable generator physics checks passed.\n";
    return 0;
}
