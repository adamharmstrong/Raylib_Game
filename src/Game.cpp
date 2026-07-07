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

    bool HasWaterPit(const Level& level) {
        return HasArea(level.waterPit.bounds);
    }

    Rectangle GetFilledWaterRect(const WaterPit& waterPit) {
        float pitBottom = waterPit.bounds.y + waterPit.bounds.height;
        float surfaceY = fminf(pitBottom, fmaxf(waterPit.surfaceY, waterPit.bounds.y));
        return {waterPit.bounds.x, surfaceY, waterPit.bounds.width, pitBottom - surfaceY};
    }

    bool IsRectInWater(Rectangle rect, const WaterPit& waterPit) {
        Rectangle waterRect = GetFilledWaterRect(waterPit);
        return waterRect.height > 0.0f && CheckCollisionRecs(rect, waterRect);
    }

    bool IsPlayerSwimming(const Player& player, const Level& level) {
        if (!HasWaterPit(level)) {
            return false;
        }

        Rectangle waterRect = GetFilledWaterRect(level.waterPit);
        Vector2 playerCenter{
            player.rect.x + player.rect.width * 0.5f,
            player.rect.y + player.rect.height * 0.5f
        };
        return waterRect.height > 0.0f && CheckCollisionPointRec(playerCenter, waterRect);
    }

    float GetWaterFillProgress(const WaterPit& waterPit) {
        float totalTravel = waterPit.bounds.y + waterPit.bounds.height - waterPit.targetSurfaceY;
        if (totalTravel <= 0.0f) {
            return 0.0f;
        }

        float currentTravel = waterPit.bounds.y + waterPit.bounds.height - waterPit.surfaceY;
        return Clamp01(currentTravel / totalTravel);
    }

    bool HorizontallyOverlaps(Rectangle a, Rectangle b) {
        return a.x < b.x + b.width && a.x + a.width > b.x;
    }

    bool HasSolidTouchingTop(Rectangle rect, const std::vector<Rectangle>& solids) {
        constexpr float Epsilon = 0.5f;
        for (const Rectangle& other : solids) {
            if (fabsf((other.y + other.height) - rect.y) <= Epsilon && HorizontallyOverlaps(rect, other)) {
                return true;
            }
        }

        return false;
    }

    float RectCenterX(Rectangle rect) {
        return rect.x + rect.width * 0.5f;
    }

    std::vector<Rectangle> BuildStoneBlockSolids(const std::vector<StoneBlock>& blocks, int ignoredBlock) {
        std::vector<Rectangle> solids;
        for (int i = 0; i < static_cast<int>(blocks.size()); i++) {
            if (i != ignoredBlock) {
                solids.push_back(blocks[i].rect);
            }
        }
        return solids;
    }

    float GetStoneBlockPushScale(const StoneBlock& block) {
        return 0.75f / fmaxf(1.0f, block.mass);
    }

    void ResolveStoneBlockHorizontal(StoneBlock& block, const std::vector<Rectangle>& solids, const std::vector<StoneBlock>& blocks, int blockIndex) {
        std::vector<Rectangle> collisionRects = solids;
        std::vector<Rectangle> otherBlocks = BuildStoneBlockSolids(blocks, blockIndex);
        collisionRects.insert(collisionRects.end(), otherBlocks.begin(), otherBlocks.end());

        for (const Rectangle& solid : collisionRects) {
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

    void ResolveStoneBlockVertical(StoneBlock& block, const std::vector<Rectangle>& solids, const std::vector<StoneBlock>& blocks, int blockIndex) {
        block.onGround = false;
        std::vector<Rectangle> collisionRects = solids;
        std::vector<Rectangle> otherBlocks = BuildStoneBlockSolids(blocks, blockIndex);
        collisionRects.insert(collisionRects.end(), otherBlocks.begin(), otherBlocks.end());

        for (const Rectangle& solid : collisionRects) {
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
        float leftX = seeSaw.pivot.x - seeSaw.length * 0.5f;
        float rightX = seeSaw.pivot.x + seeSaw.length * 0.5f;
        float leftY = GetSeeSawSurfaceY(seeSaw, leftX);
        float rightY = GetSeeSawSurfaceY(seeSaw, rightX);
        float minX = fminf(leftX, rightX);
        float maxX = fmaxf(leftX, rightX);
        float minY = fminf(leftY, rightY) - seeSaw.thickness;
        float maxY = fmaxf(leftY, rightY) + seeSaw.thickness;

        return {minX, minY, maxX - minX, maxY - minY};
    }

    std::vector<Rectangle> BuildChainColliders(const Level& level, const Player& player) {
        std::vector<Rectangle> colliders = BuildSolids(level);
        colliders.push_back(player.rect);

        for (const StoneBlock& block : level.stoneBlocks) {
            colliders.push_back(block.rect);
        }

        for (const HangingWeight& weight : level.weights) {
            colliders.push_back(weight.rect);
        }

        for (const SeeSaw& seeSaw : level.seeSaws) {
            colliders.push_back(GetSeeSawBounds(seeSaw));
        }

        return colliders;
    }

    float MoveTowardsFloat(float current, float target, float maxDelta) {
        if (current < target) {
            return fminf(current + maxDelta, target);
        }

        return fmaxf(current - maxDelta, target);
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
    skullTexture = LoadTexture("assets/first_party/characters/skull.png");
    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");
    chainLinksTexture = LoadTexture("assets/first_party/machines/chain_links.png");
    // Placeholder only: swap this before final enemy art lock.
    enemyPlaceholderTexture = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/6. Character Animations 32p/Anim_Robot_Walk1_v1.1_spritesheet.png");

    if (bobTexture.id > 0) SetTextureFilter(bobTexture, TEXTURE_FILTER_POINT);
    if (skullTexture.id > 0) SetTextureFilter(skullTexture, TEXTURE_FILTER_POINT);
    if (industrialTiles.id > 0) SetTextureFilter(industrialTiles, TEXTURE_FILTER_POINT);
    if (industrialBackground.id > 0) SetTextureFilter(industrialBackground, TEXTURE_FILTER_POINT);
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

    ResetPlayer(player);
    deathRect = player.rect;

    machineWinch.rect.x = machineWinch.startX;
    machineWinch.grabbed = false;

    won = false;
    lost = false;
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
        {"test_level", "T", "Test Level", {1410.0f, 430.0f}, true, false}
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
    UpdateEnemies(dt);
    CheckFailureConditions();

    CheckWinCondition(gateBottom);
}

void Game::UpdateTitle() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        OpenOverworld();
    }

    std::vector<MenuButton> buttons{
        {{1080, 510, 310, 46}, "New Game"},
        {{1080, 566, 310, 46}, "Continue"},
        {{1080, 622, 310, 46}, "Load Custom"},
        {{1080, 678, 310, 46}, "Settings"},
        {{1080, 734, 310, 46}, "Quit Game"}
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
    bool controlsEnabled = !won && !lost;

    if (moveInput < 0.0f) {
        player.facingRight = false;
    }
    else if (moveInput > 0.0f) {
        player.facingRight = true;
    }

    bool onLadder = CheckCollisionRecs(player.rect, level.ladder);
    bool swimming = IsPlayerSwimming(player, level);
    player.climbing = onLadder && !swimming && (IsKeyDown(KEY_W) || IsKeyDown(KEY_S)) && controlsEnabled;
    player.walking = (fabsf(player.velocity.x) > 8.0f || player.climbing) && controlsEnabled;

    float maxMoveSpeed = swimming ? 170.0f : Constants::PlayerSpeed;
    float targetSpeed = controlsEnabled ? moveInput * maxMoveSpeed : 0.0f;
    bool applyingInput = fabsf(targetSpeed) > 0.0f;
    bool changingDirection = (targetSpeed < 0.0f && player.velocity.x > 0.0f) || (targetSpeed > 0.0f && player.velocity.x < 0.0f);
    float acceleration = swimming
        ? (applyingInput ? 820.0f : 620.0f)
        : player.onGround
        ? (applyingInput ? Constants::PlayerGroundAcceleration : Constants::PlayerGroundDeceleration)
        : (applyingInput ? Constants::PlayerAirAcceleration : Constants::PlayerAirDeceleration);
    if (changingDirection && player.onGround && !swimming) {
        acceleration = Constants::PlayerGroundDeceleration;
    }
    player.velocity.x = ApproachFloat(player.velocity.x, targetSpeed, acceleration * dt);

    if (player.onGround) {
        player.coyoteTimer = Constants::CoyoteTime;
    }
    else {
        player.coyoteTimer -= dt;
    }

    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && controlsEnabled) {
        player.jumpBufferTimer = Constants::JumpBufferTime;
    }
    else {
        player.jumpBufferTimer -= dt;
    }

    if (swimming && controlsEnabled) {
        float swimInput = 0.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE)) swimInput -= 1.0f;
        if (IsKeyDown(KEY_S)) swimInput += 1.0f;

        float targetSwimSpeed = swimInput * 165.0f;
        float swimAcceleration = fabsf(targetSwimSpeed) > 0.0f ? 920.0f : 680.0f;
        player.velocity.y = ApproachFloat(player.velocity.y, targetSwimSpeed, swimAcceleration * dt);
        player.velocity.y = fminf(player.velocity.y, 210.0f);
        player.onGround = false;
        player.coyoteTimer = 0.0f;
    }
    else if (onLadder && IsKeyDown(KEY_W) && controlsEnabled) {
        player.velocity.y = -Constants::PlayerClimbSpeed;
    }
    else if (onLadder && IsKeyDown(KEY_S) && controlsEnabled) {
        player.velocity.y = Constants::PlayerClimbSpeed;
    }
    else {
        player.velocity.y += Constants::Gravity * dt;
        player.velocity.y = fminf(player.velocity.y, Constants::PlayerMaxFallSpeed);
    }

    if (player.jumpBufferTimer > 0.0f && player.coyoteTimer > 0.0f && !onLadder && !swimming && controlsEnabled) {
        player.velocity.y = Constants::PlayerJumpVelocity;
        player.onGround = false;
        player.coyoteTimer = 0.0f;
        player.jumpBufferTimer = 0.0f;
    }

    if ((IsKeyReleased(KEY_W) || IsKeyReleased(KEY_SPACE)) && player.velocity.y < 0.0f && !onLadder && !swimming && controlsEnabled) {
        player.velocity.y *= Constants::PlayerJumpCutMultiplier;
    }

    if (player.walking) {
        player.animationTimer += dt;
    }
    else {
        player.animationTimer = 0.0f;
    }

    if (controlsEnabled) {
        std::vector<Rectangle> solids = BuildSolids(level);

        player.rect.x += player.velocity.x * dt;
        ResolveHorizontal(player, solids);

        for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
            StoneBlock& block = level.stoneBlocks[i];
            if (!CheckCollisionRecs(player.rect, block.rect)) {
                continue;
            }

            if (player.velocity.x > 0.0f) {
                block.velocity.x = player.velocity.x * GetStoneBlockPushScale(block);
                block.rect.x += block.velocity.x * dt;
                ResolveStoneBlockHorizontal(block, solids, level.stoneBlocks, i);
                player.rect.x = block.rect.x - player.rect.width;
            }
            else if (player.velocity.x < 0.0f) {
                block.velocity.x = player.velocity.x * GetStoneBlockPushScale(block);
                block.rect.x += block.velocity.x * dt;
                ResolveStoneBlockHorizontal(block, solids, level.stoneBlocks, i);
                player.rect.x = block.rect.x + block.rect.width;
            }
        }

        player.rect.y += player.velocity.y * dt;
        std::vector<Rectangle> playerSolids = solids;
        for (const StoneBlock& block : level.stoneBlocks) {
            playerSolids.push_back(block.rect);
        }

        ResolveVertical(player, playerSolids);
        ResolveSeeSawStanding(player.rect, player.velocity, player.onGround, level.seeSaws);
    }
}

