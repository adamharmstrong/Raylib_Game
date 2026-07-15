#include "Render.h"

#include "Fluid.h"
#include "Machine.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {
    constexpr float TilesetTileSize = 32.0f;

    bool IsWaterColumnOccluded(const FluidField& fluid, int column) {
        for (int row = 0; row < fluid.gridRows; row++) {
            const FluidCell& cell = fluid.cells[row * fluid.gridColumns + column];
            if (cell.solid) {
                return true;
            }
        }
        return false;
    }

    float GetVisibleWaterColumnSurface(const FluidField& fluid, int column) {
        float visibleMass = 0.0f;
        for (int row = 0; row < fluid.gridRows; row++) {
            const FluidCell& cell = fluid.cells[row * fluid.gridColumns + column];
            if (!cell.solid) {
                visibleMass += fminf(cell.mass, 1.0f);
            }
        }
        return floorf(std::clamp(
            fluid.bounds.y + fluid.bounds.height - visibleMass * fluid.cellSize,
            fluid.bounds.y,
            fluid.bounds.y + fluid.bounds.height
        ) + 0.5f);
    }

    float GetNeighborWaterSurface(const FluidField& fluid, const std::vector<float>& surfaces, int column) {
        float replacement = 0.0f;
        float weight = 0.0f;
        for (int distance = 1; distance <= fluid.gridColumns; distance++) {
            int left = column - distance;
            if (left >= 0 && !IsWaterColumnOccluded(fluid, left)) {
                replacement += surfaces[static_cast<size_t>(left)];
                weight += 1.0f;
                break;
            }
        }
        for (int distance = 1; distance <= fluid.gridColumns; distance++) {
            int right = column + distance;
            if (right < fluid.gridColumns && !IsWaterColumnOccluded(fluid, right)) {
                replacement += surfaces[static_cast<size_t>(right)];
                weight += 1.0f;
                break;
            }
        }
        return weight > 0.0f ? floorf(replacement / weight + 0.5f) : surfaces[static_cast<size_t>(column)];
    }

    void DrawWaterSplashRun(float x0, float x1, float y, Color color) {
        float width = x1 - x0;
        if (width <= 0.5f) {
            return;
        }

        int strokeCount = std::clamp(static_cast<int>(width / 5.0f), 1, 4);
        for (int stroke = 0; stroke < strokeCount; stroke++) {
            float t = (static_cast<float>(stroke) + 0.5f) / static_cast<float>(strokeCount);
            float x = x0 + width * t;
            float direction = (stroke & 1) == 0 ? -1.0f : 1.0f;
            float length = std::clamp(width / static_cast<float>(strokeCount) * 0.55f, 3.0f, 7.0f);
            DrawLineEx(
                {x - length * 0.45f, y + 1.0f},
                {x + length * 0.45f, y - 2.0f * direction},
                1.0f,
                color
            );
        }
    }

    void DrawWaterSurfaceRun(
        float x0,
        float x1,
        float y,
        Color color,
        const std::vector<Rectangle>& splashSources
    ) {
        constexpr float SplashHorizontalPadding = 4.0f;
        constexpr float SplashVerticalPadding = 4.0f;
        std::vector<std::pair<float, float>> splashIntervals;

        for (const Rectangle& source : splashSources) {
            float sourceBottom = source.y + source.height;
            if (y < source.y - SplashVerticalPadding || y > sourceBottom + SplashVerticalPadding) {
                continue;
            }

            float splashStart = fmaxf(x0, source.x - SplashHorizontalPadding);
            float splashEnd = fminf(x1, source.x + source.width + SplashHorizontalPadding);
            if (splashEnd - splashStart > 0.5f) {
                splashIntervals.emplace_back(splashStart, splashEnd);
            }
        }

        if (splashIntervals.empty()) {
            DrawLineEx({x0, y}, {x1, y}, 1.0f, color);
            return;
        }

        std::sort(splashIntervals.begin(), splashIntervals.end());
        float cursor = x0;
        for (const auto& interval : splashIntervals) {
            float splashStart = fmaxf(cursor, interval.first);
            float splashEnd = interval.second;
            if (splashEnd - splashStart <= 0.5f) {
                continue;
            }
            if (splashStart - cursor > 0.5f) {
                DrawLineEx({cursor, y}, {splashStart, y}, 1.0f, color);
            }
            DrawWaterSplashRun(splashStart, splashEnd, y, color);
            cursor = fmaxf(cursor, splashEnd);
        }
        if (x1 - cursor > 0.5f) {
            DrawLineEx({cursor, y}, {x1, y}, 1.0f, color);
        }
    }

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

    Rectangle GetTilesetCell(int column, int row, float drawWidth = TilesetTileSize, float drawHeight = TilesetTileSize) {
        return {
            column * TilesetTileSize,
            row * TilesetTileSize,
            drawWidth,
            drawHeight
        };
    }

    void DrawTilesetCell(Texture2D texture, int column, int row, Rectangle target, Color tint) {
        Rectangle source = GetTilesetCell(column, row, target.width, target.height);
        DrawTexturePro(texture, source, target, {0, 0}, 0.0f, tint);
    }

    void DrawTilesetWallColumn(Texture2D texture, int column, Rectangle dest, Color tint) {
        int rows = static_cast<int>(ceilf(dest.height / TilesetTileSize));
        for (int row = 0; row < rows; row++) {
            float y = dest.y + row * TilesetTileSize;
            float drawHeight = fminf(TilesetTileSize, dest.y + dest.height - y);
            int sourceRow = 1 + (row % 2);
            if (row == rows - 1) {
                sourceRow = 3;
            }

            DrawTilesetCell(texture, column, sourceRow, {dest.x, y, dest.width, drawHeight}, tint);
        }
    }

    Rectangle GetTilesetSolidCell(int column, int row, int columns, int rows, float drawWidth, float drawHeight) {
        float sourceX = 0.0f;
        if (columns == 1) {
            sourceX = 3.0f * TilesetTileSize;
        }
        else if (column == 0) {
            sourceX = 0.0f;
        }
        else if (column == columns - 1) {
            sourceX = 2.0f * TilesetTileSize;
        }
        else {
            sourceX = 1.0f * TilesetTileSize;
        }

        float sourceY = 0.0f;
        if (row == 0) {
            sourceY = 0.0f;
        }
        else if (row == rows - 1) {
            sourceY = 3.0f * TilesetTileSize;
        }
        else if (row == 1) {
            sourceY = 1.0f * TilesetTileSize;
        }
        else {
            sourceY = 2.0f * TilesetTileSize;
        }

        if (column == columns - 1 && drawWidth < TilesetTileSize) {
            sourceX += TilesetTileSize - drawWidth;
        }
        if (row == rows - 1 && drawHeight < TilesetTileSize) {
            sourceY += TilesetTileSize - drawHeight;
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

void DrawRope(Vector2 start, Vector2 end, float thickness, float patternOffset) {
    DrawLineEx(start, end, thickness, BROWN);

    Vector2 delta{end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (length < 8.0f) return;

    Vector2 direction{delta.x / length, delta.y / length};
    Vector2 perpendicular{-direction.y, direction.x};
    float spacing = fmaxf(10.0f, thickness * 2.4f);
    float along = fmaxf(2.5f, thickness * 0.65f);
    float across = thickness * 0.48f;
    float firstMark = fmodf(patternOffset, spacing);
    if (firstMark < 0.0f) firstMark += spacing;
    for (float distance = firstMark; distance < length; distance += spacing) {
        Vector2 center{start.x + direction.x * distance, start.y + direction.y * distance};
        Vector2 markStart{
            center.x - direction.x * along + perpendicular.x * across,
            center.y - direction.y * along + perpendicular.y * across
        };
        Vector2 markEnd{
            center.x + direction.x * along - perpendicular.x * across,
            center.y + direction.y * along - perpendicular.y * across
        };
        DrawLineEx(markStart, markEnd, 1.0f, BLACK);
    }
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

void DrawTilesetTile(Texture2D texture, int column, int row, Vector2 position, Color tint) {
    if (texture.id <= 0) {
        DrawRectangle(static_cast<int>(position.x), static_cast<int>(position.y), 32, 32, tint);
        return;
    }

    DrawTilesetCell(texture, column, row, {position.x, position.y, TilesetTileSize, TilesetTileSize}, tint);
}

void DrawTilesetBackgroundFill(Texture2D texture, Rectangle dest, Color tint, float detailOpacity) {
    Rectangle fillTile{TilesetTileSize, TilesetTileSize * 2.0f, TilesetTileSize, TilesetTileSize};
    Rectangle detailTile{0.0f, 0.0f, TilesetTileSize, TilesetTileSize};
    DrawTiledTextureRect(texture, fillTile, dest, tint);
    DrawTiledTextureRect(texture, detailTile, dest, Fade(tint, detailOpacity));
}

void DrawTilesetSolidFill(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    DrawRectangleRec(dest, Color{8, 12, 16, tint.a});

    if (dest.width <= TilesetTileSize) {
        DrawTilesetWallColumn(texture, 4, dest, tint);
        return;
    }

    DrawTilesetWallColumn(texture, 4, {dest.x, dest.y, TilesetTileSize, dest.height}, tint);
    DrawTilesetWallColumn(texture, 5, {dest.x + dest.width - TilesetTileSize, dest.y, TilesetTileSize, dest.height}, tint);
}

void DrawTilesetWall(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    DrawRectangleRec(dest, Color{8, 12, 16, tint.a});

    if (dest.width <= TilesetTileSize) {
        DrawTilesetWallColumn(texture, 4, dest, tint);
        return;
    }

    DrawTilesetWallColumn(texture, 4, {dest.x, dest.y, TilesetTileSize, dest.height}, tint);
    DrawTilesetWallColumn(texture, 5, {dest.x + dest.width - TilesetTileSize, dest.y, TilesetTileSize, dest.height}, tint);
}

void DrawTilesetCeiling(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    int columns = static_cast<int>(ceilf(dest.width / TilesetTileSize));
    int rows = static_cast<int>(ceilf(dest.height / TilesetTileSize));

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            float x = dest.x + column * TilesetTileSize;
            float y = dest.y + row * TilesetTileSize;
            float drawWidth = fminf(TilesetTileSize, dest.x + dest.width - x);
            float drawHeight = fminf(TilesetTileSize, dest.y + dest.height - y);

            int sourceColumn = 1;
            if (column == 0) {
                sourceColumn = 0;
            }
            else if (column == columns - 1) {
                sourceColumn = 2;
            }

            DrawTilesetCell(texture, sourceColumn, 3, {x, y, drawWidth, drawHeight}, tint);
        }
    }
}

void DrawTilesetPitWalls(Texture2D texture, Rectangle pit, Color tint) {
    if (texture.id <= 0 || pit.height <= 0.0f) {
        return;
    }

    int rows = static_cast<int>(ceilf(pit.height / TilesetTileSize));
    for (int tileRow = 0; tileRow < rows; tileRow++) {
        float y = pit.y + tileRow * TilesetTileSize;
        float drawHeight = fminf(TilesetTileSize, pit.y + pit.height - y);
        int sourceRow = 1 + (tileRow % 2);
        if (tileRow == rows - 1) {
            sourceRow = 3;
        }

        DrawTilesetCell(texture, 5, sourceRow, {pit.x - TilesetTileSize, y, TilesetTileSize, drawHeight}, tint);
        DrawTilesetCell(texture, 4, sourceRow, {pit.x + pit.width, y, TilesetTileSize, drawHeight}, tint);
    }
}

void DrawTilesetPitFoundation(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    if (dest.width < TilesetTileSize * 2.0f || dest.height <= 0.0f) {
        DrawTilesetSolidFill(texture, dest, tint);
        return;
    }

    int columns = static_cast<int>(ceilf(dest.width / TilesetTileSize));
    int rows = static_cast<int>(ceilf(dest.height / TilesetTileSize));

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            float x = dest.x + column * TilesetTileSize;
            float y = dest.y + row * TilesetTileSize;
            float drawWidth = fminf(TilesetTileSize, dest.x + dest.width - x);
            float drawHeight = fminf(TilesetTileSize, dest.y + dest.height - y);

            int sourceColumn = 1;
            int sourceRow = 2;
            if (row == 0) {
                if (column == 0) {
                    sourceColumn = 0;
                }
                else if (column == columns - 1) {
                    sourceColumn = 2;
                }
                else {
                    sourceColumn = 1;
                }
                sourceRow = 0;
            }
            else if (row == rows - 1) {
                if (column == 0) {
                    sourceColumn = 0;
                }
                else if (column == columns - 1) {
                    sourceColumn = 2;
                }
                else {
                    sourceColumn = 1;
                }
                sourceRow = 3;
            }
            else if (column == 0) {
                sourceColumn = 0;
                sourceRow = 1 + (row % 2);
            }
            else if (column == columns - 1) {
                sourceColumn = 2;
                sourceRow = 1 + (row % 2);
            }
            else {
                sourceColumn = 1;
                sourceRow = 1 + (row % 2);
            }

            DrawTilesetCell(texture, sourceColumn, sourceRow, {x, y, drawWidth, drawHeight}, tint);
        }
    }

    DrawTilesetPitWalls(texture, dest, tint);
}

