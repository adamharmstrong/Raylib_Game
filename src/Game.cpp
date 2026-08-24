#include "Game.h"

#include "Collision.h"
#include "Constants.h"
#include "Fluid.h"
#include "Machine.h"
#include "MathUtils.h"
#include "Render.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <iterator>
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

    const char* MusicPath(const char* desktopPath, const char* webPath) {
#if defined(GAME_WEB_BUILD)
        return webPath;
#else
        return desktopPath;
#endif
    }

    std::array<DirectionalSpikeHazard, 2> GetPlatformSideSpikes(Rectangle platform) {
        constexpr float SpikeDepth = 12.0f;
        return {
            DirectionalSpikeHazard{
                {platform.x - SpikeDepth, platform.y, SpikeDepth, platform.height},
                SpikeDirection::Left
            },
            DirectionalSpikeHazard{
                {platform.x + platform.width, platform.y, SpikeDepth, platform.height},
                SpikeDirection::Right
            }
        };
    }

    bool IsPlayerRunningIntoPlatformSpikes(
        const Player& player,
        const DirectionalSpikeHazard& hazard
    ) {
        if (hazard.direction != SpikeDirection::Left &&
            hazard.direction != SpikeDirection::Right) {
            return false;
        }

        // Ignore the player's head and feet so landing on the platform or
        // striking its underside cannot be mistaken for a side impact.
        constexpr float VerticalInset = 6.0f;
        const Rectangle bodyCore{
            player.rect.x,
            player.rect.y + VerticalInset,
            player.rect.width,
            fmaxf(1.0f, player.rect.height - VerticalInset * 2.0f)
        };
        if (!CheckCollisionRecs(bodyCore, hazard.rect)) {
            return false;
        }

        constexpr float MinimumImpactSpeed = 20.0f;
        const float playerCenterX = player.rect.x + player.rect.width * 0.5f;
        const float hazardCenterX = hazard.rect.x + hazard.rect.width * 0.5f;
        if (hazard.direction == SpikeDirection::Left) {
            return player.velocity.x > MinimumImpactSpeed &&
                playerCenterX <= hazardCenterX;
        }
        return player.velocity.x < -MinimumImpactSpeed &&
            playerCenterX >= hazardCenterX;
    }

    bool IsPlayerStandingOnMovingPlatform(const Player& player, Rectangle platform) {
        const float overlapLeft = fmaxf(player.rect.x, platform.x);
        const float overlapRight = fminf(
            player.rect.x + player.rect.width,
            platform.x + platform.width
        );
        if (overlapRight - overlapLeft < 4.0f) return false;

        const float footY = player.rect.y + player.rect.height;
        return player.velocity.y >= -1.0f &&
            footY >= platform.y - 5.0f &&
            footY <= platform.y + 7.0f;
    }

    bool IsBoulderStandingOnMovingPlatform(const Boulder& boulder, Rectangle platform) {
        const float footY = boulder.center.y + boulder.radius;
        return boulder.velocity.y >= -1.0f &&
            boulder.center.x >= platform.x - boulder.radius * 0.25f &&
            boulder.center.x <= platform.x + platform.width + boulder.radius * 0.25f &&
            footY >= platform.y - 5.0f &&
            footY <= platform.y + 7.0f;
    }

    void AdvanceButtonPlatformLoops(
        Level& level,
        float dt,
        const std::array<Player*, 4>& players
    ) {
        std::array<bool, 4> carriedPlayers{};
        std::vector<bool> carriedBoulders(level.boulders.size(), false);

        for (ButtonPlatformLoop& loop : level.buttonPlatformLoops) {
            loop.active = loop.buttonIndex < 0 ||
                (loop.buttonIndex < static_cast<int>(level.buttons.size()) &&
                    level.buttons[loop.buttonIndex].pressed);
            if (!loop.active) continue;

            const std::vector<Rectangle> previousPlatforms = loop.platforms;
            loop.phase = fmodf(loop.phase + loop.speed * dt, 360.0f);
            UpdateButtonPlatformLoopPositions(loop);

            const int platformCount = std::min(
                static_cast<int>(previousPlatforms.size()),
                static_cast<int>(loop.platforms.size())
            );
            for (int platformIndex = 0; platformIndex < platformCount; ++platformIndex) {
                const Rectangle previous = previousPlatforms[platformIndex];
                const Rectangle current = loop.platforms[platformIndex];
                const Vector2 displacement{
                    current.x - previous.x,
                    current.y - previous.y
                };

                for (int playerIndex = 0; playerIndex < static_cast<int>(players.size()); ++playerIndex) {
                    Player* activePlayer = players[playerIndex];
                    if (activePlayer == nullptr) continue;

                    if (!carriedPlayers[playerIndex] &&
                        IsPlayerStandingOnMovingPlatform(*activePlayer, previous)) {
                        activePlayer->rect.x += displacement.x;
                        activePlayer->rect.y += displacement.y;
                        activePlayer->onGround = true;
                        carriedPlayers[playerIndex] = true;
                        continue;
                    }

                    // A platform can move into an otherwise stationary player.
                    // Resolve that intrusion using the platform's movement,
                    // because the player's own zero velocity gives the regular
                    // axis solver no direction from which to separate them.
                    if (CheckCollisionRecs(activePlayer->rect, current) &&
                        !CheckCollisionRecs(activePlayer->rect, previous)) {
                        if (fabsf(displacement.x) >= fabsf(displacement.y)) {
                            activePlayer->rect.x = displacement.x >= 0.0f
                                ? current.x + current.width
                                : current.x - activePlayer->rect.width;
                        }
                        else if (displacement.y < 0.0f) {
                            activePlayer->rect.y = current.y - activePlayer->rect.height;
                            activePlayer->onGround = true;
                        }
                        else {
                            activePlayer->rect.y = current.y + current.height;
                        }
                    }
                }

                for (int boulderIndex = 0;
                     boulderIndex < static_cast<int>(level.boulders.size());
                     ++boulderIndex) {
                    Boulder& boulder = level.boulders[boulderIndex];
                    if (carriedBoulders[boulderIndex] ||
                        !IsBoulderStandingOnMovingPlatform(boulder, previous)) {
                        continue;
                    }
                    boulder.center.x += displacement.x;
                    boulder.center.y += displacement.y;
                    boulder.onGround = true;
                    carriedBoulders[boulderIndex] = true;
                }
            }
        }

        UpdatePlatformLoopButtonPositions(level);
    }

    void DrawPlatformRailTrack(const ButtonPlatformLoop& loop) {
        constexpr int SegmentCount = 72;
        const Color railEdge{39, 52, 61, 255};
        const Color railSurface{126, 157, 171, 255};
        const Color tieColor{78, 96, 104, 255};

        const auto railPointsAt = [&](int segment) {
            const float progress =
                static_cast<float>(segment) / static_cast<float>(SegmentCount);
            const Vector2 centerPoint = GetButtonPlatformLoopPoint(loop, progress);
            const Vector2 nextPoint = GetButtonPlatformLoopPoint(
                loop,
                progress + 0.001f
            );
            Vector2 tangent{
                nextPoint.x - centerPoint.x,
                nextPoint.y - centerPoint.y
            };
            const float tangentLength = fmaxf(0.001f, hypotf(tangent.x, tangent.y));
            const Vector2 normal{
                tangent.y / tangentLength,
                -tangent.x / tangentLength
            };
            return std::array<Vector2, 2>{
                Vector2{centerPoint.x + normal.x * 6.0f, centerPoint.y + normal.y * 6.0f},
                Vector2{centerPoint.x - normal.x * 6.0f, centerPoint.y - normal.y * 6.0f}
            };
        };

        // Two fixed steel rails follow a capsule-shaped route: straight sides
        // joined by semicircular end caps. The track has no moving tread.
        for (int segment = 0; segment < SegmentCount; ++segment) {
            const auto start = railPointsAt(segment);
            const auto end = railPointsAt(segment + 1);
            for (int rail = 0; rail < 2; ++rail) {
                DrawLineEx(start[rail], end[rail], 5.0f, railEdge);
                DrawLineEx(start[rail], end[rail], 2.2f, railSurface);
            }
            if (segment % 6 == 0) {
                DrawLineEx(start[0], start[1], 3.0f, tieColor);
            }
        }
    }

    void DrawElectricalWire(
        std::initializer_list<Vector2> points,
        float thickness,
        Color wireColor,
        bool active
    ) {
        if (points.size() < 2) return;

        auto previous = points.begin();
        for (auto point = std::next(previous); point != points.end(); ++point) {
            DrawLineEx(*previous, *point, thickness, wireColor);
            previous = point;
        }

        if (!active) return;

        float wireLength = 0.0f;
        previous = points.begin();
        for (auto point = std::next(previous); point != points.end(); ++point) {
            wireLength += hypotf(point->x - previous->x, point->y - previous->y);
            previous = point;
        }
        if (wireLength <= 0.0f) return;

        constexpr float DotSpacing = 30.0f;
        constexpr float DotSpeed = 72.0f;
        const float firstDotDistance =
            fmodf(static_cast<float>(GetTime()) * DotSpeed, DotSpacing);

        for (float dotDistance = firstDotDistance;
             dotDistance <= wireLength;
             dotDistance += DotSpacing) {
            float traversed = 0.0f;
            previous = points.begin();
            for (auto point = std::next(previous); point != points.end(); ++point) {
                const float segmentLength =
                    hypotf(point->x - previous->x, point->y - previous->y);
                if (dotDistance <= traversed + segmentLength && segmentLength > 0.0f) {
                    const float segmentProgress =
                        (dotDistance - traversed) / segmentLength;
                    const Vector2 position{
                        previous->x + (point->x - previous->x) * segmentProgress,
                        previous->y + (point->y - previous->y) * segmentProgress
                    };
                    // Keep energized current visually distinct from the blue
                    // insulation, regardless of which circuit is carrying it.
                    DrawCircleV(position, 4.2f, Fade(YELLOW, 0.32f));
                    DrawCircleV(position, 2.2f, YELLOW);
                    break;
                }
                traversed += segmentLength;
                previous = point;
            }
        }
    }

    void DrawPortalLiftWiring(const Level& level) {
        if (level.script != LevelScript::PortalLift || level.buttons.empty() ||
            level.buttonPlatformLoops.empty()) {
            return;
        }

        const ButtonPlatformLoop& loop = level.buttonPlatformLoops.front();
        const Vector2 trackJunction{
            loop.center.x + loop.radius.x + 8.0f,
            loop.center.y
        };
        const Button& lightButton = level.buttons.front();
        const Vector2 lightButtonCenter{
            lightButton.rect.x + lightButton.rect.width * 0.5f,
            lightButton.rect.y + lightButton.rect.height * 0.5f
        };
        const Color lightWireColor =
            lightButton.pressed ? SKYBLUE : Fade(DARKBLUE, 0.65f);
        const Rectangle chamber = level.darknessAreas.empty()
            ? Rectangle{
                loop.center.x - loop.radius.x - 24.0f,
                level.worldBounds.y,
                loop.radius.x * 2.0f + 48.0f,
                level.worldBounds.height
            }
            : level.darknessAreas.front();
        const float machineRiserX = chamber.x + chamber.width + 24.0f;
        const float lowerBusY = level.worldBounds.y + level.worldBounds.height - 40.0f;
        const float exitUnderShelfY = 690.0f;
        float buttonServiceX = 1060.0f;
        if (!level.ramps.empty()) {
            const Ramp& arrivalRamp = level.ramps.front();
            const float angle = arrivalRamp.angle * DEG2RAD;
            const float rampHalfWidth =
                fabsf(cosf(angle)) * arrivalRamp.length * 0.5f +
                fabsf(sinf(angle)) * arrivalRamp.thickness * 0.5f;
            buttonServiceX = arrivalRamp.center.x + rampHalfWidth + 18.0f;
        }
        const Vector2 lampConnection{
            loop.center.x + 15.0f,
            chamber.y + 32.0f
        };
        const Vector2 lampApproach{
            lampConnection.x + 18.0f,
            lampConnection.y + 58.0f
        };

        // Circuit one is completely independent. It leaves the landing above
        // the catch-basin floor, passes beneath the ramp, then rises from below
        // and enters the right side of the lamp's junction box.
        DrawElectricalWire(
            {
                lightButtonCenter,
                {buttonServiceX, lightButtonCenter.y},
                {buttonServiceX, 390.0f},
                {machineRiserX, 390.0f},
                {machineRiserX, lampApproach.y},
                lampApproach,
                {lampApproach.x, lampConnection.y},
                lampConnection
            },
            3.0f,
            lightWireColor,
            lightButton.pressed
        );

        const auto buttonLatched = [&](int buttonIndex) {
            return std::any_of(
                level.platformLoopButtonLinks.begin(),
                level.platformLoopButtonLinks.end(),
                [&](const PlatformLoopButtonLink& link) {
                    return link.buttonIndex == buttonIndex && link.activated;
                }
            );
        };
        for (const ButtonFanLink& link : level.buttonFanLinks) {
            if (link.fanIndex < 0 || link.fanIndex >= static_cast<int>(level.fans.size())) continue;
            const bool powered = buttonLatched(link.buttonIndex);
            const Color circuitColor = powered ? SKYBLUE : Fade(DARKBLUE, 0.65f);
            const Vector2 fanCenter = level.fans[link.fanIndex].center;

            // Circuit two begins at the rail junction, where the moving button
            // latches the fan and exit systems. It remains physically separate
            // from the lighting circuit above.
            DrawElectricalWire(
                {
                    trackJunction,
                    {machineRiserX, trackJunction.y},
                    {machineRiserX, lowerBusY},
                    {fanCenter.x, lowerBusY},
                    fanCenter
                },
                4.0f,
                circuitColor,
                powered
            );
            if (level.buttonExitLink.buttonIndex == link.buttonIndex &&
                level.exitTrigger.width > 0.0f && level.exitTrigger.height > 0.0f) {
                const Vector2 exitConnection{
                    level.exitTrigger.x,
                    level.exitTrigger.y + 35.0f
                };
                DrawElectricalWire(
                    {
                        trackJunction,
                        {machineRiserX, trackJunction.y},
                        {machineRiserX, exitUnderShelfY},
                        {exitConnection.x, exitUnderShelfY},
                        exitConnection
                    },
                    3.0f,
                    circuitColor,
                    powered
                );
            }
        }
    }

    void DrawPortalLiftChamberLamp(const Level& level, float power) {
        if (level.script != LevelScript::PortalLift || level.darknessAreas.empty()) return;

        const Rectangle chamber = level.darknessAreas.front();
        const float centerX = level.buttonPlatformLoops.empty()
            ? chamber.x + chamber.width * 0.5f
            : level.buttonPlatformLoops.front().center.x;
        const float ceilingBottom = chamber.y;
        const float lightAmount = Clamp01(power);
        if (lightAmount <= 0.02f) return;

        const float housingTop = ceilingBottom + 31.0f;
        const float shadeBottom = housingTop + 22.0f;
        const float bulbY = shadeBottom + 8.0f;
        const Color mountingSteel{55, 69, 76, 255};
        const Color steelEdge{21, 29, 34, 255};
        const Color steelHighlight{132, 158, 166, 255};
        const Color warmLight{255, 226, 116, 255};

        // A ceiling conduit, bolted junction box, and heavy pressed-steel shade
        // give the chamber light an industrial silhouette without a cage.
        DrawRectangleRec(
            {centerX - 4.0f, ceilingBottom, 8.0f, housingTop - ceilingBottom},
            mountingSteel
        );
        DrawRectangleLinesEx(
            {centerX - 4.0f, ceilingBottom, 8.0f, housingTop - ceilingBottom},
            2.0f,
            steelEdge
        );
        Rectangle junctionBox{centerX - 15.0f, housingTop - 8.0f, 30.0f, 18.0f};
        DrawRectangleRec(junctionBox, mountingSteel);
        DrawRectangleLinesEx(junctionBox, 3.0f, steelEdge);
        DrawCircleV({junctionBox.x + 5.0f, junctionBox.y + 5.0f}, 2.0f, steelHighlight);
        DrawCircleV(
            {junctionBox.x + junctionBox.width - 5.0f, junctionBox.y + 5.0f},
            2.0f,
            steelHighlight
        );

        DrawTriangle(
            {centerX - 28.0f, shadeBottom},
            {centerX + 28.0f, shadeBottom},
            {centerX, housingTop + 4.0f},
            mountingSteel
        );
        DrawLineEx(
            {centerX - 28.0f, shadeBottom},
            {centerX + 28.0f, shadeBottom},
            5.0f,
            steelEdge
        );
        DrawLineEx(
            {centerX - 19.0f, shadeBottom - 4.0f},
            {centerX + 19.0f, shadeBottom - 4.0f},
            2.0f,
            steelHighlight
        );

        DrawCircleV({centerX, bulbY}, 10.0f, warmLight);
        DrawCircleV(
            {centerX, bulbY},
            19.0f,
            Fade(warmLight, 0.32f * lightAmount)
        );

        DrawTriangle(
            {centerX - 88.0f, chamber.y + chamber.height},
            {centerX + 88.0f, chamber.y + chamber.height},
            {centerX, bulbY + 10.0f},
            Fade(warmLight, lightAmount * 0.12f)
        );
    }

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
    constexpr int kCharacterCount = 4;
    constexpr const char* kCharacterNames[kCharacterCount] = {
        "Character 1", "Character 2", "Character 3", "Character 4"
    };
    constexpr Color kPlayerSelectColors[4] = {
        ORANGE, SKYBLUE, LIME, VIOLET
    };

    bool gPixelPerfectScaling = true;
    float gUiScale = 1.0f;

    struct SettingsMenuLayout {
        Rectangle panel;
        std::array<Rectangle, 4> tabs;
        std::array<Rectangle, 4> playerTabs;
        std::array<Rectangle, 2> inputTabs;
        std::array<Rectangle, kSettingsControlCount> controls;
        std::array<Rectangle, kSettingsControlCount> controlRows;
        Rectangle applyButton;
        Rectangle closeButton;
    };

    struct DropdownLayout {
        Rectangle panel;
        std::vector<Rectangle> options;
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
            layout.playerTabs[i] = {
                panelX + panelPadding + i * (tabWidth + tabGap),
                panelY + 110.0f,
                tabWidth,
                30.0f
            };
        }
        for (int i = 0; i < static_cast<int>(layout.inputTabs.size()); ++i) {
            layout.inputTabs[i] = {
                panelX + panelPadding + i * (controlWidth + controlGap),
                panelY + 146.0f,
                controlWidth,
                30.0f
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
            layout.controlRows[i] = {
                panelX + panelPadding + column * (controlWidth + controlGap),
                panelY + 184.0f + row * (controlHeight + rowGap),
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
        optionCount = std::max(0, optionCount);
        columns = std::max(1, columns);
        const int rows = (optionCount + columns - 1) / columns;
        const float optionWidth = (anchor.width - padding * 2.0f - gap * (columns - 1)) / columns;

        DropdownLayout layout{};
        layout.options.resize(optionCount);
        layout.panel = {
            anchor.x,
            anchor.y + anchor.height + 4.0f,
            anchor.width,
            padding * 2.0f + rows * optionHeight + std::max(0, rows - 1) * gap
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

    std::string GetInteractPrompt(
        bool player1Near,
        bool player2Near,
        bool player3Near,
        bool player4Near,
        const char* action
    ) {
        std::string keys;
        if (player1Near) keys = "E";
        if (player2Near) keys += keys.empty() ? "U" : " / U";
        if (player3Near) keys += keys.empty() ? "Right Ctrl" : " / Right Ctrl";
        if (player4Near) keys += keys.empty() ? "Numpad 0" : " / Numpad 0";
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

    std::string KeyLabel(KeyboardKey key) {
        if (key >= KEY_A && key <= KEY_Z) {
            return std::string(1, static_cast<char>('A' + key - KEY_A));
        }
        if (key >= KEY_ZERO && key <= KEY_NINE) {
            return std::string(1, static_cast<char>('0' + key - KEY_ZERO));
        }
        if (key >= KEY_F1 && key <= KEY_F12) {
            return "F" + std::to_string(key - KEY_F1 + 1);
        }
        if (key >= KEY_KP_0 && key <= KEY_KP_9) {
            return "Numpad " + std::to_string(key - KEY_KP_0);
        }

        switch (key) {
        case KEY_NULL: return "Unbound";
        case KEY_SPACE: return "Space";
        case KEY_ESCAPE: return "Escape";
        case KEY_ENTER: return "Enter";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "Backspace";
        case KEY_INSERT: return "Insert";
        case KEY_DELETE: return "Delete";
        case KEY_RIGHT: return "Right";
        case KEY_LEFT: return "Left";
        case KEY_DOWN: return "Down";
        case KEY_UP: return "Up";
        case KEY_PAGE_UP: return "Page Up";
        case KEY_PAGE_DOWN: return "Page Down";
        case KEY_HOME: return "Home";
        case KEY_END: return "End";
        case KEY_CAPS_LOCK: return "Caps Lock";
        case KEY_SCROLL_LOCK: return "Scroll Lock";
        case KEY_NUM_LOCK: return "Num Lock";
        case KEY_PRINT_SCREEN: return "Print Screen";
        case KEY_PAUSE: return "Pause";
        case KEY_LEFT_SHIFT: return "Left Shift";
        case KEY_LEFT_CONTROL: return "Left Ctrl";
        case KEY_LEFT_ALT: return "Left Alt";
        case KEY_LEFT_SUPER: return "Left Super";
        case KEY_RIGHT_SHIFT: return "Right Shift";
        case KEY_RIGHT_CONTROL: return "Right Ctrl";
        case KEY_RIGHT_ALT: return "Right Alt";
        case KEY_RIGHT_SUPER: return "Right Super";
        case KEY_MENU: return "Menu";
        case KEY_APOSTROPHE: return "Apostrophe";
        case KEY_COMMA: return "Comma";
        case KEY_MINUS: return "Minus";
        case KEY_PERIOD: return "Period";
        case KEY_SLASH: return "Slash";
        case KEY_SEMICOLON: return "Semicolon";
        case KEY_EQUAL: return "Equals";
        case KEY_LEFT_BRACKET: return "Left Bracket";
        case KEY_BACKSLASH: return "Backslash";
        case KEY_RIGHT_BRACKET: return "Right Bracket";
        case KEY_GRAVE: return "Grave";
        case KEY_KP_DECIMAL: return "Numpad Decimal";
        case KEY_KP_DIVIDE: return "Numpad Divide";
        case KEY_KP_MULTIPLY: return "Numpad Multiply";
        case KEY_KP_SUBTRACT: return "Numpad Subtract";
        case KEY_KP_ADD: return "Numpad Add";
        case KEY_KP_ENTER: return "Numpad Enter";
        case KEY_KP_EQUAL: return "Numpad Equals";
        default: return "Key " + std::to_string(static_cast<int>(key));
        }
    }

    const char* GamepadButtonLabel(GamepadButton button) {
        switch (button) {
        case GAMEPAD_BUTTON_RIGHT_FACE_UP: return "Y / Triangle";
        case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "B / Circle";
        case GAMEPAD_BUTTON_RIGHT_FACE_DOWN: return "A / Cross";
        case GAMEPAD_BUTTON_RIGHT_FACE_LEFT: return "X / Square";
        case GAMEPAD_BUTTON_LEFT_TRIGGER_1: return "Left Bumper";
        case GAMEPAD_BUTTON_LEFT_TRIGGER_2: return "Left Trigger";
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_1: return "Right Bumper";
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_2: return "Right Trigger";
        case GAMEPAD_BUTTON_MIDDLE_LEFT: return "Back / Select";
        case GAMEPAD_BUTTON_MIDDLE: return "Guide";
        case GAMEPAD_BUTTON_MIDDLE_RIGHT: return "Start";
        case GAMEPAD_BUTTON_LEFT_THUMB: return "Left Stick";
        case GAMEPAD_BUTTON_RIGHT_THUMB: return "Right Stick";
        default: return "Unbound";
        }
    }

    std::string ControllerDeviceLabel(int gamepad) {
        if (gamepad < 0) return "Not Assigned";
        return "Controller " + std::to_string(gamepad + 1);
    }

    int AvailableGamepad(const PlayerControllerSettings& settings) {
        return settings.gamepad >= 0 && settings.gamepad < 4 && IsGamepadAvailable(settings.gamepad)
            ? settings.gamepad
            : -1;
    }

    constexpr std::array<GamepadButton, 11> kBindableGamepadButtons{{
        GAMEPAD_BUTTON_RIGHT_FACE_UP,
        GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
        GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
        GAMEPAD_BUTTON_LEFT_TRIGGER_1,
        GAMEPAD_BUTTON_LEFT_TRIGGER_2,
        GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
        GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
        GAMEPAD_BUTTON_MIDDLE_LEFT,
        GAMEPAD_BUTTON_LEFT_THUMB,
        GAMEPAD_BUTTON_RIGHT_THUMB
    }};

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

    struct HoveredObject {
        const char* name{nullptr};
        const char* description{nullptr};
        float score{INFINITY};
    };

    Rectangle ExpandedHoverBounds(Rectangle bounds) {
        constexpr float minimumSize = 20.0f;
        if (bounds.width < minimumSize) {
            bounds.x -= (minimumSize - bounds.width) * 0.5f;
            bounds.width = minimumSize;
        }
        if (bounds.height < minimumSize) {
            bounds.y -= (minimumSize - bounds.height) * 0.5f;
            bounds.height = minimumSize;
        }
        return bounds;
    }

    Rectangle RotatedObjectBounds(Vector2 center, float length, float thickness, float angleDegrees) {
        float angle = angleDegrees * DEG2RAD;
        float cosine = fabsf(cosf(angle));
        float sine = fabsf(sinf(angle));
        float width = cosine * length + sine * thickness;
        float height = sine * length + cosine * thickness;
        return {center.x - width * 0.5f, center.y - height * 0.5f, width, height};
    }

    float PointSegmentDistanceSquared(Vector2 point, Vector2 start, Vector2 end) {
        Vector2 segment{end.x - start.x, end.y - start.y};
        float lengthSquared = segment.x * segment.x + segment.y * segment.y;
        if (lengthSquared <= 0.0001f) {
            float dx = point.x - start.x;
            float dy = point.y - start.y;
            return dx * dx + dy * dy;
        }
        float amount = ((point.x - start.x) * segment.x + (point.y - start.y) * segment.y) / lengthSquared;
        amount = std::clamp(amount, 0.0f, 1.0f);
        float dx = point.x - (start.x + segment.x * amount);
        float dy = point.y - (start.y + segment.y * amount);
        return dx * dx + dy * dy;
    }

    bool IsPointNearPolyline(Vector2 point, const std::vector<Vector2>& points, float radius) {
        if (points.empty()) return false;
        if (points.size() == 1) return PointSegmentDistanceSquared(point, points.front(), points.front()) <= radius * radius;
        for (size_t i = 1; i < points.size(); ++i) {
            if (PointSegmentDistanceSquared(point, points[i - 1], points[i]) <= radius * radius) return true;
        }
        return false;
    }

    void DrawHoveredObjectTooltip(const Level& currentLevel, Camera2D camera) {
        Vector2 mouseScreen = GetUiMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
        HoveredObject hovered{};

        auto consider = [&](Rectangle rawBounds, const char* name, const char* description, float scoreScale = 1.0f) {
            Rectangle bounds = ExpandedHoverBounds(rawBounds);
            if (!CheckCollisionPointRec(mouseWorld, bounds)) return;
            float score = fmaxf(1.0f, bounds.width * bounds.height) * scoreScale;
            if (score < hovered.score) hovered = {name, description, score};
        };
        auto considerCircle = [&](Vector2 center, float radius, const char* name, const char* description) {
            float hitRadius = fmaxf(radius, 10.0f);
            float dx = mouseWorld.x - center.x;
            float dy = mouseWorld.y - center.y;
            if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                float score = PI * hitRadius * hitRadius;
                if (score < hovered.score) hovered = {name, description, score};
            }
        };

        for (const GuideObject& object : currentLevel.guideObjects) {
            if (!object.active || object.collected) continue;
            Rectangle bounds = GetGuideObjectBounds(object);
            if (object.type == GuideObjectType::RocketThruster || object.type == GuideObjectType::SteamVent) {
                Vector2 visualCenter{
                    object.transform.position.x - object.direction.x * 20.0f,
                    object.transform.position.y - object.direction.y * 20.0f
                };
                bounds = RotatedObjectBounds(visualCenter, 64.0f, fmaxf(28.0f, object.width),
                    atan2f(object.direction.y, object.direction.x) * RAD2DEG);
            }
            consider(bounds, GetGuideObjectName(object.type), GetGuideObjectDescription(object.type));
        }

        for (const StoneBlock& object : currentLevel.stoneBlocks) consider(object.rect, "Stone Block", "A heavy movable block used to hold buttons down or counterbalance mechanisms.");
        for (const Boulder& object : currentLevel.boulders) considerCircle(object.center, object.radius, "Boulder", "A heavy round stone that rolls down slopes and transfers strong momentum.");
        for (const PhysicsWheel& object : currentLevel.physicsWheels) considerCircle(object.center, object.radius, "Physics Wheel", "A freely moving wheel that rolls, spins, and collides with other physics objects.");
        for (const Gear& object : currentLevel.gears) {
            const bool mounted = object.mounting == GearMounting::Mounted;
            const char* description = object.clockHand != ClockHandType::None
                ? "A motorized physics gear constrained to the wall. Its brake also stops the corresponding clock hand."
                : (mounted
                    ? "A rotational physics gear constrained to a fixed axle. It transfers torque through touching teeth."
                    : "A free physics gear that falls, rolls, collides, meshes with other gears, and can engage a screw.");
            considerCircle(object.center, object.radius * GearOuterRadiusScale,
                mounted ? "Mounted Physics Gear" : "Physics Gear", description);
        }
        for (const Flywheel& object : currentLevel.flywheels) considerCircle(object.center, object.radius, "Flywheel", "A heavy rotating wheel that stores rotational energy and smooths changes in speed.");
        for (const SteeringWheel& object : currentLevel.steeringWheels) considerCircle(object.center, object.radius, "Steering Wheel", "A hand wheel used to turn valves and other player-operated mechanisms.");
        for (const Screw& object : currentLevel.screws) consider(RotatedObjectBounds(object.center, object.length, object.radius * 2.4f, object.angle), "Screw", "A rotating helical shaft that converts rotation into linear force and can engage a gear.");
        for (const Fan& object : currentLevel.fans) considerCircle(object.center, 28.0f, "Fan", "A powered fan that creates directional wind and pushes exposed objects.");
        for (const Pinwheel& object : currentLevel.pinwheels) considerCircle(object.center, object.radius, "Pinwheel", "A lightweight rotor that spins faster when stronger wind passes over it.");
        for (const Ramp& object : currentLevel.ramps) consider(RotatedObjectBounds(object.center, object.length, object.thickness, object.angle), "Ramp", "A fixed inclined plane that lets objects gain or lose height gradually.");
        for (const SeeSaw& object : currentLevel.seeSaws) consider(RotatedObjectBounds(object.pivot, object.length, fmaxf(object.thickness, 36.0f), object.angle), "See-Saw", "A lever balanced on a central fulcrum; weight on one side raises the other.");
        for (const TrapDoor& object : currentLevel.trapDoors) {
            float angle = object.angle * DEG2RAD;
            Vector2 center{object.hinge.x + cosf(angle) * object.length * 0.5f, object.hinge.y + sinf(angle) * object.length * 0.5f};
            consider(RotatedObjectBounds(center, object.length + 18.0f, fmaxf(object.thickness, 28.0f), object.angle), "Trap Door", "A hinged floor door with attachment rings that ropes and chains can pull open.");
        }
        for (const Button& object : currentLevel.buttons) consider(object.rect, "Button", "A pressure switch activated by a player or physics object resting on it.");
        for (const ArrowTrap& object : currentLevel.arrowTraps) consider({object.position.x - 19.0f, object.position.y - 19.0f, 38.0f, 38.0f}, "Arrow Trap", "A timed hazard that repeatedly fires arrows in its facing direction.");
        for (const BreakableTile& object : currentLevel.breakableTiles) {
            if (!object.broken) consider(object.rect, "Breakable Tile", "Looks like ordinary flooring, then cracks and collapses after being stepped on.");
        }

        for (const Chain& object : currentLevel.chains) {
            const std::vector<Vector2>& points = object.points.empty() ? std::vector<Vector2>{object.start, object.end} : object.points;
            float radius = fmaxf(10.0f, object.collisionRadius * object.scale + 3.0f);
            if (IsPointNearPolyline(mouseWorld, points, radius) && radius * radius < hovered.score) {
                hovered = {"Chain", "A heavy flexible linkage whose ends can be carried and attached to compatible anchor points.", radius * radius};
            }
        }
        for (const PhysicsRope& object : currentLevel.physicsRopes) {
            const std::vector<Vector2>& points = object.points.empty() ? std::vector<Vector2>{object.start, object.end} : object.points;
            float radius = fmaxf(9.0f, object.thickness + 5.0f);
            if (IsPointNearPolyline(mouseWorld, points, radius) && radius * radius < hovered.score) {
                hovered = {"Physics Rope", "A flexible rope that bends, swings, collides, and can attach to compatible anchor points.", radius * radius};
            }
        }

        for (Vector2 pulley : currentLevel.pulleys) considerCircle(pulley, 48.0f, "Pulley", "A grooved wheel that redirects rope tension and changes the direction of a pulling force.");
        for (const HangingWeight& object : currentLevel.weights) consider(object.rect, "Hanging Weight", "A suspended mass that provides a downward force and can act as a counterweight.");
        for (const RotaryLatch& object : currentLevel.rotaryLatches) considerCircle(object.center, object.radius, "Rotary Latch", "A turnable lock that secures its mechanism when aligned to the target angle.");
        considerCircle(currentLevel.valve.center, currentLevel.valve.radius, "Valve", "A hand-operated control that regulates the connected fluid or gas flow.");
        if (currentLevel.exitTrigger.width > 0.0f && currentLevel.exitTrigger.height > 0.0f) {
            consider(currentLevel.exitTrigger, "Exit Door", "The level exit. It opens after the required mechanism or objective is completed.", 2.0f);
        }

        if (hovered.name == nullptr) return;

        constexpr float panelWidth = 430.0f;
        constexpr float panelHeight = 112.0f;
        float panelX = mouseScreen.x + 20.0f;
        float panelY = mouseScreen.y + 20.0f;
        if (panelX + panelWidth > Constants::ScreenWidth - 10.0f) panelX = mouseScreen.x - panelWidth - 20.0f;
        if (panelY + panelHeight > Constants::ScreenHeight - 10.0f) panelY = mouseScreen.y - panelHeight - 20.0f;
        panelX = std::clamp(panelX, 10.0f, Constants::ScreenWidth - panelWidth - 10.0f);
        panelY = std::clamp(panelY, 10.0f, Constants::ScreenHeight - panelHeight - 10.0f);
        Rectangle panel{panelX, panelY, panelWidth, panelHeight};
        DrawRectangleRounded(panel, 0.07f, 4, Color{23, 29, 34, 244});
        DrawRectangleRoundedLinesEx(panel, 0.07f, 4, 2.0f, Color{218, 145, 42, 255});
        DrawRectangle(static_cast<int>(panel.x + 13.0f), static_cast<int>(panel.y + 15.0f), 4, 27, Color{218, 145, 42, 255});
        DrawText(hovered.name, static_cast<int>(panel.x + 27.0f), static_cast<int>(panel.y + 14.0f), 22, RAYWHITE);
        DrawWrappedText(hovered.description, {panel.x + 16.0f, panel.y + 53.0f, panel.width - 32.0f, 48.0f}, 17, 4, Color{202, 210, 214, 255});
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

    constexpr float ClockMidnightAngle = 270.0f;
    constexpr float ClockHandTolerance = 8.0f;
    constexpr float ClockGearInteractionReach = 32.0f;

    float ClockAngleDifference(float a, float b) {
        float difference = fmodf(a - b + 180.0f, 360.0f);
        if (difference < 0.0f) difference += 360.0f;
        return difference - 180.0f;
    }

    bool IsClockGearAligned(const Gear& gear) {
        return fabsf(ClockAngleDifference(gear.rotation, ClockMidnightAngle)) <= ClockHandTolerance;
    }

    bool IsClockGearLocked(const Gear& gear) {
        return gear.clockHand != ClockHandType::None && gear.stopped && IsClockGearAligned(gear);
    }

    bool IsPlayerInsideClockGearProxy(const Gear& gear, const Player& player) {
        // Interaction proxies intentionally bridge visual layers. They never become
        // collision geometry, so a middleground player can operate a background brake.
        return CheckCollisionCircleRec(gear.center, gear.radius + ClockGearInteractionReach, player.rect);
    }

    const Gear* FindClockHandGear(const Level& level, ClockHandType hand) {
        for (const Gear& gear : level.gears) {
            if (gear.clockHand == hand) return &gear;
        }
        return nullptr;
    }

    int CountClockHandGears(const Level& level) {
        return static_cast<int>(std::count_if(level.gears.begin(), level.gears.end(), [](const Gear& gear) {
            return gear.clockHand != ClockHandType::None;
        }));
    }

    int CountLockedClockHands(const Level& level) {
        return static_cast<int>(std::count_if(level.gears.begin(), level.gears.end(), IsClockGearLocked));
    }

    const char* ClockHandName(ClockHandType hand) {
        switch (hand) {
        case ClockHandType::Hour: return "HOUR";
        case ClockHandType::Minute: return "MINUTE";
        case ClockHandType::Second: return "SECOND";
        case ClockHandType::None: break;
        }
        return "CLOCK";
    }

    void DrawClocktowerMovementFrame(const Level& level) {
        if (level.clockFaceRadius <= 0.0f) return;

        const Vector2 center = level.clockFaceCenter;
        const Color bridgeDark{38, 30, 24, 255};
        const Color bridgeBase{112, 78, 42, 255};
        const Color bridgeLight{181, 132, 62, 255};
        const Color steel{117, 126, 126, 255};

        // These planted arbor lines mirror the going, motion-work, and strike
        // trains declared in clocktower_core.level. The gears cover most of each
        // bridge, leaving the connected brass skeleton seen in real movements.
        if (level.gears.size() < 25) return;
        const std::array<size_t, 6> goingTrain{{0, 2, 4, 6, 7, 8}};
        const std::array<size_t, 7> strikeTrain{{15, 17, 19, 20, 22, 23, 24}};
        const std::array<size_t, 4> motionWork{{11, 9, 13, 14}};

        const auto drawBridge = [&](const auto& arbors) {
            for (size_t index = 1; index < arbors.size(); ++index) {
                const Vector2 previous = level.gears[arbors[index - 1]].center;
                const Vector2 current = level.gears[arbors[index]].center;
                DrawLineEx(previous, current, 22.0f, Fade(BLACK, 0.38f));
                DrawLineEx(previous, current, 16.0f, bridgeDark);
                DrawLineEx(previous, current, 11.0f, bridgeBase);
                DrawLineEx(previous, current, 2.0f, Fade(bridgeLight, 0.72f));
            }
            for (size_t gearIndex : arbors) {
                const Vector2 arbor = level.gears[gearIndex].center;
                DrawCircleV({arbor.x + 3.0f, arbor.y + 4.0f}, 10.0f, Fade(BLACK, 0.42f));
                DrawCircleV(arbor, 9.0f, bridgeDark);
                DrawCircleV(arbor, 6.0f, steel);
                DrawLineEx({arbor.x - 3.0f, arbor.y + 2.0f},
                    {arbor.x + 3.0f, arbor.y - 2.0f}, 1.5f, bridgeDark);
            }
        };

        DrawCircleV(center, level.clockFaceRadius + 31.0f, Fade(BLACK, 0.26f));
        DrawRing(center, level.clockFaceRadius + 20.0f, level.clockFaceRadius + 27.0f,
            0.0f, 360.0f, 96, bridgeBase);
        drawBridge(goingTrain);
        drawBridge(strikeTrain);
        drawBridge(motionWork);
    }

    void DrawClocktowerFace(const Level& level) {
        if (level.clockFaceRadius <= 0.0f) return;

        const Vector2 center = level.clockFaceCenter;
        const float radius = level.clockFaceRadius;
        const Color brass{194, 145, 55, 255};
        const Color face{224, 214, 181, 255};
        const Color ink{29, 35, 37, 255};
        // An open center keeps the movement and its motion work visible while
        // retaining one readable clock face as an opaque chapter ring.
        DrawRing(center, radius + 9.0f, radius + 14.0f, 0.0f, 360.0f, 96, BLACK);
        DrawRing(center, radius, radius + 9.0f, 0.0f, 360.0f, 96, brass);
        DrawCircleV(center, radius - 34.0f, Color{18, 25, 28, 112});
        DrawRing(center, radius - 34.0f, radius, 0.0f, 360.0f, 96, face);
        DrawRing(center, radius - 11.0f, radius - 5.0f, 0.0f, 360.0f, 96, ink);

        for (int mark = 0; mark < 60; ++mark) {
            const float angle = (static_cast<float>(mark) * 6.0f - 90.0f) * DEG2RAD;
            const float innerRadius = radius - (mark % 5 == 0 ? 27.0f : 18.0f);
            Vector2 inner{center.x + cosf(angle) * innerRadius, center.y + sinf(angle) * innerRadius};
            Vector2 outer{center.x + cosf(angle) * (radius - 8.0f), center.y + sinf(angle) * (radius - 8.0f)};
            DrawLineEx(inner, outer, mark % 5 == 0 ? 5.0f : 2.0f, ink);
        }

        DrawText("XII", static_cast<int>(center.x - 22.0f), static_cast<int>(center.y - radius + 37.0f), 30, ink);
        DrawText("III", static_cast<int>(center.x + radius - 62.0f), static_cast<int>(center.y - 15.0f), 30, ink);
        DrawText("VI", static_cast<int>(center.x - 15.0f), static_cast<int>(center.y + radius - 69.0f), 30, ink);
        DrawText("IX", static_cast<int>(center.x - radius + 36.0f), static_cast<int>(center.y - 15.0f), 30, ink);

        const auto drawHand = [&](ClockHandType hand, float length, float thickness, Color color) {
            const Gear* gear = FindClockHandGear(level, hand);
            const float angleDegrees = gear != nullptr ? gear->rotation : ClockMidnightAngle;
            const float angle = angleDegrees * DEG2RAD;
            Vector2 end{center.x + cosf(angle) * length, center.y + sinf(angle) * length};
            Vector2 tail{center.x - cosf(angle) * 22.0f, center.y - sinf(angle) * 22.0f};
            DrawLineEx(tail, end, thickness + 3.0f, BLACK);
            DrawLineEx(tail, end, thickness, color);
        };
        drawHand(ClockHandType::Hour, radius * 0.48f, 11.0f, Color{54, 63, 66, 255});
        drawHand(ClockHandType::Minute, radius * 0.69f, 7.0f, Color{43, 52, 55, 255});
        drawHand(ClockHandType::Second, radius * 0.82f, 3.0f, Color{174, 47, 39, 255});
        DrawCircleV(center, 13.0f, BLACK);
        DrawCircleV(center, 8.0f, brass);
    }

    Rectangle GetRevealedLadderRect(const ButtonLadderLink& link) {
        const float progress = Clamp01(link.revealProgress);
        const float revealedHeight = link.ladder.height * progress;
        return {
            link.ladder.x,
            link.ladder.y + link.ladder.height - revealedHeight,
            link.ladder.width,
            revealedHeight
        };
    }

    bool IsOnAnyLadder(const Level& level, Rectangle rect) {
        if (std::any_of(level.ladders.begin(), level.ladders.end(), [rect](Rectangle ladder) {
            return CheckCollisionRecs(rect, ladder);
        })) {
            return true;
        }
        return std::any_of(level.buttonLadderLinks.begin(), level.buttonLadderLinks.end(),
            [rect](const ButtonLadderLink& link) {
                return link.activated && CheckCollisionRecs(rect, GetRevealedLadderRect(link));
            });
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

    bool IsPlayerHeadSubmerged(const Player& player, const Level& level) {
        const float headY = player.rect.y + fminf(8.0f, player.rect.height * 0.20f);
        const Vector2 headSamples[]{
            {player.rect.x + player.rect.width * 0.25f, headY},
            {player.rect.x + player.rect.width * 0.50f, headY},
            {player.rect.x + player.rect.width * 0.75f, headY}
        };

        if (HasWaterPit(level)) {
            const Rectangle waterRect = GetFilledWaterRect(level.waterPit);
            for (Vector2 point : headSamples) {
                if (waterRect.height > 0.0f && CheckCollisionPointRec(point, waterRect)) {
                    return true;
                }
            }
        }

        int submergedSamples = 0;
        for (Vector2 point : headSamples) {
            if (SampleFluid(level.fluids, FluidType::Water, point).density >= 0.14f) {
                submergedSamples++;
            }
        }
        return submergedSamples >= 2;
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
        return level.script == LevelScript::FloodedFoundry &&
            (HasValveFluidFill(level) || HasWaterPit(level));
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
        std::vector<std::pair<float, float>> coveredIntervals;
        for (const Rectangle& other : solids) {
            if (fabsf((other.y + other.height) - rect.y) > Epsilon) continue;
            float start = fmaxf(rect.x, other.x);
            float end = fminf(rect.x + rect.width, other.x + other.width);
            if (end > start + Epsilon) coveredIntervals.emplace_back(start, end);
        }

        if (coveredIntervals.empty()) return false;
        std::sort(coveredIntervals.begin(), coveredIntervals.end());
        float coveredWidth = 0.0f;
        float start = coveredIntervals.front().first;
        float end = coveredIntervals.front().second;
        for (size_t index = 1; index < coveredIntervals.size(); index++) {
            if (coveredIntervals[index].first <= end + Epsilon) {
                end = fmaxf(end, coveredIntervals[index].second);
            }
            else {
                coveredWidth += end - start;
                start = coveredIntervals[index].first;
                end = coveredIntervals[index].second;
            }
        }
        coveredWidth += end - start;

        // A small machine or barricade resting on a floor should not turn the
        // entire floor span into an untextured interior fill.
        return coveredWidth >= rect.width * 0.80f;
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
        float radius = gear.radius * GearOuterRadiusScale;
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

    void ApplyGearMotorAndBrake(Gear& gear, float dt) {
        if (gear.stopped) {
            gear.angularVelocity = 0.0f;
            return;
        }
        if (fabsf(gear.driveSpeed) <= 0.001f) {
            return;
        }

        const float motorAcceleration = 900.0f / sqrtf(fmaxf(0.1f, gear.mass));
        gear.angularVelocity = ApproachFloat(gear.angularVelocity, gear.driveSpeed,
            motorAcceleration * dt);
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
            ResolveRoundBodyWithRect(gear, solid, gear.radius * GearOuterRadiusScale);
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

    bool ResolvePlayerRampStanding(
        Player& player,
        float previousFootX,
        float previousFootY,
        const std::vector<Ramp>& ramps,
        float dt
    ) {
        if (player.velocity.y < 0.0f) return false;

        // ResolveVertical runs immediately before this function. If it found a
        // rectangular floor, keep that result instead of allowing an
        // overlapping ramp to pull the player to a second, different surface
        // in the same frame. Level 4 joins its slide directly to masonry at
        // both ends, so deterministic floor precedence is especially
        // important at those seams.
        if (player.onGround) return false;

        const float footX = RectCenterX(player.rect);
        const float footY = player.rect.y + player.rect.height;
        if (!std::isfinite(previousFootX) || !std::isfinite(previousFootY) ||
            !std::isfinite(footX) || !std::isfinite(footY)) {
            return false;
        }

        for (const Ramp& ramp : ramps) {
            const float angle = ramp.angle * DEG2RAD;
            const float halfLength = ramp.length * 0.5f;
            const float halfThickness = ramp.thickness * 0.5f;
            const float axisX = cosf(angle);
            const float normalX = -sinf(angle);
            const float firstX =
                ramp.center.x - axisX * halfLength - normalX * halfThickness;
            const float secondX =
                ramp.center.x + axisX * halfLength - normalX * halfThickness;
            constexpr float CornerInset = 2.0f;
            const float minimumX = fminf(firstX, secondX) + CornerInset;
            const float maximumX = fmaxf(firstX, secondX) - CornerInset;
            if (footX < minimumX || footX > maximumX) continue;

            const float surfaceY = GetRampSurfaceY(ramp, footX);
            const float previousSurfaceY = GetRampSurfaceY(ramp, previousFootX);
            if (!std::isfinite(surfaceY) || !std::isfinite(previousSurfaceY)) continue;

            const float surfaceTravel = fabsf(surfaceY - previousSurfaceY);
            const float downwardTravel =
                fmaxf(0.0f, player.velocity.y * dt);
            const float correction = surfaceY - footY;
            const float correctionBudget = std::clamp(
                downwardTravel + surfaceTravel + 3.0f,
                5.0f,
                32.0f
            );

            // Compare each foot position with the surface beneath that same X.
            // Using the new surface for both frames turns normal slope travel
            // into apparent penetration and can produce violent endpoint snaps.
            // GetRampSurfaceY clamps to the nearest endpoint, so this also
            // handles stepping onto the ramp cleanly from either platform.
            const bool wasAboveSurface = previousFootY <= previousSurfaceY + 4.0f;
            const bool reachedSurface = footY >= surfaceY - correctionBudget;
            const bool correctionIsLocal = fabsf(correction) <= correctionBudget;
            if (wasAboveSurface && reachedSurface && correctionIsLocal) {
                player.rect.y = surfaceY - player.rect.height;
                player.velocity.y = 0.0f;
                player.onGround = true;
                return true;
            }
        }

        return false;
    }

    bool TryGetStandingRampAngle(Rectangle rect, const std::vector<Ramp>& ramps, float& angle) {
        const float footX = RectCenterX(rect);
        const float footY = rect.y + rect.height;
        for (const Ramp& ramp : ramps) {
            if (!IsPointOverRamp(ramp, footX)) continue;
            const float surfaceY = GetRampSurfaceY(ramp, footX);
            if (footY >= surfaceY - 5.0f && footY <= surfaceY + 8.0f) {
                angle = ramp.angle;
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

    bool ResolveBoulderRampStanding(
        Boulder& boulder,
        const std::vector<Ramp>& ramps,
        float dt,
        float previousFootY
    ) {
        if (boulder.velocity.y < 0.0f) {
            return false;
        }

        const float footX = boulder.center.x;
        const float footY = boulder.center.y + boulder.radius;
        for (const Ramp& ramp : ramps) {
            const float angle = ramp.angle * DEG2RAD;
            const float halfLength = ramp.length * 0.5f;
            const float halfThickness = ramp.thickness * 0.5f;
            const float axisX = cosf(angle);
            const float normalX = -sinf(angle);
            const float firstX =
                ramp.center.x - axisX * halfLength - normalX * halfThickness;
            const float secondX =
                ramp.center.x + axisX * halfLength - normalX * halfThickness;
            constexpr float CornerInset = 2.0f;
            if (footX < fminf(firstX, secondX) + CornerInset ||
                footX > fmaxf(firstX, secondX) - CornerInset) {
                continue;
            }

            const float surfaceY = GetRampSurfaceY(ramp, footX);
            const float slopeTravel =
                fabsf(boulder.velocity.x * dt * tanf(angle));
            const float downwardTravel =
                fmaxf(0.0f, boulder.velocity.y * dt);
            const float contactTolerance = std::clamp(
                downwardTravel + slopeTravel + 4.0f,
                6.0f,
                40.0f
            );
            if (previousFootY <= surfaceY + 4.0f &&
                footY >= surfaceY - contactTolerance &&
                footY <= surfaceY + contactTolerance) {
                boulder.center.y = surfaceY - boulder.radius;
                boulder.velocity.y = 0.0f;
                boulder.onGround = true;
                // The impulse must clear the boulder's low-speed resting cutoff
                // on the first frame or it will be reset to zero forever.
                AccelerateDownSlope(boulder.velocity, ramp.angle, dt, 0.82f);
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

    void ApplyScrewConveyor(Rectangle rect, Vector2& velocity, float mass, WorldLayer layer,
                            const std::vector<Screw>& screws, float dt) {
        float inverseMass = 1.0f / fmaxf(1.0f, mass);
        for (const Screw& screw : screws) {
            if (layer != screw.layer || !IsRectTouchingScrew(rect, screw)) {
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
            if (boulder.layer != screw.layer || !IsCircleTouchingScrew(boulder.center, radius, screw)) {
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
            if (body.layer != screw.layer || !IsCircleTouchingScrew(body.center, radius, screw)) {
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

        constexpr float OutletOffset = 18.0f;
        const Vector2 windOrigin{
            fan.center.x + fan.direction.x * OutletOffset,
            fan.center.y + fan.direction.y * OutletOffset
        };
        Vector2 offset{point.x - windOrigin.x, point.y - windOrigin.y};
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

    Rectangle GetFlexibleBounds(
        const std::vector<Vector2>& points,
        Vector2 fallbackStart,
        Vector2 fallbackEnd,
        float padding
    ) {
        float minX = fminf(fallbackStart.x, fallbackEnd.x);
        float maxX = fmaxf(fallbackStart.x, fallbackEnd.x);
        float minY = fminf(fallbackStart.y, fallbackEnd.y);
        float maxY = fmaxf(fallbackStart.y, fallbackEnd.y);
        for (Vector2 point : points) {
            minX = fminf(minX, point.x);
            maxX = fmaxf(maxX, point.x);
            minY = fminf(minY, point.y);
            maxY = fmaxf(maxY, point.y);
        }
        return {
            minX - padding,
            minY - padding,
            fmaxf(1.0f, maxX - minX + padding * 2.0f),
            fmaxf(1.0f, maxY - minY + padding * 2.0f)
        };
    }

    Rectangle GetChainBounds(const Chain& chain, float padding = 0.0f) {
        return GetFlexibleBounds(chain.points, chain.start, chain.end,
            padding + fmaxf(2.0f, chain.collisionRadius * chain.scale));
    }

    Rectangle GetPhysicsRopeBounds(const PhysicsRope& rope, float padding = 0.0f) {
        return GetFlexibleBounds(rope.points, rope.start, rope.end,
            padding + fmaxf(1.0f, rope.thickness * 0.5f));
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
        const Player* player4,
        int ignoredChain,
        int ignoredRope
    ) {
        std::vector<Rectangle> colliders = BuildSolids(level);
        Rectangle flexibleQueryBounds{};
        bool hasFlexibleQuery = false;
        if (ignoredChain >= 0 && ignoredChain < static_cast<int>(level.chains.size())) {
            flexibleQueryBounds = GetChainBounds(level.chains[ignoredChain], 48.0f);
            hasFlexibleQuery = true;
        }
        else if (ignoredRope >= 0 && ignoredRope < static_cast<int>(level.physicsRopes.size())) {
            flexibleQueryBounds = GetPhysicsRopeBounds(level.physicsRopes[ignoredRope], 48.0f);
            hasFlexibleQuery = true;
        }
        if (player != nullptr) {
            colliders.push_back(player->rect);
        }
        if (player2 != nullptr) {
            colliders.push_back(player2->rect);
        }
        if (player3 != nullptr) {
            colliders.push_back(player3->rect);
        }
        if (player4 != nullptr) {
            colliders.push_back(player4->rect);
        }

        for (const StoneBlock& block : level.stoneBlocks) {
            if (!IsPlayerCollisionLayer(block.layer)) continue;
            colliders.push_back(block.rect);
        }

        for (const Boulder& boulder : level.boulders) {
            if (!IsPlayerCollisionLayer(boulder.layer)) continue;
            colliders.push_back(GetBoulderBounds(boulder));
        }

        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (!IsPlayerCollisionLayer(wheel.layer)) continue;
            colliders.push_back(GetWheelBounds(wheel));
        }

        for (const Gear& gear : level.gears) {
            if (!IsPlayerCollisionLayer(gear.layer)) continue;
            colliders.push_back(GetGearBounds(gear));
        }

        for (const Flywheel& flywheel : level.flywheels) {
            if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
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
            if (!IsPlayerCollisionLayer(screw.layer)) continue;
            AppendScrewColliders(colliders, screw);
        }

        for (const Enemy& enemy : level.enemies) {
            colliders.push_back(enemy.rect);
        }

        for (int i = 0; i < static_cast<int>(level.chains.size()); i++) {
            if (i != ignoredChain &&
                (!hasFlexibleQuery || CheckCollisionRecs(flexibleQueryBounds, GetChainBounds(level.chains[i])))) {
                AppendChainPointColliders(colliders, level.chains[i]);
            }
        }
        for (int i = 0; i < static_cast<int>(level.physicsRopes.size()); i++) {
            if (i != ignoredRope &&
                (!hasFlexibleQuery || CheckCollisionRecs(flexibleQueryBounds, GetPhysicsRopeBounds(level.physicsRopes[i])))) {
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
        const Player* player3,
        const Player* player4,
        bool includeActors
    ) {
        std::vector<Rectangle> obstacles = BuildSolids(level);
        if (includeActors) {
            if (player1 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player1->rect));
            if (player2 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player2->rect));
            if (player3 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player3->rect));
            if (player4 != nullptr) obstacles.push_back(GetCharacterFluidBounds(player4->rect));
        }

        for (const StoneBlock& block : level.stoneBlocks) {
            if (IsPlayerCollisionLayer(block.layer)) obstacles.push_back(block.rect);
        }
        for (const Boulder& boulder : level.boulders) {
            if (!IsPlayerCollisionLayer(boulder.layer)) continue;
            AppendCircularFluidObstacle(obstacles, boulder.center, boulder.radius);
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (!IsPlayerCollisionLayer(wheel.layer)) continue;
            AppendCircularFluidObstacle(obstacles, wheel.center, wheel.radius);
        }
        for (const Gear& gear : level.gears) {
            if (!IsPlayerCollisionLayer(gear.layer)) continue;
            AppendCircularFluidObstacle(obstacles, gear.center, gear.radius * GearOuterRadiusScale);
        }
        for (const Flywheel& flywheel : level.flywheels) {
            if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
            AppendCircularFluidObstacle(obstacles, flywheel.center, flywheel.radius);
        }
        for (const HangingWeight& weight : level.weights) obstacles.push_back(weight.rect);
        if (includeActors) {
            for (const Enemy& enemy : level.enemies) {
                obstacles.push_back(GetCharacterFluidBounds(enemy.rect));
            }
        }
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

    std::vector<Rectangle> FilterFluidObstacles(
        const FluidField& fluid,
        const std::vector<Rectangle>& obstacles
    ) {
        const float padding = fmaxf(16.0f, fmaxf(fluid.cellSize, fluid.particleRadius) * 2.0f);
        const Rectangle influenceBounds{
            fluid.bounds.x - padding,
            fluid.bounds.y - padding,
            fluid.bounds.width + padding * 2.0f,
            fluid.bounds.height + padding * 2.0f
        };
        std::vector<Rectangle> nearby;
        nearby.reserve(obstacles.size());
        for (Rectangle obstacle : obstacles) {
            if (CheckCollisionRecs(influenceBounds, obstacle)) {
                nearby.push_back(obstacle);
            }
        }
        return nearby;
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
    pendingPlayerBindings = playerBindings;
    pendingControllerSettings = controllerSettings;
    pendingScreenShakeSetting = screenShakeSetting;
    pendingReducedFlashing = reducedFlashing;
    pendingHighContrast = highContrast;
    pendingObjectTooltipsEnabled = objectTooltipsEnabled;
    pendingColorblindSetting = colorblindSetting;
    settingsDropdown = SettingsDropdown::None;
    settingsBindingCapture = -1;
    settingsGamepadBindingCapture = -1;
}

void Game::OpenSettingsPopup() {
    ResetPendingSettings();
    settingsPage = SettingsPage::Display;
    settingsSelectedPlayer = 0;
    settingsControlsInputView = ControlsInputView::Keyboard;
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
    playerBindings = pendingPlayerBindings;
    controllerSettings = pendingControllerSettings;
    screenShakeSetting = pendingScreenShakeSetting;
    reducedFlashing = pendingReducedFlashing;
    highContrast = pendingHighContrast;
    objectTooltipsEnabled = pendingObjectTooltipsEnabled;
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
        const Player* activePlayer4 = fourPlayerEnabled && player4Alive ? &player4 : nullptr;
        std::vector<Rectangle> obstacles =
            BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3, activePlayer4, true);
        std::vector<Rectangle> gasObstacles =
            BuildFluidObstacles(level, nullptr, nullptr, nullptr, nullptr, false);
        for (FluidField& fluid : level.fluids) {
            const std::vector<Rectangle>& relevantObstacles =
                fluid.type == FluidType::Gas ? gasObstacles : obstacles;
            InitializeFluidField(
                fluid,
                FilterFluidObstacles(fluid, relevantObstacles),
                SelectedFluidMode(advancedFluidSimulation)
            );
        }
    }
}

void Game::Load() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(Constants::ScreenWidth, Constants::ScreenHeight, "Spin to Win - Power Pulley Panic");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();
    if (IsAudioDeviceReady()) {
        SetMasterVolume(masterVolume);
        titleMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_title_theme.wav",
            "assets/Music/power_pulley_panic_title_theme.ogg"));
        titleMusicLoaded = IsMusicValid(titleMusic);
        if (titleMusicLoaded) {
            titleMusic.looping = true;
            SetMusicVolume(titleMusic, musicVolume);
        }
        levelSelectMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_select_theme.wav",
            "assets/Music/power_pulley_panic_level_select_theme.ogg"));
        levelSelectMusicLoaded = IsMusicValid(levelSelectMusic);
        if (levelSelectMusicLoaded) {
            levelSelectMusic.looping = true;
            SetMusicVolume(levelSelectMusic, musicVolume);
        }
        levelOneMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_01_gatehouse.wav",
            "assets/Music/power_pulley_panic_level_01_gatehouse.ogg"));
        levelOneMusicLoaded = IsMusicValid(levelOneMusic);
        if (levelOneMusicLoaded) {
            levelOneMusic.looping = true;
            SetMusicVolume(levelOneMusic, musicVolume);
        }
        levelTwoMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_02_rotary_latch_lab.wav",
            "assets/Music/power_pulley_panic_level_02_rotary_latch_lab.ogg"));
        levelTwoMusicLoaded = IsMusicValid(levelTwoMusic);
        if (levelTwoMusicLoaded) {
            levelTwoMusic.looping = true;
            SetMusicVolume(levelTwoMusic, musicVolume);
        }
        levelThreeMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_03_flooded_lower_works.wav",
            "assets/Music/power_pulley_panic_level_03_flooded_lower_works.ogg"));
        levelThreeMusicLoaded = IsMusicValid(levelThreeMusic);
        if (levelThreeMusicLoaded) {
            levelThreeMusic.looping = true;
            SetMusicVolume(levelThreeMusic, musicVolume);
        }
        levelFourMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_04_counterweight_row.wav",
            "assets/Music/power_pulley_panic_level_04_counterweight_row.ogg"));
        levelFourMusicLoaded = IsMusicValid(levelFourMusic);
        if (levelFourMusicLoaded) {
            levelFourMusic.looping = true;
            SetMusicVolume(levelFourMusic, musicVolume);
        }
        levelFiveMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_05_neurotoxin_annex.wav",
            "assets/Music/power_pulley_panic_level_05_neurotoxin_annex.ogg"));
        levelFiveMusicLoaded = IsMusicValid(levelFiveMusic);
        if (levelFiveMusicLoaded) {
            levelFiveMusic.looping = true;
            SetMusicVolume(levelFiveMusic, musicVolume);
        }
        levelSixMusic = LoadMusicStream(MusicPath(
            "assets/third_party/Clark Audio/Clark Audio - MERCURY Beta/processed/power_pulley_panic_level_06_clocktower_core.wav",
            "assets/Music/power_pulley_panic_level_06_clocktower_core.ogg"));
        levelSixMusicLoaded = IsMusicValid(levelSixMusic);
        if (levelSixMusicLoaded) {
            levelSixMusic.looping = true;
            SetMusicVolume(levelSixMusic, musicVolume);
        }
        wendiLevelOneMusic = LoadMusicStream(MusicPath(
            "assets/first_party/audio/power_pulley_panic_wendi_01_three_step_tumble.wav",
            "assets/Music/power_pulley_panic_wendi_01_three_step_tumble.ogg"));
        wendiLevelOneMusicLoaded = IsMusicValid(wendiLevelOneMusic);
        if (wendiLevelOneMusicLoaded) {
            wendiLevelOneMusic.looping = true;
            SetMusicVolume(wendiLevelOneMusic, musicVolume);
        }
        portalLiftMusic = LoadMusicStream(MusicPath(
            "assets/first_party/music/power_pulley_panic_wendi_02_portal_lift.wav",
            "assets/Music/power_pulley_panic_wendi_02_portal_lift.ogg"));
        portalLiftMusicLoaded = IsMusicValid(portalLiftMusic);
        if (portalLiftMusicLoaded) {
            portalLiftMusic.looping = true;
            SetMusicVolume(portalLiftMusic, musicVolume);
        }
    }
    sceneTarget = LoadRenderTexture(Constants::ScreenWidth, Constants::ScreenHeight);
    if (sceneTarget.id > 0) SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_POINT);
    selectedResolutionIndex = FindResolutionPresetIndex(Constants::ScreenWidth, Constants::ScreenHeight);
    ResetPendingSettings();

    playerSpritesTexture = LoadTexture("assets/first_party/characters/Player_Sprites.png");
    playerFourSpritesTexture = LoadTexture("assets/first_party/characters/knight.png");
    skullTexture = LoadTexture("assets/first_party/characters/skull.png");
    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");
    industrialFarBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/3_Far_Background_Tile.png");
    chainLinksTexture = LoadTexture("assets/first_party/machines/chain_links.png");
    // Placeholder only: swap this before final enemy art lock.
    enemyPlaceholderTexture = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/6. Character Animations 32p/Anim_Robot_Walk1_v1.1_spritesheet.png");
    gasMaskTexture = LoadTexture("assets/first_party/items/gasmask.png");

    if (playerSpritesTexture.id > 0) SetTextureFilter(playerSpritesTexture, TEXTURE_FILTER_POINT);
    if (playerFourSpritesTexture.id > 0) SetTextureFilter(playerFourSpritesTexture, TEXTURE_FILTER_POINT);
    if (skullTexture.id > 0) SetTextureFilter(skullTexture, TEXTURE_FILTER_POINT);
    if (industrialTiles.id > 0) SetTextureFilter(industrialTiles, TEXTURE_FILTER_POINT);
    if (industrialBackground.id > 0) SetTextureFilter(industrialBackground, TEXTURE_FILTER_POINT);
    if (industrialFarBackground.id > 0) SetTextureFilter(industrialFarBackground, TEXTURE_FILTER_POINT);
    if (chainLinksTexture.id > 0) SetTextureFilter(chainLinksTexture, TEXTURE_FILTER_POINT);
    if (enemyPlaceholderTexture.id > 0) SetTextureFilter(enemyPlaceholderTexture, TEXTURE_FILTER_POINT);
    if (gasMaskTexture.id > 0) SetTextureFilter(gasMaskTexture, TEXTURE_FILTER_POINT);
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);

    achievements.Initialize("game_data/achievements.txt", "save/achievements.dat");

    InitializeOverworld();
    Reset();
}

