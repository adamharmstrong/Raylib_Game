#include "Game.h"

#include "Collision.h"
#include "Constants.h"
#include "Fluid.h"
#include "Machine.h"
#include "MathUtils.h"
#include "Render.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {
    struct MenuButton {
        Rectangle rect;
        const char* text;
        bool enabled{true};
    };

    struct ResolutionPreset {
        int width;
        int height;
        const char* label;
    };

    constexpr ResolutionPreset kResolutionPresets[] = {
        {1024, 576, "1024x576"},
        {1280, 720, "1280x720"},
        {1280, 800, "1280x800"},
        {1366, 768, "1366x768"},
        {1440, 900, "1440x900"},
        {1600, 900, "1600x900"},
        {1920, 1080, "1920x1080"},
        {2560, 1440, "2560x1440"},
        {3840, 2160, "3840x2160"}
    };
    constexpr int kResolutionPresetCount = static_cast<int>(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]));
    constexpr int kDefaultResolutionPresetIndex = 5;

    constexpr int kFrameRateValues[] = {30, 60, 120, 144, 0};
    constexpr const char* kFrameRateLabels[] = {"30 FPS", "60 FPS", "120 FPS", "144 FPS", "Unlimited"};
    constexpr float kUiScaleValues[] = {0.75f, 1.0f, 1.25f, 1.5f};
    constexpr const char* kUiScaleLabels[] = {"75%", "100%", "125%", "150%"};
    constexpr int kFrameRateCount = static_cast<int>(sizeof(kFrameRateValues) / sizeof(kFrameRateValues[0]));
    constexpr int kUiScaleCount = static_cast<int>(sizeof(kUiScaleValues) / sizeof(kUiScaleValues[0]));
    constexpr int kSettingsControlCount = 8;

    bool gPixelPerfectScaling = true;
    float gUiScale = 1.0f;

    struct SettingsMenuLayout {
        Rectangle panel;
        std::array<Rectangle, 4> tabs;
        std::array<Rectangle, kSettingsControlCount> controls;
        Rectangle applyButton;
        Rectangle closeButton;
    };

    struct DropdownLayout {
        Rectangle panel;
        std::array<Rectangle, kResolutionPresetCount> options;
    };

    SettingsMenuLayout GetSettingsMenuLayout() {
        constexpr float panelWidth = 860.0f;
        constexpr float panelHeight = 520.0f;
        constexpr float panelPadding = 28.0f;
        constexpr float tabGap = 6.0f;
        constexpr float tabHeight = 38.0f;
        constexpr float controlGap = 16.0f;
        constexpr float controlHeight = 44.0f;
        constexpr float rowGap = 8.0f;

        const float panelX = (Constants::ScreenWidth - panelWidth) * 0.5f;
        const float panelY = (Constants::ScreenHeight - panelHeight) * 0.5f;
        const float contentWidth = panelWidth - panelPadding * 2.0f;
        const float tabWidth = (contentWidth - tabGap * 3.0f) / 4.0f;
        const float controlWidth = (contentWidth - controlGap) * 0.5f;

        SettingsMenuLayout layout{};
        layout.panel = {panelX, panelY, panelWidth, panelHeight};
        for (int i = 0; i < static_cast<int>(layout.tabs.size()); ++i) {
            layout.tabs[i] = {
                panelX + panelPadding + i * (tabWidth + tabGap),
                panelY + 62.0f,
                tabWidth,
                tabHeight
            };
        }
        for (int i = 0; i < kSettingsControlCount; ++i) {
            const int column = i % 2;
            const int row = i / 2;
            layout.controls[i] = {
                panelX + panelPadding + column * (controlWidth + controlGap),
                panelY + 122.0f + row * (controlHeight + rowGap),
                controlWidth,
                controlHeight
            };
        }
        layout.applyButton = {panelX + panelWidth * 0.5f - 158.0f, panelY + 456.0f, 145.0f, 42.0f};
        layout.closeButton = {panelX + panelWidth * 0.5f + 13.0f, panelY + 456.0f, 145.0f, 42.0f};
        return layout;
    }

    DropdownLayout GetDropdownLayout(Rectangle anchor, int optionCount, int columns = 1) {
        constexpr float padding = 6.0f;
        constexpr float gap = 4.0f;
        constexpr float optionHeight = 34.0f;
        const int rows = (optionCount + columns - 1) / columns;
        const float optionWidth = (anchor.width - padding * 2.0f - gap * (columns - 1)) / columns;

        DropdownLayout layout{};
        layout.panel = {
            anchor.x,
            anchor.y + anchor.height + 4.0f,
            anchor.width,
            padding * 2.0f + rows * optionHeight + (rows - 1) * gap
        };
        for (int i = 0; i < optionCount; ++i) {
            const int column = i % columns;
            const int row = i / columns;
            layout.options[i] = {
                layout.panel.x + padding + column * (optionWidth + gap),
                layout.panel.y + padding + row * (optionHeight + gap),
                optionWidth,
                optionHeight
            };
        }
        return layout;
    }

    Rectangle GetVirtualScreenViewport() {
        const float windowWidth = static_cast<float>(GetScreenWidth());
        const float windowHeight = static_cast<float>(GetScreenHeight());
        float scale = fminf(
            windowWidth / static_cast<float>(Constants::ScreenWidth),
            windowHeight / static_cast<float>(Constants::ScreenHeight)
        );
        if (gPixelPerfectScaling && scale >= 1.0f) {
            scale = floorf(scale);
        }
        scale = fmaxf(scale, 0.01f);
        const float width = Constants::ScreenWidth * scale;
        const float height = Constants::ScreenHeight * scale;
        return {(windowWidth - width) * 0.5f, (windowHeight - height) * 0.5f, width, height};
    }

    Vector2 GetUiMousePosition() {
        const Rectangle viewport = GetVirtualScreenViewport();
        const Vector2 mouse = GetMousePosition();
        return {
            (mouse.x - viewport.x) * Constants::ScreenWidth / viewport.width,
            (mouse.y - viewport.y) * Constants::ScreenHeight / viewport.height
        };
    }

    bool IsControlDown(KeyboardKey key) {
        return key != KEY_NULL && IsKeyDown(key);
    }

    bool IsControlPressed(KeyboardKey key) {
        return key != KEY_NULL && IsKeyPressed(key);
    }

    bool IsControlReleased(KeyboardKey key) {
        return key != KEY_NULL && IsKeyReleased(key);
    }

    bool IsNearRect(Rectangle a, Rectangle b, float distance) {
        Rectangle expanded{b.x - distance, b.y - distance, b.width + distance * 2.0f, b.height + distance * 2.0f};
        return CheckCollisionRecs(a, expanded);
    }

    std::string GetInteractPrompt(bool player1Near, bool player2Near, bool player3Near, const char* action) {
        std::string keys;
        if (player1Near) keys = "E";
        if (player2Near) keys += keys.empty() ? "U" : " / U";
        if (player3Near) keys += keys.empty() ? "Right Ctrl" : " / Right Ctrl";
        std::string prompt = action;
        if (!prompt.empty()) prompt += " ";
        return prompt + (keys.empty() ? "E" : keys);
    }

    void DrawDeathMarker(Texture2D skullTexture, Rectangle rect) {
        if (skullTexture.id > 0) {
            float scale = fminf(
                rect.width / static_cast<float>(skullTexture.width),
                rect.height / static_cast<float>(skullTexture.height)
            );
            float width = skullTexture.width * scale;
            float height = skullTexture.height * scale;
            Rectangle destination{
                rect.x + (rect.width - width) * 0.5f,
                rect.y + rect.height - height,
                width,
                height
            };
            DrawTexturePro(
                skullTexture,
                {0.0f, 0.0f, static_cast<float>(skullTexture.width), static_cast<float>(skullTexture.height)},
                destination,
                {0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }
        else {
            DrawRectangleRec(rect, BLACK);
        }
    }

    int ResolutionPresetCount() {
        return kResolutionPresetCount;
    }

    const ResolutionPreset& GetResolutionPreset(int index) {
        if (index < 0) index = 0;
        if (index >= ResolutionPresetCount()) {
            index = ResolutionPresetCount() - 1;
        }
        return kResolutionPresets[index];
    }

    int FindResolutionPresetIndex(int width, int height) {
        for (int i = 0; i < ResolutionPresetCount(); ++i) {
            if (kResolutionPresets[i].width == width && kResolutionPresets[i].height == height) {
                return i;
            }
        }
        return kDefaultResolutionPresetIndex;
    }

    const char* WindowModeLabel(WindowModeSetting mode) {
        switch (mode) {
        case WindowModeSetting::Borderless: return "Borderless";
        case WindowModeSetting::Fullscreen: return "Fullscreen";
        default: return "Windowed";
        }
    }

    const char* ScreenShakeLabel(ScreenShakeSetting setting) {
        switch (setting) {
        case ScreenShakeSetting::Off: return "Off";
        case ScreenShakeSetting::Reduced: return "Reduced";
        default: return "Full";
        }
    }

    const char* ColorblindLabel(ColorblindSetting setting) {
        switch (setting) {
        case ColorblindSetting::Protanopia: return "Protanopia";
        case ColorblindSetting::Deuteranopia: return "Deuteranopia";
        case ColorblindSetting::Tritanopia: return "Tritanopia";
        default: return "Off";
        }
    }

    const char* OnOffLabel(bool enabled) {
        return enabled ? "On" : "Off";
    }

    Color AccessibleDangerColor(ColorblindSetting setting) {
        switch (setting) {
        case ColorblindSetting::Protanopia: return Color{62, 174, 255, 255};
        case ColorblindSetting::Deuteranopia: return Color{255, 184, 52, 255};
        case ColorblindSetting::Tritanopia: return Color{255, 92, 126, 255};
        default: return RED;
        }
    }

    Color AccessibleSuccessColor(ColorblindSetting setting) {
        switch (setting) {
        case ColorblindSetting::Protanopia: return Color{255, 214, 74, 255};
        case ColorblindSetting::Deuteranopia: return Color{79, 176, 255, 255};
        case ColorblindSetting::Tritanopia: return Color{96, 225, 139, 255};
        default: return GREEN;
        }
    }

    int FindResolutionPresetIndexBySize(int width, int height) {
        return FindResolutionPresetIndex(width, height);
    }

    void DrawWrappedText(const std::string& text, Rectangle bounds, int fontSize, int lineSpacing, Color color) {
        std::istringstream stream(text);
        std::string word;
        std::string line;
        int y = static_cast<int>(bounds.y);
        int lineHeight = fontSize + lineSpacing;
        int maxY = static_cast<int>(bounds.y + bounds.height);

        while (stream >> word) {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > bounds.width) {
                if (y + fontSize > maxY) return;
                DrawText(line.c_str(), static_cast<int>(bounds.x), y, fontSize, color);
                y += lineHeight;
                line = word;
            }
            else {
                line = candidate;
            }
        }

        if (!line.empty() && y + fontSize <= maxY) {
            DrawText(line.c_str(), static_cast<int>(bounds.x), y, fontSize, color);
        }
    }

    std::vector<std::string> SplitCommandLine(const std::string& line) {
        std::istringstream stream(line);
        std::vector<std::string> parts;
        std::string part;

        while (stream >> part) {
            parts.push_back(part);
        }

        return parts;
    }

    std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    bool ParseFloat(const std::string& text, float& value) {
        try {
            size_t consumed = 0;
            value = std::stof(text, &consumed);
            return consumed == text.size();
        }
        catch (...) {
            return false;
        }
    }

    bool ParseInt(const std::string& text, int& value) {
        try {
            size_t consumed = 0;
            value = std::stoi(text, &consumed);
            return consumed == text.size();
        }
        catch (...) {
            return false;
        }
    }

    std::string OnOff(bool value) {
        return value ? "on" : "off";
    }

    FluidSimulationMode SelectedFluidMode(bool advancedFluidSimulation) {
        return advancedFluidSimulation ? FluidSimulationMode::Advanced : FluidSimulationMode::Tile;
    }

    const char* FluidModeName(bool advancedFluidSimulation) {
        return advancedFluidSimulation ? "Advanced" : "Simple";
    }

    bool WasButtonPressed(const MenuButton& button) {
        return button.enabled &&
            CheckCollisionPointRec(GetUiMousePosition(), button.rect) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    }

    int ScaledUiFontSize(int fontSize) {
        return std::clamp(static_cast<int>(roundf(fontSize * gUiScale)), 14, 96);
    }

    float ApproachFloat(float current, float target, float amount) {
        if (current < target) {
            return fminf(current + amount, target);
        }
        if (current > target) {
            return fmaxf(current - amount, target);
        }
        return target;
    }

    void DrawMenuButton(const MenuButton& button) {
        bool hovered = button.enabled && CheckCollisionPointRec(GetUiMousePosition(), button.rect);
        Color fill = button.enabled ? (hovered ? Color{76, 91, 104, 235} : Color{34, 42, 52, 225}) : Color{28, 30, 34, 180};
        Color border = hovered ? ORANGE : Fade(RAYWHITE, 0.45f);
        Color textColor = button.enabled ? RAYWHITE : Fade(RAYWHITE, 0.42f);

        DrawRectangleRec(button.rect, fill);
        DrawRectangleLinesEx(button.rect, 2.0f, border);

        int fontSize = std::clamp(ScaledUiFontSize(20), 16, 26);
        int textWidth = MeasureText(button.text, fontSize);
        while (fontSize > 14 && textWidth > button.rect.width - 16.0f) {
            --fontSize;
            textWidth = MeasureText(button.text, fontSize);
        }
        DrawText(
            button.text,
            static_cast<int>(button.rect.x + button.rect.width * 0.5f - textWidth * 0.5f),
            static_cast<int>(button.rect.y + button.rect.height * 0.5f - fontSize * 0.5f),
            fontSize,
            textColor
        );
    }

    void DrawCenteredText(const char* text, int centerX, int y, int fontSize, Color color) {
        fontSize = ScaledUiFontSize(fontSize);
        int textWidth = MeasureText(text, fontSize);
        DrawText(text, centerX - textWidth / 2, y, fontSize, color);
    }

    void DrawPulleyRope(Vector2 firstCenter, float firstRadius, Vector2 secondCenter, float secondRadius, float side, float thickness, Color color, float patternOffset) {
        Vector2 delta{secondCenter.x - firstCenter.x, secondCenter.y - firstCenter.y};
        float distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
        if (distance <= fabsf(firstRadius - secondRadius)) return;

        Vector2 direction{delta.x / distance, delta.y / distance};
        Vector2 perpendicular{-direction.y, direction.x};
        float radiusRatio = (firstRadius - secondRadius) / distance;
        float perpendicularScale = sqrtf(fmaxf(0.0f, 1.0f - radiusRatio * radiusRatio)) * side;
        Vector2 normal{
            direction.x * radiusRatio + perpendicular.x * perpendicularScale,
            direction.y * radiusRatio + perpendicular.y * perpendicularScale
        };
        Vector2 firstTangent{
            firstCenter.x + normal.x * firstRadius,
            firstCenter.y + normal.y * firstRadius
        };
        Vector2 secondTangent{
            secondCenter.x + normal.x * secondRadius,
            secondCenter.y + normal.y * secondRadius
        };
        if (color.r == BROWN.r && color.g == BROWN.g && color.b == BROWN.b) {
            DrawRope(firstTangent, secondTangent, thickness, patternOffset);
        }
        else {
            DrawLineEx(firstTangent, secondTangent, thickness, color);
        }
    }

    bool HasArea(Rectangle rect) {
        return rect.width > 0.0f && rect.height > 0.0f;
    }

    bool HasWaterPit(const Level& level) {
        return HasArea(level.waterPit.bounds);
    }

    FluidField* GetValveFluid(Level& level) {
        int index = level.valveFluidFill.fluidIndex;
        if (index < 0 || index >= static_cast<int>(level.fluids.size())) {
            return nullptr;
        }
        FluidField& fluid = level.fluids[index];
        return fluid.type == FluidType::Water ? &fluid : nullptr;
    }

    const FluidField* GetValveFluid(const Level& level) {
        int index = level.valveFluidFill.fluidIndex;
        if (index < 0 || index >= static_cast<int>(level.fluids.size())) {
            return nullptr;
        }
        const FluidField& fluid = level.fluids[index];
        return fluid.type == FluidType::Water ? &fluid : nullptr;
    }

    bool HasValveFluidFill(const Level& level) {
        return GetValveFluid(level) != nullptr &&
            level.valveFluidFill.targetFill > 0.0f && level.valveFluidFill.riseRate > 0.0f;
    }

    float GetValveFluidTargetMass(const Level& level, const FluidField& fluid) {
        float capacity = static_cast<float>(fluid.gridColumns * fluid.gridRows);
        return capacity * level.valveFluidFill.targetFill;
    }

    float GetValveFluidFillProgress(const Level& level) {
        const FluidField* fluid = GetValveFluid(level);
        if (fluid == nullptr) {
            return 0.0f;
        }
        float targetMass = GetValveFluidTargetMass(level, *fluid);
        return targetMass > 0.0f ? Clamp01(GetFluidMass(*fluid) / targetMass) : 0.0f;
    }

    float GetValveFluidSurfaceY(const Level& level) {
        const FluidField* fluid = GetValveFluid(level);
        if (fluid == nullptr || fluid->gridColumns <= 0) {
            return 0.0f;
        }
        float averageFilledRows = GetFluidMass(*fluid) / static_cast<float>(fluid->gridColumns);
        return std::clamp(
            fluid->bounds.y + fluid->bounds.height - averageFilledRows * fluid->cellSize,
            fluid->bounds.y,
            fluid->bounds.y + fluid->bounds.height
        );
    }

    bool IsTilesetReferenceLevel(const Level& level) {
        return level.script == LevelScript::TilesetReference;
    }

    bool HasFarBackgroundTiles(const Level& level) {
        return std::any_of(level.visualTiles.begin(), level.visualTiles.end(), [](const VisualTile& tile) {
            return tile.layer == TileLayer::FarBackground;
        });
    }

    Rectangle GetFilledWaterRect(const WaterPit& waterPit) {
        float pitBottom = waterPit.bounds.y + waterPit.bounds.height;
        float surfaceY = fminf(pitBottom, fmaxf(waterPit.surfaceY, waterPit.bounds.y));
        return {waterPit.bounds.x, surfaceY, waterPit.bounds.width, pitBottom - surfaceY};
    }

    FluidSample SampleFluidAroundRectangle(const Level& level, FluidType type, Rectangle rect) {
        float centerX = rect.x + rect.width * 0.5f;
        float centerY = rect.y + rect.height * 0.5f;
        constexpr float ProbeOffset = 10.0f;
        Vector2 samplePoints[]{
            {rect.x - ProbeOffset, centerY},
            {rect.x + rect.width + ProbeOffset, centerY},
            {centerX, rect.y - ProbeOffset},
            {centerX, rect.y + rect.height + ProbeOffset},
            {centerX, centerY}
        };

        FluidSample result{};
        float velocityWeight = 0.0f;
        for (Vector2 point : samplePoints) {
            FluidSample sample = SampleFluid(level.fluids, type, point);
            result.density += sample.density;
            result.velocity.x += sample.velocity.x * sample.density;
            result.velocity.y += sample.velocity.y * sample.density;
            velocityWeight += sample.density;
        }

        if (velocityWeight > 0.0001f) {
            result.velocity.x /= velocityWeight;
            result.velocity.y /= velocityWeight;
        }
        result.density = std::clamp(result.density / 5.0f, 0.0f, 1.0f);
        return result;
    }

    bool IsTouchingSimulatedWaterSurface(const Level& level, Rectangle rect) {
        float footY = rect.y + rect.height;
        float left = rect.x + 2.0f;
        float right = rect.x + rect.width - 2.0f;

        for (const FluidField& fluid : level.fluids) {
            if (fluid.type != FluidType::Water || fluid.cells.empty() ||
                right < fluid.bounds.x || left > fluid.bounds.x + fluid.bounds.width) {
                continue;
            }

            int firstColumn = std::clamp(
                static_cast<int>(floorf((left - fluid.bounds.x) / fluid.cellSize)),
                0,
                fluid.gridColumns - 1
            );
            int lastColumn = std::clamp(
                static_cast<int>(floorf((right - fluid.bounds.x) / fluid.cellSize)),
                0,
                fluid.gridColumns - 1
            );

            for (int column = firstColumn; column <= lastColumn; column++) {
                float columnMass = 0.0f;
                for (int row = 0; row < fluid.gridRows; row++) {
                    const FluidCell& cell = fluid.cells[row * fluid.gridColumns + column];
                    columnMass += fminf(cell.mass, 1.0f);
                }
                if (columnMass <= 0.01f) {
                    continue;
                }

                float surfaceY = std::clamp(
                    fluid.bounds.y + fluid.bounds.height - columnMass * fluid.cellSize,
                    fluid.bounds.y,
                    fluid.bounds.y + fluid.bounds.height
                );
                if (footY >= surfaceY - 6.0f && footY <= surfaceY + 22.0f) {
                    return true;
                }
            }
        }

        return false;
    }

    bool IsPlayerSwimming(const Player& player, const Level& level) {
        Vector2 playerCenter{
            player.rect.x + player.rect.width * 0.5f,
            player.rect.y + player.rect.height * 0.5f
        };
        if (HasWaterPit(level)) {
            Rectangle waterRect = GetFilledWaterRect(level.waterPit);
            if (waterRect.height > 0.0f && CheckCollisionPointRec(playerCenter, waterRect)) {
                return true;
            }
        }

        if (SampleFluidAroundRectangle(level, FluidType::Water, player.rect).density >= 0.18f) {
            return true;
        }

        if (IsTouchingSimulatedWaterSurface(level, player.rect)) {
            return true;
        }

        float centerX = player.rect.x + player.rect.width * 0.5f;
        float footY = player.rect.y + player.rect.height;
        constexpr float FootInset = 5.0f;
        const Vector2 footSamples[]{
            {centerX, footY + 2.0f},
            {player.rect.x + FootInset, footY + 2.0f},
            {player.rect.x + player.rect.width - FootInset, footY + 2.0f},
            {centerX, footY + 8.0f},
            {player.rect.x + FootInset, footY + 12.0f},
            {player.rect.x + player.rect.width - FootInset, footY + 12.0f},
            {player.rect.x - 4.0f, footY + 2.0f},
            {player.rect.x + player.rect.width + 4.0f, footY + 2.0f},
            {player.rect.x - 4.0f, footY + 8.0f},
            {player.rect.x + player.rect.width + 4.0f, footY + 8.0f},
            {player.rect.x - 4.0f, footY + 14.0f},
            {player.rect.x + player.rect.width + 4.0f, footY + 14.0f}
        };
        for (Vector2 point : footSamples) {
            if (SampleFluid(level.fluids, FluidType::Water, point).density >= 0.10f) {
                return true;
            }
        }

        return false;
    }

    float GetWaterFillProgress(const WaterPit& waterPit) {
        float totalTravel = waterPit.bounds.y + waterPit.bounds.height - waterPit.targetSurfaceY;
        if (totalTravel <= 0.0f) {
            return 0.0f;
        }

        float currentTravel = waterPit.bounds.y + waterPit.bounds.height - waterPit.surfaceY;
        return Clamp01(currentTravel / totalTravel);
    }

    bool HasFloodWaterControl(const Level& level) {
        return HasValveFluidFill(level) || HasWaterPit(level);
    }

    float GetFloodWaterProgress(const Level& level) {
        return HasValveFluidFill(level) ? GetValveFluidFillProgress(level) : GetWaterFillProgress(level.waterPit);
    }

    float GetFloodWaterSurfaceY(const Level& level) {
        return HasValveFluidFill(level) ? GetValveFluidSurfaceY(level) : level.waterPit.surfaceY;
    }

    float GetValveOpenAmount(const Valve& valve) {
        return Clamp01(valve.turnDegrees / 360.0f);
    }

    bool HorizontallyOverlaps(Rectangle a, Rectangle b) {
        return a.x < b.x + b.width && a.x + a.width > b.x;
    }

    float HorizontalOverlapWidth(Rectangle a, Rectangle b) {
        return fminf(a.x + a.width, b.x + b.width) - fmaxf(a.x, b.x);
    }

    bool HasSolidTouchingTop(Rectangle rect, const std::vector<Rectangle>& solids) {
        constexpr float Epsilon = 0.5f;
        constexpr float MinimumCoveredWidth = 48.0f;
        for (const Rectangle& other : solids) {
            if (fabsf((other.y + other.height) - rect.y) <= Epsilon && HorizontalOverlapWidth(rect, other) >= MinimumCoveredWidth) {
                return true;
            }
        }

        return false;
    }

    bool IsCeilingSolid(Rectangle rect) {
        return rect.y <= 0.5f && rect.width > rect.height * 2.0f;
    }

    bool IsWallSolid(Rectangle rect) {
        return rect.height > rect.width * 2.0f;
    }

    float RectCenterX(Rectangle rect) {
        return rect.x + rect.width * 0.5f;
    }

    Rectangle GetBoulderBounds(const Boulder& boulder) {
        return {
            boulder.center.x - boulder.radius,
            boulder.center.y - boulder.radius,
            boulder.radius * 2.0f,
            boulder.radius * 2.0f
        };
    }

    Rectangle GetWheelBounds(const PhysicsWheel& wheel) {
        return {
            wheel.center.x - wheel.radius,
            wheel.center.y - wheel.radius,
            wheel.radius * 2.0f,
            wheel.radius * 2.0f
        };
    }

    float GetStoneBlockPushScale(const StoneBlock& block) {
        return 0.75f / fmaxf(1.0f, block.mass);
    }

    float GetBoulderPushScale(const Boulder& boulder) {
        return 0.95f / fmaxf(1.0f, boulder.mass);
    }

    float GetWheelPushScale(const PhysicsWheel& wheel) {
        return 1.25f / fmaxf(1.0f, wheel.mass);
    }

    Rectangle GetGearBounds(const Gear& gear) {
        float radius = gear.radius * 1.12f;
        return {gear.center.x - radius, gear.center.y - radius, radius * 2.0f, radius * 2.0f};
    }

    Rectangle GetFlywheelBounds(const Flywheel& flywheel) {
        return {
            flywheel.center.x - flywheel.radius,
            flywheel.center.y - flywheel.radius,
            flywheel.radius * 2.0f,
            flywheel.radius * 2.0f
        };
    }

    float GetGearPushScale(const Gear& gear) {
        return 1.05f / fmaxf(1.0f, gear.mass);
    }

    float GetFlywheelPushScale(const Flywheel& flywheel) {
        return 0.70f / fmaxf(1.0f, flywheel.mass);
    }

    void ResolveStoneBlockHorizontal(StoneBlock& block, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            if (CheckCollisionRecs(block.rect, solid)) {
                if (block.velocity.x > 0.0f) {
                    block.rect.x = solid.x - block.rect.width;
                }
                else if (block.velocity.x < 0.0f) {
                    block.rect.x = solid.x + solid.width;
                }

                block.velocity.x = 0.0f;
            }
        }
    }

    void ResolveStoneBlockVertical(StoneBlock& block, const std::vector<Rectangle>& solids) {
        block.onGround = false;
        for (const Rectangle& solid : solids) {
            if (CheckCollisionRecs(block.rect, solid)) {
                if (block.velocity.y > 0.0f) {
                    block.rect.y = solid.y - block.rect.height;
                    block.velocity.y = 0.0f;
                    block.onGround = true;
                }
                else if (block.velocity.y < 0.0f) {
                    block.rect.y = solid.y + solid.height;
                    block.velocity.y = 0.0f;
                }
            }
        }
    }

    void ResolveStoneBlockPenetration(StoneBlock& block, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            float overlapX = fminf(block.rect.x + block.rect.width, solid.x + solid.width) - fmaxf(block.rect.x, solid.x);
            float overlapY = fminf(block.rect.y + block.rect.height, solid.y + solid.height) - fmaxf(block.rect.y, solid.y);
            if (overlapX <= 0.0f || overlapY <= 0.0f) {
                continue;
            }

            float blockCenterX = block.rect.x + block.rect.width * 0.5f;
            float blockCenterY = block.rect.y + block.rect.height * 0.5f;
            float solidCenterX = solid.x + solid.width * 0.5f;
            float solidCenterY = solid.y + solid.height * 0.5f;
            if (overlapX < overlapY) {
                block.rect.x += blockCenterX < solidCenterX ? -overlapX : overlapX;
                block.velocity.x = 0.0f;
            }
            else {
                bool blockAbove = blockCenterY < solidCenterY;
                block.rect.y += blockAbove ? -overlapY : overlapY;
                block.velocity.y = 0.0f;
                if (blockAbove) {
                    block.onGround = true;
                }
            }
        }
    }

    void ResolveBoulderWithRect(Boulder& boulder, Rectangle solid) {
        float closestX = std::clamp(boulder.center.x, solid.x, solid.x + solid.width);
        float closestY = std::clamp(boulder.center.y, solid.y, solid.y + solid.height);
        float dx = boulder.center.x - closestX;
        float dy = boulder.center.y - closestY;
        float distanceSquared = dx * dx + dy * dy;
        float radiusSquared = boulder.radius * boulder.radius;

        if (distanceSquared > radiusSquared) {
            return;
        }

        Vector2 normal{};
        float penetration = 0.0f;
        if (distanceSquared <= 0.0001f) {
            float left = fabsf(boulder.center.x - solid.x);
            float right = fabsf((solid.x + solid.width) - boulder.center.x);
            float top = fabsf(boulder.center.y - solid.y);
            float bottom = fabsf((solid.y + solid.height) - boulder.center.y);
            float nearest = fminf(fminf(left, right), fminf(top, bottom));

            if (nearest == left) {
                normal = {-1.0f, 0.0f};
                penetration = boulder.radius + left;
            }
            else if (nearest == right) {
                normal = {1.0f, 0.0f};
                penetration = boulder.radius + right;
            }
            else if (nearest == top) {
                normal = {0.0f, -1.0f};
                penetration = boulder.radius + top;
            }
            else {
                normal = {0.0f, 1.0f};
                penetration = boulder.radius + bottom;
            }
        }
        else {
            float distance = sqrtf(distanceSquared);
            normal = {dx / distance, dy / distance};
            penetration = boulder.radius - distance;
        }

        boulder.center.x += normal.x * penetration;
        boulder.center.y += normal.y * penetration;

        if (fabsf(normal.x) > fabsf(normal.y)) {
            boulder.velocity.x = 0.0f;
            boulder.angularVelocity *= -0.20f;
        }
        else {
            if (normal.y < 0.0f) {
                boulder.onGround = true;
            }
            boulder.velocity.y = 0.0f;
        }
    }

    void ResolveBoulderCollisions(Boulder& boulder, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            ResolveBoulderWithRect(boulder, solid);
        }
    }

    void ResolveWheelWithRect(PhysicsWheel& wheel, Rectangle solid) {
        float closestX = std::clamp(wheel.center.x, solid.x, solid.x + solid.width);
        float closestY = std::clamp(wheel.center.y, solid.y, solid.y + solid.height);
        float dx = wheel.center.x - closestX;
        float dy = wheel.center.y - closestY;
        float distanceSquared = dx * dx + dy * dy;
        float radiusSquared = wheel.radius * wheel.radius;

        if (distanceSquared > radiusSquared) {
            return;
        }

        Vector2 normal{};
        float penetration = 0.0f;
        if (distanceSquared <= 0.0001f) {
            float left = fabsf(wheel.center.x - solid.x);
            float right = fabsf((solid.x + solid.width) - wheel.center.x);
            float top = fabsf(wheel.center.y - solid.y);
            float bottom = fabsf((solid.y + solid.height) - wheel.center.y);
            float nearest = fminf(fminf(left, right), fminf(top, bottom));

            if (nearest == left) {
                normal = {-1.0f, 0.0f};
                penetration = wheel.radius + left;
            }
            else if (nearest == right) {
                normal = {1.0f, 0.0f};
                penetration = wheel.radius + right;
            }
            else if (nearest == top) {
                normal = {0.0f, -1.0f};
                penetration = wheel.radius + top;
            }
            else {
                normal = {0.0f, 1.0f};
                penetration = wheel.radius + bottom;
            }
        }
        else {
            float distance = sqrtf(distanceSquared);
            normal = {dx / distance, dy / distance};
            penetration = wheel.radius - distance;
        }

        wheel.center.x += normal.x * penetration;
        wheel.center.y += normal.y * penetration;

        if (fabsf(normal.x) > fabsf(normal.y)) {
            wheel.velocity.x = 0.0f;
            wheel.angularVelocity *= -0.25f;
        }
        else {
            if (normal.y < 0.0f) {
                wheel.onGround = true;
            }
            wheel.velocity.y = 0.0f;
        }
    }

    void ResolveWheelCollisions(PhysicsWheel& wheel, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            ResolveWheelWithRect(wheel, solid);
        }
    }

    template <typename RoundBody>
    void ResolveRoundBodyWithRect(RoundBody& body, Rectangle solid, float collisionRadius) {
        float closestX = std::clamp(body.center.x, solid.x, solid.x + solid.width);
        float closestY = std::clamp(body.center.y, solid.y, solid.y + solid.height);
        float dx = body.center.x - closestX;
        float dy = body.center.y - closestY;
        float distanceSquared = dx * dx + dy * dy;
        float radiusSquared = collisionRadius * collisionRadius;

        if (distanceSquared > radiusSquared) {
            return;
        }

        Vector2 normal{};
        float penetration = 0.0f;
        if (distanceSquared <= 0.0001f) {
            float left = fabsf(body.center.x - solid.x);
            float right = fabsf((solid.x + solid.width) - body.center.x);
            float top = fabsf(body.center.y - solid.y);
            float bottom = fabsf((solid.y + solid.height) - body.center.y);
            float nearest = fminf(fminf(left, right), fminf(top, bottom));

            if (nearest == left) {
                normal = {-1.0f, 0.0f};
                penetration = collisionRadius + left;
            }
            else if (nearest == right) {
                normal = {1.0f, 0.0f};
                penetration = collisionRadius + right;
            }
            else if (nearest == top) {
                normal = {0.0f, -1.0f};
                penetration = collisionRadius + top;
            }
            else {
                normal = {0.0f, 1.0f};
                penetration = collisionRadius + bottom;
            }
        }
        else {
            float distance = sqrtf(distanceSquared);
            normal = {dx / distance, dy / distance};
            penetration = collisionRadius - distance;
        }

        body.center.x += normal.x * penetration;
        body.center.y += normal.y * penetration;

        if (fabsf(normal.x) > fabsf(normal.y)) {
            body.velocity.x = 0.0f;
            body.angularVelocity *= -0.25f;
        }
        else {
            if (normal.y < 0.0f) {
                body.onGround = true;
            }
            body.velocity.y = 0.0f;
        }
    }

    void ResolveGearCollisions(Gear& gear, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            ResolveRoundBodyWithRect(gear, solid, gear.radius * 1.12f);
        }
    }

    void ResolveFlywheelCollisions(Flywheel& flywheel, const std::vector<Rectangle>& solids) {
        for (const Rectangle& solid : solids) {
            ResolveRoundBodyWithRect(flywheel, solid, flywheel.radius);
        }
    }

    void AccelerateDownSlope(Vector2& velocity, float angleDegrees, float dt, float scale) {
        float angle = angleDegrees * DEG2RAD;
        velocity.x += Constants::Gravity * sinf(angle) * cosf(angle) * dt * scale;
    }

    bool ResolveBoulderSeeSawStanding(Boulder& boulder, const std::vector<SeeSaw>& seeSaws, float dt) {
        if (boulder.velocity.y < 0.0f) {
            return false;
        }

        float footX = boulder.center.x;
        float footY = boulder.center.y + boulder.radius;
        for (const SeeSaw& seeSaw : seeSaws) {
            if (!IsPointOverSeeSaw(seeSaw, footX)) {
                continue;
            }

            float surfaceY = GetSeeSawSurfaceY(seeSaw, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                boulder.center.y = surfaceY - boulder.radius;
                boulder.velocity.y = 0.0f;
                boulder.onGround = true;
                AccelerateDownSlope(boulder.velocity, seeSaw.angle, dt, 0.32f);
                return true;
            }
        }

        return false;
    }

    bool ResolveWheelSeeSawStanding(PhysicsWheel& wheel, const std::vector<SeeSaw>& seeSaws, float dt) {
        if (wheel.velocity.y < 0.0f) {
            return false;
        }

        float footX = wheel.center.x;
        float footY = wheel.center.y + wheel.radius;
        for (const SeeSaw& seeSaw : seeSaws) {
            if (!IsPointOverSeeSaw(seeSaw, footX)) {
                continue;
            }

            float surfaceY = GetSeeSawSurfaceY(seeSaw, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                wheel.center.y = surfaceY - wheel.radius;
                wheel.velocity.y = 0.0f;
                wheel.onGround = true;
                AccelerateDownSlope(wheel.velocity, seeSaw.angle, dt, 0.42f);
                return true;
            }
        }

        return false;
    }

    template <typename RoundBody>
    bool ResolveRoundBodySeeSawStanding(
        RoundBody& body,
        const std::vector<SeeSaw>& seeSaws,
        float radius,
        float dt,
        float rollScale
    ) {
        if (body.velocity.y < 0.0f) {
            return false;
        }

        float footX = body.center.x;
        float footY = body.center.y + radius;
        for (const SeeSaw& seeSaw : seeSaws) {
            if (!IsPointOverSeeSaw(seeSaw, footX)) {
                continue;
            }

            float surfaceY = GetSeeSawSurfaceY(seeSaw, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                body.center.y = surfaceY - radius;
                body.velocity.y = 0.0f;
                body.onGround = true;
                AccelerateDownSlope(body.velocity, seeSaw.angle, dt, rollScale);
                return true;
            }
        }

        return false;
    }

    Rectangle MakeArrowRect(Vector2 position, Vector2 direction) {
        if (fabsf(direction.x) >= fabsf(direction.y)) {
            return {position.x - 14.0f, position.y - 3.0f, 28.0f, 6.0f};
        }

        return {position.x - 3.0f, position.y - 14.0f, 6.0f, 28.0f};
    }

    bool IsRectStandingOnTile(Rectangle rect, Rectangle tile) {
        constexpr float FootTolerance = 4.0f;
        float rectBottom = rect.y + rect.height;
        bool footOnTop = rectBottom >= tile.y - FootTolerance && rectBottom <= tile.y + FootTolerance;
        return footOnTop && HorizontallyOverlaps(rect, tile);
    }

    void SpawnBreakableDebris(BreakableTile& tile) {
        if (!tile.debris.empty()) {
            return;
        }

        constexpr int Columns = 4;
        constexpr int Rows = 2;
        float shardWidth = tile.rect.width / static_cast<float>(Columns);
        float shardHeight = tile.rect.height / static_cast<float>(Rows);

        for (int row = 0; row < Rows; row++) {
            for (int column = 0; column < Columns; column++) {
                float centerOffset = static_cast<float>(column) - (Columns - 1) * 0.5f;
                BreakableDebris debris{};
                debris.rect = {
                    tile.rect.x + column * shardWidth + 1.0f,
                    tile.rect.y + row * shardHeight + 1.0f,
                    shardWidth - 2.0f,
                    shardHeight - 2.0f
                };
                debris.velocity = {
                    centerOffset * 55.0f,
                    70.0f + static_cast<float>(row) * 45.0f + fabsf(centerOffset) * 18.0f
                };
                debris.maxLife = 0.75f + static_cast<float>(column + row) * 0.045f;
                debris.life = debris.maxLife;
                tile.debris.push_back(debris);
            }
        }
    }

    bool ResolveEnemyHorizontal(Enemy& enemy, const std::vector<Rectangle>& solids) {
        bool collided = false;

        for (const Rectangle& solid : solids) {
            if (CheckCollisionRecs(enemy.rect, solid)) {
                if (enemy.velocity.x > 0.0f) {
                    enemy.rect.x = solid.x - enemy.rect.width;
                }
                else if (enemy.velocity.x < 0.0f) {
                    enemy.rect.x = solid.x + solid.width;
                }

                enemy.velocity.x = 0.0f;
                collided = true;
            }
        }

        return collided;
    }

    void ResolveEnemyVertical(Enemy& enemy, const std::vector<Rectangle>& solids) {
        enemy.onGround = false;

        for (const Rectangle& solid : solids) {
            if (CheckCollisionRecs(enemy.rect, solid)) {
                if (enemy.velocity.y > 0.0f) {
                    enemy.rect.y = solid.y - enemy.rect.height;
                    enemy.velocity.y = 0.0f;
                    enemy.onGround = true;
                }
                else if (enemy.velocity.y < 0.0f) {
                    enemy.rect.y = solid.y + solid.height;
                    enemy.velocity.y = 0.0f;
                }
            }
        }
    }

    bool ResolveSeeSawStanding(Rectangle& rect, Vector2& velocity, bool& onGround, const std::vector<SeeSaw>& seeSaws) {
        if (velocity.y < 0.0f) {
            return false;
        }

        float footX = RectCenterX(rect);
        float footY = rect.y + rect.height;

        for (const SeeSaw& seeSaw : seeSaws) {
            if (!IsPointOverSeeSaw(seeSaw, footX)) {
                continue;
            }

            float surfaceY = GetSeeSawSurfaceY(seeSaw, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                rect.y = surfaceY - rect.height;
                velocity.y = 0.0f;
                onGround = true;
                return true;
            }
        }

        return false;
    }

    bool ResolveRampStanding(Rectangle& rect, Vector2& velocity, bool& onGround, const std::vector<Ramp>& ramps) {
        if (velocity.y < 0.0f) {
            return false;
        }

        float footX = RectCenterX(rect);
        float footY = rect.y + rect.height;

        for (const Ramp& ramp : ramps) {
            if (!IsPointOverRamp(ramp, footX)) {
                continue;
            }

            float surfaceY = GetRampSurfaceY(ramp, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                rect.y = surfaceY - rect.height;
                velocity.y = 0.0f;
                onGround = true;
                return true;
            }
        }

        return false;
    }

    bool ResolveTrapDoorStanding(Rectangle& rect, Vector2& velocity, bool& onGround, const std::vector<TrapDoor>& trapDoors) {
        if (velocity.y < 0.0f) {
            return false;
        }

        float footX = RectCenterX(rect);
        float footY = rect.y + rect.height;

        for (const TrapDoor& trapDoor : trapDoors) {
            if (!IsPointOverTrapDoor(trapDoor, footX)) {
                continue;
            }

            float surfaceY = GetTrapDoorSurfaceY(trapDoor, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                rect.y = surfaceY - rect.height;
                velocity.y = 0.0f;
                onGround = true;
                return true;
            }
        }

        return false;
    }

    bool ResolveBoulderRampStanding(Boulder& boulder, const std::vector<Ramp>& ramps, float dt) {
        if (boulder.velocity.y < 0.0f) {
            return false;
        }

        float footX = boulder.center.x;
        float footY = boulder.center.y + boulder.radius;
        for (const Ramp& ramp : ramps) {
            if (!IsPointOverRamp(ramp, footX)) {
                continue;
            }

            float surfaceY = GetRampSurfaceY(ramp, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                boulder.center.y = surfaceY - boulder.radius;
                boulder.velocity.y = 0.0f;
                boulder.onGround = true;
                AccelerateDownSlope(boulder.velocity, ramp.angle, dt, 0.34f);
                return true;
            }
        }

        return false;
    }

    bool ResolveBoulderTrapDoorStanding(Boulder& boulder, const std::vector<TrapDoor>& trapDoors, float dt) {
        if (boulder.velocity.y < 0.0f) {
            return false;
        }

        float footX = boulder.center.x;
        float footY = boulder.center.y + boulder.radius;
        for (const TrapDoor& trapDoor : trapDoors) {
            if (!IsPointOverTrapDoor(trapDoor, footX)) {
                continue;
            }

            float surfaceY = GetTrapDoorSurfaceY(trapDoor, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                boulder.center.y = surfaceY - boulder.radius;
                boulder.velocity.y = 0.0f;
                boulder.onGround = true;
                AccelerateDownSlope(boulder.velocity, trapDoor.angle, dt, 0.30f);
                return true;
            }
        }

        return false;
    }

    bool ResolveWheelRampStanding(PhysicsWheel& wheel, const std::vector<Ramp>& ramps, float dt) {
        if (wheel.velocity.y < 0.0f) {
            return false;
        }

        float footX = wheel.center.x;
        float footY = wheel.center.y + wheel.radius;
        for (const Ramp& ramp : ramps) {
            if (!IsPointOverRamp(ramp, footX)) {
                continue;
            }

            float surfaceY = GetRampSurfaceY(ramp, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                wheel.center.y = surfaceY - wheel.radius;
                wheel.velocity.y = 0.0f;
                wheel.onGround = true;
                AccelerateDownSlope(wheel.velocity, ramp.angle, dt, 0.42f);
                return true;
            }
        }

        return false;
    }

    bool ResolveWheelTrapDoorStanding(PhysicsWheel& wheel, const std::vector<TrapDoor>& trapDoors, float dt) {
        if (wheel.velocity.y < 0.0f) {
            return false;
        }

        float footX = wheel.center.x;
        float footY = wheel.center.y + wheel.radius;
        for (const TrapDoor& trapDoor : trapDoors) {
            if (!IsPointOverTrapDoor(trapDoor, footX)) {
                continue;
            }

            float surfaceY = GetTrapDoorSurfaceY(trapDoor, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                wheel.center.y = surfaceY - wheel.radius;
                wheel.velocity.y = 0.0f;
                wheel.onGround = true;
                AccelerateDownSlope(wheel.velocity, trapDoor.angle, dt, 0.40f);
                return true;
            }
        }

        return false;
    }

    template <typename RoundBody>
    bool ResolveRoundBodyRampStanding(RoundBody& body, const std::vector<Ramp>& ramps, float radius, float dt, float rollScale) {
        if (body.velocity.y < 0.0f) {
            return false;
        }

        float footX = body.center.x;
        float footY = body.center.y + radius;
        for (const Ramp& ramp : ramps) {
            if (!IsPointOverRamp(ramp, footX)) {
                continue;
            }

            float surfaceY = GetRampSurfaceY(ramp, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                body.center.y = surfaceY - radius;
                body.velocity.y = 0.0f;
                body.onGround = true;
                AccelerateDownSlope(body.velocity, ramp.angle, dt, rollScale);
                return true;
            }
        }

        return false;
    }

    template <typename RoundBody>
    bool ResolveRoundBodyTrapDoorStanding(
        RoundBody& body,
        const std::vector<TrapDoor>& trapDoors,
        float radius,
        float dt,
        float rollScale
    ) {
        if (body.velocity.y < 0.0f) {
            return false;
        }

        float footX = body.center.x;
        float footY = body.center.y + radius;
        for (const TrapDoor& trapDoor : trapDoors) {
            if (!IsPointOverTrapDoor(trapDoor, footX)) {
                continue;
            }

            float surfaceY = GetTrapDoorSurfaceY(trapDoor, footX);
            if (footY >= surfaceY - 18.0f && footY <= surfaceY + 28.0f) {
                body.center.y = surfaceY - radius;
                body.velocity.y = 0.0f;
                body.onGround = true;
                AccelerateDownSlope(body.velocity, trapDoor.angle, dt, rollScale);
                return true;
            }
        }

        return false;
    }

    float GetSeeSawTorqueContribution(const SeeSaw& seeSaw, Rectangle rect, float mass) {
        float footX = RectCenterX(rect);
        if (!IsPointOverSeeSaw(seeSaw, footX)) {
            return 0.0f;
        }

        float footY = rect.y + rect.height;
        float surfaceY = GetSeeSawSurfaceY(seeSaw, footX);
        if (fabsf(footY - surfaceY) > 6.0f) {
            return 0.0f;
        }

        return ((footX - seeSaw.pivot.x) / (seeSaw.length * 0.5f)) * mass;
    }

    Rectangle GetSeeSawBounds(const SeeSaw& seeSaw) {
        float angle = seeSaw.angle * DEG2RAD;
        float halfWidth = fabsf(cosf(angle)) * seeSaw.length * 0.5f + fabsf(sinf(angle)) * seeSaw.thickness * 0.5f;
        float halfHeight = fabsf(sinf(angle)) * seeSaw.length * 0.5f + fabsf(cosf(angle)) * seeSaw.thickness * 0.5f;
        return {
            seeSaw.pivot.x - halfWidth,
            seeSaw.pivot.y - halfHeight,
            halfWidth * 2.0f,
            halfHeight * 2.0f
        };
    }

    Rectangle GetRampBounds(const Ramp& ramp) {
        float angle = ramp.angle * DEG2RAD;
        float halfWidth = fabsf(cosf(angle)) * ramp.length * 0.5f + fabsf(sinf(angle)) * ramp.thickness * 0.5f;
        float halfHeight = fabsf(sinf(angle)) * ramp.length * 0.5f + fabsf(cosf(angle)) * ramp.thickness * 0.5f;
        return {
            ramp.center.x - halfWidth,
            ramp.center.y - halfHeight,
            halfWidth * 2.0f,
            halfHeight * 2.0f
        };
    }

    Rectangle GetTrapDoorBounds(const TrapDoor& trapDoor) {
        Vector2 ring = GetTrapDoorRingPosition(trapDoor);
        float angle = trapDoor.angle * DEG2RAD;
        float halfWidth = fabsf(cosf(angle)) * trapDoor.length * 0.5f + fabsf(sinf(angle)) * trapDoor.thickness * 0.5f;
        float halfHeight = fabsf(sinf(angle)) * trapDoor.length * 0.5f + fabsf(cosf(angle)) * trapDoor.thickness * 0.5f;
        Vector2 center{(trapDoor.hinge.x + ring.x) * 0.5f, (trapDoor.hinge.y + ring.y) * 0.5f};
        return {center.x - halfWidth, center.y - halfHeight, halfWidth * 2.0f, halfHeight * 2.0f};
    }

    Rectangle GetScrewBounds(const Screw& screw) {
        float angle = screw.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 start{screw.center.x - axis.x * screw.length * 0.5f, screw.center.y - axis.y * screw.length * 0.5f};
        Vector2 end{screw.center.x + axis.x * screw.length * 0.5f, screw.center.y + axis.y * screw.length * 0.5f};
        float minX = fminf(start.x, end.x) - screw.radius;
        float maxX = fmaxf(start.x, end.x) + screw.radius;
        float minY = fminf(start.y, end.y) - screw.radius;
        float maxY = fmaxf(start.y, end.y) + screw.radius;

        return {minX, minY, maxX - minX, maxY - minY};
    }

    void AppendScrewColliders(std::vector<Rectangle>& colliders, const Screw& screw) {
        float angle = screw.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        int segmentCount = std::max(1, static_cast<int>(ceilf(screw.length / fmaxf(6.0f, screw.radius))));
        for (int i = 0; i <= segmentCount; i++) {
            float amount = static_cast<float>(i) / static_cast<float>(segmentCount);
            float distance = (amount - 0.5f) * screw.length;
            Vector2 center{
                screw.center.x + axis.x * distance,
                screw.center.y + axis.y * distance
            };
            colliders.push_back({
                center.x - screw.radius,
                center.y - screw.radius,
                screw.radius * 2.0f,
                screw.radius * 2.0f
            });
        }
    }

    bool IsRectTouchingScrew(Rectangle rect, const Screw& screw) {
        float angle = screw.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 normal{-axis.y, axis.x};
        Vector2 rectCenter{
            rect.x + rect.width * 0.5f,
            rect.y + rect.height * 0.5f
        };
        Vector2 delta{rectCenter.x - screw.center.x, rectCenter.y - screw.center.y};
        float halfRectWidth = rect.width * 0.5f;
        float halfRectHeight = rect.height * 0.5f;
        float projectedOnAxis = fabsf(delta.x * axis.x + delta.y * axis.y);
        float rectRadiusOnAxis = halfRectWidth * fabsf(axis.x) + halfRectHeight * fabsf(axis.y);
        float projectedOnNormal = fabsf(delta.x * normal.x + delta.y * normal.y);
        float rectRadiusOnNormal = halfRectWidth * fabsf(normal.x) + halfRectHeight * fabsf(normal.y);
        return projectedOnAxis <= screw.length * 0.5f + screw.radius + rectRadiusOnAxis &&
            projectedOnNormal <= screw.radius + rectRadiusOnNormal;
    }

    bool IsCircleTouchingScrew(Vector2 center, float radius, const Screw& screw) {
        float angle = screw.angle * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 offset{center.x - screw.center.x, center.y - screw.center.y};
        float along = std::clamp(
            offset.x * axis.x + offset.y * axis.y,
            -screw.length * 0.5f,
            screw.length * 0.5f
        );
        Vector2 closest{
            screw.center.x + axis.x * along,
            screw.center.y + axis.y * along
        };
        float dx = center.x - closest.x;
        float dy = center.y - closest.y;
        float combinedRadius = radius + screw.radius;
        return dx * dx + dy * dy <= combinedRadius * combinedRadius;
    }

    void ApplyScrewConveyor(Rectangle rect, Vector2& velocity, float mass, const std::vector<Screw>& screws, float dt) {
        float inverseMass = 1.0f / fmaxf(1.0f, mass);
        for (const Screw& screw : screws) {
            if (!IsRectTouchingScrew(rect, screw)) {
                continue;
            }

            float angle = screw.angle * DEG2RAD;
            velocity.x += cosf(angle) * screw.spinSpeed * dt * 0.55f * inverseMass;
            velocity.y += sinf(angle) * screw.spinSpeed * dt * 0.20f * inverseMass;
        }
    }

    void ApplyScrewConveyor(Boulder& boulder, float radius, const std::vector<Screw>& screws, float dt) {
        float inverseMass = 1.0f / fmaxf(1.0f, boulder.mass);
        for (const Screw& screw : screws) {
            if (!IsCircleTouchingScrew(boulder.center, radius, screw)) {
                continue;
            }

            float angle = screw.angle * DEG2RAD;
            boulder.velocity.x += cosf(angle) * screw.spinSpeed * dt * 0.55f * inverseMass;
            boulder.velocity.y += sinf(angle) * screw.spinSpeed * dt * 0.18f * inverseMass;
            boulder.angularVelocity += screw.spinSpeed * dt * 3.2f * inverseMass;
        }
    }

    template <typename RoundBody>
    void ApplyScrewConveyor(RoundBody& body, float radius, const std::vector<Screw>& screws, float dt) {
        float inverseMass = 1.0f / fmaxf(1.0f, body.mass);
        for (const Screw& screw : screws) {
            if (!IsCircleTouchingScrew(body.center, radius, screw)) {
                continue;
            }

            float angle = screw.angle * DEG2RAD;
            body.velocity.x += cosf(angle) * screw.spinSpeed * dt * 0.65f * inverseMass;
            body.velocity.y += sinf(angle) * screw.spinSpeed * dt * 0.20f * inverseMass;
            body.angularVelocity += screw.spinSpeed * dt * 4.0f * inverseMass;
        }
    }

    float GetFanStrengthAtPoint(const Fan& fan, Vector2 point) {
        if (fan.power <= 0.0f || fan.strength <= 0.0f || fan.length <= 0.0f || fan.width <= 0.0f) {
            return 0.0f;
        }

        Vector2 offset{point.x - fan.center.x, point.y - fan.center.y};
        float along = offset.x * fan.direction.x + offset.y * fan.direction.y;
        if (along < 0.0f || along > fan.length) {
            return 0.0f;
        }

        Vector2 normal{-fan.direction.y, fan.direction.x};
        float side = offset.x * normal.x + offset.y * normal.y;
        if (fabsf(side) > fan.width * 0.5f) {
            return 0.0f;
        }

        float alongFalloff = 1.0f - (along / fan.length) * 0.35f;
        float sideFalloff = 1.0f - fabsf(side) / (fan.width * 0.5f) * 0.30f;
        return fan.strength * fan.power * alongFalloff * sideFalloff;
    }

    Vector2 GetWindAtPoint(const Level& level, Vector2 point) {
        Vector2 wind{0.0f, 0.0f};
        for (const Fan& fan : level.fans) {
            float strength = GetFanStrengthAtPoint(fan, point);
            wind.x += fan.direction.x * strength;
            wind.y += fan.direction.y * strength;
        }

        return wind;
    }

    float GetPinwheelSpinAtPoint(const Level& level, Vector2 point) {
        float spin = 0.0f;
        for (const Fan& fan : level.fans) {
            float strength = GetFanStrengthAtPoint(fan, point);
            if (strength <= 0.0f) {
                continue;
            }

            float direction = fabsf(fan.direction.x) >= fabsf(fan.direction.y)
                ? (fan.direction.x >= 0.0f ? 1.0f : -1.0f)
                : (fan.direction.y >= 0.0f ? -1.0f : 1.0f);
            spin += direction * strength;
        }

        return spin;
    }

    Vector2 RectCenter(Rectangle rect) {
        return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    }

    void ApplyWindToVelocity(Vector2& velocity, Vector2 point, float mass, const Level& level, float dt, float scale) {
        Vector2 wind = GetWindAtPoint(level, point);
        float inverseMass = 1.0f / fmaxf(1.0f, mass);
        velocity.x += wind.x * dt * scale * inverseMass;
        velocity.y += wind.y * dt * scale * inverseMass;
    }

    void ApplyFluidForcesToVelocity(
        Vector2& velocity,
        Rectangle bodyBounds,
        float mass,
        float buoyancy,
        float drag,
        const Level& level,
        float dt
    ) {
        float effectiveMass = fmaxf(0.6f, mass);
        FluidSample water = SampleFluidAroundRectangle(level, FluidType::Water, bodyBounds);
        if (water.density > 0.001f) {
            float response = 1.0f - expf(-drag * water.density * dt / sqrtf(effectiveMass));
            Vector2 target{water.velocity.x * 0.72f, water.velocity.y * 0.72f};
            velocity.x += (target.x - velocity.x) * response;
            velocity.y += (target.y - velocity.y) * response;
            velocity.y -= Constants::Gravity * buoyancy * water.density / effectiveMass * dt;
        }

        FluidSample sand = SampleFluidAroundRectangle(level, FluidType::Sand, bodyBounds);
        if (sand.density > 0.001f) {
            // Granular material resists motion strongly but does not provide liquid buoyancy.
            float response = 1.0f - expf(-drag * 1.65f * sand.density * dt / sqrtf(effectiveMass));
            Vector2 target{sand.velocity.x * 0.30f, sand.velocity.y * 0.18f};
            velocity.x += (target.x - velocity.x) * response;
            velocity.y += (target.y - velocity.y) * response;
        }

        FluidSample gel = SampleFluidAroundRectangle(level, FluidType::Gel, bodyBounds);
        if (gel.density > 0.001f) {
            // Gel entrains embedded bodies much more strongly than free-flowing water.
            float response = 1.0f - expf(-drag * 2.25f * gel.density * dt / sqrtf(effectiveMass));
            Vector2 target{gel.velocity.x * 0.58f, gel.velocity.y * 0.58f};
            velocity.x += (target.x - velocity.x) * response;
            velocity.y += (target.y - velocity.y) * response;
            velocity.y -= Constants::Gravity * buoyancy * 0.62f * gel.density / effectiveMass * dt;
        }

        FluidSample gas = SampleFluidAroundRectangle(level, FluidType::Gas, bodyBounds);
        if (gas.density > 0.001f) {
            float response = 1.0f - expf(-drag * 0.18f * gas.density * dt / sqrtf(effectiveMass));
            velocity.x += (gas.velocity.x - velocity.x) * response;
            velocity.y += (gas.velocity.y - velocity.y) * response;
            velocity.y -= 42.0f * buoyancy * gas.density / effectiveMass * dt;
        }
    }

    Vector2 GetFlexibleFluidAcceleration(
        const Level& level,
        Vector2 point,
        Vector2 velocity,
        float buoyancy,
        float drag
    ) {
        Vector2 acceleration{};
        FluidSample water = SampleFluid(level.fluids, FluidType::Water, point);
        acceleration.x += (water.velocity.x - velocity.x) * drag * water.density;
        acceleration.y += (water.velocity.y - velocity.y) * drag * water.density;
        acceleration.y -= Constants::Gravity * buoyancy * water.density;

        FluidSample sand = SampleFluid(level.fluids, FluidType::Sand, point);
        acceleration.x += (sand.velocity.x * 0.30f - velocity.x) * drag * 1.45f * sand.density;
        acceleration.y += (sand.velocity.y * 0.18f - velocity.y) * drag * 1.45f * sand.density;

        FluidSample gel = SampleFluid(level.fluids, FluidType::Gel, point);
        acceleration.x += (gel.velocity.x * 0.58f - velocity.x) * drag * 2.0f * gel.density;
        acceleration.y += (gel.velocity.y * 0.58f - velocity.y) * drag * 2.0f * gel.density;
        acceleration.y -= Constants::Gravity * buoyancy * 0.62f * gel.density;

        FluidSample gas = SampleFluid(level.fluids, FluidType::Gas, point);
        acceleration.x += (gas.velocity.x - velocity.x) * drag * 0.16f * gas.density;
        acceleration.y += (gas.velocity.y - velocity.y) * drag * 0.16f * gas.density;
        acceleration.y -= 42.0f * buoyancy * gas.density;
        constexpr float MaximumAcceleration = 2200.0f;
        float accelerationSquared = acceleration.x * acceleration.x + acceleration.y * acceleration.y;
        if (accelerationSquared > MaximumAcceleration * MaximumAcceleration) {
            float scale = MaximumAcceleration / sqrtf(accelerationSquared);
            acceleration.x *= scale;
            acceleration.y *= scale;
        }
        return acceleration;
    }

    void DisturbFluids(
        std::vector<FluidField>& fields,
        Vector2 point,
        float radius,
        Vector2 bodyVelocity,
        float strength,
        float dt
    ) {
        for (FluidField& field : fields) {
            FluidSample sample = SampleFluid(field, point);
            Vector2 velocityChange{
                (bodyVelocity.x - sample.velocity.x) * strength * dt,
                (bodyVelocity.y - sample.velocity.y) * strength * dt
            };
            AddFluidImpulse(field, point, radius, velocityChange);
        }
    }

    void AppendChainPointColliders(std::vector<Rectangle>& colliders, const Chain& chain) {
        float radius = fmaxf(2.0f, chain.collisionRadius * chain.scale);
        for (const Vector2& point : chain.points) {
            colliders.push_back({point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f});
        }
    }

    void AppendPhysicsRopePointColliders(std::vector<Rectangle>& colliders, const PhysicsRope& rope) {
        float radius = fmaxf(1.0f, rope.thickness * 0.5f);
        for (const Vector2& point : rope.points) {
            colliders.push_back({point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f});
        }
    }

    void AppendFlexibleObjectColliders(std::vector<Rectangle>& colliders, const Level& level) {
        for (const Chain& chain : level.chains) {
            AppendChainPointColliders(colliders, chain);
        }
        for (const PhysicsRope& rope : level.physicsRopes) {
            AppendPhysicsRopePointColliders(colliders, rope);
        }
    }

    std::vector<Rectangle> BuildFlexibleBodyColliders(
        const Level& level,
        const Player* player,
        const Player* player2,
        const Player* player3,
        int ignoredChain,
        int ignoredRope
    ) {
        std::vector<Rectangle> colliders = BuildSolids(level);
        if (player != nullptr) {
            colliders.push_back(player->rect);
        }
        if (player2 != nullptr) {
            colliders.push_back(player2->rect);
        }
        if (player3 != nullptr) {
            colliders.push_back(player3->rect);
        }

        for (const StoneBlock& block : level.stoneBlocks) {
            colliders.push_back(block.rect);
        }

        for (const Boulder& boulder : level.boulders) {
            colliders.push_back(GetBoulderBounds(boulder));
        }

        for (const PhysicsWheel& wheel : level.physicsWheels) {
            colliders.push_back(GetWheelBounds(wheel));
        }

        for (const Gear& gear : level.gears) {
            colliders.push_back(GetGearBounds(gear));
        }

        for (const Flywheel& flywheel : level.flywheels) {
            colliders.push_back(GetFlywheelBounds(flywheel));
        }

        for (const HangingWeight& weight : level.weights) {
            colliders.push_back(weight.rect);
        }

        for (const SeeSaw& seeSaw : level.seeSaws) {
            colliders.push_back(GetSeeSawBounds(seeSaw));
        }

        for (const Ramp& ramp : level.ramps) {
            colliders.push_back(GetRampBounds(ramp));
        }

        for (const TrapDoor& trapDoor : level.trapDoors) {
            colliders.push_back(GetTrapDoorBounds(trapDoor));
        }

        for (const Screw& screw : level.screws) {
            AppendScrewColliders(colliders, screw);
        }

        for (const Enemy& enemy : level.enemies) {
            colliders.push_back(enemy.rect);
        }

        for (int i = 0; i < static_cast<int>(level.chains.size()); i++) {
            if (i != ignoredChain) {
                AppendChainPointColliders(colliders, level.chains[i]);
            }
        }
        for (int i = 0; i < static_cast<int>(level.physicsRopes.size()); i++) {
            if (i != ignoredRope) {
                AppendPhysicsRopePointColliders(colliders, level.physicsRopes[i]);
            }
        }

        return colliders;
    }

    void AppendOrientedFluidObstacle(
        std::vector<Rectangle>& obstacles,
        Vector2 center,
        float length,
        float thickness,
        float angleDegrees
    ) {
        float angle = angleDegrees * DEG2RAD;
        Vector2 axis{cosf(angle), sinf(angle)};
        Vector2 normal{-axis.y, axis.x};
        // Follow one-pixel cellular materials closely enough that the AABB
        // approximation does not leave block-shaped voids along sloped edges.
        float segmentLength = 1.0f;
        int segmentCount = std::max(1, static_cast<int>(ceilf(length / segmentLength)));
        float actualSegmentLength = length / static_cast<float>(segmentCount);
        float halfWidth = fabsf(axis.x) * actualSegmentLength * 0.5f + fabsf(normal.x) * thickness * 0.5f;
        float halfHeight = fabsf(axis.y) * actualSegmentLength * 0.5f + fabsf(normal.y) * thickness * 0.5f;
        for (int segment = 0; segment < segmentCount; segment++) {
            float distance = (static_cast<float>(segment) + 0.5f) * actualSegmentLength - length * 0.5f;
            Vector2 segmentCenter{
                center.x + axis.x * distance,
                center.y + axis.y * distance
            };
            obstacles.push_back({
                segmentCenter.x - halfWidth,
                segmentCenter.y - halfHeight,
                halfWidth * 2.0f,
                halfHeight * 2.0f
            });
        }
    }

    Rectangle GetCharacterFluidBounds(Rectangle gameplayBounds) {
        // Character collision boxes deliberately include forgiving space around
        // the sprite. Sand should meet the visible body instead of that hidden box.
        float horizontalInset = fminf(4.0f, gameplayBounds.width * 0.18f);
        return {
            gameplayBounds.x + horizontalInset,
            gameplayBounds.y + 1.0f,
            fmaxf(1.0f, gameplayBounds.width - horizontalInset * 2.0f),
            fmaxf(1.0f, gameplayBounds.height - 1.0f)
        };
    }

    void AppendCircularFluidObstacle(std::vector<Rectangle>& obstacles, Vector2 center, float radius) {
        // Horizontal one-pixel chords preserve the round silhouette instead of
        // excluding material from the circle's entire square bounding box.
        int sliceCount = std::max(1, static_cast<int>(ceilf(radius * 2.0f)));
        float sliceHeight = radius * 2.0f / static_cast<float>(sliceCount);
        for (int slice = 0; slice < sliceCount; slice++) {
            float y = center.y - radius + static_cast<float>(slice) * sliceHeight;
            float sampleY = y + sliceHeight * 0.5f;
            float offsetY = sampleY - center.y;
            float halfWidth = sqrtf(fmaxf(0.0f, radius * radius - offsetY * offsetY));
            obstacles.push_back({center.x - halfWidth, y, halfWidth * 2.0f, sliceHeight});
        }
    }

    std::vector<Rectangle> BuildFluidObstacles(
        const Level& level,
        const Player* player1,
        const Player* player2,
        const Player* player3
    ) {
        std::vector<Rectangle> obstacles = BuildSolids(level);
        if (player1 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player1->rect));
        if (player2 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player2->rect));
        if (player3 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player3->rect));

        for (const StoneBlock& block : level.stoneBlocks) obstacles.push_back(block.rect);
        for (const Boulder& boulder : level.boulders) {
            AppendCircularFluidObstacle(obstacles, boulder.center, boulder.radius);
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            AppendCircularFluidObstacle(obstacles, wheel.center, wheel.radius);
        }
        for (const Gear& gear : level.gears) {
            AppendCircularFluidObstacle(obstacles, gear.center, gear.radius * 1.12f);
        }
        for (const Flywheel& flywheel : level.flywheels) {
            AppendCircularFluidObstacle(obstacles, flywheel.center, flywheel.radius);
        }
        for (const HangingWeight& weight : level.weights) obstacles.push_back(weight.rect);
        for (const Enemy& enemy : level.enemies) obstacles.push_back(GetCharacterFluidBounds(enemy.rect));
        for (const Fan& fan : level.fans) {
            obstacles.push_back({fan.center.x - 22.0f, fan.center.y - 22.0f, 44.0f, 44.0f});
        }

        for (const Ramp& ramp : level.ramps) {
            AppendOrientedFluidObstacle(obstacles, ramp.center, ramp.length, ramp.thickness, ramp.angle);
        }
        for (const SeeSaw& seeSaw : level.seeSaws) {
            AppendOrientedFluidObstacle(obstacles, seeSaw.pivot, seeSaw.length, seeSaw.thickness, seeSaw.angle);
        }
        for (const TrapDoor& trapDoor : level.trapDoors) {
            Vector2 ring = GetTrapDoorRingPosition(trapDoor);
            Vector2 center{(trapDoor.hinge.x + ring.x) * 0.5f, (trapDoor.hinge.y + ring.y) * 0.5f};
            AppendOrientedFluidObstacle(obstacles, center, trapDoor.length, trapDoor.thickness, trapDoor.angle);
        }
        for (const Screw& screw : level.screws) {
            AppendScrewColliders(obstacles, screw);
        }

        return obstacles;
    }

    float MoveTowardsFloat(float current, float target, float maxDelta) {
        if (current < target) {
            return fminf(current + maxDelta, target);
        }

        return fmaxf(current - maxDelta, target);
    }
}

