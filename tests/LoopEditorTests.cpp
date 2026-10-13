#include "Level.h"
#include "EditorLoops.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }
int main() {
    auto explicitLevel = LoadLevelFromFile(LOOP_EXPLICIT_PATH, {});
    Check(explicitLevel.buttons.size() == 2, "Explicit riders must not create duplicate buttons.");
    Check(explicitLevel.buttonPlatformLoops.size() == 1 && explicitLevel.platformLoopButtonLinks.size() == 1,
        "Loop and rider records must load.");
    Check(explicitLevel.buttonExitLink.buttonIndex == 1, "A riding button must retain its existing control links.");
    const auto& platform = explicitLevel.buttonPlatformLoops[0].platforms[2];
    auto& button = explicitLevel.buttons[1];
    Check(Near(button.rect.width, 24) && Near(button.rect.height, 12), "Explicit rider sizes must load.");
    Check(Near(button.rect.x, platform.x + (platform.width - 24) * 0.5f) &&
        Near(button.rect.y, platform.y - 12 + 4), "Riders must resolve after all button records load.");
    explicitLevel.buttonPlatformLoops[0].phase = 90;
    UpdateButtonPlatformLoopPositions(explicitLevel.buttonPlatformLoops[0]);
    UpdatePlatformLoopButtonPositions(explicitLevel);
    const auto& movedPlatform = explicitLevel.buttonPlatformLoops[0].platforms[2];
    Check(Near(button.rect.x, movedPlatform.x + (movedPlatform.width - 24) * 0.5f), "Riders must follow loop movement.");
    auto legacy = LoadLevelFromFile(LOOP_LEGACY_PATH, {});
    Check(legacy.buttons.size() == 3 && legacy.platformLoopButtonLinks[0].buttonIndex == 1,
        "Legacy riding records must still create one button at their original runtime index.");
    Check(legacy.buttonPlatformLoops[0].active && !explicitLevel.buttonPlatformLoops[0].active,
        "Independent loops run immediately; button-controlled loops await activation.");
    auto editing = explicitLevel;
    const auto originalBounds = EditorLoopBounds(editing.buttonPlatformLoops[0]);
    const auto originalSize = editing.buttonPlatformLoops[0].platformSize;
    const auto originalButton = editing.buttons[1].rect;
    MoveEditorLoop(editing, 0, {32, 64});
    Check(Near(editing.buttons[1].rect.x, originalButton.x + 32) && Near(editing.buttons[1].rect.y, originalButton.y + 64),
        "Moving a loop must move its attached buttons exactly once.");
    const auto movedBounds = EditorLoopBounds(editing.buttonPlatformLoops[0]);
    ResizeEditorLoop(editing, 0, {80, 120}, false);
    const auto resizedBounds = EditorLoopBounds(editing.buttonPlatformLoops[0]);
    Check(Near(resizedBounds.x, movedBounds.x) && Near(resizedBounds.y, movedBounds.y) &&
        Near(resizedBounds.width, movedBounds.width + 80) && Near(resizedBounds.height, movedBounds.height + 120),
        "Ordinary resize must preserve the top-left anchor and grow the path.");
    Check(Near(editing.buttonPlatformLoops[0].platformSize.x, originalSize.x), "Resizing the path must preserve platform size.");
    const auto center = editing.buttonPlatformLoops[0].center;
    ResizeEditorLoop(editing, 0, {32, 32}, true);
    Check(Near(editing.buttonPlatformLoops[0].center.x, center.x) && Near(editing.buttonPlatformLoops[0].center.y, center.y),
        "Centered resize must preserve loop center.");
    const auto platformHit = editing.buttonPlatformLoops[0].platforms[0];
    Check(HitEditorLoop(editing.buttonPlatformLoops[0], {platformHit.x + 1, platformHit.y + 1}, 3), "Loop platforms must be selectable.");
    const auto pathHit = GetButtonPlatformLoopPoint(editing.buttonPlatformLoops[0], 0.13f);
    Check(HitEditorLoop(editing.buttonPlatformLoops[0], pathHit, 3), "Loop paths must be selectable between platforms.");
    Check(!HitEditorLoop(editing.buttonPlatformLoops[0], {-1000, -1000}, 3), "Empty canvas must not select loops.");
    editing.buttonPlatformLoops[0].buttonIndex = 1;
    const auto originalCenter = editing.buttonPlatformLoops[0].center;
    const auto buttonCount = editing.buttons.size();
    const int duplicate = DuplicateEditorLoop(editing, 0, {32, 32});
    Check(duplicate == 1 && editing.buttons.size() == buttonCount + 1 && editing.platformLoopButtonLinks.size() == 2,
        "Duplicating a loop must copy its riders exactly once.");
    Check(editing.platformLoopButtonLinks[1].loopIndex == duplicate && editing.platformLoopButtonLinks[1].buttonIndex == static_cast<int>(buttonCount),
        "Copied riding links must reference copied loop and copied button.");
    Check(editing.buttonPlatformLoops[1].buttonIndex == static_cast<int>(buttonCount), "Self-activating riders must remap to the copied button.");
    Check(Near(editing.buttonPlatformLoops[0].center.x, originalCenter.x), "Duplication must preserve the original loop.");
    Check(DeleteEditorLoop(editing, 0) && editing.platformLoopButtonLinks.size() == 1 && editing.platformLoopButtonLinks[0].loopIndex == 0,
        "Deleting a loop must remove its riding links and remap surviving loops.");
    Check(editing.buttons.size() == buttonCount + 1, "Deleting loops must preserve detached buttons.");
    ResizeEditorLoop(editing, 0, {-10000, -10000}, false);
    Check(editing.buttonPlatformLoops[0].radius.x >= 1 && editing.buttonPlatformLoops[0].radius.y >= 1,
        "Resizing must clamp path sizes to positive values.");
    std::cout << "Loop editor loading, transforms, duplication, deletion, riding motion, and legacy compatibility passed.\n";
}
