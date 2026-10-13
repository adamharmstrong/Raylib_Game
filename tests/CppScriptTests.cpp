#include "CppScripts.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    std::string source = CPP_FIXTURE_PATH;
    source.resize(source.size() - 4); // Source attachment resolves to <source>.dll.
    std::replace(source.begin(), source.end(), '\\', '/');
    Level level;
    level.visualTiles.resize(2); level.visualTiles[1].position = {100, 0};
    level.objectScripts = {{"visualTile", 1, "[\"" + source + "\"]"}, {"visualTile", 2, "[\"" + source + "\"]"},
        {"visualTile", 1, "[\"missing.cpp\"]"}};
    Player player; player.rect = {5, 5, 10, 10};
    std::vector<std::string> messages;
    int opened = 0;
    CppScripts scripts;
    auto load = [&] { scripts.Load(level, "test.level", {&player, nullptr, nullptr, nullptr},
        [&](const std::string& message) { messages.push_back(message); }, [&] { ++opened; }); };
    auto count = [&](const std::string& text) { return std::count_if(messages.begin(), messages.end(), [&](const auto& m) { return m.find(text) != std::string::npos; }); };
    load();
    Check(level.visualTiles[0].position.x == 1 && level.visualTiles[1].position.x == 101, "Native start callbacks must target independent instances.");
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(level.visualTiles[0].position.x == 2 && level.visualTiles[1].position.x == 102 && count("first tick") == 2, "Native state and motion must be per instance.");
    Check(opened == 1 && player.velocity.y == -400 && count("cannot load plugin") == 1, "Player actions, exit unlock and missing-plugin diagnostics must work.");
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(opened == 1, "Native touch callback must fire once on entry.");
    player.rect.x = 300; scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(count("left") == 1, "Native leave callback must fire once.");
    load();
    Check(count("destroyed") == 2, "Reset must destroy plugin instance data before unloading libraries.");
    scripts.Clear(); Check(count("destroyed") == 4, "Clear must destroy every native instance exactly once.");
    std::cout << "C++ plugin checks passed: load, lifecycle, isolation, host actions, missing DLL, reset, unload.\n";
}