void Game::UpdateGameplayCamera(float dt, bool snap) {
    Vector2 focus{};
    int focusCount = 0;
    const Player* activePlayers[]{
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
    };
    for (const Player* activePlayer : activePlayers) {
        if (activePlayer == nullptr) continue;
        focus.x += activePlayer->rect.x + activePlayer->rect.width * 0.5f;
        focus.y += activePlayer->rect.y + activePlayer->rect.height * 0.5f;
        ++focusCount;
    }
    if (focusCount == 0) {
        focus = {
            player.rect.x + player.rect.width * 0.5f,
            player.rect.y + player.rect.height * 0.5f
        };
        focusCount = 1;
    }
    focus.x /= static_cast<float>(focusCount);
    focus.y /= static_cast<float>(focusCount);

    Rectangle bounds = level.worldBounds;
    int nextCameraZone = -1;
    for (int zoneIndex = 0; zoneIndex < static_cast<int>(level.cameraZones.size()); ++zoneIndex) {
        if (CheckCollisionPointRec(focus, level.cameraZones[zoneIndex])) {
            bounds = level.cameraZones[zoneIndex];
            nextCameraZone = zoneIndex;
            break;
        }
    }
    constexpr float halfViewWidth = Constants::ScreenWidth * 0.5f;
    constexpr float halfViewHeight = Constants::ScreenHeight * 0.5f;
    Vector2 desired{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f
    };
    if (bounds.width > Constants::ScreenWidth) {
        desired.x = std::clamp(focus.x, bounds.x + halfViewWidth, bounds.x + bounds.width - halfViewWidth);
    }
    if (bounds.height > Constants::ScreenHeight) {
        desired.y = std::clamp(focus.y, bounds.y + halfViewHeight, bounds.y + bounds.height - halfViewHeight);
    }

    const bool enteredFixedRoom = nextCameraZone >= 0 && nextCameraZone != activeCameraZone &&
        bounds.width <= Constants::ScreenWidth && bounds.height <= Constants::ScreenHeight;
    activeCameraZone = nextCameraZone;

    gameplayCamera.offset = {halfViewWidth, halfViewHeight};
    gameplayCamera.rotation = 0.0f;
    gameplayCamera.zoom = 1.0f;
    if (snap || dt <= 0.0f || enteredFixedRoom) {
        gameplayCamera.target = desired;
        return;
    }

    const float followAmount = 1.0f - expf(-7.5f * dt);
    gameplayCamera.target.x += (desired.x - gameplayCamera.target.x) * followAmount;
    gameplayCamera.target.y += (desired.y - gameplayCamera.target.y) * followAmount;
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
    ResetPlayer(player4);
    player.rect.x = level.playerStart.x;
    player.rect.y = level.playerStart.y;
    player2.rect.x = level.playerStart.x;
    player2.rect.y = level.playerStart.y;
    player3.rect.x = level.playerStart.x;
    player3.rect.y = level.playerStart.y;
    player4.rect.x = level.playerStart.x;
    player4.rect.y = level.playerStart.y;
    if (!IsTilesetReferenceLevel(level)) {
        player2.rect.x += 44.0f;
        player3.rect.x += 88.0f;
        player4.rect.x += 132.0f;
    }
    const Player* initialPlayer2 = multiplayerEnabled ? &player2 : nullptr;
    const Player* initialPlayer3 = threePlayerEnabled ? &player3 : nullptr;
    const Player* initialPlayer4 = fourPlayerEnabled ? &player4 : nullptr;
    std::vector<Rectangle> fluidObstacles =
        BuildFluidObstacles(level, &player, initialPlayer2, initialPlayer3, initialPlayer4, true);
    std::vector<Rectangle> gasObstacles =
        BuildFluidObstacles(level, nullptr, nullptr, nullptr, nullptr, false);
    for (FluidField& fluid : level.fluids) {
        const std::vector<Rectangle>& relevantObstacles =
            fluid.type == FluidType::Gas ? gasObstacles : fluidObstacles;
        InitializeFluidField(
            fluid,
            FilterFluidObstacles(fluid, relevantObstacles),
            SelectedFluidMode(advancedFluidSimulation)
        );
    }
    deathRect = player.rect;
    playerDeathRect = player.rect;
    player2DeathRect = player2.rect;
    player3DeathRect = player3.rect;
    player4DeathRect = player4.rect;
    checkpointRespawn = level.playerStart;
    checkpointActivated = false;
    respawnGraceTimer = 0.0f;

    machineWinch.rect.x = machineWinch.startX;
    machineWinch.grabbed = false;

    won = false;
    lost = false;
    playerAlive = true;
    player2Alive = true;
    player3Alive = true;
    player4Alive = true;
    pulleyRotation = 0.0f;
    machinePhase = 0.0f;
    machinePower = 0.0f;
    gateBottom = HasArea(level.exitTrigger) ? level.exitTrigger.y + level.exitTrigger.height : 650.0f;
    levelClearTimer = 0.0f;
    toxinExposure = {};
    playerAir = {1.0f, 1.0f, 1.0f, 1.0f};
    playerAirWarningPhase = {};
    playerGasMasks = {};
    playerEnemyDamageGraceTimers = {};
    toxinEmissionAccumulator = 0.0f;
    toxinExhaustAccumulator = 0.0f;
    toxinLevelTimer = 0.0f;
    activeCameraZone = -1;
    UpdateGameplayCamera(0.0f, true);
}

