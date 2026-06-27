#include "raylib.h"
#include <cmath>

struct Player {
    Rectangle rect;
    float speed;
};

struct Wheel {
    Vector2 center;
    float radius;
    float rotation;
    float spinSpeed;
    bool aligned;
};

float NormalizeAngle(float angle) {
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}

bool IsAligned(float angle, float targetAngle, float tolerance) {
    angle = NormalizeAngle(angle);
    targetAngle = NormalizeAngle(targetAngle);

    float difference = fabsf(angle - targetAngle);

    if (difference > 180.0f) {
        difference = 360.0f - difference;
    }

    return difference <= tolerance;
}

bool RectTouchesAnyWheel(Rectangle rect, Wheel wheels[], int wheelCount) {
    for (int i = 0; i < wheelCount; i++) {
        if (CheckCollisionCircleRec(wheels[i].center, wheels[i].radius, rect)) {
            return true;
        }
    }

    return false;
}

void RandomizeGame(Player& player, Wheel wheels[], int wheelCount, float targetAngle, float alignmentTolerance) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    const int margin = 90;

    for (int i = 0; i < wheelCount; i++) {
        bool validPosition = false;

        while (!validPosition) {
            wheels[i].center.x = static_cast<float>(GetRandomValue(margin, screenWidth - margin));
            wheels[i].center.y = static_cast<float>(GetRandomValue(margin + 80, screenHeight - margin));

            validPosition = true;

            for (int j = 0; j < i; j++) {
                float dx = wheels[i].center.x - wheels[j].center.x;
                float dy = wheels[i].center.y - wheels[j].center.y;
                float distance = sqrtf(dx * dx + dy * dy);

                if (distance < wheels[i].radius + wheels[j].radius + 80.0f) {
                    validPosition = false;
                    break;
                }
            }
        }

        do {
            wheels[i].rotation = static_cast<float>(GetRandomValue(0, 359));
        } while (IsAligned(wheels[i].rotation, targetAngle, alignmentTolerance));

        wheels[i].aligned = false;
    }

    bool validPlayerPosition = false;

    while (!validPlayerPosition) {
        player.rect.x = static_cast<float>(GetRandomValue(0, screenWidth - static_cast<int>(player.rect.width)));
        player.rect.y = static_cast<float>(GetRandomValue(100, screenHeight - static_cast<int>(player.rect.height)));

        validPlayerPosition = !RectTouchesAnyWheel(player.rect, wheels, wheelCount);
    }
}

void ResetGame(
    Player& player,
    Wheel wheels[],
    int wheelCount,
    float& timer,
    bool& gameWon,
    bool& gameLost,
    float targetAngle,
    float alignmentTolerance
) {
    RandomizeGame(player, wheels, wheelCount, targetAngle, alignmentTolerance);

    timer = 30.0f;
    gameWon = false;
    gameLost = false;
    
}

