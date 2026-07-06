#include "Game.h"

#include "Collision.h"
#include "Constants.h"
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

    std::string OnOff(bool value) {
        return value ? "on" : "off";
    }

    bool WasButtonPressed(const MenuButton& button) {
        return button.enabled &&
            CheckCollisionPointRec(GetMousePosition(), button.rect) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    }

    void DrawMenuButton(const MenuButton& button) {
        bool hovered = button.enabled && CheckCollisionPointRec(GetMousePosition(), button.rect);
        Color fill = button.enabled ? (hovered ? Color{76, 91, 104, 235} : Color{34, 42, 52, 225}) : Color{28, 30, 34, 180};
        Color border = hovered ? ORANGE : Fade(RAYWHITE, 0.45f);
        Color textColor = button.enabled ? RAYWHITE : Fade(RAYWHITE, 0.42f);

        DrawRectangleRec(button.rect, fill);
        DrawRectangleLinesEx(button.rect, 2.0f, border);

        int fontSize = 24;
        int textWidth = MeasureText(button.text, fontSize);
        DrawText(
            button.text,
            static_cast<int>(button.rect.x + button.rect.width * 0.5f - textWidth * 0.5f),
            static_cast<int>(button.rect.y + button.rect.height * 0.5f - fontSize * 0.5f),
            fontSize,
            textColor
        );
    }

    void DrawCenteredText(const char* text, int centerX, int y, int fontSize, Color color) {
        int textWidth = MeasureText(text, fontSize);
        DrawText(text, centerX - textWidth / 2, y, fontSize, color);
    }

    bool HasArea(Rectangle rect) {
        return rect.width > 0.0f && rect.height > 0.0f;
    }
}

void Game::Run() {
    Load();

    while (!shouldQuit && !WindowShouldClose()) {
        Update(GetFrameTime());
        Draw();
    }

    Unload();
}

void Game::Load() {
    InitWindow(Constants::ScreenWidth, Constants::ScreenHeight, "Spin to Win - Power Pulley Panic");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    bobTexture = LoadTexture("assets/first_party/characters/Bob.png");
    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");

    if (bobTexture.id > 0) SetTextureFilter(bobTexture, TEXTURE_FILTER_POINT);
    if (industrialTiles.id > 0) SetTextureFilter(industrialTiles, TEXTURE_FILTER_POINT);
    if (industrialBackground.id > 0) SetTextureFilter(industrialBackground, TEXTURE_FILTER_POINT);

    InitializeOverworld();
    Reset();
}

void Game::Reset() {
    std::string levelId = "gatehouse";
    if (currentLevelNode >= 0 && currentLevelNode < static_cast<int>(overworldNodes.size())) {
        levelId = overworldNodes[currentLevelNode].id;
    }

    Level fallback = levelId == "rotary_latch_lab" ? CreateRotaryLatchLabLevel() : CreatePowerPulleyPanicLevel();
    level = LoadLevelFromFile("game_data/levels/" + levelId + ".level", fallback);
    for (RotaryLatch& latch : level.rotaryLatches) {
        ResetRotaryLatch(latch);
    }

    ResetPlayer(player);

    machineWinch.rect.x = machineWinch.startX;
    machineWinch.grabbed = false;

    won = false;
    lost = false;
    pulleyRotation = 0.0f;
    machinePhase = 0.0f;
    coyoteTimer = 0.0f;
    jumpBufferTimer = 0.0f;
    machinePower = 0.0f;
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
        {"lower_works", "3", "Lower Works", {670.0f, 535.0f}, false, false},
        {"counterweight_row", "4", "Counterweight Row", {875.0f, 405.0f}, false, false},
        {"foundry_lift", "5", "Foundry Lift", {1095.0f, 480.0f}, false, false},
        {"clocktower_core", "6", "Clocktower Core", {1310.0f, 335.0f}, false, false}
    };

    overworldPaths = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5}
    };

    selectedOverworldNode = 0;
}

