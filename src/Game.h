#pragma once

#include "Achievement.h"
#include "Level.h"
#include "Player.h"
#include "DevConsole.h"
#include "raylib.h"

#include <array>
#include <string>
#include <vector>

enum class GameMode {
    Title,
    CharacterSelect,
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
    ColorblindMode,
    ControllerDevice
};

enum class ControlsInputView {
    Keyboard,
    Controller
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

struct PlayerControllerSettings {
    int gamepad{-1};
    GamepadButton jump{GAMEPAD_BUTTON_RIGHT_FACE_DOWN};
    GamepadButton interact{GAMEPAD_BUTTON_RIGHT_FACE_RIGHT};
    bool vibration{true};
};

struct OverworldNode {
    std::string id;
    std::string label;
    std::string name;
    Vector2 position;
    int world{0};
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
    void OpenCharacterSelect(int playerCount);
    void InitializeOverworld();
    void OpenOverworld();
    void SetOverworldWorld(int world);
    void StartSelectedOverworldLevel();
    void BeginLevelClear();
    void CompleteCurrentLevelAndReturnToMap();
    void UpdateTitle();
    void UpdateCharacterSelect();
    void UpdateOverworld();
    void UpdatePaused();
    void UpdateGameOverActions();
    void UpdateGameplayCamera(float dt, bool snap);
    struct PlayerControls;
    struct PlayerMachineInput {
        float moveInput{0.0f};
        bool interactHeld{false};
        bool interactPressed{false};
        bool interactReleased{false};
    };
    void UpdatePlayer(Player& activePlayer, const PlayerControls& controls, float dt, float moveInput);
    std::array<bool, 4> UpdateFlexibleEndpointInteractions(
        const std::array<Player*, 4>& players,
        const std::array<PlayerMachineInput, 4>& inputs
    );
    void UpdateEnemies(float dt);
    void UpdateMachines(
        float dt,
        const PlayerMachineInput& player1Input,
        const PlayerMachineInput& player2Input,
        const PlayerMachineInput& player3Input,
        const PlayerMachineInput& player4Input
    );
    void UpdatePhysicsObjects(float dt);
    void UpdateButtons();
    void UpdatePortals();
    void UpdateArrowTraps(float dt);
    void UpdateBreakableTiles(float dt);
    void UpdateWind(float dt);
    void UpdateFluids(float dt);
    void UpdatePlayerAir(float dt);
    void UpdateNeurotoxin(
        float dt,
        const PlayerMachineInput& player1Input,
        const PlayerMachineInput& player2Input,
        const PlayerMachineInput& player3Input,
        const PlayerMachineInput& player4Input,
        const std::array<bool, 4>& consumedInputs
    );
    void KillPlayer(const Player& defeatedPlayer);
    void CheckFailureConditions();
    void CheckWinCondition(float gateBottom);
    void ExecuteConsoleCommand(const std::string& line);
    void UpdateControlsPopup();
    void UpdateSettingsPopup();
    void UpdateQuitConfirmation();
    void UpdateMusic();
    void OpenSettingsPopup();
    void ResetPendingSettings();
    void ApplyPendingSettings();
    void DrawScene();
    void DrawTitleScreen();
    void DrawCharacterSelect();
    void DrawOverworld();
    void DrawGameplay();
    void DrawPauseScreen();
    void DrawGameOverActions();
    void DrawControlsPopup();
    void DrawSettingsPopup();
    void DrawQuitConfirmation();
    void DrawNeurotoxinInfrastructure();
    void DrawNeurotoxinLevel();
    void DrawDebugCollision() const;

    GameMode mode{GameMode::Title};
    bool shouldQuit{false};
    Level level{};
    Player player{};
    Player player2{};
    Player player3{};
    Player player4{};
    Winch machineWinch{{350, 205, 70, 45}, 350.0f, 590.0f, 7.0f};
    Texture2D playerSpritesTexture{};
    Texture2D playerFourSpritesTexture{};
    Texture2D skullTexture{};
    Texture2D industrialTiles{};
    Texture2D industrialBackground{};
    Texture2D industrialFarBackground{};
    Texture2D chainLinksTexture{};
    Texture2D enemyPlaceholderTexture{};
    RenderTexture2D sceneTarget{};
    Music titleMusic{};
    Music levelSelectMusic{};
    Music levelOneMusic{};
    Music levelTwoMusic{};
    Music levelThreeMusic{};
    Music levelFourMusic{};
    Music levelFiveMusic{};
    Music levelSixMusic{};
    Music wendiLevelOneMusic{};
    Music portalLiftMusic{};
    Camera2D gameplayCamera{};
    int activeCameraZone{-1};