struct Game::PlayerControls {
    KeyboardKey left;
    KeyboardKey right;
    KeyboardKey up;
    KeyboardKey down;
    KeyboardKey jump;
    int gamepad;
    GamepadButton upButton;
    GamepadButton downButton;
    GamepadButton jumpButton;
};

void Game::Run() {
    Load();

    while (!shouldQuit && !WindowShouldClose()) {
        Update(GetFrameTime());
        Draw();
    }

    Unload();
}

void Game::ResetPendingSettings() {
    pendingWindowMode = windowMode;
    pendingResolutionIndex = selectedResolutionIndex;
    pendingAdvancedFluidSimulation = advancedFluidSimulation;
    pendingVsyncEnabled = vsyncEnabled;
    pendingFrameRateIndex = frameRateIndex;
    pendingUiScaleIndex = uiScaleIndex;
    pendingPixelPerfectScaling = pixelPerfectScaling;
    pendingMasterVolume = masterVolume;
    pendingMusicVolume = musicVolume;
    pendingSoundEffectsVolume = soundEffectsVolume;
    pendingAudioMuted = audioMuted;
    pendingPlayer1Bindings = player1Bindings;
    pendingControllerEnabled = controllerEnabled;
    pendingControllerVibrationEnabled = controllerVibrationEnabled;
    pendingScreenShakeSetting = screenShakeSetting;
    pendingReducedFlashing = reducedFlashing;
    pendingHighContrast = highContrast;
    pendingColorblindSetting = colorblindSetting;
    settingsDropdown = SettingsDropdown::None;
    settingsBindingCapture = -1;
}