void Game::OpenOverworld() {
    if (overworldNodes.empty()) {
        InitializeOverworld();
    }

    selectedOverworldNode = std::clamp(selectedOverworldNode, 0, static_cast<int>(overworldNodes.size()) - 1);
    mode = GameMode::Overworld;
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
        UpdateMachines(dt, 0.0f);
        if (levelClearTimer <= 0.0f) {
            CompleteCurrentLevelAndReturnToMap();
        }
        return;
    }

    float moveInput = 0.0f;
    if (!won && !lost) {
        if (IsKeyDown(KEY_A)) moveInput -= 1.0f;
        if (IsKeyDown(KEY_D)) moveInput += 1.0f;
    }

    UpdatePlayer(dt, moveInput);
    UpdateMachines(dt, moveInput);
    CheckFailureConditions();

    float gateBottom = 650.0f - machinePower * 170.0f;
    CheckWinCondition(gateBottom);
}

void Game::UpdateTitle() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        OpenOverworld();
    }

    std::vector<MenuButton> buttons{
        {{245, 525, 310, 46}, "New Game"},
        {{245, 581, 310, 46}, "Continue"},
        {{245, 637, 310, 46}, "Load Custom"},
        {{245, 693, 310, 46}, "Settings"},
        {{245, 749, 310, 46}, "Quit Game"}
    };

    if (WasButtonPressed(buttons[0])) {
        OpenOverworld();
    }
    else if (WasButtonPressed(buttons[1])) {
        OpenOverworld();
    }
    else if (WasButtonPressed(buttons[2])) {
        menuMessage = "Custom level loading is not available yet.";
    }
    else if (WasButtonPressed(buttons[3])) {
        menuMessage = "Settings are not available yet.";
    }
    else if (WasButtonPressed(buttons[4])) {
        quitConfirmationOpen = true;
        menuMessage.clear();
    }
}

void Game::UpdateOverworld() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = GameMode::Title;
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
        menuMessage.clear();
    }
    else if (WasButtonPressed(buttons[1])) {
        quitConfirmationOpen = true;
        menuMessage.clear();
    }

    Vector2 mouse = GetMousePosition();
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
        menuMessage = "Settings are not available yet.";
    }
    else if (WasButtonPressed(buttons[4])) {
        OpenOverworld();
    }
    else if (WasButtonPressed(buttons[5])) {
        mode = GameMode::Title;
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
        {{705, 710, 190, 46}, "Close"}
    };

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || WasButtonPressed(buttons[0])) {
        controlsPopupOpen = false;
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

void Game::UpdatePlayer(float dt, float moveInput) {
    if (moveInput < 0.0f) {
        player.facingRight = false;
    }
    else if (moveInput > 0.0f) {
        player.facingRight = true;
    }

    bool onLadder = CheckCollisionRecs(player.rect, level.ladder);
    bool climbing = onLadder && (IsKeyDown(KEY_W) || IsKeyDown(KEY_S)) && !won && !lost;
    player.walking = (moveInput != 0.0f || climbing) && !won && !lost;
    player.velocity.x = moveInput * Constants::PlayerSpeed;

    if (player.onGround) {
        coyoteTimer = Constants::CoyoteTime;
    }
    else {
        coyoteTimer -= dt;
    }

    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && !won && !lost) {
        jumpBufferTimer = Constants::JumpBufferTime;
    }
    else {
        jumpBufferTimer -= dt;
    }

    if (onLadder && IsKeyDown(KEY_W) && !won && !lost) {
        player.velocity.y = -Constants::PlayerClimbSpeed;
    }
    else if (onLadder && IsKeyDown(KEY_S) && !won && !lost) {
        player.velocity.y = Constants::PlayerClimbSpeed;
    }
    else {
        player.velocity.y += Constants::Gravity * dt;
    }

    if (jumpBufferTimer > 0.0f && coyoteTimer > 0.0f && !onLadder && !won && !lost) {
        player.velocity.y = Constants::PlayerJumpVelocity;
        player.onGround = false;
        coyoteTimer = 0.0f;
        jumpBufferTimer = 0.0f;
    }

    if (!won && !lost) {
        std::vector<Rectangle> solids = BuildSolids(level);

        player.rect.x += player.velocity.x * dt;
        ResolveHorizontal(player, solids);

        player.rect.y += player.velocity.y * dt;
        ResolveVertical(player, solids);
    }
}

