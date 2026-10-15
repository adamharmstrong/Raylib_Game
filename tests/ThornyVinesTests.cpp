#include "GuideObjects.h"
#include "Level.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    GuideObject vines{};
    std::istringstream authored("120 240 48 160 0.45");
    if (!ParseGuideObject("thornyVines", authored, vines)) {
        std::cerr << "Failed to parse thorny vines.\n";
        return 1;
    }

    const Rectangle bounds = GetGuideObjectBounds(vines);
    if (vines.type != GuideObjectType::ThornyVines || !vines.collider.isTrigger ||
        fabsf(bounds.x - 120.0f) > 0.001f || fabsf(bounds.y - 240.0f) > 0.001f ||
        fabsf(bounds.width - 48.0f) > 0.001f || fabsf(bounds.height - 160.0f) > 0.001f ||
        fabsf(GetGuideObjectContactDamage(vines) - 0.45f) > 0.001f) {
        std::cerr << "Thorny vines did not preserve their bounds or damage rate.\n";
        return 1;
    }
    if (!IsGuideObjectClimbable(vines) ||
        std::string(GetGuideObjectName(vines.type)) != "Thorny Vines") {
        std::cerr << "Active thorny vines were not exposed as a climbable object.\n";
        return 1;
    }

    vines.active = false;
    if (IsGuideObjectClimbable(vines) || GetGuideObjectContactDamage(vines) != 0.0f) {
        std::cerr << "Inactive thorny vines remained climbable or damaging.\n";
        return 1;
    }

    GuideObject defaults{};
    std::istringstream defaultArguments("20 30 32 96");
    if (!ParseGuideObject("thornyVines", defaultArguments, defaults) ||
        fabsf(defaults.damage - 0.34f) > 0.001f) {
        std::cerr << "Thorny vines did not apply their default damage rate.\n";
        return 1;
    }

    const Level waterEscape = LoadLevelFromFile(WATER_ESCAPE_LEVEL_PATH, {});
    if (waterEscape.author != "Kiril") {
        std::cerr << "Kiril's Water Escape has the wrong author metadata.\n";
        return 1;
    }
    int waterEscapeVines = 0;
    for (const GuideObject& object : waterEscape.guideObjects) {
        if (object.type == GuideObjectType::ThornyVines) ++waterEscapeVines;
    }
    if (waterEscapeVines != 3) {
        std::cerr << "Kiril's Water Escape must contain exactly three authored thorny vine clusters.\n";
        return 1;
    }

    std::cout << "All thorny vines checks passed.\n";
    return 0;
}