void Game::OpenSettingsPopup() {
    ResetPendingSettings();
    settingsPage = SettingsPage::Display;
    settingsPopupOpen = true;
    menuMessage.clear();
}

void Game::ApplyPendingSettings() {
    const bool fluidModeChanged = advancedFluidSimulation != pendingAdvancedFluidSimulation;
    const bool windowNeedsReconfigure = windowMode != pendingWindowMode ||
        (pendingWindowMode != WindowModeSetting::Borderless && selectedResolutionIndex != pendingResolutionIndex);

    windowMode = pendingWindowMode;
    selectedResolutionIndex = pendingResolutionIndex;
    advancedFluidSimulation = pendingAdvancedFluidSimulation;
    vsyncEnabled = pendingVsyncEnabled;
    frameRateIndex = std::clamp(pendingFrameRateIndex, 0, kFrameRateCount - 1);
    uiScaleIndex = std::clamp(pendingUiScaleIndex, 0, kUiScaleCount - 1);
    pixelPerfectScaling = pendingPixelPerfectScaling;
    masterVolume = std::clamp(pendingMasterVolume, 0.0f, 1.0f);
    musicVolume = std::clamp(pendingMusicVolume, 0.0f, 1.0f);
    soundEffectsVolume = std::clamp(pendingSoundEffectsVolume, 0.0f, 1.0f);
    audioMuted = pendingAudioMuted;
    player1Bindings = pendingPlayer1Bindings;
    controllerEnabled = pendingControllerEnabled;
    controllerVibrationEnabled = pendingControllerVibrationEnabled;
    screenShakeSetting = pendingScreenShakeSetting;
    reducedFlashing = pendingReducedFlashing;
    highContrast = pendingHighContrast;
    colorblindSetting = pendingColorblindSetting;

    gPixelPerfectScaling = pixelPerfectScaling;
    gUiScale = kUiScaleValues[uiScaleIndex];
    if (sceneTarget.id > 0) {
        SetTextureFilter(sceneTarget.texture, pixelPerfectScaling ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    }
    SetTargetFPS(kFrameRateValues[frameRateIndex]);
    if (vsyncEnabled) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    if (IsAudioDeviceReady()) SetMasterVolume(audioMuted ? 0.0f : masterVolume);

    if (windowNeedsReconfigure) {
        if (IsWindowFullscreen()) ToggleFullscreen();
        if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();

        const ResolutionPreset& preset = GetResolutionPreset(selectedResolutionIndex);
        SetWindowSize(preset.width, preset.height);
        if (windowMode == WindowModeSetting::Borderless) {
            ToggleBorderlessWindowed();
        }
        else if (windowMode == WindowModeSetting::Fullscreen) {
            ToggleFullscreen();
        }
    }

    if (fluidModeChanged) {
        const Player* activePlayer1 = playerAlive ? &player : nullptr;
        const Player* activePlayer2 = multiplayerEnabled && player2Alive ? &player2 : nullptr;
        const Player* activePlayer3 = threePlayerEnabled && player3Alive ? &player3 : nullptr;
        std::vector<Rectangle> obstacles = BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3);
        for (FluidField& fluid : level.fluids) {
            InitializeFluidField(fluid, obstacles, SelectedFluidMode(advancedFluidSimulation));
        }
    }
}

void Game::Load() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(Constants::ScreenWidth, Constants::ScreenHeight, "Spin to Win - Power Pulley Panic");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();
    if (IsAudioDeviceReady()) SetMasterVolume(masterVolume);
    sceneTarget = LoadRenderTexture(Constants::ScreenWidth, Constants::ScreenHeight);
    if (sceneTarget.id > 0) SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_POINT);
    selectedResolutionIndex = FindResolutionPresetIndex(Constants::ScreenWidth, Constants::ScreenHeight);
    ResetPendingSettings();

    playerSpritesTexture = LoadTexture("assets/first_party/characters/Player_Sprites.png");
    skullTexture = LoadTexture("assets/first_party/characters/skull.png");
    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");
    industrialFarBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/3_Far_Background_Tile.png");
    chainLinksTexture = LoadTexture("assets/first_party/machines/chain_links.png");
    // Placeholder only: swap this before final enemy art lock.
    enemyPlaceholderTexture = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/6. Character Animations 32p/Anim_Robot_Walk1_v1.1_spritesheet.png");

    if (playerSpritesTexture.id > 0) SetTextureFilter(playerSpritesTexture, TEXTURE_FILTER_POINT);
    if (skullTexture.id > 0) SetTextureFilter(skullTexture, TEXTURE_FILTER_POINT);
    if (industrialTiles.id > 0) SetTextureFilter(industrialTiles, TEXTURE_FILTER_POINT);
    if (industrialBackground.id > 0) SetTextureFilter(industrialBackground, TEXTURE_FILTER_POINT);
    if (industrialFarBackground.id > 0) SetTextureFilter(industrialFarBackground, TEXTURE_FILTER_POINT);
    if (chainLinksTexture.id > 0) SetTextureFilter(chainLinksTexture, TEXTURE_FILTER_POINT);
    if (enemyPlaceholderTexture.id > 0) SetTextureFilter(enemyPlaceholderTexture, TEXTURE_FILTER_POINT);

    InitializeOverworld();
    Reset();
}

void Game::Reset() {
    std::string levelId = "gatehouse";
    if (currentLevelNode >= 0 && currentLevelNode < static_cast<int>(overworldNodes.size())) {
        levelId = overworldNodes[currentLevelNode].id;
    }

    Level fallback = CreatePowerPulleyPanicLevel();
    if (levelId == "rotary_latch_lab") {
        fallback = CreateRotaryLatchLabLevel();
    }
    else if (levelId == "lower_works") {
        fallback = CreateFloodedFoundryLevel();
    }
    level = LoadLevelFromFile("game_data/levels/" + levelId + ".level", fallback);
    for (RotaryLatch& latch : level.rotaryLatches) {
        ResetRotaryLatch(latch);
    }
    for (Chain& chain : level.chains) {
        InitializeChain(chain);
    }
    for (PhysicsRope& rope : level.physicsRopes) {
        InitializePhysicsRope(rope);
    }

    ResetPlayer(player);
    ResetPlayer(player2);
    ResetPlayer(player3);
    player.rect.x = level.playerStart.x;
    player.rect.y = level.playerStart.y;
    player2.rect.x = level.playerStart.x;
    player2.rect.y = level.playerStart.y;
    player3.rect.x = level.playerStart.x;
    player3.rect.y = level.playerStart.y;
    if (!IsTilesetReferenceLevel(level)) {
        player2.rect.x += 44.0f;
        player3.rect.x += 88.0f;
    }
    const Player* initialPlayer2 = multiplayerEnabled ? &player2 : nullptr;
    const Player* initialPlayer3 = threePlayerEnabled ? &player3 : nullptr;
    std::vector<Rectangle> fluidObstacles = BuildFluidObstacles(level, &player, initialPlayer2, initialPlayer3);
    for (FluidField& fluid : level.fluids) {
        InitializeFluidField(fluid, fluidObstacles, SelectedFluidMode(advancedFluidSimulation));
    }
    deathRect = player.rect;
    playerDeathRect = player.rect;
    player2DeathRect = player2.rect;
    player3DeathRect = player3.rect;

    machineWinch.rect.x = machineWinch.startX;
    machineWinch.grabbed = false;

    won = false;
    lost = false;
    playerAlive = true;
    player2Alive = true;
    player3Alive = true;
    pulleyRotation = 0.0f;
    machinePhase = 0.0f;
    machinePower = 0.0f;
    gateBottom = HasArea(level.exitTrigger) ? level.exitTrigger.y + level.exitTrigger.height : 650.0f;
    levelClearTimer = 0.0f;
}

void Game::StartGame() {
    Reset();
    mode = GameMode::Playing;
    menuMessage.clear();
}

void Game::InitializeOverworld() {
    overworldNodes = {
        {"gatehouse", "1", "Gatehouse Generator", {285.0f, 545.0f}, true, false},
        {"rotary_latch_lab", "2", "Rotary Latch Lab", {470.0f, 465.0f}, false, false},
        {"lower_works", "3", "Flooded Lower Works", {670.0f, 535.0f}, false, false},
        {"counterweight_row", "4", "Counterweight Row", {875.0f, 405.0f}, false, false},
        {"foundry_lift", "5", "Foundry Lift", {1095.0f, 480.0f}, false, false},
        {"clocktower_core", "6", "Clocktower Core", {1310.0f, 335.0f}, false, false},
        {"test_level", "T", "Test Level", {1410.0f, 430.0f}, true, false},
        {"tileset_reference", "R", "Tileset Reference", {1510.0f, 330.0f}, true, false}
    };

    overworldPaths = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5},
        {6, 7}
    };

    selectedOverworldNode = 0;
}

void Game::OpenOverworld() {
    if (overworldNodes.empty()) {
        InitializeOverworld();
    }

    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);
    mode = GameMode::Overworld;
    titleModeMenuOpen = false;
    quitConfirmationOpen = false;
    menuMessage.clear();
}

void Game::StartSelectedOverworldLevel() {
    if (overworldNodes.empty()) return;

    const OverworldNode& node = overworldNodes[selectedOverworldNode];
    if (!node.unlocked) {
        menuMessage = node.name + " is locked.";
        return;
    }

    currentLevelNode = selectedOverworldNode;
    StartGame();
}

void Game::BeginLevelClear() {
    won = true;
    lost = false;
    levelClearTimer = 1.65f;
}

void Game::CompleteCurrentLevelAndReturnToMap() {
    if (!overworldNodes.empty()) {
        currentLevelNode = std::clamp(currentLevelNode, 0, static_cast<int>(overworldNodes.size()) - 1);
        overworldNodes[currentLevelNode].completed = true;
        overworldNodes[currentLevelNode].unlocked = true;

        int nextNode = currentLevelNode + 1;
        if (nextNode < static_cast<int>(overworldNodes.size())) {
            overworldNodes[nextNode].unlocked = true;
            selectedOverworldNode = nextNode;
            menuMessage = overworldNodes[currentLevelNode].name + " complete. " + overworldNodes[nextNode].name + " unlocked.";
        }
        else {
            selectedOverworldNode = currentLevelNode;
            menuMessage = overworldNodes[currentLevelNode].name + " complete.";
        }
    }

    mode = GameMode::Overworld;
    won = false;
    lost = false;
    levelClearTimer = 0.0f;
}

