#include "LuaScripts.h"
#include "ScriptWorld.h"
#include <nlohmann/json.hpp>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>


struct LuaScripts::Impl {
    struct Instance {
        Impl* host{};
        lua_State* state{};
        std::string type, path;
        int index{}, module{LUA_NOREF}, self{LUA_NOREF};
        bool enabled{true};
        std::array<bool, 4> touching{};
        size_t memory{};
        ~Instance() { if (state) lua_close(state); }
        static void* Allocate(void* ud, void* ptr, size_t oldSize, size_t newSize) {
            auto& instance = *static_cast<Instance*>(ud);
            if (!ptr) oldSize = 0;
            if (newSize == 0) { std::free(ptr); instance.memory -= oldSize; return nullptr; }
            if (newSize > oldSize && newSize - oldSize > 8 * 1024 * 1024 - instance.memory) return nullptr;
            void* result = std::realloc(ptr, newSize);
            if (result) instance.memory = instance.memory - oldSize + newSize;
            return result;
        }
        void Error(const std::string& message) {
            enabled = false;
            host->log("Lua " + path + " (" + type + " " + std::to_string(index) + "): " + message);
        }
        bool Protected(int args, int results) {
            // A runaway callback becomes a normal Lua error instead of freezing playtest.
            lua_sethook(state, [](lua_State* L, lua_Debug*) { luaL_error(L, "instruction limit exceeded"); }, LUA_MASKCOUNT, 100000);
            const int status = lua_pcall(state, args, results, 0);
            lua_sethook(state, nullptr, 0, 0);
            if (status == LUA_OK) return true;
            const char* message = lua_type(state, -1) == LUA_TSTRING ? lua_tostring(state, -1) : nullptr;
            Error(message ? message : "unknown script error");
            lua_pop(state, 1);
            return false;
        }
        void Call(const char* name, float dt = 0, int playerIndex = 0) {
            if (!enabled) return;
            // Protect callback lookup and argument setup as well as execution:
            // script memory exhaustion must never reach Lua's panic handler.
            lua_pushcfunction(state, [](lua_State* L) -> int {
                auto& i = *static_cast<Instance*>(lua_touserdata(L, 1));
                const char* callback = static_cast<const char*>(lua_touserdata(L, 2));
                lua_rawgeti(L, LUA_REGISTRYINDEX, i.module);
                lua_pushstring(L, callback); lua_rawget(L, -2); lua_remove(L, -2);
                if (lua_isnil(L, -1)) return 0;
                if (!lua_isfunction(L, -1)) return luaL_error(L, "%s must be a function", callback);
                lua_rawgeti(L, LUA_REGISTRYINDEX, i.self);
                int args = 1;
                if (std::string(callback) == "on_update") { lua_pushvalue(L, 3); ++args; }
                if (lua_tointeger(L, 4) != 0) { lua_pushvalue(L, 4); ++args; }
                lua_call(L, args, 0);
                return 0;
            });
            lua_pushlightuserdata(state, this);
            lua_pushlightuserdata(state, const_cast<char*>(name));
            lua_pushnumber(state, dt); lua_pushinteger(state, playerIndex);
            Protected(4, 0);
        }
    };
    Level* level{};
    std::array<Player*, 4> players{};
    Logger log;
    std::function<void()> openExit;
    std::vector<std::unique_ptr<Instance>> instances;