void Game::UpdateMachines(float dt, float moveInput) {
    if (level.script == LevelScript::RotaryLatchLab) {
        machinePower = 0.0f;
        if (!won && !lost) {
            for (RotaryLatch& latch : level.rotaryLatches) {
                UpdateRotaryLatch(latch, player, 1.0f, dt);
            }
        }
        return;
    }

    float winchDelta = 0.0f;
    if (!won && !lost) {
        winchDelta = UpdateWinch(machineWinch, player, moveInput, dt);
    }

    machinePower = GetMachinePower(machineWinch);
    float spinAmount = fabsf(winchDelta) * 3.0f + machinePower * 140.0f * dt;
    pulleyRotation += spinAmount;
    machinePhase += (0.35f + machinePower * 3.3f) * dt;

    UpdateHangingWeights(level.weights, machinePower, machinePhase);
}

void Game::CheckFailureConditions() {
    if (won || lost) return;

    if (player.rect.y > Constants::ScreenHeight ||
        (HasArea(level.spikeHazard) && CheckCollisionRecs(player.rect, level.spikeHazard))) {
        lost = true;
    }

    for (const HangingWeight& weight : level.weights) {
        if (CheckCollisionRecs(player.rect, weight.rect)) {
            lost = true;
        }
    }
}

void Game::CheckWinCondition(float gateBottom) {
    if (won || lost) return;

    bool latchesComplete = level.rotaryLatches.empty() || AreAllRotaryLatchesLatched(level.rotaryLatches);
    if (!HasArea(level.exitTrigger)) {
        return;
    }

    Vector2 playerCenter{
        player.rect.x + player.rect.width * 0.5f,
        player.rect.y + player.rect.height * 0.5f
    };
    float doorwayCenterX = level.exitTrigger.x + level.exitTrigger.width * 0.5f;
    bool playerCenteredInDoorway = fabsf(playerCenter.x - doorwayCenterX) <= 2.0f;
    bool hasFactoryMachine = level.pulleys.size() >= 5;
    bool doorUnlocked = latchesComplete && (!hasFactoryMachine || gateBottom <= player.rect.y + 4.0f);

    if (doorUnlocked && playerCenteredInDoorway && CheckCollisionPointRec(playerCenter, level.exitTrigger)) {
        BeginLevelClear();
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (mode == GameMode::Title) {
        DrawTitleScreen();
        if (quitConfirmationOpen) {
            DrawQuitConfirmation();
        }
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        EndDrawing();
        return;
    }

    if (mode == GameMode::Overworld) {
        DrawOverworld();
        if (quitConfirmationOpen) {
            DrawQuitConfirmation();
        }
        console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);
        EndDrawing();
        return;
    }

    DrawGameplay();

    if (mode == GameMode::Paused) {
        DrawPauseScreen();
    }

    if (controlsPopupOpen) {
        DrawControlsPopup();
    }

    if (quitConfirmationOpen) {
        DrawQuitConfirmation();
    }

    if (debugCollision) {
        DrawDebugCollision();
    }

    console.Draw(Constants::ScreenWidth, Constants::ScreenHeight);

    EndDrawing();
}

