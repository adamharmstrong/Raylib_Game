#pragma once

#include "Level.h"
#include "Player.h"
#include "DevConsole.h"
#include "raylib.h"

#include <array>
#include <string>
#include <vector>

enum class GameMode {
    Title,
    Overworld,
    Playing,
    Paused
};

enum class WindowModeSetting {
    Windowed,
    Borderless,
    Fullscreen
};

enum class SettingsPage {
    Display,
    Audio,
    Controls,
    Accessibility
};

enum class SettingsDropdown {
    None,
    WindowMode,
    Resolution,
    FrameRate,
    UiScale,
    ScreenShake,
    ColorblindMode
};

enum class ScreenShakeSetting {
    Off,
    Reduced,
    Full
};

enum class ColorblindSetting {
    Off,
    Protanopia,
    Deuteranopia,
    Tritanopia
};

struct PlayerKeyBindings {
    KeyboardKey left{KEY_A};
    KeyboardKey right{KEY_D};
    KeyboardKey up{KEY_W};
    KeyboardKey down{KEY_S};
    KeyboardKey jump{KEY_SPACE};
    KeyboardKey interact{KEY_E};
};

struct OverworldNode {
    std::string id;
    std::string label;
    std::string name;
    Vector2 position;
    bool unlocked{false};
    bool completed{false};
};

struct OverworldPath {
    int fromNode{0};
    int toNode{0};
};

class Game {
public:
    void Run();

private:
    void Load();
    void Reset();
    void Update(float dt);
    void Draw();
    void Unload();

    void StartGame();
    void InitializeOverworld();
    void OpenOverworld();
    void StartSelectedOverworldLevel();
    void BeginLevelClear();
    void CompleteCurrentLevelAndReturnToMap();
    void UpdateTitle();
    void UpdateOverworld();
    void UpdatePaused();
    void UpdateGameOverActions();
    struct PlayerControls;
    struct PlayerMachineInput {
        float moveInput{0.0f};
        bool interactHeld{false};
        bool interactPressed{false};
        bool interactReleased{false};
    };
    void UpdatePlayer(Player& activePlayer, const PlayerControls& controls, float dt, float moveInput);
    std::array<bool, 3> UpdateFlexibleEndpointInteractions(
        const std::array<Player*, 3>& players,
        const std::array<PlayerMachineInput, 3>& inputs
    );
    void UpdateEnemies(float dt);
    void UpdateMachines(float dt, const PlayerMachineInput& player1Input, const PlayerMachineInput& player2Input, const PlayerMachineInput& player3Input);
    void UpdatePhysicsObjects(float dt);
    void UpdateButtons();
    void UpdateArrowTraps(float dt);
    void UpdateBreakableTiles(float dt);
    void UpdateWind(float dt);
    void UpdateFluids(float dt);
    void KillPlayer(const Player& defeatedPlayer);
    void CheckFailureConditions();
    void CheckWinCondition(float gateBottom);
    void ExecuteConsoleCommand(const std::string& line);
    void UpdateControlsPopup();
    void UpdateSettingsPopup();
    void UpdateQuitConfirmation();
    void OpenSettingsPopup();
    void ResetPendingSettings();
    void ApplyPendingSettings();
    void DrawScene();
    void DrawTitleScreen();
    void DrawOverworld();
    void DrawGameplay();
    void DrawPauseScreen();
    void DrawGameOverActions();
    void DrawControlsPopup();
    void DrawSettingsPopup();
    void DrawQuitConfirmation();
    void DrawDebugCollision() const;

    GameMode mode{GameMode::Title};
    bool shouldQuit{false};
    Level level{};
    Player player{};
    Player player2{};
    Player player3{};
    Winch machineWinch{{350, 205, 70, 45}, 350.0f, 590.0f, 7.0f};
    Texture2D playerSpritesTexture{};
    Texture2D skullTexture{};
    Texture2D industrialTiles{};
    Texture2D industrialBackground{};
    Texture2D industrialFarBackground{};
    Texture2D chainLinksTexture{};
    Texture2D enemyPlaceholderTexture{};
    RenderTexture2D sceneTarget{};

    bool showFPS{false};
    bool won{false};
    bool lost{false};
    bool playerAlive{true};
    bool player2Alive{true};
    bool player3Alive{true};
    Rectangle deathRect{80, 600, 31, 40};
    Rectangle playerDeathRect{80, 600, 31, 40};
    Rectangle player2DeathRect{124, 600, 31, 40};
    Rectangle player3DeathRect{168, 600, 31, 40};
    bool debugCollision{false};
    bool arrowTrapsDisabled{false};
    bool playerInvincible{false};
    bool quitConfirmationOpen{false};
    bool controlsPopupOpen{false};
    bool settingsPopupOpen{false};
    bool titleModeMenuOpen{false};
    bool advancedFluidSimulation{false};
    WindowModeSetting windowMode{WindowModeSetting::Windowed};
    int selectedResolutionIndex{5};
    bool vsyncEnabled{true};
    int frameRateIndex{1};
    int uiScaleIndex{1};
    bool pixelPerfectScaling{true};
    float masterVolume{1.0f};
    float musicVolume{0.8f};
    float soundEffectsVolume{0.9f};
    bool audioMuted{false};
    PlayerKeyBindings player1Bindings{};
    bool controllerEnabled{true};
    bool controllerVibrationEnabled{true};
    ScreenShakeSetting screenShakeSetting{ScreenShakeSetting::Full};
    bool reducedFlashing{false};
    bool highContrast{false};
    ColorblindSetting colorblindSetting{ColorblindSetting::Off};

    SettingsPage settingsPage{SettingsPage::Display};
    SettingsDropdown settingsDropdown{SettingsDropdown::None};
    int settingsBindingCapture{-1};
    WindowModeSetting pendingWindowMode{WindowModeSetting::Windowed};
    int pendingResolutionIndex{5};
    bool pendingAdvancedFluidSimulation{false};
    bool pendingVsyncEnabled{true};
    int pendingFrameRateIndex{1};
    int pendingUiScaleIndex{1};
    bool pendingPixelPerfectScaling{true};
    float pendingMasterVolume{1.0f};
    float pendingMusicVolume{0.8f};
    float pendingSoundEffectsVolume{0.9f};
    bool pendingAudioMuted{false};
    PlayerKeyBindings pendingPlayer1Bindings{};
    bool pendingControllerEnabled{true};
    bool pendingControllerVibrationEnabled{true};
    ScreenShakeSetting pendingScreenShakeSetting{ScreenShakeSetting::Full};
    bool pendingReducedFlashing{false};
    bool pendingHighContrast{false};
    ColorblindSetting pendingColorblindSetting{ColorblindSetting::Off};
    bool multiplayerEnabled{false};
    bool threePlayerEnabled{false};
    std::string menuMessage{};
    DevConsole console{};
    std::vector<OverworldNode> overworldNodes{};
    std::vector<OverworldPath> overworldPaths{};
    int selectedOverworldNode{0};
    int currentLevelNode{0};

    float pulleyRotation{0.0f};
    float machinePhase{0.0f};
    float machinePower{0.0f};
    float gateBottom{650.0f};
    float levelClearTimer{0.0f};
    float screenShakeTimer{0.0f};
};