void Game::StartGame() {
    Reset();
    mode = GameMode::Playing;
    menuMessage.clear();
}

void Game::OpenCharacterSelect(int playerCount) {
    characterSelectPlayerCount = std::clamp(playerCount, 1, 4);
    multiplayerEnabled = characterSelectPlayerCount >= 2;
    threePlayerEnabled = characterSelectPlayerCount >= 3;
    fourPlayerEnabled = characterSelectPlayerCount >= 4;
    characterSelectFocusPlayer = 0;
    selectedCharacters = {0, 1, 2, 3};
    characterSelectReady = {};
    mode = GameMode::CharacterSelect;
    titleModeMenuOpen = false;
    quitConfirmationOpen = false;
    menuMessage.clear();
}

void Game::InitializeOverworld() {
    overworldNodes = {
        {"gatehouse", "1", "Gatehouse Generator", {280.0f, 500.0f}, 0, true, false},
        {"rotary_latch_lab", "2", "Rotary Latch Lab", {475.0f, 390.0f}, 0, false, false},
        {"lower_works", "3", "Flooded Lower Works", {680.0f, 505.0f}, 0, false, false},
        {"counterweight_row", "4", "Counterweight Row", {885.0f, 365.0f}, 0, false, false},
        {"neurotoxin_maze", "5", "The Neurotoxin Annex", {1100.0f, 475.0f}, 0, false, false},
        {"clocktower_core", "6", "Clocktower Core", {1320.0f, 330.0f}, 0, false, false},

        {"wendis_level_1", "W1", "Wendi's Three-Step Tumble", {545.0f, 465.0f}, 1, true, false},
        {"wendis_level_2", "W2", "Portal Lift", {960.0f, 385.0f}, 1, true, false},
        {"wendis_level_3", "W3", "Rising Water Escape", {1250.0f, 520.0f}, 1, true, false},

        {"test_level", "T", "Test Level", {285.0f, 405.0f}, 2, true, false},
        {"massive_test_level", "M", "Massive Object Test Facility", {500.0f, 515.0f}, 2, true, false},
        {"spring_test_level", "S", "Spring Laboratory", {720.0f, 375.0f}, 2, true, false},
        {"tileset_reference", "R", "Tileset Reference", {935.0f, 490.0f}, 2, true, false},
        {"gear_render_gallery", "G", "Gear Render Gallery", {1160.0f, 350.0f}, 2, true, false},
        {"gatehouse_generator_test", "1G", "Gatehouse Generator Test", {1380.0f, 465.0f}, 2, true, false}
    };

    overworldPaths = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5},
        {6, 7},
        {7, 8},
        {9, 10},
        {10, 11},
        {11, 12},
        {12, 13},
        {0, 14}
    };

    selectedOverworldNode = 0;
    selectedOverworldWorld = 0;
    selectedOverworldNodesByWorld = {0, 6, 8};
}

void Game::OpenOverworld() {
    if (overworldNodes.empty()) {
        InitializeOverworld();
    }
    if (overworldNodes.empty()) {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        menuMessage = "The level-select map could not be initialized.";
        return;
    }

    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);
    selectedOverworldWorld = std::clamp(overworldNodes[selectedOverworldNode].world, 0, 2);
    selectedOverworldNodesByWorld[selectedOverworldWorld] = selectedOverworldNode;
    mode = GameMode::Overworld;
    titleModeMenuOpen = false;
    quitConfirmationOpen = false;
    menuMessage.clear();
}

void Game::SetOverworldWorld(int world) {
    if (overworldNodes.empty()) return;

    selectedOverworldWorld = std::clamp(world, 0, 2);
    int rememberedNode = selectedOverworldNodesByWorld[selectedOverworldWorld];
    if (rememberedNode >= 0 &&
        rememberedNode < static_cast<int>(overworldNodes.size()) &&
        overworldNodes[rememberedNode].world == selectedOverworldWorld) {
        selectedOverworldNode = rememberedNode;
        menuMessage.clear();
        return;
    }

    for (int nodeIndex = 0; nodeIndex < static_cast<int>(overworldNodes.size()); nodeIndex++) {
        if (overworldNodes[nodeIndex].world == selectedOverworldWorld) {
            selectedOverworldNode = nodeIndex;
            selectedOverworldNodesByWorld[selectedOverworldWorld] = nodeIndex;
            menuMessage.clear();
            return;
        }
    }
}

void Game::StartSelectedOverworldLevel() {
    if (overworldNodes.empty()) return;

    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);
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
            selectedOverworldWorld = overworldNodes[nextNode].world;
            selectedOverworldNodesByWorld[selectedOverworldWorld] = nextNode;
            menuMessage = overworldNodes[currentLevelNode].name + " complete. " + overworldNodes[nextNode].name + " unlocked.";
        }
        else {
            selectedOverworldNode = currentLevelNode;
            selectedOverworldWorld = overworldNodes[currentLevelNode].world;
            selectedOverworldNodesByWorld[selectedOverworldWorld] = currentLevelNode;
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
    UpdateMusic();
    screenShakeTimer = fmaxf(0.0f, screenShakeTimer - dt);
    achievements.Update(dt);
    respawnGraceTimer = fmaxf(0.0f, respawnGraceTimer - dt);

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

    if (mode == GameMode::CharacterSelect) {
        UpdateCharacterSelect();
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

    bool controllerPausePressed = false;
    for (const PlayerControllerSettings& controller : controllerSettings) {
        const int gamepad = AvailableGamepad(controller);
        controllerPausePressed = controllerPausePressed ||
            (gamepad >= 0 && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT));
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || controllerPausePressed) {
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
        UpdateMachines(dt, {}, {}, {}, {});
        if (levelClearTimer <= 0.0f) {
            CompleteCurrentLevelAndReturnToMap();
        }
        return;
    }

    const int playerOneGamepad = AvailableGamepad(controllerSettings[0]);
    const int playerTwoGamepad = AvailableGamepad(controllerSettings[1]);
    const int playerThreeGamepad = AvailableGamepad(controllerSettings[2]);
    const int playerFourGamepad = AvailableGamepad(controllerSettings[3]);
    const PlayerControls PlayerOneControls{
        playerBindings[0].left,
        playerBindings[0].right,
        playerBindings[0].up,
        playerBindings[0].down,
        playerBindings[0].jump,
        playerOneGamepad,
        GAMEPAD_BUTTON_LEFT_FACE_UP,
        GAMEPAD_BUTTON_LEFT_FACE_DOWN,
        controllerSettings[0].jump
    };
    const PlayerControls PlayerTwoControls{
        playerBindings[1].left, playerBindings[1].right, playerBindings[1].up,
        playerBindings[1].down, playerBindings[1].jump, playerTwoGamepad,
        GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN, controllerSettings[1].jump
    };
    const PlayerControls PlayerThreeControls{
        playerBindings[2].left, playerBindings[2].right, playerBindings[2].up,
        playerBindings[2].down, playerBindings[2].jump, playerThreeGamepad,
        GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN, controllerSettings[2].jump
    };
    const PlayerControls PlayerFourControls{
        playerBindings[3].left, playerBindings[3].right, playerBindings[3].up,
        playerBindings[3].down, playerBindings[3].jump, playerFourGamepad,
        GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN, controllerSettings[3].jump
    };

    PlayerMachineInput player1Input{};
    PlayerMachineInput player2Input{};
    PlayerMachineInput player3Input{};
    PlayerMachineInput player4Input{};
    if (!won && !lost) {
        auto readMachineInput = [](const PlayerKeyBindings& keys,
                                   const PlayerControllerSettings& controller,
                                   int gamepad) {
            PlayerMachineInput input{};
            if (IsControlDown(keys.left)) input.moveInput -= 1.0f;
            if (IsControlDown(keys.right)) input.moveInput += 1.0f;
            if (gamepad >= 0) {
                const float axis = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
                if (fabsf(axis) >= 0.2f) input.moveInput = axis;
                if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) input.moveInput = -1.0f;
                if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) input.moveInput = 1.0f;
            }
            input.interactHeld = IsControlDown(keys.interact) ||
                (gamepad >= 0 && IsGamepadButtonDown(gamepad, controller.interact));
            input.interactPressed = IsControlPressed(keys.interact) ||
                (gamepad >= 0 && IsGamepadButtonPressed(gamepad, controller.interact));
            input.interactReleased = IsControlReleased(keys.interact) ||
                (gamepad >= 0 && IsGamepadButtonReleased(gamepad, controller.interact));
            return input;
        };
        player1Input = readMachineInput(playerBindings[0], controllerSettings[0], playerOneGamepad);
        player2Input = readMachineInput(playerBindings[1], controllerSettings[1], playerTwoGamepad);
        player3Input = readMachineInput(playerBindings[2], controllerSettings[2], playerThreeGamepad);
        player4Input = readMachineInput(playerBindings[3], controllerSettings[3], playerFourGamepad);
    }

    if (level.script == LevelScript::PortalLift) {
        AdvanceButtonPlatformLoops(
            level,
            dt,
            {
                playerAlive ? &player : nullptr,
                multiplayerEnabled && player2Alive ? &player2 : nullptr,
                threePlayerEnabled && player3Alive ? &player3 : nullptr,
                fourPlayerEnabled && player4Alive ? &player4 : nullptr
            }
        );
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
    if (fourPlayerEnabled && player4Alive) {
        UpdatePlayer(player4, PlayerFourControls, dt, player4Input.moveInput);
    }
    UpdateMachines(dt, player1Input, player2Input, player3Input, player4Input);
    UpdatePlayerAir(dt);
    UpdateEnemies(dt);
    CheckFailureConditions();
    UpdateGameplayCamera(dt, false);

    CheckWinCondition(gateBottom);
}

void Game::UpdateTitle() {
    const int titleGamepad = AvailableGamepad(controllerSettings[0]);
    const bool controllerConfirmPressed =
        titleGamepad >= 0 && IsGamepadButtonPressed(titleGamepad, controllerSettings[0].jump);
    const bool controllerBackPressed =
        titleGamepad >= 0 && IsGamepadButtonPressed(titleGamepad, controllerSettings[0].interact);

    if (titleModeMenuOpen) {
        if (IsKeyPressed(KEY_ESCAPE) || controllerBackPressed) {
            titleModeMenuOpen = false;
            menuMessage.clear();
            return;
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || controllerConfirmPressed) {
            OpenCharacterSelect(1);
        }

        std::vector<MenuButton> buttons{
            {{92, 506, 386, 40}, "Single Player"},
            {{92, 555, 386, 40}, "2 Players"},
            {{92, 604, 386, 40}, "3 Players"},
            {{92, 653, 386, 40}, "4 Players"},
            {{92, 702, 386, 40}, "Back"}
        };

        if (WasButtonPressed(buttons[0])) {
            OpenCharacterSelect(1);
        }
        else if (WasButtonPressed(buttons[1])) {
            OpenCharacterSelect(2);
        }
        else if (WasButtonPressed(buttons[2])) {
            OpenCharacterSelect(3);
        }
        else if (WasButtonPressed(buttons[3])) {
            OpenCharacterSelect(4);
        }
        else if (WasButtonPressed(buttons[4])) {
            titleModeMenuOpen = false;
            menuMessage.clear();
        }

        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || controllerConfirmPressed) {
        titleModeMenuOpen = true;
        menuMessage.clear();
    }

    std::vector<MenuButton> buttons{
        {{92, 490, 386, 40}, "New Game"},
        {{92, 539, 386, 40}, "Continue"},
        {{92, 588, 386, 40}, "Load Custom"},
        {{92, 637, 386, 40}, "Settings"},
        {{92, 686, 386, 40}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        titleModeMenuOpen = true;
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[1])) {
        OpenCharacterSelect(1);
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

void Game::UpdateCharacterSelect() {
    const int primaryGamepad = AvailableGamepad(controllerSettings[0]);
    const bool controllerBackPressed =
        primaryGamepad >= 0 &&
        IsGamepadButtonPressed(primaryGamepad, controllerSettings[0].interact);
    if (IsKeyPressed(KEY_ESCAPE) || controllerBackPressed) {
        mode = GameMode::Title;
        titleModeMenuOpen = true;
        characterSelectReady = {};
        menuMessage.clear();
        return;
    }

    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
        int direction = 0;
        if (IsControlPressed(playerBindings[playerIndex].left)) direction--;
        if (IsControlPressed(playerBindings[playerIndex].right)) direction++;

        const int gamepad = AvailableGamepad(controllerSettings[playerIndex]);
        if (gamepad >= 0) {
            if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) direction--;
            if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) direction++;
        }

        if (direction != 0) {
            selectedCharacters[playerIndex] =
                (selectedCharacters[playerIndex] + direction + kCharacterCount) % kCharacterCount;
            characterSelectReady[playerIndex] = false;
            characterSelectFocusPlayer = playerIndex;
            menuMessage.clear();
        }

        bool readyPressed = IsControlPressed(playerBindings[playerIndex].interact);
        if (gamepad >= 0) {
            readyPressed = readyPressed ||
                IsGamepadButtonPressed(gamepad, controllerSettings[playerIndex].jump);
        }
        if (readyPressed) {
            characterSelectReady[playerIndex] = !characterSelectReady[playerIndex];
            characterSelectFocusPlayer = playerIndex;
            menuMessage.clear();
        }
    }

    constexpr float cardWidth = 260.0f;
    constexpr float cardGap = 28.0f;
    constexpr float cardsX = 238.0f;
    const Vector2 mouse = GetUiMousePosition();
    for (int characterIndex = 0; characterIndex < kCharacterCount; characterIndex++) {
        Rectangle card{
            cardsX + characterIndex * (cardWidth + cardGap),
            235.0f,
            cardWidth,
            340.0f
        };
        if (CheckCollisionPointRec(mouse, card) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedCharacters[characterSelectFocusPlayer] = characterIndex;
            characterSelectReady[characterSelectFocusPlayer] = false;
            menuMessage.clear();
        }
    }

    const float playerButtonsWidth =
        characterSelectPlayerCount * 220.0f + (characterSelectPlayerCount - 1) * 40.0f;
    const float playerButtonsX = (Constants::ScreenWidth - playerButtonsWidth) * 0.5f;
    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
        Rectangle playerButton{
            playerButtonsX + playerIndex * 260.0f,
            635.0f,
            220.0f,
            52.0f
        };
        if (CheckCollisionPointRec(mouse, playerButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            characterSelectFocusPlayer = playerIndex;
        }
        Rectangle readyButton{playerButton.x, 696.0f, playerButton.width, 42.0f};
        if (CheckCollisionPointRec(mouse, readyButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            characterSelectFocusPlayer = playerIndex;
            characterSelectReady[playerIndex] = !characterSelectReady[playerIndex];
        }
    }

    bool allReady = true;
    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
        allReady = allReady && characterSelectReady[playerIndex];
    }

    Rectangle startButton{625.0f, 782.0f, 350.0f, 48.0f};
    Rectangle backButton{100.0f, 782.0f, 220.0f, 48.0f};
    bool controllerStartPressed = false;
    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; ++playerIndex) {
        const int gamepad = AvailableGamepad(controllerSettings[playerIndex]);
        controllerStartPressed = controllerStartPressed ||
            (gamepad >= 0 && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT));
    }
    if ((allReady && (IsKeyPressed(KEY_ENTER) || controllerStartPressed)) ||
        (allReady && WasButtonPressed({startButton, "Continue"}))) {
        OpenOverworld();
    }
    else if (WasButtonPressed({backButton, "Back"})) {
        mode = GameMode::Title;
        titleModeMenuOpen = true;
        characterSelectReady = {};
        menuMessage.clear();
    }
    else if (!allReady && IsKeyPressed(KEY_ENTER)) {
        menuMessage = "Every player must be ready before continuing.";
    }
}

