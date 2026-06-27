#include "raylib.h"
#include <cmath>
#include <vector>

struct Player {
    Rectangle rect{80, 600, 28, 40};
    Vector2 velocity{0, 0};
    bool onGround{false};
};

struct Winch {
    Rectangle rect;
    float startX;
    float maxX;
    float returnSpeed;
    bool grabbed{false};
};

struct HangingWeight {
    Vector2 pulley;
    float pulleyRadius;
    float phase;
    float speed;
    Rectangle rect;
};

float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float LerpFloat(float start, float end, float amount) {
    return start + (end - start) * amount;
}

float GetMachinePower(const Winch& winch) {
    return Clamp01((winch.rect.x - winch.startX) / (winch.maxX - winch.startX));
}

bool NearRect(Rectangle a, Rectangle b, float padding) {
    Rectangle expanded{
        b.x - padding,
        b.y - padding,
        b.width + padding * 2.0f,
        b.height + padding * 2.0f
    };

    return CheckCollisionRecs(a, expanded);
}

void ResetPlayer(Player& player) {
    player.rect = {80, 600, 28, 40};
    player.velocity = {0, 0};
    player.onGround = false;
}

void DrawPulley(Vector2 center, float radius, float rotation, Color color) {
    DrawCircleLinesV(center, radius, color);
    DrawCircleLinesV(center, radius - 8, color);

    for (int i = 0; i < 6; i++) {
        float angle = rotation + i * 60.0f;

        Vector2 end{
            center.x + cosf(angle * DEG2RAD) * radius,
            center.y + sinf(angle * DEG2RAD) * radius
        };

        DrawLineEx(center, end, 3.0f, color);
    }

    DrawCircleV(center, 5.0f, color);
}

void DrawWinch(const Winch& winch) {
    Color color = winch.grabbed ? ORANGE : GRAY;

    DrawRectangle(winch.startX, winch.rect.y + winch.rect.height + 8, winch.maxX - winch.startX + winch.rect.width, 8, DARKGRAY);
    DrawRectangleLines(winch.startX, winch.rect.y + winch.rect.height + 8, winch.maxX - winch.startX + winch.rect.width, 8, BLACK);

    DrawRectangleRec(winch.rect, color);
    DrawRectangleLinesEx(winch.rect, 3, BLACK);

    DrawLineEx({winch.rect.x + 10, winch.rect.y + 10}, {winch.rect.x + winch.rect.width - 10, winch.rect.y + winch.rect.height - 10}, 3, DARKGRAY);
    DrawLineEx({winch.rect.x + winch.rect.width - 10, winch.rect.y + 10}, {winch.rect.x + 10, winch.rect.y + winch.rect.height - 10}, 3, DARKGRAY);
}

void DrawSolidRedTriangle(Vector2 a, Vector2 b, Vector2 c) {
    DrawTriangle(a, b, c, RED);
    DrawTriangle(a, c, b, RED);
}

void DrawSpikes(float startX, float baseY, int count) {
    for (int i = 0; i < count; i++) {
        float x = startX + i * 28.0f;

        DrawSolidRedTriangle(
            {x, baseY},
            {x + 14.0f, baseY - 32.0f},
            {x + 28.0f, baseY}
        );
    }
}

