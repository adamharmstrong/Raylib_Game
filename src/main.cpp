#include "Game.h"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string playtestPath;
    if (argc > 1) {
        if (argc != 3 || std::string(argv[1]) != "--playtest" ||
            !std::filesystem::is_regular_file(argv[2])) {
            std::cerr << "Usage: RaylibGame --playtest <existing level file>\n";
            return 1;
        }
        playtestPath = argv[2];
    }
    Game game;
    game.Run(playtestPath);
    return 0;
}