void Game::Update(float dt) {
    dt = std::clamp(dt, 0.0f, 1.0f / 30.0f);
    screenShakeTimer = fmaxf(0.0f, screenShakeTimer - dt);

    bool consoleToggled = false;
    if (IsKeyPressed(KEY_GRAVE)) {
        console.Toggle();
        consoleToggled = true;
    }

    if (console.IsOpen()) {
        if (!consoleToggled) {
            console.Update();
        }

        std::string commandLine;
        while (console.ConsumeSubmittedLine(commandLine)) {
            ExecuteConsoleCommand(commandLine);
        }

        return;
    }

    if (quitConfirmationOpen) {
        UpdateQuitConfirmation();
        return;
    }

    if (settingsPopupOpen) {
        UpdateSettingsPopup();
        return;
    }

    if (mode == GameMode::Title) {
        UpdateTitle();
        return;
    }

    if (mode == GameMode::Overworld) {
        UpdateOverworld();
        return;
    }

    if (mode == GameMode::Paused) {
        UpdatePaused();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        mode = GameMode::Paused;
        return;
    }

    if (IsKeyPressed(KEY_F)) showFPS = !showFPS;

    if (lost) {
        UpdateGameOverActions();
        return;
    }

    if (won) {
        levelClearTimer -= dt;
        UpdateMachines(dt, {}, {}, {});
        if (levelClearTimer <= 0.0f) {
            CompleteCurrentLevelAndReturnToMap();
        }
        return;
    }

    const int playerOneGamepad = controllerEnabled && IsGamepadAvailable(0) ? 0 : -1;
    const PlayerControls PlayerOneControls{
        player1Bindings.left,
        player1Bindings.right,
        player1Bindings.up,
        player1Bindings.down,
        player1Bindings.jump,
        playerOneGamepad,
        GAMEPAD_BUTTON_LEFT_FACE_UP,
        GAMEPAD_BUTTON_LEFT_FACE_DOWN,
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN
    };
    constexpr PlayerControls PlayerTwoControls{
        KEY_J, KEY_L, KEY_I, KEY_K, KEY_NULL, -1,
        GAMEPAD_BUTTON_UNKNOWN, GAMEPAD_BUTTON_UNKNOWN, GAMEPAD_BUTTON_UNKNOWN
    };
    constexpr PlayerControls PlayerThreeControls{
        KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_NULL, -1,
        GAMEPAD_BUTTON_UNKNOWN, GAMEPAD_BUTTON_UNKNOWN, GAMEPAD_BUTTON_UNKNOWN
    };

    PlayerMachineInput player1Input{};
    PlayerMachineInput player2Input{};
    PlayerMachineInput player3Input{};
    if (!won && !lost) {
        if (IsKeyDown(player1Bindings.left)) player1Input.moveInput -= 1.0f;
        if (IsKeyDown(player1Bindings.right)) player1Input.moveInput += 1.0f;
        if (playerOneGamepad >= 0) {
            const float axis = GetGamepadAxisMovement(playerOneGamepad, GAMEPAD_AXIS_LEFT_X);
            if (fabsf(axis) >= 0.2f) player1Input.moveInput = axis;
            if (IsGamepadButtonDown(playerOneGamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) player1Input.moveInput = -1.0f;
            if (IsGamepadButtonDown(playerOneGamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) player1Input.moveInput = 1.0f;
        }
        if (IsKeyDown(KEY_J)) player2Input.moveInput -= 1.0f;
        if (IsKeyDown(KEY_L)) player2Input.moveInput += 1.0f;
        if (IsKeyDown(KEY_LEFT)) player3Input.moveInput -= 1.0f;
        if (IsKeyDown(KEY_RIGHT)) player3Input.moveInput += 1.0f;

        player1Input.interactHeld = IsKeyDown(player1Bindings.interact) ||
            (playerOneGamepad >= 0 && IsGamepadButtonDown(playerOneGamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        player1Input.interactPressed = IsKeyPressed(player1Bindings.interact) ||
            (playerOneGamepad >= 0 && IsGamepadButtonPressed(playerOneGamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        player1Input.interactReleased = IsKeyReleased(player1Bindings.interact) ||
            (playerOneGamepad >= 0 && IsGamepadButtonReleased(playerOneGamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        player2Input.interactHeld = IsKeyDown(KEY_U);
        player2Input.interactPressed = IsKeyPressed(KEY_U);
        player2Input.interactReleased = IsKeyReleased(KEY_U);
        player3Input.interactHeld = IsKeyDown(KEY_RIGHT_CONTROL);
        player3Input.interactPressed = IsKeyPressed(KEY_RIGHT_CONTROL);
        player3Input.interactReleased = IsKeyReleased(KEY_RIGHT_CONTROL);
    }

    if (playerAlive) {
        UpdatePlayer(player, PlayerOneControls, dt, player1Input.moveInput);
    }
    if (multiplayerEnabled && player2Alive) {
        UpdatePlayer(player2, PlayerTwoControls, dt, player2Input.moveInput);
    }
    if (threePlayerEnabled && player3Alive) {
        UpdatePlayer(player3, PlayerThreeControls, dt, player3Input.moveInput);
    }
    UpdateMachines(dt, player1Input, player2Input, player3Input);
    UpdateEnemies(dt);
    CheckFailureConditions();

    CheckWinCondition(gateBottom);
}

void Game::UpdateTitle() {
    if (titleModeMenuOpen) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            titleModeMenuOpen = false;
            menuMessage.clear();
            return;
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            multiplayerEnabled = false;
            threePlayerEnabled = false;
            OpenOverworld();
        }

        std::vector<MenuButton> buttons{
            {{1080, 530, 310, 46}, "Single Player"},
            {{1080, 586, 310, 46}, "2 Players"},
            {{1080, 642, 310, 46}, "3 Players"},
            {{1080, 698, 310, 46}, "Back"}
        };

        if (WasButtonPressed(buttons[0])) {
            multiplayerEnabled = false;
            threePlayerEnabled = false;
            OpenOverworld();
        }
        else if (WasButtonPressed(buttons[1])) {
            multiplayerEnabled = true;
            threePlayerEnabled = false;
            OpenOverworld();
        }
        else if (WasButtonPressed(buttons[2])) {
            multiplayerEnabled = true;
            threePlayerEnabled = true;
            OpenOverworld();
        }
        else if (WasButtonPressed(buttons[3])) {
            titleModeMenuOpen = false;
            menuMessage.clear();
        }

        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        titleModeMenuOpen = true;
        menuMessage.clear();
    }

    std::vector<MenuButton> buttons{
        {{1080, 510, 310, 46}, "New Game"},
        {{1080, 566, 310, 46}, "Continue"},
        {{1080, 622, 310, 46}, "Load Custom"},
        {{1080, 678, 310, 46}, "Settings"},
        {{1080, 734, 310, 46}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        titleModeMenuOpen = true;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[1])) {
        multiplayerEnabled = false;
        threePlayerEnabled = false;
        OpenOverworld();
    }
    else if (WasButtonPressed(buttons[2])) {
        menuMessage = "Custom level loading is not available yet.";
    }
    else if (WasButtonPressed(buttons[3])) {
        OpenSettingsPopup();
    }
    else if (WasButtonPressed(buttons[4])) {
        quitConfirmationOpen = true;
        titleModeMenuOpen = false;
        menuMessage.clear();
    }
}

void Game::UpdateOverworld() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        menuMessage.clear();
        return;
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
        selectedOverworldNode = std::max(0, selectedOverworldNode - 1);
        menuMessage.clear();
    }

    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
        selectedOverworldNode = std::min(static_cast<int>(overworldNodes.size()) - 1, selectedOverworldNode + 1);
        menuMessage.clear();
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        StartSelectedOverworldLevel();
    }

    std::vector<MenuButton> buttons{
        {{1240, 760, 250, 46}, "Back to Title"},
        {{1240, 816, 250, 46}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[1])) {
        quitConfirmationOpen = true;
        menuMessage.clear();
    }

    Vector2 mouse = GetUiMousePosition();
    for (int i = 0; i < static_cast<int>(overworldNodes.size()); i++) {
        if (CheckCollisionPointCircle(mouse, overworldNodes[i].position, 34.0f) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedOverworldNode = i;
            StartSelectedOverworldLevel();
            break;
        }
    }
}

void Game::UpdatePaused() {
    if (controlsPopupOpen) {
        UpdateControlsPopup();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        mode = GameMode::Playing;
    }

    if (IsKeyPressed(KEY_T)) {
        mode = GameMode::Title;
    }

    std::vector<MenuButton> buttons{
        {{625, 400, 350, 46}, "Resume"},
        {{625, 456, 350, 46}, "Restart Level"},
        {{625, 512, 350, 46}, "Controls"},
        {{625, 568, 350, 46}, "Settings"},
        {{625, 624, 350, 46}, "Return to Map"},
        {{625, 680, 350, 46}, "Return to Title Screen"},
        {{625, 736, 350, 46}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        mode = GameMode::Playing;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[1])) {
        StartGame();
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[2])) {
        controlsPopupOpen = true;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[3])) {
        OpenSettingsPopup();
    }
    else if (WasButtonPressed(buttons[4])) {
        OpenOverworld();
    }
    else if (WasButtonPressed(buttons[5])) {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[6])) {
        quitConfirmationOpen = true;
        menuMessage.clear();
    }
}

void Game::UpdateGameOverActions() {
    std::vector<MenuButton> buttons{
        {{625, 535, 350, 46}, "Restart Level"},
        {{625, 591, 350, 46}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        StartGame();
    }
    else if (WasButtonPressed(buttons[1])) {
        quitConfirmationOpen = true;
    }
}

void Game::UpdateControlsPopup() {
    std::vector<MenuButton> buttons{
        {{705, 775, 190, 46}, "Close"}
    };

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || WasButtonPressed(buttons[0])) {
        controlsPopupOpen = false;
    }
}

void Game::UpdateSettingsPopup() {
    const SettingsMenuLayout layout = GetSettingsMenuLayout();
    const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    const bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const Vector2 mouse = GetUiMousePosition();
    const auto clicked = [&](Rectangle bounds) {
        return mousePressed && CheckCollisionPointRec(mouse, bounds);
    };

    KeyboardKey* bindingTargets[] = {
        &pendingPlayer1Bindings.left,
        &pendingPlayer1Bindings.right,
        &pendingPlayer1Bindings.up,
        &pendingPlayer1Bindings.down,
        &pendingPlayer1Bindings.jump,
        &pendingPlayer1Bindings.interact
    };
    if (settingsBindingCapture >= 0) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            settingsBindingCapture = -1;
            return;
        }
        const int key = GetKeyPressed();
        if (key != 0) {
            *bindingTargets[settingsBindingCapture] = static_cast<KeyboardKey>(key);
            settingsBindingCapture = -1;
        }
        return;
    }

    if (settingsDropdown != SettingsDropdown::None) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            settingsDropdown = SettingsDropdown::None;
            return;
        }
        if (!mousePressed) return;

        Rectangle anchor{};
        int optionCount = 0;
        int columns = 1;
        switch (settingsDropdown) {
        case SettingsDropdown::WindowMode: anchor = layout.controls[0]; optionCount = 3; break;
        case SettingsDropdown::Resolution: anchor = layout.controls[1]; optionCount = ResolutionPresetCount(); columns = 2; break;
        case SettingsDropdown::FrameRate: anchor = layout.controls[3]; optionCount = kFrameRateCount; break;
        case SettingsDropdown::UiScale: anchor = layout.controls[4]; optionCount = kUiScaleCount; break;
        case SettingsDropdown::ScreenShake: anchor = layout.controls[0]; optionCount = 3; break;
        case SettingsDropdown::ColorblindMode: anchor = layout.controls[3]; optionCount = 4; break;
        default: break;
        }
        const DropdownLayout dropdown = GetDropdownLayout(anchor, optionCount, columns);
        for (int i = 0; i < optionCount; ++i) {
            if (!clicked(dropdown.options[i])) continue;
            switch (settingsDropdown) {
            case SettingsDropdown::WindowMode: pendingWindowMode = static_cast<WindowModeSetting>(i); break;
            case SettingsDropdown::Resolution: pendingResolutionIndex = i; break;
            case SettingsDropdown::FrameRate: pendingFrameRateIndex = i; break;
            case SettingsDropdown::UiScale: pendingUiScaleIndex = i; break;
            case SettingsDropdown::ScreenShake: pendingScreenShakeSetting = static_cast<ScreenShakeSetting>(i); break;
            case SettingsDropdown::ColorblindMode: pendingColorblindSetting = static_cast<ColorblindSetting>(i); break;
            default: break;
            }
            break;
        }
        settingsDropdown = SettingsDropdown::None;
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        settingsPopupOpen = false;
        ResetPendingSettings();
        menuMessage.clear();
        return;
    }

    if (mousePressed) {
        for (int i = 0; i < static_cast<int>(layout.tabs.size()); ++i) {
            if (clicked(layout.tabs[i])) {
                settingsPage = static_cast<SettingsPage>(i);
                return;
            }
        }
    }

    if (settingsPage == SettingsPage::Display && mousePressed) {
        if (clicked(layout.controls[0])) settingsDropdown = SettingsDropdown::WindowMode;
        else if (clicked(layout.controls[1])) settingsDropdown = SettingsDropdown::Resolution;
        else if (clicked(layout.controls[2])) pendingVsyncEnabled = !pendingVsyncEnabled;
        else if (clicked(layout.controls[3])) settingsDropdown = SettingsDropdown::FrameRate;
        else if (clicked(layout.controls[4])) settingsDropdown = SettingsDropdown::UiScale;
        else if (clicked(layout.controls[5])) pendingPixelPerfectScaling = !pendingPixelPerfectScaling;
        else if (clicked(layout.controls[6])) pendingAdvancedFluidSimulation = !pendingAdvancedFluidSimulation;
        if (settingsDropdown != SettingsDropdown::None || clicked(layout.controls[2]) ||
            clicked(layout.controls[5]) || clicked(layout.controls[6])) return;
    }
    else if (settingsPage == SettingsPage::Audio) {
        float* volumeValues[] = {&pendingMasterVolume, &pendingMusicVolume, &pendingSoundEffectsVolume};
        for (int i = 0; i < 3; ++i) {
            Rectangle track{
                layout.controls[i].x + 150.0f,
                layout.controls[i].y + 14.0f,
                layout.controls[i].width - 220.0f,
                16.0f
            };
            Rectangle hitArea{track.x, layout.controls[i].y, track.width, layout.controls[i].height};
            if (mouseDown && CheckCollisionPointRec(mouse, hitArea)) {
                *volumeValues[i] = std::clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
                return;
            }
        }
        if (clicked(layout.controls[3])) {
            pendingAudioMuted = !pendingAudioMuted;
            return;
        }
    }
    else if (settingsPage == SettingsPage::Controls && mousePressed) {
        for (int i = 0; i < 6; ++i) {
            if (clicked(layout.controls[i])) {
                settingsBindingCapture = i;
                return;
            }
        }
        if (clicked(layout.controls[6])) {
            pendingControllerEnabled = !pendingControllerEnabled;
            return;
        }
        if (clicked(layout.controls[7])) {
            pendingControllerVibrationEnabled = !pendingControllerVibrationEnabled;
            return;
        }
    }
    else if (settingsPage == SettingsPage::Accessibility && mousePressed) {
        if (clicked(layout.controls[0])) settingsDropdown = SettingsDropdown::ScreenShake;
        else if (clicked(layout.controls[1])) pendingReducedFlashing = !pendingReducedFlashing;
        else if (clicked(layout.controls[2])) pendingHighContrast = !pendingHighContrast;
        else if (clicked(layout.controls[3])) settingsDropdown = SettingsDropdown::ColorblindMode;
        if (settingsDropdown != SettingsDropdown::None || clicked(layout.controls[1]) || clicked(layout.controls[2])) return;
    }

    if (clicked(layout.applyButton)) {
        ApplyPendingSettings();
        settingsPopupOpen = false;
        settingsDropdown = SettingsDropdown::None;
        menuMessage = "Settings applied.";
    }
    else if (clicked(layout.closeButton)) {
        settingsPopupOpen = false;
        ResetPendingSettings();
        menuMessage.clear();
    }
}

void Game::UpdateQuitConfirmation() {
    std::vector<MenuButton> buttons{
        {{610, 500, 170, 46}, "Yes"},
        {{820, 500, 170, 46}, "No"}
    };

    if (IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER) || WasButtonPressed(buttons[0])) {
        shouldQuit = true;
    }
    else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE) || WasButtonPressed(buttons[1])) {
        quitConfirmationOpen = false;
    }
}

void Game::UpdatePlayer(Player& activePlayer, const PlayerControls& controls, float dt, float moveInput) {
    bool controlsEnabled = !won && !lost;
    const auto controlDown = [&](KeyboardKey key, GamepadButton button) {
        if (IsControlDown(key)) return true;
        if (controls.gamepad < 0 || button == GAMEPAD_BUTTON_UNKNOWN) return false;
        const float verticalAxis = GetGamepadAxisMovement(controls.gamepad, GAMEPAD_AXIS_LEFT_Y);
        if (button == controls.upButton && verticalAxis <= -0.35f) return true;
        if (button == controls.downButton && verticalAxis >= 0.35f) return true;
        return IsGamepadButtonDown(controls.gamepad, button);
    };
    const auto controlPressed = [&](KeyboardKey key, GamepadButton button) {
        if (IsControlPressed(key)) return true;
        if (controls.gamepad < 0 || button == GAMEPAD_BUTTON_UNKNOWN) return false;
        return IsGamepadButtonPressed(controls.gamepad, button);
    };
    const auto controlReleased = [&](KeyboardKey key, GamepadButton button) {
        if (IsControlReleased(key)) return true;
        if (controls.gamepad < 0 || button == GAMEPAD_BUTTON_UNKNOWN) return false;
        return IsGamepadButtonReleased(controls.gamepad, button);
    };

    if (moveInput < 0.0f) {
        activePlayer.facingRight = false;
    }
    else if (moveInput > 0.0f) {
        activePlayer.facingRight = true;
    }

    bool onLadder = CheckCollisionRecs(activePlayer.rect, level.ladder);
    FluidSample simulatedWater = SampleFluidAroundRectangle(level, FluidType::Water, activePlayer.rect);
    bool swimming = IsPlayerSwimming(activePlayer, level);
    activePlayer.climbing = onLadder && !swimming &&
        (controlDown(controls.up, controls.upButton) || controlDown(controls.down, controls.downButton)) && controlsEnabled;

    float maxMoveSpeed = swimming ? 170.0f : Constants::PlayerSpeed;
    float targetSpeed = controlsEnabled ? moveInput * maxMoveSpeed : 0.0f;
    bool applyingInput = fabsf(targetSpeed) > 0.0f;
    activePlayer.walking = (applyingInput || activePlayer.climbing) && controlsEnabled;
    bool changingDirection = (targetSpeed < 0.0f && activePlayer.velocity.x > 0.0f) || (targetSpeed > 0.0f && activePlayer.velocity.x < 0.0f);
    float acceleration = swimming
        ? (applyingInput ? 820.0f : 620.0f)
        : activePlayer.onGround
        ? (applyingInput ? Constants::PlayerGroundAcceleration : Constants::PlayerGroundDeceleration)
        : (applyingInput ? Constants::PlayerAirAcceleration : Constants::PlayerAirDeceleration);
    if (changingDirection && activePlayer.onGround && !swimming) {
        acceleration = Constants::PlayerGroundDeceleration;
    }
    activePlayer.velocity.x = ApproachFloat(activePlayer.velocity.x, targetSpeed, acceleration * dt);

    if (activePlayer.onGround) {
        activePlayer.coyoteTimer = Constants::CoyoteTime;
    }
    else {
        activePlayer.coyoteTimer -= dt;
    }

    bool jumpPressed = controlPressed(controls.up, controls.upButton) ||
        controlPressed(controls.jump, controls.jumpButton);
    if (jumpPressed && controlsEnabled) {
        activePlayer.jumpBufferTimer = Constants::JumpBufferTime;
    }
    else {
        activePlayer.jumpBufferTimer -= dt;
    }

    if (swimming && controlsEnabled) {
        float centerX = activePlayer.rect.x + activePlayer.rect.width * 0.5f;
        bool atLegacySurface = HasWaterPit(level) && activePlayer.rect.y <= level.waterPit.surfaceY + 22.0f;
        float waterAbove = SampleFluid(
            level.fluids,
            FluidType::Water,
            {centerX, activePlayer.rect.y - 10.0f}
        ).density;
        bool atSimulatedSurface = IsTouchingSimulatedWaterSurface(level, activePlayer.rect) ||
            (simulatedWater.density >= 0.18f && waterAbove < 0.12f);
        bool atSurface = atLegacySurface || atSimulatedSurface;
        float targetSwimSpeed = atSurface ? 58.0f : 34.0f;
        bool launchingFromSurface = atSurface && jumpPressed;
        if (controlDown(controls.up, controls.upButton) || controlDown(controls.jump, controls.jumpButton)) {
            targetSwimSpeed = -165.0f;
        }
        else if (controlDown(controls.down, controls.downButton)) {
            targetSwimSpeed = 170.0f;
        }

        float swimAcceleration = targetSwimSpeed < 0.0f ? 960.0f : 560.0f;
        activePlayer.velocity.y = launchingFromSurface
            ? Constants::PlayerJumpVelocity
            : ApproachFloat(activePlayer.velocity.y, targetSwimSpeed, swimAcceleration * dt);
        activePlayer.velocity.y = fminf(activePlayer.velocity.y, 210.0f);
        activePlayer.onGround = false;
        activePlayer.coyoteTimer = 0.0f;
        if (launchingFromSurface) {
            activePlayer.jumpBufferTimer = 0.0f;
        }
    }
    else if (onLadder && controlDown(controls.up, controls.upButton) && controlsEnabled) {
        activePlayer.velocity.y = -Constants::PlayerClimbSpeed;
    }
    else if (onLadder && controlDown(controls.down, controls.downButton) && controlsEnabled) {
        activePlayer.velocity.y = Constants::PlayerClimbSpeed;
    }
    else {
        activePlayer.velocity.y += Constants::Gravity * dt;
        activePlayer.velocity.y = fminf(activePlayer.velocity.y, Constants::PlayerMaxFallSpeed);
    }

    ApplyWindToVelocity(activePlayer.velocity, RectCenter(activePlayer.rect), 1.0f, level, dt, swimming ? 0.22f : 0.55f);
    ApplyFluidForcesToVelocity(
        activePlayer.velocity,
        activePlayer.rect,
        1.0f,
        swimming ? 0.08f : 0.85f,
        swimming ? 4.8f : 2.4f,
        level,
        dt
    );

    if (activePlayer.jumpBufferTimer > 0.0f && activePlayer.coyoteTimer > 0.0f && !onLadder && !swimming && controlsEnabled) {
        activePlayer.velocity.y = Constants::PlayerJumpVelocity;
        activePlayer.onGround = false;
        activePlayer.coyoteTimer = 0.0f;
        activePlayer.jumpBufferTimer = 0.0f;
    }

    if ((controlReleased(controls.up, controls.upButton) || controlReleased(controls.jump, controls.jumpButton)) &&
        activePlayer.velocity.y < 0.0f && !onLadder && !swimming && controlsEnabled) {
        activePlayer.velocity.y *= Constants::PlayerJumpCutMultiplier;
    }

    if (activePlayer.walking) {
        activePlayer.animationTimer += dt;
    }
    else {
        activePlayer.animationTimer = 0.0f;
    }

    if (controlsEnabled) {
        std::vector<Rectangle> solids = BuildSolids(level);
        AppendFlexibleObjectColliders(solids, level);

        activePlayer.rect.x += activePlayer.velocity.x * dt;
        ResolveHorizontal(activePlayer, solids);

        for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
            StoneBlock& block = level.stoneBlocks[i];
            if (!CheckCollisionRecs(activePlayer.rect, block.rect)) {
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                block.velocity.x = activePlayer.velocity.x * GetStoneBlockPushScale(block);
                block.rect.x += block.velocity.x * dt;
                ResolveStoneBlockHorizontal(block, solids);
                activePlayer.rect.x = block.rect.x - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                block.velocity.x = activePlayer.velocity.x * GetStoneBlockPushScale(block);
                block.rect.x += block.velocity.x * dt;
                ResolveStoneBlockHorizontal(block, solids);
                activePlayer.rect.x = block.rect.x + block.rect.width;
            }
        }

        std::vector<Rectangle> boulderPushSolids = solids;

        for (int i = 0; i < static_cast<int>(level.boulders.size()); i++) {
            Boulder& boulder = level.boulders[i];
            if (!CheckCollisionCircleRec(boulder.center, boulder.radius, activePlayer.rect)) {
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                boulder.velocity.x = activePlayer.velocity.x * GetBoulderPushScale(boulder);
                boulder.angularVelocity = boulder.velocity.x / fmaxf(1.0f, boulder.radius) * RAD2DEG;
                boulder.center.x += boulder.velocity.x * dt;
                ResolveBoulderCollisions(boulder, boulderPushSolids);
                activePlayer.rect.x = boulder.center.x - boulder.radius - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                boulder.velocity.x = activePlayer.velocity.x * GetBoulderPushScale(boulder);
                boulder.angularVelocity = boulder.velocity.x / fmaxf(1.0f, boulder.radius) * RAD2DEG;
                boulder.center.x += boulder.velocity.x * dt;
                ResolveBoulderCollisions(boulder, boulderPushSolids);
                activePlayer.rect.x = boulder.center.x + boulder.radius;
            }
        }

        std::vector<Rectangle> wheelPushSolids = solids;

        for (int i = 0; i < static_cast<int>(level.physicsWheels.size()); i++) {
            PhysicsWheel& wheel = level.physicsWheels[i];
            if (!CheckCollisionCircleRec(wheel.center, wheel.radius, activePlayer.rect)) {
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                wheel.velocity.x = activePlayer.velocity.x * GetWheelPushScale(wheel);
                wheel.angularVelocity = wheel.velocity.x / fmaxf(1.0f, wheel.radius) * RAD2DEG;
                wheel.center.x += wheel.velocity.x * dt;
                ResolveWheelCollisions(wheel, wheelPushSolids);
                activePlayer.rect.x = wheel.center.x - wheel.radius - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                wheel.velocity.x = activePlayer.velocity.x * GetWheelPushScale(wheel);
                wheel.angularVelocity = wheel.velocity.x / fmaxf(1.0f, wheel.radius) * RAD2DEG;
                wheel.center.x += wheel.velocity.x * dt;
                ResolveWheelCollisions(wheel, wheelPushSolids);
                activePlayer.rect.x = wheel.center.x + wheel.radius;
            }
        }

        std::vector<Rectangle> gearPushSolids = solids;
        for (int i = 0; i < static_cast<int>(level.gears.size()); i++) {
            Gear& gear = level.gears[i];
            if (!CheckCollisionCircleRec(gear.center, gear.radius * 1.12f, activePlayer.rect)) {
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                gear.velocity.x = activePlayer.velocity.x * GetGearPushScale(gear);
                gear.angularVelocity = gear.velocity.x / fmaxf(1.0f, gear.radius) * RAD2DEG;
                gear.center.x += gear.velocity.x * dt;
                ResolveGearCollisions(gear, gearPushSolids);
                activePlayer.rect.x = gear.center.x - gear.radius * 1.12f - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                gear.velocity.x = activePlayer.velocity.x * GetGearPushScale(gear);
                gear.angularVelocity = gear.velocity.x / fmaxf(1.0f, gear.radius) * RAD2DEG;
                gear.center.x += gear.velocity.x * dt;
                ResolveGearCollisions(gear, gearPushSolids);
                activePlayer.rect.x = gear.center.x + gear.radius * 1.12f;
            }
        }

        std::vector<Rectangle> flywheelPushSolids = solids;
        for (int i = 0; i < static_cast<int>(level.flywheels.size()); i++) {
            Flywheel& flywheel = level.flywheels[i];
            if (!CheckCollisionCircleRec(flywheel.center, flywheel.radius, activePlayer.rect)) {
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                flywheel.velocity.x = activePlayer.velocity.x * GetFlywheelPushScale(flywheel);
                flywheel.angularVelocity += flywheel.velocity.x / fmaxf(1.0f, flywheel.radius) * RAD2DEG;
                flywheel.center.x += flywheel.velocity.x * dt;
                ResolveFlywheelCollisions(flywheel, flywheelPushSolids);
                activePlayer.rect.x = flywheel.center.x - flywheel.radius - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                flywheel.velocity.x = activePlayer.velocity.x * GetFlywheelPushScale(flywheel);
                flywheel.angularVelocity += flywheel.velocity.x / fmaxf(1.0f, flywheel.radius) * RAD2DEG;
                flywheel.center.x += flywheel.velocity.x * dt;
                ResolveFlywheelCollisions(flywheel, flywheelPushSolids);
                activePlayer.rect.x = flywheel.center.x + flywheel.radius;
            }
        }

        activePlayer.rect.y += activePlayer.velocity.y * dt;
        std::vector<Rectangle> playerSolids = solids;
        for (const StoneBlock& block : level.stoneBlocks) {
            playerSolids.push_back(block.rect);
        }
        for (const Boulder& boulder : level.boulders) {
            playerSolids.push_back(GetBoulderBounds(boulder));
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            playerSolids.push_back(GetWheelBounds(wheel));
        }
        for (const Gear& gear : level.gears) {
            playerSolids.push_back(GetGearBounds(gear));
        }
        for (const Flywheel& flywheel : level.flywheels) {
            playerSolids.push_back(GetFlywheelBounds(flywheel));
        }
        for (const Screw& screw : level.screws) {
            AppendScrewColliders(playerSolids, screw);
        }

        ResolveVertical(activePlayer, playerSolids);
        ApplyScrewConveyor(activePlayer.rect, activePlayer.velocity, 1.0f, level.screws, dt);
        ResolveRampStanding(activePlayer.rect, activePlayer.velocity, activePlayer.onGround, level.ramps);
        ResolveTrapDoorStanding(activePlayer.rect, activePlayer.velocity, activePlayer.onGround, level.trapDoors);
        ResolveSeeSawStanding(activePlayer.rect, activePlayer.velocity, activePlayer.onGround, level.seeSaws);
    }

    DisturbFluids(
        level.fluids,
        RectCenter(activePlayer.rect),
        fmaxf(activePlayer.rect.width, activePlayer.rect.height) * 0.85f,
        activePlayer.velocity,
        swimming ? 3.2f : 1.2f,
        dt
    );
}

