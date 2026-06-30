#include "Game.h"

#include "Collision.h"
#include "Constants.h"
#include "Machine.h"
#include "MathUtils.h"
#include "Render.h"

#include <cmath>
#include <vector>

void Game::Run() {
    Load();

    while (!WindowShouldClose()) {
        Update(GetFrameTime());
        Draw();
    }

    Unload();
}

void Game::Load() {
    InitWindow(Constants::ScreenWidth, Constants::ScreenHeight, "Spin to Win - Power Pulley Panic");
    SetTargetFPS(60);

    bobTexture = LoadTexture("assets/first_party/characters/Bob.png");
    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");

    if (bobTexture.id > 0) SetTextureFilter(bobTexture, TEXTURE_FILTER_POINT);
    if (industrialTiles.id > 0) SetTextureFilter(industrialTiles, TEXTURE_FILTER_POINT);
    if (industrialBackground.id > 0) SetTextureFilter(industrialBackground, TEXTURE_FILTER_POINT);

    Reset();
}

void Game::Reset() {
    level = CreatePowerPulleyPanicLevel();
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
}

void Game::Update(float dt) {
    if (IsKeyPressed(KEY_F)) showFPS = !showFPS;
    if (IsKeyPressed(KEY_R)) Reset();

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

    if (player.rect.y > Constants::ScreenHeight || CheckCollisionRecs(player.rect, level.spikeHazard)) {
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

    Vector2 playerCenter{
        player.rect.x + player.rect.width * 0.5f,
        player.rect.y + player.rect.height * 0.5f
    };
    float doorwayCenterX = level.exitTrigger.x + level.exitTrigger.width * 0.5f;
    bool playerCenteredInDoorway = fabsf(playerCenter.x - doorwayCenterX) <= 2.0f;

    if (playerCenteredInDoorway && CheckCollisionPointRec(playerCenter, level.exitTrigger) && gateBottom <= player.rect.y + 4.0f) {
        won = true;
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

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

    DrawLineEx({level.ladder.x + 8, level.ladder.y}, {level.ladder.x + 8, level.ladder.y + level.ladder.height}, 4, BLACK);
    DrawLineEx({level.ladder.x + level.ladder.width - 8, level.ladder.y}, {level.ladder.x + level.ladder.width - 8, level.ladder.y + level.ladder.height}, 4, BLACK);
    for (int i = 0; i < 13; i++) {
        float y = level.ladder.y + i * 30;
        DrawLineEx({level.ladder.x + 8, y}, {level.ladder.x + level.ladder.width - 8, y}, 3, BLACK);
    }

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

    for (const HangingWeight& weight : level.weights) {
        DrawHazardWeight(weight);
    }

    bool lightsOn = machinePower > 0.12f;
    Color wireColor = lightsOn ? BLUE : DARKBLUE;

    DrawLineEx({655, 400}, {760, 400}, 4, wireColor);
    DrawLineEx({760, 400}, {760, 320}, 4, wireColor);
    DrawLineEx({760, 320}, {1220, 320}, 4, wireColor);
    DrawLineEx({1220, 320}, {1220, 598}, 4, wireColor);
    DrawLineEx({1220, 598}, {1370, 598}, 4, wireColor);

    float flicker = 0.0f;
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

    DrawTiledTextureRect(industrialTiles, darkPitTile, {300, 650, 960, 160}, Fade(WHITE, 0.85f));
    DrawSpikes(302, 805, 34);

    for (Rectangle platform : level.pitPlatforms) {
        DrawTiledTextureRect(industrialTiles, platformTile, platform, WHITE);
        DrawRectangleLinesEx(platform, 2, BLACK);
    }

    Rectangle gateMotorBox{1370, 565, 85, 65};
    Vector2 gateMotorGear{
        gateMotorBox.x + gateMotorBox.width * 0.42f,
        gateMotorBox.y + gateMotorBox.height * 0.48f
    };
    DrawMachineBox(gateMotorBox, pulleyRotation * 1.4f, machinePower > 0.04f);
    DrawLineEx({level.pulleys[4].x - 48.0f, level.pulleys[4].y + 27.0f}, {gateMotorGear.x - 25.0f, gateMotorGear.y - 7.0f}, 5, BROWN);
    DrawLineEx({level.pulleys[4].x + 24.0f, level.pulleys[4].y + 50.0f}, {gateMotorGear.x + 23.0f, gateMotorGear.y + 10.0f}, 5, BROWN);
    DrawRing(gateMotorGear, 22.0f, 27.0f, 0.0f, 360.0f, 32, BROWN);

    Rectangle doorway{1490, 430, 75, 220};
    DrawRectangleRec(level.exitTrigger, LIGHTGRAY);
    DrawOutdoorDoorway(doorway);
    DrawRectangleLinesEx(level.exitTrigger, 1, BLACK);

    float gateBottom = 650.0f - machinePower * 170.0f;
    DrawRectangle(1490, 430, 75, static_cast<int>(gateBottom - 430.0f), BROWN);
    for (int i = 0; i < 6; i++) {
        float x = 1495 + i * 13;
        DrawLineEx({x, 430}, {x, gateBottom}, 5, BLACK);
    }
    DrawRectangle(1490, gateBottom - 15, 75, 15, BLACK);

    DrawPlayer(player, bobTexture);

    float blackoutFlicker = machinePower > 0.02f ? flicker : 0.0f;
    float safeAreaDimAlpha = Clamp01(0.20f + (1.0f - machinePower) * 0.18f - flicker * 0.45f);
    float blackoutAlpha = Clamp01(1.0f - machinePower - blackoutFlicker);
    DrawRectangle(0, 0, 300, Constants::ScreenHeight, Fade(BLACK, safeAreaDimAlpha));
    DrawRectangle(300, 0, 960, 275, Fade(BLACK, safeAreaDimAlpha));
    DrawRectangleRec(level.darknessArea, Fade(BLACK, blackoutAlpha));
    DrawRectangleRec(level.rightDarknessArea, Fade(BLACK, blackoutAlpha));

    if (won) {
        DrawText("YOU WIN!", 690, 420, 60, GREEN);
        DrawText("Press R to restart", 690, 485, 26, machinePower < 0.45f ? WHITE : BLACK);
    }

    if (lost) {
        DrawText("YOU DIED!", 660, 420, 54, RED);
        DrawText("Press R to restart", 660, 485, 26, machinePower < 0.45f ? WHITE : BLACK);
    }

    EndDrawing();
}

void Game::Unload() {
    UnloadTexture(bobTexture);
    UnloadTexture(industrialTiles);
    UnloadTexture(industrialBackground);
    CloseWindow();
}
