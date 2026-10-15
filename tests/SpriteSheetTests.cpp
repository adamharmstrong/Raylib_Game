#include "Level.h"
#include "Collision.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "power-pulley-sprite-sheet-test.level";
    {
        std::ofstream file(path);
        file << "spriteSheet 3 C:/Sprite Sheets/custom image.png\n"
             << "tileLayer collision\ntileLayer user2\ntileLayer user1\ntileLayerName user1 Machinery Front\ntileLayerAbove user2\n"
             << "spriteSheet 5 C:/Sprite Sheets/second image.png\n"
             << "spriteSheet 9999 invalid.png\n"
             << "objectScripts visualTile 1 [\"../scripts/tile behavior.cpp\",\"../scripts/extra.cpp\"]\n"
             << "objectScripts visualTile 0 []\n"
             << "tilePassage 3 2 1 1 24\n"
             << "tilePassage 3 4 5 3\n"
             << "tilePassage 9999 0 0 15\n"
             << "visualTile foreground 2 1 32 64 0 0 0 1 0.12 3\n"
             << "visualTile background 0 0 64 64\n"
             << "visualTile middleground 1 2 96 64\n";
    }
    const Level level = LoadLevelFromFile(path.string(), {});
    std::filesystem::remove(path);
    if (level.objectScripts.size() != 1 || level.objectScripts[0].objectType != "visualTile" ||
        level.objectScripts[0].objectIndex != 1 ||
        level.objectScripts[0].pathsJson != "[\"../scripts/tile behavior.cpp\",\"../scripts/extra.cpp\"]")
        throw std::runtime_error("C++ script source references must load without losing paths.");
    const auto order = GetTileLayerDrawOrder(level);
    const auto foreground = std::find(order.begin(), order.end(), TileLayer::Foreground);
    const auto middle = std::find(order.begin(), order.end(), TileLayer::Middleground);
    const auto background = std::find(order.begin(), order.end(), TileLayer::Background);
    if (order.size() != 13 || !(background < middle && middle < foreground) || order.front() != TileLayer::User1 ||
        order.back() != TileLayer::User2 || level.customTileLayerNames[0] != "Machinery Front")
        throw std::runtime_error("Layer names and custom stacking order must load without moving the default layers.");
    if (level.spriteSheetPaths.size() != 3 || level.spriteSheetPaths[0] != "C:/Sprite Sheets/custom image.png" ||
        !level.spriteSheetPaths[1].empty() || level.spriteSheetPaths[2] != "C:/Sprite Sheets/second image.png" ||
        level.visualTiles.size() != 3 || level.visualTiles[0].sheetIndex != 3 || level.visualTiles[1].sheetIndex != 1 ||
        level.visualTiles[2].layer != TileLayer::Middleground || static_cast<int>(TileLayer::User1) != 4)
        throw std::runtime_error("Custom sheet paths, sparse IDs, or legacy tile defaults were lost.");
    if (level.tilePassages.size() != 2 || GetTileBlockedFaces(level, level.visualTiles[0]) != 1 ||
        GetTilePassageLineWidth(level, level.visualTiles[0]) != 24)
        throw std::runtime_error("Passage settings did not load.");
    auto legacyPassageTile = level.visualTiles[0];
    legacyPassageTile.column = 4;
    legacyPassageTile.row = 5;
    if (GetTilePassageLineWidth(level, legacyPassageTile) != 50)
        throw std::runtime_error("Legacy passage width default changed.");
    auto tile = level.visualTiles[0];
    tile.quarterTurns = 1;
    if (GetTileBlockedFaces(level, tile) != 2) throw std::runtime_error("Rotated passage face is incorrect.");
    tile.quarterTurns = 0; tile.flipY = true;
    if (GetTileBlockedFaces(level, tile) != 4) throw std::runtime_error("Flipped passage face is incorrect.");
    Player player;
    Level movementLevel = level;
    movementLevel.visualTiles[0].layer = TileLayer::Middleground;
    player.rect = {32, 50, 16, 20}; player.velocity = {0, 40};
    ResolveTilePassage(player, movementLevel, {32, 40, 16, 20}, false);
    if (player.rect.y != 44 || !player.onGround || player.velocity.y != 0)
        throw std::runtime_error("Top face did not catch a falling player.");
    player.rect = {32, 70, 16, 20}; player.velocity = {0, -40}; player.onGround = false;
    ResolveTilePassage(player, movementLevel, {32, 100, 16, 20}, false);
    if (player.rect.y != 70 || player.velocity.y != -40)
        throw std::runtime_error("Top-only tile blocked movement from below.");
    Level passageLevel;
    tile = level.visualTiles[0]; passageLevel.visualTiles.push_back(tile);
    passageLevel.tilePassages.push_back({3, 2, 1, 15});
    const auto solids = BuildSolids(passageLevel, WorldLayer::Foreground);
    if (std::none_of(solids.begin(), solids.end(), [](Rectangle r) { return r.x == 32 && r.y == 64 && r.width == 32; }))
        throw std::runtime_error("Solid visual tile did not enter shared physics colliders.");
    passageLevel.visualTiles[0].layer = TileLayer::Collision;
    passageLevel.tilePassages[0].blockedFaces = 0;
    if (GetTileBlockedFaces(passageLevel, passageLevel.visualTiles[0]) != 0)
        throw std::runtime_error("Explicit passable override was ignored.");
    passageLevel.tilePassages.clear();
    if (GetTileBlockedFaces(passageLevel, passageLevel.visualTiles[0]) != 15)
        throw std::runtime_error("Legacy collision layer default changed.");
    passageLevel.tilePassages.push_back({3, 2, 1, 8});
    player.rect = {25, 64, 16, 20}; player.velocity = {40, 0};
    ResolveTilePassage(player, passageLevel, {10, 64, 16, 20}, true);
    if (player.rect.x != 16 || player.velocity.x != 0)
        throw std::runtime_error("Left face did not stop a player approaching from the left.");
    passageLevel.tilePassages[0].blockedFaces = 2;
    player.rect = {55, 64, 16, 20}; player.velocity = {-40, 0};
    ResolveTilePassage(player, passageLevel, {70, 64, 16, 20}, true);
    if (player.rect.x != 64 || player.velocity.x != 0)
        throw std::runtime_error("Right face did not stop a player approaching from the right.");
    passageLevel.tilePassages[0].blockedFaces = 4;
    player.rect = {32, 90, 16, 20}; player.velocity = {0, -40};
    ResolveTilePassage(player, passageLevel, {32, 100, 16, 20}, false);
    if (player.rect.y != 96 || player.velocity.y != 0)
        throw std::runtime_error("Bottom face did not stop a player approaching from below.");
    for (const TileLayer tileLayer : {TileLayer::Background, TileLayer::Middleground, TileLayer::Foreground,
        TileLayer::Collision, TileLayer::FarBackground, TileLayer::User1}) {
        for (const WorldLayer bodyLayer : {WorldLayer::Background, WorldLayer::Middleground, WorldLayer::Foreground}) {
            passageLevel.visualTiles[0].layer = tileLayer;
            passageLevel.tilePassages[0].blockedFaces = 15;
            const bool expected = tileLayer == TileLayer::Collision ||
                (tileLayer == TileLayer::Background && bodyLayer == WorldLayer::Background) ||
                (tileLayer == TileLayer::Middleground && bodyLayer == WorldLayer::Middleground) ||
                (tileLayer == TileLayer::Foreground && bodyLayer == WorldLayer::Foreground);
            if (!BuildSolids(passageLevel, bodyLayer).empty() != expected)
                throw std::runtime_error("Solid tile layer isolation failed.");
            passageLevel.tilePassages[0].blockedFaces = 1;
            std::vector<Rectangle> directional;
            AppendDirectionalTileSolids(directional, passageLevel, bodyLayer, {32, 40, 16, 20}, {0, 40}, false);
            if (!directional.empty() != expected)
                throw std::runtime_error("Directional tile layer isolation failed.");
            directional.clear();
            AppendDirectionalTileSolids(directional, passageLevel, bodyLayer, {32, 100, 16, 20}, {0, -40}, false);
            if (!directional.empty()) throw std::runtime_error("Top-only tile blocked movement from below.");
        }
    }
    player.rect = {32, 50, 16, 20}; player.velocity = {0, 40}; player.onGround = false;
    ResolveTilePassage(player, level, {32, 40, 16, 20}, false);
    if (player.rect.y != 50 || player.velocity.y != 40)
        throw std::runtime_error("Middleground player collided with foreground passage.");
    passageLevel.visualTiles[0].layer = TileLayer::Foreground;
    passageLevel.tilePassages[0].blockedFaces = 1;
    for (const WorldLayer bodyLayer : {WorldLayer::Background, WorldLayer::Foreground}) {
        GuideObject ball;
        std::istringstream arguments("40 50 8 1");
        if (!ParseGuideObject("ball", arguments, ball)) throw std::runtime_error("Ball fixture could not load.");
        ball.layer = bodyLayer;
        ball.body.velocity = {0, 40};
        ball.collider.restitution = 0;
        std::vector<GuideObject> objects{ball};
        std::array<std::vector<Rectangle>, WorldLayerCount> worldSolids{
            BuildSolids(passageLevel, WorldLayer::Background), BuildSolids(passageLevel),
            BuildSolids(passageLevel, WorldLayer::Foreground)};
        Vector2 checkpoint{};
        UpdateGuideObjects(objects, {nullptr, nullptr, nullptr, nullptr}, worldSolids, 0, 0.25f,
            checkpoint, nullptr, FluidSimulationMode::Advanced, &passageLevel);
        if (objects[0].body.onGround != (bodyLayer == WorldLayer::Foreground))
            throw std::runtime_error("Dynamic object did not respect directional tile layer isolation.");
    }
    std::cout << "Sprite sheet, tile passage, and layer isolation checks passed.\n";
}
