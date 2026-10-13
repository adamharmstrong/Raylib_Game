#include "CppScripts.h"
#include "ScriptWorld.h"
#include "CppScript.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cmath>
#include <algorithm>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef ShowCursor
#undef CloseWindow
#undef Rectangle
#endif

struct CppScripts::Impl {
    struct Instance {
        Impl* owner{};
        std::string type, path;
        int index{};
        PppHost host{};
        const PppScript* script{};
        void* data{};
        std::array<bool, 4> touching{};
#if defined(_WIN32)
        HMODULE module{};
#endif
        ~Instance() {
            if (script && script->destroy) script->destroy(data);
#if defined(_WIN32)
            if (module) FreeLibrary(module);
#endif
        }
    };
    Level* level{};
    std::array<Player*, 4> players{};
    LuaScripts::Logger log;
    std::function<void()> openExit;
    std::vector<std::unique_ptr<Instance>> instances;
    static Instance& Get(void* p) { return *static_cast<Instance*>(p); }
    static bool Valid(float x, float y) { return std::isfinite(x) && std::isfinite(y) && std::abs(x) <= 1000000 && std::abs(y) <= 1000000; }
    static Player* PlayerAt(Instance& i, int index) { return index >= 1 && index <= 4 ? i.owner->players[index - 1] : nullptr; }
    static int Bounds(void* p, PppRect* result) {
        auto& i = Get(p); Rectangle r;
        if (!result || !ScriptTarget(*i.owner->level, i.type, i.index, r)) return 0;
        *result = {r.x, r.y, r.width, r.height}; return 1;
    }
    static int Position(void* p, float x, float y) {
        auto& i = Get(p); Rectangle r;
        if (!Valid(x, y) || !ScriptTarget(*i.owner->level, i.type, i.index, r)) return 0;
        const Vector2 delta{x - r.x, y - r.y};
        return ScriptTarget(*i.owner->level, i.type, i.index, r, &delta);
    }
    static int ReadPlayer(void* p, int index, PppPlayer* result) {
        auto* player = PlayerAt(Get(p), index);
        if (!player || !result) return 0;
        const auto& r = player->rect;
        *result = {{r.x, r.y, r.width, r.height}, player->velocity.x, player->velocity.y, player->onGround}; return 1;
    }
    static int Velocity(void* p, int index, float x, float y) {
        auto* player = PlayerAt(Get(p), index);
        if (!player || !Valid(x, y)) return 0;
        player->velocity = {x, y}; return 1;
    }
    static int Teleport(void* p, int index, float x, float y) {
        auto* player = PlayerAt(Get(p), index);
        if (!player || !Valid(x, y)) return 0;
        player->rect.x = x; player->rect.y = y; return 1;
    }
    static int Touching(void* p, int index) {
        auto& i = Get(p); auto* player = PlayerAt(i, index); Rectangle r;
        return player && ScriptTarget(*i.owner->level, i.type, i.index, r) && ScriptOverlap(r, player->rect);
    }
    static void OpenExit(void* p) { Get(p).owner->openExit(); }
    static void Log(void* p, const char* message) { auto& i = Get(p); if (message) i.owner->log("C++ " + i.path + ": " + message); }
    void LoadFile(const std::string& type, int index, const std::filesystem::path& source) {
        auto i = std::make_unique<Instance>();
        i->owner = this; i->type = type; i->index = index;
        auto dll = source; dll += ".dll"; i->path = dll.u8string();
#if defined(_WIN32)
        i->module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!i->module) { log("C++ " + i->path + ": cannot load plugin (Windows error " + std::to_string(GetLastError()) + "). Use Build Scripts."); return; }
        const auto entry = reinterpret_cast<PppGetScript>(GetProcAddress(i->module, "ppp_get_script"));
        if (!entry) { log("C++ " + i->path + ": missing ppp_get_script export"); return; }
        i->script = entry(PPP_SCRIPT_ABI);
        if (!i->script || i->script->abi_version != PPP_SCRIPT_ABI || i->script->struct_size != sizeof(PppScript)) {
            i->script = nullptr; log("C++ " + i->path + ": incompatible plugin ABI"); return;
        }
        i->host = {PPP_SCRIPT_ABI, sizeof(PppHost), i.get(), i->type.c_str(), index,
            Bounds, Position, ReadPlayer, Velocity, Teleport, Touching, OpenExit, Log};
        if (i->script->create) i->data = i->script->create(&i->host);
        if (i->script->on_start) i->script->on_start(i->data, &i->host);
        instances.push_back(std::move(i));
#else
        log("C++ plugins require the Windows desktop game; Lua is available on other targets.");
#endif
    }
};
CppScripts::CppScripts() : impl(std::make_unique<Impl>()) {}
CppScripts::~CppScripts() = default;
void CppScripts::Clear() { impl->instances.clear(); }
void CppScripts::Load(Level& level, const std::string& levelPath, std::array<Player*, 4> players,
    LuaScripts::Logger log, std::function<void()> openExit) {
    Clear(); impl->level = &level; impl->players = players; impl->log = std::move(log); impl->openExit = std::move(openExit);
    const auto root = std::filesystem::absolute(std::filesystem::u8path(levelPath)).parent_path();
    for (const auto& attachment : level.objectScripts) {
        const auto paths = nlohmann::json::parse(attachment.pathsJson, nullptr, false);
        if (!paths.is_array()) continue;
        for (const auto& entry : paths) {
            if (!entry.is_string()) continue;
            auto source = std::filesystem::u8path(entry.get<std::string>());
            auto ext = source.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".cpp" && ext != ".cc" && ext != ".cxx") continue;
            if (source.is_relative()) source = root / source;
            impl->LoadFile(attachment.objectType, attachment.objectIndex, source.lexically_normal());
        }
    }
}
void CppScripts::Update(float dt, std::array<Player*, 4> players) {
    impl->players = players;
    for (auto& i : impl->instances) {
        if (i->script->on_update) i->script->on_update(i->data, &i->host, dt);
        Rectangle r;
        if (!ScriptTarget(*impl->level, i->type, i->index, r)) continue;
        for (size_t p = 0; p < players.size(); ++p) {
            const bool touch = players[p] && ScriptOverlap(r, players[p]->rect);
            if (touch && !i->touching[p] && i->script->on_touch) i->script->on_touch(i->data, &i->host, static_cast<int>(p + 1));
            if (!touch && i->touching[p] && i->script->on_leave) i->script->on_leave(i->data, &i->host, static_cast<int>(p + 1));
            i->touching[p] = touch;
        }
    }
}