void Game::DrawTitleScreen() {
    Rectangle backgroundTile{0, 0, 32, 32};
    DrawTiledTextureRect(industrialBackground, backgroundTile, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.45f));

    Vector2 titlePosition{240.0f, 190.0f};
    DrawText("POWER", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y), 96, RAYWHITE);
    DrawText("PULLEY", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 92.0f), 96, ORANGE);
    DrawText("PANIC", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 184.0f), 96, RAYWHITE);

    float rotation = static_cast<float>(GetTime()) * 80.0f;
    DrawLineEx({930, 285}, {1200, 410}, 8.0f, BROWN);
    DrawLineEx({1010, 540}, {1260, 415}, 8.0f, BROWN);
    DrawPulley({910, 280}, 70, rotation, RAYWHITE);
    DrawPulley({1210, 415}, 82, rotation * -0.75f, RAYWHITE);
    DrawPulley({1000, 545}, 56, rotation * 1.2f, RAYWHITE);

    std::vector<MenuButton> buttons{
        {{245, 525, 310, 46}, "New Game"},
        {{245, 581, 310, 46}, "Continue"},
        {{245, 637, 310, 46}, "Load Custom"},
        {{245, 693, 310, 46}, "Settings"},
        {{245, 749, 310, 46}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    if (!menuMessage.empty()) {
        DrawText(menuMessage.c_str(), 590, 760, 22, LIGHTGRAY);
    }

    DrawText("` opens console", 245, 820, 22, LIGHTGRAY);
}

void Game::DrawOverworld() {
    Rectangle backgroundTile{0, 0, 32, 32};
    DrawTiledTextureRect(industrialBackground, backgroundTile, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.34f));

    DrawRectangle(0, 690, Constants::ScreenWidth, 235, Color{20, 24, 28, 215});
    for (int x = 0; x < Constants::ScreenWidth; x += 70) {
        DrawLineEx({static_cast<float>(x), 690.0f}, {static_cast<float>(x + 120), 925.0f}, 2.0f, Fade(BLACK, 0.22f));
    }

    DrawText("FACTORY DISTRICT", 95, 70, 54, RAYWHITE);
    DrawText("Level Select", 100, 128, 28, ORANGE);

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
        bool hovered = CheckCollisionPointCircle(GetMousePosition(), node.position, 34.0f);

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
        int labelSize = node.unlocked ? 26 : 24;
        DrawCenteredText(label, static_cast<int>(node.position.x), static_cast<int>(node.position.y - labelSize * 0.5f), labelSize, node.unlocked ? RAYWHITE : Fade(RAYWHITE, 0.38f));
    }

    Rectangle panel{1030, 510, 460, 220};
    DrawRectangleRec(panel, Color{20, 27, 34, 235});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.45f));

    const OverworldNode& selectedNode = overworldNodes[selectedOverworldNode];
    DrawText(selectedNode.name.c_str(), 1060, 540, 28, RAYWHITE);
    DrawText(TextFormat("Node %s", selectedNode.label.c_str()), 1060, 580, 22, ORANGE);
    DrawText(selectedNode.unlocked ? "Status: Unlocked" : "Status: Locked", 1060, 614, 22, selectedNode.unlocked ? GREEN : LIGHTGRAY);
    DrawText(selectedNode.unlocked ? "Enter / click starts the level" : "Complete earlier levels to unlock", 1060, 648, 20, LIGHTGRAY);

    std::vector<MenuButton> buttons{
        {{1240, 760, 250, 46}, "Back to Title"},
        {{1240, 816, 250, 46}, "Quit Game"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    DrawText("A/D or arrows select    Enter starts    Esc returns", 95, 825, 22, LIGHTGRAY);
    DrawText("` opens console", 95, 858, 22, LIGHTGRAY);

    if (!menuMessage.empty()) {
        DrawText(menuMessage.c_str(), 1060, 682, 20, ORANGE);
    }
}

void Game::DrawPauseScreen() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.58f));

    constexpr float buttonWidth = 350.0f;
    constexpr float buttonHeight = 46.0f;
    constexpr float buttonGap = 10.0f;
    float menuX = Constants::ScreenWidth * 0.5f - buttonWidth * 0.5f;

    DrawCenteredText("PAUSED", Constants::ScreenWidth / 2, 300, 72, RAYWHITE);

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
        int textWidth = MeasureText(menuMessage.c_str(), 22);
        DrawText(menuMessage.c_str(), Constants::ScreenWidth / 2 - textWidth / 2, 810, 22, LIGHTGRAY);
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

    Rectangle panel{430, 245, 740, 530};
    int panelCenterX = static_cast<int>(panel.x + panel.width * 0.5f);

    DrawRectangleRec(panel, Color{22, 28, 35, 248});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.55f));

    DrawCenteredText("Controls", panelCenterX, 282, 42, RAYWHITE);

    struct ControlRow {
        const char* action;
        const char* input;
    };

    const std::vector<ControlRow> rows{
        {"Move", "A / D"},
        {"Jump", "Space"},
        {"Climb", "W / S"},
        {"Grab Winch", "Hold E"},
        {"Lock Wheel", "E near aligned wheel"},
        {"Pause / Resume", "Esc / Enter"},
        {"Map Select", "A / D or Arrow Keys"},
        {"Select / Start", "Enter / Space"},
        {"Console", "` / ~"}
    };

    int startY = 350;
    int rowHeight = 34;
    int actionX = 525;
    int inputX = 795;
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        int y = startY + i * rowHeight;
        Color rowColor = i % 2 == 0 ? Fade(RAYWHITE, 0.06f) : Fade(RAYWHITE, 0.025f);
        DrawRectangle(500, y - 4, 600, 30, rowColor);
        DrawText(rows[i].action, actionX, y, 22, LIGHTGRAY);
        DrawText(rows[i].input, inputX, y, 22, RAYWHITE);
    }

    std::vector<MenuButton> buttons{
        {{705, 710, 190, 46}, "Close"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }
}

