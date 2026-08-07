#include "Render.h"

#include "Fluid.h"
#include "Machine.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace {
    constexpr float TilesetTileSize = 32.0f;

    Vector2 NormalizeOr(Vector2 value, Vector2 fallback = {1.0f, 0.0f}) {
        float length = sqrtf(value.x * value.x + value.y * value.y);
        return length > 0.0001f ? Vector2{value.x / length, value.y / length} : fallback;
    }

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

    void DrawGasVolume(const FluidField& fluid) {
        if (fluid.cells.empty()) return;

        const Color gasShadow{32, 92, 23, 255};
        const Color gasBody{91, 194, 52, 255};
        const Color gasHighlight{185, 235, 83, 255};
        const float time = static_cast<float>(GetTime());

        auto cellDensity = [&](int column, int row) {
            if (column < 0 || column >= fluid.gridColumns || row < 0 || row >= fluid.gridRows) {
                return -1.0f;
            }
            const FluidCell& cell = fluid.cells[row * fluid.gridColumns + column];
            if (!cell.solid) return std::clamp(cell.mass, 0.0f, 1.0f);

            // The collision grid conservatively marks a whole tile solid when a
            // wall clips any part of it. Extrapolating the neighboring density
            // into that tile lets the wall artwork mask the gas at its exact face
            // instead of leaving a cell-sized empty border.
            float adjacentDensity = 0.0f;
            const int adjacent[4][2]{
                {column - 1, row}, {column + 1, row},
                {column, row - 1}, {column, row + 1}
            };
            for (const auto& candidate : adjacent) {
                if (candidate[0] < 0 || candidate[0] >= fluid.gridColumns ||
                    candidate[1] < 0 || candidate[1] >= fluid.gridRows) {
                    continue;
                }
                const FluidCell& neighbor = fluid.cells[candidate[1] * fluid.gridColumns + candidate[0]];
                if (!neighbor.solid) adjacentDensity = fmaxf(adjacentDensity, neighbor.mass);
            }
            return std::clamp(adjacentDensity, 0.0f, 1.0f);
        };

        auto vertexDensity = [&](int vertexColumn, int vertexRow) {
            float density = 0.0f;
            int sampleCount = 0;
            for (int rowOffset = -1; rowOffset <= 0; rowOffset++) {
                for (int columnOffset = -1; columnOffset <= 0; columnOffset++) {
                    const float sample = cellDensity(vertexColumn + columnOffset, vertexRow + rowOffset);
                    if (sample < 0.0f) continue;
                    density += sample;
                    sampleCount++;
                }
            }
            return sampleCount > 0 ? density / static_cast<float>(sampleCount) : 0.0f;
        };

        auto vertexColor = [&](float density, int column, int row) {
            density = std::clamp(density, 0.0f, 1.0f);
            if (density <= 0.002f) return Color{gasBody.r, gasBody.g, gasBody.b, 0};
            const float noise = 0.5f + sinf(
                time * 0.52f + static_cast<float>(column) * 0.71f + static_cast<float>(row) * 0.49f
            ) * 0.5f;
            Color color = ColorLerp(gasBody, gasShadow, density * 0.43f);
            color = ColorLerp(color, gasHighlight, 0.04f + noise * 0.10f);
            color.a = static_cast<unsigned char>(std::clamp(
                powf(density, 0.78f) * (132.0f + noise * 16.0f),
                0.0f,
                188.0f
            ));
            return color;
        };

        auto drawVertex = [](float x, float y, Color color) {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(x, y);
        };

        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        for (int row = 0; row < fluid.gridRows; row++) {
            const float top = fluid.bounds.y + static_cast<float>(row) * fluid.cellSize;
            const float bottom = fminf(top + fluid.cellSize, fluid.bounds.y + fluid.bounds.height);
            for (int column = 0; column < fluid.gridColumns; column++) {
                const float left = fluid.bounds.x + static_cast<float>(column) * fluid.cellSize;
                const float right = fminf(left + fluid.cellSize, fluid.bounds.x + fluid.bounds.width);
                const float topLeftDensity = vertexDensity(column, row);
                const float topRightDensity = vertexDensity(column + 1, row);
                const float bottomLeftDensity = vertexDensity(column, row + 1);
                const float bottomRightDensity = vertexDensity(column + 1, row + 1);
                if (topLeftDensity <= 0.002f && topRightDensity <= 0.002f &&
                    bottomLeftDensity <= 0.002f && bottomRightDensity <= 0.002f) {
                    continue;
                }

                const Color topLeft = vertexColor(topLeftDensity, column, row);
                const Color topRight = vertexColor(topRightDensity, column + 1, row);
                const Color bottomLeft = vertexColor(bottomLeftDensity, column, row + 1);
                const Color bottomRight = vertexColor(bottomRightDensity, column + 1, row + 1);
                drawVertex(left, top, topLeft);
                drawVertex(left, bottom, bottomLeft);
                drawVertex(right, bottom, bottomRight);
                drawVertex(left, top, topLeft);
                drawVertex(right, bottom, bottomRight);
                drawVertex(right, top, topRight);
            }
        }
        rlEnd();
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

void DrawSpikes(Rectangle hazard) {
    if (hazard.width <= 0.0f || hazard.height <= 0.0f) {
        return;
    }

    const Color outline{38, 43, 45, 255};
    const Color cone{137, 144, 144, 255};
    const Color coneLight{204, 209, 204, 255};
    float left = hazard.x + 2.0f;
    float right = hazard.x + hazard.width - 2.0f;
    float baseY = hazard.y + hazard.height;
    float usableWidth = fmaxf(1.0f, right - left);
    int count = std::max(1, static_cast<int>(roundf(usableWidth / 16.0f)));
    float spacing = usableWidth / static_cast<float>(count);
    float spikeWidth = fmaxf(2.0f, fminf(14.0f, spacing - 1.5f));
    float spikeHeight = fmaxf(8.0f, hazard.height);

    for (int i = 0; i < count; i++) {
        float centerX = left + (static_cast<float>(i) + 0.5f) * spacing;
        float halfWidth = spikeWidth * 0.5f;
        Vector2 tip{centerX, baseY - spikeHeight};
        Vector2 leftBase{centerX - halfWidth, baseY};
        Vector2 rightBase{centerX + halfWidth, baseY};

        DrawSolidTriangle(leftBase, tip, rightBase, outline);
        DrawSolidTriangle(
            {leftBase.x + 1.6f, leftBase.y - 1.0f},
            {tip.x, tip.y + 2.0f},
            {rightBase.x - 1.6f, rightBase.y - 1.0f},
            cone
        );
        DrawLineEx(
            {centerX - halfWidth * 0.24f, baseY - 3.0f},
            {centerX - 0.8f, tip.y + 5.0f},
            1.1f,
            Fade(coneLight, 0.68f)
        );
    }
}

void DrawDirectionalSpikes(const DirectionalSpikeHazard& hazard) {
    Rectangle rect = hazard.rect;
    if (rect.width <= 0.0f || rect.height <= 0.0f) return;

    const Color outline{38, 43, 45, 255};
    const Color cone{137, 144, 144, 255};
    const bool horizontal = hazard.direction == SpikeDirection::Up ||
        hazard.direction == SpikeDirection::Down;
    const float span = horizontal ? rect.width : rect.height;
    const int count = std::max(1, static_cast<int>(roundf(span / 16.0f)));
    const float spacing = span / static_cast<float>(count);

    for (int index = 0; index < count; ++index) {
        const float offset = (static_cast<float>(index) + 0.5f) * spacing;
        Vector2 tip{};
        Vector2 baseA{};
        Vector2 baseB{};
        switch (hazard.direction) {
        case SpikeDirection::Down:
            tip = {rect.x + offset, rect.y + rect.height};
            baseA = {rect.x + offset - spacing * 0.42f, rect.y};
            baseB = {rect.x + offset + spacing * 0.42f, rect.y};
            break;
        case SpikeDirection::Left:
            tip = {rect.x, rect.y + offset};
            baseA = {rect.x + rect.width, rect.y + offset - spacing * 0.42f};
            baseB = {rect.x + rect.width, rect.y + offset + spacing * 0.42f};
            break;
        case SpikeDirection::Right:
            tip = {rect.x + rect.width, rect.y + offset};
            baseA = {rect.x, rect.y + offset - spacing * 0.42f};
            baseB = {rect.x, rect.y + offset + spacing * 0.42f};
            break;
        case SpikeDirection::Up:
        default:
            tip = {rect.x + offset, rect.y};
            baseA = {rect.x + offset - spacing * 0.42f, rect.y + rect.height};
            baseB = {rect.x + offset + spacing * 0.42f, rect.y + rect.height};
            break;
        }
        DrawSolidTriangle(baseA, tip, baseB, outline);
        const Vector2 insetA{
            baseA.x + (tip.x - baseA.x) * 0.10f,
            baseA.y + (tip.y - baseA.y) * 0.10f
        };
        const Vector2 insetB{
            baseB.x + (tip.x - baseB.x) * 0.10f,
            baseB.y + (tip.y - baseB.y) * 0.10f
        };
        const Vector2 insetTip{
            tip.x + ((baseA.x + baseB.x) * 0.5f - tip.x) * 0.12f,
            tip.y + ((baseA.y + baseB.y) * 0.5f - tip.y) * 0.12f
        };
        DrawSolidTriangle(insetA, insetTip, insetB, cone);
    }
}

void DrawPortal(Rectangle portal, Color color) {
    if (portal.width <= 0.0f || portal.height <= 0.0f) return;
    const Vector2 center{
        portal.x + portal.width * 0.5f,
        portal.y + portal.height * 0.5f
    };
    const float radiusX = portal.width * 0.5f;
    const float radiusY = portal.height * 0.5f;
    const float time = static_cast<float>(GetTime());
    const float pulse = 0.5f + 0.5f * sinf(time * 2.4f);
    const bool bluePortal = color.b > color.r;
    const Color outerColor = bluePortal
        ? Color{0, 112, 255, 255}
        : Color{255, 76, 0, 255};
    const Color middleColor = bluePortal
        ? Color{0, 205, 255, 255}
        : Color{255, 184, 0, 255};
    const Color coreColor = bluePortal
        ? Color{42, 132, 255, 255}
        : Color{255, 112, 20, 255};
    const Color highlightColor = bluePortal
        ? Color{145, 248, 255, 255}
        : Color{255, 244, 92, 255};

    const auto ellipsePoint = [&](float angle, float scaleX, float scaleY) {
        return Vector2{
            center.x + cosf(angle) * radiusX * scaleX,
            center.y + sinf(angle) * radiusY * scaleY
        };
    };
    const auto drawEllipseBand = [&](float scaleX, float scaleY, float phase,
        float thickness, Color tint, int segments, float coverage) {
        Vector2 previous = ellipsePoint(phase, scaleX, scaleY);
        for (int segment = 1; segment <= segments; ++segment) {
            const float angle = phase + coverage * static_cast<float>(segment) /
                static_cast<float>(segments);
            const Vector2 current = ellipsePoint(angle, scaleX, scaleY);
            DrawLineEx(previous, current, thickness, tint);
            previous = current;
        }
    };

    // Wide translucent halos make the portal read as emitted light rather than
    // a flat outline painted onto the level.
    for (int layer = 4; layer >= 1; --layer) {
        const float expansion = static_cast<float>(layer) * 0.055f;
        DrawEllipse(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            radiusX * (1.0f + expansion),
            radiusY * (1.0f + expansion),
            Fade(outerColor, 0.045f + (4 - layer) * 0.022f)
        );
    }

    // Opaque, saturated layers prevent the dark level background from showing
    // through and keep the entire opening bright.
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y),
        radiusX, radiusY, outerColor);
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y),
        radiusX * 0.88f, radiusY * 0.72f, middleColor);
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y + radiusY * 0.10f),
        radiusX * 0.67f, radiusY * 0.42f,
        Fade(coreColor, 0.82f + pulse * 0.18f));

    // Counter-rotating energy ribbons give the opening a flowing interior.
    drawEllipseBand(0.88f, 0.72f, time * 1.35f, 2.2f,
        Fade(highlightColor, 0.94f), 18, PI * 1.22f);
    drawEllipseBand(0.72f, 0.52f, -time * 1.70f + 1.1f, 1.5f,
        Fade(middleColor, 0.96f), 14, PI * 0.92f);
    drawEllipseBand(1.0f, 1.0f, 0.0f, 3.2f,
        outerColor, 40, PI * 2.0f);
    drawEllipseBand(0.94f, 0.82f, 0.0f, 1.2f,
        highlightColor, 40, PI * 2.0f);

    // A few slow sparks orbit the rim without rapid flashing.
    for (int spark = 0; spark < 7; ++spark) {
        const float angle = time * (0.55f + spark * 0.035f) +
            static_cast<float>(spark) * PI * 2.0f / 7.0f;
        const Vector2 position = ellipsePoint(angle, 1.08f, 1.18f);
        const float sparkRadius = 1.2f + 0.8f *
            (0.5f + 0.5f * sinf(time * 1.8f + spark));
        DrawCircleV(position, sparkRadius + 1.2f, Fade(outerColor, 0.34f));
        DrawCircleV(position, sparkRadius, highlightColor);
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

    const Color skyTop{74, 151, 202, 255};
    const Color skyBottom{176, 211, 210, 255};
    DrawRectangleGradientV(
        static_cast<int>(doorway.x),
        static_cast<int>(doorway.y),
        static_cast<int>(ceilf(doorway.width)),
        static_cast<int>(ceilf(doorway.height)),
        skyTop,
        skyBottom
    );

    Vector2 sun{doorway.x + doorway.width - 17.0f, doorway.y + 33.0f};
    DrawCircleV(sun, 30.0f, Fade(YELLOW, 0.18f));
    DrawCircleV(sun, 18.0f, Color{246, 213, 94, 255});

    for (int i = 0; i < 8; i++) {
        float angle = i * 45.0f;
        Vector2 start{
            sun.x + cosf(angle * DEG2RAD) * 23.0f,
            sun.y + sinf(angle * DEG2RAD) * 23.0f
        };
        Vector2 end{
            sun.x + cosf(angle * DEG2RAD) * 29.0f,
            sun.y + sinf(angle * DEG2RAD) * 29.0f
        };
        DrawLineEx(start, end, 1.5f, Fade(YELLOW, 0.78f));
    }

    float horizonY = doorway.y + doorway.height * 0.61f;
    DrawSolidTriangle({doorway.x - 20.0f, horizonY + 22.0f},
        {doorway.x + doorway.width * 0.34f, horizonY - 35.0f},
        {doorway.x + doorway.width * 0.72f, horizonY + 22.0f}, Color{72, 115, 120, 255});
    DrawSolidTriangle({doorway.x + doorway.width * 0.25f, horizonY + 24.0f},
        {doorway.x + doorway.width * 0.78f, horizonY - 20.0f},
        {doorway.x + doorway.width + 24.0f, horizonY + 24.0f}, Color{58, 97, 105, 255});
    DrawRectangleRec({doorway.x, horizonY, doorway.width, doorway.y + doorway.height - horizonY},
        Color{67, 126, 69, 255});
    DrawCircleV({doorway.x + 13.0f, horizonY + 3.0f}, 18.0f, Color{74, 143, 73, 255});
    DrawCircleV({doorway.x + doorway.width * 0.70f, horizonY + 7.0f}, 22.0f, Color{54, 116, 63, 255});
    DrawLineEx({doorway.x, doorway.y + doorway.height - 18.0f},
        {doorway.x + doorway.width, doorway.y + doorway.height - 7.0f}, 20.0f, Color{83, 79, 68, 255});

    EndScissorMode();
}