std::array<bool, 3> Game::UpdateFlexibleEndpointInteractions(
    const std::array<Player*, 3>& players,
    const std::array<PlayerMachineInput, 3>& inputs
) {
    struct EndpointHandle {
        bool* pinned;
        Vector2* target;
        FlexibleEndpointBinding* binding;
        Vector2* point;
        Vector2* previousPoint;
    };

    std::vector<EndpointHandle> endpoints;
    endpoints.reserve((level.chains.size() + level.physicsRopes.size()) * 2);
    for (Chain& chain : level.chains) {
        if (chain.points.size() < 2 || chain.previousPoints.size() != chain.points.size()) {
            InitializeChain(chain);
        }
        endpoints.push_back({&chain.pinStart, &chain.start, &chain.startBinding,
            &chain.points.front(), &chain.previousPoints.front()});
        endpoints.push_back({&chain.pinEnd, &chain.end, &chain.endBinding,
            &chain.points.back(), &chain.previousPoints.back()});
    }
    for (PhysicsRope& rope : level.physicsRopes) {
        if (rope.points.size() < 2 || rope.previousPoints.size() != rope.points.size()) {
            InitializePhysicsRope(rope);
        }
        endpoints.push_back({&rope.pinStart, &rope.start, &rope.startBinding,
            &rope.points.front(), &rope.previousPoints.front()});
        endpoints.push_back({&rope.pinEnd, &rope.end, &rope.endBinding,
            &rope.points.back(), &rope.previousPoints.back()});
    }

    auto setPinnedPosition = [](EndpointHandle& endpoint, Vector2 position) {
        *endpoint.pinned = true;
        *endpoint.target = position;
        *endpoint.point = position;
        *endpoint.previousPoint = position;
    };
    auto clearAnchor = [](FlexibleEndpointBinding& binding) {
        binding.anchorType = FlexibleAnchorType::None;
        binding.objectIndex = -1;
        binding.pointIndex = -1;
    };
    auto getCarryPoint = [](const Player& activePlayer) {
        float direction = activePlayer.facingRight ? 1.0f : -1.0f;
        return Vector2{
            activePlayer.rect.x + activePlayer.rect.width * 0.5f + direction * (activePlayer.rect.width * 0.55f + 5.0f),
            activePlayer.rect.y + activePlayer.rect.height * 0.46f
        };
    };
    auto anchorOccupied = [&](int trapDoorIndex, int ringIndex, const EndpointHandle* ignored) {
        for (const EndpointHandle& endpoint : endpoints) {
            if (&endpoint == ignored) {
                continue;
            }
            const FlexibleEndpointBinding& binding = *endpoint.binding;
            if (binding.carriedByPlayer < 0 &&
                binding.anchorType == FlexibleAnchorType::TrapDoorRing &&
                binding.objectIndex == trapDoorIndex && binding.pointIndex == ringIndex) {
                return true;
            }
        }
        return false;
    };

    for (EndpointHandle& endpoint : endpoints) {
        FlexibleEndpointBinding& binding = *endpoint.binding;
        if (binding.carriedByPlayer >= 0) {
            int playerIndex = binding.carriedByPlayer;
            if (playerIndex < static_cast<int>(players.size()) && players[playerIndex] != nullptr) {
                setPinnedPosition(endpoint, getCarryPoint(*players[playerIndex]));
            }
            else {
                binding.carriedByPlayer = -1;
                *endpoint.pinned = false;
                *endpoint.previousPoint = *endpoint.point;
            }
        }
        else if (binding.anchorType == FlexibleAnchorType::TrapDoorRing) {
            if (binding.objectIndex >= 0 && binding.objectIndex < static_cast<int>(level.trapDoors.size()) &&
                binding.pointIndex >= 0 && binding.pointIndex < 2) {
                std::array<Vector2, 2> rings = GetTrapDoorRingPositions(level.trapDoors[binding.objectIndex]);
                setPinnedPosition(endpoint, rings[binding.pointIndex]);
            }
            else {
                clearAnchor(binding);
                *endpoint.pinned = false;
            }
        }
    }

    std::array<bool, 3> consumed{};
    for (int playerIndex = 0; playerIndex < static_cast<int>(players.size()); playerIndex++) {
        Player* activePlayer = players[playerIndex];
        if (activePlayer == nullptr) {
            continue;
        }

        EndpointHandle* carried = nullptr;
        for (EndpointHandle& endpoint : endpoints) {
            if (endpoint.binding->carriedByPlayer == playerIndex) {
                carried = &endpoint;
                break;
            }
        }

        if (carried != nullptr) {
            consumed[playerIndex] = true;
            if (inputs[playerIndex].interactReleased) {
                Vector2 carryPoint = getCarryPoint(*activePlayer);
                float bestDistanceSquared = 32.0f * 32.0f;
                int bestTrapDoor = -1;
                int bestRing = -1;
                for (int trapDoorIndex = 0; trapDoorIndex < static_cast<int>(level.trapDoors.size()); trapDoorIndex++) {
                    std::array<Vector2, 2> rings = GetTrapDoorRingPositions(level.trapDoors[trapDoorIndex]);
                    for (int ringIndex = 0; ringIndex < static_cast<int>(rings.size()); ringIndex++) {
                        Rectangle anchorBounds{rings[ringIndex].x - 2.0f, rings[ringIndex].y - 2.0f, 4.0f, 4.0f};
                        if (!IsNearRect(activePlayer->rect, anchorBounds, 24.0f) ||
                            anchorOccupied(trapDoorIndex, ringIndex, carried)) {
                            continue;
                        }
                        float dx = rings[ringIndex].x - carryPoint.x;
                        float dy = rings[ringIndex].y - carryPoint.y;
                        float distanceSquared = dx * dx + dy * dy;
                        if (distanceSquared < bestDistanceSquared) {
                            bestDistanceSquared = distanceSquared;
                            bestTrapDoor = trapDoorIndex;
                            bestRing = ringIndex;
                        }
                    }
                }

                FlexibleEndpointBinding& binding = *carried->binding;
                binding.carriedByPlayer = -1;
                if (bestTrapDoor >= 0) {
                    binding.anchorType = FlexibleAnchorType::TrapDoorRing;
                    binding.objectIndex = bestTrapDoor;
                    binding.pointIndex = bestRing;
                    std::array<Vector2, 2> rings = GetTrapDoorRingPositions(level.trapDoors[bestTrapDoor]);
                    setPinnedPosition(*carried, rings[bestRing]);
                }
                else {
                    clearAnchor(binding);
                    *carried->pinned = false;
                    *carried->previousPoint = *carried->point;
                }
            }
            else {
                setPinnedPosition(*carried, getCarryPoint(*activePlayer));
            }
            continue;
        }

        if (!inputs[playerIndex].interactPressed) {
            continue;
        }

        EndpointHandle* nearest = nullptr;
        float nearestDistanceSquared = 48.0f * 48.0f;
        Vector2 playerCenter{
            activePlayer->rect.x + activePlayer->rect.width * 0.5f,
            activePlayer->rect.y + activePlayer->rect.height * 0.5f
        };
        for (EndpointHandle& endpoint : endpoints) {
            if (endpoint.binding->carriedByPlayer >= 0) {
                continue;
            }
            Vector2 position = *endpoint.point;
            Rectangle endpointBounds{position.x - 2.0f, position.y - 2.0f, 4.0f, 4.0f};
            if (!IsNearRect(activePlayer->rect, endpointBounds, 16.0f)) {
                continue;
            }
            float dx = position.x - playerCenter.x;
            float dy = position.y - playerCenter.y;
            float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < nearestDistanceSquared) {
                nearestDistanceSquared = distanceSquared;
                nearest = &endpoint;
            }
        }

        if (nearest != nullptr) {
            clearAnchor(*nearest->binding);
            nearest->binding->carriedByPlayer = playerIndex;
            setPinnedPosition(*nearest, getCarryPoint(*activePlayer));
            consumed[playerIndex] = true;
        }
    }

    return consumed;
}

