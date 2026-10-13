#include "CppScript.h"

// Each attachment owns its own state; allocation and destruction stay in DLL.
struct State { int touches = 0; };
static void* Create(const PppHost*) { return new State; }
static void Destroy(void* state) { delete static_cast<State*>(state); }
static void OnTouch(void* state, const PppHost* host, int playerIndex) {
    PppPlayer player{};
    if (host->get_player(host->context, playerIndex, &player)) {
        host->set_player_velocity(host->context, playerIndex, player.vx, -650);
        ++static_cast<State*>(state)->touches;
        host->log(host->context, "C++ bounce activated");
    }
}
PPP_SCRIPT_EXPORT const PppScript* ppp_get_script(uint32_t version) {
    static const PppScript script{PPP_SCRIPT_ABI, sizeof(PppScript), Create, Destroy,
        nullptr, nullptr, OnTouch, nullptr};
    return version == PPP_SCRIPT_ABI ? &script : nullptr;
}
