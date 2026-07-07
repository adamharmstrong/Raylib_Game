#include "Render.h"

#include "Machine.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float AtomicRealmTileSize = 32.0f;

    void DrawSolidRedTriangle(Vector2 a, Vector2 b, Vector2 c) {
        DrawTriangle(a, b, c, RED);
        DrawTriangle(a, c, b, RED);
    }

    void DrawSolidTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
        DrawTriangle(a, b, c, color);
        DrawTriangle(a, c, b, color);
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

    Rectangle GetAtomicRealmSolidCell(int column, int row, int columns, int rows, float drawWidth, float drawHeight) {
        float sourceX = 0.0f;
        if (columns == 1) {
            sourceX = 3.0f * AtomicRealmTileSize;
        }
        else if (column == 0) {
            sourceX = 0.0f;
        }
        else if (column == columns - 1) {
            sourceX = 2.0f * AtomicRealmTileSize;
        }
        else {
            sourceX = 1.0f * AtomicRealmTileSize;
        }

        float sourceY = 0.0f;
        if (row == 0) {
            sourceY = 0.0f;
        }
        else if (row == rows - 1) {
            sourceY = 3.0f * AtomicRealmTileSize;
        }
        else if (row == 1) {
            sourceY = 1.0f * AtomicRealmTileSize;
        }
        else {
            sourceY = 2.0f * AtomicRealmTileSize;
        }

        if (column == columns - 1 && drawWidth < AtomicRealmTileSize) {
            sourceX += AtomicRealmTileSize - drawWidth;
        }
        if (row == rows - 1 && drawHeight < AtomicRealmTileSize) {
            sourceY += AtomicRealmTileSize - drawHeight;
        }

        return {sourceX, sourceY, drawWidth, drawHeight};
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

void DrawRepeatingTexture(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    Rectangle sourceTile{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    DrawTiledTextureRect(texture, sourceTile, dest, tint);
}

void DrawAtomicRealmBackgroundFill(Texture2D texture, Rectangle dest, Color tint) {
    Rectangle fillTile{AtomicRealmTileSize, AtomicRealmTileSize * 2.0f, AtomicRealmTileSize, AtomicRealmTileSize};
    Rectangle detailTile{0.0f, 0.0f, AtomicRealmTileSize, AtomicRealmTileSize};
    DrawTiledTextureRect(texture, fillTile, dest, tint);
    DrawTiledTextureRect(texture, detailTile, dest, Fade(tint, 0.28f));
}

void DrawAtomicRealmSolidFill(Texture2D texture, Rectangle dest, Color tint) {
    Rectangle fillTile{AtomicRealmTileSize, AtomicRealmTileSize * 2.0f, AtomicRealmTileSize, AtomicRealmTileSize};
    DrawTiledTextureRect(texture, fillTile, dest, tint);
}

void DrawAtomicRealmPitWalls(Texture2D texture, Rectangle pit, Color tint) {
    if (texture.id <= 0 || pit.height <= 0.0f) {
        return;
    }

    Rectangle leftEdge{0.0f, AtomicRealmTileSize, AtomicRealmTileSize, AtomicRealmTileSize};
    Rectangle rightEdge{AtomicRealmTileSize * 2.0f, AtomicRealmTileSize, AtomicRealmTileSize, AtomicRealmTileSize};
    DrawTiledTextureRect(texture, leftEdge, {pit.x - AtomicRealmTileSize, pit.y, AtomicRealmTileSize, pit.height}, tint);
    DrawTiledTextureRect(texture, rightEdge, {pit.x + pit.width, pit.y, AtomicRealmTileSize, pit.height}, tint);
}

void DrawAtomicRealmPitFoundation(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    if (dest.width < AtomicRealmTileSize * 2.0f || dest.height <= 0.0f) {
        DrawAtomicRealmSolidFill(texture, dest, tint);
        return;
    }

    Rectangle floorCap{dest.x, dest.y, dest.width, AtomicRealmTileSize};
    Rectangle lowerFill{dest.x, dest.y + AtomicRealmTileSize, dest.width, fmaxf(0.0f, dest.height - AtomicRealmTileSize)};
    DrawAtomicRealmSolid(texture, floorCap, tint);
    DrawAtomicRealmSolidFill(texture, lowerFill, tint);
    DrawAtomicRealmPitWalls(texture, dest, tint);
}

void DrawAtomicRealmSolid(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    int columns = static_cast<int>(ceilf(dest.width / AtomicRealmTileSize));
    int rows = static_cast<int>(ceilf(dest.height / AtomicRealmTileSize));

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            float x = dest.x + column * AtomicRealmTileSize;
            float y = dest.y + row * AtomicRealmTileSize;
            float drawWidth = fminf(AtomicRealmTileSize, dest.x + dest.width - x);
            float drawHeight = fminf(AtomicRealmTileSize, dest.y + dest.height - y);
            Rectangle source = GetAtomicRealmSolidCell(column, row, columns, rows, drawWidth, drawHeight);
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

void DrawPlayer(const Player& player, Texture2D texture, int frameCount, Rectangle frameCrop) {
    if (texture.id > 0) {
        frameCount = std::max(1, frameCount);
        float frameWidth = static_cast<float>(texture.width) / static_cast<float>(frameCount);
        float frameHeight = static_cast<float>(texture.height);
        bool hasFrameCrop = frameCrop.width > 0.0f && frameCrop.height > 0.0f;
        if (!hasFrameCrop) {
            frameCrop = {0.0f, 0.0f, frameWidth, frameHeight};
        }

        int frameIndex = std::min(1, frameCount - 1);
        if (player.walking && frameCount == 3) {
            frameIndex = (static_cast<int>(player.animationTimer * 8.0f) % 2 == 0) ? 0 : 2;
        }
        else if (player.walking) {
            frameIndex = static_cast<int>(player.animationTimer * 8.0f) % frameCount;
        }

        float frameSourceX = frameWidth * frameIndex;
        Rectangle source{
            player.facingRight ? frameSourceX : frameSourceX + frameWidth,
            0.0f,
            player.facingRight ? frameWidth : -frameWidth,
            frameHeight
        };

        Rectangle dest = player.rect;
        if (hasFrameCrop) {
            float scaleX = player.rect.width / frameCrop.width;
            float scaleY = player.rect.height / frameCrop.height;
            float visibleCropX = player.facingRight ? frameCrop.x : frameWidth - frameCrop.x - frameCrop.width;
            dest = {
                player.rect.x - visibleCropX * scaleX,
                player.rect.y - frameCrop.y * scaleY,
                frameWidth * scaleX,
                frameHeight * scaleY
            };
        }

        DrawTexturePro(texture, source, dest, {0, 0}, 0.0f, WHITE);
    }
    else {
        DrawRectangleRec(player.rect, BLACK);
    }
}

void DrawEnemy(const Enemy& enemy, Texture2D texture) {
    if (texture.id > 0) {
        constexpr int frameCount = 6;
        constexpr int columns = 3;
        float frameWidth = static_cast<float>(texture.width) / columns;
        float frameHeight = static_cast<float>(texture.height) / (frameCount / columns);
        int frameIndex = enemy.walking ? static_cast<int>(GetTime() * 8.0f) % frameCount : 0;
        float sourceX = frameWidth * (frameIndex % columns);
        float sourceY = frameHeight * (frameIndex / columns);
        Rectangle source{
            enemy.facingRight ? sourceX : sourceX + frameWidth,
            sourceY,
            enemy.facingRight ? frameWidth : -frameWidth,
            frameHeight
        };

        DrawTexturePro(texture, source, enemy.rect, {0, 0}, 0.0f, WHITE);
        return;
    }

    DrawRectangleRec(enemy.rect, MAROON);
    DrawRectangleLinesEx(enemy.rect, 2, BLACK);
}

void DrawRotaryLatch(const RotaryLatch& latch, bool playerNear, const char* interactPrompt) {
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
        int promptWidth = MeasureText(interactPrompt, 20);
        DrawText(interactPrompt, static_cast<int>(latch.center.x - promptWidth * 0.5f), static_cast<int>(latch.center.y - latch.radius - 28.0f), 20, ringColor);
    }
}

void DrawValve(const Valve& valve, bool playerNear, const char* interactPrompt) {
    float openAmount = fminf(1.0f, fmaxf(0.0f, valve.turnDegrees / 360.0f));
    Color ringColor = valve.opened ? BLUE : (playerNear ? ORANGE : BLACK);
    float rotation = valve.turnDegrees;

    DrawRing(valve.center, valve.radius - 6.0f, valve.radius, 0.0f, 360.0f, 32, BROWN);
    DrawCircleLinesV(valve.center, valve.radius, ringColor);
    DrawCircleV(valve.center, 5.0f, BLACK);
    if (!valve.opened && openAmount > 0.0f) {
        DrawRing(valve.center, valve.radius + 5.0f, valve.radius + 9.0f, -90.0f, -90.0f + openAmount * 360.0f, 32, BLUE);
    }

    for (int i = 0; i < 4; i++) {
        float angle = rotation + i * 90.0f;
        Vector2 end{
            valve.center.x + cosf(angle * DEG2RAD) * valve.radius,
            valve.center.y + sinf(angle * DEG2RAD) * valve.radius
        };
        DrawLineEx(valve.center, end, 5.0f, ringColor);
    }

    if (playerNear && !valve.opened) {
        int promptWidth = MeasureText(interactPrompt, 20);
        DrawText(interactPrompt, static_cast<int>(valve.center.x - promptWidth * 0.5f), static_cast<int>(valve.center.y - valve.radius - 28.0f), 20, ringColor);
    }
}

void DrawWaterPit(const WaterPit& waterPit) {
    if (waterPit.bounds.width <= 0.0f || waterPit.bounds.height <= 0.0f) {
        return;
    }

    float pitBottom = waterPit.bounds.y + waterPit.bounds.height;
    float surfaceY = fminf(pitBottom, fmaxf(waterPit.surfaceY, waterPit.bounds.y));
    if (surfaceY >= pitBottom) {
        return;
    }

    Color waterColor = Fade(SKYBLUE, 0.62f);
    float waveTime = static_cast<float>(GetTime()) * 4.0f;
    constexpr float WaveStep = 16.0f;
    constexpr float WaveAmplitude = 4.0f;

    for (float x = waterPit.bounds.x; x < waterPit.bounds.x + waterPit.bounds.width; x += WaveStep) {
        float nextX = fminf(x + WaveStep, waterPit.bounds.x + waterPit.bounds.width);
        float y0 = surfaceY + sinf(waveTime + x * 0.045f) * WaveAmplitude;
        float y1 = surfaceY + sinf(waveTime + nextX * 0.045f) * WaveAmplitude;

        DrawTriangle({x, y0}, {x, pitBottom}, {nextX, pitBottom}, waterColor);
        DrawTriangle({x, y0}, {nextX, pitBottom}, {nextX, y1}, waterColor);
    }
}

void DrawStoneBlock(const StoneBlock& block) {
    DrawRectangleRec(block.rect, Color{92, 94, 92, 255});
    DrawRectangleLinesEx(block.rect, 3.0f, BLACK);

    DrawLineEx({block.rect.x + 9.0f, block.rect.y + 11.0f}, {block.rect.x + block.rect.width - 11.0f, block.rect.y + 7.0f}, 2.0f, DARKGRAY);
    DrawLineEx({block.rect.x + 8.0f, block.rect.y + block.rect.height - 13.0f}, {block.rect.x + block.rect.width - 12.0f, block.rect.y + block.rect.height - 8.0f}, 2.0f, DARKGRAY);
    DrawLineEx({block.rect.x + 16.0f, block.rect.y + 12.0f}, {block.rect.x + 12.0f, block.rect.y + block.rect.height - 14.0f}, 2.0f, Color{64, 65, 64, 255});
    DrawCircleV({block.rect.x + block.rect.width - 14.0f, block.rect.y + block.rect.height * 0.48f}, 3.0f, DARKGRAY);
}

void DrawSeeSaw(const SeeSaw& seeSaw) {
    float halfLength = seeSaw.length * 0.5f;
    float angle = seeSaw.angle * DEG2RAD;
    Vector2 axis{cosf(angle), sinf(angle)};
    Vector2 normal{-axis.y, axis.x};
    Vector2 left{
        seeSaw.pivot.x - axis.x * halfLength,
        seeSaw.pivot.y - axis.y * halfLength
    };
    Vector2 right{
        seeSaw.pivot.x + axis.x * halfLength,
        seeSaw.pivot.y + axis.y * halfLength
    };

    Vector2 p1{left.x - normal.x * seeSaw.thickness * 0.5f, left.y - normal.y * seeSaw.thickness * 0.5f};
    Vector2 p2{right.x - normal.x * seeSaw.thickness * 0.5f, right.y - normal.y * seeSaw.thickness * 0.5f};
    Vector2 p3{right.x + normal.x * seeSaw.thickness * 0.5f, right.y + normal.y * seeSaw.thickness * 0.5f};
    Vector2 p4{left.x + normal.x * seeSaw.thickness * 0.5f, left.y + normal.y * seeSaw.thickness * 0.5f};

    DrawSolidTriangle(p1, p2, p3, BROWN);
    DrawSolidTriangle(p1, p3, p4, BROWN);
    DrawLineEx(p1, p2, 2.0f, BLACK);
    DrawLineEx(p2, p3, 2.0f, BLACK);
    DrawLineEx(p3, p4, 2.0f, BLACK);
    DrawLineEx(p4, p1, 2.0f, BLACK);

    DrawTriangle(
        {seeSaw.pivot.x - 18.0f, seeSaw.pivot.y + 32.0f},
        {seeSaw.pivot.x + 18.0f, seeSaw.pivot.y + 32.0f},
        {seeSaw.pivot.x, seeSaw.pivot.y + 4.0f},
        DARKGRAY
    );
    DrawCircleV(seeSaw.pivot, 6.0f, BLACK);
}

void DrawChain(const Chain& chain, Texture2D texture) {
    float frameHeight = texture.id > 0 ? static_cast<float>(texture.height) : 18.0f;
    Rectangle linkSources[2]{
        {0.0f, 0.0f, 7.0f, frameHeight},
        {7.0f, 0.0f, 15.0f, frameHeight}
    };

    if (texture.id > 0 && !chain.points.empty()) {
        auto drawSimulatedLink = [&](int i) {
            Vector2 previous = i > 0 ? chain.points[i - 1] : chain.points[i];
            Vector2 next = i + 1 < static_cast<int>(chain.points.size()) ? chain.points[i + 1] : chain.points[i];
            Vector2 tangent{
                next.x - previous.x,
                next.y - previous.y
            };
            float angle = atan2f(tangent.y, tangent.x) * RAD2DEG + 90.0f;
            Rectangle source = linkSources[i % 2];
            Rectangle dest{
                chain.points[i].x,
                chain.points[i].y,
                source.width * chain.scale,
                frameHeight * chain.scale
            };
            Vector2 origin{
                dest.width * 0.5f,
                dest.height * 0.5f
            };

            DrawTexturePro(texture, source, dest, origin, angle, WHITE);
        };

        for (int i = 1; i < static_cast<int>(chain.points.size()); i += 2) {
            drawSimulatedLink(i);
        }
        for (int i = 0; i < static_cast<int>(chain.points.size()); i += 2) {
            drawSimulatedLink(i);
        }
        return;
    }

    Vector2 delta{
        chain.end.x - chain.start.x,
        chain.end.y - chain.start.y
    };
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.0f) {
        return;
    }

    float spacing = fmaxf(1.0f, chain.spacing * chain.scale);
    int linkCount = static_cast<int>(length / spacing) + 1;
    float angle = atan2f(delta.y, delta.x) * RAD2DEG + 90.0f;
    Vector2 direction{delta.x / length, delta.y / length};

    if (texture.id <= 0) {
        DrawLineEx(chain.start, chain.end, fmaxf(2.0f, 4.0f * chain.scale), DARKGRAY);
        return;
    }

    auto drawStraightLink = [&](int i) {
        float distance = fminf(i * spacing, length);
        Vector2 position{
            chain.start.x + direction.x * distance,
            chain.start.y + direction.y * distance
        };
        Rectangle source = linkSources[i % 2];
        Rectangle dest{
            position.x,
            position.y,
            source.width * chain.scale,
            frameHeight * chain.scale
        };
        Vector2 origin{
            dest.width * 0.5f,
            dest.height * 0.5f
        };

        DrawTexturePro(texture, source, dest, origin, angle, WHITE);
    };

    for (int i = 1; i < linkCount; i += 2) {
        drawStraightLink(i);
    }
    for (int i = 0; i < linkCount; i += 2) {
        drawStraightLink(i);
    }
}
