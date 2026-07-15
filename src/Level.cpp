#include "Level.h"

#include "Constants.h"

#include <algorithm>
#include <fstream>
#include <cmath>
#include <sstream>
#include <string>

namespace {
    constexpr float VisualTileSize = 32.0f;
    constexpr float PitWallSize = 32.0f;
    constexpr float PitTopY = 682.0f;

    LevelScript ParseLevelScript(const std::string& value) {
        if (value == "rotary_latch_lab") {
            return LevelScript::RotaryLatchLab;
        }
        if (value == "flooded_foundry") {
            return LevelScript::FloodedFoundry;
        }
        if (value == "counterweight_row") {
            return LevelScript::CounterweightRow;
        }
        if (value == "tileset_reference") {
            return LevelScript::TilesetReference;
        }

        return LevelScript::PowerPulleyPanic;
    }

    TileLayer ParseTileLayer(const std::string& value) {
        if (value == "farBackground" || value == "far_background" || value == "far-background") {
            return TileLayer::FarBackground;
        }
        if (value == "background") {
            return TileLayer::Background;
        }

        return TileLayer::Foreground;
    }

    bool IsCollidableVisualTile(const VisualTile& tile) {
        bool isVoidTile = tile.column == 1 && tile.row == 2;
        return tile.layer == TileLayer::Foreground && !isVoidTile;
    }
}

