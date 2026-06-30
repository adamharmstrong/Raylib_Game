#include "Render.h"

#include "Machine.h"

#include <cmath>

namespace {
    void DrawSolidRedTriangle(Vector2 a, Vector2 b, Vector2 c) {
        DrawTriangle(a, b, c, RED);
        DrawTriangle(a, c, b, RED);
    }

    void DrawHazardSpikesOnBlock(Rectangle rect) {
        const float spikeWidth = 11.0f;
        const float spikeHeight = 9.0f;
        int horizontalCount = static_cast<int>(rect.width / spikeWidth);

        for (int i = 0; i < horizontalCount; i++) {
            float x = rect.x + i * spikeWidth + 2.0f;

            DrawSolidRedTriangle({x, rect.y}, {x + spikeWidth * 0.5f, rect.y - spikeHeight}, {x + spikeWidth, rect.y});
            DrawSolidRedTriangle({x, rect.y + rect.height}, {x + spikeWidth * 0.5f, rect.y + rect.height + spikeHeight}, {x + spikeWidth, rect.y + rect.height});
        }

        const float sideSpikeHeight = 11.0f;
        const float sideSpikeWidth = 9.0f;
        int verticalCount = static_cast<int>(rect.height / sideSpikeHeight);

        for (int i = 0; i < verticalCount; i++) {
            float y = rect.y + i * sideSpikeHeight + 2.0f;

            DrawSolidRedTriangle({rect.x, y}, {rect.x - sideSpikeWidth, y + sideSpikeHeight * 0.5f}, {rect.x, y + sideSpikeHeight});
            DrawSolidRedTriangle({rect.x + rect.width, y}, {rect.x + rect.width + sideSpikeWidth, y + sideSpikeHeight * 0.5f}, {rect.x + rect.width, y + sideSpikeHeight});
        }
    }

    void DrawGear(Vector2 center, float radius, float rotation) {
        DrawCircleLinesV(center, radius, BLACK);
        DrawCircleLinesV(center, radius * 0.42f, BLACK);

        for (int i = 0; i < 8; i++) {
            float angle = rotation + i * 45.0f;
            Vector2 inner{
                center.x + cosf(angle * DEG2RAD) * radius * 0.55f,
                center.y + sinf(angle * DEG2RAD) * radius * 0.55f
            };
            Vector2 outer{
                center.x + cosf(angle * DEG2RAD) * (radius + 5.0f),
                center.y + sinf(angle * DEG2RAD) * (radius + 5.0f)
            };

            DrawLineEx(inner, outer, 3.0f, BLACK);
        }
    }
}

void DrawPulley(Vector2 center, float radius, float rotation, Color color) {
    DrawRing(center, radius - 9.0f, radius, 0.0f, 360.0f, 48, BROWN);
    DrawCircleLinesV(center, radius, color);
    DrawCircleLinesV(center, radius - 9.0f, color);

    for (int i = 0; i < 6; i++) {
        float angle = rotation + i * 60.0f;
        Vector2 end{
            center.x + cosf(angle * DEG2RAD) * (radius - 9.0f),
            center.y + sinf(angle * DEG2RAD) * (radius - 9.0f)
        };

        DrawLineEx(center, end, 3.0f, color);
    }

    DrawCircleV(center, 5.0f, color);
}