void DrawTilesetSolid(Texture2D texture, Rectangle dest, Color tint) {
    if (texture.id <= 0) {
        DrawRectangleRec(dest, tint);
        return;
    }

    int columns = static_cast<int>(ceilf(dest.width / TilesetTileSize));
    int rows = static_cast<int>(ceilf(dest.height / TilesetTileSize));

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            float x = dest.x + column * TilesetTileSize;
            float y = dest.y + row * TilesetTileSize;
            float drawWidth = fminf(TilesetTileSize, dest.x + dest.width - x);
            float drawHeight = fminf(TilesetTileSize, dest.y + dest.height - y);
            Rectangle source = GetTilesetSolidCell(column, row, columns, rows, drawWidth, drawHeight);
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

void DrawHazardWeight(const HangingWeight& weight, float ropePatternOffset) {
    float ropeX = weight.pulley.x + weight.pulleyRadius;

    DrawRope({ropeX, weight.pulley.y}, {weight.rect.x + weight.rect.width / 2.0f, weight.rect.y}, 4, ropePatternOffset);
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

void DrawPlayer(const Player& player, Texture2D texture, int spriteRow) {
    if (texture.id > 0) {
        constexpr float FrameWidth = 37.0f;
        constexpr float FrameHeight = 47.0f;

        spriteRow = std::clamp(spriteRow, 0, 2);
        int frameIndex = 1;
        if (player.walking) {
            frameIndex = (static_cast<int>(player.animationTimer * 8.0f) % 2 == 0) ? 0 : 2;
        }
        float sourceX = frameIndex * FrameWidth;
        float sourceY = spriteRow * FrameHeight;
        Rectangle source{
            sourceX,
            sourceY,
            player.facingRight ? FrameWidth : -FrameWidth,
            FrameHeight
        };
        Rectangle dest{
            player.rect.x + (player.rect.width - FrameWidth) * 0.5f,
            player.rect.y + player.rect.height - FrameHeight + 1.0f,
            FrameWidth,
            FrameHeight
        };

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

void DrawFluidBackground(const FluidField& fluid) {
    if (fluid.type != FluidType::Water || fluid.cells.empty()) {
        return;
    }

    Color backingColor{24, 132, 201, 255};
    std::vector<float> surfaces(static_cast<size_t>(fluid.gridColumns), fluid.bounds.y + fluid.bounds.height);
    std::vector<unsigned char> occludedColumns(static_cast<size_t>(fluid.gridColumns), 0);
    for (int column = 0; column < fluid.gridColumns; column++) {
        occludedColumns[static_cast<size_t>(column)] = IsWaterColumnOccluded(fluid, column) ? 1 : 0;
        surfaces[static_cast<size_t>(column)] = GetVisibleWaterColumnSurface(fluid, column);
    }

    for (int column = 0; column < fluid.gridColumns; column++) {
        if (occludedColumns[static_cast<size_t>(column)] == 0) {
            continue;
        }

        surfaces[static_cast<size_t>(column)] = GetNeighborWaterSurface(fluid, surfaces, column);
    }

    int runStart = 0;
    float runSurface = fluid.bounds.y + fluid.bounds.height;
    for (int column = 0; column <= fluid.gridColumns; column++) {
        float surface = fluid.bounds.y + fluid.bounds.height;
        if (column < fluid.gridColumns) {
            surface = surfaces[static_cast<size_t>(column)];
        }

        if (column == 0) {
            runSurface = surface;
            continue;
        }
        if (column < fluid.gridColumns && fabsf(surface - runSurface) < 0.5f) {
            continue;
        }

        float x = fluid.bounds.x + static_cast<float>(runStart) * fluid.cellSize;
        float right = fminf(
            fluid.bounds.x + static_cast<float>(column) * fluid.cellSize,
            fluid.bounds.x + fluid.bounds.width
        );
        DrawRectangleRec(
            {x, runSurface, right - x, fluid.bounds.y + fluid.bounds.height - runSurface},
            backingColor
        );
        runStart = column;
        runSurface = surface;
    }
}

void DrawFluidField(const FluidField& fluid, const std::vector<Rectangle>& splashSources) {
    if (fluid.type == FluidType::Water || fluid.type == FluidType::Sand) {
        if (fluid.cells.empty()) return;
        bool sand = fluid.type == FluidType::Sand;
        Color material = sand ? Color{198, 158, 82, 255} : Color{28, 143, 211, 238};
        Color denseMaterial = sand ? Color{139, 96, 45, 255} : Color{18, 112, 184, 245};
        Color surfaceColor = sand ? Color{241, 208, 132, 255} : Color{205, 241, 250, 255};
        auto hasRenderableMass = [&](const FluidCell& cell) {
            return cell.mass > 0.01f && (!cell.solid || !sand);
        };
        // Merge adjacent pixels with similar fill into horizontal runs. A full pool
        // therefore costs roughly one draw call per row rather than one per pixel.
        for (int row = 0; row < fluid.gridRows; row++) {
            int column = 0;
            while (column < fluid.gridColumns) {
                const FluidCell& first = fluid.cells[row * fluid.gridColumns + column];
                if (!hasRenderableMass(first)) {
                    column++;
                    continue;
                }
                int fillBand = std::clamp(static_cast<int>(first.mass * 16.0f + 0.5f), 1, 16);
                int runStart = column++;
                while (column < fluid.gridColumns) {
                    const FluidCell& next = fluid.cells[row * fluid.gridColumns + column];
                    int nextBand = std::clamp(static_cast<int>(next.mass * 16.0f + 0.5f), 1, 16);
                    if (!hasRenderableMass(next) || nextBand != fillBand) break;
                    column++;
                }
                float fill = static_cast<float>(fillBand) / 16.0f;
                float x = fluid.bounds.x + static_cast<float>(runStart) * fluid.cellSize;
                float y = fluid.bounds.y + static_cast<float>(row) * fluid.cellSize;
                float width = fminf(static_cast<float>(column - runStart) * fluid.cellSize + 0.01f,
                    fluid.bounds.x + fluid.bounds.width - x);
                float cellBottom = fminf(y + fluid.cellSize, fluid.bounds.y + fluid.bounds.height);
                float height = fminf(fluid.cellSize * fill + 0.01f, cellBottom - y);
                DrawRectangleRec({x, cellBottom - height, width, height},
                    ColorLerp(material, denseMaterial, std::clamp((fill - 0.75f) * 1.6f, 0.0f, 1.0f)));
            }

            column = 0;
            while (column < fluid.gridColumns) {
                auto isSurface = [&](int candidate) {
                    const FluidCell& cell = fluid.cells[row * fluid.gridColumns + candidate];
                    bool openAbove = row == 0 ||
                        (!fluid.cells[(row - 1) * fluid.gridColumns + candidate].solid &&
                         fluid.cells[(row - 1) * fluid.gridColumns + candidate].mass < 0.03f);
                    return (sand || !IsWaterColumnOccluded(fluid, candidate)) &&
                        !cell.solid && hasRenderableMass(cell) && cell.mass > 0.20f &&
                        openAbove;
                };
                if (!isSurface(column)) {
                    column++;
                    continue;
                }
                int runStart = column++;
                while (column < fluid.gridColumns && isSurface(column)) column++;
                float x0 = fluid.bounds.x + static_cast<float>(runStart) * fluid.cellSize;
                float x1 = fluid.bounds.x + static_cast<float>(column) * fluid.cellSize;
                float y = fluid.bounds.y + static_cast<float>(row) * fluid.cellSize + 0.5f;
                Color highlight = Fade(surfaceColor, sand ? 0.34f : 0.55f);
                if (!sand) {
                    DrawWaterSurfaceRun(x0, x1, y, highlight, splashSources);
                }
                else {
                    DrawLineEx({x0, y}, {x1, y}, 1.0f, highlight);
                }
            }
        }
        return;
    }

    if (fluid.particles.empty()) {
        return;
    }

    float time = static_cast<float>(GetTime());
    if (fluid.type == FluidType::Gel) {
        Color water{55, 126, 194, 255};
        float renderRadius = fluid.particleRadius * 1.08f;
        for (const FluidParticle& particle : fluid.particles) {
            DrawCircleV(particle.position, renderRadius, water);
        }

        for (int index = 0; index < static_cast<int>(fluid.particles.size()); index++) {
            const FluidParticle& particle = fluid.particles[index];
            if (particle.surface && index % 5 == 0) {
                float wave = sinf(time * 3.6f + particle.position.x * 0.055f) * 0.65f;
                Vector2 highlight{
                    particle.position.x,
                    particle.position.y - renderRadius * 0.72f + wave
                };
                DrawLineEx(
                    {highlight.x - renderRadius * 0.42f, highlight.y},
                    {highlight.x + renderRadius * 0.42f, highlight.y},
                    1.35f,
                    Fade(Color{214, 243, 249, 255}, 0.78f)
                );
            }

            float speed = sqrtf(
                particle.velocity.x * particle.velocity.x +
                particle.velocity.y * particle.velocity.y
            );
            if (speed > 42.0f && index % 13 == 0) {
                float scale = fminf(5.0f, speed * 0.016f) / speed;
                DrawLineEx(
                    {
                        particle.position.x - particle.velocity.x * scale,
                        particle.position.y - particle.velocity.y * scale
                    },
                    {
                        particle.position.x + particle.velocity.x * scale,
                        particle.position.y + particle.velocity.y * scale
                    },
                    1.0f,
                    Fade(RAYWHITE, 0.34f)
                );
            }
        }
        return;
    }

    Color gas{174, 211, 197, 255};
    for (int index = 0; index < static_cast<int>(fluid.particles.size()); index++) {
        const FluidParticle& particle = fluid.particles[index];
        float phase = time * 1.9f + static_cast<float>(index) * 1.71f;
        float pulse = 0.92f + sinf(phase) * 0.08f;
        float radius = fluid.particleRadius * (1.55f + particle.density * 0.40f) * pulse;
        DrawCircleV(particle.position, radius, Fade(gas, 0.07f + particle.density * 0.12f));

        if (index % 4 == 0) {
            Vector2 direction{
                particle.velocity.x * 0.018f + cosf(phase) * 3.0f,
                particle.velocity.y * 0.018f - 2.5f
            };
            DrawLineEx(
                {particle.position.x - direction.x, particle.position.y - direction.y},
                {particle.position.x + direction.x, particle.position.y + direction.y},
                1.2f,
                Fade(RAYWHITE, 0.20f + particle.density * 0.28f)
            );
        }
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

void DrawBoulder(const Boulder& boulder) {
    float rotation = boulder.rotation * DEG2RAD;
    auto rotateOffset = [&](float x, float y) {
        return Vector2{
            boulder.center.x + x * cosf(rotation) - y * sinf(rotation),
            boulder.center.y + x * sinf(rotation) + y * cosf(rotation)
        };
    };

    DrawCircleV(boulder.center, boulder.radius, Color{96, 97, 94, 255});
    DrawCircleLinesV(boulder.center, boulder.radius, BLACK);
    DrawCircleLinesV(rotateOffset(-boulder.radius * 0.18f, boulder.radius * 0.10f), boulder.radius * 0.62f, DARKGRAY);
    DrawLineEx(
        rotateOffset(-boulder.radius * 0.65f, -boulder.radius * 0.15f),
        rotateOffset(-boulder.radius * 0.08f, -boulder.radius * 0.42f),
        2.0f,
        Color{65, 66, 65, 255}
    );
    DrawLineEx(
        rotateOffset(boulder.radius * 0.18f, boulder.radius * 0.42f),
        rotateOffset(boulder.radius * 0.70f, boulder.radius * 0.12f),
        2.0f,
        Color{65, 66, 65, 255}
    );
    DrawCircleV(rotateOffset(-boulder.radius * 0.28f, -boulder.radius * 0.28f), boulder.radius * 0.12f, Fade(RAYWHITE, 0.35f));
}

void DrawPhysicsWheel(const PhysicsWheel& wheel) {
    DrawCircleV(wheel.center, wheel.radius, Color{48, 52, 54, 255});
    DrawCircleV(wheel.center, wheel.radius * 0.72f, Color{104, 109, 111, 255});
    DrawCircleLinesV(wheel.center, wheel.radius, BLACK);
    DrawCircleLinesV(wheel.center, wheel.radius * 0.72f, BLACK);
    DrawCircleV(wheel.center, wheel.radius * 0.18f, Color{32, 34, 36, 255});

    for (int i = 0; i < 6; i++) {
        float angle = wheel.rotation * DEG2RAD + static_cast<float>(i) * 60.0f * DEG2RAD;
        Vector2 end{
            wheel.center.x + cosf(angle) * wheel.radius * 0.62f,
            wheel.center.y + sinf(angle) * wheel.radius * 0.62f
        };
        DrawLineEx(wheel.center, end, 3.0f, BLACK);
    }

    Vector2 highlight{
        wheel.center.x + cosf(wheel.rotation * DEG2RAD - 0.8f) * wheel.radius * 0.48f,
        wheel.center.y + sinf(wheel.rotation * DEG2RAD - 0.8f) * wheel.radius * 0.48f
    };
    DrawCircleV(highlight, wheel.radius * 0.09f, Fade(RAYWHITE, 0.35f));
}

void DrawGear(const Gear& gear) {
    // Eight narrow teeth leave enough clearance to engage the screw's standard flight spacing.
    float rootRadius = gear.radius * 0.82f;
    float toothDepth = gear.radius * 0.30f;
    float toothWidth = gear.radius * 0.34f;
    Color toothColor{124, 111, 79, 255};
    Color webColor{102, 108, 109, 255};

    for (int i = 0; i < GearToothCount; i++) {
        float angleDegrees = gear.rotation + static_cast<float>(i) * (360.0f / GearToothCount);
        float angle = angleDegrees * DEG2RAD;
        Vector2 toothCenter{
            gear.center.x + cosf(angle) * (rootRadius + toothDepth * 0.5f),
            gear.center.y + sinf(angle) * (rootRadius + toothDepth * 0.5f)
        };
        DrawRectanglePro(
            {toothCenter.x, toothCenter.y, toothDepth + 2.0f, toothWidth + 2.0f},
            {(toothDepth + 2.0f) * 0.5f, (toothWidth + 2.0f) * 0.5f},
            angleDegrees,
            BLACK
        );
        DrawRectanglePro(
            {toothCenter.x, toothCenter.y, toothDepth, toothWidth},
            {toothDepth * 0.5f, toothWidth * 0.5f},
            angleDegrees,
            toothColor
        );
    }

    DrawCircleV(gear.center, rootRadius + 1.5f, BLACK);
    DrawCircleV(gear.center, rootRadius, webColor);

    for (int i = 0; i < 4; i++) {
        float angle = (gear.rotation + 45.0f + static_cast<float>(i) * 90.0f) * DEG2RAD;
        Vector2 hole{
            gear.center.x + cosf(angle) * gear.radius * 0.48f,
            gear.center.y + sinf(angle) * gear.radius * 0.48f
        };
        DrawCircleV(hole, gear.radius * 0.16f + 1.0f, BLACK);
        DrawCircleV(hole, gear.radius * 0.16f, Color{38, 43, 45, 255});
    }

    DrawCircleV(gear.center, gear.radius * 0.25f + 1.5f, BLACK);
    DrawCircleV(gear.center, gear.radius * 0.25f, toothColor);
    DrawCircleV(gear.center, gear.radius * 0.09f, Color{32, 35, 36, 255});
    DrawCircleLinesV(gear.center, rootRadius, BLACK);
}

void DrawFlywheel(const Flywheel& flywheel) {
    Color edgeColor{27, 31, 33, 255};
    Color ironColor{68, 74, 77, 255};
    Color machinedColor{112, 119, 121, 255};
    float outerRadius = flywheel.radius;
    float inertiaBandInner = flywheel.radius * 0.72f;
    float hubRadius = flywheel.radius * 0.24f;

    DrawCircleV(flywheel.center, outerRadius + 2.0f, BLACK);
    DrawCircleV(flywheel.center, outerRadius, ironColor);
    DrawRing(flywheel.center, inertiaBandInner, outerRadius - 2.0f, 0.0f, 360.0f, 40, machinedColor);
    DrawCircleLinesV(flywheel.center, inertiaBandInner, edgeColor);

    for (int i = 0; i < 3; i++) {
        float angle = (flywheel.rotation + 30.0f + static_cast<float>(i) * 120.0f) * DEG2RAD;
        Vector2 balanceHole{
            flywheel.center.x + cosf(angle) * flywheel.radius * 0.48f,
            flywheel.center.y + sinf(angle) * flywheel.radius * 0.48f
        };
        DrawCircleV(balanceHole, flywheel.radius * 0.105f + 1.5f, edgeColor);
        DrawCircleV(balanceHole, flywheel.radius * 0.105f, Color{41, 46, 48, 255});
    }

    DrawCircleV(flywheel.center, hubRadius + 2.0f, edgeColor);
    DrawCircleV(flywheel.center, hubRadius, machinedColor);
    DrawCircleV(flywheel.center, flywheel.radius * 0.085f, edgeColor);

    float keyAngle = flywheel.rotation * DEG2RAD;
    Vector2 keyStart{
        flywheel.center.x + cosf(keyAngle) * flywheel.radius * 0.08f,
        flywheel.center.y + sinf(keyAngle) * flywheel.radius * 0.08f
    };
    Vector2 keyEnd{
        flywheel.center.x + cosf(keyAngle) * flywheel.radius * 0.22f,
        flywheel.center.y + sinf(keyAngle) * flywheel.radius * 0.22f
    };
    DrawLineEx(keyStart, keyEnd, fmaxf(2.0f, flywheel.radius * 0.065f), Color{186, 151, 73, 255});

    DrawRing(
        flywheel.center,
        flywheel.radius * 0.90f,
        flywheel.radius * 0.95f,
        195.0f,
        335.0f,
        20,
        Fade(RAYWHITE, 0.30f)
    );
}

void DrawSteeringWheel(const SteeringWheel& steeringWheel) {
    float rimInnerRadius = steeringWheel.radius * 0.72f;
    float hubRadius = steeringWheel.radius * 0.18f;
    Color castIron{70, 77, 80, 255};
    Color rimHighlight{119, 126, 128, 255};

    for (int i = 0; i < 3; i++) {
        float angle = (steeringWheel.rotation + static_cast<float>(i) * 120.0f) * DEG2RAD;
        Vector2 spokeEnd{
            steeringWheel.center.x + cosf(angle) * rimInnerRadius,
            steeringWheel.center.y + sinf(angle) * rimInnerRadius
        };
        DrawLineEx(steeringWheel.center, spokeEnd, steeringWheel.radius * 0.18f + 2.0f, BLACK);
        DrawLineEx(steeringWheel.center, spokeEnd, steeringWheel.radius * 0.18f, castIron);
    }

    DrawRing(steeringWheel.center, rimInnerRadius - 2.0f, steeringWheel.radius, 0.0f, 360.0f, 40, BLACK);
    DrawRing(steeringWheel.center, rimInnerRadius, steeringWheel.radius - 2.0f, 0.0f, 360.0f, 40, castIron);
    DrawRing(
        steeringWheel.center,
        steeringWheel.radius * 0.88f,
        steeringWheel.radius * 0.94f,
        195.0f,
        335.0f,
        20,
        Fade(rimHighlight, 0.75f)
    );

    DrawCircleV(steeringWheel.center, hubRadius + 2.0f, BLACK);
    DrawCircleV(steeringWheel.center, hubRadius, rimHighlight);
    DrawCircleV(steeringWheel.center, steeringWheel.radius * 0.075f, BLACK);

    float keyAngle = steeringWheel.rotation * DEG2RAD;
    Vector2 keyStart{
        steeringWheel.center.x + cosf(keyAngle) * steeringWheel.radius * 0.07f,
        steeringWheel.center.y + sinf(keyAngle) * steeringWheel.radius * 0.07f
    };
    Vector2 keyEnd{
        steeringWheel.center.x + cosf(keyAngle) * steeringWheel.radius * 0.17f,
        steeringWheel.center.y + sinf(keyAngle) * steeringWheel.radius * 0.17f
    };
    DrawLineEx(keyStart, keyEnd, fmaxf(2.0f, steeringWheel.radius * 0.07f), Color{184, 151, 74, 255});
}

void DrawScrew(const Screw& screw) {
    float angle = screw.angle * DEG2RAD;
    Vector2 axis{cosf(angle), sinf(angle)};
    Vector2 normal{-axis.y, axis.x};
    Vector2 start{
        screw.center.x - axis.x * screw.length * 0.5f,
        screw.center.y - axis.y * screw.length * 0.5f
    };
    Vector2 end{
        screw.center.x + axis.x * screw.length * 0.5f,
        screw.center.y + axis.y * screw.length * 0.5f
    };

    struct FlightPoint {
        Vector2 outer;
        Vector2 inner;
        float depth;
    };

    float shaftRadius = fmaxf(2.5f, screw.radius * 0.28f);
    float flightRadius = screw.radius;
    float pitch = fmaxf(20.0f, screw.radius * 1.85f);
    float flightMargin = fminf(screw.length * 0.16f, screw.radius * 0.62f);
    float flightLength = fmaxf(1.0f, screw.length - flightMargin * 2.0f);
    int segmentCount = std::max(18, static_cast<int>(ceilf(flightLength / 2.5f)));
    float rotationPhase = screw.rotation * DEG2RAD;
    std::vector<FlightPoint> flight(static_cast<size_t>(segmentCount + 1));

    for (int segment = 0; segment <= segmentCount; segment++) {
        float amount = static_cast<float>(segment) / static_cast<float>(segmentCount);
        float distance = flightMargin + amount * flightLength;
        float phase = rotationPhase + distance / pitch * 2.0f * PI;
        float projectedOffset = sinf(phase);
        Vector2 center{
            start.x + axis.x * distance,
            start.y + axis.y * distance
        };
        flight[segment] = {
            {
                center.x + normal.x * projectedOffset * flightRadius,
                center.y + normal.y * projectedOffset * flightRadius
            },
            {
                center.x + normal.x * projectedOffset * shaftRadius,
                center.y + normal.y * projectedOffset * shaftRadius
            },
            cosf(phase)
        };
    }

    auto drawFlightLayer = [&](bool front) {
        Color flightColor = front ? Color{132, 139, 142, 255} : Color{57, 62, 65, 255};
        Color edgeColor = front ? Color{29, 33, 35, 255} : Color{20, 23, 25, 255};
        for (int segment = 0; segment < segmentCount; segment++) {
            const FlightPoint& first = flight[segment];
            const FlightPoint& second = flight[segment + 1];
            bool segmentIsFront = (first.depth + second.depth) * 0.5f >= 0.0f;
            if (segmentIsFront != front) {
                continue;
            }

            DrawSolidTriangle(first.outer, second.outer, second.inner, flightColor);
            DrawSolidTriangle(first.outer, second.inner, first.inner, flightColor);
            DrawLineEx(first.outer, second.outer, front ? 2.4f : 2.0f, edgeColor);
            DrawLineEx(first.inner, second.inner, 1.2f, Fade(edgeColor, 0.82f));
            if (front && first.depth > 0.42f && second.depth > 0.42f) {
                DrawLineEx(first.outer, second.outer, 1.0f, Color{202, 208, 210, 255});
            }
        }
    };

    drawFlightLayer(false);

    DrawLineEx(start, end, shaftRadius * 2.0f + 3.0f, BLACK);
    DrawLineEx(start, end, shaftRadius * 2.0f, Color{74, 80, 83, 255});
    DrawLineEx(
        {start.x - normal.x * shaftRadius * 0.30f, start.y - normal.y * shaftRadius * 0.30f},
        {end.x - normal.x * shaftRadius * 0.30f, end.y - normal.y * shaftRadius * 0.30f},
        1.2f,
        Fade(Color{184, 191, 194, 255}, 0.62f)
    );

    drawFlightLayer(true);

    Vector2 startTop{start.x - normal.x * flightRadius * 0.82f, start.y - normal.y * flightRadius * 0.82f};
    Vector2 startBottom{start.x + normal.x * flightRadius * 0.82f, start.y + normal.y * flightRadius * 0.82f};
    Vector2 endTop{end.x - normal.x * flightRadius * 0.82f, end.y - normal.y * flightRadius * 0.82f};
    Vector2 endBottom{end.x + normal.x * flightRadius * 0.82f, end.y + normal.y * flightRadius * 0.82f};
    DrawLineEx(startTop, startBottom, 4.5f, BLACK);
    DrawLineEx(startTop, startBottom, 2.2f, Color{111, 118, 121, 255});
    DrawLineEx(endTop, endBottom, 4.5f, BLACK);
    DrawLineEx(endTop, endBottom, 2.2f, Color{111, 118, 121, 255});
    DrawCircleV(start, shaftRadius + 2.0f, BLACK);
    DrawCircleV(start, shaftRadius, Color{92, 99, 102, 255});
    DrawCircleV(end, shaftRadius + 2.0f, BLACK);
    DrawCircleV(end, shaftRadius, Color{92, 99, 102, 255});
}

void DrawFan(const Fan& fan) {
    float baseAngle = atan2f(fan.direction.y, fan.direction.x);
    Vector2 normal{-fan.direction.y, fan.direction.x};
    Rectangle housing{fan.center.x - 22.0f, fan.center.y - 22.0f, 44.0f, 44.0f};

    DrawRectangleRec(housing, Color{62, 66, 70, 255});
    DrawRectangleLinesEx(housing, 2.0f, BLACK);
    DrawCircleV(fan.center, 19.0f, Color{102, 108, 112, 255});
    DrawCircleLinesV(fan.center, 19.0f, BLACK);

    for (int i = 0; i < 4; i++) {
        float angle = fan.rotation * DEG2RAD + baseAngle + static_cast<float>(i) * 90.0f * DEG2RAD;
        Vector2 tip{fan.center.x + cosf(angle) * 16.0f, fan.center.y + sinf(angle) * 16.0f};
        Vector2 side{
            fan.center.x + cosf(angle + 0.55f) * 7.0f,
            fan.center.y + sinf(angle + 0.55f) * 7.0f
        };
        DrawTriangle(fan.center, side, tip, Color{38, 42, 45, 255});
    }
    DrawCircleV(fan.center, 4.0f, BLACK);

    Vector2 nozzle{
        fan.center.x + fan.direction.x * 28.0f,
        fan.center.y + fan.direction.y * 28.0f
    };
    DrawLineEx(
        {fan.center.x + normal.x * 14.0f, fan.center.y + normal.y * 14.0f},
        {nozzle.x + normal.x * 14.0f, nozzle.y + normal.y * 14.0f},
        3.0f,
        BLACK
    );
    DrawLineEx(
        {fan.center.x - normal.x * 14.0f, fan.center.y - normal.y * 14.0f},
        {nozzle.x - normal.x * 14.0f, nozzle.y - normal.y * 14.0f},
        3.0f,
        BLACK
    );
}

void DrawPinwheel(const Pinwheel& pinwheel) {
    DrawLineEx({pinwheel.center.x, pinwheel.center.y + pinwheel.radius * 0.28f}, {pinwheel.center.x, pinwheel.center.y + pinwheel.radius * 2.0f}, 3.0f, DARKGRAY);
    for (int i = 0; i < 4; i++) {
        float angle = pinwheel.rotation * DEG2RAD + static_cast<float>(i) * 90.0f * DEG2RAD;
        Vector2 tip{
            pinwheel.center.x + cosf(angle) * pinwheel.radius,
            pinwheel.center.y + sinf(angle) * pinwheel.radius
        };
        Vector2 side{
            pinwheel.center.x + cosf(angle + 0.68f) * pinwheel.radius * 0.46f,
            pinwheel.center.y + sinf(angle + 0.68f) * pinwheel.radius * 0.46f
        };
        Color bladeColor = i % 2 == 0 ? Color{230, 232, 235, 255} : Color{185, 210, 235, 255};
        DrawTriangle(pinwheel.center, side, tip, bladeColor);
        DrawTriangle(pinwheel.center, tip, side, bladeColor);
        DrawLineEx(pinwheel.center, tip, 1.5f, BLACK);
    }
    DrawCircleV(pinwheel.center, pinwheel.radius * 0.16f, BLACK);
}

void DrawWindStreaks(const Fan& fan) {
    if (fan.power <= 0.01f || fan.strength <= 0.0f || fan.length <= 0.0f || fan.width <= 0.0f) {
        return;
    }

    Vector2 normal{-fan.direction.y, fan.direction.x};
    float speed = fmaxf(40.0f, fan.strength * fan.power);
    float time = static_cast<float>(GetTime());
    int streakCount = static_cast<int>(fmaxf(5.0f, fan.length / 34.0f));

    for (int i = 0; i < streakCount; i++) {
        float seed = static_cast<float>(i) * 37.0f;
        float along = fmodf(seed + time * speed, fan.length);
        float lane = (fmodf(seed * 1.7f, fan.width) - fan.width * 0.5f);
        Vector2 start{
            fan.center.x + fan.direction.x * along + normal.x * lane,
            fan.center.y + fan.direction.y * along + normal.y * lane
        };
        float streakLength = 14.0f + fan.power * 18.0f;
        Vector2 end{
            start.x + fan.direction.x * streakLength,
            start.y + fan.direction.y * streakLength
        };
        float fade = 1.0f - along / fan.length;
        DrawLineEx(start, end, 2.0f, Fade(RAYWHITE, 0.18f + fade * 0.45f));
    }
}

void DrawRamp(const Ramp& ramp) {
    float halfLength = ramp.length * 0.5f;
    float angle = ramp.angle * DEG2RAD;
    Vector2 axis{cosf(angle), sinf(angle)};
    Vector2 normal{-axis.y, axis.x};
    Vector2 left{
        ramp.center.x - axis.x * halfLength,
        ramp.center.y - axis.y * halfLength
    };
    Vector2 right{
        ramp.center.x + axis.x * halfLength,
        ramp.center.y + axis.y * halfLength
    };

    Vector2 p1{left.x - normal.x * ramp.thickness * 0.5f, left.y - normal.y * ramp.thickness * 0.5f};
    Vector2 p2{right.x - normal.x * ramp.thickness * 0.5f, right.y - normal.y * ramp.thickness * 0.5f};
    Vector2 p3{right.x + normal.x * ramp.thickness * 0.5f, right.y + normal.y * ramp.thickness * 0.5f};
    Vector2 p4{left.x + normal.x * ramp.thickness * 0.5f, left.y + normal.y * ramp.thickness * 0.5f};

    DrawSolidTriangle(p1, p2, p3, Color{122, 93, 58, 255});
    DrawSolidTriangle(p1, p3, p4, Color{122, 93, 58, 255});
    DrawLineEx(p1, p2, 2.0f, BLACK);
    DrawLineEx(p2, p3, 2.0f, BLACK);
    DrawLineEx(p3, p4, 2.0f, BLACK);
    DrawLineEx(p4, p1, 2.0f, BLACK);

    for (int i = 1; i < 4; i++) {
        float amount = static_cast<float>(i) / 4.0f;
        Vector2 top{
            p1.x + (p2.x - p1.x) * amount,
            p1.y + (p2.y - p1.y) * amount
        };
        Vector2 bottom{
            p4.x + (p3.x - p4.x) * amount,
            p4.y + (p3.y - p4.y) * amount
        };
        DrawLineEx(top, bottom, 1.5f, Fade(BLACK, 0.45f));
    }
}

void DrawTrapDoor(const TrapDoor& trapDoor) {
    float angle = trapDoor.angle * DEG2RAD;
    Vector2 axis{cosf(angle), sinf(angle)};
    Vector2 normal{-axis.y, axis.x};
    Vector2 hinge = trapDoor.hinge;
    Vector2 end = GetTrapDoorRingPosition(trapDoor);
    std::array<Vector2, 2> rings = GetTrapDoorRingPositions(trapDoor);
    float ringOuterRadius = trapDoor.thickness * 0.48f;
    float ringInnerRadius = trapDoor.thickness * 0.25f;
    Color wood{116, 77, 42, 255};
    Color darkWood{73, 46, 28, 255};
    Color iron{82, 89, 92, 255};
    Color ironHighlight{148, 156, 158, 255};

    // The frame and latch stay in the floor while the hatch panel rotates away.
    Vector2 closedFrameStart{hinge.x - 8.0f, hinge.y + trapDoor.thickness * 0.68f};
    Vector2 closedFrameEnd{hinge.x + trapDoor.length + 8.0f, closedFrameStart.y};
    DrawRectangleRec(
        {
            hinge.x + 3.0f,
            hinge.y + trapDoor.thickness * 0.44f,
            fmaxf(1.0f, trapDoor.length - 6.0f),
            trapDoor.thickness * 0.58f
        },
        Color{22, 25, 27, 255}
    );
    DrawLineEx(closedFrameStart, closedFrameEnd, 8.0f, BLACK);
    DrawLineEx(closedFrameStart, closedFrameEnd, 4.0f, iron);
    Rectangle hingeSocket{
        hinge.x - 13.0f,
        hinge.y + trapDoor.thickness * 0.48f,
        22.0f,
        trapDoor.thickness * 0.72f
    };
    Rectangle latchSocket{
        hinge.x + trapDoor.length - 6.0f,
        hinge.y + trapDoor.thickness * 0.48f,
        18.0f,
        trapDoor.thickness * 0.72f
    };
    DrawRectangleRec(hingeSocket, iron);
    DrawRectangleLinesEx(hingeSocket, 2.0f, BLACK);
    DrawRectangleRec(latchSocket, Color{55, 61, 64, 255});
    DrawRectangleLinesEx(latchSocket, 2.0f, BLACK);
    DrawRectangleRec(
        {latchSocket.x + 4.0f, latchSocket.y + 2.0f, latchSocket.width - 8.0f, 3.0f},
        BLACK
    );

    auto drawRing = [&](Vector2 ring, Vector2 face) {
        DrawLineEx(face, ring, trapDoor.thickness * 0.28f, BLACK);
        DrawLineEx(face, ring, trapDoor.thickness * 0.14f, ironHighlight);
        DrawRing(ring, ringInnerRadius, ringOuterRadius, 0.0f, 360.0f, 24, iron);
        DrawCircleLinesV(ring, ringOuterRadius, BLACK);
        DrawCircleLinesV(ring, ringInnerRadius, BLACK);
    };

    Vector2 rearFace{
        rings[0].x + normal.x * trapDoor.thickness * 0.12f,
        rings[0].y + normal.y * trapDoor.thickness * 0.12f
    };
    drawRing(rings[0], rearFace);

    Vector2 p1{hinge.x - normal.x * trapDoor.thickness * 0.5f, hinge.y - normal.y * trapDoor.thickness * 0.5f};
    Vector2 p2{end.x - normal.x * trapDoor.thickness * 0.5f, end.y - normal.y * trapDoor.thickness * 0.5f};
    Vector2 p3{end.x + normal.x * trapDoor.thickness * 0.5f, end.y + normal.y * trapDoor.thickness * 0.5f};
    Vector2 p4{hinge.x + normal.x * trapDoor.thickness * 0.5f, hinge.y + normal.y * trapDoor.thickness * 0.5f};

    DrawSolidTriangle(p1, p2, p3, darkWood);
    DrawSolidTriangle(p1, p3, p4, darkWood);

    float faceInset = trapDoor.thickness * 0.18f;
    Vector2 f1{p1.x + normal.x * faceInset, p1.y + normal.y * faceInset};
    Vector2 f2{p2.x + normal.x * faceInset, p2.y + normal.y * faceInset};
    Vector2 f3{p3.x - normal.x * faceInset, p3.y - normal.y * faceInset};
    Vector2 f4{p4.x - normal.x * faceInset, p4.y - normal.y * faceInset};
    DrawSolidTriangle(f1, f2, f3, wood);
    DrawSolidTriangle(f1, f3, f4, wood);

    for (int i = 1; i < 4; i++) {
        float amount = static_cast<float>(i) / 4.0f;
        Vector2 top{
            f1.x + (f2.x - f1.x) * amount,
            f1.y + (f2.y - f1.y) * amount
        };
        Vector2 bottom{
            f4.x + (f3.x - f4.x) * amount,
            f4.y + (f3.y - f4.y) * amount
        };
        DrawLineEx(top, bottom, 1.5f, Fade(BLACK, 0.58f));
    }

    Vector2 braceStart{
        hinge.x + axis.x * trapDoor.length * 0.18f + normal.x * trapDoor.thickness * 0.28f,
        hinge.y + axis.y * trapDoor.length * 0.18f + normal.y * trapDoor.thickness * 0.28f
    };
    Vector2 braceEnd{
        hinge.x + axis.x * trapDoor.length * 0.78f - normal.x * trapDoor.thickness * 0.28f,
        hinge.y + axis.y * trapDoor.length * 0.78f - normal.y * trapDoor.thickness * 0.28f
    };
    DrawLineEx(braceStart, braceEnd, 6.0f, BLACK);
    DrawLineEx(braceStart, braceEnd, 3.0f, iron);

    DrawLineEx(p1, p2, 3.5f, BLACK);
    DrawLineEx(p1, p2, 1.5f, ironHighlight);
    DrawLineEx(p3, p4, 4.0f, BLACK);
    DrawLineEx(p3, p4, 2.0f, iron);
    DrawLineEx(p4, p1, 6.0f, BLACK);
    DrawLineEx(p4, p1, 3.0f, iron);
    DrawLineEx(p2, p3, 7.0f, BLACK);
    DrawLineEx(p2, p3, 4.0f, ironHighlight);

    Vector2 hingeLeafStart{
        hinge.x + axis.x * trapDoor.length * 0.05f,
        hinge.y + axis.y * trapDoor.length * 0.05f
    };
    Vector2 hingeLeafEnd{
        hinge.x + axis.x * trapDoor.length * 0.27f,
        hinge.y + axis.y * trapDoor.length * 0.27f
    };
    DrawLineEx(hingeLeafStart, hingeLeafEnd, trapDoor.thickness * 0.46f + 2.0f, BLACK);
    DrawLineEx(hingeLeafStart, hingeLeafEnd, trapDoor.thickness * 0.46f, iron);
    for (float amount : {0.10f, 0.22f}) {
        Vector2 bolt{
            hinge.x + axis.x * trapDoor.length * amount,
            hinge.y + axis.y * trapDoor.length * amount
        };
        DrawCircleV(bolt, 2.6f, BLACK);
        DrawCircleV(bolt, 1.3f, ironHighlight);
    }

    DrawCircleV(hinge, trapDoor.thickness * 0.72f, BLACK);
    DrawCircleV(hinge, trapDoor.thickness * 0.57f, ironHighlight);
    DrawCircleV(hinge, trapDoor.thickness * 0.24f, Color{31, 35, 37, 255});
    DrawLineEx(
        {hinge.x - normal.x * trapDoor.thickness * 0.28f, hinge.y - normal.y * trapDoor.thickness * 0.28f},
        {hinge.x + normal.x * trapDoor.thickness * 0.28f, hinge.y + normal.y * trapDoor.thickness * 0.28f},
        2.0f,
        BLACK
    );

    Vector2 frontFace{
        rings[1].x - normal.x * trapDoor.thickness * 0.12f,
        rings[1].y - normal.y * trapDoor.thickness * 0.12f
    };
    drawRing(rings[1], frontFace);
}

void DrawButton(const Button& button) {
    float pressOffset = button.pressed ? button.rect.height * 0.34f : 0.0f;
    Rectangle base{
        button.rect.x,
        button.rect.y + button.rect.height * 0.48f,
        button.rect.width,
        button.rect.height * 0.52f
    };
    Rectangle plate{
        button.rect.x + 3.0f,
        button.rect.y + pressOffset,
        button.rect.width - 6.0f,
        button.rect.height * 0.52f
    };

    DrawRectangleRec(base, DARKGRAY);
    DrawRectangleLinesEx(base, 2.0f, BLACK);
    DrawRectangleRec(plate, button.pressed ? Color{160, 42, 35, 255} : Color{210, 61, 49, 255});
    DrawRectangleLinesEx(plate, 2.0f, BLACK);
}

void DrawArrowTrap(const ArrowTrap& trap) {
    Rectangle housing{trap.position.x - 16.0f, trap.position.y - 16.0f, 32.0f, 32.0f};
    DrawRectangleRec(housing, Color{70, 74, 76, 255});
    DrawRectangleLinesEx(housing, 2.0f, BLACK);

    Vector2 muzzle{
        trap.position.x + trap.direction.x * 17.0f,
        trap.position.y + trap.direction.y * 17.0f
    };
    DrawCircleV(muzzle, 5.0f, BLACK);
    DrawLineEx(
        trap.position,
        {trap.position.x + trap.direction.x * 20.0f, trap.position.y + trap.direction.y * 20.0f},
        5.0f,
        DARKGRAY
    );
}

void DrawArrowProjectile(const ArrowProjectile& arrow) {
    Vector2 center{
        arrow.rect.x + arrow.rect.width * 0.5f,
        arrow.rect.y + arrow.rect.height * 0.5f
    };
    bool horizontal = fabsf(arrow.velocity.x) >= fabsf(arrow.velocity.y);
    float direction = horizontal ? (arrow.velocity.x < 0.0f ? -1.0f : 1.0f) : (arrow.velocity.y < 0.0f ? -1.0f : 1.0f);

    if (horizontal) {
        Vector2 tip{center.x + direction * arrow.rect.width * 0.5f, center.y};
        Vector2 tail{center.x - direction * arrow.rect.width * 0.5f, center.y};
        DrawLineEx(tail, tip, 3.0f, BROWN);
        DrawTriangle(tip, {tip.x - direction * 9.0f, tip.y - 5.0f}, {tip.x - direction * 9.0f, tip.y + 5.0f}, BLACK);
    }
    else {
        Vector2 tip{center.x, center.y + direction * arrow.rect.height * 0.5f};
        Vector2 tail{center.x, center.y - direction * arrow.rect.height * 0.5f};
        DrawLineEx(tail, tip, 3.0f, BROWN);
        DrawTriangle(tip, {tip.x - 5.0f, tip.y - direction * 9.0f}, {tip.x + 5.0f, tip.y - direction * 9.0f}, BLACK);
    }
}

void DrawBreakableTile(Texture2D texture, const BreakableTile& tile) {
    if (!tile.broken) {
        DrawTilesetSolid(texture, tile.rect, WHITE);
        DrawRectangleLinesEx(tile.rect, 2.0f, BLACK);

        if (tile.cracking) {
            float progress = tile.breakDelay > 0.0f ? std::clamp(tile.crackTimer / tile.breakDelay, 0.0f, 1.0f) : 1.0f;
            int stage = progress < 0.34f ? 1 : (progress < 0.68f ? 2 : 3);
            Color crackColor = Fade(BLACK, 0.55f + progress * 0.35f);
            float left = tile.rect.x;
            float top = tile.rect.y;
            float right = tile.rect.x + tile.rect.width;
            float bottom = tile.rect.y + tile.rect.height;
            float midX = tile.rect.x + tile.rect.width * 0.5f;
            float midY = tile.rect.y + tile.rect.height * 0.5f;

            DrawLineEx({midX - 6.0f, top + 3.0f}, {midX + 2.0f, midY - 2.0f}, 2.0f, crackColor);
            DrawLineEx({midX + 2.0f, midY - 2.0f}, {midX - 3.0f, bottom - 4.0f}, 2.0f, crackColor);
            if (stage >= 2) {
                DrawLineEx({midX + 2.0f, midY - 2.0f}, {right - 6.0f, top + 8.0f}, 2.0f, crackColor);
                DrawLineEx({midX - 1.0f, midY + 4.0f}, {left + 7.0f, bottom - 7.0f}, 2.0f, crackColor);
            }
            if (stage >= 3) {
                DrawLineEx({left + 8.0f, top + 7.0f}, {midX - 5.0f, midY + 1.0f}, 2.0f, crackColor);
                DrawLineEx({midX + 6.0f, midY + 5.0f}, {right - 8.0f, bottom - 5.0f}, 2.0f, crackColor);
                DrawRectangleRec(tile.rect, Fade(BLACK, progress * 0.14f));
            }
        }
    }

    for (const BreakableDebris& debris : tile.debris) {
        float alpha = debris.maxLife > 0.0f ? std::clamp(debris.life / debris.maxLife, 0.0f, 1.0f) : 0.0f;
        DrawRectangleRec(debris.rect, Fade(Color{92, 94, 92, 255}, alpha));
        DrawRectangleLinesEx(debris.rect, 1.0f, Fade(BLACK, alpha));
    }
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
    auto drawEndpoint = [&](Vector2 position, bool pinned, const FlexibleEndpointBinding& binding) {
        bool attached = binding.anchorType != FlexibleAnchorType::None;
        bool carried = binding.carriedByPlayer >= 0;
        Color metal = carried ? Color{199, 157, 67, 255} :
            (attached ? Color{151, 160, 163, 255} : (pinned ? DARKGRAY : Color{84, 88, 90, 255}));
        float outerRadius = fmaxf(4.0f, chain.collisionRadius * chain.scale * 0.72f);
        DrawCircleV(position, outerRadius + 1.5f, BLACK);
        DrawCircleV(position, outerRadius, metal);
        DrawCircleV(position, outerRadius * 0.42f, BLACK);
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
        drawEndpoint(chain.points.front(), chain.pinStart, chain.startBinding);
        drawEndpoint(chain.points.back(), chain.pinEnd, chain.endBinding);
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
        drawEndpoint(chain.start, chain.pinStart, chain.startBinding);
        drawEndpoint(chain.end, chain.pinEnd, chain.endBinding);
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
    drawEndpoint(chain.start, chain.pinStart, chain.startBinding);
    drawEndpoint(chain.end, chain.pinEnd, chain.endBinding);
}

void DrawPhysicsRope(const PhysicsRope& rope) {
    if (rope.points.size() < 2) {
        DrawRope(rope.start, rope.end, rope.thickness);
        return;
    }

    float thickness = fmaxf(1.0f, rope.thickness);
    Color outline = Color{43, 31, 21, 255};
    Color fiber = Color{137, 92, 50, 255};
    for (int i = 0; i < static_cast<int>(rope.points.size()) - 1; i++) {
        DrawLineEx(rope.points[i], rope.points[i + 1], thickness + 2.0f, outline);
    }
    for (int i = 0; i < static_cast<int>(rope.points.size()) - 1; i++) {
        DrawLineEx(rope.points[i], rope.points[i + 1], thickness, fiber);
        if (i > 0) {
            DrawCircleV(rope.points[i], thickness * 0.5f, fiber);
        }
    }

    float traveled = 0.0f;
    float nextMark = fmaxf(5.0f, thickness * 1.4f);
    float markSpacing = fmaxf(10.0f, thickness * 2.4f);
    for (int i = 0; i < static_cast<int>(rope.points.size()) - 1; i++) {
        Vector2 start = rope.points[i];
        Vector2 end = rope.points[i + 1];
        Vector2 delta{end.x - start.x, end.y - start.y};
        float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.0001f) {
            continue;
        }

        Vector2 direction{delta.x / length, delta.y / length};
        Vector2 normal{-direction.y, direction.x};
        while (nextMark <= traveled + length) {
            float amount = (nextMark - traveled) / length;
            Vector2 center{
                start.x + (end.x - start.x) * amount,
                start.y + (end.y - start.y) * amount
            };
            float along = fmaxf(1.5f, thickness * 0.42f);
            float across = thickness * 0.42f;
            DrawLineEx(
                {
                    center.x - direction.x * along + normal.x * across,
                    center.y - direction.y * along + normal.y * across
                },
                {
                    center.x + direction.x * along - normal.x * across,
                    center.y + direction.y * along - normal.y * across
                },
                1.0f,
                Fade(BLACK, 0.75f)
            );
            nextMark += markSpacing;
        }
        traveled += length;
    }

    auto drawEndpoint = [&](Vector2 position, bool pinned, const FlexibleEndpointBinding& binding) {
        bool attached = binding.anchorType != FlexibleAnchorType::None;
        bool carried = binding.carriedByPlayer >= 0;
        Color ferrule = carried ? Color{199, 157, 67, 255} :
            (attached ? Color{151, 160, 163, 255} : (pinned ? Color{100, 105, 106, 255} : fiber));
        DrawCircleV(position, thickness * 1.08f + 1.0f, outline);
        DrawCircleV(position, thickness * 1.08f, ferrule);
        DrawCircleV(position, thickness * 0.42f, outline);
    };
    drawEndpoint(rope.points.front(), rope.pinStart, rope.startBinding);
    drawEndpoint(rope.points.back(), rope.pinEnd, rope.endBinding);
}
