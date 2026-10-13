#pragma once
#include "LuaScripts.h"
class CppScripts {
public:
    CppScripts();
    ~CppScripts();
    void Clear();
    void Load(Level& level, const std::string& levelPath, std::array<Player*, 4> players,
        LuaScripts::Logger log, std::function<void()> openExit);
    void Update(float dt, std::array<Player*, 4> players);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
