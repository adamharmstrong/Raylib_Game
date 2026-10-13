#pragma once
#include "Level.h"
#include "Player.h"
#include <array>
#include <functional>
#include <memory>

// One isolated Lua state per attached source file and instance.
class LuaScripts {
public:
    using Logger = std::function<void(const std::string&)>;
    LuaScripts();
    ~LuaScripts();
    LuaScripts(const LuaScripts&) = delete;
    LuaScripts& operator=(const LuaScripts&) = delete;
    void Clear();
    void Load(Level& level, const std::string& levelPath,
        std::array<Player*, 4> players, Logger log, std::function<void()> openExit);
    void Update(float dt, std::array<Player*, 4> players);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