void DrawHazardSpikesOnBlock(Rectangle rect) {
    const float spikeWidth = 11.0f;
    const float spikeHeight = 9.0f;
    int horizontalCount = static_cast<int>(rect.width / spikeWidth);

    for (int i = 0; i < horizontalCount; i++) {
        float x = rect.x + i * spikeWidth + 2.0f;

        DrawSolidRedTriangle(
            {x, rect.y},
            {x + spikeWidth * 0.5f, rect.y - spikeHeight},
            {x + spikeWidth, rect.y}
        );

        DrawSolidRedTriangle(
            {x, rect.y + rect.height},
            {x + spikeWidth * 0.5f, rect.y + rect.height + spikeHeight},
            {x + spikeWidth, rect.y + rect.height}
        );
    }

    const float sideSpikeHeight = 11.0f;
    const float sideSpikeWidth = 9.0f;
    int verticalCount = static_cast<int>(rect.height / sideSpikeHeight);

    for (int i = 0; i < verticalCount; i++) {
        float y = rect.y + i * sideSpikeHeight + 2.0f;

        DrawSolidRedTriangle(
            {rect.x, y},
            {rect.x - sideSpikeWidth, y + sideSpikeHeight * 0.5f},
            {rect.x, y + sideSpikeHeight}
        );

        DrawSolidRedTriangle(
            {rect.x + rect.width, y},
            {rect.x + rect.width + sideSpikeWidth, y + sideSpikeHeight * 0.5f},
            {rect.x + rect.width, y + sideSpikeHeight}
        );
    }
}

void DrawHazardWeight(const HangingWeight& weight) {
    float ropeX = weight.pulley.x + weight.pulleyRadius;

    DrawLineEx(
        {ropeX, weight.pulley.y},
        {weight.rect.x + weight.rect.width / 2.0f, weight.rect.y},
        4,
        BROWN
    );

    DrawRectangleRec(weight.rect, GRAY);
    DrawRectangleLinesEx(weight.rect, 2, BLACK);
    DrawHazardSpikesOnBlock(weight.rect);
}

void ResolveHorizontal(Player& player, const std::vector<Rectangle>& solids) {
    for (const Rectangle& solid : solids) {
        if (CheckCollisionRecs(player.rect, solid)) {
            if (player.velocity.x > 0) {
                player.rect.x = solid.x - player.rect.width;
            }
            else if (player.velocity.x < 0) {
                player.rect.x = solid.x + solid.width;
            }

            player.velocity.x = 0;
        }
    }
}

void ResolveVertical(Player& player, const std::vector<Rectangle>& solids) {
    player.onGround = false;

    for (const Rectangle& solid : solids) {
        if (CheckCollisionRecs(player.rect, solid)) {
            if (player.velocity.y > 0) {
                player.rect.y = solid.y - player.rect.height;
                player.velocity.y = 0;
                player.onGround = true;
            }
            else if (player.velocity.y < 0) {
                player.rect.y = solid.y + solid.height;
                player.velocity.y = 0;
            }
        }
    }
}

float UpdateWinch(Winch& winch, Player& player, float moveInput, float dt) {
    float oldX = winch.rect.x;
    winch.grabbed = NearRect(player.rect, winch.rect, 18.0f) && IsKeyDown(KEY_E);

    if (winch.grabbed) {
        winch.rect.x += moveInput * 120.0f * dt;

        if (winch.rect.x < winch.startX) winch.rect.x = winch.startX;
        if (winch.rect.x > winch.maxX) winch.rect.x = winch.maxX;

        if (moveInput != 0.0f) {
            player.rect.x = winch.rect.x - player.rect.width - 4.0f;
        }
    }
    else {
        winch.rect.x -= winch.returnSpeed * dt;

        if (winch.rect.x < winch.startX) {
            winch.rect.x = winch.startX;
        }
    }

    return winch.rect.x - oldX;
}