void Game::UpdateMachines(float dt, const PlayerMachineInput& player1Input, const PlayerMachineInput& player2Input, const PlayerMachineInput& player3Input) {
    std::array<bool, 3> flexibleInputConsumed{};
    if (!won && !lost) {
        UpdateWind(dt);
        UpdateFluids(dt);
        UpdatePhysicsObjects(dt);
        UpdateButtons();
        UpdateArrowTraps(dt);
        UpdateBreakableTiles(dt);
        const Player* activePlayer1 = playerAlive ? &player : nullptr;
        const Player* activePlayer2 = multiplayerEnabled && player2Alive ? &player2 : nullptr;
        const Player* activePlayer3 = threePlayerEnabled && player3Alive ? &player3 : nullptr;
        std::array<Player*, 3> flexiblePlayers{
            playerAlive ? &player : nullptr,
            multiplayerEnabled && player2Alive ? &player2 : nullptr,
            threePlayerEnabled && player3Alive ? &player3 : nullptr
        };
        std::array<PlayerMachineInput, 3> flexibleInputs{player1Input, player2Input, player3Input};
        flexibleInputConsumed = UpdateFlexibleEndpointInteractions(flexiblePlayers, flexibleInputs);

        for (int chainIndex = 0; chainIndex < static_cast<int>(level.chains.size()); chainIndex++) {
            Chain& chain = level.chains[chainIndex];
            std::vector<Rectangle> colliders = BuildFlexibleBodyColliders(
                level,
                activePlayer1,
                activePlayer2,
                activePlayer3,
                chainIndex,
                -1
            );
            std::vector<Vector2> chainWind(chain.points.size());
            for (int i = 0; i < static_cast<int>(chain.points.size()); i++) {
                Vector2 wind = GetWindAtPoint(level, chain.points[i]);
                Vector2 pointVelocity{
                    (chain.points[i].x - chain.previousPoints[i].x) / fmaxf(0.001f, dt),
                    (chain.points[i].y - chain.previousPoints[i].y) / fmaxf(0.001f, dt)
                };
                Vector2 fluidAcceleration = GetFlexibleFluidAcceleration(level, chain.points[i], pointVelocity, 0.24f, 4.2f);
                chainWind[i] = {
                    wind.x * 0.30f + fluidAcceleration.x,
                    wind.y * 0.30f + fluidAcceleration.y
                };
            }
            UpdateChainPhysics(chain, colliders, chainWind, dt);
            for (int i = 0; i < static_cast<int>(chain.points.size()); i += 2) {
                Vector2 pointVelocity{
                    (chain.points[i].x - chain.previousPoints[i].x) / fmaxf(0.001f, dt),
                    (chain.points[i].y - chain.previousPoints[i].y) / fmaxf(0.001f, dt)
                };
                DisturbFluids(level.fluids, chain.points[i], chain.collisionRadius * 2.0f, pointVelocity, 0.65f, dt);
            }
        }

        for (int ropeIndex = 0; ropeIndex < static_cast<int>(level.physicsRopes.size()); ropeIndex++) {
            PhysicsRope& rope = level.physicsRopes[ropeIndex];
            std::vector<Rectangle> colliders = BuildFlexibleBodyColliders(
                level,
                activePlayer1,
                activePlayer2,
                activePlayer3,
                -1,
                ropeIndex
            );
            std::vector<Vector2> ropeWind(rope.points.size());
            for (int i = 0; i < static_cast<int>(rope.points.size()); i++) {
                Vector2 wind = GetWindAtPoint(level, rope.points[i]);
                Vector2 pointVelocity{
                    (rope.points[i].x - rope.previousPoints[i].x) / fmaxf(0.001f, dt),
                    (rope.points[i].y - rope.previousPoints[i].y) / fmaxf(0.001f, dt)
                };
                Vector2 fluidAcceleration = GetFlexibleFluidAcceleration(level, rope.points[i], pointVelocity, 0.96f, 5.8f);
                ropeWind[i] = {
                    wind.x * 0.62f + fluidAcceleration.x,
                    wind.y * 0.62f + fluidAcceleration.y
                };
            }
            UpdatePhysicsRope(rope, colliders, ropeWind, dt);
            for (int i = 1; i < static_cast<int>(rope.points.size()) - 1; i += 3) {
                Vector2 pointVelocity{
                    (rope.points[i].x - rope.previousPoints[i].x) / fmaxf(0.001f, dt),
                    (rope.points[i].y - rope.previousPoints[i].y) / fmaxf(0.001f, dt)
                };
                DisturbFluids(level.fluids, rope.points[i], rope.thickness * 2.5f, pointVelocity, 0.75f, dt);
            }
        }
    }

    if (level.script == LevelScript::FloodedFoundry) {
        if (!won && !lost) {
            bool player1NearValve = playerAlive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
            bool player2NearValve = multiplayerEnabled && player2Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player2.rect);
            bool player3NearValve = threePlayerEnabled && player3Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player3.rect);
            bool valveHeld = (player1NearValve && player1Input.interactHeld && !flexibleInputConsumed[0]) ||
                (player2NearValve && player2Input.interactHeld && !flexibleInputConsumed[1]) ||
                (player3NearValve && player3Input.interactHeld && !flexibleInputConsumed[2]);
            if (valveHeld && !level.valve.opened) {
                level.valve.turnDegrees = fminf(360.0f, level.valve.turnDegrees + level.valve.turnSpeed * dt);
                if (level.valve.turnDegrees >= 360.0f) {
                    level.valve.opened = true;
                }
            }

            float valveOpenAmount = GetValveOpenAmount(level.valve);
            FluidField* valveFluid = GetValveFluid(level);
            if (valveOpenAmount > 0.0f && HasValveFluidFill(level) && valveFluid != nullptr && valveFluid->cellSize > 0.0f) {
                float targetMass = GetValveFluidTargetMass(level, *valveFluid);
                float remainingMass = fmaxf(0.0f, targetMass - GetFluidMass(*valveFluid));
                float massPerVerticalPixel = static_cast<float>(valveFluid->gridColumns) / valveFluid->cellSize;
                float requestedMass = level.valveFluidFill.riseRate * massPerVerticalPixel * valveOpenAmount * dt;
                AddCellularFluidMass(*valveFluid, fminf(requestedMass, remainingMass));
            }
            else if (valveOpenAmount > 0.0f && HasWaterPit(level)) {
                level.waterPit.filling = true;
                level.waterPit.surfaceY = MoveTowardsFloat(
                    level.waterPit.surfaceY,
                    level.waterPit.targetSurfaceY,
                    level.waterPit.fillRate * valveOpenAmount * dt
                );
            }
        }

        machinePower = GetFloodWaterProgress(level);
        if (HasArea(level.exitTrigger)) {
            float targetGateBottom = level.exitTrigger.y + level.exitTrigger.height;
            if (level.valve.opened) {
                targetGateBottom = level.exitTrigger.y;
            }
            gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
        }
        return;
    }

    if (level.script == LevelScript::RotaryLatchLab) {
        machinePower = 0.0f;
        if (!won && !lost) {
            for (RotaryLatch& latch : level.rotaryLatches) {
                AdvanceRotaryLatch(latch, 1.0f, dt);
                if (playerAlive) {
                    TryLockRotaryLatch(latch, player, player1Input.interactPressed);
                }
                if (multiplayerEnabled && player2Alive) {
                    TryLockRotaryLatch(latch, player2, player2Input.interactPressed);
                }
                if (threePlayerEnabled && player3Alive) {
                    TryLockRotaryLatch(latch, player3, player3Input.interactPressed);
                }
            }
        }
        if (HasArea(level.exitTrigger)) {
            float targetGateBottom = level.exitTrigger.y + level.exitTrigger.height;
            if (!level.rotaryLatches.empty() && AreAllRotaryLatchesLatched(level.rotaryLatches)) {
                targetGateBottom = level.exitTrigger.y;
            }
            gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
        }
        return;
    }

    if (level.script == LevelScript::CounterweightRow) {
        bool heavyPlatePressed = false;
        for (const Button& button : level.buttons) {
            for (const Boulder& boulder : level.boulders) {
                if (CheckCollisionCircleRec(boulder.center, boulder.radius, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const PhysicsWheel& wheel : level.physicsWheels) {
                if (CheckCollisionCircleRec(wheel.center, wheel.radius, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const Gear& gear : level.gears) {
                if (CheckCollisionCircleRec(gear.center, gear.radius * 1.12f, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const Flywheel& flywheel : level.flywheels) {
                if (CheckCollisionCircleRec(flywheel.center, flywheel.radius, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            if (heavyPlatePressed) break;
        }

        machinePower = heavyPlatePressed ? 1.0f : 0.0f;
        if (HasArea(level.exitTrigger)) {
            float targetGateBottom = heavyPlatePressed
                ? level.exitTrigger.y
                : level.exitTrigger.y + level.exitTrigger.height;
            gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
        }
        return;
    }

    float winchDelta = 0.0f;
    if (!won && !lost) {
        bool player1GrabbingWinch = playerAlive && IsNearRect(player.rect, machineWinch.rect, 18.0f) && player1Input.interactHeld && !flexibleInputConsumed[0];
        bool player2GrabbingWinch = multiplayerEnabled && player2Alive && IsNearRect(player2.rect, machineWinch.rect, 18.0f) && player2Input.interactHeld && !flexibleInputConsumed[1];
        bool player3GrabbingWinch = threePlayerEnabled && player3Alive && IsNearRect(player3.rect, machineWinch.rect, 18.0f) && player3Input.interactHeld && !flexibleInputConsumed[2];
        if (player3GrabbingWinch && !player1GrabbingWinch && !player2GrabbingWinch) {
            winchDelta = UpdateWinch(machineWinch, player3, player3Input.moveInput, player3Input.interactHeld, dt);
        }
        else if (player2GrabbingWinch && !player1GrabbingWinch) {
            winchDelta = UpdateWinch(machineWinch, player2, player2Input.moveInput, player2Input.interactHeld, dt);
        }
        else if (player1GrabbingWinch) {
            winchDelta = UpdateWinch(machineWinch, player, player1Input.moveInput, player1Input.interactHeld, dt);
        }
        else if (playerAlive) {
            winchDelta = UpdateWinch(machineWinch, player, 0.0f, false, dt);
        }
        else if (multiplayerEnabled && player2Alive) {
            winchDelta = UpdateWinch(machineWinch, player2, 0.0f, false, dt);
        }
        else if (threePlayerEnabled && player3Alive) {
            winchDelta = UpdateWinch(machineWinch, player3, 0.0f, false, dt);
        }
    }

    machinePower = GetMachinePower(machineWinch);
    float spinAmount = fabsf(winchDelta) * 3.0f + machinePower * 140.0f * dt;
    pulleyRotation += spinAmount;
    machinePhase += (0.35f + machinePower * 3.3f) * dt;

    UpdateHangingWeights(level.weights, machinePower, machinePhase);

    if (HasArea(level.exitTrigger)) {
        float closedGateBottom = level.exitTrigger.y + level.exitTrigger.height;
        float targetGateBottom = closedGateBottom - machinePower * 170.0f;
        gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 240.0f * dt);
    }
}

void Game::UpdateEnemies(float dt) {
    std::vector<Rectangle> solids = BuildSolids(level);
    AppendFlexibleObjectColliders(solids, level);
    for (const StoneBlock& block : level.stoneBlocks) {
        solids.push_back(block.rect);
    }
    for (const Boulder& boulder : level.boulders) {
        solids.push_back(GetBoulderBounds(boulder));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        solids.push_back(GetWheelBounds(wheel));
    }
    for (const Gear& gear : level.gears) {
        solids.push_back(GetGearBounds(gear));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        solids.push_back(GetFlywheelBounds(flywheel));
    }
    for (const Screw& screw : level.screws) {
        AppendScrewColliders(solids, screw);
    }

    for (Enemy& enemy : level.enemies) {
        float direction = enemy.facingRight ? 1.0f : -1.0f;
        enemy.velocity.x = direction * enemy.speed;
        enemy.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(enemy.velocity, RectCenter(enemy.rect), 1.0f, level, dt, 0.32f);
        ApplyFluidForcesToVelocity(enemy.velocity, enemy.rect, 1.0f, 0.82f, 4.2f, level, dt);

        enemy.rect.x += enemy.velocity.x * dt;
        bool hitWall = ResolveEnemyHorizontal(enemy, solids);

        if (enemy.rect.x <= enemy.patrolMinX) {
            enemy.rect.x = enemy.patrolMinX;
            enemy.facingRight = true;
        }
        else if (enemy.rect.x + enemy.rect.width >= enemy.patrolMaxX) {
            enemy.rect.x = enemy.patrolMaxX - enemy.rect.width;
            enemy.facingRight = false;
        }
        else if (hitWall) {
            enemy.facingRight = !enemy.facingRight;
        }

        enemy.rect.y += enemy.velocity.y * dt;
        ResolveEnemyVertical(enemy, solids);
        ResolveRampStanding(enemy.rect, enemy.velocity, enemy.onGround, level.ramps);
        ResolveTrapDoorStanding(enemy.rect, enemy.velocity, enemy.onGround, level.trapDoors);
        ResolveSeeSawStanding(enemy.rect, enemy.velocity, enemy.onGround, level.seeSaws);
        enemy.walking = enemy.onGround && enemy.speed > 0.0f;
        DisturbFluids(
            level.fluids,
            RectCenter(enemy.rect),
            fmaxf(enemy.rect.width, enemy.rect.height) * 0.75f,
            enemy.velocity,
            1.8f,
            dt
        );
    }
}

void Game::UpdateWind(float dt) {
    for (Fan& fan : level.fans) {
        fan.rotation += fan.power * fan.strength * dt * 1.8f;
    }

    for (Pinwheel& pinwheel : level.pinwheels) {
        float targetSpin = GetPinwheelSpinAtPoint(level, pinwheel.center) * 2.3f;
        pinwheel.angularVelocity = ApproachFloat(pinwheel.angularVelocity, targetSpin, 620.0f * dt);
        pinwheel.rotation += pinwheel.angularVelocity * dt;
    }
}

void Game::UpdateFluids(float dt) {
    if (level.fluids.empty()) {
        return;
    }

    const Player* activePlayer1 = playerAlive ? &player : nullptr;
    const Player* activePlayer2 = multiplayerEnabled && player2Alive ? &player2 : nullptr;
    const Player* activePlayer3 = threePlayerEnabled && player3Alive ? &player3 : nullptr;
    std::vector<Rectangle> obstacles = BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3);

    for (FluidField& fluid : level.fluids) {
        // Cellular materials do not need a per-pixel wind query. Object impulses
        // still stir them, while particle gel and gas retain full wind interaction.
        int flowPointCount = (fluid.type == FluidType::Water || fluid.type == FluidType::Sand) ?
            0 : GetFluidSimulationPointCount(fluid);
        std::vector<Vector2> externalFlow(static_cast<size_t>(flowPointCount));
        for (int index = 0; index < static_cast<int>(externalFlow.size()); index++) {
            externalFlow[index] = GetWindAtPoint(level, GetFluidSimulationPoint(fluid, index));
        }
        UpdateFluidField(fluid, obstacles, externalFlow, dt, SelectedFluidMode(advancedFluidSimulation));
    }
}

void Game::UpdatePhysicsObjects(float dt) {
    std::vector<Rectangle> solids = BuildSolids(level);
    AppendFlexibleObjectColliders(solids, level);
    std::vector<Rectangle> dynamicWorldSolids = solids;
    for (const Screw& screw : level.screws) {
        AppendScrewColliders(dynamicWorldSolids, screw);
    }
    const std::vector<Rectangle>& blockSolids = dynamicWorldSolids;

    for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
        StoneBlock& block = level.stoneBlocks[i];

        if (block.onGround) {
            block.velocity.x *= powf(0.000001f, dt);
            if (fabsf(block.velocity.x) < 12.0f) {
                block.velocity.x = 0.0f;
            }
        }

        block.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(block.velocity, RectCenter(block.rect), block.mass, level, dt, 0.40f);
        ApplyFluidForcesToVelocity(block.velocity, block.rect, block.mass, 0.30f, 5.6f, level, dt);

        block.rect.x += block.velocity.x * dt;
        ResolveStoneBlockHorizontal(block, blockSolids);

        block.rect.y += block.velocity.y * dt;
        ResolveStoneBlockVertical(block, blockSolids);
        ApplyScrewConveyor(block.rect, block.velocity, block.mass, level.screws, dt);
        ResolveRampStanding(block.rect, block.velocity, block.onGround, level.ramps);
        ResolveTrapDoorStanding(block.rect, block.velocity, block.onGround, level.trapDoors);
        ResolveSeeSawStanding(block.rect, block.velocity, block.onGround, level.seeSaws);
        DisturbFluids(
            level.fluids,
            RectCenter(block.rect),
            fmaxf(block.rect.width, block.rect.height) * 0.75f,
            block.velocity,
            2.2f,
            dt
        );
    }

    const std::vector<Rectangle>& boulderSolids = dynamicWorldSolids;

    for (int i = 0; i < static_cast<int>(level.boulders.size()); i++) {
        Boulder& boulder = level.boulders[i];

        if (boulder.onGround) {
            boulder.velocity.x *= powf(0.34f, dt);
            boulder.angularVelocity *= powf(0.30f, dt);
            if (fabsf(boulder.velocity.x) < 4.0f) {
                boulder.velocity.x = 0.0f;
            }
            if (fabsf(boulder.angularVelocity) < 6.0f) {
                boulder.angularVelocity = 0.0f;
            }
        }

        boulder.onGround = false;
        boulder.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(boulder.velocity, boulder.center, boulder.mass, level, dt, 0.45f);
        ApplyFluidForcesToVelocity(
            boulder.velocity,
            GetBoulderBounds(boulder),
            boulder.mass,
            0.42f,
            4.8f,
            level,
            dt
        );

        boulder.center.x += boulder.velocity.x * dt;
        ResolveBoulderCollisions(boulder, boulderSolids);

        boulder.center.y += boulder.velocity.y * dt;
        ResolveBoulderCollisions(boulder, boulderSolids);
        ApplyScrewConveyor(boulder, boulder.radius, level.screws, dt);
        ResolveBoulderRampStanding(boulder, level.ramps, dt);
        ResolveBoulderTrapDoorStanding(boulder, level.trapDoors, dt);
        ResolveBoulderSeeSawStanding(boulder, level.seeSaws, dt);

        if (boulder.onGround && fabsf(boulder.velocity.x) > 0.0f) {
            float rollingSpeed = boulder.velocity.x / fmaxf(1.0f, boulder.radius) * RAD2DEG;
            boulder.angularVelocity = ApproachFloat(boulder.angularVelocity, rollingSpeed, 520.0f * dt);
        }
        boulder.rotation += boulder.angularVelocity * dt;
        DisturbFluids(level.fluids, boulder.center, boulder.radius * 1.35f, boulder.velocity, 2.0f, dt);
    }

    const std::vector<Rectangle>& wheelSolids = dynamicWorldSolids;

    for (int i = 0; i < static_cast<int>(level.physicsWheels.size()); i++) {
        PhysicsWheel& wheel = level.physicsWheels[i];

        if (wheel.onGround) {
            wheel.velocity.x *= powf(0.28f, dt);
            wheel.angularVelocity *= powf(0.18f, dt);
            if (fabsf(wheel.velocity.x) < 4.0f) {
                wheel.velocity.x = 0.0f;
            }
            if (fabsf(wheel.angularVelocity) < 8.0f) {
                wheel.angularVelocity = 0.0f;
            }
        }

        wheel.onGround = false;
        wheel.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(wheel.velocity, wheel.center, wheel.mass, level, dt, 0.70f);
        ApplyFluidForcesToVelocity(
            wheel.velocity,
            GetWheelBounds(wheel),
            wheel.mass,
            1.30f,
            5.8f,
            level,
            dt
        );

        wheel.center.x += wheel.velocity.x * dt;
        ResolveWheelCollisions(wheel, wheelSolids);

        wheel.center.y += wheel.velocity.y * dt;
        ResolveWheelCollisions(wheel, wheelSolids);
        ApplyScrewConveyor(wheel, wheel.radius, level.screws, dt);
        ResolveWheelRampStanding(wheel, level.ramps, dt);
        ResolveWheelTrapDoorStanding(wheel, level.trapDoors, dt);
        ResolveWheelSeeSawStanding(wheel, level.seeSaws, dt);

        if (wheel.onGround && fabsf(wheel.velocity.x) > 0.0f) {
            wheel.angularVelocity = wheel.velocity.x / fmaxf(1.0f, wheel.radius) * RAD2DEG;
        }
        wheel.rotation += wheel.angularVelocity * dt;
        DisturbFluids(level.fluids, wheel.center, wheel.radius * 1.35f, wheel.velocity, 2.3f, dt);
    }

    const std::vector<Rectangle>& gearSolids = dynamicWorldSolids;

    for (int i = 0; i < static_cast<int>(level.gears.size()); i++) {
        Gear& gear = level.gears[i];
        if (gear.onGround) {
            gear.velocity.x *= powf(0.18f, dt);
            gear.angularVelocity *= powf(0.14f, dt);
        }

        gear.onGround = false;
        gear.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(gear.velocity, gear.center, gear.mass, level, dt, 0.55f);
        ApplyFluidForcesToVelocity(
            gear.velocity,
            GetGearBounds(gear),
            gear.mass,
            0.72f,
            5.2f,
            level,
            dt
        );
        gear.center.x += gear.velocity.x * dt;
        ResolveGearCollisions(gear, gearSolids);
        gear.center.y += gear.velocity.y * dt;
        ResolveGearCollisions(gear, gearSolids);
        ApplyScrewGearCoupling(gear, level.screws, dt);
        ResolveRoundBodyRampStanding(gear, level.ramps, gear.radius * 1.12f, dt, 0.38f);
        ResolveRoundBodyTrapDoorStanding(gear, level.trapDoors, gear.radius * 1.12f, dt, 0.36f);
        ResolveRoundBodySeeSawStanding(gear, level.seeSaws, gear.radius * 1.12f, dt, 0.38f);

        if (gear.onGround && fabsf(gear.velocity.x) > 0.0f) {
            float rollingSpeed = gear.velocity.x / fmaxf(1.0f, gear.radius * 1.12f) * RAD2DEG;
            gear.angularVelocity = ApproachFloat(gear.angularVelocity, rollingSpeed, 620.0f * dt);
        }
        gear.rotation += gear.angularVelocity * dt;
        DisturbFluids(level.fluids, gear.center, gear.radius * 1.45f, gear.velocity, 2.1f, dt);
    }

    const std::vector<Rectangle>& flywheelSolids = dynamicWorldSolids;

    for (int i = 0; i < static_cast<int>(level.flywheels.size()); i++) {
        Flywheel& flywheel = level.flywheels[i];
        if (flywheel.onGround) {
            float surfaceSpeed = flywheel.angularVelocity * DEG2RAD * flywheel.radius;
            float slipSpeed = surfaceSpeed - flywheel.velocity.x;
            float coupling = Clamp01(1.8f * dt / sqrtf(fmaxf(1.0f, flywheel.mass)));
            flywheel.velocity.x += slipSpeed * coupling;
            flywheel.angularVelocity -= slipSpeed / fmaxf(1.0f, flywheel.radius) * RAD2DEG * coupling * 0.45f;
            flywheel.velocity.x *= powf(0.55f, dt);
            flywheel.angularVelocity *= powf(0.78f, dt);
        }

        flywheel.onGround = false;
        flywheel.velocity.y += Constants::Gravity * dt;
        ApplyWindToVelocity(flywheel.velocity, flywheel.center, flywheel.mass, level, dt, 0.35f);
        ApplyFluidForcesToVelocity(
            flywheel.velocity,
            GetFlywheelBounds(flywheel),
            flywheel.mass,
            0.48f,
            4.2f,
            level,
            dt
        );
        flywheel.center.x += flywheel.velocity.x * dt;
        ResolveFlywheelCollisions(flywheel, flywheelSolids);
        flywheel.center.y += flywheel.velocity.y * dt;
        ResolveFlywheelCollisions(flywheel, flywheelSolids);
        ApplyScrewConveyor(flywheel, flywheel.radius, level.screws, dt);
        ResolveRoundBodyRampStanding(flywheel, level.ramps, flywheel.radius, dt, 0.22f);
        ResolveRoundBodyTrapDoorStanding(flywheel, level.trapDoors, flywheel.radius, dt, 0.20f);
        ResolveRoundBodySeeSawStanding(flywheel, level.seeSaws, flywheel.radius, dt, 0.22f);

        if (flywheel.onGround && fabsf(flywheel.velocity.x) > 0.0f) {
            float rollingSpeed = flywheel.velocity.x / fmaxf(1.0f, flywheel.radius) * RAD2DEG;
            flywheel.angularVelocity = ApproachFloat(flywheel.angularVelocity, rollingSpeed, 85.0f * dt);
        }
        flywheel.rotation += flywheel.angularVelocity * dt;
        DisturbFluids(level.fluids, flywheel.center, flywheel.radius * 1.35f, flywheel.velocity, 1.8f, dt);
    }

    ResolveDynamicBodyCollisions(
        level.stoneBlocks,
        level.boulders,
        level.physicsWheels,
        level.gears,
        level.flywheels
    );

    for (StoneBlock& block : level.stoneBlocks) {
        ResolveStoneBlockPenetration(block, dynamicWorldSolids);
        ResolveRampStanding(block.rect, block.velocity, block.onGround, level.ramps);
        ResolveTrapDoorStanding(block.rect, block.velocity, block.onGround, level.trapDoors);
        ResolveSeeSawStanding(block.rect, block.velocity, block.onGround, level.seeSaws);
    }
    for (Boulder& boulder : level.boulders) {
        ResolveBoulderCollisions(boulder, dynamicWorldSolids);
        ResolveBoulderRampStanding(boulder, level.ramps, 0.0f);
        ResolveBoulderTrapDoorStanding(boulder, level.trapDoors, 0.0f);
        ResolveBoulderSeeSawStanding(boulder, level.seeSaws, 0.0f);
    }
    for (PhysicsWheel& wheel : level.physicsWheels) {
        ResolveWheelCollisions(wheel, dynamicWorldSolids);
        ResolveWheelRampStanding(wheel, level.ramps, 0.0f);
        ResolveWheelTrapDoorStanding(wheel, level.trapDoors, 0.0f);
        ResolveWheelSeeSawStanding(wheel, level.seeSaws, 0.0f);
    }
    for (Gear& gear : level.gears) {
        ResolveGearCollisions(gear, dynamicWorldSolids);
        ResolveRoundBodyRampStanding(gear, level.ramps, gear.radius * 1.12f, 0.0f, 0.0f);
        ResolveRoundBodyTrapDoorStanding(gear, level.trapDoors, gear.radius * 1.12f, 0.0f, 0.0f);
        ResolveRoundBodySeeSawStanding(gear, level.seeSaws, gear.radius * 1.12f, 0.0f, 0.0f);
    }
    for (Flywheel& flywheel : level.flywheels) {
        ResolveFlywheelCollisions(flywheel, dynamicWorldSolids);
        ResolveRoundBodyRampStanding(flywheel, level.ramps, flywheel.radius, 0.0f, 0.0f);
        ResolveRoundBodyTrapDoorStanding(flywheel, level.trapDoors, flywheel.radius, 0.0f, 0.0f);
        ResolveRoundBodySeeSawStanding(flywheel, level.seeSaws, flywheel.radius, 0.0f, 0.0f);
    }

    for (Screw& screw : level.screws) {
        screw.rotation += screw.spinSpeed * dt;
    }

    for (SeeSaw& seeSaw : level.seeSaws) {
        float torque = playerAlive ? GetSeeSawTorqueContribution(seeSaw, player.rect, 1.0f) : 0.0f;
        if (multiplayerEnabled && player2Alive) {
            torque += GetSeeSawTorqueContribution(seeSaw, player2.rect, 1.0f);
        }
        if (threePlayerEnabled && player3Alive) {
            torque += GetSeeSawTorqueContribution(seeSaw, player3.rect, 1.0f);
        }
        for (const StoneBlock& block : level.stoneBlocks) {
            torque += GetSeeSawTorqueContribution(seeSaw, block.rect, block.mass);
        }
        for (const Boulder& boulder : level.boulders) {
            torque += GetSeeSawTorqueContribution(seeSaw, GetBoulderBounds(boulder), boulder.mass);
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            torque += GetSeeSawTorqueContribution(seeSaw, GetWheelBounds(wheel), wheel.mass);
        }
        for (const Gear& gear : level.gears) {
            torque += GetSeeSawTorqueContribution(seeSaw, GetGearBounds(gear), gear.mass);
        }
        for (const Flywheel& flywheel : level.flywheels) {
            torque += GetSeeSawTorqueContribution(seeSaw, GetFlywheelBounds(flywheel), flywheel.mass);
        }

        float targetAngle = std::clamp(torque * 13.0f, seeSaw.minAngle, seeSaw.maxAngle);
        float naturalFrequency = fmaxf(1.0f, seeSaw.response * 1.4f);
        float angularAcceleration =
            (targetAngle - seeSaw.angle) * naturalFrequency * naturalFrequency -
            seeSaw.angularVelocity * naturalFrequency * 1.45f;
        seeSaw.angularVelocity += angularAcceleration * dt;
        seeSaw.angle += seeSaw.angularVelocity * dt;

        if (seeSaw.angle <= seeSaw.minAngle) {
            seeSaw.angle = seeSaw.minAngle;
            if (seeSaw.angularVelocity < 0.0f) {
                seeSaw.angularVelocity = 0.0f;
            }
        }
        else if (seeSaw.angle >= seeSaw.maxAngle) {
            seeSaw.angle = seeSaw.maxAngle;
            if (seeSaw.angularVelocity > 0.0f) {
                seeSaw.angularVelocity = 0.0f;
            }
        }
    }
}

void Game::UpdateButtons() {
    for (Button& button : level.buttons) {
        button.pressed = false;

        const Player* players[] = {
            playerAlive ? &player : nullptr,
            multiplayerEnabled && player2Alive ? &player2 : nullptr,
            threePlayerEnabled && player3Alive ? &player3 : nullptr
        };
        if (level.script != LevelScript::CounterweightRow) {
            for (const Player* activePlayer : players) {
                if (activePlayer != nullptr && CheckCollisionRecs(button.rect, activePlayer->rect)) {
                    button.pressed = true;
                }
            }
        }

        for (const StoneBlock& block : level.stoneBlocks) {
            if (CheckCollisionRecs(button.rect, block.rect)) {
                button.pressed = true;
            }
        }
        for (const Boulder& boulder : level.boulders) {
            if (CheckCollisionCircleRec(boulder.center, boulder.radius, button.rect)) {
                button.pressed = true;
            }
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (CheckCollisionCircleRec(wheel.center, wheel.radius, button.rect)) {
                button.pressed = true;
            }
        }
        for (const Gear& gear : level.gears) {
            if (CheckCollisionCircleRec(gear.center, gear.radius * 1.12f, button.rect)) {
                button.pressed = true;
            }
        }
        for (const Flywheel& flywheel : level.flywheels) {
            if (CheckCollisionCircleRec(flywheel.center, flywheel.radius, button.rect)) {
                button.pressed = true;
            }
        }
        for (const Chain& chain : level.chains) {
            float radius = fmaxf(2.0f, chain.collisionRadius * chain.scale);
            for (const Vector2& point : chain.points) {
                if (CheckCollisionCircleRec(point, radius, button.rect)) {
                    button.pressed = true;
                    break;
                }
            }
        }
        for (const PhysicsRope& rope : level.physicsRopes) {
            float radius = fmaxf(1.0f, rope.thickness * 0.5f);
            for (const Vector2& point : rope.points) {
                if (CheckCollisionCircleRec(point, radius, button.rect)) {
                    button.pressed = true;
                    break;
                }
            }
        }
        for (const Enemy& enemy : level.enemies) {
            if (CheckCollisionRecs(button.rect, enemy.rect)) {
                button.pressed = true;
            }
        }
    }
}

void Game::UpdateArrowTraps(float dt) {
    if (arrowTrapsDisabled) {
        return;
    }

    std::vector<Rectangle> solids = BuildSolids(level);
    AppendFlexibleObjectColliders(solids, level);
    for (const StoneBlock& block : level.stoneBlocks) {
        solids.push_back(block.rect);
    }
    for (const Boulder& boulder : level.boulders) {
        solids.push_back(GetBoulderBounds(boulder));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        solids.push_back(GetWheelBounds(wheel));
    }
    for (const Gear& gear : level.gears) {
        solids.push_back(GetGearBounds(gear));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        solids.push_back(GetFlywheelBounds(flywheel));
    }
    for (const Screw& screw : level.screws) {
        AppendScrewColliders(solids, screw);
    }

    for (ArrowTrap& trap : level.arrowTraps) {
        trap.timer -= dt;
        if (trap.timer <= 0.0f) {
            ArrowProjectile arrow{};
            Vector2 spawn{
                trap.position.x + trap.direction.x * 24.0f,
                trap.position.y + trap.direction.y * 24.0f
            };
            arrow.rect = MakeArrowRect(spawn, trap.direction);
            arrow.velocity = {trap.direction.x * trap.speed, trap.direction.y * trap.speed};
            arrow.active = true;
            trap.arrows.push_back(arrow);
            trap.timer = fmaxf(0.08f, trap.interval);
        }

        for (ArrowProjectile& arrow : trap.arrows) {
            if (!arrow.active) {
                continue;
            }

            ApplyWindToVelocity(arrow.velocity, RectCenter(arrow.rect), 0.25f, level, dt, 0.12f);
            ApplyFluidForcesToVelocity(arrow.velocity, arrow.rect, 0.25f, 0.04f, 1.8f, level, dt);
            arrow.rect.x += arrow.velocity.x * dt;
            arrow.rect.y += arrow.velocity.y * dt;
            DisturbFluids(level.fluids, RectCenter(arrow.rect), 10.0f, arrow.velocity, 0.35f, dt);

            if (arrow.rect.x + arrow.rect.width < 0.0f ||
                arrow.rect.x > Constants::ScreenWidth ||
                arrow.rect.y + arrow.rect.height < 0.0f ||
                arrow.rect.y > Constants::ScreenHeight) {
                arrow.active = false;
                continue;
            }

            for (const Rectangle& solid : solids) {
                if (CheckCollisionRecs(arrow.rect, solid)) {
                    arrow.active = false;
                    break;
                }
            }
        }

        trap.arrows.erase(
            std::remove_if(trap.arrows.begin(), trap.arrows.end(), [](const ArrowProjectile& arrow) {
                return !arrow.active;
            }),
            trap.arrows.end()
        );
    }
}

void Game::UpdateBreakableTiles(float dt) {
    const Player* players[] = {
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr
    };

    for (BreakableTile& tile : level.breakableTiles) {
        if (!tile.broken && !tile.cracking) {
            for (const Player* activePlayer : players) {
                if (activePlayer != nullptr && IsRectStandingOnTile(activePlayer->rect, tile.rect)) {
                    tile.cracking = true;
                    break;
                }
            }
        }

        if (!tile.broken && tile.cracking) {
            tile.crackTimer += dt;
            if (tile.crackTimer >= tile.breakDelay) {
                tile.broken = true;
                SpawnBreakableDebris(tile);
            }
        }

        for (BreakableDebris& debris : tile.debris) {
            ApplyWindToVelocity(debris.velocity, RectCenter(debris.rect), 0.2f, level, dt, 0.24f);
            debris.velocity.y += Constants::Gravity * dt;
            ApplyFluidForcesToVelocity(debris.velocity, debris.rect, 0.2f, 0.18f, 3.6f, level, dt);
            debris.rect.x += debris.velocity.x * dt;
            debris.rect.y += debris.velocity.y * dt;
            DisturbFluids(level.fluids, RectCenter(debris.rect), 9.0f, debris.velocity, 0.45f, dt);
            debris.life -= dt;
        }

        tile.debris.erase(
            std::remove_if(tile.debris.begin(), tile.debris.end(), [](const BreakableDebris& debris) {
                return debris.life <= 0.0f;
            }),
            tile.debris.end()
        );
    }
}

void Game::KillPlayer(const Player& defeatedPlayer) {
    if (playerInvincible || lost) {
        return;
    }

    deathRect = defeatedPlayer.rect;
    won = false;
    if (screenShakeSetting != ScreenShakeSetting::Off) {
        screenShakeTimer = screenShakeSetting == ScreenShakeSetting::Reduced ? 0.12f : 0.24f;
    }
    if (controllerEnabled && controllerVibrationEnabled && IsGamepadAvailable(0)) {
        const float vibrationStrength = screenShakeSetting == ScreenShakeSetting::Reduced ? 0.35f : 0.75f;
        SetGamepadVibration(0, vibrationStrength, vibrationStrength, 0.18f);
    }

    if (!multiplayerEnabled) {
        playerAlive = false;
        playerDeathRect = defeatedPlayer.rect;
        player.velocity = {0.0f, 0.0f};
        player.walking = false;
        player.climbing = false;
        lost = true;
        return;
    }

    if (&defeatedPlayer == &player) {
        playerAlive = false;
        playerDeathRect = defeatedPlayer.rect;
        player.velocity = {0.0f, 0.0f};
        player.walking = false;
        player.climbing = false;
    }
    else if (&defeatedPlayer == &player2) {
        player2Alive = false;
        player2DeathRect = defeatedPlayer.rect;
        player2.velocity = {0.0f, 0.0f};
        player2.walking = false;
        player2.climbing = false;
    }
    else if (&defeatedPlayer == &player3) {
        player3Alive = false;
        player3DeathRect = defeatedPlayer.rect;
        player3.velocity = {0.0f, 0.0f};
        player3.walking = false;
        player3.climbing = false;
    }

    lost = !playerAlive && !player2Alive && (!threePlayerEnabled || !player3Alive);
}

void Game::CheckFailureConditions() {
    if (won || lost) return;

    const Player* players[] = {
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr
    };
    for (const Player* activePlayer : players) {
        if (activePlayer == nullptr) continue;

        if (activePlayer->rect.y > Constants::ScreenHeight ||
            (HasArea(level.spikeHazard) && CheckCollisionRecs(activePlayer->rect, level.spikeHazard))) {
            KillPlayer(*activePlayer);
            return;
        }

        for (const HangingWeight& weight : level.weights) {
            if (CheckCollisionRecs(activePlayer->rect, weight.rect)) {
                KillPlayer(*activePlayer);
                return;
            }
        }

        for (const Enemy& enemy : level.enemies) {
            if (CheckCollisionRecs(activePlayer->rect, enemy.rect)) {
                KillPlayer(*activePlayer);
                return;
            }
        }

        for (const ArrowTrap& trap : level.arrowTraps) {
            for (const ArrowProjectile& arrow : trap.arrows) {
                if (arrow.active && CheckCollisionRecs(activePlayer->rect, arrow.rect)) {
                    KillPlayer(*activePlayer);
                    return;
                }
            }
        }
    }
}

void Game::CheckWinCondition(float gateBottom) {
    if (won || lost) return;

    bool latchesComplete = level.rotaryLatches.empty() || AreAllRotaryLatchesLatched(level.rotaryLatches);
    if (!HasArea(level.exitTrigger)) {
        return;
    }

    bool hasFactoryMachine = level.pulleys.size() >= 5;
    float doorwayCenterX = level.exitTrigger.x + level.exitTrigger.width * 0.5f;
    constexpr float DoorwayCenterTolerance = 10.0f;
    const Player* players[] = {
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr
    };
    for (const Player* activePlayer : players) {
        if (activePlayer == nullptr) continue;

        Vector2 playerCenter{
            activePlayer->rect.x + activePlayer->rect.width * 0.5f,
            activePlayer->rect.y + activePlayer->rect.height * 0.5f
        };
        bool playerCenteredInDoorway = fabsf(playerCenter.x - doorwayCenterX) <= DoorwayCenterTolerance;
        bool doorRaisedPastPlayer = gateBottom <= activePlayer->rect.y + 4.0f;
        bool doorUnlocked = latchesComplete && doorRaisedPastPlayer && (!hasFactoryMachine || machinePower > 0.05f);

        if (doorUnlocked && playerCenteredInDoorway && CheckCollisionPointRec(playerCenter, level.exitTrigger)) {
            BeginLevelClear();
            return;
        }
    }
}

void Game::DrawScene() {
    ClearBackground(RAYWHITE);

    if (mode == GameMode::Title) {
        DrawTitleScreen();
        if (settingsPopupOpen) {
            DrawSettingsPopup();
        }
        if (quitConfirmationOpen) {
            DrawQuitConfirmation();
        }
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        return;
    }

    if (mode == GameMode::Overworld) {
        DrawOverworld();
        if (quitConfirmationOpen) {
            DrawQuitConfirmation();
        }
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        return;
    }

    Camera2D shakeCamera{};
    shakeCamera.zoom = 1.0f;
    if (screenShakeTimer > 0.0f && screenShakeSetting != ScreenShakeSetting::Off) {
        const float strength = screenShakeSetting == ScreenShakeSetting::Reduced ? 2.0f : 5.0f;
        shakeCamera.offset = {
            static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * strength,
            static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * strength
        };
    }
    BeginMode2D(shakeCamera);
    DrawGameplay();
    EndMode2D();

    if (mode == GameMode::Paused) {
        DrawPauseScreen();
    }

    if (controlsPopupOpen) {
        DrawControlsPopup();
    }

    if (settingsPopupOpen) {
        DrawSettingsPopup();
    }

    if (quitConfirmationOpen) {
        DrawQuitConfirmation();
    }

    if (debugCollision) {
        DrawDebugCollision();
    }

    console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
}

void Game::Draw() {
    if (sceneTarget.id <= 0) {
        BeginDrawing();
        DrawScene();
        EndDrawing();
        return;
    }

    BeginTextureMode(sceneTarget);
    DrawScene();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    const Rectangle viewport = GetVirtualScreenViewport();
    DrawTexturePro(
        sceneTarget.texture,
        {0.0f, 0.0f, static_cast<float>(sceneTarget.texture.width), -static_cast<float>(sceneTarget.texture.height)},
        viewport,
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
    EndDrawing();
}

void Game::DrawTitleScreen() {
    DrawTilesetBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, Fade(WHITE, 0.72f));
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(Color{9, 14, 20, 255}, 0.42f));

    DrawTilesetSolid(industrialTiles, {0, 690, static_cast<float>(Constants::ScreenWidth), 210}, WHITE);
    DrawTilesetSolid(industrialTiles, {92, 94, 530, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {1030, 442, 410, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {1450, 270, 32, 420}, Fade(WHITE, 0.9f));

    DrawRectangle(80, 145, 720, 390, Fade(BLACK, 0.24f));
    DrawRectangleLinesEx({80, 145, 720, 390}, 2.0f, Fade(RAYWHITE, 0.16f));

    Vector2 titlePosition{120.0f, 176.0f};
    DrawText("POWER", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 6.0f), 100, Fade(BLACK, 0.55f));
    DrawText("PULLEY", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 106.0f), 100, Fade(BLACK, 0.55f));
    DrawText("PANIC", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 206.0f), 100, Fade(BLACK, 0.55f));
    DrawText("POWER", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y), 100, RAYWHITE);
    DrawText("PULLEY", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 100.0f), 100, ORANGE);
    DrawText("PANIC", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 200.0f), 100, RAYWHITE);
    DrawText("Spin to Win", 126, 492, 30, Fade(RAYWHITE, 0.82f));

    float rotation = static_cast<float>(GetTime()) * 80.0f;
    constexpr Vector2 upperLeftPulley{940, 285};
    constexpr Vector2 centerPulley{1188, 408};
    constexpr Vector2 upperRightPulley{1370, 310};
    constexpr Vector2 lowerPulley{1180, 540};
    float titleRopeOffset = rotation * DEG2RAD * 58.0f;
    DrawPulleyRope(upperLeftPulley, 72, centerPulley, 58, -1.0f, 8.0f, BROWN, titleRopeOffset);
    DrawPulleyRope(centerPulley, 58, upperRightPulley, 86, 1.0f, 8.0f, BROWN, titleRopeOffset);
    DrawPulleyRope(lowerPulley, 48, upperRightPulley, 86, 1.0f, 8.0f, BROWN, titleRopeOffset);
    float counterweightRopeX = upperRightPulley.x - 86.0f;
    DrawRope({counterweightRopeX, upperRightPulley.y}, {counterweightRopeX, 612}, 6.0f, titleRopeOffset);
    DrawPulley(upperLeftPulley, 72, rotation, RAYWHITE);
    DrawPulley(centerPulley, 58, rotation * -1.15f, RAYWHITE);
    DrawPulley(upperRightPulley, 86, rotation * 0.72f, RAYWHITE);
    DrawPulley(lowerPulley, 48, rotation * 1.35f, RAYWHITE);

    Rectangle counterweight{counterweightRopeX - 32.0f, 612, 64, 78};
    DrawRectangleRec(counterweight, GRAY);
    DrawRectangleLinesEx(counterweight, 2.0f, BLACK);

    if (playerSpritesTexture.id > 0) {
        DrawTexturePro(
            playerSpritesTexture,
            {0.0f, 0.0f, 37.0f, 47.0f},
            {1117, 395, 37, 47},
            {0, 0},
            0.0f,
            WHITE
        );
    }

    DrawRectangle(1048, 486, 374, 324, Fade(BLACK, 0.26f));
    DrawRectangleLinesEx({1048, 486, 374, 324}, 2.0f, Fade(RAYWHITE, 0.15f));

    std::vector<MenuButton> buttons;
    if (titleModeMenuOpen) {
        DrawText("Select Mode", 1080, 495, 20, Fade(RAYWHITE, 0.86f));
        buttons = {
            {{1080, 530, 310, 46}, "Single Player"},
            {{1080, 586, 310, 46}, "2 Players"},
            {{1080, 642, 310, 46}, "3 Players"},
            {{1080, 698, 310, 46}, "Back"}
        };
    }
    else {
        buttons = {
            {{1080, 510, 310, 46}, "New Game"},
            {{1080, 566, 310, 46}, "Continue"},
            {{1080, 622, 310, 46}, "Load Custom"},
            {{1080, 678, 310, 46}, "Settings"},
            {{1080, 734, 310, 46}, "Quit Game"}
        };
    }

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    if (!menuMessage.empty()) {
        DrawText(menuMessage.c_str(), 1080, 825, 20, ORANGE);
    }
}

