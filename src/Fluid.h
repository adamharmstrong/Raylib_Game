#pragma once

#include "raylib.h"

#include <vector>

enum class FluidType {
    Water,
    Sand,
    Gel,
    Gas
};

enum class FluidSimulationMode {
    Advanced,
    Tile
};

struct FluidParticle {
    Vector2 position{};
    Vector2 previousPosition{};
    Vector2 velocity{};
    float density{0.0f};
    bool surface{false};
};

struct FluidCell {
    float mass{0.0f};
    Vector2 velocity{};
    bool solid{false};
};

struct GelBond {
    int first{-1};
    int second{-1};
    float restLength{0.0f};
    bool active{true};
};

struct FluidField {
    FluidType type{FluidType::Water};
    Rectangle bounds{};
    float particleSpacing{14.0f};
    float particleRadius{8.0f};
    float interactionRadius{28.0f};
    float initialFill{0.4f};
    float flowSpeed{1.0f};
    float accumulator{0.0f};
    unsigned long long simulationStep{0};
    bool initialized{false};
    std::vector<FluidParticle> particles;
    std::vector<GelBond> gelBonds;
    int gridColumns{0};
    int gridRows{0};
    float cellSize{7.0f};
    std::vector<FluidCell> cells;
};

struct FluidSample {
    float density{0.0f};
    Vector2 velocity{};
};

void InitializeFluidField(
    FluidField& field,
    const std::vector<Rectangle>& obstacles,
    FluidSimulationMode mode = FluidSimulationMode::Advanced
);
void UpdateFluidField(
    FluidField& field,
    const std::vector<Rectangle>& obstacles,
    const std::vector<Vector2>& externalFlow,
    float dt,
    FluidSimulationMode mode = FluidSimulationMode::Advanced
);
void AddFluidImpulse(FluidField& field, Vector2 point, float radius, Vector2 velocityChange);
float AddCellularFluidMass(FluidField& field, float amount, Vector2 velocity = {});

int GetFluidSimulationPointCount(const FluidField& field);
Vector2 GetFluidSimulationPoint(const FluidField& field, int index);
FluidSample SampleFluid(const FluidField& field, Vector2 point);
FluidSample SampleFluid(const std::vector<FluidField>& fields, FluidType type, Vector2 point);
float GetFluidMass(const FluidField& field);
