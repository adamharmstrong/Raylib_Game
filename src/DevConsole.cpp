#include "DevConsole.h"

#include <algorithm>

void DevConsole::Toggle() {
    open = !open;
    historyIndex = -1;
}

bool DevConsole::IsOpen() const {
    return open;
}

void DevConsole::Update() {
    if (!open) return;

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) {
            input.push_back(static_cast<char>(key));
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !input.empty()) {
        input.pop_back();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        open = false;
        historyIndex = -1;
    }

    if (IsKeyPressed(KEY_UP) && !history.empty()) {
        if (historyIndex < 0) {
            historyIndex = static_cast<int>(history.size()) - 1;
        }
        else if (historyIndex > 0) {
            historyIndex--;
        }

        input = history[historyIndex];
    }

    if (IsKeyPressed(KEY_DOWN) && !history.empty()) {
        if (historyIndex >= 0 && historyIndex < static_cast<int>(history.size()) - 1) {
            historyIndex++;
            input = history[historyIndex];
        }
        else {
            historyIndex = -1;
            input.clear();
        }
    }

    if (IsKeyPressed(KEY_ENTER)) {
        submittedLine = input;
        hasSubmittedLine = true;

        if (!input.empty()) {
            history.push_back(input);
            if (history.size() > 50) {
                history.erase(history.begin());
            }
        }

        input.clear();
        historyIndex = -1;
    }
}

void DevConsole::Draw(int screenWidth, int screenHeight) const {
    if (!open) return;

    int height = screenHeight / 3;
    DrawRectangle(0, 0, screenWidth, height, Fade(BLACK, 0.82f));
    DrawRectangleLines(0, 0, screenWidth, height, Fade(WHITE, 0.45f));

    const int margin = 12;
    const int lineHeight = 20;
    const int promptHeight = 28;
    int maxOutputLines = std::max(1, (height - promptHeight - margin * 2) / lineHeight);
    int firstLine = std::max(0, static_cast<int>(outputLines.size()) - maxOutputLines);

    int y = margin;
    for (int i = firstLine; i < static_cast<int>(outputLines.size()); i++) {
        DrawText(outputLines[i].c_str(), margin, y, 18, RAYWHITE);
        y += lineHeight;
    }

    std::string prompt = "> " + input;
    bool showCursor = static_cast<int>(GetTime() * 2.0) % 2 == 0;
    if (showCursor) {
        prompt += "_";
    }

    DrawRectangle(0, height - promptHeight, screenWidth, promptHeight, Fade(BLACK, 0.88f));
    DrawText(prompt.c_str(), margin, height - promptHeight + 6, 18, GREEN);
}

bool DevConsole::ConsumeSubmittedLine(std::string& line) {
    if (!hasSubmittedLine) return false;

    line = submittedLine;
    submittedLine.clear();
    hasSubmittedLine = false;
    return true;
}

void DevConsole::AddLine(const std::string& line) {
    outputLines.push_back(line);
    if (outputLines.size() > 100) {
        outputLines.erase(outputLines.begin());
    }
}

void DevConsole::Clear() {
    outputLines.clear();
}