int main_0(void) {
    const int windowWidth{ 1200 };
    const int windowHeight{ 700 };

    const float targetAngle{ 270.0f };
    const float alignmentTolerance{ 1.0f };

    Player player{
        { 0.0f, 0.0f, 30.0f, 30.0f },
        300.0f
    };

    const int wheelCount{ 5 };

    Wheel wheels[wheelCount]{
        { {0.0f, 0.0f}, 60.0f, 0.0f, 160.0f, false },
        { {0.0f, 0.0f}, 60.0f, 0.0f, 160.0f, false },
        { {0.0f, 0.0f}, 60.0f, 0.0f, 160.0f, false },
        { {0.0f, 0.0f}, 60.0f, 0.0f, 160.0f, false },
        { {0.0f, 0.0f}, 60.0f, 0.0f, 160.0f, false }
    };

    float timer{ 30.0f };
    bool gameWon{ false };
    bool gameLost{ false };
    bool showFPS{ false };

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(windowWidth, windowHeight, "Raylib Alignment Puzzle");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    ResetGame(player, wheels, wheelCount, timer, gameWon, gameLost, targetAngle, alignmentTolerance);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        if (IsKeyPressed(KEY_F)) {
            showFPS = !showFPS;
        }

        if (IsKeyPressed(KEY_R)) {
            ResetGame(player, wheels, wheelCount, timer, gameWon, gameLost, targetAngle, alignmentTolerance);
        }

        if (!gameWon && !gameLost) {
            timer -= deltaTime;

            if (timer <= 0.0f) {
                timer = 0.0f;
                gameLost = true;
            }

            if (IsKeyDown(KEY_W)) player.rect.y -= player.speed * deltaTime;
            if (IsKeyDown(KEY_A)) player.rect.x -= player.speed * deltaTime;
            if (IsKeyDown(KEY_S)) player.rect.y += player.speed * deltaTime;
            if (IsKeyDown(KEY_D)) player.rect.x += player.speed * deltaTime;

            if (player.rect.x < 0) player.rect.x = 0;
            if (player.rect.y < 0) player.rect.y = 0;

            if (player.rect.x > GetScreenWidth() - player.rect.width)
                player.rect.x = GetScreenWidth() - player.rect.width;

            if (player.rect.y > GetScreenHeight() - player.rect.height)
                player.rect.y = GetScreenHeight() - player.rect.height;

            int alignedCount = 0;

            for (int i = 0; i < wheelCount; i++) {
                bool playerTouchingWheel = CheckCollisionCircleRec(
                    wheels[i].center,
                    wheels[i].radius,
                    player.rect
                );

                if (playerTouchingWheel && !wheels[i].aligned) {
                    wheels[i].rotation += wheels[i].spinSpeed * deltaTime;
                    wheels[i].rotation = NormalizeAngle(wheels[i].rotation);
                }

                wheels[i].aligned = IsAligned(
                    wheels[i].rotation,
                    targetAngle,
                    alignmentTolerance
                );

                if (wheels[i].aligned) {
                    alignedCount++;
                }
            }

            if (alignedCount == wheelCount) {
                gameWon = true;
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        if (showFPS) {
            DrawFPS(10, 10);
        }

        DrawText(TextFormat("Time: %.1f", timer), 10, 40, 30, RED);

        int alignedCount = 0;

        for (int i = 0; i < wheelCount; i++) {
            if (wheels[i].aligned) alignedCount++;
        }

        DrawText(TextFormat("Aligned: %d / %d", alignedCount, wheelCount), 10, 80, 24, BLACK);

        for (int i = 0; i < wheelCount; i++) {
            Color wheelColor = wheels[i].aligned ? GREEN : BLACK;

            DrawCircleLines(
                static_cast<int>(wheels[i].center.x),
                static_cast<int>(wheels[i].center.y),
                wheels[i].radius,
                wheelColor
            );

            Vector2 targetStart{
                wheels[i].center.x,
                wheels[i].center.y - wheels[i].radius
            };

            Vector2 targetEnd{
                wheels[i].center.x,
                wheels[i].center.y - wheels[i].radius + 18.0f
            };

            DrawLineEx(targetStart, targetEnd, 4.0f, GREEN);

            Vector2 spokeEnd{
                wheels[i].center.x + cosf(wheels[i].rotation * DEG2RAD) * wheels[i].radius,
                wheels[i].center.y + sinf(wheels[i].rotation * DEG2RAD) * wheels[i].radius
            };

			// The rotating line turns green when aligned.
			Color spokeColor = wheels[i].aligned ? GREEN : BLACK;

			DrawLineEx(wheels[i].center, spokeEnd, 5.0f, spokeColor);
        }

        DrawRectangleRec(player.rect, BLACK);

        DrawText(
            "WASD: Move   F: Toggle FPS   R: Restart",
            10,
            GetScreenHeight() - 25,
            20,
            DARKGRAY
        );

        if (gameWon) {
            DrawText("YOU WIN!", 470, 300, 50, GREEN);
            DrawText("Press R to restart", 485, 360, 24, BLACK);
        }

        if (gameLost) {
            DrawText("TIME'S UP!", 440, 300, 50, RED);
            DrawText("Press R to restart", 485, 360, 24, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}