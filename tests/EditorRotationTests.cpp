#include "LevelEditor.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

struct EditorRotationTestAccess {
    static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    static void Run() {
        LevelEditor editor;
        Gear gear{}; gear.center = {200, 200}; gear.radius = 32;
        editor.level.gears.push_back(gear);
        editor.selection = {LevelEditor::SelectionKind::SceneObject, 0};
        editor.selectedSceneName = "Gear 1";
        editor.transformMode = LevelEditor::TransformMode::Rotate;
        for (float zoom : {0.25f, 1.0f, 6.0f}) {
            editor.camera.zoom = zoom;
            editor.selectedSceneBounds = {};
            editor.EnsureRotationSelectionBounds();
            Check(editor.selectedSceneBounds.width == 64 && editor.selectedSceneBounds.x == 168,
                "Newly placed gears must receive real bounds before displaying their rotation ring.");
            const auto pivot = editor.SelectedRotationCenter();
            const float radius = 64 * 0.72f + 24 / zoom;
            Check(editor.TransformHandleAt({pivot.x + radius, pivot.y}) == LevelEditor::TransformHandle::RotateRing,
                "The visible ring must accept rotation drags at every zoom.");
            Check(editor.TransformHandleAt({pivot.x, pivot.y + radius}) == LevelEditor::TransformHandle::RotateRing,
                "Quarter-turn ring grips must be clickable.");
            Check(editor.TransformHandleAt(pivot) == LevelEditor::TransformHandle::AnchorPivot,
                "The anchor handle must remain distinct from the rotation ring.");
            *editor.SelectedRotation() = 45;
            Check(editor.level.gears[0].rotation == 45, "The gizmo must bind to the actual authored object angle.");
        }
        editor.HitTest({200, 200});
        Check(editor.transformMode == LevelEditor::TransformMode::Rotate,
            "Selecting another object must retain Rotate mode.");
        editor.selectedSceneName = "Steering Wheel 1";
        SteeringWheel wheel{}; wheel.center = {400, 300}; wheel.radius = 40;
        editor.level.steeringWheels.push_back(wheel);
        editor.selectedSceneBounds = {};
        editor.EnsureRotationSelectionBounds();
        Check(editor.selectedSceneBounds.width == 80 && editor.SelectedRotation() != nullptr,
            "Selection bounds must initialize across rotatable object types.");
    }
};
int main() { EditorRotationTestAccess::Run(); std::cout << "Rotation selection bounds, ring hit detection, zoom, and angle binding passed.\n"; }