void DrawExitDoor(Rectangle trigger, float gateBottom) {
    if (trigger.width <= 0.0f || trigger.height <= 0.0f) {
        return;
    }

    const Color outline{22, 27, 30, 255};
    const Color ironDark{48, 56, 60, 255};
    const Color iron{82, 92, 95, 255};
    const Color ironLight{142, 150, 149, 255};
    const Color brass{181, 132, 43, 255};
    const float inset = 5.0f;
    Rectangle opening{trigger.x + inset, trigger.y, trigger.width - inset * 2.0f, trigger.height};
    float closedBottom = trigger.y + trigger.height;
    float clampedBottom = std::clamp(gateBottom, trigger.y, closedBottom);
    float openAmount = trigger.height > 0.0f ?
        std::clamp((closedBottom - clampedBottom) / trigger.height, 0.0f, 1.0f) : 0.0f;

    DrawRectangleRec({trigger.x - 4.0f, trigger.y - 8.0f, trigger.width + 8.0f, trigger.height + 8.0f}, outline);
    DrawRectangleRec(trigger, ironDark);
    DrawOutdoorDoorway(opening);

    float visibleGateHeight = clampedBottom - trigger.y;
    if (visibleGateHeight > 0.0f) {
        BeginScissorMode(static_cast<int>(opening.x), static_cast<int>(opening.y),
            static_cast<int>(opening.width), static_cast<int>(opening.height));
        float shutterTop = clampedBottom - trigger.height;
        Rectangle shutter{opening.x, shutterTop, opening.width, trigger.height};
        DrawRectangleRec(shutter, Color{48, 51, 50, 250});

        int slatCount = std::max(3, static_cast<int>(roundf(opening.width / 12.0f)));
        float slatWidth = opening.width / static_cast<float>(slatCount);
        for (int i = 0; i < slatCount; i++) {
            float x = opening.x + static_cast<float>(i) * slatWidth;
            DrawRectangleRec({x + 1.0f, shutterTop, slatWidth - 2.0f, trigger.height},
                (i & 1) == 0 ? iron : ironDark);
            DrawLineEx({x + 2.0f, shutterTop}, {x + 2.0f, clampedBottom}, 1.0f, Fade(ironLight, 0.55f));
        }

        for (float y = shutterTop + 31.0f; y < clampedBottom - 11.0f; y += 38.0f) {
            DrawRectangleRec({opening.x, y, opening.width, 7.0f}, outline);
            DrawLineEx({opening.x + 2.0f, y + 1.5f}, {opening.x + opening.width - 2.0f, y + 1.5f},
                1.0f, ironLight);
        }

        Rectangle bottomBeam{opening.x, clampedBottom - 13.0f, opening.width, 13.0f};
        DrawRectangleRec(bottomBeam, outline);
        DrawRectangleRec({bottomBeam.x + 2.0f, bottomBeam.y + 2.0f, bottomBeam.width - 4.0f, 7.0f}, brass);
        for (float x = opening.x + 8.0f; x < opening.x + opening.width; x += 18.0f) {
            DrawCircleV({x, clampedBottom - 6.5f}, 2.0f, outline);
            DrawCircleV({x - 0.5f, clampedBottom - 7.0f}, 0.8f, ironLight);
        }
        EndScissorMode();
    }

    // Side tracks and lintel remain in front of the moving shutter.
    Rectangle leftTrack{trigger.x - 1.0f, trigger.y - 2.0f, 9.0f, trigger.height + 2.0f};
    Rectangle rightTrack{trigger.x + trigger.width - 8.0f, trigger.y - 2.0f, 9.0f, trigger.height + 2.0f};
    for (Rectangle track : {leftTrack, rightTrack}) {
        DrawRectangleRec(track, outline);
        DrawRectangleRec({track.x + 2.0f, track.y + 2.0f, track.width - 4.0f, track.height - 2.0f}, iron);
        DrawLineEx({track.x + 3.0f, track.y + 3.0f},
            {track.x + 3.0f, track.y + track.height - 2.0f}, 1.0f, ironLight);
    }

    Rectangle lintel{trigger.x - 4.0f, trigger.y - 18.0f, trigger.width + 8.0f, 28.0f};
    DrawRectangleRec(lintel, outline);
    DrawRectangleRec({lintel.x + 3.0f, lintel.y + 3.0f, lintel.width - 6.0f, lintel.height - 6.0f}, iron);
    DrawLineEx({lintel.x + 5.0f, lintel.y + 5.0f}, {lintel.x + lintel.width - 5.0f, lintel.y + 5.0f},
        1.5f, ironLight);

    const char* exitLabel = "EXIT";
    Font labelFont = GetFontDefault();
    float labelScale = trigger.width < 70.0f ? 1.0f : 2.0f;
    float labelSize = static_cast<float>(labelFont.baseSize) * labelScale;
    float labelSpacing = labelScale;
    Rectangle labelPlate{
        lintel.x + 5.0f,
        lintel.y + 4.0f,
        lintel.width - 27.0f,
        lintel.height - 8.0f
    };
    DrawRectangleRec(labelPlate, Color{29, 35, 36, 255});
    DrawRectangleLinesEx(labelPlate, 1.0f, Fade(ironLight, 0.75f));
    Vector2 labelMeasure = MeasureTextEx(labelFont, exitLabel, labelSize, labelSpacing);
    DrawTextEx(
        labelFont,
        exitLabel,
        {
            floorf(labelPlate.x + (labelPlate.width - labelMeasure.x) * 0.5f),
            floorf(labelPlate.y + (labelPlate.height - labelMeasure.y) * 0.5f)
        },
        labelSize,
        labelSpacing,
        Color{232, 238, 218, 255}
    );

    Color lampColor = openAmount >= 0.98f ? Color{77, 215, 112, 255} :
        (openAmount > 0.02f ? ORANGE : Color{173, 48, 38, 255});
    Vector2 lampCenter{lintel.x + lintel.width - 10.0f, lintel.y + lintel.height * 0.5f};
    DrawCircleV(lampCenter, 5.0f, outline);
    DrawCircleV(lampCenter, 3.0f, lampColor);

    for (Vector2 bolt : std::initializer_list<Vector2>{
        {lintel.x + 6.0f, lintel.y + 6.0f},
        {lintel.x + 6.0f, lintel.y + lintel.height - 6.0f},
        {lintel.x + lintel.width - 6.0f, lintel.y + 6.0f},
        {lintel.x + lintel.width - 6.0f, lintel.y + lintel.height - 6.0f}
    }) {
        DrawCircleV(bolt, 1.7f, outline);
        DrawCircleV({bolt.x - 0.4f, bolt.y - 0.4f}, 0.7f, ironLight);
    }
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

void DrawPlayer(
    const Player& player,
    Texture2D texture,
    int spriteRow,
    float frameWidth,
    float frameHeight,
    int spriteRowCount,
    float frameStride,
    float sourceOffsetX,
    float sourceOffsetY,
    Color tint
) {
    if (texture.id > 0) {
        if (frameStride <= 0.0f) frameStride = frameWidth;
        spriteRow = std::clamp(spriteRow, 0, std::max(0, spriteRowCount - 1));
        int frameIndex = 1;
        if (player.walking) {
            frameIndex = (static_cast<int>(player.animationTimer * 8.0f) % 2 == 0) ? 0 : 2;
        }
        float sourceX = frameIndex * frameStride + sourceOffsetX;
        float sourceY = spriteRow * frameHeight + sourceOffsetY;
        Rectangle source{
            sourceX,
            sourceY,
            player.facingRight ? frameWidth : -frameWidth,
            frameHeight
        };
        Rectangle dest{
            player.rect.x + (player.rect.width - frameWidth) * 0.5f,
            player.rect.y + player.rect.height - frameHeight + 1.0f,
            frameWidth,
            frameHeight
        };

        DrawTexturePro(texture, source, dest, {0, 0}, 0.0f, tint);
    }
    else {
        const bool warningTint =
            tint.r != WHITE.r || tint.g != WHITE.g ||
            tint.b != WHITE.b || tint.a != WHITE.a;
        DrawRectangleRec(player.rect, warningTint ? ColorLerp(BLACK, tint, 0.65f) : BLACK);
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

void DrawFloodPump(const Valve& valve, Vector2 outlet, float waterSurfaceY, bool filling) {
    const Color outline{24, 29, 32, 255};
    const Color ironDark{48, 57, 61, 255};
    const Color iron{78, 89, 93, 255};
    const Color ironLight{135, 146, 147, 255};
    const Color brass{188, 139, 48, 255};
    const Color water{47, 139, 218, 255};
    const float openAmount = std::clamp(valve.turnDegrees / 360.0f, 0.0f, 1.0f);
    const float pipeRadius = 9.0f;
    const float innerRadius = 6.0f;
    const Vector2 elbow{outlet.x, valve.center.y};
    const Vector2 pipeStart{valve.center.x + valve.radius * 0.72f, valve.center.y};

    // Pipe outline, cast-iron body, and a subtle top-edge highlight keep the
    // assembly readable against both light background panels and dark tiles.
    DrawLineEx(pipeStart, elbow, pipeRadius * 2.0f + 4.0f, outline);
    DrawLineEx(elbow, outlet, pipeRadius * 2.0f + 4.0f, outline);
    DrawCircleV(elbow, pipeRadius + 2.0f, outline);
    DrawLineEx(pipeStart, elbow, pipeRadius * 2.0f, iron);
    DrawLineEx(elbow, outlet, pipeRadius * 2.0f, iron);
    DrawCircleV(elbow, pipeRadius, iron);
    DrawLineEx({pipeStart.x, pipeStart.y - 4.5f}, {elbow.x - 1.0f, elbow.y - 4.5f}, 2.0f, ironLight);
    DrawLineEx({elbow.x - 4.5f, elbow.y}, {outlet.x - 4.5f, outlet.y}, 2.0f, ironLight);

    if (openAmount > 0.001f) {
        Color flowColor = Fade(water, 0.35f + openAmount * 0.55f);
        DrawLineEx(pipeStart, elbow, innerRadius, flowColor);
        DrawLineEx(elbow, outlet, innerRadius, flowColor);
        DrawCircleV(elbow, innerRadius * 0.5f, flowColor);

        float dashOffset = fmodf(static_cast<float>(GetTime()) * (28.0f + openAmount * 52.0f), 18.0f);
        for (float x = pipeStart.x + dashOffset; x < elbow.x - 5.0f; x += 18.0f) {
            DrawLineEx({x, valve.center.y - 1.0f}, {fminf(x + 7.0f, elbow.x - 5.0f), valve.center.y - 1.0f},
                1.5f, Fade(RAYWHITE, 0.32f + openAmount * 0.34f));
        }
        for (float y = valve.center.y + dashOffset; y < outlet.y - 5.0f; y += 18.0f) {
            DrawLineEx({outlet.x - 1.0f, y}, {outlet.x - 1.0f, fminf(y + 7.0f, outlet.y - 5.0f)},
                1.5f, Fade(RAYWHITE, 0.32f + openAmount * 0.34f));
        }
    }

    // Bolted flanges make the direction changes and component boundaries clear.
    const auto drawFlange = [&](Vector2 center, bool vertical) {
        Rectangle flange = vertical
            ? Rectangle{center.x - 13.0f, center.y - 5.0f, 26.0f, 10.0f}
            : Rectangle{center.x - 5.0f, center.y - 13.0f, 10.0f, 26.0f};
        DrawRectangleRec(flange, outline);
        Rectangle face{flange.x + 2.0f, flange.y + 2.0f, flange.width - 4.0f, flange.height - 4.0f};
        DrawRectangleRec(face, ironLight);
        if (vertical) {
            DrawCircleV({center.x - 8.0f, center.y}, 1.8f, outline);
            DrawCircleV({center.x + 8.0f, center.y}, 1.8f, outline);
        }
        else {
            DrawCircleV({center.x, center.y - 8.0f}, 1.8f, outline);
            DrawCircleV({center.x, center.y + 8.0f}, 1.8f, outline);
        }
    };
    drawFlange({valve.center.x + valve.radius + 8.0f, valve.center.y}, false);
    drawFlange({outlet.x, outlet.y - 11.0f}, true);

    // Centrifugal pump casing and foot. The wheel is drawn afterward on top.
    DrawRectangleRec({valve.center.x - 22.0f, valve.center.y + valve.radius - 3.0f, 44.0f, 9.0f}, outline);
    DrawRectangleRec({valve.center.x - 18.0f, valve.center.y + valve.radius - 2.0f, 36.0f, 5.0f}, ironLight);
    DrawCircleV(valve.center, valve.radius + 6.0f, outline);
    DrawCircleV(valve.center, valve.radius + 2.0f, ironDark);
    DrawRing(valve.center, valve.radius - 5.0f, valve.radius + 1.0f, 205.0f, 335.0f, 20, iron);
    for (float angle : {45.0f, 135.0f, 225.0f, 315.0f}) {
        Vector2 bolt{
            valve.center.x + cosf(angle * DEG2RAD) * (valve.radius + 2.0f),
            valve.center.y + sinf(angle * DEG2RAD) * (valve.radius + 2.0f)
        };
        DrawCircleV(bolt, 3.0f, outline);
        DrawCircleV({bolt.x - 0.6f, bolt.y - 0.6f}, 1.4f, ironLight);
    }

    // Small pressure gauge: its needle tracks valve opening, giving the pump a
    // second readable state cue without relying only on color.
    Vector2 gaugeCenter{valve.center.x + valve.radius + 34.0f, valve.center.y - 24.0f};
    DrawLineEx({gaugeCenter.x, gaugeCenter.y + 11.0f}, {gaugeCenter.x, valve.center.y - pipeRadius}, 4.0f, outline);
    DrawLineEx({gaugeCenter.x, gaugeCenter.y + 11.0f}, {gaugeCenter.x, valve.center.y - pipeRadius}, 2.0f, brass);
    DrawCircleV(gaugeCenter, 13.0f, outline);
    DrawCircleV(gaugeCenter, 10.0f, Color{218, 214, 190, 255});
    DrawRing(gaugeCenter, 8.5f, 9.5f, 200.0f, 340.0f, 12, brass);
    float needleAngle = (210.0f + openAmount * 120.0f) * DEG2RAD;
    Vector2 needleEnd{
        gaugeCenter.x + cosf(needleAngle) * 7.0f,
        gaugeCenter.y + sinf(needleAngle) * 7.0f
    };
    DrawLineEx(gaugeCenter, needleEnd, 1.8f, Color{150, 38, 31, 255});
    DrawCircleV(gaugeCenter, 2.0f, outline);

    // Downturned outlet with a dark mouth and riveted face.
    Rectangle nozzleOutline{outlet.x - 21.0f, outlet.y - 5.0f, 42.0f, 22.0f};
    DrawRectangleRec(nozzleOutline, outline);
    DrawRectangleRec({outlet.x - 18.0f, outlet.y - 3.0f, 36.0f, 16.0f}, iron);
    DrawLineEx({outlet.x - 15.0f, outlet.y}, {outlet.x + 15.0f, outlet.y}, 2.0f, ironLight);
    DrawRectangleRec({outlet.x - 13.0f, outlet.y + 10.0f, 26.0f, 7.0f}, Color{17, 23, 27, 255});
    DrawCircleV({outlet.x - 15.0f, outlet.y + 5.0f}, 2.0f, ironLight);
    DrawCircleV({outlet.x + 15.0f, outlet.y + 5.0f}, 2.0f, ironLight);

    if (filling && openAmount > 0.001f) {
        float streamWidth = 3.0f + openAmount * 7.0f;
        Vector2 streamStart{outlet.x, outlet.y + 17.0f};
        Vector2 streamEnd{outlet.x, fmaxf(streamStart.y, waterSurfaceY + 8.0f)};
        DrawLineEx(streamStart, streamEnd, streamWidth + 2.0f, Fade(DARKBLUE, 0.30f));
        DrawLineEx(streamStart, streamEnd, streamWidth, Fade(water, 0.45f + openAmount * 0.42f));
        DrawLineEx({streamStart.x - streamWidth * 0.18f, streamStart.y},
            {streamEnd.x - streamWidth * 0.18f, streamEnd.y}, 1.2f, Fade(RAYWHITE, 0.45f));
    }
}

void DrawValveBody(const Valve& valve, bool playerNear) {
    const Color outline{24, 29, 32, 255};
    const Color rustDark{105, 54, 35, 255};
    const Color rust{174, 83, 45, 255};
    const Color rustLight{224, 128, 66, 255};
    const Color brass{198, 149, 55, 255};
    float openAmount = std::clamp(valve.turnDegrees / 360.0f, 0.0f, 1.0f);
    Color stateColor = valve.opened ? SKYBLUE : (playerNear ? ORANGE : rustLight);
    Color wheelColor = playerNear && !valve.opened ? rustLight : rust;
    float rotation = valve.turnDegrees;

    // Progress collar reads as part of the machine instead of a floating HUD arc.
    DrawRing(valve.center, valve.radius + 5.0f, valve.radius + 9.0f, 0.0f, 360.0f, 40, outline);
    if (openAmount > 0.001f) {
        DrawRing(valve.center, valve.radius + 6.0f, valve.radius + 8.0f,
            -90.0f, -90.0f + openAmount * 360.0f, 40, SKYBLUE);
    }
    else if (playerNear) {
        DrawRing(valve.center, valve.radius + 6.0f, valve.radius + 8.0f, -100.0f, -80.0f, 4, ORANGE);
    }

    DrawRing(valve.center, valve.radius - 6.0f, valve.radius + 1.5f, 0.0f, 360.0f, 40, outline);
    DrawRing(valve.center, valve.radius - 5.0f, valve.radius - 1.0f, 0.0f, 360.0f, 40, wheelColor);
    DrawRing(valve.center, valve.radius - 4.0f, valve.radius - 2.0f, 205.0f, 330.0f, 18, rustDark);
    DrawRing(valve.center, valve.radius - 4.5f, valve.radius - 2.5f, 25.0f, 155.0f, 18, rustLight);

    for (int i = 0; i < 6; i++) {
        float angle = rotation + i * 60.0f;
        Vector2 end{
            valve.center.x + cosf(angle * DEG2RAD) * (valve.radius - 4.0f),
            valve.center.y + sinf(angle * DEG2RAD) * (valve.radius - 4.0f)
        };
        DrawLineEx(valve.center, end, 7.0f, outline);
        DrawLineEx(valve.center, end, 4.0f, wheelColor);
        DrawCircleV(end, 3.6f, outline);
        DrawCircleV(end, 2.2f, rustLight);
    }

    DrawCircleV(valve.center, 10.0f, outline);
    DrawCircleV(valve.center, 7.0f, brass);
    DrawCircleV({valve.center.x - 1.4f, valve.center.y - 1.4f}, 3.2f, Color{231, 190, 92, 255});
    DrawCircleLinesV(valve.center, valve.radius + 1.0f, stateColor);
}

void DrawValvePrompt(
    const Valve& valve,
    bool playerNear,
    const char* interactPrompt,
    bool belowValve
) {
    if (!playerNear || valve.opened) return;

    int promptWidth = MeasureText(interactPrompt, 20);
    Rectangle promptPanel{
        valve.center.x - promptWidth * 0.5f - 8.0f,
        belowValve
            ? valve.center.y + valve.radius + 7.0f
            : valve.center.y - valve.radius - 35.0f,
        static_cast<float>(promptWidth + 16),
        25.0f
    };
    DrawRectangleRounded(promptPanel, 0.24f, 4, Fade(BLACK, 0.78f));
    DrawRectangleRoundedLinesEx(promptPanel, 0.24f, 4, 1.5f, Fade(ORANGE, 0.90f));
    DrawText(interactPrompt, static_cast<int>(valve.center.x - promptWidth * 0.5f),
        static_cast<int>(promptPanel.y + 2.0f), 20, RAYWHITE);
}

void DrawValve(const Valve& valve, bool playerNear, const char* interactPrompt) {
    DrawValveBody(valve, playerNear);
    DrawValvePrompt(valve, playerNear, interactPrompt);
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
    if (fluid.type == FluidType::Gas) {
        DrawGasVolume(fluid);
        return;
    }
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

    if (fluid.type == FluidType::Gas) {
        // Gas is drawn behind level geometry by DrawFluidBackground so exact
        // wall silhouettes mask the continuous density mesh without a gap.
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
    const bool mounted = gear.mounting == GearMounting::Mounted;
    const bool horizontal = gear.orientation == GearOrientation::Horizontal;
    // Horizontal gears need enough apparent depth for their teeth and cage rods
    // to survive pixel rounding at the small sizes used by clock mechanisms.
    const float verticalScale = horizontal ? 0.56f : 1.0f;
    const float horizontalDepth = horizontal
        ? std::clamp(gear.radius * 0.16f, 4.0f, 9.0f)
        : 0.0f;
    const float rimInner = gear.radius * 0.57f;
    const float rimOuter = gear.radius * 0.80f;
    const float hubRadius = gear.radius * 0.21f;
    const bool isLantern = gear.visualType == GearVisualType::LanternPinion;
    const bool isEscape = gear.visualType == GearVisualType::Escape;
    const bool isRatchet = gear.visualType == GearVisualType::Ratchet;
    const bool isBevel = gear.visualType == GearVisualType::Bevel;
    const bool isSector = gear.visualType == GearVisualType::Sector;
    const bool isCount = gear.visualType == GearVisualType::Count;

    Color rimDark{67, 48, 29, 255};
    Color rimBase{142, 105, 53, 255};
    Color rimLight{218, 176, 91, 255};
    Color webDark{39, 47, 50, 255};
    Color webBase{79, 88, 89, 255};
    Color webLight{132, 143, 142, 255};
    if (isRatchet) {
        rimDark = {76, 36, 25, 255};
        rimBase = {169, 82, 43, 255};
        rimLight = {230, 137, 72, 255};
        webBase = {92, 63, 49, 255};
    }
    else if (isEscape) {
        rimDark = {78, 54, 17, 255};
        rimBase = {190, 142, 38, 255};
        rimLight = {245, 205, 87, 255};
        webBase = {91, 73, 42, 255};
    }
    else if (isBevel) {
        rimDark = {39, 50, 55, 255};
        rimBase = {94, 111, 116, 255};
        rimLight = {177, 194, 194, 255};
        webBase = {59, 73, 78, 255};
    }
    else if (isSector || isCount) {
        rimDark = {61, 43, 31, 255};
        rimBase = {126, 87, 55, 255};
        rimLight = {198, 143, 79, 255};
        webBase = {82, 67, 55, 255};
    }
    else if (gear.toothCount >= 90) {
        rimDark = {43, 51, 53, 255};
        rimBase = {103, 116, 116, 255};
        rimLight = {185, 198, 194, 255};
        webBase = {68, 78, 79, 255};
    }

    const auto projectedPoint = [&](float angle, float radius, float yOffset = 0.0f) {
        return Vector2{
            gear.center.x + cosf(angle) * radius,
            gear.center.y + sinf(angle) * radius * verticalScale + yOffset
        };
    };

    const auto drawQuad = [](Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
        DrawSolidTriangle(a, b, c, color);
        DrawSolidTriangle(a, c, d, color);
    };
    const auto drawEllipticalRing = [&](float innerRadius, float outerRadius, float startDegrees,
                                        float endDegrees, float yOffset, Color color) {
        const int segments = std::max(8, static_cast<int>(ceilf(fabsf(endDegrees - startDegrees) / 7.5f)));
        for (int segment = 0; segment < segments; ++segment) {
            const float amount0 = static_cast<float>(segment) / segments;
            const float amount1 = static_cast<float>(segment + 1) / segments;
            const float angle0 = (startDegrees + (endDegrees - startDegrees) * amount0) * DEG2RAD;
            const float angle1 = (startDegrees + (endDegrees - startDegrees) * amount1) * DEG2RAD;
            drawQuad(projectedPoint(angle0, outerRadius, yOffset), projectedPoint(angle1, outerRadius, yOffset),
                projectedPoint(angle1, innerRadius, yOffset), projectedPoint(angle0, innerRadius, yOffset), color);
        }
    };
    const auto drawNearRimWalls = [&](float innerRadius, float outerRadius, float startDegrees,
                                      float endDegrees, float topOffset, float bottomOffset) {
        const int segments = std::max(8, static_cast<int>(ceilf(fabsf(endDegrees - startDegrees) / 7.5f)));
        for (int segment = 0; segment < segments; ++segment) {
            const float amount0 = static_cast<float>(segment) / segments;
            const float amount1 = static_cast<float>(segment + 1) / segments;
            const float angle0 = (startDegrees + (endDegrees - startDegrees) * amount0) * DEG2RAD;
            const float angle1 = (startDegrees + (endDegrees - startDegrees) * amount1) * DEG2RAD;
            const float middleAngle = (angle0 + angle1) * 0.5f;
            if (sinf(middleAngle) < 0.0f) continue;

            drawQuad(projectedPoint(angle0, outerRadius, topOffset),
                projectedPoint(angle1, outerRadius, topOffset),
                projectedPoint(angle1, outerRadius, bottomOffset),
                projectedPoint(angle0, outerRadius, bottomOffset), rimDark);
            drawQuad(projectedPoint(angle1, innerRadius, topOffset),
                projectedPoint(angle0, innerRadius, topOffset),
                projectedPoint(angle0, innerRadius, bottomOffset),
                projectedPoint(angle1, innerRadius, bottomOffset), webDark);
        }
    };
    const auto drawBevelGrooves = [&](float yOffset) {
        if (!isBevel) return;

        for (int groove = 0; groove < 12; ++groove) {
            const float angle = (gear.rotation + groove * 30.0f) * DEG2RAD;
            DrawLineEx(projectedPoint(angle - 0.035f, rimInner + 2.0f, yOffset),
                projectedPoint(angle + 0.035f, rimOuter - 2.0f, yOffset),
                1.5f, Fade(rimLight, 0.68f));
        }
    };

    const float sectorSpan = isSector ? 235.0f : 360.0f;
    const float sectorStart = gear.rotation - (isSector ? 117.5f : 0.0f);
    const float sectorEnd = sectorStart + sectorSpan;
    const int visualTeeth = std::min(gear.toothCount,
        std::clamp(static_cast<int>(roundf(gear.radius / 4.2f)), 7, isEscape ? 24 : 28));

    if (mounted) {
        // A wall shadow and backing washer identify the fixed axle constraint.
        if (horizontal) {
            drawEllipticalRing(rimInner - 2.0f, rimOuter + gear.radius * 0.15f,
                sectorStart, sectorEnd, 10.0f, Fade(BLACK, 0.40f));
        }
        else if (isSector) {
            DrawRing({gear.center.x + 5.0f, gear.center.y + 7.0f}, rimInner - 2.0f, rimOuter + 3.0f,
                sectorStart, sectorEnd, 40, Fade(BLACK, 0.42f));
        }
        else {
            DrawCircleV({gear.center.x + 5.0f, gear.center.y + 7.0f}, rimOuter + gear.radius * 0.16f,
                Fade(BLACK, 0.24f));
        }
        if (!horizontal) {
            DrawCircleV({gear.center.x + 3.0f, gear.center.y + 4.0f}, hubRadius * 1.55f, Fade(BLACK, 0.42f));
            DrawCircleV(gear.center, hubRadius * 1.42f, webDark);
            DrawRing(gear.center, hubRadius * 1.08f, hubRadius * 1.30f,
                0.0f, 360.0f, 24, rimBase);
        }
    }
    else {
        DrawEllipse(static_cast<int>(gear.center.x + gear.radius * 0.10f),
            static_cast<int>(gear.center.y + gear.radius * (horizontal ? 0.42f : 0.70f)),
            gear.radius * 0.76f, gear.radius * (horizontal ? 0.15f : 0.22f), Fade(BLACK, 0.20f));
    }

    const auto drawTeeth = [&](bool nearPass) {
        if (isLantern) return;

        std::vector<int> toothOrder;
        toothOrder.reserve(visualTeeth);
        for (int tooth = 0; tooth < visualTeeth; ++tooth) toothOrder.push_back(tooth);
        if (horizontal) {
            // Teeth overlap in the foreshortened view, so angular index order is
            // not a valid painter's order. Sort from the back of the ellipse to
            // the front; this is especially visible as teeth round the left side.
            std::stable_sort(toothOrder.begin(), toothOrder.end(), [&](int first, int second) {
                const float firstDegrees = (static_cast<float>(first) + 0.5f) * (sectorSpan / visualTeeth);
                const float secondDegrees = (static_cast<float>(second) + 0.5f) * (sectorSpan / visualTeeth);
                const float firstDepth = sinf((sectorStart + firstDegrees) * DEG2RAD);
                const float secondDepth = sinf((sectorStart + secondDegrees) * DEG2RAD);
                return firstDepth < secondDepth;
            });
        }

        for (int tooth : toothOrder) {
            const float localDegrees = (static_cast<float>(tooth) + 0.5f) * (sectorSpan / visualTeeth);
            const float angle = (sectorStart + localDegrees) * DEG2RAD;
            const bool nearSide = sinf(angle) >= 0.0f;
            if (horizontal && nearSide != nearPass) continue;
            const float surfaceOffset = horizontal && nearPass
                ? fmaxf(1.5f, horizontalDepth * 0.28f)
                : 0.0f;

            const float toothStep = sectorSpan * DEG2RAD / visualTeeth;
            const float depth = gear.radius * (isEscape ? 0.28f : 0.18f);
            const float rootHalfAngle = toothStep * (isRatchet ? 0.36f : 0.30f);
            const float tipHalfAngle = toothStep * (isEscape ? 0.07f : (isRatchet ? 0.10f : 0.20f));
            const float skew = isRatchet ? toothStep * 0.24f : 0.0f;

            using ToothCorners = std::array<Vector2, 4>;
            const auto toothCorners = [&](float yOffset, float border) {
                const float borderAngle = border / fmaxf(rimOuter, 1.0f);
                const float rootRadius = rimOuter - 1.0f - border;
                const float tipRadius = rimOuter + depth + border;
                const Vector2 a = projectedPoint(angle - rootHalfAngle - borderAngle, rootRadius, yOffset);
                const Vector2 b = projectedPoint(angle + skew - tipHalfAngle - borderAngle, tipRadius, yOffset);
                const Vector2 c = projectedPoint(angle + skew + tipHalfAngle + borderAngle, tipRadius, yOffset);
                const Vector2 d = projectedPoint(angle + rootHalfAngle + borderAngle, rootRadius, yOffset);
                return ToothCorners{a, b, c, d};
            };
            const auto drawToothFace = [&](const ToothCorners& corners, Color color) {
                drawQuad(corners[0], corners[1], corners[2], corners[3], color);
            };
            const auto drawToothSides = [&](const ToothCorners& top, const ToothCorners& bottom, Color color) {
                // Only extrude the exposed flanks and tip. A complete lower tooth
                // face would show through an open wheel and look like a layer-order bug.
                drawQuad(top[0], top[1], bottom[1], bottom[0], color);
                drawQuad(top[1], top[2], bottom[2], bottom[1], color);
                drawQuad(top[2], top[3], bottom[3], bottom[2], color);
            };

            if (horizontal) {
                const ToothCorners borderedTop = toothCorners(1.0f + surfaceOffset, 1.5f);
                const ToothCorners borderedBottom = toothCorners(horizontalDepth, 1.5f);
                drawToothSides(borderedTop, borderedBottom, webDark);

                const ToothCorners top = toothCorners(surfaceOffset, 0.0f);
                const ToothCorners bottom = toothCorners(horizontalDepth, 0.0f);
                drawToothSides(top, bottom, rimDark);
            }
            drawToothFace(toothCorners(1.0f + surfaceOffset, 1.5f), webDark);
            drawToothFace(toothCorners(surfaceOffset, 0.0f), rimBase);

            // A thin highlight on the leading half keeps individual teeth readable.
            if (tooth % 2 == 0) {
                const Vector2 highlightStart = projectedPoint(
                    angle - rootHalfAngle * 0.72f, rimOuter - 0.5f, surfaceOffset);
                const Vector2 highlightEnd = projectedPoint(
                    angle + skew - tipHalfAngle * 0.65f,
                    rimOuter + depth * 0.88f, surfaceOffset);
                DrawLineEx(highlightStart, highlightEnd, fmaxf(1.0f, gear.radius * 0.018f), Fade(rimLight, 0.72f));
            }
        }
    };

    const auto drawCountPins = [&](bool nearPass) {
        if (!isCount) return;

        for (int notch = 0; notch < 12; ++notch) {
            const float angle = (gear.rotation + 15.0f + notch * 30.0f) * DEG2RAD;
            const bool nearSide = sinf(angle) >= 0.0f;
            if (horizontal && nearSide != nearPass) continue;

            const float surfaceOffset = horizontal && nearPass ? 2.0f : 0.0f;
            const Vector2 pin = projectedPoint(angle, (rimInner + rimOuter) * 0.5f, surfaceOffset);
            const float pinRadius = fmaxf(1.5f, gear.radius * 0.035f);
            if (horizontal) {
                DrawEllipse(static_cast<int>(pin.x), static_cast<int>(pin.y),
                    pinRadius, fmaxf(1.0f, pinRadius * verticalScale), Color{226, 178, 78, 255});
            }
            else {
                DrawCircleV(pin, pinRadius, Color{226, 178, 78, 255});
            }
        }
    };

    // Rear teeth belong behind the wheel body. Upright wheels only need this
    // single pass because their teeth and rim share the same visual plane.
    drawTeeth(false);

    const int spokeCount = isEscape ? 4 : (isCount ? 8 : (isSector ? 3 : (gear.radius < 30.0f ? 4 : 6)));
    for (int spoke = 0; spoke < spokeCount; ++spoke) {
        const float amount = isSector
            ? (static_cast<float>(spoke) + 0.5f) / spokeCount
            : static_cast<float>(spoke) / spokeCount;
        const float angleDegrees = isSector
            ? sectorStart + sectorSpan * amount
            : gear.rotation + static_cast<float>(spoke) * (360.0f / spokeCount);
        const float angle = angleDegrees * DEG2RAD;
        const Vector2 start = projectedPoint(angle, hubRadius * 0.78f, horizontal ? 3.0f : 0.0f);
        const Vector2 end = projectedPoint(angle, rimInner + 3.0f, horizontal ? 3.0f : 0.0f);
        const float spokeWidth = fmaxf(3.0f, gear.radius * (isEscape ? 0.075f : 0.105f));
        DrawLineEx(start, end, spokeWidth + 3.0f, webDark);
        DrawLineEx(start, end, spokeWidth, webBase);
        const Vector2 highlightEnd = projectedPoint(angle, rimInner - gear.radius * 0.10f, horizontal ? 1.5f : -1.0f);
        DrawLineEx(start, highlightEnd, fmaxf(1.0f, spokeWidth * 0.22f), Fade(webLight, 0.60f));
    }

    if (horizontal) {
        if (!isLantern) {
            // Only the near half exposes the wheel's thickness. A second full
            // ring underneath leaks into openings and reads as incorrect depth.
            drawNearRimWalls(rimInner, rimOuter + 1.5f, sectorStart, sectorEnd,
                2.0f, horizontalDepth);
            drawEllipticalRing(rimInner, rimOuter, sectorStart, sectorEnd, 2.0f, rimBase);
            drawEllipticalRing(rimInner + 2.0f, rimOuter - 2.0f, sectorStart + 195.0f,
                fminf(sectorEnd, sectorStart + 340.0f), 0.0f, Fade(rimLight, 0.78f));
            // Grooves are engraved into the wheel face. They must be present
            // before foreground teeth are composited over the near edge.
            drawBevelGrooves(2.0f);

            // Far count-wheel pins sit on the upper surface but remain behind
            // the foreground edge details.
            drawCountPins(false);

            // Front teeth must be composited after the spokes and rim. Drawing the
            // whole tooth ring first caused the near half to disappear under the body.
            drawTeeth(true);

            // Near pins share the lowered front surface with the near teeth.
            drawCountPins(true);
        }
    }
    else {
        DrawRing(gear.center, rimInner - 2.0f, rimOuter + 2.0f, sectorStart, sectorEnd, 56, webDark);
        DrawRing(gear.center, rimInner, rimOuter, sectorStart, sectorEnd, 56, rimBase);
        DrawRing(gear.center, rimInner + 2.0f, rimOuter - 2.0f,
            sectorStart + (isSector ? 18.0f : 195.0f),
            sectorStart + (isSector ? sectorSpan * 0.55f : 340.0f), 28, Fade(rimLight, 0.74f));
        DrawRing(gear.center, rimInner + 1.0f, rimInner + 3.0f, sectorStart, sectorEnd, 48, Fade(webDark, 0.82f));
        drawBevelGrooves(0.0f);
    }

    // Small fasteners at the spoke joints sell the wheel as an assembled mechanism.
    if (!isLantern) {
        for (int spoke = 0; spoke < spokeCount; ++spoke) {
            const float amount = isSector
                ? (static_cast<float>(spoke) + 0.5f) / spokeCount
                : static_cast<float>(spoke) / spokeCount;
            const float angleDegrees = isSector
                ? sectorStart + sectorSpan * amount
                : gear.rotation + static_cast<float>(spoke) * (360.0f / spokeCount);
            const Vector2 fastener = projectedPoint(angleDegrees * DEG2RAD, rimInner + gear.radius * 0.035f,
                horizontal ? 1.5f : 0.0f);
            const float fastenerRadius = fmaxf(1.5f, gear.radius * 0.027f);
            DrawCircleV(fastener, fastenerRadius + 1.0f, webDark);
            DrawCircleV(fastener, fastenerRadius, rimLight);
            DrawCircleV({fastener.x - fastenerRadius * 0.28f, fastener.y - fastenerRadius * 0.28f},
                fmaxf(0.6f, fastenerRadius * 0.22f), Fade(RAYWHITE, 0.65f));
        }
    }

    // Lantern pinions use a cage of bright steel rods instead of a conventional toothed rim.
    if (isLantern) {
        const int rodCount = std::clamp(gear.toothCount, 6, 12);
        if (horizontal) {
            const Color trundleDark{42, 49, 51, 255};
            const Color trundleSteel{166, 180, 178, 255};

            // A horizontal lantern is read from above as one foreshortened
            // plate with trundles seated around its face. Stacking two complete
            // plates and connecting them with long screen-vertical rods made it
            // look like a basket instead of a gear.
            const float plateTop = 2.0f;
            const float plateBottom = fmaxf(4.0f, horizontalDepth * 0.82f);
            drawNearRimWalls(gear.radius * 0.47f, gear.radius * 0.76f,
                0.0f, 360.0f, plateTop, plateBottom);
            drawEllipticalRing(gear.radius * 0.45f, gear.radius * 0.78f,
                0.0f, 360.0f, plateTop + 1.0f, webDark);
            drawEllipticalRing(gear.radius * 0.49f, gear.radius * 0.74f,
                0.0f, 360.0f, plateTop, rimBase);
            drawEllipticalRing(gear.radius * 0.51f, gear.radius * 0.72f,
                195.0f, 340.0f, plateTop - 1.0f, Fade(rimLight, 0.76f));

            for (int rod = 0; rod < rodCount; ++rod) {
                const float angle = (gear.rotation + static_cast<float>(rod) * 360.0f / rodCount) * DEG2RAD;
                const Vector2 trundle = projectedPoint(angle, gear.radius * 0.62f, plateTop - 0.5f);
                const float radius = fmaxf(1.25f, gear.radius * 0.052f);
                DrawEllipse(static_cast<int>(trundle.x), static_cast<int>(trundle.y + 1.0f),
                    radius + 1.0f, fmaxf(1.0f, radius * verticalScale + 1.0f), trundleDark);
                DrawEllipse(static_cast<int>(trundle.x), static_cast<int>(trundle.y),
                    radius, fmaxf(1.0f, radius * verticalScale), trundleSteel);
            }

        }
        else {
            DrawRing(gear.center, gear.radius * 0.55f, gear.radius * 0.70f, 0.0f, 360.0f, 36, rimBase);
            for (int rod = 0; rod < rodCount; ++rod) {
                const float angle = (gear.rotation + static_cast<float>(rod) * 360.0f / rodCount) * DEG2RAD;
                const Vector2 rodCenter = projectedPoint(angle, gear.radius * 0.62f);
                const float rodRadius = fmaxf(2.0f, gear.radius * 0.075f);
                DrawCircleV(rodCenter, rodRadius + 1.5f, webDark);
                DrawCircleV(rodCenter, rodRadius, rimLight);
            }
        }
    }

    if (!horizontal) drawCountPins(false);

    if (horizontal) {
        // The spindle is centered on the wheel. The old detached vertical post
        // was drawn last and floated over open sector and escape-wheel areas.
        DrawEllipse(static_cast<int>(gear.center.x), static_cast<int>(gear.center.y + 5.0f),
            hubRadius + 2.0f, hubRadius * verticalScale + 2.0f, webDark);
        DrawEllipse(static_cast<int>(gear.center.x), static_cast<int>(gear.center.y),
            hubRadius, hubRadius * verticalScale, rimBase);
        const float capY = gear.center.y - 1.0f;
        DrawEllipse(static_cast<int>(gear.center.x), static_cast<int>(capY),
            hubRadius * 0.36f, hubRadius * 0.16f, rimLight);
    }
    else {
        DrawCircleV(gear.center, hubRadius + 3.0f, webDark);
        DrawCircleV(gear.center, hubRadius, rimBase);
        DrawRing(gear.center, hubRadius * 0.72f, hubRadius * 0.90f, 205.0f, 335.0f, 12, rimLight);
        DrawCircleV(gear.center, gear.radius * 0.065f + 1.0f, BLACK);
        DrawCircleV(gear.center, gear.radius * 0.065f, Color{205, 164, 75, 255});
        const float keyAngle = gear.rotation * DEG2RAD;
        DrawLineEx(gear.center, projectedPoint(keyAngle, hubRadius * 0.72f),
            fmaxf(1.5f, gear.radius * 0.025f), webDark);
    }
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
    const float poweredAmount = std::clamp(fan.power, 0.0f, 1.0f);
    const Color edge{18, 28, 34, 255};
    const Color housingSteel{56, 79, 88, 255};
    const Color housingFront{42, 60, 68, 255};
    const Color steelHighlight{137, 169, 177, 255};
    const Color grilleSteel{92, 122, 132, 255};
    const Color bladeColor = poweredAmount > 0.02f
        ? Color{55, 183, 213, 255}
        : Color{62, 102, 116, 255};
    const Color warningYellow{255, 187, 48, 255};
    constexpr float OuterRadiusX = 34.0f;
    constexpr float OuterRadiusY = 14.0f;
    constexpr float FaceOffsetY = -7.0f;
    constexpr float DrumDepth = 15.0f;
    const Vector2 faceCenter{fan.center.x, fan.center.y + FaceOffsetY};
    const Vector2 frontCenter{faceCenter.x, faceCenter.y + DrumDepth};

    // The shadow, visible front wall, and offset ellipses establish a short
    // cylindrical drum lying flat on the floor with its blades facing upward.
    DrawEllipse(
        static_cast<int>(fan.center.x),
        static_cast<int>(frontCenter.y + 5.0f),
        OuterRadiusX + 4.0f,
        OuterRadiusY * 0.82f,
        Fade(BLACK, 0.28f)
    );
    DrawRectangle(
        static_cast<int>(faceCenter.x - OuterRadiusX),
        static_cast<int>(faceCenter.y),
        static_cast<int>(OuterRadiusX * 2.0f),
        static_cast<int>(DrumDepth),
        housingSteel
    );
    DrawEllipse(
        static_cast<int>(frontCenter.x),
        static_cast<int>(frontCenter.y),
        OuterRadiusX,
        OuterRadiusY,
        housingFront
    );
    DrawLineEx(
        {faceCenter.x - OuterRadiusX, faceCenter.y},
        {frontCenter.x - OuterRadiusX, frontCenter.y},
        2.5f,
        edge
    );
    DrawLineEx(
        {faceCenter.x + OuterRadiusX, faceCenter.y},
        {frontCenter.x + OuterRadiusX, frontCenter.y},
        2.5f,
        edge
    );

    DrawEllipse(
        static_cast<int>(faceCenter.x),
        static_cast<int>(faceCenter.y),
        OuterRadiusX,
        OuterRadiusY,
        edge
    );
    DrawEllipse(
        static_cast<int>(faceCenter.x),
        static_cast<int>(faceCenter.y),
        OuterRadiusX - 3.0f,
        OuterRadiusY - 2.0f,
        Color{34, 48, 56, 255}
    );
    if (poweredAmount > 0.02f) {
        DrawEllipse(
            static_cast<int>(faceCenter.x),
            static_cast<int>(faceCenter.y),
            OuterRadiusX - 5.0f,
            OuterRadiusY - 3.0f,
            Fade(SKYBLUE, 0.10f + poweredAmount * 0.12f)
        );
    }

    const auto bladePoint = [&](float angle, float radial, float lateral) {
        return Vector2{
            faceCenter.x + cosf(angle) * radial - sinf(angle) * lateral,
            faceCenter.y + (sinf(angle) * radial + cosf(angle) * lateral) * 0.42f
        };
    };
    constexpr std::array<Vector2, 10> BladeProfile{{
        {4.0f, -1.5f},
        {9.0f, -4.5f},
        {17.0f, -7.0f},
        {24.0f, -7.0f},
        {28.0f, -4.0f},
        {30.0f, 0.0f},
        {28.0f, 4.0f},
        {22.0f, 6.5f},
        {13.0f, 5.5f},
        {6.0f, 2.0f}
    }};

    for (int i = 0; i < 6; i++) {
        const float angle =
            fan.rotation * DEG2RAD +
            static_cast<float>(i) * 60.0f * DEG2RAD;
        std::array<Vector2, 12> blade{};
        blade[0] = bladePoint(angle, 17.0f, 0.0f);
        for (int pointIndex = 0; pointIndex < static_cast<int>(BladeProfile.size()); ++pointIndex) {
            blade[pointIndex + 1] = bladePoint(
                angle,
                BladeProfile[pointIndex].x,
                BladeProfile[pointIndex].y
            );
        }
        blade.back() = blade[1];
        DrawTriangleFan(blade.data(), static_cast<int>(blade.size()), bladeColor);
        for (int pointIndex = 1; pointIndex < static_cast<int>(blade.size()) - 1; ++pointIndex) {
            DrawLineEx(
                blade[pointIndex],
                blade[pointIndex + 1],
                1.0f,
                Fade(steelHighlight, 0.72f)
            );
        }
    }

    // Foreshortened grille bars share the same ellipse as the blade disk,
    // reinforcing the upward-facing perspective.
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * 45.0f * DEG2RAD;
        DrawLineEx(
            {
                faceCenter.x + cosf(angle) * 5.0f,
                faceCenter.y + sinf(angle) * 2.0f
            },
            {
                faceCenter.x + cosf(angle) * 29.0f,
                faceCenter.y + sinf(angle) * 11.5f
            },
            1.5f,
            Fade(grilleSteel, 0.88f)
        );
    }
    DrawEllipseLines(
        static_cast<int>(faceCenter.x),
        static_cast<int>(faceCenter.y),
        OuterRadiusX - 2.0f,
        OuterRadiusY - 1.0f,
        grilleSteel
    );
    DrawEllipseLines(
        static_cast<int>(faceCenter.x),
        static_cast<int>(faceCenter.y),
        16.0f,
        6.5f,
        grilleSteel
    );
    DrawCircleV(faceCenter, 6.0f, edge);
    DrawCircleV(faceCenter, 3.5f, warningYellow);

    for (float side : {-1.0f, 1.0f}) {
        const Vector2 bolt{
            frontCenter.x + side * 24.0f,
            frontCenter.y + 1.0f
        };
        DrawCircleV(bolt, 2.5f, edge);
        DrawCircleV(bolt, 1.0f, steelHighlight);
    }

    const Vector2 statusCenter{frontCenter.x, frontCenter.y + 6.0f};
    DrawEllipse(
        static_cast<int>(statusCenter.x),
        static_cast<int>(statusCenter.y),
        7.0f,
        3.5f,
        edge
    );
    DrawCircleV(
        statusCenter,
        1.8f,
        poweredAmount > 0.02f ? Color{66, 244, 151, 255} : Color{201, 65, 50, 255}
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
    constexpr float OutletOffset = 18.0f;
    Vector2 windOrigin{
        fan.center.x + fan.direction.x * OutletOffset,
        fan.center.y + fan.direction.y * OutletOffset
    };
    float speed = fmaxf(40.0f, fan.strength * fan.power);
    float time = static_cast<float>(GetTime());
    int streakCount = static_cast<int>(fmaxf(5.0f, fan.length / 34.0f));

    for (int i = 0; i < streakCount; i++) {
        float seed = static_cast<float>(i) * 37.0f;
        float along = fmodf(seed + time * speed, fan.length);
        float lane = (fmodf(seed * 1.7f, fan.width) - fan.width * 0.5f);
        Vector2 start{
            windOrigin.x + fan.direction.x * along + normal.x * lane,
            windOrigin.y + fan.direction.y * along + normal.y * lane
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

    for (int i = 1; i < ramp.segmentCount; i++) {
        float amount = static_cast<float>(i) / static_cast<float>(ramp.segmentCount);
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

    Vector2 ringMountCenter{
        (rings[0].x + rings[1].x) * 0.5f,
        (rings[0].y + rings[1].y) * 0.5f
    };
    Vector2 rearFace{
        ringMountCenter.x - normal.x * trapDoor.thickness * 0.5f,
        ringMountCenter.y - normal.y * trapDoor.thickness * 0.5f
    };
    drawRing(rings[0], rearFace);

    Vector2 p1{hinge.x - normal.x * trapDoor.thickness * 0.5f, hinge.y - normal.y * trapDoor.thickness * 0.5f};
    Vector2 p2{end.x - normal.x * trapDoor.thickness * 0.5f, end.y - normal.y * trapDoor.thickness * 0.5f};
    Vector2 p3{end.x + normal.x * trapDoor.thickness * 0.5f, end.y + normal.y * trapDoor.thickness * 0.5f};
    Vector2 p4{hinge.x + normal.x * trapDoor.thickness * 0.5f, hinge.y + normal.y * trapDoor.thickness * 0.5f};

    DrawSolidTriangle(p1, p2, p3, wood);
    DrawSolidTriangle(p1, p3, p4, wood);

    for (int i = 1; i < 3; i++) {
        float amount = static_cast<float>(i) / 3.0f;
        Vector2 top{
            p1.x + (p2.x - p1.x) * amount,
            p1.y + (p2.y - p1.y) * amount
        };
        Vector2 bottom{
            p4.x + (p3.x - p4.x) * amount,
            p4.y + (p3.y - p4.y) * amount
        };
        DrawLineEx(top, bottom, 1.2f, Fade(BLACK, 0.42f));
    }

    DrawLineEx(p1, p2, 2.0f, BLACK);
    DrawLineEx(p3, p4, 2.0f, BLACK);
    DrawLineEx(p4, p1, 5.0f, BLACK);
    DrawLineEx(p4, p1, 3.0f, iron);
    DrawLineEx(p2, p3, 5.0f, BLACK);
    DrawLineEx(p2, p3, 3.0f, ironHighlight);

    DrawCircleV(hinge, trapDoor.thickness * 0.64f, BLACK);
    DrawCircleV(hinge, trapDoor.thickness * 0.48f, ironHighlight);
    DrawCircleV(hinge, trapDoor.thickness * 0.19f, Color{31, 35, 37, 255});

    Vector2 frontFace{
        ringMountCenter.x + normal.x * trapDoor.thickness * 0.5f,
        ringMountCenter.y + normal.y * trapDoor.thickness * 0.5f
    };
    drawRing(rings[1], frontFace);
}

void DrawButton(const Button& button) {
    const float scale = std::clamp(button.rect.height / 16.0f, 0.62f, 1.35f);
    const float pressOffset = button.pressed ? button.rect.height * 0.32f : 0.0f;
    Rectangle base{
        button.rect.x - 4.0f * scale,
        button.rect.y + button.rect.height * 0.46f,
        button.rect.width + 8.0f * scale,
        button.rect.height * 0.58f
    };
    Rectangle plate{
        button.rect.x + 2.0f * scale,
        button.rect.y + pressOffset,
        button.rect.width - 4.0f * scale,
        button.rect.height * 0.50f
    };

    const Color steelEdge{20, 31, 38, 255};
    const Color steelBody{58, 82, 92, 255};
    const Color steelHighlight{128, 164, 174, 255};
    const Color warningYellow{255, 190, 54, 255};
    const Color plateFace = button.pressed
        ? Color{45, 214, 139, 255}
        : Color{244, 67, 54, 255};
    const Color plateHighlight = button.pressed
        ? Color{142, 255, 202, 255}
        : Color{255, 151, 72, 255};

    // Wide mounting feet and a riveted steel housing make this read as a
    // heavy-duty floor switch rather than a plain colored rectangle.
    Rectangle leftFoot{
        base.x - 3.0f * scale,
        base.y + base.height * 0.45f,
        8.0f * scale,
        base.height * 0.55f
    };
    Rectangle rightFoot{
        base.x + base.width - 5.0f * scale,
        leftFoot.y,
        8.0f * scale,
        leftFoot.height
    };
    DrawRectangleRec(leftFoot, steelEdge);
    DrawRectangleRec(rightFoot, steelEdge);
    DrawRectangleRec(base, steelBody);
    DrawRectangleLinesEx(base, 2.0f * scale, steelEdge);
    DrawLineEx(
        {base.x + 2.0f * scale, base.y + 2.0f * scale},
        {base.x + base.width - 2.0f * scale, base.y + 2.0f * scale},
        1.5f * scale,
        steelHighlight
    );

    const int stripeCount = std::max(3, static_cast<int>(button.rect.width / (15.0f * scale)));
    const float stripeWidth = base.width / static_cast<float>(stripeCount);
    for (int index = 0; index < stripeCount; ++index) {
        if (index % 2 != 0) continue;
        DrawRectangleRec(
            {
                base.x + index * stripeWidth,
                base.y + base.height - 3.0f * scale,
                stripeWidth,
                3.0f * scale
            },
            warningYellow
        );
    }

    const float boltRadius = 1.7f * scale;
    DrawCircleV(
        {base.x + 7.0f * scale, base.y + base.height * 0.55f},
        boltRadius,
        steelHighlight
    );
    DrawCircleV(
        {base.x + base.width - 7.0f * scale, base.y + base.height * 0.55f},
        boltRadius,
        steelHighlight
    );

    if (!button.pressed) {
        DrawRectangleRec(
            {
                plate.x + 5.0f * scale,
                plate.y + plate.height,
                plate.width - 10.0f * scale,
                button.rect.height * 0.22f
            },
            steelEdge
        );
    }
    DrawRectangleRec(plate, plateFace);
    DrawRectangleLinesEx(plate, 2.0f * scale, steelEdge);
    DrawLineEx(
        {plate.x + 3.0f * scale, plate.y + 2.0f * scale},
        {plate.x + plate.width - 3.0f * scale, plate.y + 2.0f * scale},
        2.0f * scale,
        plateHighlight
    );
}

void DrawArrowTrap(const ArrowTrap& trap) {
    Vector2 direction = NormalizeOr(trap.direction);
    Vector2 normal{-direction.y, direction.x};
    Rectangle housing{trap.position.x - 19.0f, trap.position.y - 19.0f, 38.0f, 38.0f};
    DrawRectangleRec(housing, Color{48, 53, 56, 255});
    DrawRectangleLinesEx(housing, 3.0f, BLACK);

    Color warning{210, 62, 40, 255};
    DrawLineEx({housing.x + 4.0f, housing.y + 5.0f}, {housing.x + 13.0f, housing.y + 5.0f}, 3.0f, warning);
    DrawLineEx({housing.x + housing.width - 13.0f, housing.y + housing.height - 5.0f},
        {housing.x + housing.width - 4.0f, housing.y + housing.height - 5.0f}, 3.0f, warning);
    DrawCircleV({housing.x + 6.0f, housing.y + housing.height - 6.0f}, 2.5f, Color{145, 150, 151, 255});
    DrawCircleV({housing.x + housing.width - 6.0f, housing.y + 6.0f}, 2.5f, Color{145, 150, 151, 255});

    Vector2 throatLeft{
        trap.position.x + direction.x * 6.0f - normal.x * 11.0f,
        trap.position.y + direction.y * 6.0f - normal.y * 11.0f
    };
    Vector2 throatRight{
        trap.position.x + direction.x * 6.0f + normal.x * 11.0f,
        trap.position.y + direction.y * 6.0f + normal.y * 11.0f
    };
    Vector2 muzzleLeft{
        trap.position.x + direction.x * 24.0f - normal.x * 8.0f,
        trap.position.y + direction.y * 24.0f - normal.y * 8.0f
    };
    Vector2 muzzleRight{
        trap.position.x + direction.x * 24.0f + normal.x * 8.0f,
        trap.position.y + direction.y * 24.0f + normal.y * 8.0f
    };
    DrawSolidTriangle(throatLeft, throatRight, muzzleRight, BLACK);
    DrawSolidTriangle(throatLeft, muzzleRight, muzzleLeft, BLACK);
    DrawSolidTriangle(
        {throatLeft.x + direction.x * 2.0f + normal.x * 2.0f, throatLeft.y + direction.y * 2.0f + normal.y * 2.0f},
        {throatRight.x + direction.x * 2.0f - normal.x * 2.0f, throatRight.y + direction.y * 2.0f - normal.y * 2.0f},
        {muzzleRight.x - direction.x * 2.0f - normal.x * 2.0f, muzzleRight.y - direction.y * 2.0f - normal.y * 2.0f},
        Color{92, 99, 102, 255});
    DrawSolidTriangle(
        {throatLeft.x + direction.x * 2.0f + normal.x * 2.0f, throatLeft.y + direction.y * 2.0f + normal.y * 2.0f},
        {muzzleRight.x - direction.x * 2.0f - normal.x * 2.0f, muzzleRight.y - direction.y * 2.0f - normal.y * 2.0f},
        {muzzleLeft.x - direction.x * 2.0f + normal.x * 2.0f, muzzleLeft.y - direction.y * 2.0f + normal.y * 2.0f},
        Color{92, 99, 102, 255});
    DrawLineEx(muzzleLeft, muzzleRight, 6.0f, BLACK);

    Vector2 loadedTail{trap.position.x - direction.x * 10.0f, trap.position.y - direction.y * 10.0f};
    Vector2 loadedTip{trap.position.x + direction.x * 13.0f, trap.position.y + direction.y * 13.0f};
    DrawLineEx(loadedTail, loadedTip, 2.5f, Color{190, 131, 63, 255});
    DrawSolidTriangle(loadedTip,
        {loadedTip.x - direction.x * 7.0f + normal.x * 4.0f, loadedTip.y - direction.y * 7.0f + normal.y * 4.0f},
        {loadedTip.x - direction.x * 7.0f - normal.x * 4.0f, loadedTip.y - direction.y * 7.0f - normal.y * 4.0f},
        Color{205, 211, 211, 255});
}

void DrawArrowProjectile(const ArrowProjectile& arrow) {
    Vector2 center{
        arrow.rect.x + arrow.rect.width * 0.5f,
        arrow.rect.y + arrow.rect.height * 0.5f
    };
    Vector2 direction = NormalizeOr(arrow.velocity);
    Vector2 normal{-direction.y, direction.x};
    float halfLength = fmaxf(10.0f, fmaxf(arrow.rect.width, arrow.rect.height) * 0.5f);
    Vector2 tip{center.x + direction.x * halfLength, center.y + direction.y * halfLength};
    Vector2 tail{center.x - direction.x * halfLength, center.y - direction.y * halfLength};
    Vector2 headBase{tip.x - direction.x * 9.0f, tip.y - direction.y * 9.0f};

    DrawLineEx({tail.x - direction.x * 10.0f, tail.y - direction.y * 10.0f}, tail, 2.0f, Fade(RED, 0.28f));
    DrawLineEx(tail, headBase, 3.0f, Color{120, 76, 39, 255});
    DrawSolidTriangle(tip,
        {headBase.x + normal.x * 6.0f, headBase.y + normal.y * 6.0f},
        {headBase.x - normal.x * 6.0f, headBase.y - normal.y * 6.0f}, BLACK);
    DrawSolidTriangle({tip.x - direction.x * 2.0f, tip.y - direction.y * 2.0f},
        {headBase.x + direction.x * 1.5f + normal.x * 4.0f, headBase.y + direction.y * 1.5f + normal.y * 4.0f},
        {headBase.x + direction.x * 1.5f - normal.x * 4.0f, headBase.y + direction.y * 1.5f - normal.y * 4.0f},
        Color{183, 190, 191, 255});
    DrawSolidTriangle(tail,
        {tail.x - direction.x * 7.0f + normal.x * 5.0f, tail.y - direction.y * 7.0f + normal.y * 5.0f},
        {tail.x - direction.x * 1.0f + normal.x * 1.5f, tail.y - direction.y * 1.0f + normal.y * 1.5f},
        Color{162, 43, 34, 255});
    DrawSolidTriangle(tail,
        {tail.x - direction.x * 1.0f - normal.x * 1.5f, tail.y - direction.y * 1.0f - normal.y * 1.5f},
        {tail.x - direction.x * 7.0f - normal.x * 5.0f, tail.y - direction.y * 7.0f - normal.y * 5.0f},
        Color{162, 43, 34, 255});
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
