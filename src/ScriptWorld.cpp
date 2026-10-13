#include "ScriptWorld.h"
#include <sstream>
// Translation always uses the current runtime bounds, never editor positions.
bool ScriptTarget(Level& level, const std::string& type, int ordinal, Rectangle& bounds,
    const Vector2* delta) {
    const auto i = static_cast<size_t>(ordinal - 1);
    if (ordinal < 1) return false;
    auto rect = [&](Rectangle& value) {
        bounds = value;
        if (delta) { value.x += delta->x; value.y += delta->y; }
        return true;
    };
    auto center = [&](Vector2& value, float width, float height) {
        bounds = {value.x - width / 2, value.y - height / 2, width, height};
        if (delta) { value.x += delta->x; value.y += delta->y; }
        return true;
    };
#define RECTS(name, member) if (type == name && i < level.member.size()) return rect(level.member[i]);
#define BODIES(name, member) if (type == name && i < level.member.size()) return rect(level.member[i].rect);
#define CIRCLES(name, member) if (type == name && i < level.member.size()) { auto& o = level.member[i]; return center(o.center, o.radius * 2, o.radius * 2); }
    RECTS("solid", baseSolids) RECTS("platform", pitPlatforms) RECTS("ladder", ladders)
    RECTS("darkness", darknessAreas) RECTS("cameraZone", cameraZones)
    BODIES("stoneBlock", stoneBlocks) BODIES("enemy", enemies)
    BODIES("button", buttons) BODIES("breakableTile", breakableTiles) BODIES("weight", weights)
    BODIES("directionalSpikeHazard", directionalSpikeHazards)
    CIRCLES("boulder", boulders) CIRCLES("physicsWheel", physicsWheels)
    CIRCLES("gear", gears) CIRCLES("flywheel", flywheels)
    CIRCLES("rotaryLatch", rotaryLatches) CIRCLES("steeringWheel", steeringWheels) CIRCLES("pinwheel", pinwheels)
#undef RECTS
#undef BODIES
#undef CIRCLES
    if (type == "visualTile" && i < level.visualTiles.size()) {
        auto& tile = level.visualTiles[i];
        bounds = {tile.position.x, tile.position.y, 32, 32};
        if (delta) { tile.position.x += delta->x; tile.position.y += delta->y; }
        return true;
    }
    if (type == "exit" && i == 0) return rect(level.exitTrigger);
    if (type == "bounds" && i == 0) return rect(level.worldBounds);
    if (type == "spikeHazard" && i == 0) return rect(level.spikeHazard);
    if (type == "valve" && i == 0) return center(level.valve.center, level.valve.radius * 2, level.valve.radius * 2);
    if (type == "pulley" && i < level.pulleys.size()) return center(level.pulleys[i], 32, 32);
    if (type == "arrowTrap" && i < level.arrowTraps.size()) return center(level.arrowTraps[i].position, 32, 32);
    if (type == "fan" && i < level.fans.size()) return center(level.fans[i].center, 32, 32);
    if (type == "ramp" || type == "wedge") {
        size_t found = 0;
        for (auto& o : level.ramps) {
            if (o.wedge != (type == "wedge") || found++ != i) continue;
            return center(o.center, o.length, o.thickness);
        }
        return false;
    }
    if (type == "screw" && i < level.screws.size()) { auto& o = level.screws[i]; return center(o.center, o.length, o.radius * 2); }
    if (type == "seeSaw" && i < level.seeSaws.size()) { auto& o = level.seeSaws[i]; return center(o.pivot, o.length, o.thickness); }
    if (type == "trapDoor" && i < level.trapDoors.size()) { auto& o = level.trapDoors[i]; return center(o.hinge, o.length, o.thickness); }
    // Guide parts share a runtime vector, but attachment indices are per type.
    GuideObject prototype;
    std::istringstream defaults("0 0 32 32 1 1 1 1 1 1 1 1");
    if (!ParseGuideObject(type, defaults, prototype)) return false;
    size_t found = 0;
    for (auto& object : level.guideObjects) {
        if (object.type != prototype.type || found++ != i) continue;
        bounds = GetGuideObjectBounds(object);
        if (delta) {
            auto move = [&](Vector2& p) { p.x += delta->x; p.y += delta->y; };
            move(object.transform.position); move(object.origin); move(object.previousPosition);
            move(object.constraint.anchorA); move(object.constraint.anchorB);
        }
        return true;
    }
    return false;
}
bool ScriptOverlap(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width && a.x + a.width > b.x &&
        a.y < b.y + b.height && a.y + a.height > b.y;
}