void Game::DrawOverworld() {
    DrawTilesetBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.34f));

    DrawRectangle(0, 690, Constants::ScreenWidth, 235, Color{20, 24, 28, 215});
    for (int x = 0; x < Constants::ScreenWidth; x += 70) {
        DrawLineEx({static_cast<float>(x), 690.0f}, {static_cast<float>(x + 120), 925.0f}, 2.0f, Fade(BLACK, 0.22f));
    }

    DrawText("FACTORY DISTRICT", 95, 70, 50, RAYWHITE);
    DrawText("Level Select", 100, 128, 30, ORANGE);

    DrawCircle(150, 250, 70, Fade(BLACK, 0.22f));
    DrawCircleLines(150, 250, 70, Fade(RAYWHITE, 0.16f));
    DrawLineEx({110, 250}, {190, 250}, 6.0f, Fade(RAYWHITE, 0.16f));
    DrawLineEx({150, 210}, {150, 290}, 6.0f, Fade(RAYWHITE, 0.16f));
    DrawCircle(1420, 165, 105, Fade(BLACK, 0.18f));
    DrawCircleLines(1420, 165, 105, Fade(RAYWHITE, 0.14f));

    for (const OverworldPath& path : overworldPaths) {
        const OverworldNode& from = overworldNodes[path.fromNode];
        const OverworldNode& to = overworldNodes[path.toNode];
        bool powered = from.unlocked && to.unlocked;
        Color pipeColor = powered ? Color{178, 128, 45, 255} : Color{75, 82, 88, 255};
        Color innerColor = powered ? ORANGE : Color{34, 40, 46, 255};

        DrawLineEx(from.position, to.position, 26.0f, Fade(BLACK, 0.35f));
        DrawLineEx(from.position, to.position, 18.0f, pipeColor);
        DrawLineEx(from.position, to.position, 5.0f, innerColor);
    }

    for (int i = 0; i < static_cast<int>(overworldNodes.size()); i++) {
        const OverworldNode& node = overworldNodes[i];
        bool selected = i == selectedOverworldNode;
        bool hovered = CheckCollisionPointCircle(GetUiMousePosition(), node.position, 34.0f);

        Color fill = Color{45, 52, 59, 255};
        Color ring = Fade(RAYWHITE, 0.55f);
        if (node.completed) {
            fill = Color{42, 92, 64, 255};
            ring = GREEN;
        }
        else if (node.unlocked) {
            fill = Color{64, 75, 83, 255};
            ring = Color{210, 154, 58, 255};
        }

        if (selected) {
            DrawCircleV(node.position, 47.0f + sinf(static_cast<float>(GetTime()) * 6.0f) * 2.5f, Fade(ORANGE, 0.28f));
        }

        DrawCircleV(node.position, 38.0f, Fade(BLACK, 0.45f));
        DrawCircleV(node.position, 32.0f, fill);
        DrawCircleLinesV(node.position, 32.0f, hovered || selected ? ORANGE : ring);
        DrawCircleLinesV(node.position, 25.0f, Fade(RAYWHITE, node.unlocked ? 0.24f : 0.11f));

        const char* label = node.unlocked ? node.label.c_str() : "X";
        int labelSize = node.unlocked ? 30 : 20;
        DrawCenteredText(label, static_cast<int>(node.position.x), static_cast<int>(node.position.y - labelSize * 0.5f), labelSize, node.unlocked ? RAYWHITE : Fade(RAYWHITE, 0.38f));
    }

    Rectangle panel{1030, 510, 460, 220};
    DrawRectangleRec(panel, Color{20, 27, 34, 235});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.45f));

    const OverworldNode& selectedNode = overworldNodes[selectedOverworldNode];
    DrawText(selectedNode.name.c_str(), 1060, 540, 30, RAYWHITE);
    DrawText(TextFormat("Node %s", selectedNode.label.c_str()), 1060, 580, 20, ORANGE);
    DrawText(selectedNode.unlocked ? "Status: Unlocked" : "Status: Locked", 1060, 614, 20, selectedNode.unlocked ? GREEN : LIGHTGRAY);
    DrawText(selectedNode.unlocked ? "Enter / click starts the level" : "Complete earlier levels to unlock", 1060, 648, 20, LIGHTGRAY);

    std::vector<MenuButton> buttons{
        {{1240, 760, 250, 46}, "Back to Title"},
        {{1240, 816, 250, 46}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    DrawText("A/D or arrows select    Enter starts    Esc returns", 95, 825, 20, LIGHTGRAY);
    DrawText("` opens console", 95, 858, 20, LIGHTGRAY);

    if (!menuMessage.empty()) {
        DrawWrappedText(menuMessage, {1060, 682, 400, 48}, 20, 4, ORANGE);
    }
}

void Game::DrawPauseScreen() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.58f));

    constexpr float buttonWidth = 350.0f;
    constexpr float buttonHeight = 46.0f;
    constexpr float buttonGap = 10.0f;
    float menuX = Constants::ScreenWidth * 0.5f - buttonWidth * 0.5f;

    DrawCenteredText("PAUSED", Constants::ScreenWidth / 2, 300, 70, RAYWHITE);

    std::vector<MenuButton> buttons{
        {{menuX, 400, buttonWidth, buttonHeight}, "Resume"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 1, buttonWidth, buttonHeight}, "Restart Level"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 2, buttonWidth, buttonHeight}, "Controls"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 3, buttonWidth, buttonHeight}, "Settings"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 4, buttonWidth, buttonHeight}, "Return to Map"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 5, buttonWidth, buttonHeight}, "Return to Title Screen"},
        {{menuX, 400 + (buttonHeight + buttonGap) * 6, buttonWidth, buttonHeight}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    if (!menuMessage.empty()) {
        int textWidth = MeasureText(menuMessage.c_str(), 20);
        DrawText(menuMessage.c_str(), Constants::ScreenWidth / 2 - textWidth / 2, 810, 20, LIGHTGRAY);
    }
}

void Game::DrawGameOverActions() {
    std::vector<MenuButton> buttons{
        {{625, 535, 350, 46}, "Restart Level"},
        {{625, 591, 350, 46}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }
}

void Game::DrawControlsPopup() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.35f));

    Rectangle panel{430, 180, 740, 660};
    int panelCenterX = static_cast<int>(panel.x + panel.width * 0.5f);

    DrawRectangleRec(panel, Color{22, 28, 35, 248});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.55f));

    DrawCenteredText("Controls", panelCenterX, 218, 40, RAYWHITE);

    struct ControlRow {
        const char* action;
        std::string input;
    };

    const std::vector<ControlRow> rows{
        {"Player 1 Move", std::string(GetKeyName(player1Bindings.left)) + " / " + GetKeyName(player1Bindings.right)},
        {"Player 1 Jump", std::string(GetKeyName(player1Bindings.up)) + " / " + GetKeyName(player1Bindings.jump)},
        {"Player 1 Swim", std::string(GetKeyName(player1Bindings.up)) + " / " + GetKeyName(player1Bindings.down)},
        {"Player 1 Climb", std::string(GetKeyName(player1Bindings.up)) + " / " + GetKeyName(player1Bindings.down)},
        {"Player 2 Move", "J / L"},
        {"Player 2 Jump", "I"},
        {"Player 2 Swim", "I / K"},
        {"Player 2 Climb", "I / K"},
        {"Player 3 Move", "Left / Right"},
        {"Player 3 Jump", "Up"},
        {"Player 3 Swim / Climb", "Up / Down"},
        {"Player 1 Interact", GetKeyName(player1Bindings.interact)},
        {"Player 2 Interact", "U"},
        {"Player 3 Interact", "Right Ctrl"},
        {"Grab / Turn / Lock", std::string(GetKeyName(player1Bindings.interact)) + " / U / Right Ctrl"},
        {"Pause / Resume", "Esc / Enter"},
        {"Map Select", "A / D or Arrow Keys"},
        {"Select / Start", "Enter / Space"},
        {"Console", "` / ~"}
    };

    int startY = 285;
    int rowHeight = 25;
    int actionX = 525;
    int inputX = 795;
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        int y = startY + i * rowHeight;
        Color rowColor = i % 2 == 0 ? Fade(RAYWHITE, 0.06f) : Fade(RAYWHITE, 0.025f);
        DrawRectangle(500, y - 4, 600, 30, rowColor);
        DrawText(rows[i].action, actionX, y, 20, LIGHTGRAY);
        DrawText(rows[i].input.c_str(), inputX, y, 20, RAYWHITE);
    }

    std::vector<MenuButton> buttons{
        {{705, 775, 190, 46}, "Close"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }
}

void Game::DrawSettingsPopup() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.35f));

    const SettingsMenuLayout layout = GetSettingsMenuLayout();
    const int panelCenterX = static_cast<int>(layout.panel.x + layout.panel.width * 0.5f);

    DrawRectangleRec(layout.panel, Color{22, 28, 35, 248});
    DrawRectangleLinesEx(layout.panel, 2.0f, Fade(RAYWHITE, 0.55f));

    DrawCenteredText("Settings", panelCenterX, static_cast<int>(layout.panel.y + 14.0f), 36, RAYWHITE);

    constexpr const char* tabLabels[] = {"Display", "Audio", "Controls", "Accessibility"};
    for (int i = 0; i < static_cast<int>(layout.tabs.size()); ++i) {
        DrawMenuButton({layout.tabs[i], tabLabels[i]});
        if (static_cast<int>(settingsPage) == i) {
            DrawRectangle(
                static_cast<int>(layout.tabs[i].x + 8.0f),
                static_cast<int>(layout.tabs[i].y + layout.tabs[i].height - 4.0f),
                static_cast<int>(layout.tabs[i].width - 16.0f),
                3,
                ORANGE
            );
        }
    }

    if (settingsPage == SettingsPage::Display) {
        DrawMenuButton({layout.controls[0], TextFormat("Window Mode: %s", WindowModeLabel(pendingWindowMode))});
        DrawMenuButton({layout.controls[1], TextFormat("Resolution: %s", GetResolutionPreset(pendingResolutionIndex).label)});
        DrawMenuButton({layout.controls[2], TextFormat("VSync: %s", OnOffLabel(pendingVsyncEnabled))});
        DrawMenuButton({layout.controls[3], TextFormat("Frame Limit: %s", kFrameRateLabels[pendingFrameRateIndex])});
        DrawMenuButton({layout.controls[4], TextFormat("UI Scale: %s", kUiScaleLabels[pendingUiScaleIndex])});
        DrawMenuButton({layout.controls[5], TextFormat("Pixel-Perfect: %s", OnOffLabel(pendingPixelPerfectScaling))});
        DrawMenuButton({layout.controls[6], TextFormat("Fluid Simulation: %s", FluidModeName(pendingAdvancedFluidSimulation))});
    }
    else if (settingsPage == SettingsPage::Audio) {
        const char* labels[] = {"Master", "Music", "Sound Effects"};
        const float values[] = {pendingMasterVolume, pendingMusicVolume, pendingSoundEffectsVolume};
        for (int i = 0; i < 3; ++i) {
            const Rectangle row = layout.controls[i];
            const Rectangle track{row.x + 150.0f, row.y + 16.0f, row.width - 220.0f, 12.0f};
            DrawRectangleRec(row, Color{34, 42, 52, 225});
            DrawRectangleLinesEx(row, 2.0f, Fade(RAYWHITE, 0.45f));
            DrawText(labels[i], static_cast<int>(row.x + 12.0f), static_cast<int>(row.y + 12.0f), 18, RAYWHITE);
            DrawRectangleRec(track, Color{16, 21, 27, 255});
            DrawRectangleRec({track.x, track.y, track.width * values[i], track.height}, ORANGE);
            DrawCircleV({track.x + track.width * values[i], track.y + track.height * 0.5f}, 8.0f, RAYWHITE);
            DrawText(TextFormat("%d%%", static_cast<int>(roundf(values[i] * 100.0f))),
                static_cast<int>(row.x + row.width - 54.0f), static_cast<int>(row.y + 12.0f), 16, RAYWHITE);
        }
        DrawMenuButton({layout.controls[3], TextFormat("Mute All: %s", OnOffLabel(pendingAudioMuted))});
    }
    else if (settingsPage == SettingsPage::Controls) {
        const char* actions[] = {"Move Left", "Move Right", "Move Up", "Move Down", "Jump", "Interact"};
        const KeyboardKey keys[] = {
            pendingPlayer1Bindings.left,
            pendingPlayer1Bindings.right,
            pendingPlayer1Bindings.up,
            pendingPlayer1Bindings.down,
            pendingPlayer1Bindings.jump,
            pendingPlayer1Bindings.interact
        };
        for (int i = 0; i < 6; ++i) {
            const char* keyName = settingsBindingCapture == i ? "Press a key..." : GetKeyName(keys[i]);
            DrawMenuButton({layout.controls[i], TextFormat("%s: %s", actions[i], keyName)});
        }
        DrawMenuButton({layout.controls[6], TextFormat("Controller: %s", OnOffLabel(pendingControllerEnabled))});
        DrawMenuButton({layout.controls[7], TextFormat("Controller Vibration: %s", OnOffLabel(pendingControllerVibrationEnabled))});
    }
    else {
        DrawMenuButton({layout.controls[0], TextFormat("Screen Shake: %s", ScreenShakeLabel(pendingScreenShakeSetting))});
        DrawMenuButton({layout.controls[1], TextFormat("Reduced Flashing: %s", OnOffLabel(pendingReducedFlashing))});
        DrawMenuButton({layout.controls[2], TextFormat("High Contrast: %s", OnOffLabel(pendingHighContrast))});
        DrawMenuButton({layout.controls[3], TextFormat("Colorblind Mode: %s", ColorblindLabel(pendingColorblindSetting))});
    }

    DrawMenuButton({layout.applyButton, "Apply"});
    DrawMenuButton({layout.closeButton, "Close"});

    if (settingsDropdown != SettingsDropdown::None) {
        Rectangle anchor{};
        int optionCount = 0;
        int columns = 1;
        switch (settingsDropdown) {
        case SettingsDropdown::WindowMode: anchor = layout.controls[0]; optionCount = 3; break;
        case SettingsDropdown::Resolution: anchor = layout.controls[1]; optionCount = ResolutionPresetCount(); columns = 2; break;
        case SettingsDropdown::FrameRate: anchor = layout.controls[3]; optionCount = kFrameRateCount; break;
        case SettingsDropdown::UiScale: anchor = layout.controls[4]; optionCount = kUiScaleCount; break;
        case SettingsDropdown::ScreenShake: anchor = layout.controls[0]; optionCount = 3; break;
        case SettingsDropdown::ColorblindMode: anchor = layout.controls[3]; optionCount = 4; break;
        default: break;
        }

        const DropdownLayout dropdown = GetDropdownLayout(anchor, optionCount, columns);
        DrawRectangleRec(dropdown.panel, Color{17, 22, 28, 255});
        DrawRectangleLinesEx(dropdown.panel, 1.5f, Fade(RAYWHITE, 0.35f));
        for (int i = 0; i < optionCount; ++i) {
            const char* label = "";
            switch (settingsDropdown) {
            case SettingsDropdown::WindowMode: label = WindowModeLabel(static_cast<WindowModeSetting>(i)); break;
            case SettingsDropdown::Resolution: label = GetResolutionPreset(i).label; break;
            case SettingsDropdown::FrameRate: label = kFrameRateLabels[i]; break;
            case SettingsDropdown::UiScale: label = kUiScaleLabels[i]; break;
            case SettingsDropdown::ScreenShake: label = ScreenShakeLabel(static_cast<ScreenShakeSetting>(i)); break;
            case SettingsDropdown::ColorblindMode: label = ColorblindLabel(static_cast<ColorblindSetting>(i)); break;
            default: break;
            }
            DrawMenuButton({dropdown.options[i], label});
        }
    }
}