void Game::DrawQuitConfirmation() {
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(BLACK, 0.45f));

    Rectangle panel{450, 330, 700, 260};
    int panelCenterX = static_cast<int>(panel.x + panel.width * 0.5f);

    DrawRectangleRec(panel, Color{22, 28, 35, 245});
    DrawRectangleLinesEx(panel, 2.0f, Fade(RAYWHITE, 0.55f));

    DrawCenteredText("Quit Game?", panelCenterX, 370, 42, RAYWHITE);
    DrawCenteredText("Unsaved progress in this session will be lost.", panelCenterX, 430, 22, LIGHTGRAY);

    std::vector<MenuButton> buttons{
        {{575, 500, 190, 46}, "Yes"},
        {{835, 500, 190, 46}, "No"}
    };

    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    DrawCenteredText("Enter/Y confirms    Esc/N cancels", panelCenterX, 552, 18, Fade(RAYWHITE, 0.72f));
}

void Game::DrawGameplay() {
    if (showFPS) DrawFPS(10, 10);

    Rectangle backgroundTile{0, 0, 32, 32};
    Rectangle wallTile{0, 0, 32, 32};
    Rectangle platformTile{32, 0, 32, 32};
    Rectangle darkPitTile{64, 64, 32, 32};

    DrawTiledTextureRect(industrialBackground, backgroundTile, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);

    for (const Rectangle& solid : level.baseSolids) {
        DrawTiledTextureRect(industrialTiles, wallTile, solid, WHITE);
        DrawRectangleLinesEx(solid, 2, BLACK);
    }

    if (HasArea(level.ladder)) {
        DrawLineEx({level.ladder.x + 8, level.ladder.y}, {level.ladder.x + 8, level.ladder.y + level.ladder.height}, 4, BLACK);
        DrawLineEx({level.ladder.x + level.ladder.width - 8, level.ladder.y}, {level.ladder.x + level.ladder.width - 8, level.ladder.y + level.ladder.height}, 4, BLACK);
        for (int i = 0; i < 13; i++) {
            float y = level.ladder.y + i * 30;
            DrawLineEx({level.ladder.x + 8, y}, {level.ladder.x + level.ladder.width - 8, y}, 3, BLACK);
        }
    }

    bool hasFactoryMachine = level.pulleys.size() >= 5;
    float flicker = 0.0f;
    if (hasFactoryMachine) {
        DrawWinch(machineWinch);
        DrawText(machineWinch.grabbed ? "Move to push" : "Press E", static_cast<int>(machineWinch.rect.x - 12.0f), static_cast<int>(machineWinch.rect.y - 28.0f), 18, machineWinch.grabbed ? ORANGE : BLACK);

        DrawLineEx({machineWinch.rect.x + machineWinch.rect.width, machineWinch.rect.y + 20}, {level.pulleys[0].x - 38, level.pulleys[0].y - 38}, 5, BROWN);
        DrawLineEx({level.pulleys[0].x, level.pulleys[0].y + 55}, {level.pulleys[1].x, level.pulleys[1].y - 45}, 5, BROWN);
        DrawLineEx({level.pulleys[1].x, level.pulleys[1].y + 45}, {level.pulleys[2].x, level.pulleys[2].y - 45}, 5, BROWN);
        DrawLineEx({level.pulleys[2].x, level.pulleys[2].y + 45}, {level.pulleys[3].x, level.pulleys[3].y - 45}, 5, BROWN);
        DrawLineEx({level.pulleys[3].x, level.pulleys[3].y + 45}, {level.pulleys[4].x, level.pulleys[4].y - 55}, 5, BROWN);

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
        float mainRopeX = level.pulleys[0].x + 55.0f;
        DrawLineEx({mainRopeX, level.pulleys[0].y}, {mainRopeX, mainWeightY}, 5, BROWN);
        DrawRectangle(mainRopeX - 25, mainWeightY, 50, 60, GRAY);
        DrawRectangleLines(mainRopeX - 25, mainWeightY, 50, 60, BLACK);
        DrawLineEx({mainRopeX, mainWeightY + 60}, generatorGear, 4, BROWN);

        bool lightsOn = machinePower > 0.12f;
        Color wireColor = lightsOn ? BLUE : DARKBLUE;

        DrawLineEx({655, 400}, {760, 400}, 4, wireColor);
        DrawLineEx({760, 400}, {760, 320}, 4, wireColor);
        DrawLineEx({760, 320}, {1220, 320}, 4, wireColor);
        DrawLineEx({1220, 320}, {1220, 598}, 4, wireColor);
        DrawLineEx({1220, 598}, {1370, 598}, 4, wireColor);

        float flickerCycle = fmodf(static_cast<float>(GetTime()), 3.4f);
        if (machinePower < 0.65f && flickerCycle < 0.22f) {
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
        DrawHazardWeight(weight);
    }

    if (HasArea(level.spikeHazard)) {
        DrawTiledTextureRect(industrialTiles, darkPitTile, {300, 650, 960, 160}, Fade(WHITE, 0.85f));
        DrawSpikes(302, 805, 34);
    }

    for (Rectangle platform : level.pitPlatforms) {
        DrawTiledTextureRect(industrialTiles, platformTile, platform, WHITE);
        DrawRectangleLinesEx(platform, 2, BLACK);
    }

    int latchedCount = 0;
    for (const RotaryLatch& latch : level.rotaryLatches) {
        bool playerNear = CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player.rect);
        if (latch.latched) {
            latchedCount++;
        }

        DrawRotaryLatch(latch, playerNear);
    }

    float gateBottom = 650.0f - machinePower * 170.0f;
    if (HasArea(level.exitTrigger)) {
        if (hasFactoryMachine) {
            Rectangle gateMotorBox{1370, 565, 85, 65};
            Vector2 gateMotorGear{
                gateMotorBox.x + gateMotorBox.width * 0.42f,
                gateMotorBox.y + gateMotorBox.height * 0.48f
            };
            DrawMachineBox(gateMotorBox, pulleyRotation * 1.4f, machinePower > 0.04f);
            DrawLineEx({level.pulleys[4].x - 48.0f, level.pulleys[4].y + 27.0f}, {gateMotorGear.x - 25.0f, gateMotorGear.y - 7.0f}, 5, BROWN);
            DrawLineEx({level.pulleys[4].x + 24.0f, level.pulleys[4].y + 50.0f}, {gateMotorGear.x + 23.0f, gateMotorGear.y + 10.0f}, 5, BROWN);
            DrawRing(gateMotorGear, 22.0f, 27.0f, 0.0f, 360.0f, 32, BROWN);
        }
        else {
            bool allLatchesLocked = !level.rotaryLatches.empty() && latchedCount == static_cast<int>(level.rotaryLatches.size());
            gateBottom = allLatchesLocked ? level.exitTrigger.y : level.exitTrigger.y + level.exitTrigger.height;
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

    DrawPlayer(player, bobTexture);

    if (HasArea(level.darknessArea) || HasArea(level.rightDarknessArea)) {
        float blackoutFlicker = machinePower > 0.02f ? flicker : 0.0f;
        float safeAreaDimAlpha = Clamp01(0.20f + (1.0f - machinePower) * 0.18f - flicker * 0.45f);
        float blackoutAlpha = Clamp01(1.0f - machinePower - blackoutFlicker);
        DrawRectangle(0, 0, 300, Constants::ScreenHeight, Fade(BLACK, safeAreaDimAlpha));
        DrawRectangle(300, 0, 960, 275, Fade(BLACK, safeAreaDimAlpha));
        DrawRectangleRec(level.darknessArea, Fade(BLACK, blackoutAlpha));
        DrawRectangleRec(level.rightDarknessArea, Fade(BLACK, blackoutAlpha));
    }

    int latchTotal = static_cast<int>(level.rotaryLatches.size());
    if (latchTotal > 0) {
        bool allLatchesLocked = latchedCount == latchTotal;
        DrawRectangle(20, 20, 260, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Wheel locks: %d / %d", latchedCount, latchTotal), 34, 32, 22, RAYWHITE);
        DrawText(allLatchesLocked ? "Gate circuit complete" : "Align spokes, press E", 34, 60, 18, allLatchesLocked ? GREEN : ORANGE);
    }

    if (won) {
        DrawText("LEVEL CLEAR!", 615, 420, 60, GREEN);
        DrawText("Returning to map...", 650, 485, 26, machinePower < 0.45f && HasArea(level.darknessArea) ? WHITE : BLACK);
    }

    if (lost) {
        DrawText("YOU DIED!", 660, 420, 54, RED);
        DrawGameOverActions();
    }
}

void Game::Unload() {
    UnloadTexture(bobTexture);
    UnloadTexture(industrialTiles);
    UnloadTexture(industrialBackground);
    CloseWindow();
}

void Game::ExecuteConsoleCommand(const std::string& line) {
    std::vector<std::string> args = SplitCommandLine(line);
    if (args.empty()) return;

    std::string command = ToLower(args[0]);
    console.AddLine("> " + line);

    if (command == "help") {
        console.AddLine("Commands: help, clear, start, overworld, title, pause, resume, quit, reset, win, kill, fps, debug_collision, unlock_all_levels, teleport, power, player, machine");
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
    else if (command == "kill") {
        mode = GameMode::Playing;
        lost = true;
        won = false;
        console.AddLine("Loss state set.");
    }
    else if (command == "fps") {
        showFPS = !showFPS;
        console.AddLine("FPS display " + OnOff(showFPS) + ".");
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
        mode = GameMode::Playing;
        won = false;
        lost = false;
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

    for (const HangingWeight& weight : level.weights) {
        DrawRectangleLinesEx(weight.rect, 2, Fade(RED, 0.95f));
    }

    for (const RotaryLatch& latch : level.rotaryLatches) {
        DrawCircleLinesV(latch.center, latch.radius + 20.0f, Fade(PURPLE, 0.9f));
    }

    DrawRectangleLinesEx(player.rect, 2, Fade(ORANGE, 0.95f));
}
