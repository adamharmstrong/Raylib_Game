#pragma once

#include "Level.h"
#include "Player.h"
#include "raylib.h"

class Game {
public:
    void Run();

private:
    void Load();
    void Reset();
    void Update(float dt);
    void Draw();
    void Unload();

    void UpdatePlayer(float dt, float moveInput);
    void UpdateMachines(float dt, float moveInput);
    void CheckFailureConditions();
    void CheckWinCondition(float gateBottom);

    Level level{};
    Player player{};
    Winch machineWinch{{350, 205, 70, 45}, 350.0f, 590.0f, 7.0f};
    Texture2D bobTexture{};
    Texture2D industrialTiles{};
    Texture2D industrialBackground{};

    bool showFPS{false};
    bool won{false};
    bool lost{false};

    float pulleyRotation{0.0f};
    float machinePhase{0.0f};
    float coyoteTimer{0.0f};
    float jumpBufferTimer{0.0f};
    float machinePower{0.0f};
};
