#pragma once
#include <stdint.h>
#if defined(_WIN32) && !defined(_WIN64)
#error C++ script plugins require a 64-bit compiler.
#endif

// Versioned C ABI: no engine headers, STL types or shared allocations.
#define PPP_SCRIPT_ABI 1u
#if defined(_WIN32)
#define PPP_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#define PPP_SCRIPT_EXPORT extern "C" __attribute__((visibility("default")))
#endif
struct PppRect { float x, y, width, height; };
struct PppPlayer { PppRect bounds; float vx, vy; int on_ground; };
struct PppHost {
    uint32_t abi_version, struct_size;
    void* context;
    const char* object_type;
    int object_index;
    int (*get_bounds)(void*, PppRect*);
    int (*set_position)(void*, float, float);
    int (*get_player)(void*, int, PppPlayer*);
    int (*set_player_velocity)(void*, int, float, float);
    int (*teleport_player)(void*, int, float, float);
    int (*overlaps_player)(void*, int);
    void (*open_exit)(void*);
    void (*log)(void*, const char*);
};
struct PppScript {
    uint32_t abi_version, struct_size;
    void* (*create)(const PppHost*);
    void (*destroy)(void*);
    void (*on_start)(void*, const PppHost*);
    void (*on_update)(void*, const PppHost*, float);
    void (*on_touch)(void*, const PppHost*, int);
    void (*on_leave)(void*, const PppHost*, int);
};
// Export this exact entry point. All callbacks are optional; they must not throw.
// create/destroy own per-instance state within the plugin's allocator.
typedef const PppScript* (*PppGetScript)(uint32_t);