    static Instance& Get(lua_State* L) { return *static_cast<Instance*>(lua_touserdata(L, lua_upvalueindex(1))); }
    static float Number(lua_State* L, int index) {
        const auto number = luaL_checknumber(L, index);
        if (!std::isfinite(number) || std::abs(number) > 1000000) luaL_error(L, "number must be finite and within +/- 1000000");
        return static_cast<float>(number);
    }
    static Player* PlayerAt(lua_State* L, Instance& instance, int arg) {
        const auto index = luaL_optinteger(L, arg, 1);
        if (index < 1 || index > 4) luaL_error(L, "player index must be 1..4");
        return instance.host->players[static_cast<size_t>(index - 1)];
    }
    static void RectTable(lua_State* L, Rectangle rect) {
        lua_newtable(L);
        for (auto pair : {std::pair<const char*, float>{"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}}) {
            lua_pushnumber(L, pair.second); lua_setfield(L, -2, pair.first);
        }
    }
    static int Bounds(lua_State* L) {
        auto& i = Get(L); Rectangle bounds;
        if (!ScriptTarget(*i.host->level, i.type, i.index, bounds)) return luaL_error(L, "attached record has no supported spatial target");
        RectTable(L, bounds); return 1;
    }
    static int Position(lua_State* L) {
        auto& i = Get(L); Rectangle bounds;
        const float x = Number(L, 2), y = Number(L, 3);
        if (!ScriptTarget(*i.host->level, i.type, i.index, bounds)) return luaL_error(L, "attached record has no supported spatial target");
        const Vector2 delta{x - bounds.x, y - bounds.y};
        ScriptTarget(*i.host->level, i.type, i.index, bounds, &delta); return 0;
    }
    static int ReadPlayer(lua_State* L) {
        auto& i = Get(L); auto* player = PlayerAt(L, i, 2);
        if (!player) { lua_pushnil(L); return 1; }
        RectTable(L, player->rect);
        lua_pushnumber(L, player->velocity.x); lua_setfield(L, -2, "vx");
        lua_pushnumber(L, player->velocity.y); lua_setfield(L, -2, "vy");
        lua_pushboolean(L, player->onGround); lua_setfield(L, -2, "on_ground");
        return 1;
    }
    static int Velocity(lua_State* L) {
        auto& i = Get(L); auto* player = PlayerAt(L, i, 2);
        const float x = Number(L, 3), y = Number(L, 4);
        if (player) player->velocity = {x, y};
        return 0;
    }
    static int Teleport(lua_State* L) {
        auto& i = Get(L); auto* player = PlayerAt(L, i, 2);
        const float x = Number(L, 3), y = Number(L, 4);
        if (player) { player->rect.x = x; player->rect.y = y; }
        return 0;
    }
    static int Touching(lua_State* L) {
        auto& i = Get(L); auto* player = PlayerAt(L, i, 2); Rectangle bounds;
        lua_pushboolean(L, player && ScriptTarget(*i.host->level, i.type, i.index, bounds) && ScriptOverlap(bounds, player->rect)); return 1;
    }
    static int OpenExit(lua_State* L) { Get(L).host->openExit(); return 0; }
    static int Log(lua_State* L) {
        auto& i = Get(L);
        const char* message = luaL_checkstring(L, 2);
        i.host->log("Lua " + i.path + ": " + message); return 0;
    }
    static int Print(lua_State* L) {
        auto& i = Get(L); std::string message;
        for (int arg = 1; arg <= lua_gettop(L); ++arg) {
            if (arg > 1) message += '\t';
            const char* value = lua_tostring(L, arg);
            message += value ? value : lua_typename(L, lua_type(L, arg));
        }
        i.host->log("Lua " + i.path + ": " + message); return 0;
    }
    void LoadFile(const std::string& type, int index, const std::filesystem::path& path) {
        auto item = std::make_unique<Instance>();
        item->host = this; item->type = type; item->index = index; item->path = path.u8string();
        auto& i = *item;
        i.state = lua_newstate(Instance::Allocate, &i);
        if (!i.state) { i.Error("cannot allocate Lua state"); return; }
        auto* L = i.state;
        for (const auto& lib : {std::pair<const char*, lua_CFunction>{"_G", luaopen_base},
            {LUA_MATHLIBNAME, luaopen_math}, {LUA_STRLIBNAME, luaopen_string},
            {LUA_TABLIBNAME, luaopen_table}, {LUA_UTF8LIBNAME, luaopen_utf8}}) {
            luaL_requiref(L, lib.first, lib.second, 1); lua_pop(L, 1);
        }
        // No filesystem, shell, native module or debug API in attached behaviors.
        // Removing protected calls prevents scripts swallowing the instruction hook error.
        for (const char* name : {"dofile", "loadfile", "load", "collectgarbage", "pcall", "xpcall", "setmetatable", "getmetatable"}) {
            lua_pushnil(L); lua_setglobal(L, name);
        }
        lua_pushlightuserdata(L, &i); lua_pushcclosure(L, Print, 1); lua_setglobal(L, "print");
        lua_newtable(L);
        lua_pushstring(L, type.c_str()); lua_setfield(L, -2, "type");
        lua_pushinteger(L, index); lua_setfield(L, -2, "index");
        for (const auto& method : {std::pair<const char*, lua_CFunction>{"get_bounds", Bounds},
            {"set_position", Position}, {"get_player", ReadPlayer}, {"set_player_velocity", Velocity},
            {"teleport_player", Teleport}, {"overlaps_player", Touching}, {"open_exit", OpenExit}, {"log", Log}}) {
            lua_pushlightuserdata(L, &i); lua_pushcclosure(L, method.second, 1); lua_setfield(L, -2, method.first);
        }
        i.self = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_newtable(L);
        i.module = luaL_ref(L, LUA_REGISTRYINDEX); // Reserve before running user code.
        std::ifstream file(path, std::ios::binary);
        if (!file) { i.Error("cannot open script file"); return; }
        file.seekg(0, std::ios::end);
        if (file.tellg() > 1024 * 1024) { i.Error("script exceeds 1 MB source limit"); return; }
        file.seekg(0);
        std::string source((std::istreambuf_iterator<char>(file)), {});
        if (source.size() > 1024 * 1024) { i.Error("script exceeds 1 MB source limit"); return; }
        if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) source.erase(0, 3);
        if (!source.empty() && source[0] == '#') {
            const auto end = source.find('\n');
            source = end == std::string::npos ? "\n" : source.substr(end);
        }
        if (luaL_loadbufferx(L, source.data(), source.size(), i.path.c_str(), "t") != LUA_OK) {
            i.Error(lua_tostring(L, -1)); return;
        }
        if (!i.Protected(0, 1)) return;
        if (!lua_istable(L, -1)) { i.Error("script must return a table of callbacks"); return; }
        lua_rawseti(L, LUA_REGISTRYINDEX, i.module);
        i.Call("on_start");
        instances.push_back(std::move(item));
    }
};

LuaScripts::LuaScripts() : impl(std::make_unique<Impl>()) {}
LuaScripts::~LuaScripts() = default;
void LuaScripts::Clear() { impl->instances.clear(); }
void LuaScripts::Load(Level& level, const std::string& levelPath, std::array<Player*, 4> players,
    Logger log, std::function<void()> openExit) {
    Clear(); impl->level = &level; impl->players = players;
    impl->log = std::move(log); impl->openExit = std::move(openExit);
    const auto root = std::filesystem::u8path(levelPath).parent_path();
    for (const auto& attachment : level.objectScripts) {
        const auto paths = nlohmann::json::parse(attachment.pathsJson, nullptr, false);
        if (!paths.is_array()) { impl->log("Lua: invalid objectScripts path array"); continue; }
        for (const auto& entry : paths) {
            if (!entry.is_string()) { impl->log("Lua: script path must be a string"); continue; }
            auto path = std::filesystem::u8path(entry.get<std::string>());
            auto extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".lua") continue; // C++ remains a compiled source attachment.
            if (path.is_relative()) path = root / path;
            impl->LoadFile(attachment.objectType, attachment.objectIndex, path.lexically_normal());
        }
    }
}
void LuaScripts::Update(float dt, std::array<Player*, 4> players) {
    impl->players = players;
    for (auto& item : impl->instances) {
        item->Call("on_update", dt);
        if (!item->enabled) continue;
        Rectangle bounds;
        if (!ScriptTarget(*impl->level, item->type, item->index, bounds)) continue;
        for (size_t p = 0; p < players.size(); ++p) {
            const bool touching = players[p] && ScriptOverlap(bounds, players[p]->rect);
            if (touching && !item->touching[p]) item->Call("on_touch", 0, static_cast<int>(p + 1));
            if (!touching && item->touching[p]) item->Call("on_leave", 0, static_cast<int>(p + 1));
            item->touching[p] = touching;
        }
    }
}
