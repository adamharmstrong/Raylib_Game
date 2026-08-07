#include "LevelEditor.h"

#include "GuideObjects.h"
#include "Render.h"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <utility>

namespace {
constexpr int InitialWindowWidth = 1440;
constexpr int InitialWindowHeight = 900;
constexpr float ToolbarHeight = 52.0f;
constexpr float FilePanelWidth = 232.0f;
constexpr float InspectorWidth = 288.0f;
constexpr float StatusBarHeight = 30.0f;
constexpr float FileRowHeight = 28.0f;
constexpr float FileListTop = 92.0f;

constexpr Color WindowBackground{20, 23, 29, 255};
constexpr Color PanelBackground{29, 33, 41, 255};
constexpr Color PanelBorder{55, 62, 74, 255};
constexpr Color CanvasBackground{15, 18, 23, 255};
constexpr Color PrimaryText{226, 231, 239, 255};
constexpr Color SecondaryText{145, 154, 171, 255};
constexpr Color Accent{69, 154, 255, 255};

bool IsUsableRectangle(Rectangle rect) {
    return rect.width > 0.0f && rect.height > 0.0f;
}

void DrawPreviewRectangle(Rectangle rect, Color fill, Color outline, float lineWidth) {
    if (!IsUsableRectangle(rect)) {
        return;
    }

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, lineWidth, outline);
}

void DrawCenteredSegment(Vector2 center, float length, float angleDegrees, float thickness, Color color) {
    const float radians = angleDegrees * DEG2RAD;
    const Vector2 half{
        cosf(radians) * length * 0.5f,
        sinf(radians) * length * 0.5f
    };
    DrawLineEx(
        {center.x - half.x, center.y - half.y},
        {center.x + half.x, center.y + half.y},
        thickness,
        color
    );
}

Color FluidPreviewColor(FluidType type) {
    switch (type) {
    case FluidType::Water: return Color{55, 150, 235, 255};
    case FluidType::Sand: return Color{220, 174, 70, 255};
    case FluidType::Gel: return Color{124, 91, 220, 255};
    case FluidType::Gas: return Color{104, 213, 126, 255};
    }

    return SKYBLUE;
}

Color WorldLayerPreviewColor(WorldLayer layer) {
    switch (layer) {
    case WorldLayer::Background: return Color{83, 145, 235, 235};
    case WorldLayer::Middleground: return Color{92, 224, 157, 235};
    case WorldLayer::Foreground: return Color{224, 105, 214, 235};
    }
    return RAYWHITE;
}

float MeasureUiText(Font font, const char* text, float fontSize) {
    return MeasureTextEx(font, text, fontSize, 1.0f).x;
}

void DrawUiText(Font font, const char* text, float x, float y, float fontSize, Color color) {
    DrawTextEx(font, text, {x, y}, fontSize, 1.0f, color);
}

void DrawToolbarButton(Rectangle bounds, const char* label, Font font) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, 0.18f, 6, hovered ? Color{59, 69, 84, 255} : Color{43, 49, 60, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, hovered ? Accent : PanelBorder);
    const float textWidth = MeasureUiText(font, label, 18.0f);
    DrawUiText(font, label, bounds.x + (bounds.width - textWidth) * 0.5f,
        bounds.y + 7.0f, 18.0f, PrimaryText);
}
}

