#include "EditorObjectPlacement.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    for (const auto& entry : AdvancedPlacements) {
        GuideObject object{};
        std::istringstream defaults(entry.defaults);
        if (!ParseGuideObject(entry.command, defaults, object) || object.type != entry.type)
            throw std::runtime_error(std::string("Invalid placement defaults: ") + entry.name);
        // Moving and resizing must serialize current authored values, not defaults.
        object.origin = {320, 192};
        object.transform.position = object.origin;
        object.collider.size.x += 16;
        object.collider.radius += 4;
        object.power.channel = 7;
        object.constraint.anchorB = {400, 192};
        const auto record = AdvancedObjectRecord(object);
        std::istringstream stream(record);
        std::string command;
        stream >> command;
        GuideObject restored{};
        if (!ParseGuideObject(command, stream, restored) || restored.type != entry.type ||
            std::fabs(restored.origin.x - 320) > 0.01f || std::fabs(restored.origin.y - 192) > 0.01f ||
            AdvancedObjectRecord(restored) != record)
            throw std::runtime_error(std::string("Placement round trip failed: ") + entry.name);
        const auto bounds = GetGuideObjectBounds(restored);
        if (bounds.width <= 0 || bounds.height <= 0)
            throw std::runtime_error(std::string("Unselectable placement: ") + entry.name);
    }
    GuideObject rock{};
    std::istringstream rockArgs("12 24 32 32 45");
    if (!ParseGuideObject("rock", rockArgs, rock) || rock.transform.rotation != 45 ||
        !rock.collider.isTrigger || rock.body.type != BodyType::Static)
        throw std::runtime_error("Rock must preserve rotation and remain a static decoration.");
    std::vector<Rectangle> rockSolids;
    AppendGuideObjectSolids(rockSolids, {rock});
    if (!rockSolids.empty()) throw std::runtime_error("Decorative rocks must not block movement.");
    const auto rockRecord = AdvancedObjectRecord(rock);
    GuideObject restoredRock{};
    std::istringstream restoredArgs(rockRecord.substr(5));
    if (!ParseGuideObject("rock", restoredArgs, restoredRock) || restoredRock.transform.rotation != 45)
        throw std::runtime_error("Rock rotation was lost during serialization.");
    std::cout << "All advanced placement defaults, transforms, bounds, and authored records passed.\n";
}
