#pragma once

#include "Level.h"
#include "Player.h"
#include "DevConsole.h"
#include "raylib.h"

#include <string>
#include <vector>

enum class GameMode {
    Title,
    Overworld,
    Playing,
    Paused
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
    void UpdatePlayer(Player& activePlayer, const PlayerControls& controls, float dt, float moveInput);
    void UpdateEnemies(float dt);
    void UpdateMachines(float dt, float moveInput);
    void UpdateStoneBlocksAndSeeSaws(float dt);
    void KillPlayer(const Player& defeatedPlayer);
    void CheckFailureConditions();
    void CheckWinCondition(float gateBottom);
    void ExecuteConsoleCommand(const std::string& line);
    void UpdateControlsPopup();
    void UpdateQuitConfirmation();
    void DrawTitleScreen();
    void DrawOverworld();
    void DrawGameplay();
    void DrawPauseScreen();
    void DrawGameOverActions();
    void DrawControlsPopup();
    void DrawQuitConfirmation();
    void DrawDebugCollision() const;

    GameMode mode{GameMode::Title};
    bool shouldQuit{false};
    Level level{};
    Player player{};
    Player player2{};
    Winch machineWinch{{350, 205, 70, 45}, 350.0f, 590.0f, 7.0f};
    Texture2D bobTexture{};
    Texture2D dobTexture{};
    Texture2D skullTexture{};
    Texture2D industrialTiles{};
    Texture2D industrialBackground{};
    Texture2D chainLinksTexture{};
    Texture2D enemyPlaceholderTexture{};

    bool showFPS{false};
    bool won{false};
    bool lost{false};
    bool playerAlive{true};
    bool player2Alive{true};
    Rectangle deathRect{80, 600, 31, 40};
    Rectangle playerDeathRect{80, 600, 31, 40};
    Rectangle player2DeathRect{124, 600, 31, 40};
    bool debugCollision{false};
    bool quitConfirmationOpen{false};
    bool controlsPopupOpen{false};
    bool titleModeMenuOpen{false};
    bool multiplayerEnabled{false};
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
};
