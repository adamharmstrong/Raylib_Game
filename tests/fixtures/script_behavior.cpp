#include "CppScript.h"
struct State { int ticks{}; const PppHost* host{}; };
static void* Create(const PppHost* host) { return new State{0, host}; }
static void Destroy(void* data) { auto* state = static_cast<State*>(data); state->host->log(state->host->context, "destroyed"); delete state; }
static void Start(void*, const PppHost* host) {
    PppRect rect{};
    if (host->get_bounds(host->context, &rect)) host->set_position(host->context, rect.x + 1, rect.y);
}
static void Update(void* data, const PppHost* host, float dt) {
    auto* state = static_cast<State*>(data);
    PppRect rect{}; host->get_bounds(host->context, &rect);
    host->set_position(host->context, rect.x + dt * 10, rect.y);
    if (++state->ticks == 1) host->log(host->context, "first tick");
}
static void Touch(void*, const PppHost* host, int index) {
    if (!host->overlaps_player(host->context, index)) return;
    PppPlayer player{};
    if (host->get_player(host->context, index, &player)) host->set_player_velocity(host->context, index, 100, -400);
    host->open_exit(host->context); host->log(host->context, "touched");
}
static void Leave(void*, const PppHost* host, int) { host->log(host->context, "left"); }
PPP_SCRIPT_EXPORT const PppScript* ppp_get_script(uint32_t version) {
    static const PppScript script{PPP_SCRIPT_ABI, sizeof(PppScript), Create, Destroy, Start, Update, Touch, Leave};
    return version == PPP_SCRIPT_ABI ? &script : nullptr;
}