    bool showFPS{false};
    bool won{false};
    bool lost{false};
    bool playerAlive{true};
    bool player2Alive{true};
    bool player3Alive{true};
    bool player4Alive{true};
    Rectangle deathRect{80, 600, 31, 40};
    Rectangle playerDeathRect{80, 600, 31, 40};
    Rectangle player2DeathRect{124, 600, 31, 40};
    Rectangle player3DeathRect{168, 600, 31, 40};
    Rectangle player4DeathRect{212, 600, 31, 40};
    bool debugCollision{false};
    bool arrowTrapsDisabled{false};
    bool playerInvincible{false};
    bool quitConfirmationOpen{false};
    bool controlsPopupOpen{false};
    bool settingsPopupOpen{false};
    bool titleModeMenuOpen{false};
    int characterSelectPlayerCount{1};
    int characterSelectFocusPlayer{0};
    std::array<int, 4> selectedCharacters{0, 1, 2, 3};
    std::array<bool, 4> characterSelectReady{};
    bool titleMusicLoaded{false};
    bool levelSelectMusicLoaded{false};
    bool levelOneMusicLoaded{false};
    bool levelTwoMusicLoaded{false};
    bool levelThreeMusicLoaded{false};
    bool levelFourMusicLoaded{false};
    bool levelFiveMusicLoaded{false};
    bool levelSixMusicLoaded{false};
    bool wendiLevelOneMusicLoaded{false};
    bool portalLiftMusicLoaded{false};
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
    std::array<PlayerKeyBindings, 4> playerBindings{{
        {KEY_A, KEY_D, KEY_W, KEY_S, KEY_SPACE, KEY_E},
        {KEY_J, KEY_L, KEY_I, KEY_K, KEY_O, KEY_U},
        {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_RIGHT_ALT, KEY_RIGHT_CONTROL},
        {KEY_KP_4, KEY_KP_6, KEY_KP_8, KEY_KP_5, KEY_KP_ENTER, KEY_KP_0}
    }};
    std::array<PlayerControllerSettings, 4> controllerSettings{{
        {0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, true},
        {1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, true},
        {2, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, true},
        {3, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, true}
    }};
    ScreenShakeSetting screenShakeSetting{ScreenShakeSetting::Full};
    bool reducedFlashing{false};
    bool highContrast{false};
    bool objectTooltipsEnabled{false};
    ColorblindSetting colorblindSetting{ColorblindSetting::Off};

    SettingsPage settingsPage{SettingsPage::Display};
    SettingsDropdown settingsDropdown{SettingsDropdown::None};
    ControlsInputView settingsControlsInputView{ControlsInputView::Keyboard};
    int settingsSelectedPlayer{0};
    int settingsBindingCapture{-1};
    int settingsGamepadBindingCapture{-1};
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
    std::array<PlayerKeyBindings, 4> pendingPlayerBindings{};
    std::array<PlayerControllerSettings, 4> pendingControllerSettings{};
    ScreenShakeSetting pendingScreenShakeSetting{ScreenShakeSetting::Full};
    bool pendingReducedFlashing{false};
    bool pendingHighContrast{false};
    bool pendingObjectTooltipsEnabled{false};
    ColorblindSetting pendingColorblindSetting{ColorblindSetting::Off};
    bool multiplayerEnabled{false};
    bool threePlayerEnabled{false};
    bool fourPlayerEnabled{false};
    std::string menuMessage{};
    DevConsole console{};
    AchievementSystem achievements{};
    std::vector<OverworldNode> overworldNodes{};
    std::vector<OverworldPath> overworldPaths{};
    int selectedOverworldNode{0};
    int selectedOverworldWorld{0};
    std::array<int, 3> selectedOverworldNodesByWorld{0, 6, 8};
    int currentLevelNode{0};

    float pulleyRotation{0.0f};
    float machinePhase{0.0f};
    float machinePower{0.0f};
    float gateBottom{650.0f};
    float levelClearTimer{0.0f};
    float screenShakeTimer{0.0f};
    std::array<float, 4> toxinExposure{};
    std::array<float, 4> playerAir{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 4> playerAirWarningPhase{};
    float toxinEmissionAccumulator{0.0f};
    float toxinExhaustAccumulator{0.0f};
    float toxinLevelTimer{0.0f};
    bool suffocationKills{true};
    Vector2 checkpointRespawn{80.0f, 600.0f};
    bool checkpointActivated{false};
    float respawnGraceTimer{0.0f};
};
