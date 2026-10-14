#pragma once

#include "LevelEditor.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

struct AdvancedPlacement {
    LevelEditor::EditTool tool;
    GuideObjectType type;
    const char* name;
    const char* command;
    const char* defaults;
};
constexpr AdvancedPlacement AdvancedPlacements[] = {
    {LevelEditor::EditTool::Cam, GuideObjectType::Cam, "Cam", "cam", "0 0 24 90 8 0"},
    {LevelEditor::EditTool::ConveyorBelt, GuideObjectType::ConveyorBelt, "Conveyor Belt", "conveyorBelt", "0 0 96 20 80"},
    {LevelEditor::EditTool::Turntable, GuideObjectType::Turntable, "Turntable", "turntable", "0 0 32 90 0"},
    {LevelEditor::EditTool::Battery, GuideObjectType::Battery, "Battery", "battery", "0 0 48 48 1 0"},
    {LevelEditor::EditTool::ElectricMotor, GuideObjectType::ElectricMotor, "Electric Motor", "electricMotor", "0 0 28 120 0"},
    {LevelEditor::EditTool::Generator, GuideObjectType::Generator, "Generator", "generator", "0 0 64 48 180 1 0.85 0"},
    {LevelEditor::EditTool::LimitSwitch, GuideObjectType::LimitSwitch, "Limit Switch", "limitSwitch", "0 0 32 20 0"},
    {LevelEditor::EditTool::Relay, GuideObjectType::Relay, "Relay", "relay", "0 0 40 32 0"},
    {LevelEditor::EditTool::Fuse, GuideObjectType::Fuse, "Fuse", "fuse", "0 0 32 20 1 0"},
    {LevelEditor::EditTool::Magnet, GuideObjectType::Magnet, "Magnet", "magnet", "0 0 80 300 0"},
    {LevelEditor::EditTool::Piston, GuideObjectType::Piston, "Piston", "piston", "0 0 64 24 1 0 120 1 0"},
    {LevelEditor::EditTool::HydraulicCylinder, GuideObjectType::HydraulicCylinder, "Hydraulic Cylinder", "hydraulicCylinder", "0 0 64 24 1 0 120 1 0"},
    {LevelEditor::EditTool::RocketThruster, GuideObjectType::RocketThruster, "Rocket Thruster", "rocketThruster", "0 0 0 -1 160 40 500 0"},
    {LevelEditor::EditTool::CrushingBlock, GuideObjectType::CrushingBlock, "Crushing Block", "crushingBlock", "0 0 64 64 0 1 120 1"},
    {LevelEditor::EditTool::SwingingHammer, GuideObjectType::SwingingHammer, "Swinging Hammer", "swingingHammer", "0 0 120 28 0 1"},
    {LevelEditor::EditTool::SawBlade, GuideObjectType::SawBlade, "Saw Blade", "sawBlade", "0 0 28 1 0 120 1"},
    {LevelEditor::EditTool::SpinnerTrap, GuideObjectType::SpinnerTrap, "Spinner Trap", "spinnerTrap", "0 0 40 120"},
    {LevelEditor::EditTool::SteamVent, GuideObjectType::SteamVent, "Steam Vent", "steamVent", "0 0 0 -1 160 40 400 2"},
    {LevelEditor::EditTool::ElectricalArc, GuideObjectType::ElectricalArc, "Electrical Arc", "electricalArc", "0 0 80 0 2 0"},
    {LevelEditor::EditTool::Oil, GuideObjectType::Oil, "Oil", "oil", "0 0 128 32"},
    {LevelEditor::EditTool::Mud, GuideObjectType::Mud, "Mud", "mud", "0 0 128 32"},
    {LevelEditor::EditTool::SpeedSensor, GuideObjectType::SpeedSensor, "Speed Sensor", "speedSensor", "0 0 64 32 80 0"},
    {LevelEditor::EditTool::BeamSensor, GuideObjectType::BeamSensor, "Beam Sensor", "beamSensor", "0 0 1 0 160 0"},
    {LevelEditor::EditTool::Key, GuideObjectType::Key, "Key", "key", "0 0 12"},
    {LevelEditor::EditTool::ScubaGear, GuideObjectType::ScubaGear, "Scuba Gear", "scubaGear", "0 0 16"},
    {LevelEditor::EditTool::GasMask, GuideObjectType::GasMask, "Gas Mask", "gasMask", "0 0 16"},
    {LevelEditor::EditTool::BreakableCrate, GuideObjectType::BreakableCrate, "Breakable Crate", "breakableCrate", "0 0 48 48 2"},
    {LevelEditor::EditTool::ExplosiveBarrel, GuideObjectType::ExplosiveBarrel, "Explosive Barrel", "explosiveBarrel", "0 0 40 56 2"},
    {LevelEditor::EditTool::Tnt, GuideObjectType::Tnt, "TNT", "tnt", "0 0 52 38 1.6 1.5 180"},
    {LevelEditor::EditTool::AntiGravityPad, GuideObjectType::AntiGravityPad, "Anti-Gravity Pad", "antiGravityPad", "0 0 96 24 160 500 0"},
    {LevelEditor::EditTool::TripodDerrick, GuideObjectType::TripodDerrick, "Tripod Derrick", "tripodDerrick", "0 0 220 300"},
    {LevelEditor::EditTool::ChainHoist, GuideObjectType::ChainHoist, "Chain Hoist", "chainHoist", "0 0 180 1 240 0"},
    {LevelEditor::EditTool::BlockAndTackle, GuideObjectType::BlockAndTackle, "Block and Tackle", "blockAndTackle", "0 0 180 0.7 420 0"},
    {LevelEditor::EditTool::ComeAlong, GuideObjectType::ComeAlong, "Come-Along", "comeAlong", "0 0 160 30 1.5 95 0"},
    {LevelEditor::EditTool::ManualBenchGrinder, GuideObjectType::ManualBenchGrinder, "Manual Bench Grinder", "manualBenchGrinder", "0 0 30 280"},
    {LevelEditor::EditTool::LoadCart, GuideObjectType::LoadCart, "Load Cart", "loadCart", "0 0 70 36 3"},
    {LevelEditor::EditTool::RevolvingStoneGate, GuideObjectType::RevolvingStoneGate, "Revolving Stone Gate", "revolvingStoneGate", "0 0 60 220 0.3"},
    {LevelEditor::EditTool::RockHammer, GuideObjectType::RockHammer, "Rock Hammer", "rockHammer", "0 0 60 24 2.4"},
    {LevelEditor::EditTool::FieldCoil, GuideObjectType::FieldCoil, "Field Coil", "fieldCoil", "0 0 60 700 0"},
    {LevelEditor::EditTool::SpokePress, GuideObjectType::SpokePress, "Spoke Press", "spokePress", "0 0 70 40 0 1 90 1.2 0"},
    {LevelEditor::EditTool::PolarisTelescope, GuideObjectType::PolarisTelescope, "Polaris Telescope", "polarisTelescope", "0 0 160 -30 0"},
    {LevelEditor::EditTool::Sundial, GuideObjectType::Sundial, "Sundial", "leedskalninSundial", "0 0 45 4 0"},
    {LevelEditor::EditTool::PerpetualMotionHolder, GuideObjectType::PerpetualMotionHolder, "Perpetual Motion Holder", "perpetualMotionHolder", "0 0 80 70 120 900 0"},
    {LevelEditor::EditTool::Bellows, GuideObjectType::Bellows, "Bellows", "bellows", "0 0 56 34 1 0 110 520"},
    {LevelEditor::EditTool::Trampoline, GuideObjectType::Trampoline, "Trampoline", "trampoline", "0 0 96 20 740"},
    {LevelEditor::EditTool::BoxingGlove, GuideObjectType::BoxingGlove, "Boxing Glove", "boxingGlove", "0 0 42 30 1 0 72 620"},
    {LevelEditor::EditTool::Vacuum, GuideObjectType::Vacuum, "Vacuum", "vacuum", "0 0 52 42 1 0 180 520 0"},
    {LevelEditor::EditTool::MagnifyingGlass, GuideObjectType::MagnifyingGlass, "Magnifying Glass", "magnifyingGlass", "0 0 22 1 0 120"},
    {LevelEditor::EditTool::LightSource, GuideObjectType::LightSource, "Light Source", "lightSource", "0 0 1 0 220 5 0"},
    {LevelEditor::EditTool::IgnitableFuse, GuideObjectType::IgnitableFuse, "Ignitable Fuse", "ignitableFuse", "0 0 96 12 0"},
    {LevelEditor::EditTool::ProgrammableBall, GuideObjectType::ProgrammableBall, "Programmable Ball", "programmableBall", "0 0 18 1 0.65 0.35 1"},
    {LevelEditor::EditTool::TransRotoConverter, GuideObjectType::TransRotoConverter, "Trans-Roto Converter", "transRotoConverter", "0 0 26 1"},
    {LevelEditor::EditTool::RotoTransConverter, GuideObjectType::RotoTransConverter, "Roto-Trans Converter", "rotoTransConverter", "0 0 64 28 1 0 90 1.5"},
    {LevelEditor::EditTool::AcceleratorTube, GuideObjectType::AcceleratorTube, "Accelerator Tube", "acceleratorTube", "0 0 128 34 1 0 560 0"},
    {LevelEditor::EditTool::PinballBumper, GuideObjectType::PinballBumper, "Pinball Bumper", "pinballBumper", "0 0 24 620"},
    {LevelEditor::EditTool::PinballFlipper, GuideObjectType::PinballFlipper, "Pinball Flipper", "pinballFlipper", "0 0 72 16 1 620 0"},
    {LevelEditor::EditTool::LaserEmitter, GuideObjectType::LaserEmitter, "Laser Emitter", "laserEmitter", "0 0 1 0 260 red 0"},
    {LevelEditor::EditTool::LaserMixer, GuideObjectType::LaserMixer, "Laser Mixer", "laserMixer", "0 0 42 42 1 0 220 0"},
    {LevelEditor::EditTool::LaserDetector, GuideObjectType::LaserDetector, "Laser Detector", "laserDetector", "0 0 34 34 red 0"},
    {LevelEditor::EditTool::HotAirBalloon, GuideObjectType::HotAirBalloon, "Hot-Air Balloon", "hotAirBalloon", "0 0 30 1.2 11"},
    {LevelEditor::EditTool::Blimp, GuideObjectType::Blimp, "Blimp", "blimp", "0 0 100 44 2 11"},
    {LevelEditor::EditTool::LeakyBucket, GuideObjectType::LeakyBucket, "Leaky Bucket", "leakyBucket", "0 0 42 42 2 20 1.5"},
    {LevelEditor::EditTool::DestructibleWood, GuideObjectType::DestructibleWood, "Destructible Wood", "destructibleWood", "0 0 96 24 80"},
    {LevelEditor::EditTool::DestructibleBrick, GuideObjectType::DestructibleBrick, "Destructible Brick", "destructibleBrick", "0 0 96 24 180"},
    {LevelEditor::EditTool::DestructibleGlass, GuideObjectType::DestructibleGlass, "Destructible Glass", "destructibleGlass", "0 0 96 24 35"},
    {LevelEditor::EditTool::GlassPane, GuideObjectType::GlassPane, "Glass Pane", "glassPane", "0 0 12 96 24"},
    {LevelEditor::EditTool::GlassBlock, GuideObjectType::GlassBlock, "Glass Block", "glassBlock", "0 0 48 48 95"},
    {LevelEditor::EditTool::IceBlock, GuideObjectType::IceBlock, "Ice Block", "iceBlock", "0 0 64 64 120"},
    {LevelEditor::EditTool::IceSheet, GuideObjectType::IceSheet, "Ice Sheet", "iceSheet", "0 0 128 14 45"},
    {LevelEditor::EditTool::Heater, GuideObjectType::Heater, "Heater", "heater", "0 0 48 48 1 0 220 54 42 0"},
    {LevelEditor::EditTool::Campfire, GuideObjectType::Campfire, "Campfire", "campfire", "0 0 22 150 30"},
    {LevelEditor::EditTool::Torch, GuideObjectType::Torch, "Torch", "torch", "0 0 12 48 0.8 82 18"},
    {LevelEditor::EditTool::WaterBucket, GuideObjectType::WaterBucket, "Bucket of Water", "waterBucket", "0 0 38 42 1.2 24 8 0"},
    {LevelEditor::EditTool::SignalDust, GuideObjectType::SignalDust, "Signal Dust", "signalDust", "0 0 64 0 1"},
    {LevelEditor::EditTool::TernaryTrace, GuideObjectType::TernaryTrace, "Ternary Trace", "ternaryTrace", "0 0 64 0 2 3"},
    {LevelEditor::EditTool::TernarySwitch, GuideObjectType::TernarySwitch, "Ternary Switch", "ternarySwitch", "0 0 48 32 0"},
    {LevelEditor::EditTool::Teleforce, GuideObjectType::Teleforce, "Teleforce Projector", "teleforce", "0 0 86 58 1 0 420 22 1800 1.6 0.24 1.1 0"},
    {LevelEditor::EditTool::TeslaCoil, GuideObjectType::TeslaCoil, "Tesla Coil", "teslaCoil", "0 0 72 124 190 460 1.2 0.32 0.9 0 1"},
};