int main() {
    const int screenWidth = 1600;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Spin to Win - Power Pulley Panic");
    SetTargetFPS(60);

    Player player;

    Winch machineWinch{
        {350, 205, 70, 45},
        350.0f,
        590.0f,
        42.0f
    };

    Rectangle ladder{235, 250, 45, 400};
    Rectangle spikeHazard{330, 773, 895, 32};
    Rectangle darknessArea{300, 300, 960, 510};

    std::vector<Rectangle> baseSolids{
        {0, 650, 300, 40},
        {300, 250, 420, 25},
        {1260, 650, 340, 40},
        {0, 690, 300, 210},
        {1260, 690, 340, 210}
    };

    Rectangle pitPlatforms[] = {
        {420, 570, 70, 18},
        {590, 520, 70, 18},
        {760, 570, 70, 18},
        {930, 515, 70, 18},
        {1100, 570, 70, 18}
    };

    Vector2 pulleys[] = {
        {650, 210},
        {820, 210},
        {990, 210},
        {1160, 210},
        {1430, 515}
    };

    HangingWeight weights[] = {
        {pulleys[1], 45.0f, 0.0f, 1.00f, {0, 0, 44, 55}},
        {pulleys[2], 45.0f, 1.6f, 1.25f, {0, 0, 44, 55}},
        {pulleys[3], 45.0f, 3.2f, 1.10f, {0, 0, 44, 55}}
    };

    bool showFPS = false;
    bool won = false;
    bool lost = false;

    float pulleyRotation = 0.0f;
    float machinePhase = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_F)) showFPS = !showFPS;

        if (IsKeyPressed(KEY_R)) {
            ResetPlayer(player);
            machineWinch.rect.x = machineWinch.startX;
            machineWinch.grabbed = false;
            pulleyRotation = 0.0f;
            machinePhase = 0.0f;
            won = false;
            lost = false;
        }

        float moveInput = 0.0f;
        if (!won && !lost) {
            if (IsKeyDown(KEY_A)) moveInput -= 1.0f;
            if (IsKeyDown(KEY_D)) moveInput += 1.0f;
        }

        bool onLadder = CheckCollisionRecs(player.rect, ladder);

        player.velocity.x = moveInput * 260.0f;

        if (onLadder && IsKeyDown(KEY_W) && !won && !lost) {
            player.velocity.y = -190.0f;
        }
        else if (onLadder && IsKeyDown(KEY_S) && !won && !lost) {
            player.velocity.y = 190.0f;
        }
        else {
            player.velocity.y += 900.0f * dt;
        }

        if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && player.onGround && !onLadder && !won && !lost) {
            player.velocity.y = -430.0f;
        }

        float winchDelta = 0.0f;
        if (!won && !lost) {
            winchDelta = UpdateWinch(machineWinch, player, moveInput, dt);
        }

        float machinePower = GetMachinePower(machineWinch);
        float spinAmount = fabsf(winchDelta) * 3.0f + machinePower * 140.0f * dt;
        pulleyRotation += spinAmount;
        machinePhase += (0.35f + machinePower * 3.3f) * dt;

        bool lightsOn = machinePower > 0.12f;
        bool gateOpen = machinePower > 0.72f;

        for (HangingWeight& weight : weights) {
            float travel = LerpFloat(28.0f, 88.0f, machinePower);
            float y = 505.0f + sinf(machinePhase * weight.speed + weight.phase) * travel;
            float ropeX = weight.pulley.x + weight.pulleyRadius;
            weight.rect.x = ropeX - weight.rect.width / 2.0f;
            weight.rect.y = y;
        }

        std::vector<Rectangle> solids = baseSolids;

        for (Rectangle platform : pitPlatforms) {
            solids.push_back(platform);
        }

        if (!won && !lost) {
            player.rect.x += player.velocity.x * dt;
            ResolveHorizontal(player, solids);

            player.rect.y += player.velocity.y * dt;
            ResolveVertical(player, solids);
        }

        if (player.rect.y > screenHeight || CheckCollisionRecs(player.rect, spikeHazard)) {
            lost = true;
        }

        for (const HangingWeight& weight : weights) {
            if (CheckCollisionRecs(player.rect, weight.rect)) {
                lost = true;
            }
        }

        if (gateOpen && player.rect.x > 1500) {
            won = true;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (showFPS) DrawFPS(10, 10);

        DrawText("SPIN TO WIN: POWER PULLEY PANIC", 20, 20, 28, BLACK);
        DrawText("A/D: Move   W/Space: Jump/Climb   Hold E near block: Push/Pull   F: FPS   R: Restart", 20, 55, 20, DARKGRAY);

        for (const Rectangle& solid : baseSolids) {
            DrawRectangleRec(solid, LIGHTGRAY);
            DrawRectangleLinesEx(solid, 2, BLACK);
        }

        DrawLineEx({ladder.x + 8, ladder.y}, {ladder.x + 8, ladder.y + ladder.height}, 4, BLACK);
        DrawLineEx({ladder.x + ladder.width - 8, ladder.y}, {ladder.x + ladder.width - 8, ladder.y + ladder.height}, 4, BLACK);

        for (int i = 0; i < 13; i++) {
            float y = ladder.y + i * 30;
            DrawLineEx({ladder.x + 8, y}, {ladder.x + ladder.width - 8, y}, 3, BLACK);
        }

        DrawWinch(machineWinch);
        DrawText("PUSH BLOCK", 338, 175, 18, BLACK);
        DrawText("Hold E + A/D", 338, 150, 16, DARKGRAY);

        DrawLineEx({machineWinch.rect.x + machineWinch.rect.width, machineWinch.rect.y + 20}, {pulleys[0].x - 55, pulleys[0].y}, 5, BROWN);
        DrawLineEx({pulleys[0].x, pulleys[0].y + 55}, {pulleys[1].x, pulleys[1].y - 45}, 5, BROWN);
        DrawLineEx({pulleys[1].x, pulleys[1].y + 45}, {pulleys[2].x, pulleys[2].y - 45}, 5, BROWN);
        DrawLineEx({pulleys[2].x, pulleys[2].y + 45}, {pulleys[3].x, pulleys[3].y - 45}, 5, BROWN);
        DrawLineEx({pulleys[3].x, pulleys[3].y + 45}, {pulleys[4].x, pulleys[4].y - 55}, 5, BROWN);

        DrawPulley(pulleys[0], 55, pulleyRotation, BLACK);
        DrawPulley(pulleys[1], 45, pulleyRotation * 1.2f, BLACK);
        DrawPulley(pulleys[2], 45, pulleyRotation * 1.4f, BLACK);
        DrawPulley(pulleys[3], 45, pulleyRotation * 1.6f, BLACK);
        DrawPulley(pulleys[4], 55, pulleyRotation * 1.1f, BLACK);

        DrawRectangle(565, 365, 90, 70, LIGHTGRAY);
        DrawRectangleLines(565, 365, 90, 70, BLACK);
        DrawCircleLines(610, 400, 24, BLACK);
        DrawText("GENERATOR", 545, 445, 18, BLACK);

        float mainWeightY = 390.0f + machinePower * 92.0f;
        float mainRopeX = pulleys[0].x + 55.0f;
        DrawLineEx({mainRopeX, pulleys[0].y}, {mainRopeX, mainWeightY}, 5, BROWN);
        DrawRectangle(mainRopeX - 25, mainWeightY, 50, 60, GRAY);
        DrawRectangleLines(mainRopeX - 25, mainWeightY, 50, 60, BLACK);
        DrawLineEx({mainRopeX, mainWeightY + 60}, {610, 365}, 4, BROWN);

        for (const HangingWeight& weight : weights) {
            DrawHazardWeight(weight);
        }

        Color wireColor = lightsOn ? BLUE : DARKBLUE;

        DrawLineEx({655, 400}, {760, 400}, 4, wireColor);
        DrawLineEx({760, 400}, {760, 320}, 4, wireColor);
        DrawLineEx({760, 320}, {1220, 320}, 4, wireColor);
        DrawLineEx({1220, 320}, {1220, 598}, 4, wireColor);
        DrawLineEx({1220, 598}, {1370, 598}, 4, wireColor);

        float flicker = 0.0f;
        if (machinePower < 0.42f && machinePower > 0.02f) {
            flicker = sinf(static_cast<float>(GetTime()) * 20.0f) * 0.12f;
        }

        for (int i = 0; i < 5; i++) {
            float x = 780 + i * 95;
            float lampPower = Clamp01(machinePower + flicker);

            DrawLineEx({x, 320}, {x, 360}, 3, BLACK);
            DrawCircleV({x, 375}, 13, lampPower > 0.08f ? YELLOW : DARKGRAY);

            if (lampPower > 0.08f) {
                DrawTriangle(
                    {x - 38, 445},
                    {x + 38, 445},
                    {x, 385},
                    Fade(YELLOW, 0.08f + lampPower * 0.45f)
                );
            }
        }

        const char* powerLabel = "LIGHTS OUT";
        Color powerLabelColor = DARKGRAY;
        if (machinePower >= 0.70f) {
            powerLabel = "BRIGHT - RUN";
            powerLabelColor = BLUE;
        }
        else if (machinePower >= 0.40f) {
            powerLabel = "DIMMING";
            powerLabelColor = DARKBLUE;
        }
        else if (machinePower >= 0.15f) {
            powerLabel = "FLICKERING";
            powerLabelColor = ORANGE;
        }

        DrawText(powerLabel, 830, 285, 22, powerLabelColor);

        DrawRectangle(300, 650, 960, 160, DARKGRAY);
        DrawSpikes(330, 805, 32);

        for (Rectangle platform : pitPlatforms) {
            DrawRectangleRec(platform, LIGHTGRAY);
            DrawRectangleLinesEx(platform, 2, BLACK);
        }

        float darknessAlpha = Clamp01(0.10f + (1.0f - machinePower) * 0.78f - flicker);
        DrawRectangleRec(darknessArea, Fade(BLACK, darknessAlpha));

        if (machinePower < 0.15f) {
            DrawText("NEAR DARKNESS", 535, 500, 28, Fade(LIGHTGRAY, 0.7f));
        }

        DrawRectangle(1370, 565, 85, 65, LIGHTGRAY);
        DrawRectangleLines(1370, 565, 85, 65, BLACK);
        DrawCircleLines(1412, 598, 22, BLACK);
        DrawText("GATE MOTOR", 1350, 635, 18, BLACK);

        DrawRectangle(1485, 430, 85, 220, LIGHTGRAY);
        DrawRectangleLines(1485, 430, 85, 220, BLACK);

        float gateBottom = 650.0f - machinePower * 170.0f;

        for (int i = 0; i < 6; i++) {
            float x = 1495 + i * 13;
            DrawLineEx({x, 430}, {x, gateBottom}, 5, BLACK);
        }

        DrawRectangle(1490, gateBottom - 15, 75, 15, BLACK);
        DrawText(gateOpen ? "GATE OPEN" : "GATE NEEDS POWER", 1410, 395, 20, gateOpen ? GREEN : RED);

        DrawRectangleRec(player.rect, BLACK);

        DrawRectangle(20, 90, 280, 170, Fade(WHITE, 0.9f));
        DrawRectangleLines(20, 90, 280, 170, BLACK);
        DrawText("HOW IT WORKS", 35, 105, 20, BLACK);
        DrawText("1. Climb ladder.", 35, 135, 16, DARKGRAY);
        DrawText("2. Hold E near block.", 35, 158, 16, DARKGRAY);
        DrawText("3. Push it right to wind up.", 35, 181, 16, DARKGRAY);
        DrawText("4. Pulleys power everything.", 35, 204, 16, DARKGRAY);
        DrawText("5. Cross before it winds down.", 35, 227, 16, DARKGRAY);

        DrawText(TextFormat("Machine Power: %d%%", static_cast<int>(machinePower * 100)), 20, 850, 22, BLUE);

        if (machineWinch.grabbed) {
            DrawText("PUSHING BLOCK", 670, 80, 28, ORANGE);
        }

        if (won) {
            DrawText("YOU WIN!", 690, 420, 60, GREEN);
            DrawText("Press R to restart", 690, 485, 26, BLACK);
        }

        if (lost) {
            DrawText("MACHINE PANIC!", 590, 420, 54, RED);
            DrawText("Press R to restart", 660, 485, 26, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