int LevelEditor::Run(const std::filesystem::path& initialLevel) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(InitialWindowWidth, InitialWindowHeight, "Power Pulley Panic - Level Editor");
    SetWindowMinSize(960, 600);
    SetTargetFPS(120);

    uiFont = GetFontDefault();
    const std::filesystem::path fontCandidates[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    for (const std::filesystem::path& fontPath : fontCandidates) {
        std::error_code fontError;
        if (!std::filesystem::is_regular_file(fontPath, fontError)) {
            continue;
        }

        Font candidate = LoadFontEx(fontPath.string().c_str(), 32, nullptr, 0);
        if (candidate.texture.id != 0 && candidate.texture.id != GetFontDefault().texture.id && candidate.glyphCount > 0) {
            uiFont = candidate;
            ownsUiFont = true;
            SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }

    camera.zoom = 1.0f;
    camera.rotation = 0.0f;
    levelDirectory = FindLevelDirectory();
    DiscoverLevels();

    std::filesystem::path levelToLoad = initialLevel;
    if (!levelToLoad.empty() && levelToLoad.is_relative() && !std::filesystem::exists(levelToLoad)) {
        levelToLoad = levelDirectory / levelToLoad;
    }
    if (levelToLoad.empty()) {
        const auto testLevel = std::find_if(levelFiles.begin(), levelFiles.end(), [](const auto& path) {
            return path.filename() == "test_level.level";
        });
        if (testLevel != levelFiles.end()) {
            levelToLoad = *testLevel;
        }
        else if (!levelFiles.empty()) {
            levelToLoad = levelFiles.front();
        }
    }

    if (!levelToLoad.empty()) {
        LoadLevel(levelToLoad);
    }
    else {
        FrameLevel();
        statusText = "No .level files were found under game_data/levels.";
    }

    while (!WindowShouldClose()) {
        Update();
        Draw();
    }

    if (ownsUiFont) {
        UnloadFont(uiFont);
    }
    CloseWindow();
    return 0;
}

std::filesystem::path LevelEditor::FindLevelDirectory() const {
    std::error_code error;
    std::filesystem::path candidate = std::filesystem::current_path(error);
    if (error) {
        return "game_data/levels";
    }

    for (int depth = 0; depth < 5; depth++) {
        const std::filesystem::path levelPath = candidate / "game_data" / "levels";
        if (std::filesystem::is_directory(levelPath, error)) {
            return levelPath;
        }
        candidate = candidate.parent_path();
    }

    return std::filesystem::current_path() / "game_data" / "levels";
}

void LevelEditor::DiscoverLevels() {
    levelFiles.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(levelDirectory, error)) {
        return;
    }

    for (std::filesystem::directory_iterator iterator(levelDirectory, error), end;
        iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && iterator->path().extension() == ".level") {
            levelFiles.push_back(iterator->path());
        }
    }

    std::sort(levelFiles.begin(), levelFiles.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });
}

bool LevelEditor::LoadLevel(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || path.extension() != ".level") {
        statusText = "Could not load: choose an existing .level file.";
        return false;
    }

    level = LoadLevelFromFile(path.string(), Level{});
    currentLevelPath = std::filesystem::weakly_canonical(path, error);
    if (error) {
        currentLevelPath = path;
    }

    levelDirectory = currentLevelPath.parent_path();
    DiscoverLevels();
    fileScroll = 0;
    FrameLevel();
    statusText = "Loaded " + currentLevelPath.filename().string() + " (preview mode; editing and saving come next).";
    const std::string title = "Power Pulley Panic - Level Editor - " + currentLevelPath.filename().string();
    SetWindowTitle(title.c_str());
    return true;
}

Rectangle LevelEditor::GetCanvasBounds() const {
    return {
        FilePanelWidth,
        ToolbarHeight,
        std::max(1.0f, static_cast<float>(GetScreenWidth()) - FilePanelWidth - InspectorWidth),
        std::max(1.0f, static_cast<float>(GetScreenHeight()) - ToolbarHeight - StatusBarHeight)
    };
}

Rectangle LevelEditor::GetFitButtonBounds() const {
    return {FilePanelWidth + 14.0f, 10.0f, 74.0f, 32.0f};
}

Rectangle LevelEditor::GetReloadButtonBounds() const {
    return {FilePanelWidth + 98.0f, 10.0f, 88.0f, 32.0f};
}

int LevelEditor::VisibleFileRows() const {
    return std::max(1, static_cast<int>((GetScreenHeight() - FileListTop - StatusBarHeight - 10.0f) / FileRowHeight));
}

