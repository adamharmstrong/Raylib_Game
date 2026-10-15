#include "SpriteSheetTextures.h"
#include "Achievement.h"
#include "Level.h"
#include "LuaScripts.h"
#include "CppScripts.h"
#include "Player.h"
#include "DevConsole.h"
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#define private public
#include "Game.h"
#undef private

int main() {
    Game game;
    game.InitializeOverworld();
    const auto path = std::filesystem::temp_directory_path() / "custom-level-loading-test.level";
    { std::ofstream file(path); file << "script none\nplayerStart 120 240 2\n"; }
    game.LoadCustomLevel(path.string());
    if (game.customLevelPath != path.string() || !game.titleModeMenuOpen ||
        game.mode != GameMode::Title) return 1;

    // Canceling the picker preserves the selection; bad files do not replace it.
    game.LoadCustomLevel({});
    game.LoadCustomLevel(path.string() + ".missing");
    if (game.customLevelPath != path.string()) return 2;
    { std::ofstream file(path); file << "this is not a level\n"; }
    game.customLevelPath.clear();
    game.LoadCustomLevel(path.string());
    if (!game.customLevelPath.empty() || game.menuMessage.empty()) return 3;

    game.customLevelPath = path.string();
    game.mode = GameMode::Playing;
    game.won = true;
    game.CompleteCurrentLevelAndReturnToMap();
    if (game.mode != GameMode::Title || game.shouldQuit || game.won ||
        game.overworldNodes[0].completed || game.overworldNodes[1].unlocked) return 4;
    game.OpenOverworld();
    if (!game.customLevelPath.empty() || game.mode != GameMode::Overworld) return 5;

    game.playtestLevelPath = path.string();
    game.CompleteCurrentLevelAndReturnToMap();
    if (!game.shouldQuit) return 6;
    std::filesystem::remove(path);
    std::cout << "Custom level selection and completion passed.\n";
}
