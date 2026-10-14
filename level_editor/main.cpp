#include "LevelEditor.h"

#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    std::filesystem::path initialLevel;
    bool viewportOnly = false;
    std::uintptr_t viewportParent = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--viewport-only") viewportOnly = true;
        else if (argument == "--viewport-parent" && index + 1 < argc)
            viewportParent = static_cast<std::uintptr_t>(std::stoull(argv[++index], nullptr, 0));
        else initialLevel = argument;
    }

    return RunLevelEditor(initialLevel, viewportOnly, viewportParent);
}