void DrawTiledTextureRect(Texture2D texture, Rectangle sourceTile, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    for (float y = dest.y; y < dest.y + dest.height; y += sourceTile.height) {
        for (float x = dest.x; x < dest.x + dest.width; x += sourceTile.width) {
            float drawWidth = fminf(sourceTile.width, dest.x + dest.width - x);
            float drawHeight = fminf(sourceTile.height, dest.y + dest.height - y);
            Rectangle source{
                sourceTile.x,
                sourceTile.y,
                drawWidth,
                drawHeight
            };
            Rectangle target{x, y, drawWidth, drawHeight};

            DrawTexturePro(texture, source, target, {0, 0}, 0.0f, tint);
        }
    }
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

void DrawSpikes(float startX, float baseY, int count) {
    for (int i = 0; i < count; i++) {
        float x = startX + i * 28.0f;
        DrawSolidRedTriangle({x, baseY}, {x + 14.0f, baseY - 32.0f}, {x + 28.0f, baseY});
    }
}

void DrawHazardWeight(const HangingWeight& weight) {
    float ropeX = weight.pulley.x + weight.pulleyRadius;

    DrawLineEx({ropeX, weight.pulley.y}, {weight.rect.x + weight.rect.width / 2.0f, weight.rect.y}, 4, BROWN);
    DrawRectangleRec(weight.rect, GRAY);
    DrawRectangleLinesEx(weight.rect, 2, BLACK);
    DrawHazardSpikesOnBlock(weight.rect);
}

void DrawOutdoorDoorway(Rectangle doorway) {
    BeginScissorMode(static_cast<int>(doorway.x), static_cast<int>(doorway.y), static_cast<int>(doorway.width), static_cast<int>(doorway.height));

    DrawRectangleRec(doorway, SKYBLUE);
    DrawCircle(static_cast<int>(doorway.x + doorway.width - 18), static_cast<int>(doorway.y + 36), 28, YELLOW);

    for (int i = 0; i < 10; i++) {
        float angle = i * 22.0f;
        Vector2 start{
            doorway.x + doorway.width - 18 + cosf(angle * DEG2RAD) * 34.0f,
            doorway.y + 36 + sinf(angle * DEG2RAD) * 34.0f
        };
        Vector2 end{
            doorway.x + doorway.width - 18 + cosf(angle * DEG2RAD) * 45.0f,
            doorway.y + 36 + sinf(angle * DEG2RAD) * 45.0f
        };

        DrawLineEx(start, end, 2.0f, YELLOW);
    }

    DrawRectangle(static_cast<int>(doorway.x), static_cast<int>(doorway.y + doorway.height * 0.70f), static_cast<int>(doorway.width), static_cast<int>(doorway.height * 0.30f), GREEN);
    DrawCircle(static_cast<int>(doorway.x + 18), static_cast<int>(doorway.y + doorway.height * 0.72f), 22, GREEN);
    DrawCircle(static_cast<int>(doorway.x + 58), static_cast<int>(doorway.y + doorway.height * 0.70f), 26, GREEN);

    EndScissorMode();
}

void DrawMachineBox(Rectangle rect, float gearRotation, bool running) {
    DrawRectangleRec(rect, LIGHTGRAY);
    DrawRectangleLinesEx(rect, 2, BLACK);
    DrawGear({rect.x + rect.width * 0.42f, rect.y + rect.height * 0.48f}, rect.height * 0.25f, gearRotation);

    bool blinkOn = running && fmodf(static_cast<float>(GetTime()) * 4.0f, 1.0f) < 0.5f;
    Color lightColor = blinkOn ? GREEN : DARKGRAY;

    DrawCircleV({rect.x + rect.width - 19.0f, rect.y + rect.height - 13.0f}, 4.0f, lightColor);
    DrawCircleV({rect.x + rect.width - 8.0f, rect.y + rect.height - 13.0f}, 4.0f, lightColor);
}

void DrawPlayer(const Player& player, Texture2D texture) {
    if (texture.id > 0) {
        float frameWidth = static_cast<float>(texture.width) / 3.0f;
        int frameIndex = 1;
        if (player.walking) {
            frameIndex = (static_cast<int>(GetTime() * 8.0) % 2 == 0) ? 0 : 2;
        }

        float sourceX = frameWidth * frameIndex;
        Rectangle source{
            player.facingRight ? sourceX : sourceX + frameWidth,
            0,
            player.facingRight ? frameWidth : -frameWidth,
            static_cast<float>(texture.height)
        };
        DrawTexturePro(texture, source, player.rect, {0, 0}, 0.0f, WHITE);
    }
    else {
        DrawRectangleRec(player.rect, BLACK);
    }
}

void DrawRotaryLatch(const RotaryLatch& latch, bool playerNear) {
    Color ringColor = latch.latched ? GREEN : (IsRotaryLatchAligned(latch) ? ORANGE : BLACK);
    Color spokeColor = latch.latched ? GREEN : BLACK;

    DrawRing(latch.center, latch.radius - 6.0f, latch.radius, 0.0f, 360.0f, 36, BROWN);
    DrawCircleLinesV(latch.center, latch.radius, ringColor);

    Vector2 targetOuter{
        latch.center.x + cosf(latch.targetAngle * DEG2RAD) * latch.radius,
        latch.center.y + sinf(latch.targetAngle * DEG2RAD) * latch.radius
    };
    Vector2 targetInner{
        latch.center.x + cosf(latch.targetAngle * DEG2RAD) * (latch.radius - 17.0f),
        latch.center.y + sinf(latch.targetAngle * DEG2RAD) * (latch.radius - 17.0f)
    };
    DrawLineEx(targetInner, targetOuter, 5.0f, GREEN);

    Vector2 spokeEnd{
        latch.center.x + cosf(latch.angle * DEG2RAD) * latch.radius,
        latch.center.y + sinf(latch.angle * DEG2RAD) * latch.radius
    };
    DrawLineEx(latch.center, spokeEnd, 5.0f, spokeColor);

    if (playerNear && !latch.latched) {
        DrawText("E", static_cast<int>(latch.center.x - 6.0f), static_cast<int>(latch.center.y - latch.radius - 28.0f), 20, ringColor);
    }
}
