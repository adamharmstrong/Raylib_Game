#include "Level.h"

#include <filesystem>
#include <iostream>
#include <string>

int main() {
    int levelCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(LEVEL_DIRECTORY)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".level") continue;
        ++levelCount;
        const std::string author = LoadLevelAuthorFromFile(entry.path().string(), {});
        if (author.empty() || author == "Unknown") {
            std::cerr << entry.path().filename().string() << " has no author metadata.\n";
            return 1;
        }
        const Level level = LoadLevelFromFile(entry.path().string(), {});
        if (level.author != author) {
            std::cerr << entry.path().filename().string() << " did not preserve its author metadata.\n";
            return 1;
        }
    }
    if (levelCount == 0) {
        std::cerr << "No levels were found for author validation.\n";
        return 1;
    }
    if (std::filesystem::exists(std::filesystem::path(LEVEL_DIRECTORY) / "wendis_level_3.level")) {
        std::cerr << "Kiril's level still uses Wendi's filename.\n";
        return 1;
    }
    if (LoadLevelAuthorFromFile(
            (std::filesystem::path(LEVEL_DIRECTORY) / "kirils_level_3.level").string(), {}) != "Kiril") {
        std::cerr << "Kiril's renamed level is missing its credit.\n";
        return 1;
    }

    std::cout << "Validated author metadata for " << levelCount << " levels.\n";
    return 0;
}