void LevelEditor::FrameLevel() {
    const Rectangle canvas = GetCanvasBounds();
    const Rectangle bounds = IsUsableRectangle(level.worldBounds)
        ? level.worldBounds
        : Rectangle{0.0f, 0.0f, 1600.0f, 900.0f};

    camera.offset = {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
    camera.target = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
    camera.zoom = std::clamp(
        std::min(canvas.width / bounds.width, canvas.height / bounds.height) * 0.9f,
        0.05f,
        8.0f
    );
}

void LevelEditor::Update() {
    const Rectangle canvas = GetCanvasBounds();
    camera.offset = {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
    const Vector2 mouse = GetMousePosition();

    if (IsKeyPressed(KEY_F) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, GetFitButtonBounds()))) {
        FrameLevel();
    }
    if ((!currentLevelPath.empty() && IsKeyPressed(KEY_R)) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, GetReloadButtonBounds()))) {
        if (!currentLevelPath.empty()) {
            LoadLevel(currentLevelPath);
        }
    }

    const Rectangle filePanel{0.0f, ToolbarHeight, FilePanelWidth, GetScreenHeight() - ToolbarHeight - StatusBarHeight};
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, filePanel)) {
        const int maximumScroll = std::max(0, static_cast<int>(levelFiles.size()) - VisibleFileRows());
        fileScroll = std::clamp(fileScroll - static_cast<int>(wheel), 0, maximumScroll);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, filePanel) && mouse.y >= FileListTop) {
        const int visibleIndex = static_cast<int>((mouse.y - FileListTop) / FileRowHeight);
        const int fileIndex = fileScroll + visibleIndex;
        if (visibleIndex >= 0 && visibleIndex < VisibleFileRows() &&
            fileIndex >= 0 && fileIndex < static_cast<int>(levelFiles.size())) {
            LoadLevel(levelFiles[static_cast<size_t>(fileIndex)]);
        }
    }

    if (CheckCollisionPointRec(mouse, canvas)) {
        if (wheel != 0.0f) {
            const Vector2 worldBeforeZoom = GetScreenToWorld2D(mouse, camera);
            camera.zoom = std::clamp(camera.zoom * (1.0f + wheel * 0.12f), 0.05f, 8.0f);
            const Vector2 worldAfterZoom = GetScreenToWorld2D(mouse, camera);
            camera.target.x += worldBeforeZoom.x - worldAfterZoom.x;
            camera.target.y += worldBeforeZoom.y - worldAfterZoom.y;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 delta = GetMouseDelta();
            camera.target.x -= delta.x / camera.zoom;
            camera.target.y -= delta.y / camera.zoom;
        }
    }

    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        for (unsigned int index = 0; index < droppedFiles.count; index++) {
            const std::filesystem::path droppedPath = droppedFiles.paths[index];
            if (droppedPath.extension() == ".level" && LoadLevel(droppedPath)) {
                break;
            }
        }
        UnloadDroppedFiles(droppedFiles);
    }
}

void LevelEditor::Draw() const {
    BeginDrawing();
    ClearBackground(WindowBackground);

    DrawCanvas(GetCanvasBounds());
    DrawToolbar();
    DrawFilePanel();
    DrawInspector();
    DrawStatusBar();

    EndDrawing();
}

void LevelEditor::DrawCanvas(Rectangle canvas) const {
    DrawRectangleRec(canvas, CanvasBackground);
    BeginScissorMode(static_cast<int>(canvas.x), static_cast<int>(canvas.y),
        static_cast<int>(canvas.width), static_cast<int>(canvas.height));
    BeginMode2D(camera);

    const Vector2 topLeft = GetScreenToWorld2D({canvas.x, canvas.y}, camera);
    const Vector2 bottomRight = GetScreenToWorld2D({canvas.x + canvas.width, canvas.y + canvas.height}, camera);
    const float gridSize = camera.zoom < 0.18f ? 160.0f : (camera.zoom < 0.4f ? 64.0f : 32.0f);
    const float lineWidth = 1.0f / camera.zoom;
    const float startX = floorf(topLeft.x / gridSize) * gridSize;
    const float startY = floorf(topLeft.y / gridSize) * gridSize;

    for (float x = startX; x <= bottomRight.x; x += gridSize) {
        const bool major = static_cast<int>(roundf(x / gridSize)) % 5 == 0;
        DrawLineEx({x, topLeft.y}, {x, bottomRight.y}, lineWidth,
            major ? Color{48, 55, 66, 170} : Color{35, 40, 49, 150});
    }
    for (float y = startY; y <= bottomRight.y; y += gridSize) {
        const bool major = static_cast<int>(roundf(y / gridSize)) % 5 == 0;
        DrawLineEx({topLeft.x, y}, {bottomRight.x, y}, lineWidth,
            major ? Color{48, 55, 66, 170} : Color{35, 40, 49, 150});
    }

    DrawLevelPreview();
    EndMode2D();
    EndScissorMode();
    DrawRectangleLinesEx(canvas, 1.0f, PanelBorder);
}