void Game::UpdateMachines(float dt, float moveInput) {
    if (!won && !lost) {
        UpdateStoneBlocksAndSeeSaws(dt);
        std::vector<Rectangle> chainColliders = BuildChainColliders(level, player);
        for (Chain& chain : level.chains) {
            UpdateChainPhysics(chain, chainColliders, dt);
        }
    }

    if (level.script == LevelScript::FloodedFoundry) {
        if (!won && !lost) {
            bool playerNearValve = CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
            if (playerNearValve && IsKeyPressed(KEY_E)) {
                level.valve.opened = true;
                level.waterPit.filling = true;
            }

            if (level.waterPit.filling) {
                level.waterPit.surfaceY = MoveTowardsFloat(level.waterPit.surfaceY, level.waterPit.targetSurfaceY, level.waterPit.fillRate * dt);
            }
        }

        machinePower = GetWaterFillProgress(level.waterPit);
        if (HasArea(level.exitTrigger)) {
            gateBottom = MoveTowardsFloat(gateBottom, level.exitTrigger.y, 190.0f * dt);
        }
        return;
    }

    if (level.script == LevelScript::RotaryLatchLab) {
        machinePower = 0.0f;
        if (!won && !lost) {
            for (RotaryLatch& latch : level.rotaryLatches) {
                UpdateRotaryLatch(latch, player, 1.0f, dt);
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

    float winchDelta = 0.0f;
    if (!won && !lost) {
        winchDelta = UpdateWinch(machineWinch, player, moveInput, dt);
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
    for (const StoneBlock& block : level.stoneBlocks) {
        solids.push_back(block.rect);
    }

    for (Enemy& enemy : level.enemies) {
        float direction = enemy.facingRight ? 1.0f : -1.0f;
        enemy.velocity.x = direction * enemy.speed;
        enemy.velocity.y += Constants::Gravity * dt;

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
        ResolveSeeSawStanding(enemy.rect, enemy.velocity, enemy.onGround, level.seeSaws);
        enemy.walking = enemy.onGround && enemy.speed > 0.0f;
    }
}

void Game::UpdateStoneBlocksAndSeeSaws(float dt) {
    std::vector<Rectangle> solids = BuildSolids(level);

    for (int i = 0; i < static_cast<int>(level.stoneBlocks.size()); i++) {
        StoneBlock& block = level.stoneBlocks[i];

        if (block.onGround) {
            block.velocity.x *= powf(0.000001f, dt);
            if (fabsf(block.velocity.x) < 12.0f) {
                block.velocity.x = 0.0f;
            }
        }

        block.velocity.y += Constants::Gravity * dt;

        block.rect.x += block.velocity.x * dt;
        ResolveStoneBlockHorizontal(block, solids, level.stoneBlocks, i);

        block.rect.y += block.velocity.y * dt;
        ResolveStoneBlockVertical(block, solids, level.stoneBlocks, i);
        ResolveSeeSawStanding(block.rect, block.velocity, block.onGround, level.seeSaws);
    }

    for (SeeSaw& seeSaw : level.seeSaws) {
        float torque = GetSeeSawTorqueContribution(seeSaw, player.rect, 1.0f);
        for (const StoneBlock& block : level.stoneBlocks) {
            torque += GetSeeSawTorqueContribution(seeSaw, block.rect, block.mass);
        }

        float targetAngle = std::clamp(torque * 13.0f, seeSaw.minAngle, seeSaw.maxAngle);
        seeSaw.angle = LerpFloat(seeSaw.angle, targetAngle, Clamp01(seeSaw.response * dt));
    }
}

void Game::KillPlayer() {
    if (lost) {
        return;
    }

    deathRect = player.rect;
    player.velocity = {0.0f, 0.0f};
    player.walking = false;
    player.climbing = false;
    won = false;
    lost = true;
}

void Game::CheckFailureConditions() {
    if (won || lost) return;

    bool protectedByWater = HasWaterPit(level) &&
        IsRectInWater(player.rect, level.waterPit) &&
        level.waterPit.surfaceY <= level.spikeHazard.y;

    if (player.rect.y > Constants::ScreenHeight ||
        (HasArea(level.spikeHazard) && CheckCollisionRecs(player.rect, level.spikeHazard) && !protectedByWater)) {
        KillPlayer();
    }

    for (const HangingWeight& weight : level.weights) {
        if (CheckCollisionRecs(player.rect, weight.rect)) {
            KillPlayer();
        }
    }

    for (const Enemy& enemy : level.enemies) {
        if (CheckCollisionRecs(player.rect, enemy.rect)) {
            KillPlayer();
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
    bool doorRaisedPastPlayer = gateBottom <= player.rect.y + 4.0f;
    bool doorUnlocked = latchesComplete && doorRaisedPastPlayer && (!hasFactoryMachine || machinePower > 0.05f);

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
    DrawAtomicRealmBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, Fade(WHITE, 0.72f));
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(Color{9, 14, 20, 255}, 0.42f));

    DrawAtomicRealmSolid(industrialTiles, {0, 690, static_cast<float>(Constants::ScreenWidth), 210}, WHITE);
    DrawAtomicRealmSolid(industrialTiles, {92, 94, 530, 32}, Fade(WHITE, 0.95f));
    DrawAtomicRealmSolid(industrialTiles, {1030, 442, 410, 32}, Fade(WHITE, 0.95f));
    DrawAtomicRealmSolid(industrialTiles, {1450, 270, 32, 420}, Fade(WHITE, 0.9f));

    DrawRectangle(80, 145, 720, 390, Fade(BLACK, 0.24f));
    DrawRectangleLinesEx({80, 145, 720, 390}, 2.0f, Fade(RAYWHITE, 0.16f));

    Vector2 titlePosition{120.0f, 176.0f};
    DrawText("POWER", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 6.0f), 96, Fade(BLACK, 0.55f));
    DrawText("PULLEY", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 105.0f), 96, Fade(BLACK, 0.55f));
    DrawText("PANIC", static_cast<int>(titlePosition.x + 5.0f), static_cast<int>(titlePosition.y + 204.0f), 96, Fade(BLACK, 0.55f));
    DrawText("POWER", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y), 96, RAYWHITE);
    DrawText("PULLEY", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 99.0f), 96, ORANGE);
    DrawText("PANIC", static_cast<int>(titlePosition.x), static_cast<int>(titlePosition.y + 198.0f), 96, RAYWHITE);
    DrawText("Spin to Win", 126, 492, 30, Fade(RAYWHITE, 0.82f));

    float rotation = static_cast<float>(GetTime()) * 80.0f;
    DrawLineEx({958, 292}, {1168, 398}, 8.0f, BROWN);
    DrawLineEx({1168, 398}, {1370, 310}, 8.0f, BROWN);
    DrawLineEx({1180, 540}, {1370, 310}, 8.0f, BROWN);
    DrawPulley({940, 285}, 72, rotation, RAYWHITE);
    DrawPulley({1188, 408}, 58, rotation * -1.15f, RAYWHITE);
    DrawPulley({1370, 310}, 86, rotation * 0.72f, RAYWHITE);
    DrawPulley({1180, 540}, 48, rotation * 1.35f, RAYWHITE);

    DrawLineEx({1370, 396}, {1370, 612}, 6.0f, BROWN);
    DrawRectangle(1338, 612, 64, 78, GRAY);
    DrawRectangleLinesEx({1338, 612, 64, 78}, 2.0f, BLACK);

    if (bobTexture.id > 0) {
        DrawTexturePro(
            bobTexture,
            {0.0f, 0.0f, static_cast<float>(bobTexture.width) / 3.0f, static_cast<float>(bobTexture.height)},
            {1112, 386, 47, 56},
            {0, 0},
            0.0f,
            WHITE
        );
    }

    std::vector<MenuButton> buttons{
        {{1080, 510, 310, 46}, "New Game"},
        {{1080, 566, 310, 46}, "Continue"},
        {{1080, 622, 310, 46}, "Load Custom"},
        {{1080, 678, 310, 46}, "Settings"},
        {{1080, 734, 310, 46}, "Quit Game"}
    };

    DrawRectangle(1048, 486, 374, 324, Fade(BLACK, 0.26f));
    DrawRectangleLinesEx({1048, 486, 374, 324}, 2.0f, Fade(RAYWHITE, 0.15f));
    for (const MenuButton& button : buttons) {
        DrawMenuButton(button);
    }

    if (!menuMessage.empty()) {
        DrawText(menuMessage.c_str(), 1080, 825, 22, ORANGE);
    }
}

