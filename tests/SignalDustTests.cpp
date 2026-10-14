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
        std::cerr << "Failed to parse signal object: " << command << '\n';
        std::exit(1);
    }
    return object;
}

void Step(std::vector<GuideObject>& objects) {
    std::array<Player*, 4> players{};
    std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
    Vector2 checkpoint{};
    UpdateGuideObjects(objects, players, solids, 0.0f, 1.0f / 60.0f, checkpoint);
}
}

int main() {
    std::vector<GuideObject> circuit{
        Parse("battery", "0 0 48 48 1 0"),
        Parse("signalDust", "45 24 109 24 4"),
        Parse("signalDust", "109 24 173 24 4"),
        Parse("signalDust", "300 24 364 24 5"),
        Parse("electricMotor", "220 24 20 90 4")
    };

    Step(circuit);
    if (circuit[1].signalMask != 15 || circuit[2].signalMask != 14) {
        std::cerr << "Connected signal dust did not propagate with one strength lost per segment.\n";
        return 1;
    }
    if (!circuit[1].power.powered || !circuit[2].power.powered ||
        circuit[3].power.powered || circuit[3].signalMask != 0) {
        std::cerr << "Signal dust powered state did not match physical connectivity.\n";
        return 1;
    }
    if (!circuit[4].power.powered || circuit[4].power.currentPower < 0.9f) {
        std::cerr << "Signal dust did not energize an existing channel-driven machine.\n";
        return 1;
    }
    if (std::string(GetGuideObjectName(circuit[1].type)) != "Signal Dust" ||
        std::string(GetGuideObjectDescription(circuit[1].type)).empty()) {
        std::cerr << "Signal dust metadata is missing.\n";
        return 1;
    }

    std::vector<GuideObject> ternary{
        Parse("ternarySwitch", "0 0 40 30 -1"),
        Parse("ternaryTrace", "38 15 100 15 4 5"),
        Parse("ternaryTrace", "100 15 162 15 4 5"),
        Parse("electricMotor", "210 15 20 90 4"),
        Parse("electricMotor", "260 15 20 90 5")
    };
    Step(ternary);
    if (ternary[1].signalMask != -1 || ternary[1].colorMask != 15 ||
        ternary[2].signalMask != -1 || ternary[2].colorMask != 14 ||
        !ternary[3].power.powered || ternary[4].power.powered) {
        std::cerr << "Negative ternary state did not propagate to its output channel.\n";
        return 1;
    }

    CycleTernarySwitch(ternary[0]);
    Step(ternary);
    if (ternary[0].signalMask != 0 || ternary[1].signalMask != 0 ||
        ternary[3].power.powered || ternary[4].power.powered) {
        std::cerr << "Zero ternary state did not deactivate both output channels.\n";
        return 1;
    }

    CycleTernarySwitch(ternary[0]);
    Step(ternary);
    if (ternary[0].signalMask != 1 || ternary[1].signalMask != 1 ||
        ternary[3].power.powered || !ternary[4].power.powered) {
        std::cerr << "Positive ternary state did not propagate to its output channel.\n";
        return 1;
    }
    if (std::string(GetGuideObjectName(ternary[1].type)) != "Ternary Trace" ||
        std::string(GetGuideObjectName(ternary[0].type)) != "Ternary Switch") {
        std::cerr << "Ternary circuit metadata is missing.\n";
        return 1;
    }

    std::cout << "All binary and ternary trace propagation checks passed.\n";
    return 0;
}