void LevelEditor::DrawLevelPreview() const {
    const float lineWidth = std::max(1.0f / camera.zoom, 1.5f / camera.zoom);

    DrawRectangleRec(level.worldBounds, Color{25, 29, 36, 160});

    for (const Rectangle darkness : level.darknessAreas) {
        DrawPreviewRectangle(darkness, Color{28, 23, 43, 145}, Color{95, 75, 138, 210}, lineWidth);
    }
    for (const VisualTile& tile : level.visualTiles) {
        const Color tileColor = tile.layer == TileLayer::Foreground ? Color{104, 113, 127, 170} :
            (tile.layer == TileLayer::Background ? Color{74, 84, 101, 115} : Color{52, 61, 76, 90});
        DrawPreviewRectangle({tile.position.x, tile.position.y, 32.0f, 32.0f}, tileColor,
            Fade(tileColor, 0.9f), 0.5f / camera.zoom);
    }
    for (const FluidField& fluid : level.fluids) {
        const Color color = FluidPreviewColor(fluid.type);
        DrawPreviewRectangle(fluid.bounds, Fade(color, 0.27f), Fade(color, 0.95f), lineWidth);
    }
    for (const Rectangle solid : level.baseSolids) {
        DrawPreviewRectangle(solid, Color{82, 91, 106, 220}, Color{166, 176, 192, 255}, lineWidth);
    }
    for (const Rectangle platform : level.pitPlatforms) {
        DrawPreviewRectangle(platform, Color{66, 119, 151, 210}, Color{105, 196, 235, 255}, lineWidth);
    }
    for (const Rectangle ladder : level.ladders) {
        DrawPreviewRectangle(ladder, Color{167, 126, 47, 90}, Color{235, 190, 79, 255}, lineWidth);
    }
    for (const Rectangle zone : level.cameraZones) {
        DrawRectangleLinesEx(zone, lineWidth * 1.5f, Color{86, 190, 230, 210});
    }

    DrawPreviewRectangle(level.spikeHazard, Color{170, 50, 58, 180}, Color{255, 92, 100, 255}, lineWidth);
    DrawPreviewRectangle(level.exitTrigger, Color{93, 185, 102, 95}, Color{105, 234, 125, 255}, lineWidth);
    DrawCircleV(level.playerStart, 13.0f, Color{255, 205, 82, 255});
    DrawCircleLinesV(level.playerStart, 17.0f, Color{255, 235, 171, 255});

    for (const Vector2 pulley : level.pulleys) {
        DrawCircleLinesV(pulley, 18.0f, Color{225, 183, 74, 255});
        DrawCircleV(pulley, 4.0f, Color{225, 183, 74, 255});
    }
    for (const HangingWeight& weight : level.weights) {
        const Rectangle preview{
            weight.pulley.x - weight.rect.width * 0.5f,
            weight.pulley.y + weight.pulleyRadius + 34.0f,
            weight.rect.width,
            weight.rect.height
        };
        DrawLineEx(weight.pulley, {weight.pulley.x, preview.y}, lineWidth, Color{190, 197, 207, 255});
        DrawPreviewRectangle(preview, Color{128, 103, 72, 230}, Color{231, 184, 103, 255}, lineWidth);
    }
    for (const RotaryLatch& latch : level.rotaryLatches) {
        DrawCircleLinesV(latch.center, latch.radius, Color{200, 132, 255, 255});
        const Vector2 spoke{
            latch.center.x + cosf(latch.angle * DEG2RAD) * latch.radius,
            latch.center.y + sinf(latch.angle * DEG2RAD) * latch.radius
        };
        DrawLineEx(latch.center, spoke, lineWidth * 2.0f, Color{200, 132, 255, 255});
    }
    for (const StoneBlock& block : level.stoneBlocks) {
        DrawPreviewRectangle(block.rect, Color{143, 99, 58, 220}, Color{236, 159, 82, 255}, lineWidth);
        DrawRectangleLinesEx(block.rect, lineWidth * 2.0f, WorldLayerPreviewColor(block.layer));
    }
    for (const Boulder& boulder : level.boulders) {
        DrawCircleV(boulder.center, boulder.radius, Color{119, 100, 84, 220});
        DrawCircleLinesV(boulder.center, boulder.radius, Color{226, 169, 106, 255});
        DrawCircleLinesV(boulder.center, boulder.radius + lineWidth * 2.0f, WorldLayerPreviewColor(boulder.layer));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        DrawCircleLinesV(wheel.center, wheel.radius, Color{120, 199, 235, 255});
        DrawCircleLinesV(wheel.center, wheel.radius + lineWidth * 2.0f, WorldLayerPreviewColor(wheel.layer));
    }
    for (const Gear& gear : level.gears) {
        DrawGear(gear);
        if (gear.orientation == GearOrientation::Horizontal) {
            DrawEllipseLines(static_cast<int>(gear.center.x), static_cast<int>(gear.center.y),
                gear.radius * GearOuterRadiusScale, gear.radius * 0.48f,
                WorldLayerPreviewColor(gear.layer));
        }
        else {
            DrawCircleLinesV(gear.center, gear.radius * GearOuterRadiusScale,
                WorldLayerPreviewColor(gear.layer));
        }
    }
    if (level.clockFaceRadius > 0.0f) {
        DrawCircleV(level.clockFaceCenter, level.clockFaceRadius + 5.0f, Color{171, 129, 53, 255});
        DrawCircleV(level.clockFaceCenter, level.clockFaceRadius, Color{219, 210, 181, 255});
        DrawCircleLinesV(level.clockFaceCenter, level.clockFaceRadius, Color{34, 39, 42, 255});
        const auto handAngle = [&](ClockHandType hand) {
            for (const Gear& gear : level.gears) {
                if (gear.clockHand == hand) return gear.rotation;
            }
            return 270.0f;
        };
        const auto drawHand = [&](ClockHandType hand, float length, float thickness, Color color) {
            const float angle = handAngle(hand) * DEG2RAD;
            DrawLineEx(level.clockFaceCenter,
                {level.clockFaceCenter.x + cosf(angle) * length, level.clockFaceCenter.y + sinf(angle) * length},
                thickness, color);
        };
        drawHand(ClockHandType::Hour, level.clockFaceRadius * 0.48f, lineWidth * 5.0f, Color{45, 52, 55, 255});
        drawHand(ClockHandType::Minute, level.clockFaceRadius * 0.69f, lineWidth * 3.5f, Color{45, 52, 55, 255});
        drawHand(ClockHandType::Second, level.clockFaceRadius * 0.82f, lineWidth * 2.0f, Color{180, 48, 40, 255});
    }
    for (const Flywheel& flywheel : level.flywheels) {
        DrawCircleLinesV(flywheel.center, flywheel.radius, Color{240, 158, 65, 255});
        DrawCircleLinesV(flywheel.center, flywheel.radius + lineWidth * 2.0f,
            WorldLayerPreviewColor(flywheel.layer));
    }
    for (const SteeringWheel& wheel : level.steeringWheels) {
        DrawCircleLinesV(wheel.center, wheel.radius, Color{231, 182, 90, 255});
    }
    for (const Screw& screw : level.screws) {
        DrawCenteredSegment(screw.center, screw.length, screw.angle, lineWidth * 5.0f, Color{206, 171, 102, 255});
        DrawCenteredSegment(screw.center, screw.length, screw.angle, lineWidth * 1.5f,
            WorldLayerPreviewColor(screw.layer));
    }
    for (const Fan& fan : level.fans) {
        const Vector2 end{fan.center.x + fan.direction.x * fan.length, fan.center.y + fan.direction.y * fan.length};
        DrawLineEx(fan.center, end, lineWidth * 3.0f, Color{90, 188, 236, 255});
        DrawCircleV(fan.center, 8.0f, Color{90, 188, 236, 255});
    }
    for (const Pinwheel& pinwheel : level.pinwheels) {
        DrawCircleLinesV(pinwheel.center, pinwheel.radius, Color{94, 208, 236, 255});
    }
    for (const Ramp& ramp : level.ramps) {
        DrawCenteredSegment(ramp.center, ramp.length, ramp.angle, std::max(ramp.thickness, lineWidth), Color{197, 139, 81, 255});
    }
    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawCenteredSegment(seeSaw.pivot, seeSaw.length, seeSaw.angle, std::max(seeSaw.thickness, lineWidth), Color{139, 184, 230, 255});
        DrawCircleV(seeSaw.pivot, 7.0f, Color{232, 217, 160, 255});
    }
    for (const TrapDoor& door : level.trapDoors) {
        const Vector2 end{
            door.hinge.x + cosf(door.angle * DEG2RAD) * door.length,
            door.hinge.y + sinf(door.angle * DEG2RAD) * door.length
        };
        DrawLineEx(door.hinge, end, std::max(door.thickness, lineWidth), Color{188, 128, 75, 255});
    }
    for (const Chain& chain : level.chains) {
        DrawLineEx(chain.start, chain.end, lineWidth * 2.0f, Color{135, 157, 185, 255});
    }
    for (const PhysicsRope& rope : level.physicsRopes) {
        DrawLineEx(rope.start, rope.end, std::max(rope.thickness, lineWidth), Color{183, 139, 91, 255});
    }
    for (const Button& button : level.buttons) {
        DrawPreviewRectangle(button.rect, Color{168, 58, 69, 220}, Color{244, 103, 113, 255}, lineWidth);
    }
    for (const ArrowTrap& trap : level.arrowTraps) {
        DrawCircleV(trap.position, 8.0f, Color{235, 75, 85, 255});
        DrawLineEx(trap.position,
            {trap.position.x + trap.direction.x * 34.0f, trap.position.y + trap.direction.y * 34.0f},
            lineWidth * 2.0f, Color{235, 75, 85, 255});
    }
    for (const BreakableTile& tile : level.breakableTiles) {
        DrawPreviewRectangle(tile.rect, Color{150, 102, 68, 185}, Color{245, 168, 92, 255}, lineWidth);
    }
    for (const Enemy& enemy : level.enemies) {
        DrawPreviewRectangle(enemy.rect, Color{155, 50, 65, 195}, Color{250, 91, 109, 255}, lineWidth);
        DrawLineEx({enemy.patrolMinX, enemy.rect.y + enemy.rect.height + 5.0f},
            {enemy.patrolMaxX, enemy.rect.y + enemy.rect.height + 5.0f}, lineWidth,
            Color{250, 91, 109, 190});
    }
    for (const GuideObject& object : level.guideObjects) {
        DrawPreviewRectangle(GetGuideObjectBounds(object), Color{78, 142, 109, 105},
            WorldLayerPreviewColor(object.layer), lineWidth * 1.5f);
    }
    for (const LevelLabel& label : level.labels) {
        DrawUiText(uiFont, label.text.c_str(), label.position.x, label.position.y,
            fmaxf(10.0f, label.fontSize * 0.75f), Color{238, 231, 195, 255});
    }

    DrawRectangleLinesEx(level.worldBounds, lineWidth * 2.0f, Color{218, 225, 235, 235});
}

