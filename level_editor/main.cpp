#include "LevelEditor.h"

#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    std::filesystem::path initialLevel;
    bool viewportOnly = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--viewport-only") viewportOnly = true;
        else initialLevel = argument;
    }

    LevelEditor editor;
    return editor.Run(initialLevel, viewportOnly);
}
