#include "LuaScripts.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

void Check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
int main() {
    const auto folder = std::filesystem::temp_directory_path() / "power-pulley-lua-tests";
    std::filesystem::create_directories(folder);
    auto write = [&](const char* name, const char* source) { std::ofstream(folder / name) << source; };
    write("audio.level", "music \"audio/music track.ogg\"\nstoneBlock 10 20 32 32 1\nobjectSound stoneBlock 1 \"audio/hit sound.wav\"\n");
    const auto audioLevel = LoadLevelFromFile((folder / "audio.level").string(), Level{});
    Check(audioLevel.musicPath == (folder / "audio/music track.ogg").generic_string(), "Music resolves relative to its level");
    Check(audioLevel.objectSounds.size() == 1 && audioLevel.objectSounds[0].objectIndex == 1 &&
        audioLevel.objectSounds[0].path == (folder / "audio/hit sound.wav").generic_string(), "Object sound path and target load");

    write("tile behavior.lua", R"(
        assert(io == nil and os == nil and package == nil and debug == nil)
        return {
            on_start = function(self) self.counter = 0; self:log('start') end,
            on_update = function(self, dt)
                self.counter = self.counter + 1
                local b = self:get_bounds()
                self:set_position(b.x + dt * 10, b.y)
                self:log('tick' .. self.counter)
            end,
            on_touch = function(self, player)
                assert(self:overlaps_player(player))
                self:set_player_velocity(player, 100, -200)
                self:open_exit()
                self:log('touch')
            end,
            on_leave = function(self, player) self:log('leave') end
        }
    )");
    write("broken.lua", "return {on_update = function(self) error('deliberate failure') end}");
    write("loop.lua", "return {on_update = function(self) while true do end end}");
    write("syntax.lua", "this is not valid Lua");
    write("start-loop.lua", "while true do end");
    write("memory.lua", "return {on_update = function(self) local a = {}; while true do a[#a+1] = string.rep('x', 100000) end end}");
    write("object.lua", "\xEF\xBB\xBF" "#!/usr/bin/env lua\nreturn {on_start = function(self) local b = self:get_bounds(); self:set_position(b.x + 5, b.y + 7) end}");
    Level level;
    level.visualTiles.push_back({}); level.visualTiles.push_back({});
    level.visualTiles[1].position = {100, 0};
    level.stoneBlocks.push_back({{40, 50, 32, 32}});
    level.objectScripts = {
        {"visualTile", 1, R"(["tile behavior.lua", "unused.cpp", "broken.lua", "loop.lua", "syntax.lua", "missing.lua", "start-loop.lua", "memory.lua"])"},
        {"visualTile", 2, R"(["tile behavior.lua"])"},
        {"stoneBlock", 1, R"(["object.lua"])"},
        {"visualTile", 1, "invalid JSON"}
    };
    Player player; player.rect = {5, 5, 10, 10};
    LuaScripts scripts;
    std::vector<std::string> logs;
    int opened = 0;
    auto load = [&] { scripts.Load(level, (folder / "test.level").string(), {&player, nullptr, nullptr, nullptr},
        [&](const std::string& text) { logs.push_back(text); }, [&] { ++opened; }); };
    load();
    Check(level.stoneBlocks[0].rect.x == 45 && level.stoneBlocks[0].rect.y == 57, "Object start callback must change the live instance.");
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(level.visualTiles[0].position.x == 1 && level.visualTiles[1].position.x == 101, "Tile scripts must move their own runtime instances.");
    Check(player.velocity.x == 100 && player.velocity.y == -200 && opened == 1, "Touch callbacks must control the player and open the exit.");
    auto contains = [&](const std::string& fragment) { for (const auto& log : logs) if (log.find(fragment) != std::string::npos) return true; return false; };
    Check(contains("deliberate failure") && contains("instruction limit") && contains("cannot open") && contains("syntax.lua") && contains("invalid objectScripts") && contains("not enough memory"),
        "Errors, missing files, runaway scripts and memory limits must be contained and logged.");
    const auto errorsAfterFirstTick = logs.size();
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(opened == 1 && logs.size() == errorsAfterFirstTick + 2, "Touch must fire once on entry and failed scripts must stay disabled.");
    player.rect.x = 300;
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(contains("leave"), "Leaving a target must fire on_leave.");
    logs.clear(); load();
    scripts.Update(0.1f, {&player, nullptr, nullptr, nullptr});
    Check(contains("tick1") && !contains("tick4"), "Reload must reset per-instance script state.");
    scripts.Clear();
    const auto last = logs.size(); scripts.Update(0.1f, {});
    Check(logs.size() == last, "Clear must release scripts.");
    for (const auto* name : {"tile behavior.lua", "broken.lua", "loop.lua", "syntax.lua", "start-loop.lua", "memory.lua", "object.lua"}) std::filesystem::remove(folder / name);
    std::cout << "Lua runtime checks passed: lifecycle, isolation, tiles, objects, player actions, errors, budgets, reload.\n";
}