void LevelEditor::DrawToolbar() const {
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(ToolbarHeight), PanelBackground);
    DrawLine(0, static_cast<int>(ToolbarHeight - 1.0f), GetScreenWidth(),
        static_cast<int>(ToolbarHeight - 1.0f), PanelBorder);
    DrawUiText(uiFont, "LEVEL EDITOR", 16.0f, 15.0f, 22.0f, PrimaryText);
    DrawToolbarButton(GetFitButtonBounds(), "Fit (F)", uiFont);
    DrawToolbarButton(GetReloadButtonBounds(), "Reload", uiFont);

    const std::string fileName = currentLevelPath.empty() ? "No level loaded" : currentLevelPath.filename().string();
    DrawUiText(uiFont, fileName.c_str(), FilePanelWidth + 204.0f, 17.0f, 18.0f, SecondaryText);
    DrawUiText(uiFont, "Preview foundation", GetScreenWidth() - InspectorWidth + 18.0f, 17.0f, 18.0f, Accent);
}

void LevelEditor::DrawFilePanel() const {
    const Rectangle panel{0.0f, ToolbarHeight, FilePanelWidth, GetScreenHeight() - ToolbarHeight - StatusBarHeight};
    DrawRectangleRec(panel, PanelBackground);
    DrawLine(static_cast<int>(FilePanelWidth - 1.0f), static_cast<int>(ToolbarHeight),
        static_cast<int>(FilePanelWidth - 1.0f), GetScreenHeight(), PanelBorder);
    DrawUiText(uiFont, "LEVEL FILES", 16.0f, 65.0f, 17.0f, SecondaryText);

    const int rowCount = std::min(VisibleFileRows(), static_cast<int>(levelFiles.size()) - fileScroll);
    for (int visibleIndex = 0; visibleIndex < std::max(0, rowCount); visibleIndex++) {
        const int fileIndex = fileScroll + visibleIndex;
        const std::filesystem::path& path = levelFiles[static_cast<size_t>(fileIndex)];
        const Rectangle row{8.0f, FileListTop + visibleIndex * FileRowHeight, FilePanelWidth - 16.0f, FileRowHeight - 2.0f};
        std::error_code error;
        const bool selected = !currentLevelPath.empty() &&
            std::filesystem::equivalent(path, currentLevelPath, error) && !error;
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), row);
        if (selected || hovered) {
            DrawRectangleRounded(row, 0.12f, 4, selected ? Color{48, 91, 139, 255} : Color{42, 47, 57, 255});
        }
        DrawUiText(uiFont, path.stem().string().c_str(), 16.0f, row.y + 5.0f, 16.0f,
            selected ? RAYWHITE : PrimaryText);
    }

    if (levelFiles.empty()) {
        DrawUiText(uiFont, "No .level files found", 16.0f, FileListTop + 8.0f, 16.0f, SecondaryText);
    }
}