void Game::UpdateOverworld() {
    if (overworldNodes.empty()) {
        InitializeOverworld();
        if (overworldNodes.empty()) {
            mode = GameMode::Title;
            titleModeMenuOpen = false;
            menuMessage = "The level-select map could not be initialized.";
            return;
        }
    }
    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);
    selectedOverworldWorld = std::clamp(selectedOverworldWorld, 0, 2);

    const int overworldGamepad = AvailableGamepad(controllerSettings[0]);
    const bool controllerBackPressed = overworldGamepad >= 0 &&
        IsGamepadButtonPressed(overworldGamepad, controllerSettings[0].interact);
    if (IsKeyPressed(KEY_ESCAPE) || controllerBackPressed) {
        mode = GameMode::Title;
        titleModeMenuOpen = false;
        menuMessage.clear();
        return;
    }

    const bool gamepadAvailable = overworldGamepad >= 0;
    const bool previousWorldPressed =
        IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_PAGE_UP) ||
        (gamepadAvailable &&
            IsGamepadButtonPressed(overworldGamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
    const bool nextWorldPressed =
        IsKeyPressed(KEY_E) || IsKeyPressed(KEY_PAGE_DOWN) ||
        (gamepadAvailable &&
            IsGamepadButtonPressed(overworldGamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1));
    if (previousWorldPressed) {
        SetOverworldWorld((selectedOverworldWorld + 2) % 3);
    }
    else if (nextWorldPressed) {
        SetOverworldWorld((selectedOverworldWorld + 1) % 3);
    }

    std::vector<int> visibleNodes;
    for (int nodeIndex = 0; nodeIndex < static_cast<int>(overworldNodes.size()); nodeIndex++) {
        if (overworldNodes[nodeIndex].world == selectedOverworldWorld) {
            visibleNodes.push_back(nodeIndex);
        }
    }
    auto selectedIt = std::find(visibleNodes.begin(), visibleNodes.end(), selectedOverworldNode);
    int visibleSelection = selectedIt != visibleNodes.end()
        ? static_cast<int>(std::distance(visibleNodes.begin(), selectedIt))
        : 0;

    const bool previousNodePressed =
        IsControlPressed(playerBindings[0].left) || IsKeyPressed(KEY_LEFT) ||
        (gamepadAvailable &&
            IsGamepadButtonPressed(overworldGamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT));
    const bool nextNodePressed =
        IsControlPressed(playerBindings[0].right) || IsKeyPressed(KEY_RIGHT) ||
        (gamepadAvailable &&
            IsGamepadButtonPressed(overworldGamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT));
    if (previousNodePressed && !visibleNodes.empty()) {
        visibleSelection = (visibleSelection + static_cast<int>(visibleNodes.size()) - 1) %
            static_cast<int>(visibleNodes.size());
        selectedOverworldNode = visibleNodes[visibleSelection];
        selectedOverworldNodesByWorld[selectedOverworldWorld] = selectedOverworldNode;
        menuMessage.clear();
    }
    else if (nextNodePressed && !visibleNodes.empty()) {
        visibleSelection = (visibleSelection + 1) % static_cast<int>(visibleNodes.size());
        selectedOverworldNode = visibleNodes[visibleSelection];
        selectedOverworldNodesByWorld[selectedOverworldWorld] = selectedOverworldNode;
        menuMessage.clear();
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        (gamepadAvailable &&
            IsGamepadButtonPressed(overworldGamepad, controllerSettings[0].jump))) {
        StartSelectedOverworldLevel();
    }

    std::vector<MenuButton> worldButtons{
        {{770, 88, 230, 44}, "World 1"},
        {{1012, 88, 230, 44}, "World 2"},
        {{1360, 24, 150, 38}, "Test World"}
    };
    for (int worldIndex = 0; worldIndex < static_cast<int>(worldButtons.size()); worldIndex++) {
        if (WasButtonPressed(worldButtons[worldIndex])) {
            SetOverworldWorld(worldIndex);
        }
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
        if (overworldNodes[i].world != selectedOverworldWorld) continue;
        if (CheckCollisionPointCircle(mouse, overworldNodes[i].position, 34.0f) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedOverworldNode = i;
            selectedOverworldNodesByWorld[selectedOverworldWorld] = i;
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

    bool controllerResumePressed = false;
    for (const PlayerControllerSettings& controller : controllerSettings) {
        const int gamepad = AvailableGamepad(controller);
        controllerResumePressed = controllerResumePressed ||
            (gamepad >= 0 && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT));
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || controllerResumePressed) {
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

    const int gamepad = AvailableGamepad(controllerSettings[0]);
    if ((gamepad >= 0 && IsGamepadButtonPressed(gamepad, controllerSettings[0].jump)) ||
        WasButtonPressed(buttons[0])) {
        StartGame();
    }
    else if ((gamepad >= 0 && IsGamepadButtonPressed(gamepad, controllerSettings[0].interact)) ||
        WasButtonPressed(buttons[1])) {
        quitConfirmationOpen = true;
    }
}

void Game::UpdateControlsPopup() {
    std::vector<MenuButton> buttons{
        {{705, 775, 190, 46}, "Close"}
    };

    const int gamepad = AvailableGamepad(controllerSettings[0]);
    const bool controllerClosePressed = gamepad >= 0 &&
        (IsGamepadButtonPressed(gamepad, controllerSettings[0].jump) ||
            IsGamepadButtonPressed(gamepad, controllerSettings[0].interact));
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        controllerClosePressed || WasButtonPressed(buttons[0])) {
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

    settingsSelectedPlayer = std::clamp(settingsSelectedPlayer, 0, 3);
    PlayerKeyBindings& selectedBindings = pendingPlayerBindings[settingsSelectedPlayer];
    KeyboardKey* bindingTargets[] = {
        &selectedBindings.left,
        &selectedBindings.right,
        &selectedBindings.up,
        &selectedBindings.down,
        &selectedBindings.jump,
        &selectedBindings.interact
    };
    constexpr int bindingTargetCount = static_cast<int>(sizeof(bindingTargets) / sizeof(bindingTargets[0]));
    if (settingsBindingCapture < -1 || settingsBindingCapture >= bindingTargetCount) {
        settingsBindingCapture = -1;
    }
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

    if (settingsGamepadBindingCapture < -1 || settingsGamepadBindingCapture > 1) {
        settingsGamepadBindingCapture = -1;
    }
    if (settingsGamepadBindingCapture >= 0) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            settingsGamepadBindingCapture = -1;
            return;
        }
        PlayerControllerSettings& controller = pendingControllerSettings[settingsSelectedPlayer];
        const int gamepad = AvailableGamepad(controller);
        if (gamepad < 0) {
            settingsGamepadBindingCapture = -1;
            return;
        }
        for (GamepadButton button : kBindableGamepadButtons) {
            if (!IsGamepadButtonPressed(gamepad, button)) continue;
            if (settingsGamepadBindingCapture == 0) controller.jump = button;
            else controller.interact = button;
            settingsGamepadBindingCapture = -1;
            break;
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
        case SettingsDropdown::ControllerDevice: anchor = layout.controlRows[0]; optionCount = 5; break;
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
            case SettingsDropdown::ControllerDevice:
                pendingControllerSettings[settingsSelectedPlayer].gamepad = i - 1;
                if (i > 0) {
                    for (int playerIndex = 0; playerIndex < 4; ++playerIndex) {
                        if (playerIndex != settingsSelectedPlayer &&
                            pendingControllerSettings[playerIndex].gamepad == i - 1) {
                            pendingControllerSettings[playerIndex].gamepad = -1;
                        }
                    }
                }
                break;
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
                settingsDropdown = SettingsDropdown::None;
                settingsBindingCapture = -1;
                settingsGamepadBindingCapture = -1;
                return;
            }
        }
        if (settingsPage == SettingsPage::Controls) {
            for (int i = 0; i < static_cast<int>(layout.playerTabs.size()); ++i) {
                if (!clicked(layout.playerTabs[i])) continue;
                settingsSelectedPlayer = i;
                settingsDropdown = SettingsDropdown::None;
                settingsBindingCapture = -1;
                settingsGamepadBindingCapture = -1;
                return;
            }
            for (int i = 0; i < static_cast<int>(layout.inputTabs.size()); ++i) {
                if (!clicked(layout.inputTabs[i])) continue;
                settingsControlsInputView = static_cast<ControlsInputView>(i);
                settingsDropdown = SettingsDropdown::None;
                settingsBindingCapture = -1;
                settingsGamepadBindingCapture = -1;
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
        if (settingsControlsInputView == ControlsInputView::Keyboard) {
            for (int i = 0; i < 6; ++i) {
                if (!clicked(layout.controlRows[i])) continue;
                settingsBindingCapture = i;
                return;
            }
        }
        else if (clicked(layout.controlRows[0])) {
            settingsDropdown = SettingsDropdown::ControllerDevice;
            return;
        }
        else if (clicked(layout.controlRows[1])) {
            if (AvailableGamepad(pendingControllerSettings[settingsSelectedPlayer]) >= 0) {
                settingsGamepadBindingCapture = 0;
            }
            return;
        }
        else if (clicked(layout.controlRows[2])) {
            if (AvailableGamepad(pendingControllerSettings[settingsSelectedPlayer]) >= 0) {
                settingsGamepadBindingCapture = 1;
            }
            return;
        }
        else if (clicked(layout.controlRows[3])) {
            pendingControllerSettings[settingsSelectedPlayer].vibration =
                !pendingControllerSettings[settingsSelectedPlayer].vibration;
            return;
        }
    }
    else if (settingsPage == SettingsPage::Accessibility && mousePressed) {
        if (clicked(layout.controls[0])) settingsDropdown = SettingsDropdown::ScreenShake;
        else if (clicked(layout.controls[1])) pendingReducedFlashing = !pendingReducedFlashing;
        else if (clicked(layout.controls[2])) pendingHighContrast = !pendingHighContrast;
        else if (clicked(layout.controls[3])) settingsDropdown = SettingsDropdown::ColorblindMode;
        else if (clicked(layout.controls[4])) pendingObjectTooltipsEnabled = !pendingObjectTooltipsEnabled;
        if (settingsDropdown != SettingsDropdown::None || clicked(layout.controls[1]) ||
            clicked(layout.controls[2]) || clicked(layout.controls[4])) return;
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

    const int gamepad = AvailableGamepad(controllerSettings[0]);
    const bool controllerConfirmPressed =
        gamepad >= 0 && IsGamepadButtonPressed(gamepad, controllerSettings[0].jump);
    const bool controllerBackPressed =
        gamepad >= 0 && IsGamepadButtonPressed(gamepad, controllerSettings[0].interact);
    if (IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER) || controllerConfirmPressed ||
        WasButtonPressed(buttons[0])) {
        shouldQuit = true;
    }
    else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE) || controllerBackPressed ||
        WasButtonPressed(buttons[1])) {
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

    bool onLadder = IsOnAnyLadder(level, activePlayer.rect);
    FluidSample simulatedWater = SampleFluidAroundRectangle(level, FluidType::Water, activePlayer.rect);
    bool swimming = IsPlayerSwimming(activePlayer, level);
    activePlayer.climbing = onLadder &&
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

    float standingRampAngle = 0.0f;
    if (level.script == LevelScript::PortalLift && activePlayer.onGround &&
        TryGetStandingRampAngle(activePlayer.rect, level.ramps, standingRampAngle)) {
        const float downhillDirection = standingRampAngle >= 0.0f ? 1.0f : -1.0f;
        const bool resistingUphill = moveInput * downhillDirection < -0.2f;
        if (!resistingUphill) {
            constexpr float MinimumSlideSpeed = 155.0f;
            if (downhillDirection > 0.0f) {
                activePlayer.velocity.x = fmaxf(activePlayer.velocity.x, MinimumSlideSpeed);
            }
            else {
                activePlayer.velocity.x = fminf(activePlayer.velocity.x, -MinimumSlideSpeed);
            }
        }
    }

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

    if (activePlayer.climbing) {
        activePlayer.velocity.y = controlDown(controls.up, controls.upButton)
            ? -Constants::PlayerClimbSpeed
            : Constants::PlayerClimbSpeed;
    }
    else if (swimming && controlsEnabled) {
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

        const float previousFootX = RectCenterX(activePlayer.rect);
        const float previousFootY = activePlayer.rect.y + activePlayer.rect.height;
        activePlayer.rect.x += activePlayer.velocity.x * dt;
        ResolveHorizontal(activePlayer, solids);

        for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
            StoneBlock& block = level.stoneBlocks[i];
            if (!IsPlayerCollisionLayer(block.layer) || !CheckCollisionRecs(activePlayer.rect, block.rect)) {
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
            if (!IsPlayerCollisionLayer(boulder.layer) ||
                !CheckCollisionCircleRec(boulder.center, boulder.radius, activePlayer.rect)) {
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
            if (!IsPlayerCollisionLayer(wheel.layer) ||
                !CheckCollisionCircleRec(wheel.center, wheel.radius, activePlayer.rect)) {
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
            if (!IsPlayerCollisionLayer(gear.layer) ||
                !CheckCollisionCircleRec(gear.center, gear.radius * GearOuterRadiusScale, activePlayer.rect)) {
                continue;
            }

            if (gear.mounting == GearMounting::Mounted) {
                if (activePlayer.velocity.x > 0.0f) {
                    activePlayer.rect.x = gear.center.x - gear.radius * GearOuterRadiusScale - activePlayer.rect.width;
                }
                else if (activePlayer.velocity.x < 0.0f) {
                    activePlayer.rect.x = gear.center.x + gear.radius * GearOuterRadiusScale;
                }
                continue;
            }

            if (activePlayer.velocity.x > 0.0f) {
                gear.velocity.x = activePlayer.velocity.x * GetGearPushScale(gear);
                gear.angularVelocity = gear.velocity.x / fmaxf(1.0f, gear.radius) * RAD2DEG;
                gear.center.x += gear.velocity.x * dt;
                ResolveGearCollisions(gear, gearPushSolids);
                activePlayer.rect.x = gear.center.x - gear.radius * GearOuterRadiusScale - activePlayer.rect.width;
            }
            else if (activePlayer.velocity.x < 0.0f) {
                gear.velocity.x = activePlayer.velocity.x * GetGearPushScale(gear);
                gear.angularVelocity = gear.velocity.x / fmaxf(1.0f, gear.radius) * RAD2DEG;
                gear.center.x += gear.velocity.x * dt;
                ResolveGearCollisions(gear, gearPushSolids);
                activePlayer.rect.x = gear.center.x + gear.radius * GearOuterRadiusScale;
            }
        }

        std::vector<Rectangle> flywheelPushSolids = solids;
        for (int i = 0; i < static_cast<int>(level.flywheels.size()); i++) {
            Flywheel& flywheel = level.flywheels[i];
            if (!IsPlayerCollisionLayer(flywheel.layer) ||
                !CheckCollisionCircleRec(flywheel.center, flywheel.radius, activePlayer.rect)) {
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
            if (!IsPlayerCollisionLayer(block.layer)) continue;
            playerSolids.push_back(block.rect);
        }
        for (const Boulder& boulder : level.boulders) {
            if (!IsPlayerCollisionLayer(boulder.layer)) continue;
            playerSolids.push_back(GetBoulderBounds(boulder));
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (!IsPlayerCollisionLayer(wheel.layer)) continue;
            playerSolids.push_back(GetWheelBounds(wheel));
        }
        for (const Gear& gear : level.gears) {
            if (!IsPlayerCollisionLayer(gear.layer)) continue;
            playerSolids.push_back(GetGearBounds(gear));
        }
        for (const Flywheel& flywheel : level.flywheels) {
            if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
            playerSolids.push_back(GetFlywheelBounds(flywheel));
        }
        for (const Screw& screw : level.screws) {
            if (!IsPlayerCollisionLayer(screw.layer)) continue;
            AppendScrewColliders(playerSolids, screw);
        }

        ResolveVertical(activePlayer, playerSolids);
        ApplyScrewConveyor(activePlayer.rect, activePlayer.velocity, 1.0f,
            WorldLayer::Middleground, level.screws, dt);
        ResolvePlayerRampStanding(activePlayer, previousFootX, previousFootY, level.ramps, dt);
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

std::array<bool, 4> Game::UpdateFlexibleEndpointInteractions(
    const std::array<Player*, 4>& players,
    const std::array<PlayerMachineInput, 4>& inputs
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

    std::array<bool, 4> consumed{};
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

void Game::UpdateNeurotoxin(
    float dt,
    const PlayerMachineInput& player1Input,
    const PlayerMachineInput& player2Input,
    const PlayerMachineInput& player3Input,
    const PlayerMachineInput& player4Input,
    const std::array<bool, 4>& consumedInputs
) {
    toxinLevelTimer += dt;

    if (!won && !lost) {
        const bool player1Near = playerAlive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player.rect);
        const bool player2Near = multiplayerEnabled && player2Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player2.rect);
        const bool player3Near = threePlayerEnabled && player3Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player3.rect);
        const bool player4Near = fourPlayerEnabled && player4Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player4.rect);
        const bool valveHeld =
            (player1Near && player1Input.interactHeld && !consumedInputs[0]) ||
            (player2Near && player2Input.interactHeld && !consumedInputs[1]) ||
            (player3Near && player3Input.interactHeld && !consumedInputs[2]) ||
            (player4Near && player4Input.interactHeld && !consumedInputs[3]);

        if (valveHeld && !level.valve.opened) {
            level.valve.turnDegrees = fminf(360.0f, level.valve.turnDegrees + level.valve.turnSpeed * dt);
            if (level.valve.turnDegrees >= 360.0f) {
                level.valve.opened = true;
                achievements.Unlock("still_alive");
            }
        }

        const ToxinLeak& leak = level.toxinLeak;
        if (!level.valve.opened && leak.fluidIndex >= 0 &&
            leak.fluidIndex < static_cast<int>(level.fluids.size())) {
            FluidField& toxin = level.fluids[leak.fluidIndex];
            const float toxinMass = GetFluidMass(toxin);
            if (toxin.type == FluidType::Gas && toxinMass < leak.maximumMass) {
                toxinEmissionAccumulator += leak.massPerSecond * dt;
                const float requested = fminf(
                    toxinEmissionAccumulator,
                    leak.maximumMass - toxinMass
                );
                if (requested > 0.0001f) {
                    const float pulse = sinf(toxinLevelTimer * 3.7f);
                    const float emitted = EmitGasDensity(
                        toxin,
                        leak.source,
                        {48.0f + pulse * 18.0f, -42.0f},
                        requested
                    );
                    toxinEmissionAccumulator = fmaxf(0.0f, toxinEmissionAccumulator - emitted);
                }
            }
        }

        if (level.valve.opened && HasArea(level.exitTrigger) && leak.fluidIndex >= 0 &&
            leak.fluidIndex < static_cast<int>(level.fluids.size())) {
            FluidField& toxin = level.fluids[leak.fluidIndex];
            float closedBottom = level.exitTrigger.y + level.exitTrigger.height;
            float doorOpenAmount = Clamp01((closedBottom - gateBottom) / level.exitTrigger.height);
            float exhaustRate = 3.0f + GetFluidMass(toxin) * 0.003f;
            toxinExhaustAccumulator += exhaustRate * doorOpenAmount * dt;
            const float requestedRemoval = toxinExhaustAccumulator;
            toxinExhaustAccumulator = 0.0f;
            VentGasDensity(
                toxin,
                level.exitTrigger,
                {-1.0f, 0.0f},
                310.0f,
                185.0f * doorOpenAmount,
                requestedRemoval,
                dt
            );
        }

        Player* players[] = {
            playerAlive ? &player : nullptr,
            multiplayerEnabled && player2Alive ? &player2 : nullptr,
            threePlayerEnabled && player3Alive ? &player3 : nullptr,
            fourPlayerEnabled && player4Alive ? &player4 : nullptr
        };
        for (int index = 0; index < 4; ++index) {
            if (players[index] == nullptr) continue;
            if (playerGasMasks[index]) {
                toxinExposure[index] = fmaxf(0.0f, toxinExposure[index] - 0.30f * dt);
                playerAir[index] = 1.0f;
                continue;
            }
            const float density = SampleFluidAroundRectangle(level, FluidType::Gas, players[index]->rect).density;
            if (density >= 0.025f) {
                const float doseScale = 0.25f + density * 1.85f;
                toxinExposure[index] = fminf(1.0f,
                    toxinExposure[index] + level.toxinLeak.exposureRate * doseScale * dt);
                float airUseRate = 0.028f + density * 0.115f;
                playerAir[index] = fmaxf(0.0f, playerAir[index] - airUseRate * dt);
            }
            else {
                const float recoveryRate = level.valve.opened ? 0.075f : 0.032f;
                toxinExposure[index] = fmaxf(0.0f, toxinExposure[index] - recoveryRate * dt);
                float airRecoveryRate = level.valve.opened ? 0.24f : 0.14f;
                playerAir[index] = fminf(1.0f, playerAir[index] + airRecoveryRate * dt);
            }
        }
    }

    machinePower = GetValveOpenAmount(level.valve);
    if (HasArea(level.exitTrigger)) {
        const float targetGateBottom = level.valve.opened
            ? level.exitTrigger.y
            : level.exitTrigger.y + level.exitTrigger.height;
        gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
    }
}

void Game::UpdatePlayerAir(float dt) {
    if (won || lost) return;

    Player* players[] = {
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
    };

    for (int index = 0; index < 4; ++index) {
        playerEnemyDamageGraceTimers[index] = fmaxf(0.0f, playerEnemyDamageGraceTimers[index] - dt);
        Player* activePlayer = players[index];
        if (activePlayer == nullptr) {
            playerAirWarningPhase[index] = 0.0f;
            continue;
        }

        const bool toxinHazard =
            level.script == LevelScript::NeurotoxinMaze &&
            !playerGasMasks[index] &&
            SampleFluidAroundRectangle(level, FluidType::Gas, activePlayer->rect).density >= 0.025f;
        const bool underwaterHazard =
            level.script != LevelScript::NeurotoxinMaze &&
            IsPlayerHeadSubmerged(*activePlayer, level);

        if (level.script != LevelScript::NeurotoxinMaze) {
            constexpr float UnderwaterAirUseRate = 0.085f;
            constexpr float SurfaceAirRecoveryRate = 0.34f;
            if (underwaterHazard) {
                playerAir[index] = fmaxf(
                    0.0f,
                    playerAir[index] - UnderwaterAirUseRate * dt
                );
            }
            else {
                playerAir[index] = fminf(
                    1.0f,
                    playerAir[index] + SurfaceAirRecoveryRate * dt
                );
            }
        }

        if (toxinHazard || underwaterHazard) {
            const float airDanger = Clamp01(1.0f - playerAir[index]);
            const float acceleration = powf(airDanger, 1.35f);
            const float flashesPerSecond = 0.65f + acceleration * 4.85f;
            playerAirWarningPhase[index] = fmodf(
                playerAirWarningPhase[index] + flashesPerSecond * dt,
                1.0f
            );
        }
        else {
            playerAirWarningPhase[index] = 0.0f;
        }
    }
}

void Game::UpdateMachines(
    float dt,
    const PlayerMachineInput& player1Input,
    const PlayerMachineInput& player2Input,
    const PlayerMachineInput& player3Input,
    const PlayerMachineInput& player4Input
) {
    std::array<bool, 4> flexibleInputConsumed{};
    if (!won && !lost) {
        UpdateWind(dt);
        UpdateFluids(dt);
        UpdatePhysicsObjects(dt);
        UpdateButtons();
        UpdatePortals();
        UpdateArrowTraps(dt);
        UpdateBreakableTiles(dt);
        const Player* activePlayer1 = playerAlive ? &player : nullptr;
        const Player* activePlayer2 = multiplayerEnabled && player2Alive ? &player2 : nullptr;
        const Player* activePlayer3 = threePlayerEnabled && player3Alive ? &player3 : nullptr;
        const Player* activePlayer4 = fourPlayerEnabled && player4Alive ? &player4 : nullptr;
        std::array<Player*, 4> flexiblePlayers{
            playerAlive ? &player : nullptr,
            multiplayerEnabled && player2Alive ? &player2 : nullptr,
            threePlayerEnabled && player3Alive ? &player3 : nullptr,
            fourPlayerEnabled && player4Alive ? &player4 : nullptr
        };
        std::array<PlayerMachineInput, 4> flexibleInputs{
            player1Input, player2Input, player3Input, player4Input
        };
        flexibleInputConsumed = UpdateFlexibleEndpointInteractions(flexiblePlayers, flexibleInputs);

        const Rectangle flexibleSimulationBounds{
            gameplayCamera.target.x - Constants::ScreenWidth * 0.5f - 240.0f,
            gameplayCamera.target.y - Constants::ScreenHeight * 0.5f - 240.0f,
            Constants::ScreenWidth + 480.0f,
            Constants::ScreenHeight + 480.0f
        };

        for (int chainIndex = 0; chainIndex < static_cast<int>(level.chains.size()); chainIndex++) {
            Chain& chain = level.chains[chainIndex];
            if (!CheckCollisionRecs(flexibleSimulationBounds, GetChainBounds(chain))) {
                continue;
            }
            std::vector<Rectangle> colliders = BuildFlexibleBodyColliders(
                level,
                activePlayer1,
                activePlayer2,
                activePlayer3,
                activePlayer4,
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
            if (!CheckCollisionRecs(flexibleSimulationBounds, GetPhysicsRopeBounds(rope))) {
                continue;
            }
            std::vector<Rectangle> colliders = BuildFlexibleBodyColliders(
                level,
                activePlayer1,
                activePlayer2,
                activePlayer3,
                activePlayer4,
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

    if (level.script == LevelScript::NeurotoxinMaze) {
        UpdateNeurotoxin(
            dt, player1Input, player2Input, player3Input, player4Input, flexibleInputConsumed);
        return;
    }

    if (level.script == LevelScript::FloodedFoundry) {
        if (!won && !lost) {
            bool player1NearValve = playerAlive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
            bool player2NearValve = multiplayerEnabled && player2Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player2.rect);
            bool player3NearValve = threePlayerEnabled && player3Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player3.rect);
            bool player4NearValve = fourPlayerEnabled && player4Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player4.rect);
            bool valveHeld = (player1NearValve && player1Input.interactHeld && !flexibleInputConsumed[0]) ||
                (player2NearValve && player2Input.interactHeld && !flexibleInputConsumed[1]) ||
                (player3NearValve && player3Input.interactHeld && !flexibleInputConsumed[2]) ||
                (player4NearValve && player4Input.interactHeld && !flexibleInputConsumed[3]);
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

    if (level.script == LevelScript::ClocktowerCore) {
        const auto toggleNearbyClockGear = [&](const Player* activePlayer, bool interactPressed, bool inputConsumed) {
            if (activePlayer == nullptr || !interactPressed || inputConsumed) return;
            for (Gear& gear : level.gears) {
                if (gear.clockHand == ClockHandType::None) continue;
                if (IsPlayerInsideClockGearProxy(gear, *activePlayer)) {
                    gear.stopped = !gear.stopped;
                    return;
                }
            }
        };
        if (!won && !lost) {
            toggleNearbyClockGear(playerAlive ? &player : nullptr, player1Input.interactPressed, flexibleInputConsumed[0]);
            toggleNearbyClockGear(multiplayerEnabled && player2Alive ? &player2 : nullptr,
                player2Input.interactPressed, flexibleInputConsumed[1]);
            toggleNearbyClockGear(threePlayerEnabled && player3Alive ? &player3 : nullptr,
                player3Input.interactPressed, flexibleInputConsumed[2]);
            toggleNearbyClockGear(fourPlayerEnabled && player4Alive ? &player4 : nullptr,
                player4Input.interactPressed, flexibleInputConsumed[3]);
        }

        const int handCount = CountClockHandGears(level);
        const int lockedHands = CountLockedClockHands(level);
        machinePower = handCount > 0 ? static_cast<float>(lockedHands) / static_cast<float>(handCount) : 0.0f;
        if (HasArea(level.exitTrigger)) {
            const bool midnightLocked = handCount == 3 && lockedHands == handCount;
            const float targetGateBottom = midnightLocked
                ? level.exitTrigger.y
                : level.exitTrigger.y + level.exitTrigger.height;
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
                if (fourPlayerEnabled && player4Alive) {
                    TryLockRotaryLatch(latch, player4, player4Input.interactPressed);
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
                if (!IsPlayerCollisionLayer(boulder.layer)) continue;
                if (CheckCollisionCircleRec(boulder.center, boulder.radius, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const PhysicsWheel& wheel : level.physicsWheels) {
                if (!IsPlayerCollisionLayer(wheel.layer)) continue;
                if (CheckCollisionCircleRec(wheel.center, wheel.radius, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const Gear& gear : level.gears) {
                if (!IsPlayerCollisionLayer(gear.layer) || gear.mounting == GearMounting::Mounted) continue;
                if (CheckCollisionCircleRec(gear.center, gear.radius * GearOuterRadiusScale, button.rect)) {
                    heavyPlatePressed = true;
                    break;
                }
            }
            for (const Flywheel& flywheel : level.flywheels) {
                if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
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

    if (level.script == LevelScript::PortalLift) {
        for (PlatformLoopButtonLink& link : level.platformLoopButtonLinks) {
            if (link.buttonIndex >= 0 && link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[link.buttonIndex].pressed) {
                link.activated = true;
            }
        }
        const auto isButtonPowered = [&](int buttonIndex) {
            if (buttonIndex >= 0 && buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[buttonIndex].pressed) {
                return true;
            }
            return std::any_of(
                level.platformLoopButtonLinks.begin(),
                level.platformLoopButtonLinks.end(),
                [&](const PlatformLoopButtonLink& link) {
                    return link.buttonIndex == buttonIndex && link.activated;
                }
            );
        };

        const bool chamberLightPowered = !level.buttons.empty() && level.buttons.front().pressed;
        for (ButtonPlatformLink& link : level.buttonPlatformLinks) {
            link.active = link.buttonIndex >= 0 &&
                link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[link.buttonIndex].pressed;
        }
        for (ButtonPlatformLoop& loop : level.buttonPlatformLoops) {
            loop.active = loop.buttonIndex < 0 ||
                (loop.buttonIndex < static_cast<int>(level.buttons.size()) &&
                    level.buttons[loop.buttonIndex].pressed);
        }
        for (ButtonSpikeLink& link : level.buttonSpikeLinks) {
            link.active = link.buttonIndex >= 0 &&
                link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[link.buttonIndex].pressed;
        }
        for (const ButtonFanLink& link : level.buttonFanLinks) {
            const bool powered = isButtonPowered(link.buttonIndex);
            if (link.fanIndex >= 0 && link.fanIndex < static_cast<int>(level.fans.size())) {
                level.fans[link.fanIndex].power = powered ? link.poweredAmount : 0.0f;
            }
        }
        if (level.buttonExitLink.buttonIndex >= 0 &&
            isButtonPowered(level.buttonExitLink.buttonIndex)) {
            level.buttonExitLink.activated = true;
        }
        machinePower = chamberLightPowered ? 1.0f : 0.0f;
        if (HasArea(level.exitTrigger)) {
            const float targetGateBottom = level.buttonExitLink.activated
                ? level.exitTrigger.y
                : level.exitTrigger.y + level.exitTrigger.height;
            gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
        }
        return;
    }

    if (level.script == LevelScript::WaterEscape) {
        // The ruptured pipe at the base of the chamber runs continuously. The
        // shared fluid-fill settings define how quickly its water column rises.
        if (!won && !lost) {
            for (ButtonTrapDoorLink& link : level.buttonTrapDoorLinks) {
                if (link.buttonIndex >= 0 && link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                    level.buttons[link.buttonIndex].pressed) {
                    link.activated = true;
                }
                if (link.activated && link.trapDoorIndex >= 0 &&
                    link.trapDoorIndex < static_cast<int>(level.trapDoors.size())) {
                    TrapDoor& door = level.trapDoors[link.trapDoorIndex];
                    door.angle = MoveTowardsFloat(door.angle, link.openAngle, link.speed * dt);
                }
            }

            if (level.buttonExitLink.buttonIndex >= 0 &&
                level.buttonExitLink.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[level.buttonExitLink.buttonIndex].pressed) {
                level.buttonExitLink.activated = true;
            }

            FluidField* flood = GetValveFluid(level);
            if (HasValveFluidFill(level) && flood != nullptr && flood->cellSize > 0.0f) {
                const float targetMass = GetValveFluidTargetMass(level, *flood);
                const float remainingMass = fmaxf(0.0f, targetMass - GetFluidMass(*flood));
                const float massPerVerticalPixel = static_cast<float>(flood->gridColumns) / flood->cellSize;
                // Each deployed wooden door buffers the leak's pressure, but
                // its floor gap still lets a meaningful amount of water pass.
                float leakRate = level.valveFluidFill.riseRate;
                for (const ButtonTrapDoorLink& link : level.buttonTrapDoorLinks) {
                    if (link.activated) leakRate *= 0.72f;
                }
                AddCellularFluidMass(*flood, fminf(leakRate * massPerVerticalPixel * dt, remainingMass));
            }
        }

        machinePower = HasValveFluidFill(level) ? GetValveFluidFillProgress(level) : 0.0f;
        if (HasArea(level.exitTrigger)) {
            const float targetGateBottom = level.buttonExitLink.activated
                ? level.exitTrigger.y
                : level.exitTrigger.y + level.exitTrigger.height;
            gateBottom = MoveTowardsFloat(gateBottom, targetGateBottom, 190.0f * dt);
        }
        return;
    }

    if (level.script == LevelScript::ButtonSequence) {
        int activatedStages = 0;
        int totalStages = static_cast<int>(level.buttonTrapDoorLinks.size()) +
            static_cast<int>(level.buttonLadderLinks.size()) +
            (level.buttonExitLink.buttonIndex >= 0 ? 1 : 0);

        for (ButtonTrapDoorLink& link : level.buttonTrapDoorLinks) {
            if (link.buttonIndex >= 0 && link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[link.buttonIndex].pressed) {
                link.activated = true;
            }
            if (link.activated) {
                ++activatedStages;
                if (link.trapDoorIndex >= 0 && link.trapDoorIndex < static_cast<int>(level.trapDoors.size())) {
                    TrapDoor& trapDoor = level.trapDoors[link.trapDoorIndex];
                    trapDoor.angle = MoveTowardsFloat(trapDoor.angle, link.openAngle, link.speed * dt);
                }
            }
        }
        for (ButtonLadderLink& link : level.buttonLadderLinks) {
            if (link.buttonIndex >= 0 && link.buttonIndex < static_cast<int>(level.buttons.size()) &&
                level.buttons[link.buttonIndex].pressed) {
                link.activated = true;
            }
            if (link.activated) {
                link.revealProgress = MoveTowardsFloat(link.revealProgress, 1.0f, 2.4f * dt);
                ++activatedStages;
            }
        }
        if (level.buttonExitLink.buttonIndex >= 0 &&
            level.buttonExitLink.buttonIndex < static_cast<int>(level.buttons.size()) &&
            level.buttons[level.buttonExitLink.buttonIndex].pressed) {
            level.buttonExitLink.activated = true;
        }
        if (level.buttonExitLink.activated) ++activatedStages;

        machinePower = totalStages > 0
            ? static_cast<float>(activatedStages) / static_cast<float>(totalStages)
            : 0.0f;
        if (HasArea(level.exitTrigger)) {
            const float targetGateBottom = level.buttonExitLink.activated
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
        bool player4GrabbingWinch = fourPlayerEnabled && player4Alive && IsNearRect(player4.rect, machineWinch.rect, 18.0f) && player4Input.interactHeld && !flexibleInputConsumed[3];
        if (player4GrabbingWinch && !player1GrabbingWinch && !player2GrabbingWinch && !player3GrabbingWinch) {
            winchDelta = UpdateWinch(machineWinch, player4, player4Input.moveInput, player4Input.interactHeld, dt);
        }
        else if (player3GrabbingWinch && !player1GrabbingWinch && !player2GrabbingWinch) {
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
        else if (fourPlayerEnabled && player4Alive) {
            winchDelta = UpdateWinch(machineWinch, player4, 0.0f, false, dt);
        }
    }

    const float mechanicalDrive = GetMachinePower(machineWinch);
    GuideObject* factoryGenerator = nullptr;
    for (GuideObject& object : level.guideObjects) {
        if (object.type == GuideObjectType::Generator && object.active && !object.broken) {
            factoryGenerator = &object;
            break;
        }
    }

    const float generatorLoadScale = factoryGenerator != nullptr
        ? 1.0f / (1.0f + factoryGenerator->mechanicalLoad * 0.24f)
        : 1.0f;
    float spinAmount = (fabsf(winchDelta) * 3.0f + mechanicalDrive * 140.0f * dt) * generatorLoadScale;
    pulleyRotation += spinAmount;
    machinePhase += (0.35f + mechanicalDrive * 3.3f) * dt;

    if (factoryGenerator != nullptr) {
        factoryGenerator->mechanicalInputSpeed = dt > 0.0001f
            ? spinAmount * 1.7f / dt
            : 0.0f;
        machinePower = std::clamp(factoryGenerator->power.currentPower, 0.0f, 1.0f);
    }
    else {
        machinePower = mechanicalDrive;
    }

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
        if (!IsPlayerCollisionLayer(block.layer)) continue;
        solids.push_back(block.rect);
    }
    for (const Boulder& boulder : level.boulders) {
        if (!IsPlayerCollisionLayer(boulder.layer)) continue;
        solids.push_back(GetBoulderBounds(boulder));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        if (!IsPlayerCollisionLayer(wheel.layer)) continue;
        solids.push_back(GetWheelBounds(wheel));
    }
    for (const Gear& gear : level.gears) {
        if (!IsPlayerCollisionLayer(gear.layer)) continue;
        solids.push_back(GetGearBounds(gear));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
        solids.push_back(GetFlywheelBounds(flywheel));
    }
    for (const Screw& screw : level.screws) {
        if (!IsPlayerCollisionLayer(screw.layer)) continue;
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
        const float visualSpinSpeed = fminf(fan.strength * 1.8f, 1440.0f);
        fan.rotation += fan.power * visualSpinSpeed * dt;
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
    const Player* activePlayer4 = fourPlayerEnabled && player4Alive ? &player4 : nullptr;
    std::vector<Rectangle> obstacles =
        BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3, activePlayer4, true);
    std::vector<Rectangle> gasObstacles =
        BuildFluidObstacles(level, nullptr, nullptr, nullptr, nullptr, false);

    for (FluidField& fluid : level.fluids) {
        // Characters displace liquids and granular material, but a person should
        // not punch a player-sized hole into a volumetric gas concentration grid.
        const std::vector<Rectangle>& relevantObstacles = fluid.type == FluidType::Gas
            ? gasObstacles
            : obstacles;
        const std::vector<Rectangle> nearbyObstacles = FilterFluidObstacles(fluid, relevantObstacles);
        // Water and sand do not need a per-cell wind query. Gel particles and gas
        // concentration tiles retain full wind interaction.
        int flowPointCount = (fluid.type == FluidType::Water || fluid.type == FluidType::Sand) ?
            0 : GetFluidSimulationPointCount(fluid);
        std::vector<Vector2> externalFlow(static_cast<size_t>(flowPointCount));
        for (int index = 0; index < static_cast<int>(externalFlow.size()); index++) {
            externalFlow[index] = GetWindAtPoint(level, GetFluidSimulationPoint(fluid, index));
        }
        UpdateFluidField(fluid, nearbyObstacles, externalFlow, dt, SelectedFluidMode(advancedFluidSimulation));
    }
}

void Game::UpdatePhysicsObjects(float dt) {
    std::vector<Rectangle> solids = BuildSolids(level);
    std::array<std::vector<Rectangle>, WorldLayerCount> dynamicWorldSolids{
        solids, solids, solids
    };
    AppendFlexibleObjectColliders(dynamicWorldSolids[WorldLayerIndex(WorldLayer::Middleground)], level);
    for (const GuideObject& object : level.guideObjects) {
        if (object.body.type == BodyType::Dynamic && !object.broken && object.active) {
            dynamicWorldSolids[WorldLayerIndex(object.layer)].push_back(GetGuideObjectBounds(object));
        }
    }
    for (const Screw& screw : level.screws) {
        AppendScrewColliders(dynamicWorldSolids[WorldLayerIndex(screw.layer)], screw);
    }

    for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
        StoneBlock& block = level.stoneBlocks[i];
        const std::vector<Rectangle>& blockSolids = dynamicWorldSolids[WorldLayerIndex(block.layer)];

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
        ApplyScrewConveyor(block.rect, block.velocity, block.mass, block.layer, level.screws, dt);
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

    for (int i = 0; i < static_cast<int>(level.boulders.size()); i++) {
        Boulder& boulder = level.boulders[i];
        const std::vector<Rectangle>& boulderSolids = dynamicWorldSolids[WorldLayerIndex(boulder.layer)];

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

        const float previousBoulderFootY = boulder.center.y + boulder.radius;
        boulder.center.y += boulder.velocity.y * dt;
        ResolveBoulderCollisions(boulder, boulderSolids);
        ApplyScrewConveyor(boulder, boulder.radius, level.screws, dt);
        ResolveBoulderRampStanding(boulder, level.ramps, dt, previousBoulderFootY);
        ResolveBoulderTrapDoorStanding(boulder, level.trapDoors, dt);
        ResolveBoulderSeeSawStanding(boulder, level.seeSaws, dt);

        if (boulder.onGround && fabsf(boulder.velocity.x) > 0.0f) {
            float rollingSpeed = boulder.velocity.x / fmaxf(1.0f, boulder.radius) * RAD2DEG;
            boulder.angularVelocity = ApproachFloat(boulder.angularVelocity, rollingSpeed, 520.0f * dt);
        }
        boulder.rotation += boulder.angularVelocity * dt;
        DisturbFluids(level.fluids, boulder.center, boulder.radius * 1.35f, boulder.velocity, 2.0f, dt);
    }

    for (int i = 0; i < static_cast<int>(level.physicsWheels.size()); i++) {
        PhysicsWheel& wheel = level.physicsWheels[i];
        const std::vector<Rectangle>& wheelSolids = dynamicWorldSolids[WorldLayerIndex(wheel.layer)];

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

    for (int i = 0; i < static_cast<int>(level.gears.size()); i++) {
        Gear& gear = level.gears[i];
        const std::vector<Rectangle>& gearSolids = dynamicWorldSolids[WorldLayerIndex(gear.layer)];
        if (gear.mounting == GearMounting::Mounted) {
            gear.velocity = {0.0f, 0.0f};
            gear.onGround = false;
            ApplyGearMotorAndBrake(gear, dt);
            gear.rotation += gear.angularVelocity * dt;
            continue;
        }

        if (gear.onGround) {
            gear.velocity.x *= powf(0.18f, dt);
            gear.angularVelocity *= powf(0.14f, dt);
        }

        ApplyGearMotorAndBrake(gear, dt);

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
        ResolveRoundBodyRampStanding(gear, level.ramps, gear.radius * GearOuterRadiusScale, dt, 0.38f);
        ResolveRoundBodyTrapDoorStanding(gear, level.trapDoors, gear.radius * GearOuterRadiusScale, dt, 0.36f);
        ResolveRoundBodySeeSawStanding(gear, level.seeSaws, gear.radius * GearOuterRadiusScale, dt, 0.38f);

        if (gear.onGround && fabsf(gear.velocity.x) > 0.0f) {
            float rollingSpeed = gear.velocity.x / fmaxf(1.0f, gear.radius * GearOuterRadiusScale) * RAD2DEG;
            gear.angularVelocity = ApproachFloat(gear.angularVelocity, rollingSpeed, 620.0f * dt);
        }
        gear.rotation += gear.angularVelocity * dt;
        DisturbFluids(level.fluids, gear.center, gear.radius * 1.45f, gear.velocity, 2.1f, dt);
    }

    for (int i = 0; i < static_cast<int>(level.flywheels.size()); i++) {
        Flywheel& flywheel = level.flywheels[i];
        const std::vector<Rectangle>& flywheelSolids = dynamicWorldSolids[WorldLayerIndex(flywheel.layer)];
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

    for (Gear& gear : level.gears) {
        if (gear.mounting == GearMounting::Mounted) {
            gear.velocity = {0.0f, 0.0f};
        }
        if (gear.stopped) {
            gear.angularVelocity = 0.0f;
        }
    }

    for (StoneBlock& block : level.stoneBlocks) {
        ResolveStoneBlockPenetration(block, dynamicWorldSolids[WorldLayerIndex(block.layer)]);
        ResolveRampStanding(block.rect, block.velocity, block.onGround, level.ramps);
        ResolveTrapDoorStanding(block.rect, block.velocity, block.onGround, level.trapDoors);
        ResolveSeeSawStanding(block.rect, block.velocity, block.onGround, level.seeSaws);
    }
    for (Boulder& boulder : level.boulders) {
        ResolveBoulderCollisions(boulder, dynamicWorldSolids[WorldLayerIndex(boulder.layer)]);
        ResolveBoulderRampStanding(
            boulder,
            level.ramps,
            0.0f,
            boulder.center.y + boulder.radius
        );
        ResolveBoulderTrapDoorStanding(boulder, level.trapDoors, 0.0f);
        ResolveBoulderSeeSawStanding(boulder, level.seeSaws, 0.0f);
    }
    for (PhysicsWheel& wheel : level.physicsWheels) {
        ResolveWheelCollisions(wheel, dynamicWorldSolids[WorldLayerIndex(wheel.layer)]);
        ResolveWheelRampStanding(wheel, level.ramps, 0.0f);
        ResolveWheelTrapDoorStanding(wheel, level.trapDoors, 0.0f);
        ResolveWheelSeeSawStanding(wheel, level.seeSaws, 0.0f);
    }
    for (Gear& gear : level.gears) {
        if (gear.mounting == GearMounting::Mounted) continue;
        ResolveGearCollisions(gear, dynamicWorldSolids[WorldLayerIndex(gear.layer)]);
        ResolveRoundBodyRampStanding(gear, level.ramps, gear.radius * GearOuterRadiusScale, 0.0f, 0.0f);
        ResolveRoundBodyTrapDoorStanding(gear, level.trapDoors, gear.radius * GearOuterRadiusScale, 0.0f, 0.0f);
        ResolveRoundBodySeeSawStanding(gear, level.seeSaws, gear.radius * GearOuterRadiusScale, 0.0f, 0.0f);
    }
    for (Flywheel& flywheel : level.flywheels) {
        ResolveFlywheelCollisions(flywheel, dynamicWorldSolids[WorldLayerIndex(flywheel.layer)]);
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
        if (fourPlayerEnabled && player4Alive) {
            torque += GetSeeSawTorqueContribution(seeSaw, player4.rect, 1.0f);
        }
        for (const StoneBlock& block : level.stoneBlocks) {
            if (!IsPlayerCollisionLayer(block.layer)) continue;
            torque += GetSeeSawTorqueContribution(seeSaw, block.rect, block.mass);
        }
        for (const Boulder& boulder : level.boulders) {
            if (!IsPlayerCollisionLayer(boulder.layer)) continue;
            torque += GetSeeSawTorqueContribution(seeSaw, GetBoulderBounds(boulder), boulder.mass);
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (!IsPlayerCollisionLayer(wheel.layer)) continue;
            torque += GetSeeSawTorqueContribution(seeSaw, GetWheelBounds(wheel), wheel.mass);
        }
        for (const Gear& gear : level.gears) {
            if (!IsPlayerCollisionLayer(gear.layer) || gear.mounting == GearMounting::Mounted) continue;
            torque += GetSeeSawTorqueContribution(seeSaw, GetGearBounds(gear), gear.mass);
        }
        for (const Flywheel& flywheel : level.flywheels) {
            if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
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

    std::array<std::vector<Rectangle>, WorldLayerCount> guideWorldSolids{
        solids, solids, solids
    };
    AppendFlexibleObjectColliders(guideWorldSolids[WorldLayerIndex(WorldLayer::Middleground)], level);
    for (const StoneBlock& block : level.stoneBlocks) {
        guideWorldSolids[WorldLayerIndex(block.layer)].push_back(block.rect);
    }
    for (const Boulder& boulder : level.boulders) {
        guideWorldSolids[WorldLayerIndex(boulder.layer)].push_back(GetBoulderBounds(boulder));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        guideWorldSolids[WorldLayerIndex(wheel.layer)].push_back(GetWheelBounds(wheel));
    }
    for (const Gear& gear : level.gears) {
        guideWorldSolids[WorldLayerIndex(gear.layer)].push_back(GetGearBounds(gear));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        guideWorldSolids[WorldLayerIndex(flywheel.layer)].push_back(GetFlywheelBounds(flywheel));
    }
    std::array<Player*, 4> guidePlayers{
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
    };
    for (GuideObject& object : level.guideObjects) {
        if (object.type != GuideObjectType::GasMask || object.collected) continue;

        const Rectangle maskBounds = GetGuideObjectBounds(object);
        for (int playerIndex = 0; playerIndex < static_cast<int>(guidePlayers.size()); ++playerIndex) {
            Player* activePlayer = guidePlayers[playerIndex];
            if (activePlayer == nullptr || !CheckCollisionRecs(activePlayer->rect, maskBounds)) continue;

            object.collected = true;
            playerGasMasks[playerIndex] = true;
            playerAir[playerIndex] = 1.0f;
            playerAirWarningPhase[playerIndex] = 0.0f;
            toxinExposure[playerIndex] = 0.0f;
            break;
        }
    }
    Vector2 previousCheckpoint = checkpointRespawn;
    UpdateGuideObjects(level.guideObjects, guidePlayers, guideWorldSolids,
        Constants::Gravity, dt, checkpointRespawn);
    if (checkpointRespawn.x != previousCheckpoint.x || checkpointRespawn.y != previousCheckpoint.y) {
        checkpointActivated = true;
    }
}

void Game::UpdateButtons() {
    for (Button& button : level.buttons) {
        button.pressed = false;

        const Player* players[] = {
            playerAlive ? &player : nullptr,
            multiplayerEnabled && player2Alive ? &player2 : nullptr,
            threePlayerEnabled && player3Alive ? &player3 : nullptr,
            fourPlayerEnabled && player4Alive ? &player4 : nullptr
        };
        if (level.script != LevelScript::CounterweightRow &&
            level.script != LevelScript::ButtonSequence) {
            for (const Player* activePlayer : players) {
                if (activePlayer != nullptr && CheckCollisionRecs(button.rect, activePlayer->rect)) {
                    button.pressed = true;
                }
            }
        }

        for (const StoneBlock& block : level.stoneBlocks) {
            if (!IsPlayerCollisionLayer(block.layer)) continue;
            if (CheckCollisionRecs(button.rect, block.rect)) {
                button.pressed = true;
            }
        }
        for (const Boulder& boulder : level.boulders) {
            if (!IsPlayerCollisionLayer(boulder.layer)) continue;
            if (CheckCollisionCircleRec(boulder.center, boulder.radius, button.rect)) {
                button.pressed = true;
            }
        }
        for (const PhysicsWheel& wheel : level.physicsWheels) {
            if (!IsPlayerCollisionLayer(wheel.layer)) continue;
            if (CheckCollisionCircleRec(wheel.center, wheel.radius, button.rect)) {
                button.pressed = true;
            }
        }
        for (const Gear& gear : level.gears) {
            if (!IsPlayerCollisionLayer(gear.layer) || gear.mounting == GearMounting::Mounted) continue;
            if (CheckCollisionCircleRec(gear.center, gear.radius * GearOuterRadiusScale, button.rect)) {
                button.pressed = true;
            }
        }
        for (const Flywheel& flywheel : level.flywheels) {
            if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
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
        for (const GuideObject& object : level.guideObjects) {
            if (IsPlayerCollisionLayer(object.layer) && object.body.type == BodyType::Dynamic && !object.broken &&
                CheckCollisionRecs(button.rect, GetGuideObjectBounds(object))) {
                button.pressed = true;
            }
        }
    }
}

void Game::UpdatePortals() {
    const auto teleportRectangle = [](Rectangle& body, Vector2& velocity, const PortalPair& pair) {
        if (!CheckCollisionRecs(body, pair.entrance)) return;
        body.x = pair.exit.x + (pair.exit.width - body.width) * 0.5f;
        body.y = pair.exit.y + pair.exit.height + 3.0f;
        velocity.y = fmaxf(80.0f, velocity.y);
    };
    const auto teleportCircle = [](Vector2& center, Vector2& velocity, float radius, const PortalPair& pair) {
        if (!CheckCollisionCircleRec(center, radius, pair.entrance)) return;
        center.x = pair.exit.x + pair.exit.width * 0.5f;
        center.y = pair.exit.y + pair.exit.height + radius + 3.0f;
        velocity.y = fmaxf(80.0f, velocity.y);
    };

    for (const PortalPair& pair : level.portalPairs) {
        if (playerAlive) teleportRectangle(player.rect, player.velocity, pair);
        if (multiplayerEnabled && player2Alive) teleportRectangle(player2.rect, player2.velocity, pair);
        if (threePlayerEnabled && player3Alive) teleportRectangle(player3.rect, player3.velocity, pair);
        if (fourPlayerEnabled && player4Alive) teleportRectangle(player4.rect, player4.velocity, pair);
        for (StoneBlock& block : level.stoneBlocks) {
            if (IsPlayerCollisionLayer(block.layer)) teleportRectangle(block.rect, block.velocity, pair);
        }
        for (Boulder& boulder : level.boulders) {
            if (IsPlayerCollisionLayer(boulder.layer)) {
                teleportCircle(boulder.center, boulder.velocity, boulder.radius, pair);
            }
        }
        for (PhysicsWheel& wheel : level.physicsWheels) {
            if (IsPlayerCollisionLayer(wheel.layer)) {
                teleportCircle(wheel.center, wheel.velocity, wheel.radius, pair);
            }
        }
        for (Gear& gear : level.gears) {
            if (IsPlayerCollisionLayer(gear.layer) && gear.mounting == GearMounting::Dynamic) {
                teleportCircle(gear.center, gear.velocity, gear.radius * GearOuterRadiusScale, pair);
            }
        }
        for (Flywheel& flywheel : level.flywheels) {
            if (IsPlayerCollisionLayer(flywheel.layer)) {
                teleportCircle(flywheel.center, flywheel.velocity, flywheel.radius, pair);
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
        if (!IsPlayerCollisionLayer(block.layer)) continue;
        solids.push_back(block.rect);
    }
    for (const Boulder& boulder : level.boulders) {
        if (!IsPlayerCollisionLayer(boulder.layer)) continue;
        solids.push_back(GetBoulderBounds(boulder));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        if (!IsPlayerCollisionLayer(wheel.layer)) continue;
        solids.push_back(GetWheelBounds(wheel));
    }
    for (const Gear& gear : level.gears) {
        if (!IsPlayerCollisionLayer(gear.layer)) continue;
        solids.push_back(GetGearBounds(gear));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        if (!IsPlayerCollisionLayer(flywheel.layer)) continue;
        solids.push_back(GetFlywheelBounds(flywheel));
    }
    for (const Screw& screw : level.screws) {
        if (!IsPlayerCollisionLayer(screw.layer)) continue;
        AppendScrewColliders(solids, screw);
    }
    for (const GuideObject& object : level.guideObjects) {
        if (IsPlayerCollisionLayer(object.layer) && object.body.type == BodyType::Dynamic &&
            !object.broken && object.active) {
            solids.push_back(GetGuideObjectBounds(object));
        }
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

            const Rectangle worldBounds = level.worldBounds;
            if (arrow.rect.x + arrow.rect.width < worldBounds.x ||
                arrow.rect.x > worldBounds.x + worldBounds.width ||
                arrow.rect.y + arrow.rect.height < worldBounds.y ||
                arrow.rect.y > worldBounds.y + worldBounds.height) {
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
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
    };

    for (BreakableTile& tile : level.breakableTiles) {
        if (!tile.broken && !tile.cracking) {
            for (const Player* activePlayer : players) {
                if (activePlayer != nullptr && IsRectStandingOnTile(activePlayer->rect, tile.rect)) {
                    tile.cracking = true;
                    break;
                }
            }
            for (const Boulder& boulder : level.boulders) {
                if (!IsPlayerCollisionLayer(boulder.layer)) continue;
                float boulderBottom = boulder.center.y + boulder.radius;
                float supportInset = fminf(boulder.radius, tile.rect.width * 0.45f);
                bool centeredOnTile = boulder.center.x >= tile.rect.x + supportInset &&
                    boulder.center.x <= tile.rect.x + tile.rect.width - supportInset;
                if (centeredOnTile && fabsf(boulderBottom - tile.rect.y) <= 5.0f) {
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
    int defeatedPlayerIndex = 0;
    if (&defeatedPlayer == &player2) defeatedPlayerIndex = 1;
    else if (&defeatedPlayer == &player3) defeatedPlayerIndex = 2;
    else if (&defeatedPlayer == &player4) defeatedPlayerIndex = 3;
    playerGasMasks[defeatedPlayerIndex] = false;
    playerEnemyDamageGraceTimers[defeatedPlayerIndex] = 0.0f;
    const PlayerControllerSettings& controller = controllerSettings[defeatedPlayerIndex];
    const int gamepad = AvailableGamepad(controller);
    if (controller.vibration && gamepad >= 0) {
        const float vibrationStrength = screenShakeSetting == ScreenShakeSetting::Reduced ? 0.35f : 0.75f;
        SetGamepadVibration(gamepad, vibrationStrength, vibrationStrength, 0.18f);
    }

    if (!multiplayerEnabled) {
        if (checkpointActivated) {
            ResetPlayer(player);
            player.rect.x = checkpointRespawn.x;
            player.rect.y = checkpointRespawn.y;
            playerAlive = true;
            playerAir[0] = 1.0f;
            playerAirWarningPhase[0] = 0.0f;
            toxinExposure[0] = fminf(toxinExposure[0], 0.35f);
            respawnGraceTimer = 0.8f;
            return;
        }
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
    else if (&defeatedPlayer == &player4) {
        player4Alive = false;
        player4DeathRect = defeatedPlayer.rect;
        player4.velocity = {0.0f, 0.0f};
        player4.walking = false;
        player4.climbing = false;
    }

    lost = !playerAlive &&
        (!multiplayerEnabled || !player2Alive) &&
        (!threePlayerEnabled || !player3Alive) &&
        (!fourPlayerEnabled || !player4Alive);
}

void Game::CheckFailureConditions() {
    if (won || lost) return;
    if (respawnGraceTimer > 0.0f) return;

    Player* players[] = {
        playerAlive ? &player : nullptr,
        multiplayerEnabled && player2Alive ? &player2 : nullptr,
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
    };
    for (int playerIndex = 0; playerIndex < 4; ++playerIndex) {
        Player* activePlayer = players[playerIndex];
        if (activePlayer == nullptr) continue;

        const bool toxinSuffocation =
            level.script == LevelScript::NeurotoxinMaze &&
            SampleFluidAroundRectangle(level, FluidType::Gas, activePlayer->rect).density >= 0.025f;
        const bool underwaterSuffocation =
            level.script != LevelScript::NeurotoxinMaze &&
            IsPlayerHeadSubmerged(*activePlayer, level);
        if (suffocationKills && playerAir[playerIndex] <= 0.001f &&
            (toxinSuffocation || underwaterSuffocation)) {
            KillPlayer(*activePlayer);
            return;
        }

        if (activePlayer->rect.y > level.worldBounds.y + level.worldBounds.height ||
            (HasArea(level.spikeHazard) && CheckCollisionRecs(activePlayer->rect, level.spikeHazard))) {
            KillPlayer(*activePlayer);
            return;
        }

        for (const DirectionalSpikeHazard& hazard : level.directionalSpikeHazards) {
            if (CheckCollisionRecs(activePlayer->rect, hazard.rect)) {
                KillPlayer(*activePlayer);
                return;
            }
        }
        for (const ButtonSpikeLink& link : level.buttonSpikeLinks) {
            if (link.active && CheckCollisionRecs(activePlayer->rect, link.hazard.rect)) {
                KillPlayer(*activePlayer);
                return;
            }
        }
        for (const ButtonPlatformLoop& loop : level.buttonPlatformLoops) {
            if (!loop.active) continue;
            for (const Rectangle platform : loop.platforms) {
                for (const DirectionalSpikeHazard& hazard : GetPlatformSideSpikes(platform)) {
                    if (IsPlayerRunningIntoPlatformSpikes(*activePlayer, hazard)) {
                        KillPlayer(*activePlayer);
                        return;
                    }
                }
            }
        }

        for (const HangingWeight& weight : level.weights) {
            if (CheckCollisionRecs(activePlayer->rect, weight.rect)) {
                KillPlayer(*activePlayer);
                return;
            }
        }

        for (const Enemy& enemy : level.enemies) {
            if (CheckCollisionRecs(activePlayer->rect, enemy.rect)) {
                if (playerGasMasks[playerIndex]) {
                    playerGasMasks[playerIndex] = false;
                    playerAir[playerIndex] = 1.0f;
                    playerAirWarningPhase[playerIndex] = 0.0f;
                    toxinExposure[playerIndex] = 0.0f;
                    playerEnemyDamageGraceTimers[playerIndex] = 0.85f;
                    const float playerCenter = activePlayer->rect.x + activePlayer->rect.width * 0.5f;
                    const float enemyCenter = enemy.rect.x + enemy.rect.width * 0.5f;
                    activePlayer->velocity.x = playerCenter < enemyCenter ? -260.0f : 260.0f;
                    activePlayer->velocity.y = -220.0f;
                    return;
                }
                if (playerEnemyDamageGraceTimers[playerIndex] > 0.0f) continue;
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

        for (const GuideObject& object : level.guideObjects) {
            if (IsGuideObjectHazardTouchingPlayer(object, activePlayer->rect)) {
                KillPlayer(*activePlayer);
                return;
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
        threePlayerEnabled && player3Alive ? &player3 : nullptr,
        fourPlayerEnabled && player4Alive ? &player4 : nullptr
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
        achievements.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        return;
    }

    if (mode == GameMode::CharacterSelect) {
        DrawCharacterSelect();
        achievements.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        return;
    }

    if (mode == GameMode::Overworld) {
        DrawOverworld();
        if (quitConfirmationOpen) {
            DrawQuitConfirmation();
        }
        achievements.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        return;
    }

    DrawGameplay();

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

    achievements.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
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

    // Frame the title and controls on the left, leaving the machinery as a
    // dedicated animated stage instead of putting the menu on top of it.
    DrawTilesetSolid(industrialTiles, {0, 790, static_cast<float>(Constants::ScreenWidth), 110}, WHITE);
    DrawTilesetSolid(industrialTiles, {58, 72, 580, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {700, 430, 820, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {520, 640, 1000, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {1030, 145, 490, 32}, Fade(WHITE, 0.95f));
    DrawTilesetSolid(industrialTiles, {1488, 145, 32, 645}, Fade(WHITE, 0.9f));

    DrawRectangle(58, 116, 580, 318, Fade(BLACK, 0.28f));
    DrawRectangleLinesEx({58, 116, 580, 318}, 2.0f, Fade(RAYWHITE, 0.16f));

    Vector2 titlePosition{92.0f, 137.0f};
    constexpr int titleSize = 84;
    DrawText("POWER", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 6.0f), titleSize, Fade(BLACK, 0.55f));
    DrawText("PULLEY", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 91.0f), titleSize, Fade(BLACK, 0.55f));
    DrawText("PANIC", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 176.0f), titleSize, Fade(BLACK, 0.55f));
    DrawText("POWER", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y), titleSize, RAYWHITE);
    DrawText("PULLEY", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 85.0f), titleSize, ORANGE);
    DrawText("PANIC", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 170.0f), titleSize, RAYWHITE);
    DrawText("Spin to Win", 98, 394, 24, Fade(RAYWHITE, 0.82f));

    const float time = static_cast<float>(GetTime());
    float rotation = time * 92.0f;
    constexpr Vector2 upperLeftPulley{875, 256};
    constexpr Vector2 centerPulley{1108, 365};
    constexpr Vector2 upperRightPulley{1370, 248};
    constexpr Vector2 lowerPulley{1282, 535};
    float titleRopeOffset = rotation * DEG2RAD * 58.0f;
    DrawPulleyRope(upperLeftPulley, 62, centerPulley, 50, -1.0f, 8.0f, BROWN, titleRopeOffset);
    DrawPulleyRope(centerPulley, 50, upperRightPulley, 74, 1.0f, 8.0f, BROWN, titleRopeOffset);
    DrawPulleyRope(lowerPulley, 42, upperRightPulley, 74, 1.0f, 8.0f, BROWN, titleRopeOffset);
    float counterweightRopeX = upperRightPulley.x - 74.0f;
    float counterweightY = 664.0f + sinf(time * 1.35f) * 34.0f;
    DrawRope({counterweightRopeX, upperRightPulley.y}, {counterweightRopeX, counterweightY}, 6.0f, titleRopeOffset);
    DrawPulley(upperLeftPulley, 62, rotation, RAYWHITE);
    DrawPulley(centerPulley, 50, rotation * -1.15f, RAYWHITE);
    DrawPulley(upperRightPulley, 74, rotation * 0.72f, RAYWHITE);
    DrawPulley(lowerPulley, 42, rotation * 1.35f, RAYWHITE);

    Rectangle counterweight{counterweightRopeX - 27.0f, counterweightY, 54, 66};
    DrawRectangleRec(counterweight, GRAY);
    DrawRectangleLinesEx(counterweight, 2.0f, BLACK);

    // A sampling of real physics objects gives the scene more of the playful,
    // kinetic character of the levels.
    PhysicsWheel wheel{};
    wheel.center = {760.0f, 598.0f};
    wheel.radius = 35.0f;
    wheel.rotation = -rotation * 1.7f;
    DrawPhysicsWheel(wheel);

    Gear gear{};
    gear.center = {846.0f, 590.0f};
    gear.radius = 43.0f;
    gear.rotation = rotation * 1.25f;
    gear.mounting = GearMounting::Mounted;
    DrawGear(gear);

    Flywheel flywheel{};
    flywheel.center = {952.0f, 582.0f};
    flywheel.radius = 50.0f;
    flywheel.rotation = -rotation * 0.62f;
    DrawFlywheel(flywheel);

    SeeSaw seeSaw{};
    seeSaw.pivot = {1160.0f, 748.0f};
    seeSaw.length = 250.0f;
    seeSaw.angle = sinf(time * 1.15f) * 11.0f;
    DrawSeeSaw(seeSaw);

    Pinwheel pinwheel{};
    pinwheel.center = {1432.0f, 550.0f};
    pinwheel.radius = 30.0f;
    pinwheel.rotation = rotation * 2.3f;
    DrawPinwheel(pinwheel);

    auto patrolPosition = [time](float minX, float maxX, float speed, float phase, bool& facingRight) {
        const float distance = maxX - minX;
        const float travel = fmodf(time * speed + phase, distance * 2.0f);
        facingRight = travel < distance;
        return facingRight ? minX + travel : maxX - (travel - distance);
    };
    auto drawTitlePlayer = [&](int character, float minX, float maxX, float y, float speed, float phase) {
        Player runner{};
        runner.facingRight = true;
        runner.rect = {
            patrolPosition(minX, maxX, speed, phase, runner.facingRight),
            y,
            31.0f,
            40.0f
        };
        runner.walking = true;
        runner.animationTimer = time + phase / fmaxf(speed, 1.0f);
        if (character < 3) {
            DrawPlayer(runner, playerSpritesTexture, character);
        }
        else {
            DrawPlayer(runner, playerFourSpritesTexture, 0, 37.0f, 47.0f, 1, 64.0f, 4.0f, 17.0f);
        }
    };

    drawTitlePlayer(0, 715.0f, 1015.0f, 390.0f, 74.0f, 0.0f);
    drawTitlePlayer(1, 1045.0f, 1450.0f, 105.0f, 92.0f, 120.0f);
    drawTitlePlayer(2, 535.0f, 1000.0f, 600.0f, 86.0f, 260.0f);
    drawTitlePlayer(3, 1030.0f, 1450.0f, 600.0f, 101.0f, 410.0f);

    bool robotFacingRight = false;
    Enemy titleRobot{};
    titleRobot.rect = {
        patrolPosition(565.0f, 1430.0f, 118.0f, 680.0f, robotFacingRight),
        742.0f,
        48.0f,
        48.0f
    };
    titleRobot.facingRight = robotFacingRight;
    titleRobot.walking = true;
    DrawEnemy(titleRobot, enemyPlaceholderTexture);

    Boulder boulder{};
    bool boulderMovingRight = true;
    boulder.center = {
        patrolPosition(540.0f, 1450.0f, 66.0f, 90.0f, boulderMovingRight),
        760.0f
    };
    boulder.radius = 28.0f;
    boulder.rotation = (boulderMovingRight ? 1.0f : -1.0f) * rotation * 1.8f;
    DrawBoulder(boulder);

    DrawRectangle(68, 464, 434, 294, Fade(BLACK, 0.34f));
    DrawRectangleLinesEx({68, 464, 434, 294}, 2.0f, Fade(RAYWHITE, 0.17f));

    std::vector<MenuButton> buttons;
    if (titleModeMenuOpen) {
        DrawText("Select Mode", 92, 477, 20, Fade(RAYWHITE, 0.86f));
        buttons = {
            {{92, 506, 386, 40}, "Single Player"},
            {{92, 555, 386, 40}, "2 Players"},
            {{92, 604, 386, 40}, "3 Players"},
            {{92, 653, 386, 40}, "4 Players"},
            {{92, 702, 386, 40}, "Back"}
        };
    }
    else {
        buttons = {
            {{92, 490, 386, 40}, "New Game"},
            {{92, 539, 386, 40}, "Continue"},
            {{92, 588, 386, 40}, "Load Custom"},
            {{92, 637, 386, 40}, "Settings"},
            {{92, 686, 386, 40}, "Quit Game"}
        };
    }

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    if (!menuMessage.empty()) {
        DrawText(menuMessage.c_str(), 76, 766, 18, ORANGE);
    }
}

void Game::DrawCharacterSelect() {
    DrawTilesetBackgroundFill(
        industrialBackground,
        {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)},
        Fade(WHITE, 0.78f)
    );
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(Color{9, 14, 20, 255}, 0.58f));
    DrawTilesetSolid(industrialTiles, {0, 842, static_cast<float>(Constants::ScreenWidth), 58}, WHITE);

    DrawCenteredText("CHOOSE YOUR CHARACTER", Constants::ScreenWidth / 2, 58, 52, RAYWHITE);
    DrawCenteredText(
        TextFormat("%d PLAYER%s", characterSelectPlayerCount, characterSelectPlayerCount == 1 ? "" : "S"),
        Constants::ScreenWidth / 2,
        122,
        24,
        ORANGE
    );
    DrawCenteredText(
        "Use movement keys or controller D-pad to choose; interact or Jump to ready up",
        Constants::ScreenWidth / 2,
        166,
        20,
        LIGHTGRAY
    );

    constexpr float cardWidth = 260.0f;
    constexpr float cardGap = 28.0f;
    constexpr float cardsX = 238.0f;
    for (int characterIndex = 0; characterIndex < kCharacterCount; characterIndex++) {
        Rectangle card{
            cardsX + characterIndex * (cardWidth + cardGap),
            235.0f,
            cardWidth,
            340.0f
        };
        DrawRectangleRounded(card, 0.06f, 8, Color{24, 31, 39, 244});
        DrawRectangleRoundedLinesEx(card, 0.06f, 8, 2.0f, Fade(RAYWHITE, 0.34f));

        Rectangle portrait{card.x + 34.0f, card.y + 38.0f, 192.0f, 192.0f};
        DrawRectangleRounded(portrait, 0.08f, 8, Color{12, 16, 21, 255});
        DrawRectangleRoundedLinesEx(portrait, 0.08f, 8, 2.0f, Fade(RAYWHITE, 0.16f));

        if (characterIndex < 3 && playerSpritesTexture.id > 0) {
            DrawTexturePro(
                playerSpritesTexture,
                {37.0f, characterIndex * 47.0f, 37.0f, 47.0f},
                {portrait.x + 20.5f, portrait.y - 26.0f, 151.0f, 192.0f},
                {0, 0},
                0.0f,
                WHITE
            );
        }
        else if (characterIndex == 3 && playerFourSpritesTexture.id > 0) {
            DrawTexturePro(
                playerFourSpritesTexture,
                {68.0f, 17.0f, 37.0f, 47.0f},
                {portrait.x + 20.5f, portrait.y - 26.0f, 151.0f, 192.0f},
                {0, 0},
                0.0f,
                WHITE
            );
        }
        else {
            DrawCenteredText("ART MISSING", static_cast<int>(card.x + card.width * 0.5f),
                static_cast<int>(card.y + 130.0f), 18, ORANGE);
        }

        DrawCenteredText(
            kCharacterNames[characterIndex],
            static_cast<int>(card.x + card.width * 0.5f),
            static_cast<int>(card.y + 252.0f),
            24,
            RAYWHITE
        );

        int badgeX = static_cast<int>(card.x + 42.0f);
        for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
            if (selectedCharacters[playerIndex] != characterIndex) continue;
            Color playerColor = kPlayerSelectColors[playerIndex];
            DrawRectangleLinesEx(card, 5.0f, playerColor);
            DrawCircle(badgeX, static_cast<int>(card.y + 310.0f), 20.0f, playerColor);
            DrawCenteredText(TextFormat("P%d", playerIndex + 1), badgeX,
                static_cast<int>(card.y + 301.0f), 18, BLACK);
            badgeX += 52;
        }
    }

    std::array<std::string, 4> controlLabels{};
    for (int playerIndex = 0; playerIndex < 4; ++playerIndex) {
        const PlayerKeyBindings& bindings = playerBindings[playerIndex];
        controlLabels[playerIndex] =
            KeyLabel(bindings.left) + " / " + KeyLabel(bindings.right) +
            "    " + KeyLabel(bindings.interact) + " to ready";
    }
    const float playerButtonsWidth =
        characterSelectPlayerCount * 220.0f + (characterSelectPlayerCount - 1) * 40.0f;
    const float playerButtonsX = (Constants::ScreenWidth - playerButtonsWidth) * 0.5f;
    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
        Rectangle playerButton{
            playerButtonsX + playerIndex * 260.0f,
            635.0f,
            220.0f,
            52.0f
        };
        const bool focused = characterSelectFocusPlayer == playerIndex;
        const Color playerColor = kPlayerSelectColors[playerIndex];
        DrawRectangleRounded(playerButton, 0.15f, 8,
            focused ? Fade(playerColor, 0.34f) : Color{24, 31, 39, 244});
        DrawRectangleRoundedLinesEx(playerButton, 0.15f, 8, focused ? 3.0f : 1.0f,
            focused ? playerColor : Fade(RAYWHITE, 0.30f));
        DrawCenteredText(
            TextFormat("PLAYER %d: %s", playerIndex + 1, kCharacterNames[selectedCharacters[playerIndex]]),
            static_cast<int>(playerButton.x + playerButton.width * 0.5f),
            static_cast<int>(playerButton.y + 15.0f),
            18,
            RAYWHITE
        );

        Rectangle readyButton{playerButton.x, 696.0f, playerButton.width, 42.0f};
        DrawRectangleRounded(readyButton, 0.18f, 8,
            characterSelectReady[playerIndex] ? Fade(GREEN, 0.75f) : Fade(BLACK, 0.52f));
        DrawRectangleRoundedLinesEx(readyButton, 0.18f, 8, 2.0f,
            characterSelectReady[playerIndex] ? GREEN : Fade(RAYWHITE, 0.30f));
        DrawCenteredText(
            characterSelectReady[playerIndex] ? "READY" : controlLabels[playerIndex].c_str(),
            static_cast<int>(readyButton.x + readyButton.width * 0.5f),
            static_cast<int>(readyButton.y + 11.0f),
            17,
            characterSelectReady[playerIndex] ? RAYWHITE : LIGHTGRAY
        );
    }

    bool allReady = true;
    for (int playerIndex = 0; playerIndex < characterSelectPlayerCount; playerIndex++) {
        allReady = allReady && characterSelectReady[playerIndex];
    }

    MenuButton backButton{{100.0f, 782.0f, 220.0f, 48.0f}, "Back"};
    MenuButton continueButton{{625.0f, 782.0f, 350.0f, 48.0f}, "Continue", allReady};
    DrawMenuButton(backButton);
    DrawMenuButton(continueButton);
    if (allReady) {
        DrawCenteredText("Press Enter or controller Start to continue",
            Constants::ScreenWidth / 2, 748, 20, GREEN);
    }
    if (!menuMessage.empty()) {
        DrawCenteredText(menuMessage.c_str(), Constants::ScreenWidth / 2, 850, 18, ORANGE);
    }
}

void Game::DrawOverworld() {
    DrawTilesetBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.34f));

    DrawRectangle(0, 690, Constants::ScreenWidth, 235, Color{20, 24, 28, 215});
    for (int x = 0; x < Constants::ScreenWidth; x += 70) {
        DrawLineEx({static_cast<float>(x), 690.0f}, {static_cast<float>(x + 120), 925.0f}, 2.0f, Fade(BLACK, 0.22f));
    }

    constexpr const char* worldNames[3]{
        "FACTORY DISTRICT",
        "WENDI'S WORKSHOP",
        "TEST WORLD"
    };
    constexpr const char* worldDescriptions[3]{
        "World 1  /  Main Campaign",
        "World 2  /  Wendi's Levels",
        "Tests & References"
    };
    selectedOverworldWorld = std::clamp(selectedOverworldWorld, 0, 2);
    DrawText(worldNames[selectedOverworldWorld], 95, 70, 50, RAYWHITE);
    DrawText(worldDescriptions[selectedOverworldWorld], 100, 128, 25, ORANGE);

    std::vector<MenuButton> worldButtons{
        {{770, 88, 230, 44}, "World 1"},
        {{1012, 88, 230, 44}, "World 2"},
        {{1360, 24, 150, 38}, "Test World"}
    };
    for (int worldIndex = 0; worldIndex < static_cast<int>(worldButtons.size()); worldIndex++) {
        const MenuButton& button = worldButtons[worldIndex];
        if (worldIndex == selectedOverworldWorld) {
            DrawRectangleRec(button.rect, Fade(ORANGE, 0.35f));
            DrawRectangleLinesEx(button.rect, 3.0f, ORANGE);
            DrawCenteredText(
                button.text,
                static_cast<int>(button.rect.x + button.rect.width * 0.5f),
                static_cast<int>(button.rect.y + 11.0f),
                20,
                RAYWHITE
            );
        }
        else {
            DrawMenuButton(button);
        }
    }

    DrawCircle(150, 250, 70, Fade(BLACK, 0.22f));
    DrawCircleLines(150, 250, 70, Fade(RAYWHITE, 0.16f));
    DrawLineEx({110, 250}, {190, 250}, 6.0f, Fade(RAYWHITE, 0.16f));
    DrawLineEx({150, 210}, {150, 290}, 6.0f, Fade(RAYWHITE, 0.16f));
    DrawCircle(1420, 165, 105, Fade(BLACK, 0.18f));
    DrawCircleLines(1420, 165, 105, Fade(RAYWHITE, 0.14f));

    if (overworldNodes.empty()) {
        DrawCenteredText("Level-select data is unavailable", Constants::ScreenWidth / 2, 430, 30, RAYWHITE);
        return;
    }
    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);

    for (const OverworldPath& path : overworldPaths) {
        if (path.fromNode < 0 || path.toNode < 0 ||
            path.fromNode >= static_cast<int>(overworldNodes.size()) ||
            path.toNode >= static_cast<int>(overworldNodes.size())) {
            continue;
        }
        const OverworldNode& from = overworldNodes[path.fromNode];
        const OverworldNode& to = overworldNodes[path.toNode];
        if (from.world != selectedOverworldWorld || to.world != selectedOverworldWorld) {
            continue;
        }
        bool powered = from.unlocked && to.unlocked;
        Color pipeColor = powered ? Color{178, 128, 45, 255} : Color{75, 82, 88, 255};
        Color innerColor = powered ? ORANGE : Color{34, 40, 46, 255};

        DrawLineEx(from.position, to.position, 26.0f, Fade(BLACK, 0.35f));
        DrawLineEx(from.position, to.position, 18.0f, pipeColor);
        DrawLineEx(from.position, to.position, 5.0f, innerColor);
    }

    for (int i = 0; i < static_cast<int>(overworldNodes.size()); i++) {
        const OverworldNode& node = overworldNodes[i];
        if (node.world != selectedOverworldWorld) continue;
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

    Rectangle panel{1030, 560, 460, 170};
    DrawRectangleRec(panel, Color{20, 27, 34, 235});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.45f));

    const OverworldNode& selectedNode = overworldNodes[selectedOverworldNode];
    DrawText(selectedNode.name.c_str(), 1060, 583, 27, RAYWHITE);
    DrawText(TextFormat("Node %s", selectedNode.label.c_str()), 1060, 620, 20, ORANGE);
    DrawText(selectedNode.unlocked ? "Status: Unlocked" : "Status: Locked", 1060, 652, 20, selectedNode.unlocked ? GREEN : LIGHTGRAY);
    DrawText(selectedNode.unlocked ? "Enter / click starts the level" : "Complete earlier levels to unlock", 1060, 684, 20, LIGHTGRAY);

    std::vector<MenuButton> buttons{
        {{1240, 760, 250, 46}, "Back to Title"},
        {{1240, 816, 250, 46}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    DrawText("A/D or arrows select    Q/E switch worlds    Enter starts", 95, 825, 20, LIGHTGRAY);
    DrawText("Shoulders switch worlds    Esc returns    ` opens console", 95, 858, 20, LIGHTGRAY);

    if (!menuMessage.empty()) {
        DrawWrappedText(menuMessage, {555, 714, 630, 52}, 20, 4, ORANGE);
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
        std::string action;
        std::string input;
    };

    std::vector<ControlRow> rows;
    rows.reserve(19);
    for (int playerIndex = 0; playerIndex < 4; ++playerIndex) {
        const PlayerKeyBindings& bindings = playerBindings[playerIndex];
        const PlayerControllerSettings& controller = controllerSettings[playerIndex];
        const std::string playerLabel = "Player " + std::to_string(playerIndex + 1);
        rows.push_back({
            playerLabel + " Move",
            KeyLabel(bindings.left) + " / " + KeyLabel(bindings.right)
        });
        rows.push_back({
            playerLabel + " Jump",
            KeyLabel(bindings.up) + " / " + KeyLabel(bindings.jump)
        });
        rows.push_back({
            playerLabel + " Interact",
            KeyLabel(bindings.interact)
        });
        rows.push_back({
            playerLabel + " Controller",
            ControllerDeviceLabel(controller.gamepad) + " | " +
                GamepadButtonLabel(controller.jump) + " / " +
                GamepadButtonLabel(controller.interact)
        });
    }
    rows.push_back({"Swim / Climb", "Each player's Up / Down"});
    rows.push_back({"Controller Move", "Left stick / D-pad"});
    rows.push_back({"Pause / Resume", "Esc / Enter / Start"});

    int startY = 285;
    int rowHeight = 25;
    int actionX = 525;
    int inputX = 795;
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        int y = startY + i * rowHeight;
        Color rowColor = i % 2 == 0 ? Fade(RAYWHITE, 0.06f) : Fade(RAYWHITE, 0.025f);
        DrawRectangle(500, y - 4, 600, 30, rowColor);
        DrawText(rows[i].action.c_str(), actionX, y, 20, LIGHTGRAY);
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
        constexpr const char* playerLabels[] = {"Player 1", "Player 2", "Player 3", "Player 4"};
        constexpr const char* inputLabels[] = {"Keyboard", "Controller"};
        for (int i = 0; i < static_cast<int>(layout.playerTabs.size()); ++i) {
            DrawMenuButton({layout.playerTabs[i], playerLabels[i]});
            if (settingsSelectedPlayer == i) {
                DrawRectangle(
                    static_cast<int>(layout.playerTabs[i].x + 10.0f),
                    static_cast<int>(layout.playerTabs[i].y + layout.playerTabs[i].height - 3.0f),
                    static_cast<int>(layout.playerTabs[i].width - 20.0f),
                    3,
                    kPlayerSelectColors[i]
                );
            }
        }
        for (int i = 0; i < static_cast<int>(layout.inputTabs.size()); ++i) {
            DrawMenuButton({layout.inputTabs[i], inputLabels[i]});
            if (static_cast<int>(settingsControlsInputView) == i) {
                DrawRectangle(
                    static_cast<int>(layout.inputTabs[i].x + 10.0f),
                    static_cast<int>(layout.inputTabs[i].y + layout.inputTabs[i].height - 3.0f),
                    static_cast<int>(layout.inputTabs[i].width - 20.0f),
                    3,
                    ORANGE
                );
            }
        }

        if (settingsControlsInputView == ControlsInputView::Keyboard) {
            const char* actions[] = {"Move Left", "Move Right", "Move Up", "Move Down", "Jump", "Interact"};
            const PlayerKeyBindings& bindings = pendingPlayerBindings[settingsSelectedPlayer];
            const KeyboardKey keys[] = {
                bindings.left,
                bindings.right,
                bindings.up,
                bindings.down,
                bindings.jump,
                bindings.interact
            };
            for (int i = 0; i < 6; ++i) {
                const std::string keyName = settingsBindingCapture == i ? "Press a key..." : KeyLabel(keys[i]);
                DrawMenuButton({layout.controlRows[i], TextFormat("%s: %s", actions[i], keyName.c_str())});
            }
        }
        else {
            const PlayerControllerSettings& controller = pendingControllerSettings[settingsSelectedPlayer];
            const int gamepad = AvailableGamepad(controller);
            const std::string deviceLabel = ControllerDeviceLabel(controller.gamepad);
            DrawMenuButton({
                layout.controlRows[0],
                TextFormat("Device: %s", deviceLabel.c_str())
            });

            const char* jumpLabel = settingsGamepadBindingCapture == 0
                ? "Press a controller button..."
                : GamepadButtonLabel(controller.jump);
            const char* interactLabel = settingsGamepadBindingCapture == 1
                ? "Press a controller button..."
                : GamepadButtonLabel(controller.interact);
            DrawMenuButton({
                layout.controlRows[1],
                TextFormat("Jump: %s", jumpLabel),
                gamepad >= 0
            });
            DrawMenuButton({
                layout.controlRows[2],
                TextFormat("Interact: %s", interactLabel),
                gamepad >= 0
            });
            DrawMenuButton({
                layout.controlRows[3],
                TextFormat("Vibration: %s", OnOffLabel(controller.vibration))
            });

            const std::string status = controller.gamepad < 0
                ? "No controller assigned"
                : deviceLabel + (gamepad >= 0 ? " connected" : " not connected");
            DrawCenteredText(
                status.c_str(),
                panelCenterX,
                static_cast<int>(layout.panel.y + 404.0f),
                18,
                gamepad >= 0 ? LIME : LIGHTGRAY
            );
        }
    }
    else {
        DrawMenuButton({layout.controls[0], TextFormat("Screen Shake: %s", ScreenShakeLabel(pendingScreenShakeSetting))});
        DrawMenuButton({layout.controls[1], TextFormat("Reduced Flashing: %s", OnOffLabel(pendingReducedFlashing))});
        DrawMenuButton({layout.controls[2], TextFormat("High Contrast: %s", OnOffLabel(pendingHighContrast))});
        DrawMenuButton({layout.controls[3], TextFormat("Colorblind Mode: %s", ColorblindLabel(pendingColorblindSetting))});
        DrawMenuButton({layout.controls[4], TextFormat("Object Tooltips: %s", OnOffLabel(pendingObjectTooltipsEnabled))});
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
        case SettingsDropdown::ControllerDevice: anchor = layout.controlRows[0]; optionCount = 5; break;
        default: break;
        }

        const DropdownLayout dropdown = GetDropdownLayout(anchor, optionCount, columns);
        DrawRectangleRec(dropdown.panel, Color{17, 22, 28, 255});
        DrawRectangleLinesEx(dropdown.panel, 1.5f, Fade(RAYWHITE, 0.35f));
        for (int i = 0; i < optionCount; ++i) {
            const char* label = "";
            std::string dynamicLabel;
            switch (settingsDropdown) {
            case SettingsDropdown::WindowMode: label = WindowModeLabel(static_cast<WindowModeSetting>(i)); break;
            case SettingsDropdown::Resolution: label = GetResolutionPreset(i).label; break;
            case SettingsDropdown::FrameRate: label = kFrameRateLabels[i]; break;
            case SettingsDropdown::UiScale: label = kUiScaleLabels[i]; break;
            case SettingsDropdown::ScreenShake: label = ScreenShakeLabel(static_cast<ScreenShakeSetting>(i)); break;
            case SettingsDropdown::ColorblindMode: label = ColorblindLabel(static_cast<ColorblindSetting>(i)); break;
            case SettingsDropdown::ControllerDevice:
                dynamicLabel = ControllerDeviceLabel(i - 1);
                if (i > 0 && !IsGamepadAvailable(i - 1)) dynamicLabel += " (Not Connected)";
                label = dynamicLabel.c_str();
                break;
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

void Game::DrawNeurotoxinInfrastructure() {
    const bool active = !level.valve.opened;
    const Color outline{22, 27, 29, 255};
    const Color pipe{74, 82, 80, 255};
    const Color pipeHighlight{137, 146, 137, 255};
    const Color toxin{119, 212, 89, 255};
    const Color stateColor = active ? toxin : Color{83, 104, 91, 255};

    const std::array<Vector2, 5> pipePoints{
        level.toxinLeak.source,
        Vector2{210.0f, level.toxinLeak.source.y},
        Vector2{210.0f, level.valve.center.y},
        Vector2{level.valve.center.x + level.valve.radius + 28.0f, level.valve.center.y},
        Vector2{level.valve.center.x + level.valve.radius, level.valve.center.y}
    };
    for (int index = 0; index + 1 < static_cast<int>(pipePoints.size()); ++index) {
        DrawLineEx(pipePoints[index], pipePoints[index + 1], 19.0f, outline);
        DrawLineEx(pipePoints[index], pipePoints[index + 1], 13.0f, pipe);
        DrawLineEx(
            {pipePoints[index].x - 1.5f, pipePoints[index].y - 1.5f},
            {pipePoints[index + 1].x - 1.5f, pipePoints[index + 1].y - 1.5f},
            2.0f,
            pipeHighlight
        );
        if (active) DrawLineEx(pipePoints[index], pipePoints[index + 1], 4.0f, Fade(stateColor, 0.80f));
    }
    for (Vector2 joint : pipePoints) {
        DrawCircleV(joint, 12.0f, outline);
        DrawCircleV(joint, 8.0f, pipe);
        DrawCircleV(joint, 3.0f, stateColor);
    }

    // Ruptured outlet beside the entrance. The simulated gas is emitted from
    // the dark mouth while these rings provide a stable visual landmark.
    Vector2 leak = level.toxinLeak.source;
    DrawRectangleRec({leak.x - 28.0f, leak.y - 13.0f, 32.0f, 26.0f}, outline);
    DrawRectangleRec({leak.x - 24.0f, leak.y - 9.0f, 26.0f, 18.0f}, pipe);
    DrawCircleV(leak, 16.0f, outline);
    DrawCircleV(leak, 11.0f, Color{14, 22, 18, 255});
    if (active) {
        const float pulse = 0.72f + sinf(toxinLevelTimer * 7.0f) * 0.16f;
        DrawCircleV({leak.x + 6.0f, leak.y - 8.0f}, 18.0f, Fade(toxin, 0.16f * pulse));
        DrawCircleV({leak.x + 12.0f, leak.y - 22.0f}, 24.0f, Fade(toxin, 0.10f * pulse));
    }

    const bool playerNear =
        (playerAlive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player.rect)) ||
        (multiplayerEnabled && player2Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player2.rect)) ||
        (threePlayerEnabled && player3Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player3.rect)) ||
        (fourPlayerEnabled && player4Alive &&
            CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player4.rect));
    DrawValveBody(level.valve, playerNear);
}

void Game::DrawGasMaskPickup(const GuideObject& gasMask) const {
    if (gasMask.collected || gasMaskTexture.id <= 0) return;

    const float bob = sinf(static_cast<float>(GetTime()) * 3.0f + gasMask.transform.position.x * 0.03f) * 2.0f;
    const float width = 30.0f;
    const float height = width * static_cast<float>(gasMaskTexture.height) / gasMaskTexture.width;
    DrawCircleV({gasMask.transform.position.x, gasMask.transform.position.y + bob}, 17.0f, Fade(LIME, 0.15f));
    DrawTexturePro(
        gasMaskTexture,
        {0.0f, 0.0f, static_cast<float>(gasMaskTexture.width), static_cast<float>(gasMaskTexture.height)},
        {gasMask.transform.position.x - width * 0.5f, gasMask.transform.position.y - height * 0.5f + bob, width, height},
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

void Game::DrawEquippedGasMask(const Player& activePlayer) const {
    if (gasMaskTexture.id <= 0) return;

    constexpr float width = 20.0f;
    const float height = width * static_cast<float>(gasMaskTexture.height) / gasMaskTexture.width;
    DrawTexturePro(
        gasMaskTexture,
        {0.0f, 0.0f, static_cast<float>(gasMaskTexture.width), static_cast<float>(gasMaskTexture.height)},
        {activePlayer.rect.x + (activePlayer.rect.width - width) * 0.5f, activePlayer.rect.y + 5.0f, width, height},
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

void Game::DrawNeurotoxinLevel() {
    const bool active = !level.valve.opened;

    // Permanent equipment placard: unlike the facility notices below, this
    // identifies the valve itself and remains a conventional painted sign.
    const Rectangle valvePlacard{34.0f, 75.0f, 285.0f, 86.0f};
    const Color placardBorder{38, 43, 39, 255};
    const Color placardFace{231, 215, 164, 255};
    const Color placardWarning{158, 42, 31, 255};
    DrawRectangleRec(
        {valvePlacard.x + 4.0f, valvePlacard.y + 4.0f, valvePlacard.width, valvePlacard.height},
        Fade(BLACK, 0.28f)
    );
    DrawRectangleRec(valvePlacard, placardBorder);
    DrawRectangleRec(
        {
            valvePlacard.x + 4.0f,
            valvePlacard.y + 4.0f,
            valvePlacard.width - 8.0f,
            valvePlacard.height - 8.0f
        },
        placardFace
    );
    const int placardCenterX =
        static_cast<int>(valvePlacard.x + valvePlacard.width * 0.5f);
    const auto drawPlacardLine =
        [&](const char* text, int y, int fontSize, Color color) {
            const int textX = placardCenterX - MeasureText(text, fontSize) / 2;
            DrawText(text, textX, y, fontSize, color);
        };
    drawPlacardLine(
        "DEADLY NEUROTOXIN",
        static_cast<int>(valvePlacard.y + 7.0f),
        17,
        placardWarning
    );
    drawPlacardLine(
        "EMERGENCY SHUTOFF",
        static_cast<int>(valvePlacard.y + 28.0f),
        24,
        Color{30, 34, 31, 255}
    );
    drawPlacardLine(
        "DO NOT TURN",
        static_cast<int>(valvePlacard.y + 58.0f),
        18,
        placardWarning
    );

    auto drawScrollingLedSign = [&](
        Rectangle housing,
        const char* message,
        Color ledColor,
        float startingOffset,
        float scrollSpeed
    ) {
        const Color frameDark{18, 23, 25, 255};
        const Color frameMetal{65, 73, 74, 255};
        const Color frameHighlight{125, 134, 132, 255};
        Rectangle display{
            housing.x + 10.0f,
            housing.y + 10.0f,
            housing.width - 20.0f,
            housing.height - 20.0f
        };

        DrawRectangleRounded(housing, 0.12f, 5, frameDark);
        DrawRectangleRounded(
            {housing.x + 3.0f, housing.y + 3.0f, housing.width - 6.0f, housing.height - 6.0f},
            0.10f,
            5,
            frameMetal
        );
        DrawRectangleRec(display, Color{8, 13, 12, 255});
        DrawRectangleLinesEx(display, 2.0f, Color{21, 29, 27, 255});
        DrawLineEx(
            {housing.x + 8.0f, housing.y + 5.0f},
            {housing.x + housing.width - 8.0f, housing.y + 5.0f},
            1.5f,
            Fade(frameHighlight, 0.70f)
        );
        for (Vector2 bolt : std::initializer_list<Vector2>{
            {housing.x + 6.0f, housing.y + 6.0f},
            {housing.x + housing.width - 6.0f, housing.y + 6.0f},
            {housing.x + 6.0f, housing.y + housing.height - 6.0f},
            {housing.x + housing.width - 6.0f, housing.y + housing.height - 6.0f}
        }) {
            DrawCircleV(bolt, 2.0f, frameDark);
            DrawCircleV({bolt.x - 0.4f, bolt.y - 0.4f}, 0.8f, frameHighlight);
        }

        for (float scanlineY = display.y + 3.0f; scanlineY < display.y + display.height; scanlineY += 4.0f) {
            DrawLineEx(
                {display.x + 2.0f, scanlineY},
                {display.x + display.width - 2.0f, scanlineY},
                1.0f,
                Fade(ledColor, 0.035f)
            );
        }

        Font font = GetFontDefault();
        const float fontSize = static_cast<float>(font.baseSize) * 2.0f;
        const float spacing = 2.0f;
        const Vector2 textSize = MeasureTextEx(font, message, fontSize, spacing);
        const float gap = 46.0f;
        const float cycle = display.width + textSize.x + gap;
        const float distance = fmodf(
            toxinLevelTimer * scrollSpeed + display.width * startingOffset,
            cycle
        );
        const float firstX = display.x + display.width - distance;
        const float textY = display.y + (display.height - textSize.y) * 0.5f - 1.0f;
        const Vector2 scissorOrigin = GetWorldToScreen2D({display.x, display.y}, gameplayCamera);
        BeginScissorMode(
            static_cast<int>(floorf(scissorOrigin.x)),
            static_cast<int>(floorf(scissorOrigin.y)),
            static_cast<int>(ceilf(display.width * gameplayCamera.zoom)),
            static_cast<int>(ceilf(display.height * gameplayCamera.zoom))
        );
        for (float textX : {firstX, firstX + cycle}) {
            DrawTextEx(font, message, {textX + 1.0f, textY + 1.0f}, fontSize, spacing, Fade(ledColor, 0.18f));
            DrawTextEx(font, message, {textX, textY}, fontSize, spacing, ledColor);
        }
        EndScissorMode();
    };

    drawScrollingLedSign(
        {250.0f, 330.0f, 350.0f, 52.0f},
        active ? "Deadly Neurotoxin Flow: Active." : "Deadly Neurotoxin Flow: Shut Off.",
        active ? Color{255, 112, 44, 255} : Color{93, 230, 117, 255},
        0.08f,
        48.0f
    );
    drawScrollingLedSign(
        {1040.0f, 104.0f, 450.0f, 52.0f},
        "Air Quality Event In Progress.",
        Color{255, 194, 62, 255},
        0.48f,
        53.0f
    );
    drawScrollingLedSign(
        {980.0f, 520.0f, 500.0f, 52.0f},
        "Automated notice: Remain calm and continue breathing normally.",
        Color{151, 225, 90, 255},
        0.86f,
        44.0f
    );

    const bool player1Near = playerAlive &&
        CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player.rect);
    const bool player2Near = multiplayerEnabled && player2Alive &&
        CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player2.rect);
    const bool player3Near = threePlayerEnabled && player3Alive &&
        CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player3.rect);
    const bool player4Near = fourPlayerEnabled && player4Alive &&
        CheckCollisionCircleRec(level.valve.center, level.valve.radius + 25.0f, player4.rect);
    const std::string prompt =
        GetInteractPrompt(player1Near, player2Near, player3Near, player4Near, "Hold");
    DrawValvePrompt(
        level.valve,
        player1Near || player2Near || player3Near || player4Near,
        prompt.c_str(),
        true
    );
}

void Game::DrawGameplay() {
    Camera2D renderCamera = gameplayCamera;
    renderCamera.target.x = roundf(renderCamera.target.x);
    renderCamera.target.y = roundf(renderCamera.target.y);
    if (screenShakeTimer > 0.0f && screenShakeSetting != ScreenShakeSetting::Off) {
        const float strength = screenShakeSetting == ScreenShakeSetting::Reduced ? 2.0f : 5.0f;
        renderCamera.offset.x += static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * strength;
        renderCamera.offset.y += static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * strength;
    }

    BeginMode2D(renderCamera);

    constexpr float backgroundMargin = 64.0f;
    const float viewLeft = gameplayCamera.target.x - Constants::ScreenWidth * 0.5f - backgroundMargin;
    const float viewTop = gameplayCamera.target.y - Constants::ScreenHeight * 0.5f - backgroundMargin;
    const float viewRight = gameplayCamera.target.x + Constants::ScreenWidth * 0.5f + backgroundMargin;
    const float viewBottom = gameplayCamera.target.y + Constants::ScreenHeight * 0.5f + backgroundMargin;
    constexpr float backgroundTileSize = 32.0f;
    const float backgroundLeft = fmaxf(
        level.worldBounds.x,
        level.worldBounds.x + floorf((viewLeft - level.worldBounds.x) / backgroundTileSize) * backgroundTileSize
    );
    const float backgroundTop = fmaxf(
        level.worldBounds.y,
        level.worldBounds.y + floorf((viewTop - level.worldBounds.y) / backgroundTileSize) * backgroundTileSize
    );
    const float backgroundRight = fminf(
        level.worldBounds.x + level.worldBounds.width,
        level.worldBounds.x + ceilf((viewRight - level.worldBounds.x) / backgroundTileSize) * backgroundTileSize
    );
    const float backgroundBottom = fminf(
        level.worldBounds.y + level.worldBounds.height,
        level.worldBounds.y + ceilf((viewBottom - level.worldBounds.y) / backgroundTileSize) * backgroundTileSize
    );
    const Rectangle backgroundBounds{
        backgroundLeft,
        backgroundTop,
        fmaxf(0.0f, backgroundRight - backgroundLeft),
        fmaxf(0.0f, backgroundBottom - backgroundTop)
    };

    if (IsTilesetReferenceLevel(level) || HasFarBackgroundTiles(level)) {
        DrawTiledTextureRect(
            industrialTiles,
            {32.0f, 64.0f, 32.0f, 32.0f},
            backgroundBounds,
            WHITE
        );
    }
    else {
        DrawTilesetBackgroundFill(
            industrialBackground,
            backgroundBounds,
            Fade(WHITE, 0.68f),
            0.08f
        );
        DrawRectangleRec(backgroundBounds, Fade(Color{13, 20, 28, 255}, 0.16f));
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
    }
    else {
        for (const FluidField& fluid : level.fluids) {
            DrawFluidBackground(fluid);
        }
    }

    // Fixed wiring belongs in front of room-depth artwork but behind all
    // foreground architecture and moving machinery.
    DrawPortalLiftWiring(level);

    // The leaking pipe and valve sit inside the gas volume. Drawing the hardware
    // first lets the translucent gas pass visibly in front of it instead of
    // producing a valve-shaped interruption in the cloud.
    if (level.script == LevelScript::NeurotoxinMaze) {
        DrawNeurotoxinInfrastructure();
    }

    if (hasExplicitVisualTiles) {
        for (const VisualTile& tile : level.visualTiles) {
            if (tile.layer == TileLayer::Foreground) {
                DrawTilesetTile(industrialTiles, tile.column, tile.row, tile.position, WHITE);
            }
        }
    }
    else {
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

    // Background physics remains fully simulated, but is rendered behind the play plane.
    if (level.script == LevelScript::ClocktowerCore) {
        DrawClocktowerMovementFrame(level);
    }
    for (const GuideObject& object : level.guideObjects) {
        if (object.layer == WorldLayer::Background) {
            if (object.type == GuideObjectType::GasMask) DrawGasMaskPickup(object);
            else DrawGuideObject(object);
        }
    }
    for (const StoneBlock& block : level.stoneBlocks) {
        if (block.layer == WorldLayer::Background) DrawStoneBlock(block);
    }
    for (const Boulder& boulder : level.boulders) {
        if (boulder.layer == WorldLayer::Background) DrawBoulder(boulder);
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        if (wheel.layer == WorldLayer::Background) DrawPhysicsWheel(wheel);
    }
    for (const Gear& gear : level.gears) {
        if (gear.layer == WorldLayer::Background) DrawGear(gear);
    }
    for (const Flywheel& flywheel : level.flywheels) {
        if (flywheel.layer == WorldLayer::Background) DrawFlywheel(flywheel);
    }
    for (const Screw& screw : level.screws) {
        if (screw.layer == WorldLayer::Background) DrawScrew(screw);
    }
    if (level.script == LevelScript::ClocktowerCore) {
        DrawClocktowerFace(level);
        for (const Gear& gear : level.gears) {
            if (gear.clockHand == ClockHandType::None) continue;

            const bool player1Near = playerAlive && IsPlayerInsideClockGearProxy(gear, player);
            const bool player2Near = multiplayerEnabled && player2Alive &&
                IsPlayerInsideClockGearProxy(gear, player2);
            const bool player3Near = threePlayerEnabled && player3Alive &&
                IsPlayerInsideClockGearProxy(gear, player3);
            const bool player4Near = fourPlayerEnabled && player4Alive &&
                IsPlayerInsideClockGearProxy(gear, player4);
            const bool playerNear = player1Near || player2Near || player3Near || player4Near;
            const Color stateColor = IsClockGearLocked(gear) ? GREEN : (gear.stopped ? RED : ORANGE);
            if (gear.orientation == GearOrientation::Horizontal) {
                DrawEllipseLines(static_cast<int>(gear.center.x), static_cast<int>(gear.center.y),
                    gear.radius + 8.0f, (gear.radius + 8.0f) * 0.38f, stateColor);
            }
            else {
                DrawCircleLinesV(gear.center, gear.radius + 8.0f, stateColor);
            }

            const std::string brakeLabel = std::string(ClockHandName(gear.clockHand)) + " BRAKE";
            const int labelWidth = MeasureText(brakeLabel.c_str(), 18);
            const float labelY = gear.center.y - gear.radius *
                (gear.orientation == GearOrientation::Horizontal ? 0.46f : 1.0f) - 29.0f;
            DrawRectangle(static_cast<int>(gear.center.x - labelWidth * 0.5f - 6.0f), static_cast<int>(labelY - 2.0f),
                labelWidth + 12, 23, Fade(BLACK, 0.78f));
            DrawText(brakeLabel.c_str(), static_cast<int>(gear.center.x - labelWidth * 0.5f),
                static_cast<int>(labelY), 18, stateColor);

            if (playerNear) {
                const std::string prompt = GetInteractPrompt(player1Near, player2Near, player3Near, player4Near,
                    gear.stopped ? "Restart" : "Stop");
                const int promptWidth = MeasureText(prompt.c_str(), 19);
                const float promptY = gear.center.y + gear.radius *
                    (gear.orientation == GearOrientation::Horizontal ? 0.46f : 1.0f) + 14.0f;
                DrawRectangle(static_cast<int>(gear.center.x - promptWidth * 0.5f - 7.0f), static_cast<int>(promptY - 3.0f),
                    promptWidth + 14, 25, Fade(BLACK, 0.82f));
                DrawText(prompt.c_str(), static_cast<int>(gear.center.x - promptWidth * 0.5f),
                    static_cast<int>(promptY), 19, RAYWHITE);
            }
        }
    }

    if (level.script == LevelScript::WaterEscape) {
        // The broken pipe is deliberately visible; it establishes the source
        // of the flood before the player starts climbing.
        const Rectangle pipeBody{1360.0f, 796.0f, 132.0f, 46.0f};
        DrawRectangleRec(pipeBody, Color{69, 82, 89, 255});
        DrawRectangleLinesEx(pipeBody, 4.0f, Color{25, 31, 35, 255});
        DrawCircle(1360, 819, 34.0f, Color{83, 96, 102, 255});
        DrawCircleLines(1360, 819, 34.0f, Color{25, 31, 35, 255});
        DrawCircle(1360, 819, 20.0f, Color{19, 28, 33, 255});
        DrawCircle(1345, 832, 7.0f, SKYBLUE);
        DrawCircle(1328, 844, 5.0f, Fade(SKYBLUE, 0.82f));
    }

    for (const LevelLabel& label : level.labels) {
        const int fontSize = label.fontSize;
        const int padding = std::max(5, static_cast<int>(roundf(fontSize * 0.42f)));
        const int textWidth = MeasureText(label.text.c_str(), fontSize);
        Rectangle sign{
            label.position.x,
            label.position.y,
            static_cast<float>(textWidth + padding * 2),
            static_cast<float>(fontSize + padding * 2)
        };
        DrawRectangleRounded(sign, 0.18f, 6, Fade(Color{22, 28, 34, 255}, 0.90f));
        DrawRectangleRoundedLinesEx(sign, 0.18f, 6, 2.0f, Fade(ORANGE, 0.90f));
        DrawText(label.text.c_str(), static_cast<int>(sign.x) + padding, static_cast<int>(sign.y) + padding, fontSize, RAYWHITE);
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
        bool player4NearWinch = fourPlayerEnabled && player4Alive && IsNearRect(player4.rect, machineWinch.rect, 18.0f);
        std::string winchPrompt =
            GetInteractPrompt(player1NearWinch, player2NearWinch, player3NearWinch, player4NearWinch, "Press");
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

        float gateMotorWireX = HasArea(level.exitTrigger) ? level.exitTrigger.x - 115.0f : 1370.0f;
        DrawElectricalWire(
            {{655, 400}, {760, 400}, {760, 320}, {1220, 320},
             {1220, 598}, {gateMotorWireX, 598}},
            4.0f,
            wireColor,
            lightsOn
        );

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
    else {
        for (int index = 0; index < static_cast<int>(level.pulleys.size()); ++index) {
            DrawPulley(level.pulleys[index], 42.0f, pulleyRotation * (1.0f + index * 0.18f), BLACK);
        }
    }

    for (const HangingWeight& weight : level.weights) {
        DrawHazardWeight(weight, ropePatternOffset);
    }

    if (HasFloodWaterControl(level)) {
        bool player1NearValve = playerAlive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
        bool player2NearValve = multiplayerEnabled && player2Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player2.rect);
        bool player3NearValve = threePlayerEnabled && player3Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player3.rect);
        bool player4NearValve = fourPlayerEnabled && player4Alive && CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player4.rect);
        bool playerNearValve = player1NearValve || player2NearValve || player3NearValve || player4NearValve;
        std::string valvePrompt =
            GetInteractPrompt(player1NearValve, player2NearValve, player3NearValve, player4NearValve, "Hold");
        float valveOpenAmount = GetValveOpenAmount(level.valve);
        float outletX = level.valve.center.x + 118.0f;
        float outletY = 302.0f;
        bool pumpFilling = valveOpenAmount > 0.0f && GetFloodWaterProgress(level) < 0.999f;
        DrawFloodPump(level.valve, {outletX, outletY}, GetFloodWaterSurfaceY(level), pumpFilling);
        DrawValve(level.valve, playerNearValve, valvePrompt.c_str());
    }

    // The chamber lamp is part of the background so the rail and every moving
    // platform pass cleanly in front of both the fixture and its light cone.
    DrawPortalLiftChamberLamp(level, machinePower);

    if (HasArea(level.spikeHazard)) {
        float pitTopY = level.script == LevelScript::FloodedFoundry ? 672.0f : level.spikePitTopY;
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
            level.worldBounds.y + level.worldBounds.height - spikeBaseY
        };

        DrawTilesetPitWalls(industrialTiles, pitShaft, Fade(WHITE, 0.88f));
        DrawTilesetPitFoundation(industrialTiles, pitFoundation, Fade(WHITE, 0.88f));
        DrawSpikes(level.spikeHazard);
    }

    if (HasWaterPit(level)) {
        DrawWaterPit(level.waterPit);
    }

    // Fixed ramps sit beneath adjoining masonry. Drawing them first lets the
    // platform tiles cap their angled ends instead of exposing corner wedges.
    // Portal Lift keeps its arrival ramp above its scripted blackout layer.
    if (level.script != LevelScript::PortalLift) {
        for (const Ramp& ramp : level.ramps) {
            DrawRamp(ramp);
        }
    }

    // Collision-authored platforms retain exact dimensions and cropped end
    // caps. Dynamic platforms and rails must remain visible in levels whose
    // static room architecture is authored with explicit visual tiles.
    for (Rectangle platform : level.pitPlatforms) {
        DrawTilesetSolid(industrialTiles, platform, WHITE);
        DrawRectangleLinesEx(platform, 2, BLACK);
    }
    for (const ButtonPlatformLink& link : level.buttonPlatformLinks) {
        if (!link.active) continue;
        DrawTilesetSolid(industrialTiles, link.platform, WHITE);
        DrawRectangleLinesEx(link.platform, 2, BLACK);
    }
    for (const ButtonPlatformLoop& loop : level.buttonPlatformLoops) {
        if (!loop.active) continue;
        DrawPlatformRailTrack(loop);
        for (const Rectangle platform : loop.platforms) {
            DrawTilesetSolid(industrialTiles, platform, WHITE);
            DrawRectangleLinesEx(platform, 2, BLACK);
            for (const DirectionalSpikeHazard& hazard : GetPlatformSideSpikes(platform)) {
                DrawDirectionalSpikes(hazard);
            }
        }
    }

    for (const PortalPair& pair : level.portalPairs) {
        DrawPortal(pair.entrance, Color{255, 146, 52, 255});
        DrawPortal(pair.exit, Color{63, 143, 255, 255});
    }
    for (const DirectionalSpikeHazard& hazard : level.directionalSpikeHazards) {
        DrawDirectionalSpikes(hazard);
    }
    for (const ButtonSpikeLink& link : level.buttonSpikeLinks) {
        if (link.active) DrawDirectionalSpikes(link.hazard);
    }

    for (const BreakableTile& tile : level.breakableTiles) {
        DrawBreakableTile(industrialTiles, tile);
    }

    std::vector<Rectangle> splashSources;
    if (playerAlive) splashSources.push_back(player.rect);
    if (multiplayerEnabled && player2Alive) splashSources.push_back(player2.rect);
    if (threePlayerEnabled && player3Alive) splashSources.push_back(player3.rect);
    if (fourPlayerEnabled && player4Alive) splashSources.push_back(player4.rect);
    for (const Enemy& enemy : level.enemies) {
        splashSources.push_back(enemy.rect);
    }

    for (const FluidField& fluid : level.fluids) {
        DrawFluidField(fluid, splashSources);
    }

    // Draw ladders over simulated materials so submerged escape routes remain visible.
    for (Rectangle ladder : level.ladders) {
        if (!HasArea(ladder)) continue;
        DrawLineEx({ladder.x + 8, ladder.y}, {ladder.x + 8, ladder.y + ladder.height}, 4, BLACK);
        DrawLineEx({ladder.x + ladder.width - 8, ladder.y}, {ladder.x + ladder.width - 8, ladder.y + ladder.height}, 4, BLACK);
        int rungCount = static_cast<int>(ceilf(ladder.height / 30.0f));
        for (int i = 0; i <= rungCount; i++) {
            float y = fminf(ladder.y + i * 30.0f, ladder.y + ladder.height);
            DrawLineEx({ladder.x + 8, y}, {ladder.x + ladder.width - 8, y}, 3, BLACK);
        }
    }
    for (const ButtonLadderLink& link : level.buttonLadderLinks) {
        Rectangle ladder = GetRevealedLadderRect(link);
        if (!link.activated || !HasArea(ladder)) continue;
        DrawLineEx({ladder.x + 8, ladder.y}, {ladder.x + 8, ladder.y + ladder.height}, 4, BLACK);
        DrawLineEx({ladder.x + ladder.width - 8, ladder.y}, {ladder.x + ladder.width - 8, ladder.y + ladder.height}, 4, BLACK);
        int rungCount = static_cast<int>(ceilf(ladder.height / 30.0f));
        for (int i = 0; i <= rungCount; i++) {
            float y = fminf(ladder.y + i * 30.0f, ladder.y + ladder.height);
            DrawLineEx({ladder.x + 8, y}, {ladder.x + ladder.width - 8, y}, 3, BLACK);
        }
    }

    if (level.script == LevelScript::NeurotoxinMaze) {
        DrawNeurotoxinLevel();
    }

    for (const GuideObject& object : level.guideObjects) {
        if (object.layer == WorldLayer::Middleground) {
            if (object.type == GuideObjectType::GasMask) DrawGasMaskPickup(object);
            else DrawGuideObject(object);
        }
    }

    for (const Button& button : level.buttons) {
        DrawButton(button);
    }

    // Portal Lift's blackout conceals the rail, moving platforms, lamp, and
    // moving button, but remains a background layer behind the arrival ramp.
    if (level.script == LevelScript::PortalLift) {
        const bool hasPortalLiftDarkness =
            std::any_of(level.darknessAreas.begin(), level.darknessAreas.end(), HasArea);
        if (hasPortalLiftDarkness) {
            const float blackoutFlicker = machinePower > 0.02f ? flicker : 0.0f;
            const float blackoutAlpha = Clamp01(1.0f - machinePower - blackoutFlicker);
            for (Rectangle darknessArea : level.darknessAreas) {
                DrawRectangleRec(darknessArea, Fade(BLACK, blackoutAlpha));
            }
        }
    }

    for (const ArrowTrap& trap : level.arrowTraps) {
        DrawArrowTrap(trap);
    }

    if (level.script == LevelScript::PortalLift) {
        for (const Ramp& ramp : level.ramps) {
            DrawRamp(ramp);
        }
    }

    for (const TrapDoor& trapDoor : level.trapDoors) {
        DrawTrapDoor(trapDoor);
    }

    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawSeeSaw(seeSaw);
    }

    for (const StoneBlock& block : level.stoneBlocks) {
        if (block.layer == WorldLayer::Middleground) DrawStoneBlock(block);
    }

    for (const Boulder& boulder : level.boulders) {
        if (boulder.layer == WorldLayer::Middleground) DrawBoulder(boulder);
    }

    for (const PhysicsWheel& wheel : level.physicsWheels) {
        if (wheel.layer == WorldLayer::Middleground) DrawPhysicsWheel(wheel);
    }

    for (const Gear& gear : level.gears) {
        if (gear.layer == WorldLayer::Middleground) DrawGear(gear);
    }

    for (const Flywheel& flywheel : level.flywheels) {
        if (flywheel.layer == WorldLayer::Middleground) DrawFlywheel(flywheel);
    }

    for (const SteeringWheel& steeringWheel : level.steeringWheels) {
        DrawSteeringWheel(steeringWheel);
    }

    for (const Screw& screw : level.screws) {
        if (screw.layer == WorldLayer::Middleground) DrawScrew(screw);
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
        bool player4Near = fourPlayerEnabled && player4Alive && CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player4.rect);
        bool playerNear = player1Near || player2Near || player3Near || player4Near;
        std::string latchPrompt =
            GetInteractPrompt(player1Near, player2Near, player3Near, player4Near, "");
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

        DrawExitDoor(level.exitTrigger, gateBottom);
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
    if (fourPlayerEnabled && !player4Alive) {
        DrawDeathMarker(skullTexture, player4DeathRect);
    }

    const auto getAirWarningTint = [&](const Player& activePlayer, int playerIndex) {
        const bool toxinHazard =
            level.script == LevelScript::NeurotoxinMaze &&
            !playerGasMasks[playerIndex] &&
            SampleFluidAroundRectangle(level, FluidType::Gas, activePlayer.rect).density >= 0.025f;
        const bool underwaterHazard =
            level.script != LevelScript::NeurotoxinMaze &&
            IsPlayerHeadSubmerged(activePlayer, level);
        if (!toxinHazard && !underwaterHazard) {
            return WHITE;
        }

        const float airDanger = Clamp01(1.0f - playerAir[playerIndex]);
        const Color warningColor = toxinHazard
            ? Color{72, 255, 76, 255}
            : Color{70, 205, 255, 255};
        if (reducedFlashing) {
            return ColorLerp(WHITE, warningColor, 0.18f + airDanger * 0.48f);
        }

        const float flashOnFraction = 0.20f + airDanger * 0.28f;
        if (playerAirWarningPhase[playerIndex] >= flashOnFraction) {
            return WHITE;
        }

        const float tintStrength = 0.48f + sqrtf(airDanger) * 0.42f;
        return ColorLerp(WHITE, warningColor, tintStrength);
    };

    const auto drawActivePlayer = [&](const Player& activePlayer, int playerIndex, float swimPhase) {
        Player visiblePlayer = activePlayer;
        if (IsPlayerSwimming(activePlayer, level)) {
            visiblePlayer.rect.y +=
                sinf(static_cast<float>(GetTime()) * 5.0f + swimPhase) * 3.5f;
        }

        const Color tint = getAirWarningTint(activePlayer, playerIndex);
        const int character =
            std::clamp(selectedCharacters[playerIndex], 0, kCharacterCount - 1);
        if (character < 3) {
            DrawPlayer(
                visiblePlayer,
                playerSpritesTexture,
                character,
                37.0f,
                47.0f,
                3,
                0.0f,
                0.0f,
                0.0f,
                tint
            );
        }
        else {
            DrawPlayer(
                visiblePlayer,
                playerFourSpritesTexture,
                0,
                37.0f,
                47.0f,
                1,
                64.0f,
                4.0f,
                17.0f,
                tint
            );
        }
    };

    if (playerAlive) {
        drawActivePlayer(player, 0, 0.0f);
        if (playerGasMasks[0]) DrawEquippedGasMask(player);
    }
    if (multiplayerEnabled && player2Alive) {
        drawActivePlayer(player2, 1, 0.65f);
        if (playerGasMasks[1]) DrawEquippedGasMask(player2);
    }
    if (threePlayerEnabled && player3Alive) {
        drawActivePlayer(player3, 2, 1.30f);
        if (playerGasMasks[2]) DrawEquippedGasMask(player3);
    }
    if (fourPlayerEnabled && player4Alive) {
        drawActivePlayer(player4, 3, 1.95f);
        if (playerGasMasks[3]) DrawEquippedGasMask(player4);
    }

    // Foreground physics can obscure the player for depth, while remaining on its
    // own non-player collision plane.
    for (const GuideObject& object : level.guideObjects) {
        if (object.layer == WorldLayer::Foreground) {
            if (object.type == GuideObjectType::GasMask) DrawGasMaskPickup(object);
            else DrawGuideObject(object);
        }
    }
    for (const StoneBlock& block : level.stoneBlocks) {
        if (block.layer == WorldLayer::Foreground) DrawStoneBlock(block);
    }
    for (const Boulder& boulder : level.boulders) {
        if (boulder.layer == WorldLayer::Foreground) DrawBoulder(boulder);
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        if (wheel.layer == WorldLayer::Foreground) DrawPhysicsWheel(wheel);
    }
    for (const Gear& gear : level.gears) {
        if (gear.layer == WorldLayer::Foreground) DrawGear(gear);
    }
    for (const Flywheel& flywheel : level.flywheels) {
        if (flywheel.layer == WorldLayer::Foreground) DrawFlywheel(flywheel);
    }
    for (const Screw& screw : level.screws) {
        if (screw.layer == WorldLayer::Foreground) DrawScrew(screw);
    }

    bool hasDarkness = std::any_of(level.darknessAreas.begin(), level.darknessAreas.end(), HasArea);
    if (hasDarkness && level.script != LevelScript::PortalLift) {
        float blackoutFlicker = machinePower > 0.02f ? flicker : 0.0f;
        float safeAreaDimAlpha = Clamp01(0.20f + (1.0f - machinePower) * 0.18f - flicker * 0.45f);
        float blackoutAlpha = Clamp01(1.0f - machinePower - blackoutFlicker);
        DrawRectangle(0, 0, 300, Constants::ScreenHeight, Fade(BLACK, safeAreaDimAlpha));
        DrawRectangle(300, 0, 960, 275, Fade(BLACK, safeAreaDimAlpha));
        for (Rectangle darknessArea : level.darknessAreas) {
            DrawRectangleRec(darknessArea, Fade(BLACK, blackoutAlpha));
        }
    }
    if (highContrast) {
        const Color danger = AccessibleDangerColor(colorblindSetting);
        if (playerAlive) DrawRectangleLinesEx(player.rect, 3.0f, RAYWHITE);
        if (multiplayerEnabled && player2Alive) DrawRectangleLinesEx(player2.rect, 3.0f, SKYBLUE);
        if (threePlayerEnabled && player3Alive) DrawRectangleLinesEx(player3.rect, 3.0f, LIME);
        if (fourPlayerEnabled && player4Alive) DrawRectangleLinesEx(player4.rect, 3.0f, VIOLET);
        for (const Enemy& enemy : level.enemies) DrawRectangleLinesEx(enemy.rect, 3.0f, danger);
        if (HasArea(level.spikeHazard)) DrawRectangleLinesEx(level.spikeHazard, 3.0f, danger);
        if (HasArea(level.exitTrigger)) DrawRectangleLinesEx(level.exitTrigger, 3.0f, AccessibleSuccessColor(colorblindSetting));
    }

    if (debugCollision) {
        DrawDebugCollision();
    }

    EndMode2D();

    if (objectTooltipsEnabled) {
        DrawHoveredObjectTooltip(level, renderCamera);
    }

    if (showFPS) DrawFPS(10, 10);

    if (level.script == LevelScript::NeurotoxinMaze) {
        float highestExposure = playerAlive ? toxinExposure[0] : 0.0f;
        if (multiplayerEnabled && player2Alive) highestExposure = fmaxf(highestExposure, toxinExposure[1]);
        if (threePlayerEnabled && player3Alive) highestExposure = fmaxf(highestExposure, toxinExposure[2]);
        if (fourPlayerEnabled && player4Alive) highestExposure = fmaxf(highestExposure, toxinExposure[3]);

        if (highestExposure > 0.12f) {
            const float pulse = reducedFlashing ? 1.0f : 0.78f + sinf(toxinLevelTimer * 5.5f) * 0.22f;
            DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight,
                Fade(Color{116, 210, 85, 255}, (highestExposure - 0.12f) * 0.10f * pulse));
        }
    }

    int latchTotal = static_cast<int>(level.rotaryLatches.size());
    if (latchTotal > 0 && level.script == LevelScript::RotaryLatchLab) {
        bool allLatchesLocked = latchedCount == latchTotal;
        int statusWidth = fourPlayerEnabled ? 460 : (threePlayerEnabled ? 350 : 300);
        DrawRectangle(20, 20, statusWidth, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Wheel locks: %d / %d", latchedCount, latchTotal), 34, 31, 20, RAYWHITE);
        const char* latchHelp = fourPlayerEnabled ? "Align spokes: E / U / Right Ctrl / Numpad 0" :
            (threePlayerEnabled ? "Align spokes: E / U / Right Ctrl" :
            (multiplayerEnabled ? "Align spokes, press E or U" : "Align spokes, press E"));
        DrawText(allLatchesLocked ? "Gate circuit complete" : latchHelp, 34, 58, 20, allLatchesLocked ? GREEN : ORANGE);
    }

    if (HasFloodWaterControl(level)) {
        float fillPercent = GetFloodWaterProgress(level) * 100.0f;
        float valvePercent = GetValveOpenAmount(level.valve) * 100.0f;
        DrawRectangle(20, 20, 260, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Water level: %.0f%%", fillPercent), 34, 31, 20, RAYWHITE);
        const char* valveHelp = fourPlayerEnabled ? TextFormat("E/U/RCtrl/Num0: valve %.0f%%", valvePercent) :
            (threePlayerEnabled ? TextFormat("E/U/RCtrl: valve %.0f%%", valvePercent) :
            TextFormat(multiplayerEnabled ? "Hold E/U: valve %.0f%%" : "Hold E: valve %.0f%%", valvePercent));
        DrawText(level.valve.opened ? "Swim the flooded pit" : valveHelp, 34, 58, 20, level.valve.opened ? SKYBLUE : ORANGE);
    }

    if (level.script == LevelScript::CounterweightRow) {
        DrawRectangle(20, 20, 370, 66, Fade(BLACK, 0.52f));
        DrawText(machinePower > 0.5f ? "Counterweight locked" : "Drop the boulder to the lower lane", 34, 31, 20,
            machinePower > 0.5f ? GREEN : ORANGE);
        DrawText(machinePower > 0.5f ? "Exit gate is open" : "Use it as cover, then find the plate", 34, 58, 20, RAYWHITE);
    }

    if (level.script == LevelScript::ButtonSequence) {
        int stage = level.buttonExitLink.activated ? 3 :
            (!level.buttonLadderLinks.empty() && level.buttonLadderLinks.front().activated ? 2 :
            (!level.buttonTrapDoorLinks.empty() && level.buttonTrapDoorLinks.front().activated ? 1 : 0));
        const char* objective = stage == 0 ? "Roll the first ball onto Button 1" :
            (stage == 1 ? "The trap door is open - follow the falling ball" :
            (stage == 2 ? "Climb the ladder and roll the upper ball" : "Exit door open"));
        DrawRectangle(20, 20, 500, 66, Fade(BLACK, 0.52f));
        DrawText(TextFormat("Three-Step Tumble: %d / 3", stage), 34, 31, 20,
            stage == 3 ? GREEN : RAYWHITE);
        DrawText(objective, 34, 58, 20, stage == 3 ? GREEN : ORANGE);
    }

    if (level.script == LevelScript::WaterEscape) {
        const float waterPercent = HasValveFluidFill(level) ? GetValveFluidFillProgress(level) * 100.0f : 0.0f;
        DrawRectangle(20, 20, 510, 66, Fade(BLACK, 0.52f));
        DrawText(TextFormat("Rising Water: %.0f%%", waterPercent), 34, 31, 20, RAYWHITE);
        DrawText(level.buttonExitLink.activated ? "Exit gate open - get out!" : "Climb above the leak and unlock the exit", 34, 58, 20,
            level.buttonExitLink.activated ? GREEN : ORANGE);
    }

    if (level.script == LevelScript::ClocktowerCore) {
        const int clockHandCount = CountClockHandGears(level);
        const int lockedHandCount = CountLockedClockHands(level);
        const bool synchronized = clockHandCount == 3 && lockedHandCount == clockHandCount;
        DrawRectangle(20, 20, 520, 66, Fade(BLACK, 0.56f));
        DrawText(TextFormat("Clock hands locked: %d / %d", lockedHandCount, clockHandCount), 34, 31, 20,
            synchronized ? GREEN : RAYWHITE);
        DrawText(synchronized ? "Midnight synchronized - gate open" : "Stop each drive gear when its hand reaches XII",
            34, 58, 20, synchronized ? GREEN : ORANGE);
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

void Game::UpdateMusic() {
    const bool titleActive = mode == GameMode::Title || mode == GameMode::CharacterSelect;
    const bool levelSelectActive = mode == GameMode::Overworld;
    const bool gameplayMusicActive = mode == GameMode::Playing || mode == GameMode::Paused;
    const bool levelOneActive = currentLevelNode == 0 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool levelTwoActive = currentLevelNode == 1 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool levelThreeActive = currentLevelNode == 2 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool levelFourActive = currentLevelNode == 3 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool levelFiveActive = currentLevelNode == 4 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool levelSixActive = currentLevelNode == 5 &&
        (mode == GameMode::Playing || mode == GameMode::Paused);
    const bool wendiLevelOneActive = gameplayMusicActive &&
        currentLevelNode >= 0 &&
        currentLevelNode < static_cast<int>(overworldNodes.size()) &&
        overworldNodes[currentLevelNode].id == "wendis_level_1";
    const bool portalLiftActive = gameplayMusicActive &&
        currentLevelNode >= 0 &&
        currentLevelNode < static_cast<int>(overworldNodes.size()) &&
        overworldNodes[currentLevelNode].id == "wendis_level_2";

    auto updateTrack = [&](Music& music, bool loaded, bool active) {
        if (!loaded) return;
        if (active) {
            if (!IsMusicStreamPlaying(music)) PlayMusicStream(music);
            SetMusicVolume(music, musicVolume);
            UpdateMusicStream(music);
        }
        else if (IsMusicStreamPlaying(music)) {
            StopMusicStream(music);
        }
    };

    updateTrack(titleMusic, titleMusicLoaded, titleActive);
    updateTrack(levelSelectMusic, levelSelectMusicLoaded, levelSelectActive);
    updateTrack(levelOneMusic, levelOneMusicLoaded, levelOneActive);
    updateTrack(levelTwoMusic, levelTwoMusicLoaded, levelTwoActive);
    updateTrack(levelThreeMusic, levelThreeMusicLoaded, levelThreeActive);
    updateTrack(levelFourMusic, levelFourMusicLoaded, levelFourActive);
    updateTrack(levelFiveMusic, levelFiveMusicLoaded, levelFiveActive);
    updateTrack(levelSixMusic, levelSixMusicLoaded, levelSixActive);
    updateTrack(wendiLevelOneMusic, wendiLevelOneMusicLoaded, wendiLevelOneActive);
    updateTrack(portalLiftMusic, portalLiftMusicLoaded, portalLiftActive);
}

void Game::Unload() {
    if (sceneTarget.id > 0) UnloadRenderTexture(sceneTarget);
    UnloadTexture(playerSpritesTexture);
    UnloadTexture(playerFourSpritesTexture);
    UnloadTexture(skullTexture);
    UnloadTexture(industrialTiles);
    UnloadTexture(industrialBackground);
    UnloadTexture(industrialFarBackground);
    UnloadTexture(chainLinksTexture);
    UnloadTexture(enemyPlaceholderTexture);
    UnloadTexture(gasMaskTexture);
    if (titleMusicLoaded) {
        StopMusicStream(titleMusic);
        UnloadMusicStream(titleMusic);
        titleMusicLoaded = false;
    }
    if (levelSelectMusicLoaded) {
        StopMusicStream(levelSelectMusic);
        UnloadMusicStream(levelSelectMusic);
        levelSelectMusicLoaded = false;
    }
    if (levelOneMusicLoaded) {
        StopMusicStream(levelOneMusic);
        UnloadMusicStream(levelOneMusic);
        levelOneMusicLoaded = false;
    }
    if (levelTwoMusicLoaded) {
        StopMusicStream(levelTwoMusic);
        UnloadMusicStream(levelTwoMusic);
        levelTwoMusicLoaded = false;
    }
    if (levelThreeMusicLoaded) {
        StopMusicStream(levelThreeMusic);
        UnloadMusicStream(levelThreeMusic);
        levelThreeMusicLoaded = false;
    }
    if (levelFourMusicLoaded) {
        StopMusicStream(levelFourMusic);
        UnloadMusicStream(levelFourMusic);
        levelFourMusicLoaded = false;
    }
    if (levelFiveMusicLoaded) {
        StopMusicStream(levelFiveMusic);
        UnloadMusicStream(levelFiveMusic);
        levelFiveMusicLoaded = false;
    }
    if (levelSixMusicLoaded) {
        StopMusicStream(levelSixMusic);
        UnloadMusicStream(levelSixMusic);
        levelSixMusicLoaded = false;
    }
    if (wendiLevelOneMusicLoaded) {
        StopMusicStream(wendiLevelOneMusic);
        UnloadMusicStream(wendiLevelOneMusic);
        wendiLevelOneMusicLoaded = false;
    }
    if (portalLiftMusicLoaded) {
        StopMusicStream(portalLiftMusic);
        UnloadMusicStream(portalLiftMusic);
        portalLiftMusicLoaded = false;
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
}

void Game::ExecuteConsoleCommand(const std::string& line) {
    std::vector<std::string> args = SplitCommandLine(line);
    if (args.empty()) return;

    std::string command = ToLower(args[0]);
    console.AddLine("> " + line);

    if (command == "help") {
        console.AddLine("Commands: help, clear, start, overworld, title, pause, resume, quit, reset, win, kill, kill_enemy, kill_all_enemies, fps, achievements, fluid_sim, debug_collision, disable_arrow_traps, invincible, suffocation, unlock_all_levels, teleport, power, player, machine");
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
    else if (command == "achievements" || command == "achievement") {
        const std::string action = args.size() >= 2 ? ToLower(args[1]) : "list";
        if (action == "list") {
            for (const Achievement& achievement : achievements.GetAchievements()) {
                console.AddLine(std::string(achievement.unlocked ? "[unlocked] " : "[locked] ") +
                    achievement.id + " - " + achievement.title);
            }
        }
        else if (action == "unlock" && args.size() >= 3) {
            if (achievements.Unlock(ToLower(args[2]))) {
                console.AddLine("Achievement unlocked.");
            }
            else {
                console.AddLine("Achievement was already unlocked or the id was not found.");
            }
        }
        else if (action == "reset") {
            achievements.ResetProgress();
            console.AddLine("Achievement progress reset.");
        }
        else {
            console.AddLine("Usage: achievements [list|unlock <id>|reset]");
        }
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
        const Player* activePlayer4 = fourPlayerEnabled && player4Alive ? &player4 : nullptr;
        std::vector<Rectangle> obstacles =
            BuildFluidObstacles(level, activePlayer1, activePlayer2, activePlayer3, activePlayer4, true);
        std::vector<Rectangle> gasObstacles =
            BuildFluidObstacles(level, nullptr, nullptr, nullptr, nullptr, false);
        for (FluidField& fluid : level.fluids) {
            const std::vector<Rectangle>& relevantObstacles =
                fluid.type == FluidType::Gas ? gasObstacles : obstacles;
            InitializeFluidField(
                fluid,
                FilterFluidObstacles(fluid, relevantObstacles),
                SelectedFluidMode(advancedFluidSimulation)
            );
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
    else if (command == "suffocation" || command == "air_kills") {
        if (args.size() >= 2) {
            std::string value = ToLower(args[1]);
            if (value == "on") {
                suffocationKills = true;
            }
            else if (value == "off") {
                suffocationKills = false;
            }
            else {
                console.AddLine("Usage: suffocation [on|off]");
                return;
            }
        }
        else {
            suffocationKills = !suffocationKills;
        }

        console.AddLine("Suffocation kills " + OnOff(suffocationKills) + ".");
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
        player4DeathRect = player4.rect;
        mode = GameMode::Playing;
        won = false;
        lost = false;
        playerAlive = true;
        player2Alive = true;
        player3Alive = true;
        player4Alive = true;
        UpdateGameplayCamera(0.0f, true);
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

    for (Rectangle ladder : level.ladders) {
        DrawRectangleLinesEx(ladder, 2, Fade(YELLOW, 0.9f));
    }
    DrawRectangleLinesEx(level.spikeHazard, 2, Fade(RED, 0.95f));
    DrawRectangleLinesEx(level.exitTrigger, 2, Fade(PURPLE, 0.95f));

    if (HasFloodWaterControl(level)) {
        if (HasWaterPit(level)) {
            DrawRectangleLinesEx(level.waterPit.bounds, 2, Fade(SKYBLUE, 0.95f));
            DrawRectangleLinesEx(GetFilledWaterRect(level.waterPit), 2, Fade(BLUE, 0.95f));
        }
        DrawCircleLinesV(level.valve.center, level.valve.radius + 24.0f, Fade(BLUE, 0.9f));
    }
    if (level.script == LevelScript::NeurotoxinMaze) {
        DrawCircleLinesV(level.valve.center, level.valve.radius + 25.0f, Fade(ORANGE, 0.95f));
        DrawCircleLinesV(level.toxinLeak.source, 18.0f, Fade(LIME, 0.95f));
    }

    for (const FluidField& fluid : level.fluids) {
        Color fluidColor = fluid.type == FluidType::Water ? SKYBLUE :
            (fluid.type == FluidType::Sand ? GOLD : (fluid.type == FluidType::Gel ? BLUE : LIME));
        DrawRectangleLinesEx(fluid.bounds, 2, Fade(fluidColor, 0.95f));
        if (fluid.type == FluidType::Water || fluid.type == FluidType::Sand || fluid.type == FluidType::Gas) {
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
        DrawCircleLinesV(gear.center, gear.radius * GearOuterRadiusScale, Fade(GOLD, 0.95f));
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

    for (const GuideObject& object : level.guideObjects) {
        Color color = object.collider.isTrigger ? Fade(PURPLE, 0.82f) :
            (object.body.type == BodyType::Dynamic ? Fade(ORANGE, 0.9f) : Fade(GREEN, 0.82f));
        DrawRectangleLinesEx(GetGuideObjectBounds(object), 2.0f, color);
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
    if (fourPlayerEnabled && player4Alive) {
        DrawRectangleLinesEx(player4.rect, 2, Fade(VIOLET, 0.95f));
    }
}
