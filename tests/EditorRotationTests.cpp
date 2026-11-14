#include "LevelEditor.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <sstream>

struct EditorRotationTestAccess {
    static void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            throw std::runtime_error(message);
        }
    }
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
        Gear secondGear{}; secondGear.center = {300, 200}; secondGear.radius = 32;
        editor.level.gears.push_back(secondGear);
        editor.groupSelection = {
            {LevelEditor::SelectionKind::SceneObject, 0, "Gear 1", {168, 168, 64, 64}},
            {LevelEditor::SelectionKind::SceneObject, 1, "Gear 2", {268, 168, 64, 64}}
        };
        editor.selection = editor.groupSelection.front();
        editor.selectedSceneName = "Gear 1";
        editor.selectedSceneBounds = editor.groupSelection.front().sceneBounds;
        Check(editor.SupportsTransform(Mode::Rotate), "Groups of rotatable objects must allow group rotation.");
        editor.transformMode = Mode::Rotate;
        const Vector2 groupPivot = editor.SelectedRotationCenter();
        const Rectangle groupBounds = editor.SelectionGroupBounds();
        const float groupRingRadius = std::max(groupBounds.width, groupBounds.height) * 0.72f + 24.0f;
        Check(editor.TransformHandleAt({groupPivot.x + groupRingRadius, groupPivot.y}) ==
            LevelEditor::TransformHandle::RotateRing, "The group rotation ring must be interactive.");
        editor.CaptureGroupTransformState();
        editor.ApplyGroupRotation(90.0f);
        Check(std::abs(editor.level.gears[0].center.x - 250.0f) < 0.01f &&
            std::abs(editor.level.gears[0].center.y - 150.0f) < 0.01f &&
            std::abs(editor.level.gears[1].center.x - 250.0f) < 0.01f &&
            std::abs(editor.level.gears[1].center.y - 250.0f) < 0.01f,
            "Group rotation must orbit each object around the shared pivot.");
        Check(editor.level.gears[0].rotation == 90.0f && editor.level.gears[1].rotation == 90.0f,
            "Group rotation must update every selected object's authored angle.");
        editor.level.gears[0].center = {200, 200};
        editor.level.gears[0].rotation = 0.0f;
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
        LevelEditor doorEditor;
        TrapDoor hatch{}; hatch.hinge = {100, 100};
        TrapDoor plank{}; plank.hinge = {400, 200}; plank.kind = HingedDoorKind::SimpleHingedDoor;
        doorEditor.level.trapDoors = {hatch, plank};
        const auto doorHit = doorEditor.HitTest({410, 200});
        Check(doorHit.sceneName == "Simple Hinged Door 2" && doorHit.outlinerIndex == 0,
            "New door selection must retain its own type and per-type outliner index.");
        doorEditor.selection = doorHit;
        Check(doorEditor.SupportsTransform(Mode::Move) && doorEditor.SupportsTransform(Mode::Scale) &&
            doorEditor.SupportsTransform(Mode::Rotate), "New doors must support all editor transforms.");
        *doorEditor.SelectedRotation() = 76;
        Check(doorEditor.level.trapDoors[1].angle == 76 && doorEditor.level.trapDoors[0].angle == 0,
            "Rotating a new door must not affect the old hatch.");
        Check(doorEditor.DuplicateSelectedSceneObject(), "New door must support duplication.");
        Check(doorEditor.level.trapDoors.back().kind == HingedDoorKind::SimpleHingedDoor &&
            doorEditor.selection.outlinerIndex == 1, "Duplication must retain distinct type and ordinal.");

        Level passageLevel;
        VisualTile passageTile{};
        passageTile.layer = TileLayer::Collision;
        passageTile.column = 2;
        passageTile.row = 1;
        passageTile.sheetIndex = 3;
        passageLevel.tilePassages.push_back({3, 2, 1, 15, 50});
        for (const float zoom : {0.25f, 0.5f, 1.0f, 2.0f}) {
            const float worldWidth = GetTilePassageViewportStrokeWidth(passageLevel, passageTile, zoom);
            Check(std::fabs(worldWidth * zoom - 50.0f) < 0.01f,
                "Viewport passage width must remain 50 screen pixels at every zoom.");
        }
        passageLevel.tilePassages[0].lineWidth = 24;
        Check(std::fabs(GetTilePassageViewportStrokeWidth(passageLevel, passageTile, 0.5f) * 0.5f - 24.0f) < 0.01f,
            "The configured passage width must affect viewport strokes.");
    }
};
int main() { EditorRotationTestAccess::Run(); std::cout << "Rotation, transform, and Tile Passage viewport-width checks passed.\n"; }
