#include "GuideObjects.h"
#include "Level.h"

#include <algorithm>
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
            std::cerr << "Failed to parse classic contraption: " << command << '\n';
            std::exit(1);
        }
        return object;
    }

    void Step(std::vector<GuideObject>& objects, Player* player = nullptr, float dt = 1.0f / 60.0f) {
        std::array<Player*, 4> players{player, nullptr, nullptr, nullptr};
        std::array<std::vector<Rectangle>, WorldLayerCount> solids{};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, players, solids, 980.0f, dt, checkpoint);
    }
}

int main() {
    const std::array<std::pair<const char*, const char*>, 22> records{{
        {"bellows", "10 10 60 30 1 0 120 500"},
        {"trampoline", "10 100 100 20 520"},
        {"boxingGlove", "10 10 50 30 1 0 70 500"},
        {"vacuum", "10 10 50 40 1 0 180 700"},
        {"magnifyingGlass", "100 100 20 1 0 100"},
        {"lightSource", "10 100 1 0 100 4"},
        {"ignitableFuse", "190 90 20 20 3"},
        {"programmableBall", "100 100 14 2.0 0.8 0.2 0.5"},
        {"transRotoConverter", "100 100 24 1.2"},
        {"rotoTransConverter", "100 100 60 18 1 0 80 2.0"},
        {"acceleratorTube", "100 100 140 50 1 0 600"},
        {"pinballBumper", "100 100 24 500"},
        {"pinballFlipper", "100 100 90 18 1 520"},
        {"laserEmitter", "10 100 1 0 180 red"},
        {"laserMixer", "100 100 30 30 1 0 150"},
        {"laserDetector", "100 100 30 30 cyan 4"},
        {"hotAirBalloon", "100 100 35 1.5 1200"},
        {"blimp", "100 100 120 50 2.0 1700"},
        {"leakyBucket", "100 100 42 48 1.0 3.0 0.5"},
        {"destructibleWood", "100 100 40 80 50"},
        {"destructibleBrick", "100 100 40 80 100"},
        {"destructibleGlass", "100 100 40 80 20"}
    }};
    for (const auto& record : records) {
        const GuideObject object = Parse(record.first, record.second);
        if (std::string(GetGuideObjectName(object.type)) == "Object" ||
            std::string(GetGuideObjectDescription(object.type)) == "An interactive object in the level.") {
            std::cerr << "Missing catalog metadata for " << record.first << ".\n";
            return 1;
        }
    }
    for (const auto& record : std::array<std::pair<const char*, const char*>, 2>{{
        {"glassPane", "100 100 12 96 24"},
        {"glassBlock", "100 100 48 48 95"}
    }}) {
        const GuideObject object = Parse(record.first, record.second);
        if (object.body.type != BodyType::Static || object.collider.shape != ColliderShape::Rectangle ||
            object.collider.isTrigger || std::string(GetGuideObjectName(object.type)) == "Object") {
            std::cerr << "Glass object is missing collision or catalog metadata: " << record.first << ".\n";
            return 1;
        }
    }

    Player jumper{};
    jumper.rect = {130.0f, 168.0f, 24.0f, 32.0f};
    jumper.velocity.y = 160.0f;
    std::vector<GuideObject> trampoline{Parse("trampoline", "100 200 100 20 520")};
    Step(trampoline, &jumper);
    if (jumper.velocity.y > -500.0f) {
        std::cerr << "Trampoline did not return a strong upward bounce.\n";
        return 1;
    }

    std::vector<GuideObject> optics{
        Parse("lightSource", "10 100 1 0 100 4"),
        Parse("magnifyingGlass", "110 100 18 0 1 100"),
        Parse("ignitableFuse", "100 180 20 20 3"),
        Parse("electricMotor", "250 100 20 120 3")
    };
    Step(optics);
    if (!optics[1].triggered || !optics[2].triggered || !optics[3].power.powered) {
        std::cerr << "Focused light did not ignite the fuse and power its channel.\n";
        return 1;
    }

    std::vector<GuideObject> lasers{
        Parse("laserEmitter", "10 100 1 0 110 red"),
        Parse("laserEmitter", "100 10 0 1 100 green"),
        Parse("laserMixer", "100 90 30 30 1 0 120"),
        Parse("laserDetector", "225 90 35 35 yellow 7"),
        Parse("electricMotor", "300 100 20 120 7")
    };
    Step(lasers);
    if (lasers[2].colorMask != 3 || !lasers[3].sensor.active || !lasers[4].power.powered) {
        std::cerr << "RGB laser mixer or color detector did not propagate yellow light.\n";
        return 1;
    }

    std::vector<GuideObject> forceTools{
        Parse("vacuum", "100 100 50 40 1 0 180 700"),
        Parse("ball", "210 115 12 1.0"),
        Parse("acceleratorTube", "300 90 140 60 1 0 600"),
        Parse("ball", "330 115 12 1.0")
    };
    Step(forceTools);
    if (forceTools[1].body.velocity.x >= 0.0f || forceTools[3].body.velocity.x <= 0.0f) {
        std::cerr << "Vacuum or accelerator tube did not apply its directional force.\n";
        return 1;
    }

    std::vector<GuideObject> pinball{
        Parse("pinballBumper", "100 100 25 500"),
        Parse("ball", "118 100 12 1.0")
    };
    Step(pinball);
    if (pinball[1].body.velocity.x < 450.0f) {
        std::cerr << "Pinball bumper did not kick the contacting ball away.\n";
        return 1;
    }

    std::vector<GuideObject> converters{
        Parse("transRotoConverter", "100 100 28 1.0"),
        Parse("rotoTransConverter", "180 100 60 18 1 0 80 0"),
        Parse("ball", "118 100 12 1.0")
    };
    converters[2].body.velocity.x = 240.0f;
    Step(converters);
    if (fabsf(converters[0].speed) < 0.1f || fabsf(converters[1].speed - converters[0].speed) > 0.001f) {
        std::cerr << "Trans-roto and roto-trans converters did not transfer motion.\n";
        return 1;
    }

    std::vector<GuideObject> buoyancy{Parse("hotAirBalloon", "100 100 35 1.5 1300")};
    Step(buoyancy);
    if (buoyancy[0].body.velocity.y >= 0.0f) {
        std::cerr << "Hot-air balloon lift did not overcome gravity.\n";
        return 1;
    }

    GuideObject blast = Parse("explosiveBarrel", "110 110 30 45 2.0");
    blast.broken = true;
    blast.timer = 0.0f;
    std::vector<GuideObject> breakables{Parse("destructibleBrick", "100 100 45 80 120"), blast};
    Step(breakables);
    if (!breakables[0].broken) {
        std::cerr << "Explosion did not destroy nearby breakable material.\n";
        return 1;
    }

    std::vector<GuideObject> timedTnt{
        Parse("tnt", "100 100 52 38 1.6 0.1 190"),
        Parse("campfire", "110 110 22 150 30")
    };
    Step(timedTnt, nullptr, 0.01f);
    if (!timedTnt[0].triggered || timedTnt[0].broken) {
        std::cerr << "Campfire did not light the TNT fuse.\n";
        return 1;
    }
    for (int frame = 0; frame < 11; ++frame) Step(timedTnt, nullptr, 0.01f);
    if (!timedTnt[0].broken) {
        std::cerr << "TNT did not explode after its authored fuse delay.\n";
        return 1;
    }

    std::vector<GuideObject> fusedTnt{
        Parse("tnt", "100 100 52 38 1.6 1.0 180"),
        Parse("ignitableFuse", "96 96 64 16 3")
    };
    fusedTnt[1].triggered = true;
    Step(fusedTnt);
    if (!fusedTnt[0].triggered) {
        std::cerr << "An ignited fuse did not light touching TNT.\n";
        return 1;
    }

    GuideObject chainTnt = Parse("tnt", "100 100 52 38 1.6 1.5 190");
    chainTnt.broken = true;
    chainTnt.timer = 0.0f;
    std::vector<GuideObject> chainReaction{chainTnt, Parse("explosiveBarrel", "180 100 40 56 2")};
    Step(chainReaction);
    if (!chainReaction[1].broken) {
        std::cerr << "TNT did not trigger a nearby explosive in a chain reaction.\n";
        return 1;
    }

    std::vector<GuideObject> leaking{Parse("leakyBucket", "100 100 40 50 1.0 3.0 0.5")};
    const float initialMass = leaking[0].body.mass;
    Step(leaking, nullptr, 1.0f);
    if (leaking[0].capacity >= 3.0f || leaking[0].body.mass >= initialMass) {
        std::cerr << "Leaky bucket did not lose water mass over time.\n";
        return 1;
    }

    const Level lab = LoadLevelFromFile(CLASSIC_LAB_LEVEL_PATH, {});
    int newPartCount = 0;
    for (const GuideObject& object : lab.guideObjects) {
        if (object.type >= GuideObjectType::Bellows && object.type <= GuideObjectType::DestructibleGlass) {
            ++newPartCount;
        }
    }
    if (newPartCount < 22) {
        std::cerr << "Contraption Parts Laboratory is missing one or more new object families.\n";
        return 1;
    }

    const std::array<Rectangle, 4> accessFloors{{
        {28.0f, 270.0f, 155.0f, 16.0f},
        {190.0f, 350.0f, 220.0f, 16.0f},
        {455.0f, 300.0f, 535.0f, 16.0f},
        {1000.0f, 350.0f, 544.0f, 16.0f}
    }};
    for (const Rectangle floor : accessFloors) {
        const bool hasRoute = std::any_of(lab.ladders.begin(), lab.ladders.end(), [floor](Rectangle ladder) {
            const bool overlapsFloor = ladder.x < floor.x + floor.width && ladder.x + ladder.width > floor.x &&
                ladder.y <= floor.y && ladder.y + ladder.height >= 555.0f;
            return overlapsFloor;
        });
        if (std::none_of(lab.pitPlatforms.begin(), lab.pitPlatforms.end(), [floor](Rectangle platform) {
                return platform.x == floor.x && platform.y == floor.y &&
                    platform.width == floor.width && platform.height == floor.height;
            }) || !hasRoute) {
            std::cerr << "An elevated access floor is missing or has no ladder route to the main walkway.\n";
            return 1;
        }
    }

    std::cout << "All classic contraption checks passed.\n";
    return 0;
}

