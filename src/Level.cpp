#include "Level.h"

#include "Constants.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {
    LevelScript ParseLevelScript(const std::string& value) {
        if (value == "rotary_latch_lab") {
            return LevelScript::RotaryLatchLab;
        }
        if (value == "flooded_foundry") {
            return LevelScript::FloodedFoundry;
        }

        return LevelScript::PowerPulleyPanic;
    }
}

Level CreatePowerPulleyPanicLevel() {
    Level level;
    level.script = LevelScript::PowerPulleyPanic;

    level.ladder = {255, 250, 45, 400};
    level.spikeHazard = {300, 773, 960, 32};
    level.darknessArea = {300, 275, 960, 625};
    level.rightDarknessArea = {1260, 0, 340, 900};
    level.exitTrigger = {1485, 430, 85, 220};

    level.baseSolids = {
        {0, 650, 300, 40},
        {300, 250, 960, 25},
        {1238, 0, 22, 275},
        {1260, 650, 340, 40},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    level.pitPlatforms = {
        {420, 570, 70, 18},
        {590, 520, 70, 18},
        {760, 570, 70, 18},
        {930, 515, 70, 18},
        {1100, 570, 70, 18}
    };

    level.pulleys = {
        {650, 210},
        {820, 210},
        {990, 210},
        {1160, 210},
        {1430, 515}
    };

    level.weights = {
        {level.pulleys[1], 45.0f, 0.0f, 1.00f, {0, 0, 44, 55}},
        {level.pulleys[2], 45.0f, 1.6f, 1.25f, {0, 0, 44, 55}},
        {level.pulleys[3], 45.0f, 3.2f, 1.10f, {0, 0, 44, 55}}
    };

    return level;
}

Level CreateRotaryLatchLabLevel() {
    Level level;
    level.script = LevelScript::RotaryLatchLab;
    level.exitTrigger = {1485, 430, 85, 220};

    level.baseSolids = {
        {0, 650, 300, 40},
        {1260, 650, 340, 40},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    level.pitPlatforms = {
        {405, 570, 100, 18},
        {575, 520, 100, 18},
        {745, 570, 100, 18},
        {915, 515, 100, 18},
        {1085, 570, 100, 18}
    };

    level.rotaryLatches = {
        {{455, 512}, 38.0f, 35.0f, 270.0f, 8.0f, 180.0f},
        {{625, 462}, 38.0f, 120.0f, 270.0f, 8.0f, 150.0f},
        {{795, 512}, 38.0f, 215.0f, 270.0f, 8.0f, 195.0f},
        {{965, 457}, 38.0f, 305.0f, 270.0f, 8.0f, 165.0f},
        {{1135, 512}, 38.0f, 80.0f, 270.0f, 8.0f, 210.0f}
    };

    return level;
}

Level CreateFloodedFoundryLevel() {
    Level level;
    level.script = LevelScript::FloodedFoundry;

    level.ladder = {255, 250, 45, 400};
    level.spikeHazard = {300, 773, 960, 32};
    level.exitTrigger = {1485, 430, 85, 220};
    level.valve = {{515, 210}, 34.0f, 0.0f, 120.0f, false};
    level.waterPit = {{300, 650, 960, 155}, 805.0f, 646.0f, 86.0f, false};

    level.baseSolids = {
        {0, 650, 300, 40},
        {300, 250, 960, 25},
        {1238, 0, 22, 275},
        {1260, 650, 340, 40},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    return level;
}

Level LoadLevelFromFile(const std::string& path, Level fallback) {
    std::ifstream file(path);
    if (!file) {
        return fallback;
    }

    Level level;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string command;
        stream >> command;

        if (command.empty() || command[0] == '#') {
            continue;
        }

        if (command == "script") {
            std::string scriptName;
            stream >> scriptName;
            level.script = ParseLevelScript(scriptName);
        }
        else if (command == "ladder") {
            stream >> level.ladder.x >> level.ladder.y >> level.ladder.width >> level.ladder.height;
        }
        else if (command == "spikeHazard") {
            stream >> level.spikeHazard.x >> level.spikeHazard.y >> level.spikeHazard.width >> level.spikeHazard.height;
        }
        else if (command == "darkness") {
            stream >> level.darknessArea.x >> level.darknessArea.y >> level.darknessArea.width >> level.darknessArea.height;
        }
        else if (command == "rightDarkness") {
            stream >> level.rightDarknessArea.x >> level.rightDarknessArea.y >> level.rightDarknessArea.width >> level.rightDarknessArea.height;
        }
        else if (command == "exit") {
            stream >> level.exitTrigger.x >> level.exitTrigger.y >> level.exitTrigger.width >> level.exitTrigger.height;
        }
        else if (command == "valve") {
            stream >> level.valve.center.x >> level.valve.center.y >> level.valve.radius;
        }
        else if (command == "waterPit") {
            stream >> level.waterPit.bounds.x >> level.waterPit.bounds.y >> level.waterPit.bounds.width >> level.waterPit.bounds.height >>
                level.waterPit.surfaceY >> level.waterPit.targetSurfaceY >> level.waterPit.fillRate;
        }
        else if (command == "solid") {
            Rectangle rect{};
            stream >> rect.x >> rect.y >> rect.width >> rect.height;
            level.baseSolids.push_back(rect);
        }
        else if (command == "platform") {
            Rectangle rect{};
            stream >> rect.x >> rect.y >> rect.width >> rect.height;
            level.pitPlatforms.push_back(rect);
        }
        else if (command == "pulley") {
            Vector2 pulley{};
            stream >> pulley.x >> pulley.y;
            level.pulleys.push_back(pulley);
        }
        else if (command == "weight") {
            int pulleyIndex = 0;
            HangingWeight weight{};
            stream >> pulleyIndex >> weight.pulleyRadius >> weight.phase >> weight.speed >> weight.rect.width >> weight.rect.height;
            if (pulleyIndex >= 0 && pulleyIndex < static_cast<int>(level.pulleys.size())) {
                weight.pulley = level.pulleys[pulleyIndex];
                level.weights.push_back(weight);
            }
        }
        else if (command == "rotaryLatch") {
            RotaryLatch latch{};
            stream >> latch.center.x >> latch.center.y >> latch.radius >> latch.angle >> latch.targetAngle >> latch.tolerance >> latch.spinSpeed;
            level.rotaryLatches.push_back(latch);
        }
        else if (command == "stoneBlock") {
            StoneBlock block{};
            stream >> block.rect.x >> block.rect.y >> block.rect.width >> block.rect.height >> block.mass;
            level.stoneBlocks.push_back(block);
        }
        else if (command == "seeSaw") {
            SeeSaw seeSaw{};
            stream >> seeSaw.pivot.x >> seeSaw.pivot.y >> seeSaw.length >> seeSaw.thickness >> seeSaw.minAngle >> seeSaw.maxAngle >> seeSaw.response;
            level.seeSaws.push_back(seeSaw);
        }
        else if (command == "chain") {
            Chain chain{};
            stream >> chain.start.x >> chain.start.y >> chain.end.x >> chain.end.y >> chain.spacing >> chain.scale;
            level.chains.push_back(chain);
        }
        else if (command == "enemy") {
            Enemy enemy{};
            stream >> enemy.rect.x >> enemy.rect.y >> enemy.rect.width >> enemy.rect.height >> enemy.patrolMinX >> enemy.patrolMaxX >> enemy.speed;
            enemy.facingRight = enemy.speed > 0.0f;
            enemy.walking = true;
            level.enemies.push_back(enemy);
        }
    }

    return level;
}

std::vector<Rectangle> BuildSolids(const Level& level) {
    std::vector<Rectangle> solids = level.baseSolids;

    for (Rectangle platform : level.pitPlatforms) {
        solids.push_back(platform);
    }

    if (level.spikeHazard.width > 0.0f && level.spikeHazard.height > 0.0f) {
        float spikeBottom = level.spikeHazard.y + level.spikeHazard.height;
        solids.push_back({
            level.spikeHazard.x,
            spikeBottom,
            level.spikeHazard.width,
            static_cast<float>(Constants::ScreenHeight) - spikeBottom
        });
    }

    return solids;
}
