#include "LevelEditor.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <sstream>

struct EditorRotationTestAccess {
    static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    static void Run() {
        LevelEditor editor;
        using Mode = LevelEditor::TransformMode;
        Check(editor.SupportsTransform(Mode::Select) && editor.SupportsTransform(Mode::Move),
            "An empty selection must allow choosing a tool before picking an object.");
        editor.selection = {LevelEditor::SelectionKind::Exit, 0};
        editor.level.exitTrigger = {100, 200, 48, 64};
        Check(editor.SupportsTransform(Mode::Move) && !editor.SupportsTransform(Mode::Scale) &&
            !editor.SupportsTransform(Mode::Rotate), "Exit doors must move but cannot rotate or scale.");
        editor.transformMode = Mode::Scale;
        editor.UpdateTransformAvailability();
        Check(editor.transformMode == Mode::Select, "An unavailable active tool must fall back to Select.");
        editor.selection = {LevelEditor::SelectionKind::PlayerStart, 0};
        Check(editor.SupportsTransform(Mode::Move) && !editor.SupportsTransform(Mode::Scale) &&
            !editor.SupportsTransform(Mode::Rotate), "Player Start must only support Select and Move.");
        Gear gear{}; gear.center = {200, 200}; gear.radius = 32;
        editor.level.gears.push_back(gear);
        editor.selection = {LevelEditor::SelectionKind::SceneObject, 0};
        editor.selectedSceneName = "Gear 1";
        Check(editor.SupportsTransform(Mode::Move) && editor.SupportsTransform(Mode::Scale) &&
            editor.SupportsTransform(Mode::Rotate), "Gears must support all four gizmos.");
        Check(editor.level.gears[0].radius == 32 && editor.level.gears[0].center.x == 200,
            "Checking capabilities must never change object dimensions or position.");
        editor.groupSelection = {{LevelEditor::SelectionKind::SceneObject, 0, "Gear 1"},
            {LevelEditor::SelectionKind::Exit, 0}};
        Check(editor.SupportsTransform(Mode::Move) && !editor.SupportsTransform(Mode::Scale) &&
            !editor.SupportsTransform(Mode::Rotate), "Mixed groups must only enable tools supported by every member.");
        Check(editor.selectedSceneName == "Gear 1" && editor.selection.kind == LevelEditor::SelectionKind::SceneObject,
            "Group capability checks must preserve the primary selection.");
        editor.groupSelection.clear();
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
            Check(editor.TransformHandleAt(pivot) != LevelEditor::TransformHandle::AnchorPivot,
                "Pivot dragging must be locked by default.");
            editor.pivotToggledThisFrame = false;
            editor.TogglePivotLock();
            Check(editor.TransformHandleAt(pivot) == LevelEditor::TransformHandle::AnchorPivot,
                "Unlocking the pivot must enable its distinct drag handle.");
            editor.TogglePivotLock();
            Check(editor.pivotUnlocked, "Duplicate Insert routing in one frame must not toggle the lock twice.");
            editor.pivotToggledThisFrame = false;
            editor.TogglePivotLock();
            Check(!editor.pivotUnlocked, "Pressing Insert again must lock pivot dragging.");
            *editor.SelectedRotation() = 45;
            Check(editor.level.gears[0].rotation == 45, "The gizmo must bind to the actual authored object angle.");
        }
        editor.transformMode = LevelEditor::TransformMode::Scale;
        for (float zoom : {0.25f, 1.0f, 6.0f}) {
            editor.camera.zoom = zoom;
            const auto bounds = editor.selectedSceneBounds;
            const auto handles = editor.ScaleHandlePositions(bounds);
            Check(handles[0].x > bounds.x + bounds.width && handles[1].y < bounds.y,
                "Scale axis tips must extend beyond the selection outline.");
            Check(handles[2].x > bounds.x + bounds.width && handles[2].y > bounds.y + bounds.height,
                "The uniform scale grip must also sit outside the selection outline.");
            Check(editor.TransformHandleAt(handles[0]) == LevelEditor::TransformHandle::ScaleX &&
                editor.TransformHandleAt(handles[1]) == LevelEditor::TransformHandle::ScaleY &&
                editor.TransformHandleAt(handles[2]) == LevelEditor::TransformHandle::ScaleBottomRight,
                "Extended grips must remain clickable at every zoom.");
        }
        editor.transformMode = LevelEditor::TransformMode::Rotate;
        GuideObject rock{};
        std::istringstream rockArgs("500 300 32 32");
        Check(ParseGuideObject("rock", rockArgs, rock), "Rock placement must parse.");
        editor.level.guideObjects.push_back(rock);
        editor.selectedSceneName = "Guide Object 1";
        editor.selectedSceneBounds = {};
        editor.EnsureRotationSelectionBounds();
        Check(editor.selectedSceneBounds.x == 500 && editor.selectedSceneBounds.width == 32,
            "Rock rotation handles must use its actual placement bounds.");
        *editor.SelectedRotation() = 60;
        Check(editor.level.guideObjects[0].transform.rotation == 60, "Rock rotation must edit the authored angle.");
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
