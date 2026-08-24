#include "Fluid.h"
#include "Level.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    FluidField gas{};
    gas.type = FluidType::Gas;
    gas.bounds = {0.0f, 0.0f, 200.0f, 200.0f};
    gas.particleSpacing = 20.0f;
    gas.initialFill = 0.0f;
    gas.flowSpeed = 3.0f;

    const std::vector<Rectangle> obstacles{{90.0f, 0.0f, 20.0f, 150.0f}};
    InitializeFluidField(gas, obstacles, FluidSimulationMode::Advanced);
    if (!gas.initialized || gas.gridColumns != 10 || gas.gridRows != 10 ||
        gas.cells.size() != 100 || !gas.particles.empty()) {
        std::cerr << "Gas did not initialize as a 20-pixel concentration grid.\n";
        return 1;
    }

    const float emitted = EmitGasDensity(gas, {30.0f, 170.0f}, {95.0f, -90.0f}, 40.0f);
    if (fabsf(emitted - 40.0f) > 0.001f || fabsf(GetFluidMass(gas) - 40.0f) > 0.01f) {
        std::cerr << "Gas source did not preserve emitted concentration mass.\n";
        return 1;
    }

    std::vector<Vector2> wind(static_cast<size_t>(GetFluidSimulationPointCount(gas)));
    for (int step = 0; step < 180; step++) {
        UpdateFluidField(gas, obstacles, wind, 1.0f / 60.0f, FluidSimulationMode::Advanced);
    }
    if (fabsf(GetFluidMass(gas) - 40.0f) > 0.15f) {
        std::cerr << "Tile advection or diffusion did not conserve gas mass.\n";
        return 1;
    }

    int occupiedCells = 0;
    int highestOccupiedRow = gas.gridRows;
    int densestIndex = -1;
    float densestMass = 0.0f;
    for (int index = 0; index < static_cast<int>(gas.cells.size()); index++) {
        const FluidCell& cell = gas.cells[index];
        if (cell.solid || cell.mass <= 0.01f) continue;
        occupiedCells++;
        highestOccupiedRow = std::min(highestOccupiedRow, index / gas.gridColumns);
        if (cell.mass > densestMass) {
            densestMass = cell.mass;
            densestIndex = index;
        }
    }
    if (occupiedCells < 12 || highestOccupiedRow >= 7 || densestIndex < 0) {
        std::cerr << "Gas did not expand upward into a connected tile volume.\n";
        return 1;
    }

    const Vector2 samplePoint = GetFluidSimulationPoint(gas, densestIndex);
    if (SampleFluid(gas, samplePoint).density <= 0.05f) {
        std::cerr << "Gas sampling did not read the concentration grid.\n";
        return 1;
    }

    const Rectangle outlet{samplePoint.x - 10.0f, samplePoint.y - 10.0f, 20.0f, 20.0f};
    const float beforeVent = GetFluidMass(gas);
    const float removed = VentGasDensity(gas, outlet, {1.0f, 0.0f}, 100.0f, 180.0f, 0.5f, 1.0f / 60.0f);
    if (removed <= 0.0f || fabsf(GetFluidMass(gas) - (beforeVent - removed)) > 0.02f) {
        std::cerr << "Gas outlet did not remove tile concentration mass.\n";
        return 1;
    }

    Level annex = LoadLevelFromFile(NEUROTOXIN_LEVEL_PATH, {});
    if (annex.toxinLeak.fluidIndex < 0 ||
        annex.toxinLeak.fluidIndex >= static_cast<int>(annex.fluids.size())) {
        std::cerr << "Neurotoxin Annex did not load its gas field.\n";
        return 1;
    }
    if (annex.enemies.size() != 5) {
        std::cerr << "Neurotoxin Annex did not load its five security robots.\n";
        return 1;
    }
    const auto gasMask = std::find_if(
        annex.guideObjects.begin(),
        annex.guideObjects.end(),
        [](const GuideObject& object) { return object.type == GuideObjectType::GasMask; }
    );
    if (gasMask == annex.guideObjects.end() || gasMask->collected || gasMask->collider.radius < 10.0f) {
        std::cerr << "Neurotoxin Annex did not load its collectible gas mask.\n";
        return 1;
    }
    for (const Enemy& enemy : annex.enemies) {
        const float enemyBottom = enemy.rect.y + enemy.rect.height;
        const bool supported = std::any_of(
            annex.baseSolids.begin(),
            annex.baseSolids.end(),
            [&](Rectangle solid) {
                const bool horizontalOverlap =
                    enemy.rect.x < solid.x + solid.width &&
                    enemy.rect.x + enemy.rect.width > solid.x;
                return horizontalOverlap && fabsf(enemyBottom - solid.y) <= 0.01f;
            }
        );
        if (!supported || enemy.rect.x < enemy.patrolMinX ||
            enemy.rect.x + enemy.rect.width > enemy.patrolMaxX) {
            std::cerr << "A Neurotoxin Annex robot has an invalid initial patrol placement.\n";
            return 1;
        }
    }
    const Rectangle exitFrame{
        annex.exitTrigger.x - 4.0f,
        annex.exitTrigger.y - 8.0f,
        annex.exitTrigger.width + 8.0f,
        annex.exitTrigger.height + 8.0f
    };
    for (Rectangle solid : annex.baseSolids) {
        if (CheckCollisionRecs(exitFrame, solid)) {
            std::cerr << "Neurotoxin Annex exit frame intersects surrounding floor geometry.\n";
            return 1;
        }
    }
    if (fabsf(exitFrame.y - 682.0f) > 0.01f ||
        fabsf(exitFrame.y + exitFrame.height - 850.0f) > 0.01f) {
        std::cerr << "Neurotoxin Annex exit frame does not meet both surrounding floors.\n";
        return 1;
    }
    FluidField& annexGas = annex.fluids[annex.toxinLeak.fluidIndex];
    const std::vector<Rectangle> annexObstacles = BuildSolids(annex);
    InitializeFluidField(annexGas, annexObstacles, FluidSimulationMode::Advanced);
    std::vector<Vector2> annexWind(static_cast<size_t>(GetFluidSimulationPointCount(annexGas)));
    float tenSecondMass = 0.0f;
    float tenSecondMassAboveFirstTier = 0.0f;
    for (int step = 0; step < 1200; step++) {
        const float remaining = fmaxf(0.0f, annex.toxinLeak.maximumMass - GetFluidMass(annexGas));
        EmitGasDensity(
            annexGas,
            annex.toxinLeak.source,
            {48.0f, -42.0f},
            fminf(annex.toxinLeak.massPerSecond / 60.0f, remaining)
        );
        UpdateFluidField(
            annexGas,
            annexObstacles,
            annexWind,
            1.0f / 60.0f,
            FluidSimulationMode::Advanced
        );
        if (step == 599) {
            tenSecondMass = GetFluidMass(annexGas);
            for (int index = 0; index < static_cast<int>(annexGas.cells.size()); index++) {
                const Vector2 center = GetFluidSimulationPoint(annexGas, index);
                if (!annexGas.cells[index].solid && center.y < 650.0f) {
                    tenSecondMassAboveFirstTier += annexGas.cells[index].mass;
                }
            }
        }
    }
    const float annexMass = GetFluidMass(annexGas);
    float massAboveFirstTier = 0.0f;
    float peakMass = 0.0f;
    int rightmostLowerColumn = -1;
    for (int index = 0; index < static_cast<int>(annexGas.cells.size()); index++) {
        const Vector2 center = GetFluidSimulationPoint(annexGas, index);
        peakMass = fmaxf(peakMass, annexGas.cells[index].mass);
        if (!annexGas.cells[index].solid && annexGas.cells[index].mass > 0.01f && center.y > 682.0f) {
            rightmostLowerColumn = std::max(rightmostLowerColumn, index % annexGas.gridColumns);
        }
        if (!annexGas.cells[index].solid && center.y < 650.0f) {
            massAboveFirstTier += annexGas.cells[index].mass;
        }
    }
    if (fabsf(tenSecondMass - 550.0f) > 0.5f || tenSecondMassAboveFirstTier > 5.0f ||
        fabsf(annexMass - 1100.0f) > 0.5f ||
        massAboveFirstTier < 100.0f || massAboveFirstTier > 900.0f) {
        std::cerr << "Neurotoxin Annex gas did not progress gradually through the first maze opening (mass="
            << annexMass << ", above first tier=" << massAboveFirstTier << ", ten-second mass="
            << tenSecondMass << ", ten-second above=" << tenSecondMassAboveFirstTier << ", peak=" << peakMass
            << ", lower-right column=" << rightmostLowerColumn << ").\n";
        return 1;
    }

    for (int step = 1200; step < 3600; step++) {
        const float remaining = fmaxf(0.0f, annex.toxinLeak.maximumMass - GetFluidMass(annexGas));
        EmitGasDensity(
            annexGas,
            annex.toxinLeak.source,
            {48.0f, -42.0f},
            fminf(annex.toxinLeak.massPerSecond / 60.0f, remaining)
        );
        UpdateFluidField(
            annexGas,
            annexObstacles,
            annexWind,
            1.0f / 60.0f,
            FluidSimulationMode::Advanced
        );
    }
    const float valveDensity = SampleFluid(annexGas, annex.valve.center).density;
    const float valveLeftDensity =
        SampleFluid(annexGas, {annex.valve.center.x - 40.0f, annex.valve.center.y}).density;
    const float valveRightDensity =
        SampleFluid(annexGas, {annex.valve.center.x + 40.0f, annex.valve.center.y}).density;
    const float valveAboveDensity =
        SampleFluid(annexGas, {annex.valve.center.x, annex.valve.center.y - 40.0f}).density;
    const float nearbyValvePeak =
        std::max({valveLeftDensity, valveRightDensity, valveAboveDensity});
    if (valveDensity < 0.04f || valveDensity < nearbyValvePeak * 0.35f) {
        std::cerr << "Gas formed a low-density pocket around the neurotoxin valve (center="
            << valveDensity << ", nearby peak=" << nearbyValvePeak << ").\n";
        return 1;
    }
    std::cout << "Valve gas densities: center=" << valveDensity
        << ", left=" << valveLeftDensity
        << ", right=" << valveRightDensity
        << ", above=" << valveAboveDensity << ".\n";

    std::cout << "All tile gas physics checks passed (annex mass=" << annexMass
        << ", above first tier=" << massAboveFirstTier << ").\n";
    return 0;
}
