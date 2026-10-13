#include "Fluid.h"
#include "GuideObjects.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
GuideObject Parse(const char* command, const char* arguments) {
    GuideObject object{};
    std::istringstream stream(arguments);
    if (!ParseGuideObject(command, stream, object)) {
        std::cerr << "Failed to parse thermal object: " << command << '\n';
        std::exit(1);
    }
    return object;
}

void Step(std::vector<GuideObject>& objects, std::vector<FluidField>* fluids = nullptr,
    FluidSimulationMode mode = FluidSimulationMode::Advanced, float dt = 1.0f) {
    std::array<Player*, 4> players{};
    std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
    Vector2 checkpoint{};
    UpdateGuideObjects(objects, players, solids, 0.0f, dt, checkpoint, fluids, mode);
}
}

int main() {
    const std::array<std::pair<const char*, const char*>, 6> definitions{{
        {"iceBlock", "180 100 48 48 40"},
        {"iceSheet", "180 100 120 12 20"},
        {"heater", "100 100 44 44 1 0 180 48 100 0"},
        {"campfire", "100 100 20 140 80"},
        {"torch", "100 100 12 46 0.8 80 45"},
        {"waterBucket", "100 100 38 42 1.2 8 8 180"}
    }};
    for (const auto& definition : definitions) {
        const GuideObject object = Parse(definition.first, definition.second);
        if (std::string(GetGuideObjectName(object.type)) == "Object" ||
            std::string(GetGuideObjectDescription(object.type)) == "An interactive object in the level.") {
            std::cerr << "Missing metadata for " << definition.first << ".\n";
            return 1;
        }
    }

    std::vector<GuideObject> directedHeat{
        Parse("heater", "100 100 44 44 1 0 180 48 100 0"),
        Parse("iceBlock", "190 100 40 40 35"),
        Parse("iceBlock", "20 100 40 40 35")
    };
    std::vector<FluidField> meltWater;
    Step(directedHeat, &meltWater);
    if (!directedHeat[1].broken || directedHeat[2].broken || meltWater.empty() ||
        GetFluidMass(meltWater.front()) <= 0.0f) {
        std::cerr << "Directional heater did not selectively melt ice into water.\n";
        return 1;
    }

    std::vector<GuideObject> radialHeat{
        Parse("campfire", "100 100 20 140 100"),
        Parse("iceSheet", "145 100 60 12 20")
    };
    Step(radialHeat);
    if (!radialHeat[1].broken) {
        std::cerr << "Campfire did not radiate enough heat to melt nearby ice.\n";
        return 1;
    }

    std::vector<GuideObject> uprightBucket{Parse("waterBucket", "100 100 38 42 1.2 8 8 0")};
    std::vector<FluidField> uprightWater;
    Step(uprightBucket, &uprightWater);
    if (!uprightWater.empty() || uprightBucket[0].capacity < 7.99f) {
        std::cerr << "Upright bucket released water.\n";
        return 1;
    }

    std::vector<GuideObject> tippedBucket{Parse("waterBucket", "100 100 38 42 1.2 8 8 180")};
    std::vector<FluidField> pouredWater;
    Step(tippedBucket, &pouredWater);
    if (pouredWater.empty() || GetFluidMass(pouredWater.front()) <= 0.0f || tippedBucket[0].capacity >= 8.0f) {
        std::cerr << "Inverted bucket did not pour particle water.\n";
        return 1;
    }

    FluidField cellular{};
    cellular.type = FluidType::Water;
    cellular.bounds = {0, 0, 160, 160};
    cellular.initialFill = 0.0f;
    InitializeFluidField(cellular, {}, FluidSimulationMode::Tile);
    if (EmitCellularFluidMass(cellular, {80, 32}, 3.0f, {20, 40}) < 2.99f || GetFluidMass(cellular) < 2.99f) {
        std::cerr << "Localized bucket emission failed in simple fluid mode.\n";
        return 1;
    }

    std::cout << "All thermal object and bucket-water checks passed.\n";
    return 0;
}
