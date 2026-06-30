#pragma once

#include "raylib.h"

#include <string>
#include <vector>

class DevConsole {
public:
    void Toggle();
    bool IsOpen() const;

    void Update();
    void Draw(int screenWidth, int screenHeight) const;

    bool ConsumeSubmittedLine(std::string& line);
    void AddLine(const std::string& line);
    void Clear();

private:
    std::string input;
    std::string submittedLine;
    std::vector<std::string> outputLines;
    std::vector<std::string> history;

    bool open{false};
    bool hasSubmittedLine{false};
    int historyIndex{-1};
};
