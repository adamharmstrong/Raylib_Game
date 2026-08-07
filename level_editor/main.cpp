#include "LevelEditor.h"

#include <filesystem>

int main(int argc, char* argv[]) {
    std::filesystem::path initialLevel;
    if (argc > 1) {
        initialLevel = argv[1];
    }

    LevelEditor editor;
    return editor.Run(initialLevel);
}