void Game::DrawQuitConfirmation() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.45f));

    Rectangle panel{450, 330, 700, 260};
    int panelCenterX = static_cast<int>(panel.x + panel.width * 0.5f);

    DrawRectangleRec(panel, Color{22, 28, 35, 245});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.55f));

    DrawCenteredText("Quit Game?", panelCenterX, 370, 40, RAYWHITE);
    DrawCenteredText("Unsaved progress in this session will be lost.", panelCenterX, 430, 20, LIGHTGRAY);

    std::vector<MenuButton> buttons{
        {{575, 500, 190, 46}, "Yes"},
        {{835, 500, 190, 46}, "No"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    DrawCenteredText("Enter/Y confirms    Esc/N cancels", panelCenterX, 552, 20, Fade(RAYWHITE, 0.72f));
}

void Game::DrawGameplay() {
    if (showFPS) DrawFPS(10, 10);

    if (IsTilesetReferenceLevel(level) || HasFarBackgroundTiles(level)) {
        DrawTiledTextureRect(
            industrialTiles,
            {32.0f, 64.0f, 32.0f, 32.0f},
            {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)},
            WHITE
        );
    }
    else {
        DrawTilesetBackgroundFill(
            industrialBackground,
            {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)},
            Fade(WHITE, 0.68f),
            0.08f
        );
        DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(Color{13, 20, 28, 255}, 0.16f));
    }

    bool hasExplicitVisualTiles = !level.visualTiles.empty();
    if (hasExplicitVisualTiles) {
        for (const VisualTile& tile : level.visualTiles) {
            if (tile.layer == TileLayer::FarBackground) {
                DrawTilesetTile(industrialFarBackground, 0, 0, tile.position, WHITE);
            }
        }
        for (const VisualTile& tile : level.visualTiles) {
            if (tile.layer == TileLayer::Background) {
                DrawTilesetTile(industrialBackground, tile.column, tile.row, tile.position, WHITE);
            }
        }
        for (const FluidField& fluid : level.fluids) {
            DrawFluidBackground(fluid);
        }
        for (const VisualTile& tile : level.visualTiles) {
            if (tile.layer == TileLayer::Foreground) {
                DrawTilesetTile(industrialTiles, tile.column, tile.row, tile.position, WHITE);
            }
        }
    }
    else {
        for (const FluidField& fluid : level.fluids) {
            DrawFluidBackground(fluid);
        }
        for (const Rectangle& solid : level.baseSolids) {
            if (IsCeilingSolid(solid)) {
                DrawTilesetCeiling(industrialTiles, solid, WHITE);
            }
            else if (IsWallSolid(solid)) {
                DrawTilesetWall(industrialTiles, solid, WHITE);
            }
            else if (HasSolidTouchingTop(solid, level.baseSolids)) {
                DrawTilesetSolidFill(industrialTiles, solid, WHITE);
            }
            else {
                DrawTilesetSolid(industrialTiles, solid, WHITE);
            }
            DrawRectangleLinesEx(solid, 2, BLACK);
        }
    }

    if (HasArea(level.ladder)) {
        DrawLineEx({level.ladder.x + 8, level.ladder.y}, {level.ladder.x + 8, level.ladder.y + level.ladder.height}, 4, BLACK);
        DrawLineEx({level.ladder.x + level.ladder.width - 8, level.ladder.y}, {level.ladder.x + level.ladder.width - 8, level.ladder.y + level.ladder.height}, 4, BLACK);
        for (int i = 0; i < 13; i++) {
            float y = level.ladder.y + i * 30;
            DrawLineEx({level.ladder.x + 8, y}, {level.ladder.x + level.ladder.width - 8, y}, 3, BLACK);
        }
    }

    for (const Chain& chain : level.chains) {
        DrawChain(chain, chainLinksTexture);
    }
    for (const PhysicsRope& rope : level.physicsRopes) {
        DrawPhysicsRope(rope);
    }

    bool hasFactoryMachine = level.pulleys.size() >= 5;
    float flicker = 0.0f;
    float ropePatternOffset = pulleyRotation * DEG2RAD * 45.0f;
    if (hasFactoryMachine) {
        bool player1NearWinch = playerAlive && IsNearRect(player.rect, machineWinch.rect, 18.0f);
        bool player2NearWinch = multiplayerEnabled && player2Alive && IsNearRect(player2.rect, machineWinch.rect, 18.0f);
        bool player3NearWinch = threePlayerEnabled && player3Alive && IsNearRect(player3.rect, machineWinch.rect, 18.0f);
        std::string winchPrompt = GetInteractPrompt(player1NearWinch, player2NearWinch, player3NearWinch, "Press");
        DrawWinch(machineWinch);
        DrawText(machineWinch.grabbed ? "Move to push" : winchPrompt.c_str(), static_cast<int>(machineWinch.rect.x - 28.0f), static_cast<int>(machineWinch.rect.y - 28.0f), 20, machineWinch.grabbed ? ORANGE : BLACK);

        DrawRope({machineWinch.rect.x + machineWinch.rect.width, machineWinch.rect.y + 20}, {level.pulleys[0].x - 38, level.pulleys[0].y - 38}, 5, ropePatternOffset);
        DrawRope({level.pulleys[0].x, level.pulleys[0].y + 55}, {level.pulleys[1].x, level.pulleys[1].y - 45}, 5, ropePatternOffset);
        DrawRope({level.pulleys[1].x, level.pulleys[1].y + 45}, {level.pulleys[2].x, level.pulleys[2].y - 45}, 5, ropePatternOffset);
        DrawRope({level.pulleys[2].x, level.pulleys[2].y + 45}, {level.pulleys[3].x, level.pulleys[3].y - 45}, 5, ropePatternOffset);
        DrawRope({level.pulleys[3].x, level.pulleys[3].y + 45}, {level.pulleys[4].x, level.pulleys[4].y - 55}, 5, ropePatternOffset);

        DrawPulley(level.pulleys[0], 55, pulleyRotation, BLACK);
        DrawPulley(level.pulleys[1], 45, pulleyRotation * 1.2f, BLACK);
        DrawPulley(level.pulleys[2], 45, pulleyRotation * 1.4f, BLACK);
        DrawPulley(level.pulleys[3], 45, pulleyRotation * 1.6f, BLACK);
        DrawPulley(level.pulleys[4], 55, pulleyRotation * 1.1f, BLACK);

        Rectangle generatorBox{565, 365, 90, 70};
        Vector2 generatorGear{
            generatorBox.x + generatorBox.width * 0.42f,
            generatorBox.y + generatorBox.height * 0.48f
        };
        DrawMachineBox(generatorBox, pulleyRotation * 1.7f, machinePower > 0.04f);

        float mainWeightY = 390.0f + machinePower * 92.0f;
        float counterweightRopeOffset = mainWeightY - 390.0f;
        float mainRopeX = level.pulleys[0].x + 55.0f;
        DrawRope({mainRopeX, level.pulleys[0].y}, {mainRopeX, mainWeightY}, 5, counterweightRopeOffset);
        DrawRectangle(mainRopeX - 25, mainWeightY, 50, 60, GRAY);
        DrawRectangleLines(mainRopeX - 25, mainWeightY, 50, 60, BLACK);
        DrawPulleyRope(
            {mainRopeX, mainWeightY + 60},
            0.0f,
            generatorGear,
            27.0f,
            1.0f,
            4.0f,
            BROWN,
            counterweightRopeOffset
        );
        DrawRing(generatorGear, 22.0f, 27.0f, 0.0f, 360.0f, 32, BROWN);
        DrawCircleLinesV(generatorGear, 27.0f, BLACK);

        bool lightsOn = machinePower > 0.12f;
        Color wireColor = lightsOn ? BLUE : DARKBLUE;

        DrawLineEx({655, 400}, {760, 400}, 4, wireColor);
        DrawLineEx({760, 400}, {760, 320}, 4, wireColor);
        DrawLineEx({760, 320}, {1220, 320}, 4, wireColor);
        DrawLineEx({1220, 320}, {1220, 598}, 4, wireColor);
        float gateMotorWireX = HasArea(level.exitTrigger) ? level.exitTrigger.x - 115.0f : 1370.0f;
        DrawLineEx({1220, 598}, {gateMotorWireX, 598}, 4, wireColor);

        float flickerCycle = fmodf(static_cast<float>(GetTime()), 3.4f);
        if (!reducedFlashing && machinePower < 0.65f && flickerCycle < 0.22f) {
            float sputter = 0.5f + sinf(static_cast<float>(GetTime()) * 75.0f) * 0.5f;
            flicker = -0.10f * sputter * (1.0f - machinePower);
        }

        for (int i = 0; i < 5; i++) {
            float x = 780 + i * 95;
            float lampPower = Clamp01(0.18f + machinePower * 0.82f + flicker);

            DrawLineEx({x, 320}, {x, 360}, 3, BLACK);
            DrawCircleV({x, 375}, 13, Fade(YELLOW, 0.35f + lampPower * 0.65f));
            DrawTriangle({x - 38, 445}, {x + 38, 445}, {x, 385}, Fade(YELLOW, 0.08f + lampPower * 0.45f));
        }
    }

    for (const HangingWeight& weight : level.weights) {
        DrawHazardWeight(weight, ropePatternOffset);
    }

    if (HasFloodWaterControl(level)) {
        bool player1NearValve = playerAlive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
        bool player2NearValve = multiplayerEnabled && player2Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player2.rect);
        bool player3NearValve = threePlayerEnabled && player3Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player3.rect);
        bool playerNearValve = player1NearValve || player2NearValve || player3NearValve;
        std::string valvePrompt = GetInteractPrompt(player1NearValve, player2NearValve, player3NearValve, "Hold");
        float valveOpenAmount = GetValveOpenAmount(level.valve);
        Color pipeColor = valveOpenAmount > 0.0f ? BLUE : DARKGRAY;
        float outletX = level.valve.center.x + 118.0f;
        float outletY = 302.0f;
        DrawLineEx({level.valve.center.x + level.valve.radius, level.valve.center.y}, {outletX, level.valve.center.y}, 12.0f, pipeColor);
        DrawLineEx({outletX, level.valve.center.y}, {outletX, outletY}, 12.0f, pipeColor);
        DrawRectangle(static_cast<int>(outletX - 18.0f), static_cast<int>(outletY - 4.0f), 36, 18, DARKGRAY);
        DrawRectangleLines(static_cast<int>(outletX - 18.0f), static_cast<int>(outletY - 4.0f), 36, 18, BLACK);
        if (valveOpenAmount > 0.0f && GetFloodWaterProgress(level) < 0.999f) {
            DrawLineEx(
                {outletX, outletY + 14.0f},
                {outletX, GetFloodWaterSurfaceY(level) + 8.0f},
                3.0f + valveOpenAmount * 7.0f,
                Fade(SKYBLUE, 0.35f + valveOpenAmount * 0.45f)
            );
        }
        DrawValve(level.valve, playerNearValve, valvePrompt.c_str());
    }

    if (HasArea(level.spikeHazard)) {
        float pitTopY = level.script == LevelScript::FloodedFoundry ? 672.0f : 682.0f;
        float spikeBaseY = level.spikeHazard.y + level.spikeHazard.height;
        Rectangle pitShaft{
            level.spikeHazard.x,
            pitTopY,
            level.spikeHazard.width,
            spikeBaseY - pitTopY
        };
        Rectangle pitFoundation{
            level.spikeHazard.x,
            spikeBaseY,
            level.spikeHazard.width,
            static_cast<float>(Constants::ScreenHeight) - spikeBaseY
        };

        DrawTilesetPitWalls(industrialTiles, pitShaft, Fade(WHITE, 0.88f));
        DrawTilesetPitFoundation(industrialTiles, pitFoundation, Fade(WHITE, 0.88f));
        DrawSpikes(level.spikeHazard.x + 2.0f, spikeBaseY, static_cast<int>(level.spikeHazard.width / 28.0f));
    }

    if (HasWaterPit(level)) {
        DrawWaterPit(level.waterPit);
    }

    if (!hasExplicitVisualTiles) {
        for (Rectangle platform : level.pitPlatforms) {
            DrawTilesetSolid(industrialTiles, platform, WHITE);
            DrawRectangleLinesEx(platform, 2, BLACK);
        }
    }

    for (const BreakableTile& tile : level.breakableTiles) {
        DrawBreakableTile(industrialTiles, tile);
    }

    std::vector<Rectangle> splashSources;
    if (playerAlive) splashSources.push_back(player.rect);
    if (multiplayerEnabled && player2Alive) splashSources.push_back(player2.rect);
    if (threePlayerEnabled && player3Alive) splashSources.push_back(player3.rect);
    for (const Enemy& enemy : level.enemies) {
        splashSources.push_back(enemy.rect);
    }

    for (const FluidField& fluid : level.fluids) {
        DrawFluidField(fluid, splashSources);
    }

    for (const Button& button : level.buttons) {
        DrawButton(button);
    }

    for (const ArrowTrap& trap : level.arrowTraps) {
        DrawArrowTrap(trap);
    }

    for (const Ramp& ramp : level.ramps) {
        DrawRamp(ramp);
    }

    for (const TrapDoor& trapDoor : level.trapDoors) {
        DrawTrapDoor(trapDoor);
    }

    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawSeeSaw(seeSaw);
    }

    for (const StoneBlock& block : level.stoneBlocks) {
        DrawStoneBlock(block);
    }

    for (const Boulder& boulder : level.boulders) {
        DrawBoulder(boulder);
    }

    for (const PhysicsWheel& wheel : level.physicsWheels) {
        DrawPhysicsWheel(wheel);
    }

    for (const Gear& gear : level.gears) {
        DrawGear(gear);
    }

    for (const Flywheel& flywheel : level.flywheels) {
        DrawFlywheel(flywheel);
    }

    for (const SteeringWheel& steeringWheel : level.steeringWheels) {
        DrawSteeringWheel(steeringWheel);
    }

    for (const Screw& screw : level.screws) {
        DrawScrew(screw);
    }

    for (const Fan& fan : level.fans) {
        DrawWindStreaks(fan);
    }

    for (const Fan& fan : level.fans) {
        DrawFan(fan);
    }

    for (const Pinwheel& pinwheel : level.pinwheels) {
        DrawPinwheel(pinwheel);
    }

    for (const ArrowTrap& trap : level.arrowTraps) {
        for (const ArrowProjectile& arrow : trap.arrows) {
            if (arrow.active) {
                DrawArrowProjectile(arrow);
            }
        }
    }

    for (const Enemy& enemy : level.enemies) {
        DrawEnemy(enemy, enemyPlaceholderTexture);
    }

    int latchedCount = 0;
    for (const RotaryLatch& latch : level.rotaryLatches) {
        bool player1Near = playerAlive && CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player.rect);
        bool player2Near = multiplayerEnabled && player2Alive && CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player2.rect);
        bool player3Near = threePlayerEnabled && player3Alive && CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player3.rect);
        bool playerNear = player1Near || player2Near || player3Near;
        std::string latchPrompt = GetInteractPrompt(player1Near, player2Near, player3Near, "");
        if (latch.latched) {
            latchedCount++;
        }

        DrawRotaryLatch(latch, playerNear, latchPrompt.c_str());
    }

    if (HasArea(level.exitTrigger)) {
        if (hasFactoryMachine) {
            Rectangle gateMotorBox{level.exitTrigger.x - 115.0f, 565, 85, 65};
            Vector2 gateMotorGear{
                gateMotorBox.x + gateMotorBox.width * 0.42f,
                gateMotorBox.y + gateMotorBox.height * 0.48f
            };
            DrawMachineBox(gateMotorBox, pulleyRotation * 1.4f, machinePower > 0.04f);
            DrawRope({level.pulleys[4].x - 48.0f, level.pulleys[4].y + 27.0f}, {gateMotorGear.x - 25.0f, gateMotorGear.y - 7.0f}, 5, -ropePatternOffset);
            DrawRope({level.pulleys[4].x + 24.0f, level.pulleys[4].y + 50.0f}, {gateMotorGear.x + 23.0f, gateMotorGear.y + 10.0f}, 5, ropePatternOffset);
            DrawRing(gateMotorGear, 22.0f, 27.0f, 0.0f, 360.0f, 32, BROWN);
        }

        Rectangle doorway{level.exitTrigger.x + 5.0f, level.exitTrigger.y, level.exitTrigger.width - 10.0f, level.exitTrigger.height};
        DrawRectangleRec(level.exitTrigger, LIGHTGRAY);
        DrawOutdoorDoorway(doorway);
        DrawRectangleLinesEx(level.exitTrigger, 1, BLACK);

        float gateX = level.exitTrigger.x + 5.0f;
        float gateTop = level.exitTrigger.y;
        float gateWidth = level.exitTrigger.width - 10.0f;
        float visibleGateHeight = gateBottom - gateTop;
        if (visibleGateHeight > 0.0f) {
            DrawRectangle(static_cast<int>(gateX), static_cast<int>(gateTop), static_cast<int>(gateWidth), static_cast<int>(visibleGateHeight), BROWN);
            int barCount = static_cast<int>(gateWidth / 13.0f);
            for (int i = 0; i < barCount; i++) {
                float x = gateX + 5.0f + i * 13.0f;
                DrawLineEx({x, gateTop}, {x, gateBottom}, 5, BLACK);
            }
            DrawRectangle(static_cast<int>(gateX), static_cast<int>(gateBottom - 15.0f), static_cast<int>(gateWidth), 15, BLACK);
        }
    }

    if (!playerAlive) {
        DrawDeathMarker(skullTexture, playerDeathRect);
    }
    if (multiplayerEnabled && !player2Alive) {
        DrawDeathMarker(skullTexture, player2DeathRect);
    }
    if (threePlayerEnabled && !player3Alive) {
        DrawDeathMarker(skullTexture, player3DeathRect);
    }

    if (playerAlive) {
        Player visiblePlayer = player;
        if (IsPlayerSwimming(player, level)) {
            visiblePlayer.rect.y += sinf(static_cast<float>(GetTime()) * 5.0f) * 3.5f;
        }
        DrawPlayer(visiblePlayer, playerSpritesTexture, 0);
    }

    if (multiplayerEnabled && player2Alive) {
        Player visiblePlayer2 = player2;
        if (IsPlayerSwimming(player2, level)) {
            visiblePlayer2.rect.y += sinf(static_cast<float>(GetTime()) * 5.0f + 0.65f) * 3.5f;
        }
        DrawPlayer(visiblePlayer2, playerSpritesTexture, 1);
    }

    if (threePlayerEnabled && player3Alive) {
        Player visiblePlayer3 = player3;
        if (IsPlayerSwimming(player3, level)) {
            visiblePlayer3.rect.y += sinf(static_cast<float>(GetTime()) * 5.0f + 1.3f) * 3.5f;
        }
        DrawPlayer(visiblePlayer3, playerSpritesTexture, 2);
    }

    bool hasDarkness = std::any_of(level.darknessAreas.begin(), level.darknessAreas.end(), HasArea);
    if (hasDarkness) {
        float blackoutFlicker = machinePower > 0.02f ? flicker : 0.0f;
        float safeAreaDimAlpha = Clamp01(0.20f + (1.0f - machinePower) * 0.18f - flicker * 0.45f);
        float blackoutAlpha = Clamp01(1.0f - machinePower - blackoutFlicker);
        DrawRectangle(0, 0, 300, Constants::ScreenHeight, Fade(BLACK, safeAreaDimAlpha));
        DrawRectangle(300, 0, 960, 275, Fade(BLACK, safeAreaDimAlpha));
        for (Rectangle darknessArea : level.darknessAreas) {
            DrawRectangleRec(darknessArea, Fade(BLACK, blackoutAlpha));
        }
    }

    int latchTotal = static_cast<int>(level.rotaryLatches.size());
    if (latchTotal > 0) {
        bool allLatchesLocked = latchedCount == latchTotal;
        int statusWidth = threePlayerEnabled ? 350 : 300;
        DrawRectangle(20, 20, statusWidth, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Wheel locks: %d / %d", latchedCount, latchTotal), 34, 31, 20, RAYWHITE);
        const char* latchHelp = threePlayerEnabled ? "Align spokes: E / U / Right Ctrl" :
            (multiplayerEnabled ? "Align spokes, press E or U" : "Align spokes, press E");
        DrawText(allLatchesLocked ? "Gate circuit complete" : latchHelp, 34, 58, 20, allLatchesLocked ? GREEN : ORANGE);
    }

    if (HasFloodWaterControl(level)) {
        float fillPercent = GetFloodWaterProgress(level) * 100.0f;
        float valvePercent = GetValveOpenAmount(level.valve) * 100.0f;
        DrawRectangle(20, 20, 260, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Water level: %.0f%%", fillPercent), 34, 31, 20, RAYWHITE);
        const char* valveHelp = threePlayerEnabled ? TextFormat("E/U/RCtrl: valve %.0f%%", valvePercent) :
            TextFormat(multiplayerEnabled ? "Hold E/U: valve %.0f%%" : "Hold E: valve %.0f%%", valvePercent);
        DrawText(level.valve.opened ? "Swim the flooded pit" : valveHelp, 34, 58, 20, level.valve.opened ? SKYBLUE : ORANGE);
    }

    if (level.script == LevelScript::CounterweightRow) {
        DrawRectangle(20, 20, 370, 66, Fade(BLACK, 0.52f));
        DrawText(machinePower > 0.5f ? "Counterweight locked" : "Roll a boulder onto the plate", 34, 31, 20,
            machinePower > 0.5f ? GREEN : ORANGE);
        DrawText(machinePower > 0.5f ? "Exit gate is open" : "Boulders block incoming arrows", 34, 58, 20, RAYWHITE);
    }

    if (highContrast) {
        const Color danger = AccessibleDangerColor(colorblindSetting);
        if (playerAlive) DrawRectangleLinesEx(player.rect, 3.0f, RAYWHITE);
        if (multiplayerEnabled && player2Alive) DrawRectangleLinesEx(player2.rect, 3.0f, SKYBLUE);
        if (threePlayerEnabled && player3Alive) DrawRectangleLinesEx(player3.rect, 3.0f, LIME);
        for (const Enemy& enemy : level.enemies) DrawRectangleLinesEx(enemy.rect, 3.0f, danger);
        if (HasArea(level.spikeHazard)) DrawRectangleLinesEx(level.spikeHazard, 3.0f, danger);
        if (HasArea(level.exitTrigger)) DrawRectangleLinesEx(level.exitTrigger, 3.0f, AccessibleSuccessColor(colorblindSetting));
    }

    if (won) {
        DrawText("LEVEL CLEAR!", 615, 420, 60, AccessibleSuccessColor(colorblindSetting));
        DrawText("Returning to map...", 650, 485, 26, machinePower < 0.45f && hasDarkness ? WHITE : BLACK);
    }

    if (lost) {
        DrawText("YOU DIED!", 660, 420, 50, AccessibleDangerColor(colorblindSetting));
        DrawGameOverActions();
    }
}

void Game::Unload() {
    if (sceneTarget.id > 0) UnloadRenderTexture(sceneTarget);
    UnloadTexture(playerSpritesTexture);
    UnloadTexture(skullTexture);
    UnloadTexture(industrialTiles);
    UnloadTexture(industrialBackground);
    UnloadTexture(industrialFarBackground);
    UnloadTexture(chainLinksTexture);
    UnloadTexture(enemyPlaceholderTexture);
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
}

void Game::ExecuteConsoleCommand(const std::string& line) {
    std::vector<std::string> args = SplitCommandLine(line);
    if (args.empty()) return;

    std::string command = ToLower(args[0]);
    console.AddLine("> " + line);

    if (command == "help") {
        console.AddLine("Commands: help, clear, start, overworld, title, pause, resume, quit, reset, win, kill, kill_enemy, kill_all_enemies, fps, fluid_sim, debug_collision, disable_arrow_traps, invincible, unlock_all_levels, teleport, power, player, machine");
    }
    else if (command == "clear") {
        console.Clear();
    }
    else if (command == "reset") {
        StartGame();
        console.AddLine("Level reset.");
    }
    else if (command == "start") {
        StartGame();
        console.AddLine("Game started.");
    }
    else if (command == "overworld") {
        OpenOverworld();
        console.AddLine("Opened overworld map.");
    }
    else if (command == "title") {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        console.AddLine("Returned to title screen.");
    }
    else if (command == "pause") {
        mode = GameMode::Paused;
        console.AddLine("Game paused.");
    }
    else if (command == "resume") {
        mode = GameMode::Playing;
        console.AddLine("Game resumed.");
    }
    else if (command == "quit") {
        quitConfirmationOpen = true;
        console.AddLine("Quit confirmation opened.");
    }
    else if (command == "win") {
        mode = GameMode::Playing;
        BeginLevelClear();
        console.AddLine("Level clear triggered.");
    }
    else if (command == "kill_all_enemies" ||
        (command == "kill" && args.size() == 2 && ToLower(args[1]) == "enemies")) {
        size_t enemyCount = level.enemies.size();
        level.enemies.clear();
        console.AddLine(TextFormat("Killed %d enem%s.", static_cast<int>(enemyCount), enemyCount == 1 ? "y" : "ies"));
    }
    else if (command == "kill_enemy" ||
        (command == "kill" && args.size() >= 2 && ToLower(args[1]) == "enemy")) {
        if (level.enemies.empty()) {
            console.AddLine("No enemies to kill.");
            return;
        }

        size_t index = 0;
        size_t indexArgument = command == "kill_enemy" ? 1 : 2;
        if (args.size() > indexArgument) {
            int requestedIndex = 0;
            if (!ParseInt(args[indexArgument], requestedIndex) ||
                requestedIndex < 1 ||
                requestedIndex > static_cast<int>(level.enemies.size())) {
                console.AddLine(TextFormat("Enemy index must be between 1 and %d.", static_cast<int>(level.enemies.size())));
                return;
            }
            index = static_cast<size_t>(requestedIndex - 1);
        }
        else {
            Vector2 playerCenter = RectCenter(player.rect);
            float closestDistanceSquared = -1.0f;
            for (size_t i = 0; i < level.enemies.size(); i++) {
                Vector2 enemyCenter = RectCenter(level.enemies[i].rect);
                float dx = enemyCenter.x - playerCenter.x;
                float dy = enemyCenter.y - playerCenter.y;
                float distanceSquared = dx * dx + dy * dy;
                if (closestDistanceSquared < 0.0f || distanceSquared < closestDistanceSquared) {
                    closestDistanceSquared = distanceSquared;
                    index = i;
                }
            }
        }

        level.enemies.erase(level.enemies.begin() + static_cast<std::vector<Enemy>::difference_type>(index));
        console.AddLine(TextFormat("Killed enemy %d.", static_cast<int>(index + 1)));
    }
    else if (command == "kill") {
        mode = GameMode::Playing;
        KillPlayer(player);
        console.AddLine("Loss state set.");
    }
    else if (command == "fps") {
        showFPS = !showFPS;
        console.AddLine("FPS display " + OnOff(showFPS) + ".");
    }
    else if (command == "fluid_sim" || command == "fluidsim") {
        if (args.size() >= 2) {
            std::string value = ToLower(args[1]);
            if (value == "advanced" || value == "high") {
                advancedFluidSimulation = true;
            }
            else if (value == "simple" || value == "tile" || value == "low" || value == "performance") {
                advancedFluidSimulation = false;
            }
            else {
                console.AddLine("Usage: fluid_sim [advanced|simple]");
                return;
            }
        }
        else {
            advancedFluidSimulation = !advancedFluidSimulation;
        }

        const Player* activePlayer1 = playerAlive ? &player : nullptr;
        const Player* activePlayer2 = multiplayerEnabled && player2Alive ? &player2 : nullptr;
        const Player* activePlayer3 = threePlayerEnabled && player3Alive ? &player3 : nullptr;
        std::vector<Rectangle> obstacles = BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3);
        for (FluidField& fluid : level.fluids) {
            InitializeFluidField(fluid, obstacles, SelectedFluidMode(advancedFluidSimulation));
        }
        console.AddLine(std::string("Fluid simulation ") + FluidModeName(advancedFluidSimulation) + ".");
    }
    else if (command == "disable_arrow_traps") {
        if (args.size() >= 2) {
            std::string value = ToLower(args[1]);
            if (value == "on") {
                arrowTrapsDisabled = true;
            }
            else if (value == "off") {
                arrowTrapsDisabled = false;
            }
            else {
                console.AddLine("Usage: disable_arrow_traps [on|off]");
                return;
            }
        }
        else {
            arrowTrapsDisabled = !arrowTrapsDisabled;
        }

        if (arrowTrapsDisabled) {
            for (ArrowTrap& trap : level.arrowTraps) {
                trap.arrows.clear();
            }
        }
        console.AddLine("Arrow traps disabled " + OnOff(arrowTrapsDisabled) + ".");
    }
    else if (command == "invincible") {
        if (args.size() >= 2) {
            std::string value = ToLower(args[1]);
            if (value == "on") {
                playerInvincible = true;
            }
            else if (value == "off") {
                playerInvincible = false;
            }
            else {
                console.AddLine("Usage: invincible [on|off]");
                return;
            }
        }
        else {
            playerInvincible = !playerInvincible;
        }

        console.AddLine("Player invincibility " + OnOff(playerInvincible) + ".");
    }
    else if (command == "debug_collision") {
        if (args.size() >= 2) {
            std::string value = ToLower(args[1]);
            if (value == "on") {
                debugCollision = true;
            }
            else if (value == "off") {
                debugCollision = false;
            }
            else {
                console.AddLine("Usage: debug_collision [on|off]");
                return;
            }
        }
        else {
            debugCollision = !debugCollision;
        }

        console.AddLine("Collision debug " + OnOff(debugCollision) + ".");
    }
    else if (command == "unlock_all_levels" ||
        command == "unlockall" ||
        (command == "unlock" && args.size() >= 3 && ToLower(args[1]) == "all" && ToLower(args[2]) == "levels")) {
        if (overworldNodes.empty()) {
            InitializeOverworld();
        }

        for (OverworldNode& node : overworldNodes) {
            node.unlocked = true;
        }

        menuMessage = "All levels unlocked.";
        console.AddLine("All map levels unlocked.");
    }
    else if (command == "teleport") {
        if (args.size() != 3) {
            console.AddLine("Usage: teleport <x> <y>");
            return;
        }

        float x = 0.0f;
        float y = 0.0f;
        if (!ParseFloat(args[1], x) || !ParseFloat(args[2], y)) {
            console.AddLine("teleport needs numeric x and y values.");
            return;
        }

        player.rect.x = x;
        player.rect.y = y;
        player.velocity = {0, 0};
        deathRect = player.rect;
        playerDeathRect = player.rect;
        player2DeathRect = player2.rect;
        player3DeathRect = player3.rect;
        mode = GameMode::Playing;
        won = false;
        lost = false;
        playerAlive = true;
        player2Alive = true;
        player3Alive = true;
        console.AddLine("Teleported player.");
    }
    else if (command == "power") {
        if (args.size() != 2) {
            console.AddLine("Usage: power <0..1>");
            return;
        }

        float requestedPower = 0.0f;
        if (!ParseFloat(args[1], requestedPower)) {
            console.AddLine("power needs a numeric value.");
            return;
        }

        requestedPower = Clamp01(requestedPower);
        machineWinch.rect.x = machineWinch.startX + requestedPower * (machineWinch.maxX - machineWinch.startX);
        machinePower = requestedPower;
        mode = GameMode::Playing;
        console.AddLine("Machine power set.");
    }
    else if (command == "player") {
        console.AddLine(TextFormat("player x=%.1f y=%.1f vx=%.1f vy=%.1f", player.rect.x, player.rect.y, player.velocity.x, player.velocity.y));
    }
    else if (command == "machine") {
        console.AddLine(TextFormat("machine power=%.2f winch_x=%.1f grabbed=%s", machinePower, machineWinch.rect.x, machineWinch.grabbed ? "true" : "false"));
    }
    else {
        console.AddLine("Unknown command. Type help.");
    }
}

void Game::DrawDebugCollision() const {
    for (const Rectangle& solid : level.baseSolids) {
        DrawRectangleLinesEx(solid, 2, Fade(GREEN, 0.85f));
    }

    for (const Rectangle& platform : level.pitPlatforms) {
        DrawRectangleLinesEx(platform, 2, Fade(SKYBLUE, 0.9f));
    }

    DrawRectangleLinesEx(level.ladder, 2, Fade(YELLOW, 0.9f));
    DrawRectangleLinesEx(level.spikeHazard, 2, Fade(RED, 0.95f));
    DrawRectangleLinesEx(level.exitTrigger, 2, Fade(PURPLE, 0.95f));

    if (HasFloodWaterControl(level)) {
        if (HasWaterPit(level)) {
            DrawRectangleLinesEx(level.waterPit.bounds, 2, Fade(SKYBLUE, 0.95f));
            DrawRectangleLinesEx(GetFilledWaterRect(level.waterPit), 2, Fade(BLUE, 0.95f));
        }
        DrawCircleLinesV(level.valve.center, level.valve.radius + 24.0f, Fade(BLUE, 0.9f));
    }

    for (const FluidField& fluid : level.fluids) {
        Color fluidColor = fluid.type == FluidType::Water ? SKYBLUE :
            (fluid.type == FluidType::Sand ? GOLD : (fluid.type == FluidType::Gel ? BLUE : LIME));
        DrawRectangleLinesEx(fluid.bounds, 2, Fade(fluidColor, 0.95f));
        if (fluid.type == FluidType::Water || fluid.type == FluidType::Sand) {
            for (int index = 0; index < static_cast<int>(fluid.cells.size()); index++) {
                const FluidCell& cell = fluid.cells[index];
                if (cell.mass <= 0.01f || cell.solid) continue;
                Vector2 center = GetFluidSimulationPoint(fluid, index);
                DrawRectangleLinesEx({center.x - fluid.cellSize * 0.5f, center.y - fluid.cellSize * 0.5f,
                    fluid.cellSize, fluid.cellSize}, 1.0f, Fade(fluidColor, 0.14f + cell.mass * 0.34f));
                DrawLineV(center, {center.x + cell.velocity.x * 0.025f, center.y + cell.velocity.y * 0.025f},
                    Fade(RAYWHITE, 0.55f));
            }
            continue;
        }
        for (const FluidParticle& particle : fluid.particles) {
            DrawCircleLinesV(
                particle.position,
                fluid.particleRadius,
                Fade(fluidColor, 0.20f + particle.density * 0.42f)
            );
            DrawLineEx(
                particle.position,
                {
                    particle.position.x + particle.velocity.x * 0.035f,
                    particle.position.y + particle.velocity.y * 0.035f
                },
                1.0f,
                Fade(RAYWHITE, 0.55f)
            );
        }
    }

    for (const HangingWeight& weight : level.weights) {
        DrawRectangleLinesEx(weight.rect, 2, Fade(RED, 0.95f));
    }

    for (const RotaryLatch& latch : level.rotaryLatches) {
        DrawCircleLinesV(latch.center, latch.radius + 20.0f, Fade(PURPLE, 0.9f));
    }

    for (const StoneBlock& block : level.stoneBlocks) {
        DrawRectangleLinesEx(block.rect, 2, Fade(ORANGE, 0.95f));
    }

    for (const Boulder& boulder : level.boulders) {
        DrawCircleLinesV(boulder.center, boulder.radius, Fade(ORANGE, 0.95f));
    }

    for (const PhysicsWheel& wheel : level.physicsWheels) {
        DrawCircleLinesV(wheel.center, wheel.radius, Fade(GOLD, 0.95f));
        DrawLineEx(
            wheel.center,
            {
                wheel.center.x + cosf(wheel.rotation * DEG2RAD) * wheel.radius,
                wheel.center.y + sinf(wheel.rotation * DEG2RAD) * wheel.radius
            },
            2,
            Fade(GOLD, 0.8f)
        );
    }

    for (const Gear& gear : level.gears) {
        DrawCircleLinesV(gear.center, gear.radius * 1.12f, Fade(GOLD, 0.95f));
    }

    for (const Flywheel& flywheel : level.flywheels) {
        DrawCircleLinesV(flywheel.center, flywheel.radius, Fade(GOLD, 0.95f));
    }

    for (const SteeringWheel& steeringWheel : level.steeringWheels) {
        DrawCircleLinesV(steeringWheel.center, steeringWheel.radius, Fade(GOLD, 0.95f));
    }

    for (const Button& button : level.buttons) {
        DrawRectangleLinesEx(button.rect, 2, Fade(button.pressed ? GREEN : MAROON, 0.95f));
    }

    for (const BreakableTile& tile : level.breakableTiles) {
        if (!tile.broken) {
            DrawRectangleLinesEx(tile.rect, 2, Fade(tile.cracking ? ORANGE : GREEN, 0.95f));
        }
        for (const BreakableDebris& debris : tile.debris) {
            DrawRectangleLinesEx(debris.rect, 1, Fade(ORANGE, 0.7f));
        }
    }

    for (const ArrowTrap& trap : level.arrowTraps) {
        DrawCircleLinesV(trap.position, 18.0f, Fade(RED, 0.8f));
        DrawLineEx(
            trap.position,
            {trap.position.x + trap.direction.x * 40.0f, trap.position.y + trap.direction.y * 40.0f},
            2,
            Fade(RED, 0.8f)
        );
        for (const ArrowProjectile& arrow : trap.arrows) {
            DrawRectangleLinesEx(arrow.rect, 2, Fade(RED, 0.95f));
        }
    }

    for (const Ramp& ramp : level.ramps) {
        DrawLineEx(
            {ramp.center.x - ramp.length * 0.5f, GetRampSurfaceY(ramp, ramp.center.x - ramp.length * 0.5f)},
            {ramp.center.x + ramp.length * 0.5f, GetRampSurfaceY(ramp, ramp.center.x + ramp.length * 0.5f)},
            2,
            Fade(BROWN, 0.95f)
        );
    }

    for (const TrapDoor& trapDoor : level.trapDoors) {
        Vector2 end = GetTrapDoorRingPosition(trapDoor);
        std::array<Vector2, 2> rings = GetTrapDoorRingPositions(trapDoor);
        DrawLineEx(trapDoor.hinge, end, 2, Fade(BROWN, 0.95f));
        DrawCircleLinesV(trapDoor.hinge, trapDoor.thickness * 0.65f, Fade(BROWN, 0.95f));
        for (Vector2 ring : rings) {
            DrawCircleLinesV(ring, trapDoor.thickness * 0.55f, Fade(BROWN, 0.95f));
        }
    }

    for (const Screw& screw : level.screws) {
        DrawRectangleLinesEx(GetScrewBounds(screw), 2, Fade(GOLD, 0.85f));
    }

    for (const Fan& fan : level.fans) {
        Vector2 normal{-fan.direction.y, fan.direction.x};
        Vector2 farCenter{
            fan.center.x + fan.direction.x * fan.length,
            fan.center.y + fan.direction.y * fan.length
        };
        Vector2 a{fan.center.x + normal.x * fan.width * 0.5f, fan.center.y + normal.y * fan.width * 0.5f};
        Vector2 b{fan.center.x - normal.x * fan.width * 0.5f, fan.center.y - normal.y * fan.width * 0.5f};
        Vector2 c{farCenter.x - normal.x * fan.width * 0.5f, farCenter.y - normal.y * fan.width * 0.5f};
        Vector2 d{farCenter.x + normal.x * fan.width * 0.5f, farCenter.y + normal.y * fan.width * 0.5f};
        DrawLineEx(a, b, 2, Fade(SKYBLUE, 0.8f));
        DrawLineEx(b, c, 2, Fade(SKYBLUE, 0.8f));
        DrawLineEx(c, d, 2, Fade(SKYBLUE, 0.8f));
        DrawLineEx(d, a, 2, Fade(SKYBLUE, 0.8f));
        DrawLineEx(fan.center, farCenter, 2, Fade(BLUE, 0.8f));
    }

    for (const Pinwheel& pinwheel : level.pinwheels) {
        DrawCircleLinesV(pinwheel.center, pinwheel.radius, Fade(SKYBLUE, 0.9f));
    }

    for (const Enemy& enemy : level.enemies) {
        DrawRectangleLinesEx(enemy.rect, 2, Fade(RED, 0.95f));
        DrawLineEx(
            {enemy.patrolMinX, enemy.rect.y + enemy.rect.height + 8.0f},
            {enemy.patrolMaxX, enemy.rect.y + enemy.rect.height + 8.0f},
            2,
            Fade(RED, 0.55f)
        );
    }

    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawLineEx(
            {seeSaw.pivot.x - seeSaw.length * 0.5f, GetSeeSawSurfaceY(seeSaw, seeSaw.pivot.x - seeSaw.length * 0.5f)},
            {seeSaw.pivot.x + seeSaw.length * 0.5f, GetSeeSawSurfaceY(seeSaw, seeSaw.pivot.x + seeSaw.length * 0.5f)},
            2,
            Fade(BLUE, 0.95f)
        );
    }

    for (const Chain& chain : level.chains) {
        if (chain.points.size() >= 2) {
            for (int i = 0; i < static_cast<int>(chain.points.size()) - 1; i++) {
                DrawLineEx(chain.points[i], chain.points[i + 1], 2, Fade(DARKBLUE, 0.95f));
                DrawCircleLinesV(chain.points[i], chain.collisionRadius * chain.scale, Fade(DARKBLUE, 0.7f));
            }
            DrawCircleLinesV(chain.points.back(), chain.collisionRadius * chain.scale, Fade(DARKBLUE, 0.7f));
        }
        else {
            DrawLineEx(chain.start, chain.end, 2, Fade(DARKBLUE, 0.95f));
        }
    }

    for (const PhysicsRope& rope : level.physicsRopes) {
        if (rope.points.size() >= 2) {
            for (int i = 0; i < static_cast<int>(rope.points.size()) - 1; i++) {
                DrawLineEx(rope.points[i], rope.points[i + 1], 1.5f, Fade(BROWN, 0.95f));
                DrawCircleLinesV(rope.points[i], fmaxf(1.0f, rope.thickness * 0.5f), Fade(ORANGE, 0.65f));
            }
            DrawCircleLinesV(rope.points.back(), fmaxf(1.0f, rope.thickness * 0.5f), Fade(ORANGE, 0.65f));
        }
        if (rope.pinStart) {
            DrawCircleLinesV(rope.start, rope.thickness + 2.0f, Fade(GREEN, 0.9f));
        }
        if (rope.pinEnd) {
            DrawCircleLinesV(rope.end, rope.thickness + 2.0f, Fade(GREEN, 0.9f));
        }
    }

    if (playerAlive) {
        DrawRectangleLinesEx(player.rect, 2, Fade(ORANGE, 0.95f));
    }
    if (multiplayerEnabled && player2Alive) {
        DrawRectangleLinesEx(player2.rect, 2, Fade(SKYBLUE, 0.95f));
    }
    if (threePlayerEnabled && player3Alive) {
        DrawRectangleLinesEx(player3.rect, 2, Fade(GREEN, 0.95f));
    }
}
