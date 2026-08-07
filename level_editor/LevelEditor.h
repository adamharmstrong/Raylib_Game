#pragma once

#include "Level.h"

#include <filesystem>
#include <string>
#include <vector>

class LevelEditor {
public:
    int Run(const std::filesystem::path& initialLevel = {});

private:
    void DiscoverLevels();
    bool LoadLevel(const std::filesystem::path& path);
    void FrameLevel();
    void Update();
    void Draw() const;
    void DrawCanvas(Rectangle canvas) const;
    void DrawLevelPreview() const;
    void DrawFilePanel() const;
    void DrawInspector() const;
    void DrawToolbar() const;
    void DrawStatusBar() const;

    Rectangle GetCanvasBounds() const;
    Rectangle GetFitButtonBounds() const;
    Rectangle GetReloadButtonBounds() const;
    std::filesystem::path FindLevelDirectory() const;
    int VisibleFileRows() const;

    Level level{};
    Camera2D camera{};
    Font uiFont{};
    std::filesystem::path levelDirectory;
    std::filesystem::path currentLevelPath;
    std::vector<std::filesystem::path> levelFiles;
    std::string statusText{"Drop a .level file here or choose one from the list."};
    int fileScroll{0};
    bool ownsUiFont{false};
};