void LevelEditor::DrawInspector() const {
    const int panelX = GetScreenWidth() - static_cast<int>(InspectorWidth);
    DrawRectangle(panelX, static_cast<int>(ToolbarHeight), static_cast<int>(InspectorWidth),
        GetScreenHeight() - static_cast<int>(ToolbarHeight + StatusBarHeight), PanelBackground);
    DrawLine(panelX, static_cast<int>(ToolbarHeight), panelX, GetScreenHeight(), PanelBorder);

    int y = 66;
    const int x = panelX + 18;
    DrawUiText(uiFont, "LEVEL OVERVIEW", static_cast<float>(x), static_cast<float>(y), 17.0f, SecondaryText);
    y += 34;
    const std::string levelName = currentLevelPath.empty() ? "Untitled" : currentLevelPath.filename().string();
    DrawUiText(uiFont, levelName.c_str(), static_cast<float>(x), static_cast<float>(y), 20.0f, PrimaryText);
    y += 34;

    auto stat = [&](const char* label, size_t count) {
        DrawUiText(uiFont, label, static_cast<float>(x), static_cast<float>(y), 17.0f, SecondaryText);
        const std::string value = std::to_string(count);
        DrawUiText(uiFont, value.c_str(), panelX + InspectorWidth - 18.0f - MeasureUiText(uiFont, value.c_str(), 17.0f),
            static_cast<float>(y), 17.0f, PrimaryText);
        y += 25;
    };

    DrawUiText(uiFont, TextFormat("World: %.0f x %.0f", level.worldBounds.width, level.worldBounds.height),
        static_cast<float>(x), static_cast<float>(y), 17.0f, PrimaryText);
    y += 34;
    stat("Visual tiles", level.visualTiles.size());
    stat("Solids", level.baseSolids.size());
    stat("Platforms", level.pitPlatforms.size());
    stat("Camera zones", level.cameraZones.size());
    stat("Ladders", level.ladders.size());
    stat("Fluids", level.fluids.size());
    stat("Machines", level.pulleys.size() + level.rotaryLatches.size() + level.gears.size() +
        level.flywheels.size() + level.steeringWheels.size() + level.screws.size() + level.fans.size());
    stat("Physics bodies", level.stoneBlocks.size() + level.boulders.size() + level.physicsWheels.size());
    stat("Ropes / chains", level.physicsRopes.size() + level.chains.size());
    stat("Guide objects", level.guideObjects.size());
    stat("Enemies", level.enemies.size());

    const auto layerCount = [&](WorldLayer layer) {
        size_t count = 0;
        for (const StoneBlock& object : level.stoneBlocks) if (object.layer == layer) ++count;
        for (const Boulder& object : level.boulders) if (object.layer == layer) ++count;
        for (const PhysicsWheel& object : level.physicsWheels) if (object.layer == layer) ++count;
        for (const Gear& object : level.gears) if (object.layer == layer) ++count;
        for (const Flywheel& object : level.flywheels) if (object.layer == layer) ++count;
        for (const Screw& object : level.screws) if (object.layer == layer) ++count;
        for (const GuideObject& object : level.guideObjects) if (object.layer == layer) ++count;
        return count;
    };
    stat("Background layer", layerCount(WorldLayer::Background));
    stat("Middleground layer", layerCount(WorldLayer::Middleground));
    stat("Foreground layer", layerCount(WorldLayer::Foreground));

    y += 13;
    DrawLine(x, y, panelX + static_cast<int>(InspectorWidth) - 18, y, PanelBorder);
    y += 18;
    DrawUiText(uiFont, "CONTROLS", static_cast<float>(x), static_cast<float>(y), 17.0f, SecondaryText);
    y += 29;
    DrawUiText(uiFont, "Wheel     Zoom", static_cast<float>(x), static_cast<float>(y), 16.0f, PrimaryText); y += 23;
    DrawUiText(uiFont, "Middle/RMB Pan", static_cast<float>(x), static_cast<float>(y), 16.0f, PrimaryText); y += 23;
    DrawUiText(uiFont, "F         Fit level", static_cast<float>(x), static_cast<float>(y), 16.0f, PrimaryText); y += 23;
    DrawUiText(uiFont, "R         Reload", static_cast<float>(x), static_cast<float>(y), 16.0f, PrimaryText); y += 23;
    DrawUiText(uiFont, "Drop      Open .level", static_cast<float>(x), static_cast<float>(y), 16.0f, PrimaryText);
}

void LevelEditor::DrawStatusBar() const {
    const int y = GetScreenHeight() - static_cast<int>(StatusBarHeight);
    DrawRectangle(0, y, GetScreenWidth(), static_cast<int>(StatusBarHeight), Color{23, 27, 34, 255});
    DrawLine(0, y, GetScreenWidth(), y, PanelBorder);
    DrawUiText(uiFont, statusText.c_str(), 12.0f, static_cast<float>(y + 7), 15.0f, SecondaryText);

    const Rectangle canvas = GetCanvasBounds();
    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, canvas)) {
        const Vector2 world = GetScreenToWorld2D(mouse, camera);
        const char* coordinates = TextFormat("x %.0f   y %.0f   zoom %.0f%%", world.x, world.y, camera.zoom * 100.0f);
        DrawUiText(uiFont, coordinates, GetScreenWidth() - MeasureUiText(uiFont, coordinates, 15.0f) - 12.0f,
            static_cast<float>(y + 7), 15.0f, PrimaryText);
    }
}