void Game::DrawOverworld() {
    DrawAtomicRealmBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, WHITE);
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
        {"Swim", "W / S / Space"},
        {"Climb", "W / S"},
        {"Grab Winch", "Hold E"},
        {"Turn Valve", "E near valve"},
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

    DrawAtomicRealmBackgroundFill(industrialBackground, {0, 0, static_cast<float>(Constants::ScreenWidth), static_cast<float>(Constants::ScreenHeight)}, Fade(WHITE, 0.82f));
    DrawRectangle(0, 0, Constants::ScreenWidth, Constants::ScreenHeight, Fade(Color{13, 20, 28, 255}, 0.16f));

    for (const Rectangle& solid : level.baseSolids) {
        if (HasSolidTouchingTop(solid, level.baseSolids)) {
            DrawAtomicRealmSolidFill(industrialTiles, solid, WHITE);
        }
        else {
            DrawAtomicRealmSolid(industrialTiles, solid, WHITE);
        }
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

    for (const Chain& chain : level.chains) {
        DrawChain(chain, chainLinksTexture);
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

    if (HasWaterPit(level)) {
        bool playerNearValve = CheckCollisionCircleRec(level.valve.center, level.valve.radius + 24.0f, player.rect);
        Color pipeColor = level.valve.opened ? BLUE : DARKGRAY;
        DrawLineEx({level.valve.center.x + level.valve.radius, level.valve.center.y}, {650.0f, level.valve.center.y}, 12.0f, pipeColor);
        DrawLineEx({650.0f, level.valve.center.y}, {650.0f, level.waterPit.bounds.y + 34.0f}, 12.0f, pipeColor);
        DrawRectangle(632, static_cast<int>(level.waterPit.bounds.y + 20.0f), 36, 26, DARKGRAY);
        DrawRectangleLines(632, static_cast<int>(level.waterPit.bounds.y + 20.0f), 36, 26, BLACK);
        if (level.valve.opened && level.waterPit.surfaceY > level.waterPit.targetSurfaceY) {
            DrawLineEx({650.0f, level.waterPit.bounds.y + 46.0f}, {650.0f, level.waterPit.surfaceY - 8.0f}, 7.0f, Fade(SKYBLUE, 0.72f));
        }
        DrawValve(level.valve, playerNearValve);
    }

    if (HasArea(level.spikeHazard)) {
        float pitTopY = 650.0f;
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

        DrawAtomicRealmPitWalls(industrialTiles, pitShaft, Fade(WHITE, 0.88f));
        DrawAtomicRealmPitFoundation(industrialTiles, pitFoundation, Fade(WHITE, 0.88f));
        DrawSpikes(level.spikeHazard.x + 2.0f, spikeBaseY, static_cast<int>(level.spikeHazard.width / 28.0f));
    }

    if (HasWaterPit(level)) {
        DrawWaterPit(level.waterPit);
    }

    for (Rectangle platform : level.pitPlatforms) {
        DrawAtomicRealmSolid(industrialTiles, platform, WHITE);
        DrawRectangleLinesEx(platform, 2, BLACK);
    }

    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawSeeSaw(seeSaw);
    }

    for (const StoneBlock& block : level.stoneBlocks) {
        DrawStoneBlock(block);
    }

    for (const Enemy& enemy : level.enemies) {
        DrawEnemy(enemy, enemyPlaceholderTexture);
    }

    int latchedCount = 0;
    for (const RotaryLatch& latch : level.rotaryLatches) {
        bool playerNear = CheckCollisionCircleRec(latch.center, latch.radius + 20.0f, player.rect);
        if (latch.latched) {
            latchedCount++;
        }

        DrawRotaryLatch(latch, playerNear);
    }

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
            bool gateRaising = !level.rotaryLatches.empty() &&
                latchedCount == static_cast<int>(level.rotaryLatches.size()) &&
                gateBottom > level.exitTrigger.y;
            if (gateRaising) {
                DrawLineEx({level.exitTrigger.x - 18.0f, level.exitTrigger.y + 8.0f}, {level.exitTrigger.x - 18.0f, gateBottom}, 4.0f, BROWN);
                DrawLineEx({level.exitTrigger.x + level.exitTrigger.width + 18.0f, level.exitTrigger.y + 8.0f}, {level.exitTrigger.x + level.exitTrigger.width + 18.0f, gateBottom}, 4.0f, BROWN);
            }
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

    if (lost) {
        if (skullTexture.id > 0) {
            DrawTexturePro(
                skullTexture,
                {0.0f, 0.0f, static_cast<float>(skullTexture.width), static_cast<float>(skullTexture.height)},
                deathRect,
                {0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }
        else {
            DrawRectangleRec(deathRect, BLACK);
        }
    }
    else {
        DrawPlayer(player, bobTexture);
    }

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

    if (HasWaterPit(level)) {
        float fillPercent = GetWaterFillProgress(level.waterPit) * 100.0f;
        DrawRectangle(20, 20, 260, 66, Fade(BLACK, 0.45f));
        DrawText(TextFormat("Water level: %.0f%%", fillPercent), 34, 32, 22, RAYWHITE);
        DrawText(level.valve.opened ? "Swim the flooded pit" : "Turn the valve", 34, 60, 18, level.valve.opened ? SKYBLUE : ORANGE);
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
    UnloadTexture(skullTexture);
    UnloadTexture(industrialTiles);
    UnloadTexture(industrialBackground);
    UnloadTexture(chainLinksTexture);
    UnloadTexture(enemyPlaceholderTexture);
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
        KillPlayer();
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
        deathRect = player.rect;
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

    if (HasWaterPit(level)) {
        DrawRectangleLinesEx(level.waterPit.bounds, 2, Fade(SKYBLUE, 0.95f));
        DrawRectangleLinesEx(GetFilledWaterRect(level.waterPit), 2, Fade(BLUE, 0.95f));
        DrawCircleLinesV(level.valve.center, level.valve.radius + 24.0f, Fade(BLUE, 0.9f));
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

    DrawRectangleLinesEx(player.rect, 2, Fade(ORANGE, 0.95f));
}