Level CreatePowerPulleyPanicLevel() {
    Level level;
    level.script = LevelScript::PowerPulleyPanic;

    level.ladder = {255, 250, 45, 400};
    level.spikeHazard = {300, 773, 960, 32};
    level.darknessAreas = {
        {300, 275, 960, 625},
        {1260, 0, 340, 900}
    };
    level.exitTrigger = {1440, 430, 85, 220};

    level.baseSolids = {
        {0, 0, 1600, 32},
        {0, 32, 32, 618},
        {1568, 32, 32, 618},
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
        {1385, 515}
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
    level.playerStart = {80.0f, 600.0f};
    level.exitTrigger = {1440, 430, 85, 210};

    level.baseSolids = {
        {0, 0, 1600, 32},
        {0, 32, 32, 608},
        {1568, 32, 32, 608},
        {0, 640, 300, 50},
        {1260, 640, 340, 50},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    level.pitPlatforms = {
        {407, 570, 96, 32},
        {577, 520, 96, 32},
        {747, 570, 96, 32},
        {917, 515, 96, 32},
        {1087, 570, 96, 32}
    };

    level.rotaryLatches = {
        {{455, 512}, 38.0f, 35.0f, 270.0f, 8.0f, 120.0f},
        {{625, 462}, 38.0f, 120.0f, 270.0f, 8.0f, 100.0f},
        {{795, 512}, 38.0f, 215.0f, 270.0f, 8.0f, 130.0f},
        {{965, 457}, 38.0f, 305.0f, 270.0f, 8.0f, 110.0f},
        {{1135, 512}, 38.0f, 80.0f, 270.0f, 8.0f, 140.0f}
    };

    return level;
}

Level CreateFloodedFoundryLevel() {
    Level level;
    level.script = LevelScript::FloodedFoundry;
    level.playerStart = {80.0f, 600.0f};

    level.ladder = {255, 250, 45, 390};
    level.spikeHazard = {320, 773, 928, 32};
    level.exitTrigger = {1440, 430, 85, 210};
    level.valve = {{515, 210}, 34.0f, 0.0f, 120.0f, false};
    FluidField water{};
    water.type = FluidType::Water;
    water.bounds = {320, 646, 928, 159};
    water.particleSpacing = 1.0f;
    water.initialFill = 0.0f;
    water.flowSpeed = 1.0f;
    level.fluids.push_back(water);
    level.valveFluidFill = {0, 1.0f, 86.0f};

    level.baseSolids = {
        {0, 0, 1600, 32},
        {0, 32, 32, 608},
        {1568, 32, 32, 608},
        {0, 640, 320, 50},
        {300, 250, 960, 32},
        {1216, 0, 32, 256},
        {1248, 640, 352, 50},
        {0, 690, 320, 210},
        {1248, 690, 352, 210}
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
        else if (command == "playerStart") {
            stream >> level.playerStart.x >> level.playerStart.y;
        }
        else if (command == "ladder") {
            stream >> level.ladder.x >> level.ladder.y >> level.ladder.width >> level.ladder.height;
        }
        else if (command == "spikeHazard") {
            stream >> level.spikeHazard.x >> level.spikeHazard.y >> level.spikeHazard.width >> level.spikeHazard.height;
        }
        else if (command == "darkness") {
            Rectangle darknessArea{};
            stream >> darknessArea.x >> darknessArea.y >> darknessArea.width >> darknessArea.height;
            level.darknessAreas.push_back(darknessArea);
        }
        else if (command == "visualTile") {
            std::string layer;
            VisualTile tile{};
            stream >> layer >> tile.column >> tile.row >> tile.position.x >> tile.position.y;
            tile.layer = ParseTileLayer(layer);
            level.visualTiles.push_back(tile);
        }
        else if (command == "visualTileRect") {
            std::string layer;
            VisualTile tile{};
            int columns = 0;
            int rows = 0;
            stream >> layer >> tile.column >> tile.row >> tile.position.x >> tile.position.y >> columns >> rows;
            tile.layer = ParseTileLayer(layer);

            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    VisualTile expandedTile = tile;
                    expandedTile.position.x += static_cast<float>(column) * VisualTileSize;
                    expandedTile.position.y += static_cast<float>(row) * VisualTileSize;
                    level.visualTiles.push_back(expandedTile);
                }
            }
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
        else if (command == "valveFluidFill") {
            stream >> level.valveFluidFill.fluidIndex >> level.valveFluidFill.targetFill >> level.valveFluidFill.riseRate;
            level.valveFluidFill.targetFill = std::clamp(level.valveFluidFill.targetFill, 0.0f, 1.0f);
            level.valveFluidFill.riseRate = fmaxf(0.0f, level.valveFluidFill.riseRate);
        }
        else if (command == "water" || command == "sand" || command == "gel" || command == "gas") {
            FluidField fluid{};
            fluid.type = command == "water" ? FluidType::Water :
                (command == "sand" ? FluidType::Sand :
                (command == "gel" ? FluidType::Gel : FluidType::Gas));
            stream >> fluid.bounds.x >> fluid.bounds.y >> fluid.bounds.width >> fluid.bounds.height >>
                fluid.particleSpacing >> fluid.initialFill >> fluid.flowSpeed;
            fluid.bounds.width = fmaxf(1.0f, fluid.bounds.width);
            fluid.bounds.height = fmaxf(1.0f, fluid.bounds.height);
            fluid.particleSpacing = (fluid.type == FluidType::Water || fluid.type == FluidType::Sand) ?
                std::clamp(fluid.particleSpacing, 1.0f, 12.0f) :
                std::clamp(fluid.particleSpacing, 6.0f, 48.0f);
            fluid.initialFill = std::clamp(fluid.initialFill, 0.0f, 1.0f);
            fluid.flowSpeed = std::clamp(fluid.flowSpeed, 0.1f, 4.0f);
            level.fluids.push_back(fluid);
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
            block.rect.width = fmaxf(1.0f, block.rect.width);
            block.rect.height = fmaxf(1.0f, block.rect.height);
            block.mass = fmaxf(0.1f, block.mass);
            level.stoneBlocks.push_back(block);
        }
        else if (command == "boulder") {
            Boulder boulder{};
            stream >> boulder.center.x >> boulder.center.y >> boulder.radius >> boulder.mass;
            boulder.radius = fmaxf(1.0f, boulder.radius);
            boulder.mass = fmaxf(0.1f, boulder.mass);
            level.boulders.push_back(boulder);
        }
        else if (command == "physicsWheel") {
            PhysicsWheel wheel{};
            stream >> wheel.center.x >> wheel.center.y >> wheel.radius >> wheel.mass;
            wheel.radius = fmaxf(1.0f, wheel.radius);
            wheel.mass = fmaxf(0.1f, wheel.mass);
            level.physicsWheels.push_back(wheel);
        }
        else if (command == "gear") {
            Gear gear{};
            stream >> gear.center.x >> gear.center.y >> gear.radius >> gear.mass;
            gear.radius = fmaxf(1.0f, gear.radius);
            gear.mass = fmaxf(0.1f, gear.mass);
            level.gears.push_back(gear);
        }
        else if (command == "flywheel") {
            Flywheel flywheel{};
            stream >> flywheel.center.x >> flywheel.center.y >> flywheel.radius >> flywheel.mass >> flywheel.angularVelocity;
            flywheel.radius = fmaxf(1.0f, flywheel.radius);
            flywheel.mass = fmaxf(0.1f, flywheel.mass);
            level.flywheels.push_back(flywheel);
        }
        else if (command == "steeringWheel") {
            SteeringWheel steeringWheel{};
            stream >> steeringWheel.center.x >> steeringWheel.center.y >> steeringWheel.radius >> steeringWheel.rotation;
            steeringWheel.radius = fmaxf(1.0f, steeringWheel.radius);
            level.steeringWheels.push_back(steeringWheel);
        }
        else if (command == "screw") {
            Screw screw{};
            stream >> screw.center.x >> screw.center.y >> screw.length >> screw.radius >> screw.angle >> screw.spinSpeed;
            screw.length = fmaxf(1.0f, screw.length);
            screw.radius = fmaxf(1.0f, screw.radius);
            level.screws.push_back(screw);
        }
        else if (command == "fan") {
            Fan fan{};
            stream >> fan.center.x >> fan.center.y >> fan.direction.x >> fan.direction.y >> fan.length >> fan.width >> fan.strength >> fan.power;
            float length = sqrtf(fan.direction.x * fan.direction.x + fan.direction.y * fan.direction.y);
            fan.direction = length > 0.0001f ? Vector2{fan.direction.x / length, fan.direction.y / length} : Vector2{1.0f, 0.0f};
            fan.length = fmaxf(1.0f, fan.length);
            fan.width = fmaxf(1.0f, fan.width);
            fan.strength = fmaxf(0.0f, fan.strength);
            fan.power = std::clamp(fan.power, 0.0f, 1.0f);
            level.fans.push_back(fan);
        }
        else if (command == "pinwheel") {
            Pinwheel pinwheel{};
            stream >> pinwheel.center.x >> pinwheel.center.y >> pinwheel.radius;
            pinwheel.radius = fmaxf(1.0f, pinwheel.radius);
            level.pinwheels.push_back(pinwheel);
        }
        else if (command == "seeSaw") {
            SeeSaw seeSaw{};
            stream >> seeSaw.pivot.x >> seeSaw.pivot.y >> seeSaw.length >> seeSaw.thickness >> seeSaw.minAngle >> seeSaw.maxAngle >> seeSaw.response;
            seeSaw.length = fmaxf(1.0f, seeSaw.length);
            seeSaw.thickness = fmaxf(1.0f, seeSaw.thickness);
            seeSaw.minAngle = std::clamp(seeSaw.minAngle, -80.0f, 80.0f);
            seeSaw.maxAngle = std::clamp(seeSaw.maxAngle, -80.0f, 80.0f);
            if (seeSaw.minAngle > seeSaw.maxAngle) {
                std::swap(seeSaw.minAngle, seeSaw.maxAngle);
            }
            seeSaw.angle = std::clamp(seeSaw.angle, seeSaw.minAngle, seeSaw.maxAngle);
            seeSaw.response = fmaxf(0.1f, seeSaw.response);
            level.seeSaws.push_back(seeSaw);
        }
        else if (command == "ramp") {
            Ramp ramp{};
            stream >> ramp.center.x >> ramp.center.y >> ramp.length >> ramp.thickness >> ramp.angle;
            ramp.length = fmaxf(1.0f, ramp.length);
            ramp.thickness = fmaxf(1.0f, ramp.thickness);
            ramp.angle = std::clamp(ramp.angle, -80.0f, 80.0f);
            level.ramps.push_back(ramp);
        }
        else if (command == "trapDoor") {
            TrapDoor trapDoor{};
            stream >> trapDoor.hinge.x >> trapDoor.hinge.y >> trapDoor.length >> trapDoor.thickness >> trapDoor.angle;
            trapDoor.length = fmaxf(1.0f, trapDoor.length);
            trapDoor.thickness = fmaxf(1.0f, trapDoor.thickness);
            trapDoor.angle = std::clamp(trapDoor.angle, -80.0f, 80.0f);
            level.trapDoors.push_back(trapDoor);
        }
        else if (command == "chain") {
            Chain chain{};
            int pinStart = 1;
            int pinEnd = 1;
            stream >> chain.start.x >> chain.start.y >> chain.end.x >> chain.end.y >> chain.spacing >> chain.scale;
            if (stream >> pinStart >> pinEnd) {
                chain.pinStart = pinStart != 0;
                chain.pinEnd = pinEnd != 0;
            }
            chain.spacing = fmaxf(1.0f, chain.spacing);
            chain.scale = fmaxf(0.1f, chain.scale);
            level.chains.push_back(chain);
        }
        else if (command == "physicsRope") {
            PhysicsRope rope{};
            int pinStart = 1;
            int pinEnd = 1;
            stream >> rope.start.x >> rope.start.y >> rope.end.x >> rope.end.y >>
                rope.length >> rope.thickness >> pinStart >> pinEnd;
            rope.length = fmaxf(1.0f, rope.length);
            rope.thickness = fmaxf(1.0f, rope.thickness);
            rope.pinStart = pinStart != 0;
            rope.pinEnd = pinEnd != 0;
            if (rope.pinStart && rope.pinEnd) {
                float dx = rope.end.x - rope.start.x;
                float dy = rope.end.y - rope.start.y;
                rope.length = fmaxf(rope.length, sqrtf(dx * dx + dy * dy));
            }
            level.physicsRopes.push_back(rope);
        }
        else if (command == "button") {
            Button button{};
            stream >> button.rect.x >> button.rect.y >> button.rect.width >> button.rect.height;
            button.rect.width = fmaxf(1.0f, button.rect.width);
            button.rect.height = fmaxf(1.0f, button.rect.height);
            level.buttons.push_back(button);
        }
        else if (command == "arrowTrap") {
            ArrowTrap trap{};
            stream >> trap.position.x >> trap.position.y >> trap.direction.x >> trap.direction.y >> trap.interval >> trap.speed;
            if (fabsf(trap.direction.x) > fabsf(trap.direction.y)) {
                trap.direction = {trap.direction.x < 0.0f ? -1.0f : 1.0f, 0.0f};
            }
            else {
                trap.direction = {0.0f, trap.direction.y < 0.0f ? -1.0f : 1.0f};
            }
            trap.interval = fmaxf(0.08f, trap.interval);
            trap.speed = fabsf(trap.speed);
            level.arrowTraps.push_back(trap);
        }
        else if (command == "breakableTile") {
            BreakableTile tile{};
            stream >> tile.rect.x >> tile.rect.y >> tile.rect.width >> tile.rect.height >> tile.breakDelay;
            tile.rect.width = fmaxf(1.0f, tile.rect.width);
            tile.rect.height = fmaxf(1.0f, tile.rect.height);
            tile.breakDelay = fmaxf(0.05f, tile.breakDelay);
            level.breakableTiles.push_back(tile);
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
    std::vector<Rectangle> solids;

    if (level.script == LevelScript::TilesetReference && !level.visualTiles.empty()) {
        for (const VisualTile& tile : level.visualTiles) {
            if (IsCollidableVisualTile(tile)) {
                solids.push_back({tile.position.x, tile.position.y, VisualTileSize, VisualTileSize});
            }
        }
    }
    else {
        solids = level.baseSolids;

        for (Rectangle platform : level.pitPlatforms) {
            solids.push_back(platform);
        }
    }

    if (level.spikeHazard.width > 0.0f && level.spikeHazard.height > 0.0f) {
        float spikeBottom = level.spikeHazard.y + level.spikeHazard.height;
        solids.push_back({
            level.spikeHazard.x,
            spikeBottom,
            level.spikeHazard.width,
            static_cast<float>(Constants::ScreenHeight) - spikeBottom
        });
        // DrawTilesetPitWalls renders these two masonry strips immediately
        // outside the hazard bounds. Keep physics on those exact pixels too.
        solids.push_back({
            level.spikeHazard.x - PitWallSize,
            PitTopY,
            PitWallSize,
            static_cast<float>(Constants::ScreenHeight) - PitTopY
        });
        solids.push_back({
            level.spikeHazard.x + level.spikeHazard.width,
            PitTopY,
            PitWallSize,
            static_cast<float>(Constants::ScreenHeight) - PitTopY
        });
    }

    for (const BreakableTile& tile : level.breakableTiles) {
        if (!tile.broken && tile.rect.width > 0.0f && tile.rect.height > 0.0f) {
            solids.push_back(tile.rect);
        }
    }

    return solids;
}