inline const AdvancedPlacement* FindAdvancedPlacement(LevelEditor::EditTool tool) {
    for (const auto& entry : AdvancedPlacements) if (entry.tool == tool) return &entry;
    return nullptr;
}

inline std::string AdvancedObjectRecord(const GuideObject& object) {
    const AdvancedPlacement* entry = nullptr;
    for (const auto& candidate : AdvancedPlacements)
        if (candidate.type == object.type) { entry = &candidate; break; }
    if (!entry) return {};
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << entry->command << ' ' << object.origin.x << ' ' << object.origin.y;
    switch (object.type) {
    case GuideObjectType::Cam: output << ' ' << object.collider.radius << ' ' << object.speed << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::ConveyorBelt: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.speed; break;
    case GuideObjectType::Turntable: output << ' ' << object.collider.radius << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::Battery: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::ElectricMotor: output << ' ' << object.collider.radius << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::Generator: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.ratedMechanicalSpeed << ' ' << object.power.maximumPower << ' ' << object.generatorEfficiency << ' ' << object.power.channel; break;
    case GuideObjectType::LimitSwitch: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.power.channel; break;
    case GuideObjectType::Relay: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.power.channel; break;
    case GuideObjectType::Fuse: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::Magnet: output << ' ' << object.collider.radius << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::Piston: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::HydraulicCylinder: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::RocketThruster: output << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.width << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::CrushingBlock: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed; break;
    case GuideObjectType::SwingingHammer: output << ' ' << object.length << ' ' << object.collider.radius << ' ' << object.phase << ' ' << object.speed; break;
    case GuideObjectType::SawBlade: output << ' ' << object.collider.radius << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed; break;
    case GuideObjectType::SpinnerTrap: output << ' ' << object.collider.radius << ' ' << object.speed; break;
    case GuideObjectType::SteamVent: output << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.width << ' ' << object.strength << ' ' << object.interval; break;
    case GuideObjectType::ElectricalArc: output << ' ' << object.constraint.anchorB.x << ' ' << object.constraint.anchorB.y << ' ' << object.interval << ' ' << object.power.channel; break;
    case GuideObjectType::Oil: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y; break;
    case GuideObjectType::Mud: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y; break;
    case GuideObjectType::SpeedSensor: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.sensor.threshold << ' ' << object.power.channel; break;
    case GuideObjectType::BeamSensor: output << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.power.channel; break;
    case GuideObjectType::Key: output << ' ' << object.collider.radius; break;
    case GuideObjectType::ScubaGear:
    case GuideObjectType::GasMask: output << ' ' << object.collider.radius; break;
    case GuideObjectType::BreakableCrate: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass; break;
    case GuideObjectType::ExplosiveBarrel: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass; break;
    case GuideObjectType::Tnt: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass << ' ' << object.interval << ' ' << object.strength; break;
    case GuideObjectType::AntiGravityPad: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.length << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::TripodDerrick: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y; break;
    case GuideObjectType::ChainHoist:
    case GuideObjectType::BlockAndTackle: output << ' ' << object.length << ' ' << object.speed << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::ComeAlong: output << ' ' << object.constraint.anchorB.x << ' ' << object.constraint.anchorB.y << ' ' << object.speed << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::ManualBenchGrinder: output << ' ' << object.collider.radius << ' ' << object.speed; break;
    case GuideObjectType::LoadCart:
    case GuideObjectType::RockHammer: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass; break;
    case GuideObjectType::RevolvingStoneGate: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength; break;
    case GuideObjectType::FieldCoil: output << ' ' << object.collider.radius << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::SpokePress: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::PolarisTelescope: output << ' ' << object.length << ' ' << atan2f(object.direction.y, object.direction.x) * RAD2DEG << ' ' << object.power.channel; break;
    case GuideObjectType::Sundial: output << ' ' << object.collider.radius << ' ' << object.interval << ' ' << object.power.channel; break;
    case GuideObjectType::PerpetualMotionHolder: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.length << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::Bellows: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.strength; break;
    case GuideObjectType::BoxingGlove: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::Trampoline: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength; break;
    case GuideObjectType::Vacuum: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::MagnifyingGlass: output << ' ' << object.collider.radius << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length; break;
    case GuideObjectType::LightSource: output << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.width << ' ' << object.power.channel; break;
    case GuideObjectType::IgnitableFuse: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.sensor.channel; break;
    case GuideObjectType::ProgrammableBall: output << ' ' << object.collider.radius << ' ' << object.body.mass << ' ' << object.collider.restitution << ' ' << object.collider.friction << ' ' << object.strength; break;
    case GuideObjectType::TransRotoConverter: output << ' ' << object.collider.radius << ' ' << object.strength; break;
    case GuideObjectType::RotoTransConverter: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::AcceleratorTube: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::PinballBumper: output << ' ' << object.collider.radius << ' ' << object.strength; break;
    case GuideObjectType::PinballFlipper: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::LaserEmitter: output << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << (object.colorMask == 2 ? "green" : object.colorMask == 4 ? "blue" : object.colorMask == 3 ? "yellow" : object.colorMask == 5 ? "magenta" : object.colorMask == 6 ? "cyan" : object.colorMask == 7 ? "white" : "red") << ' ' << object.power.channel; break;
    case GuideObjectType::LaserMixer: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length; break;
    case GuideObjectType::LaserDetector: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << (object.colorMask == 2 ? "green" : object.colorMask == 4 ? "blue" : object.colorMask == 3 ? "yellow" : object.colorMask == 5 ? "magenta" : object.colorMask == 6 ? "cyan" : object.colorMask == 7 ? "white" : "red") << ' ' << object.sensor.channel; break;
    case GuideObjectType::HotAirBalloon: output << ' ' << object.collider.radius << ' ' << object.body.mass << ' ' << object.strength; break;
    case GuideObjectType::Blimp: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass << ' ' << object.strength; break;
    case GuideObjectType::LeakyBucket: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << fmaxf(0.1f, object.body.mass - object.capacity) << ' ' << object.capacity << ' ' << object.speed; break;
    case GuideObjectType::DestructibleWood:
    case GuideObjectType::DestructibleBrick:
    case GuideObjectType::DestructibleGlass: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength; break;
    case GuideObjectType::GlassPane:
    case GuideObjectType::GlassBlock: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength; break;
    case GuideObjectType::IceBlock:
    case GuideObjectType::IceSheet: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.strength; break;
    case GuideObjectType::Heater: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.width << ' ' << object.strength << ' ' << object.power.channel; break;
    case GuideObjectType::Campfire: output << ' ' << object.collider.radius << ' ' << object.length << ' ' << object.strength; break;
    case GuideObjectType::Torch: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.body.mass << ' ' << object.length << ' ' << object.strength; break;
    case GuideObjectType::WaterBucket: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << fmaxf(0.1f, object.body.mass - object.capacity * 0.08f) << ' ' << object.capacity << ' ' << object.speed << ' ' << object.transform.rotation; break;
    case GuideObjectType::SignalDust: output << ' ' << object.transform.position.x + object.direction.x * object.length << ' ' << object.transform.position.y + object.direction.y * object.length << ' ' << object.power.channel; break;
    case GuideObjectType::TernaryTrace: output << ' ' << object.transform.position.x + object.direction.x * object.length << ' ' << object.transform.position.y + object.direction.y * object.length << ' ' << object.sensor.channel << ' ' << object.power.channel; break;
    case GuideObjectType::TernarySwitch: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << std::clamp(object.signalMask, -1, 1); break;
    case GuideObjectType::Teleforce: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.direction.x << ' ' << object.direction.y << ' ' << object.length << ' ' << object.width << ' ' << object.strength << ' ' << object.capacity << ' ' << object.damage << ' ' << object.speed << ' ' << object.power.channel; break;
    case GuideObjectType::TeslaCoil: output << ' ' << object.collider.size.x << ' ' << object.collider.size.y << ' ' << object.length << ' ' << object.strength << ' ' << object.capacity << ' ' << object.damage << ' ' << object.speed << ' ' << object.power.channel << ' ' << object.sensor.channel; break;
    default: break;
    }
    return output.str();
}

