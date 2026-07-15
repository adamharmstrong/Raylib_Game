#include "Fluid.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {
    constexpr float FixedTimeStep = 1.0f / 60.0f;
    constexpr int MaximumStepsPerFrame = 4;
    constexpr int MaximumParticles = 2400;
    constexpr float MinimumDistance = 0.0001f;
    constexpr float TileFluidCellSize = 6.0f;

    // This transient hash only accelerates particle neighbor queries; fluid state never lives in it.
    struct NeighborGrid {
        float cellSize{1.0f};
        int columns{1};
        int rows{1};
        std::vector<std::vector<int>> buckets;
    };

    float LengthSquared(Vector2 value) {
        return value.x * value.x + value.y * value.y;
    }

    float DistanceSquared(Vector2 first, Vector2 second) {
        float dx = second.x - first.x;
        float dy = second.y - first.y;
        return dx * dx + dy * dy;
    }

    void ClampMagnitude(Vector2& value, float maximum) {
        float lengthSquared = LengthSquared(value);
        if (lengthSquared <= maximum * maximum) {
            return;
        }
        float scale = maximum / sqrtf(lengthSquared);
        value.x *= scale;
        value.y *= scale;
    }

    bool CircleOverlapsRectangle(Vector2 center, float radius, Rectangle rectangle) {
        float closestX = std::clamp(center.x, rectangle.x, rectangle.x + rectangle.width);
        float closestY = std::clamp(center.y, rectangle.y, rectangle.y + rectangle.height);
        float dx = center.x - closestX;
        float dy = center.y - closestY;
        return dx * dx + dy * dy < radius * radius;
    }

    bool ParticleOverlapsObstacles(Vector2 position, float radius, const std::vector<Rectangle>& obstacles) {
        for (Rectangle obstacle : obstacles) {
            if (obstacle.width > 0.0f && obstacle.height > 0.0f && CircleOverlapsRectangle(position, radius, obstacle)) {
                return true;
            }
        }
        return false;
    }

    void ResolveParticleBounds(FluidParticle& particle, const FluidField& field) {
        float left = field.bounds.x + field.particleRadius;
        float right = field.bounds.x + field.bounds.width - field.particleRadius;
        float top = field.bounds.y + field.particleRadius;
        float bottom = field.bounds.y + field.bounds.height - field.particleRadius;
        if (left > right) left = right = field.bounds.x + field.bounds.width * 0.5f;
        if (top > bottom) top = bottom = field.bounds.y + field.bounds.height * 0.5f;
        particle.position.x = std::clamp(particle.position.x, left, right);
        particle.position.y = std::clamp(particle.position.y, top, bottom);
    }

    void ResolveParticleRectangle(FluidParticle& particle, float radius, Rectangle obstacle) {
        if (obstacle.width <= 0.0f || obstacle.height <= 0.0f ||
            !CircleOverlapsRectangle(particle.position, radius, obstacle)) {
            return;
        }

        float closestX = std::clamp(particle.position.x, obstacle.x, obstacle.x + obstacle.width);
        float closestY = std::clamp(particle.position.y, obstacle.y, obstacle.y + obstacle.height);
        Vector2 offset{particle.position.x - closestX, particle.position.y - closestY};
        float distanceSquared = LengthSquared(offset);
        if (distanceSquared > MinimumDistance * MinimumDistance) {
            float distance = sqrtf(distanceSquared);
            float correction = radius - distance;
            particle.position.x += offset.x / distance * correction;
            particle.position.y += offset.y / distance * correction;
            return;
        }

        float distanceToLeft = fabsf(particle.position.x - obstacle.x);
        float distanceToRight = fabsf(obstacle.x + obstacle.width - particle.position.x);
        float distanceToTop = fabsf(particle.position.y - obstacle.y);
        float distanceToBottom = fabsf(obstacle.y + obstacle.height - particle.position.y);
        float nearest = fminf(fminf(distanceToLeft, distanceToRight), fminf(distanceToTop, distanceToBottom));
        if (nearest == distanceToLeft) particle.position.x = obstacle.x - radius;
        else if (nearest == distanceToRight) particle.position.x = obstacle.x + obstacle.width + radius;
        else if (nearest == distanceToTop) particle.position.y = obstacle.y - radius;
        else particle.position.y = obstacle.y + obstacle.height + radius;
    }

    void ResolveParticleCollisions(
        FluidParticle& particle,
        const FluidField& field,
        const std::vector<Rectangle>& obstacles
    ) {
        ResolveParticleBounds(particle, field);
        for (Rectangle obstacle : obstacles) {
            ResolveParticleRectangle(particle, field.particleRadius, obstacle);
            ResolveParticleBounds(particle, field);
        }
    }

    NeighborGrid BuildNeighborGrid(const FluidField& field) {
        NeighborGrid grid{};
        grid.cellSize = fmaxf(1.0f, field.interactionRadius);
        grid.columns = std::max(1, static_cast<int>(ceilf(field.bounds.width / grid.cellSize)));
        grid.rows = std::max(1, static_cast<int>(ceilf(field.bounds.height / grid.cellSize)));
        grid.buckets.resize(static_cast<size_t>(grid.columns * grid.rows));

        for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
            const Vector2 position = field.particles[index].position;
            int column = std::clamp(
                static_cast<int>((position.x - field.bounds.x) / grid.cellSize),
                0,
                grid.columns - 1
            );
            int row = std::clamp(
                static_cast<int>((position.y - field.bounds.y) / grid.cellSize),
                0,
                grid.rows - 1
            );
            grid.buckets[row * grid.columns + column].push_back(index);
        }
        return grid;
    }

    void ForEachNeighborPair(
        const FluidField& field,
        const NeighborGrid& grid,
        const std::function<void(int, int, float, Vector2)>& callback
    ) {
        float interactionSquared = field.interactionRadius * field.interactionRadius;
        for (int firstIndex = 0; firstIndex < static_cast<int>(field.particles.size()); firstIndex++) {
            const Vector2 first = field.particles[firstIndex].position;
            int centerColumn = std::clamp(
                static_cast<int>((first.x - field.bounds.x) / grid.cellSize),
                0,
                grid.columns - 1
            );
            int centerRow = std::clamp(
                static_cast<int>((first.y - field.bounds.y) / grid.cellSize),
                0,
                grid.rows - 1
            );

            for (int row = std::max(0, centerRow - 1); row <= std::min(grid.rows - 1, centerRow + 1); row++) {
                for (int column = std::max(0, centerColumn - 1); column <= std::min(grid.columns - 1, centerColumn + 1); column++) {
                    for (int secondIndex : grid.buckets[row * grid.columns + column]) {
                        if (secondIndex <= firstIndex) {
                            continue;
                        }
                        Vector2 difference{
                            field.particles[secondIndex].position.x - first.x,
                            field.particles[secondIndex].position.y - first.y
                        };
                        float distanceSquared = LengthSquared(difference);
                        if (distanceSquared >= interactionSquared) {
                            continue;
                        }
                        float distance = sqrtf(fmaxf(distanceSquared, MinimumDistance * MinimumDistance));
                        callback(firstIndex, secondIndex, distance, {difference.x / distance, difference.y / distance});
                    }
                }
            }
        }
    }

    void UpdateParticleDensities(FluidField& field) {
        for (FluidParticle& particle : field.particles) {
            particle.density = 1.0f;
            particle.surface = true;
        }
        std::vector<unsigned char> hasParticleAbove(field.particles.size(), 0);
        NeighborGrid grid = BuildNeighborGrid(field);
        ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2 direction) {
            float q = 1.0f - distance / field.interactionRadius;
            float contribution = q * q;
            field.particles[first].density += contribution;
            field.particles[second].density += contribution;
            if (distance < field.particleSpacing * 1.30f) {
                if (direction.y < -0.34f) hasParticleAbove[first] = 1;
                if (direction.y > 0.34f) hasParticleAbove[second] = 1;
            }
        });

        constexpr float RestKernelDensity = 2.55f;
        for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
            field.particles[index].density = std::clamp(
                field.particles[index].density / RestKernelDensity,
                0.0f,
                1.0f
            );
            field.particles[index].surface = hasParticleAbove[index] == 0;
        }
    }

    void ApplyWaterPressure(
        FluidField& field,
        const std::vector<Rectangle>& obstacles
    ) {
        constexpr int ConstraintIterations = 5;
        float minimumSeparation = field.particleSpacing * 0.78f;
        float restDensity = 1.58f;
        float pressureScale = field.particleSpacing * 0.12f * field.flowSpeed;
        float nearPressureScale = field.particleSpacing * 0.018f * field.flowSpeed;

        for (int iteration = 0; iteration < ConstraintIterations; iteration++) {
            NeighborGrid grid = BuildNeighborGrid(field);
            std::vector<float> density(field.particles.size(), 0.0f);
            std::vector<float> nearDensity(field.particles.size(), 0.0f);
            ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2) {
                float q = 1.0f - distance / field.interactionRadius;
                float qSquared = q * q;
                density[first] += qSquared;
                density[second] += qSquared;
                nearDensity[first] += qSquared * q;
                nearDensity[second] += qSquared * q;
            });

            std::vector<float> pressure(field.particles.size(), 0.0f);
            std::vector<float> nearPressure(field.particles.size(), 0.0f);
            for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
                pressure[index] = fmaxf(-field.particleSpacing * 0.035f, (density[index] - restDensity) * pressureScale);
                nearPressure[index] = nearDensity[index] * nearPressureScale;
            }

            std::vector<Vector2> corrections(field.particles.size());
            ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2 direction) {
                float q = 1.0f - distance / field.interactionRadius;
                float magnitude = (
                    (pressure[first] + pressure[second]) * 0.5f * q +
                    (nearPressure[first] + nearPressure[second]) * 0.5f * q * q
                ) * 0.5f;
                if (distance < minimumSeparation) {
                    magnitude += (minimumSeparation - distance) * 0.48f;
                }
                magnitude = std::clamp(
                    magnitude,
                    -field.particleSpacing * 0.055f,
                    field.particleSpacing * 0.16f
                );
                Vector2 correction{direction.x * magnitude, direction.y * magnitude};
                corrections[first].x -= correction.x;
                corrections[first].y -= correction.y;
                corrections[second].x += correction.x;
                corrections[second].y += correction.y;
            });

            for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
                ClampMagnitude(corrections[index], field.particleSpacing * 0.32f);
                field.particles[index].position.x += corrections[index].x;
                field.particles[index].position.y += corrections[index].y;
                ResolveParticleCollisions(field.particles[index], field, obstacles);
            }
        }
    }

    void BuildGelBonds(FluidField& field) {
        field.gelBonds.clear();
        if (field.type != FluidType::Gel || field.particles.empty()) {
            return;
        }
        NeighborGrid grid = BuildNeighborGrid(field);
        float bondRadius = field.particleSpacing * 1.12f;
        ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2) {
            if (distance <= bondRadius) {
                field.gelBonds.push_back({first, second, distance, true});
            }
        });
    }

    void ApplyGelElasticity(FluidField& field, const std::vector<Rectangle>& obstacles) {
        constexpr int ElasticIterations = 4;
        float stiffness = std::clamp(0.18f * field.flowSpeed, 0.10f, 0.34f);
        for (int iteration = 0; iteration < ElasticIterations; iteration++) {
            for (GelBond& bond : field.gelBonds) {
                if (!bond.active || bond.first < 0 || bond.second < 0 ||
                    bond.first >= static_cast<int>(field.particles.size()) ||
                    bond.second >= static_cast<int>(field.particles.size())) {
                    continue;
                }
                FluidParticle& first = field.particles[bond.first];
                FluidParticle& second = field.particles[bond.second];
                Vector2 difference{
                    second.position.x - first.position.x,
                    second.position.y - first.position.y
                };
                float distance = sqrtf(fmaxf(LengthSquared(difference), MinimumDistance * MinimumDistance));
                if (distance > bond.restLength * 3.0f) {
                    bond.active = false;
                    continue;
                }

                // Gel yields slightly under sustained large strain instead of behaving
                // like an ideal rubber sheet, but keeps a strong elastic memory.
                float strain = distance / fmaxf(bond.restLength, MinimumDistance);
                if (strain > 1.45f) {
                    bond.restLength += (distance - bond.restLength * 1.45f) * 0.0025f;
                }
                float correction = (distance - bond.restLength) * stiffness * 0.5f;
                Vector2 displacement{
                    difference.x / distance * correction,
                    difference.y / distance * correction
                };
                first.position.x += displacement.x;
                first.position.y += displacement.y;
                second.position.x -= displacement.x;
                second.position.y -= displacement.y;
            }
            for (FluidParticle& particle : field.particles) {
                ResolveParticleCollisions(particle, field, obstacles);
            }
        }
    }

    void ApplyGasPressure(FluidField& field, const std::vector<Rectangle>& obstacles) {
        float minimumSeparation = field.particleSpacing * 0.52f;
        for (int iteration = 0; iteration < 3; iteration++) {
            NeighborGrid grid = BuildNeighborGrid(field);
            std::vector<Vector2> corrections(field.particles.size());
            ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2 direction) {
                float q = 1.0f - distance / field.interactionRadius;
                float magnitude = q * q * field.particleSpacing * 0.026f * field.flowSpeed;
                if (distance < minimumSeparation) {
                    magnitude += (minimumSeparation - distance) * 0.42f;
                }
                Vector2 correction{direction.x * magnitude, direction.y * magnitude};
                corrections[first].x -= correction.x;
                corrections[first].y -= correction.y;
                corrections[second].x += correction.x;
                corrections[second].y += correction.y;
            });
            for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
                ClampMagnitude(corrections[index], field.particleSpacing * 0.22f);
                field.particles[index].position.x += corrections[index].x;
                field.particles[index].position.y += corrections[index].y;
                ResolveParticleCollisions(field.particles[index], field, obstacles);
            }
        }
    }

    void ApplyVelocitySmoothing(FluidField& field, float strength) {
        NeighborGrid grid = BuildNeighborGrid(field);
        std::vector<Vector2> changes(field.particles.size());
        ForEachNeighborPair(field, grid, [&](int first, int second, float distance, Vector2) {
            float q = 1.0f - distance / field.interactionRadius;
            Vector2 difference{
                field.particles[second].velocity.x - field.particles[first].velocity.x,
                field.particles[second].velocity.y - field.particles[first].velocity.y
            };
            Vector2 change{difference.x * q * strength, difference.y * q * strength};
            changes[first].x += change.x;
            changes[first].y += change.y;
            changes[second].x -= change.x;
            changes[second].y -= change.y;
        });
        for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
            field.particles[index].velocity.x += changes[index].x;
            field.particles[index].velocity.y += changes[index].y;
        }
    }

    void SimulateGel(
        FluidField& field,
        const std::vector<Rectangle>& obstacles,
        const std::vector<Vector2>& externalFlow,
        float dt
    ) {
        float windResponse = 1.0f - expf(-2.2f * dt);
        float damping = expf(-0.82f * dt);
        for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
            FluidParticle& particle = field.particles[index];
            particle.previousPosition = particle.position;
            Vector2 target = index < static_cast<int>(externalFlow.size())
                ? Vector2{externalFlow[index].x * 0.08f, externalFlow[index].y * 0.08f}
                : Vector2{};
            particle.velocity.x += (target.x - particle.velocity.x) * windResponse;
            particle.velocity.y += (target.y - particle.velocity.y) * windResponse;
            particle.velocity.y += 760.0f * dt;
            particle.velocity.x *= damping;
            particle.velocity.y *= damping;
            ClampMagnitude(particle.velocity, 470.0f);
            particle.position.x += particle.velocity.x * dt;
            particle.position.y += particle.velocity.y * dt;
            ResolveParticleCollisions(particle, field, obstacles);
        }

        ApplyWaterPressure(field, obstacles);
        ApplyGelElasticity(field, obstacles);
        for (FluidParticle& particle : field.particles) {
            particle.velocity = {
                (particle.position.x - particle.previousPosition.x) / dt,
                (particle.position.y - particle.previousPosition.y) / dt
            };
            ClampMagnitude(particle.velocity, 470.0f);
        }
        ApplyVelocitySmoothing(field, 0.030f * field.flowSpeed);
        UpdateParticleDensities(field);
    }

    int CellIndex(const FluidField& field, int column, int row) {
        return row * field.gridColumns + column;
    }

    bool IsCellularFluid(FluidType type) {
        return type == FluidType::Water || type == FluidType::Sand;
    }

    float DesiredCellSize(const FluidField& field, FluidSimulationMode mode) {
        if (!IsCellularFluid(field.type)) {
            return field.cellSize;
        }
        if (mode == FluidSimulationMode::Tile) {
            return std::clamp(fmaxf(field.particleSpacing, TileFluidCellSize), TileFluidCellSize, 12.0f);
        }
        return std::clamp(field.particleSpacing, 1.0f, 12.0f);
    }

    Vector2 CellCenter(const FluidField& field, int index) {
        int column = index % field.gridColumns;
        int row = index / field.gridColumns;
        return {
            field.bounds.x + (static_cast<float>(column) + 0.5f) * field.cellSize,
            field.bounds.y + (static_cast<float>(row) + 0.5f) * field.cellSize
        };
    }

    float StableLowerMass(float totalMass, float maximumCompression) {
        constexpr float MaximumMass = 1.0f;
        if (totalMass <= MaximumMass) {
            return MaximumMass;
        }
        if (totalMass < MaximumMass * 2.0f + maximumCompression) {
            return (MaximumMass * MaximumMass + totalMass * maximumCompression) /
                (MaximumMass + maximumCompression);
        }
        return (totalMass + maximumCompression) * 0.5f;
    }

    void UpdateCellSolids(FluidField& field, const std::vector<Rectangle>& obstacles) {
        std::vector<unsigned char> newSolid(field.cells.size(), 0);
        // Rasterize each obstacle directly into its covered cell range. At one-pixel
        // resolution this avoids testing every cell against every world collider.
        for (Rectangle obstacle : obstacles) {
            if (obstacle.width <= 0.0f || obstacle.height <= 0.0f ||
                !CheckCollisionRecs(field.bounds, obstacle)) {
                continue;
            }
            int firstColumn = std::clamp(static_cast<int>(floorf(
                (obstacle.x - field.bounds.x) / field.cellSize)), 0, field.gridColumns - 1);
            int lastColumn = std::clamp(static_cast<int>(ceilf(
                (obstacle.x + obstacle.width - field.bounds.x) / field.cellSize)) - 1, 0, field.gridColumns - 1);
            int firstRow = std::clamp(static_cast<int>(floorf(
                (obstacle.y - field.bounds.y) / field.cellSize)), 0, field.gridRows - 1);
            int lastRow = std::clamp(static_cast<int>(ceilf(
                (obstacle.y + obstacle.height - field.bounds.y) / field.cellSize)) - 1, 0, field.gridRows - 1);
            for (int row = firstRow; row <= lastRow; row++) {
                for (int column = firstColumn; column <= lastColumn; column++) {
                    newSolid[CellIndex(field, column, row)] = 1;
                }
            }
        }

        // Dynamic bodies displace cellular material into nearby open cells. If a
        // body temporarily blocks every nearby opening, the mass remains hidden in
        // that solid cell so movement cannot erase fluid volume.
        float displacementCapacity = field.type == FluidType::Water ? 1.018f : 1.0f;
        for (int index = 0; index < static_cast<int>(field.cells.size()); index++) {
            FluidCell& source = field.cells[index];
            if (newSolid[index] == 0 || source.mass <= 0.0001f) {
                continue;
            }
            int sourceColumn = index % field.gridColumns;
            int sourceRow = index / field.gridColumns;
            int maximumDistance = std::max(field.gridColumns, field.gridRows);
            float remainingMass = source.mass;
            Vector2 sourceVelocity = source.velocity;
            source.mass = 0.0f;
            source.velocity = {};
            int nearestOpenCell = -1;

            for (int distance = 1; distance <= maximumDistance && remainingMass > 0.0001f; distance++) {
                const int candidates[][2] = {
                    {sourceColumn, sourceRow - distance},
                    {sourceColumn - distance, sourceRow},
                    {sourceColumn + distance, sourceRow},
                    {sourceColumn, sourceRow + distance}
                };
                for (const auto& candidate : candidates) {
                    if (candidate[0] < 0 || candidate[0] >= field.gridColumns ||
                        candidate[1] < 0 || candidate[1] >= field.gridRows) {
                        continue;
                    }
                    int candidateIndex = CellIndex(field, candidate[0], candidate[1]);
                    if (newSolid[candidateIndex] == 0) {
                        if (nearestOpenCell < 0) {
                            nearestOpenCell = candidateIndex;
                        }
                        FluidCell& destination = field.cells[candidateIndex];
                        float moved = fminf(remainingMass, fmaxf(0.0f, displacementCapacity - destination.mass));
                        if (moved <= 0.0001f) {
                            continue;
                        }
                        destination.mass += moved;
                        float blend = moved / fmaxf(destination.mass, 0.0001f);
                        destination.velocity.x = destination.velocity.x * (1.0f - blend) + sourceVelocity.x * blend;
                        destination.velocity.y = destination.velocity.y * (1.0f - blend) + sourceVelocity.y * blend;
                        remainingMass -= moved;
                    }
                }
            }

            if (remainingMass > 0.0001f) {
                if (nearestOpenCell >= 0) {
                    FluidCell& destination = field.cells[nearestOpenCell];
                    destination.mass += remainingMass;
                    float blend = remainingMass / fmaxf(destination.mass, 0.0001f);
                    destination.velocity.x = destination.velocity.x * (1.0f - blend) + sourceVelocity.x * blend;
                    destination.velocity.y = destination.velocity.y * (1.0f - blend) + sourceVelocity.y * blend;
                }
                else {
                    source.mass = remainingMass;
                    source.velocity = sourceVelocity;
                }
            }
        }

        for (int index = 0; index < static_cast<int>(field.cells.size()); index++) {
            field.cells[index].solid = newSolid[index] != 0;
            if (field.cells[index].solid) {
                field.cells[index].velocity = {};
            }
        }
    }

    void RelaxCellOverflow(FluidField& field, float maximumMass) {
        if (field.cells.empty()) {
            return;
        }

        for (int pass = 0; pass < 4; pass++) {
            bool movedAny = false;
            for (int row = field.gridRows - 1; row >= 0; row--) {
                for (int column = 0; column < field.gridColumns; column++) {
                    int index = CellIndex(field, column, row);
                    FluidCell& source = field.cells[index];
                    if (source.solid || source.mass <= maximumMass) {
                        continue;
                    }

                    float excess = source.mass - maximumMass;
                    const int candidates[][2] = {
                        {column, row - 1},
                        {column - 1, row},
                        {column + 1, row},
                        {column, row + 1}
                    };
                    for (const auto& candidate : candidates) {
                        if (excess <= 0.0001f ||
                            candidate[0] < 0 || candidate[0] >= field.gridColumns ||
                            candidate[1] < 0 || candidate[1] >= field.gridRows) {
                            continue;
                        }

                        int destinationIndex = CellIndex(field, candidate[0], candidate[1]);
                        FluidCell& destination = field.cells[destinationIndex];
                        if (destination.solid) {
                            continue;
                        }

                        float moved = fminf(excess, fmaxf(0.0f, maximumMass - destination.mass));
                        if (moved <= 0.0001f) {
                            continue;
                        }

                        destination.mass += moved;
                        source.mass -= moved;
                        excess -= moved;
                        movedAny = true;
                    }
                }
            }

            if (!movedAny) {
                break;
            }
        }
    }

    void SimulateWater(
        FluidField& field,
        const std::vector<Rectangle>& obstacles,
        const std::vector<Vector2>& externalFlow,
        float dt
    ) {
        UpdateCellSolids(field, obstacles);
        constexpr float maximumCompression = 0.018f;
        constexpr float maximumCellMass = 1.0f + maximumCompression;
        RelaxCellOverflow(field, maximumCellMass);
        constexpr int solverIterations = 4;
        float stepDt = dt / static_cast<float>(solverIterations);
        std::vector<float> nextMass(field.cells.size());
        std::vector<Vector2> flux(field.cells.size());
        std::vector<float> hydrostaticHead(field.cells.size());
        for (int solverIteration = 0; solverIteration < solverIterations; solverIteration++) {
        std::fill(flux.begin(), flux.end(), Vector2{});
        std::fill(hydrostaticHead.begin(), hydrostaticHead.end(), 0.0f);
        for (int index = 0; index < static_cast<int>(field.cells.size()); index++) {
            nextMass[index] = field.cells[index].mass;
        }
        for (int column = 0; column < field.gridColumns; column++) {
            float massAbove = 0.0f;
            for (int row = 0; row < field.gridRows; row++) {
                int index = CellIndex(field, column, row);
                if (field.cells[index].solid) {
                    massAbove = 0.0f;
                    continue;
                }
                massAbove += field.cells[index].mass;
                hydrostaticHead[index] = massAbove;
            }
        }

        float transferRate = std::clamp(field.flowSpeed, 0.15f, 1.0f);
        float pressureResponse = 0.055f * std::clamp(field.flowSpeed, 0.5f, 2.0f);
        constexpr float minimumFlow = 0.00005f;
        // Water is nearly incompressible; a small allowance transmits pressure
        // upward and sideways without storing visible amounts at the bottom.
        constexpr float horizontalDivisor = 2.0f;
        constexpr float downwardVelocityResponse = 0.08f;
        constexpr float lateralVelocityResponse = 0.04f;
        constexpr float upwardVelocityResponse = 0.04f;
        auto transfer = [&](int source, int destination, float amount, Vector2 direction, float& remaining) {
            if (destination < 0 || field.cells[destination].solid || amount <= minimumFlow || remaining <= 0.0f) {
                return;
            }
            float capacity = fmaxf(0.0f, maximumCellMass - nextMass[destination]);
            amount = fminf(amount, fminf(remaining, capacity));
            if (amount <= minimumFlow) {
                return;
            }
            nextMass[source] -= amount;
            nextMass[destination] += amount;
            remaining -= amount;
            flux[source].x += direction.x * amount;
            flux[source].y += direction.y * amount;
            flux[destination].x += direction.x * amount;
            flux[destination].y += direction.y * amount;
        };

        for (int row = field.gridRows - 1; row >= 0; row--) {
            for (int column = 0; column < field.gridColumns; column++) {
                int index = CellIndex(field, column, row);
                const FluidCell& cell = field.cells[index];
                if (cell.solid || cell.mass <= minimumFlow) {
                    continue;
                }
                float remaining = cell.mass;
                int below = row + 1 < field.gridRows ? CellIndex(field, column, row + 1) : -1;
                if (below >= 0 && !field.cells[below].solid) {
                    float flow = StableLowerMass(remaining + field.cells[below].mass, maximumCompression) -
                        field.cells[below].mass;
                    flow += fmaxf(0.0f, cell.velocity.y) * stepDt / field.cellSize * remaining *
                        downwardVelocityResponse;
                    transfer(index, below, fmaxf(0.0f, flow) * transferRate, {0.0f, 1.0f}, remaining);
                }

                int firstDirection = ((field.simulationStep + row) & 1ULL) == 0 ? -1 : 1;
                for (int pass = 0; pass < 2; pass++) {
                    int direction = pass == 0 ? firstDirection : -firstDirection;
                    int adjacentColumn = column + direction;
                    if (adjacentColumn < 0 || adjacentColumn >= field.gridColumns) {
                        continue;
                    }
                    int adjacent = CellIndex(field, adjacentColumn, row);
                    float flow = (remaining - field.cells[adjacent].mass) / horizontalDivisor;
                    float pressureDifference = hydrostaticHead[index] - hydrostaticHead[adjacent];
                    flow += fmaxf(0.0f, pressureDifference) * pressureResponse;
                    float velocityBias = cell.velocity.x * static_cast<float>(direction) * stepDt /
                        field.cellSize * remaining * lateralVelocityResponse;
                    transfer(index, adjacent, fmaxf(0.0f, flow + velocityBias) * transferRate,
                        {static_cast<float>(direction), 0.0f}, remaining);
                }

                int above = row > 0 ? CellIndex(field, column, row - 1) : -1;
                if (above >= 0 && !field.cells[above].solid) {
                    float flow = remaining - StableLowerMass(remaining + field.cells[above].mass, maximumCompression);
                    flow += fmaxf(0.0f, -cell.velocity.y) * stepDt / field.cellSize * remaining *
                        upwardVelocityResponse;
                    transfer(index, above, fmaxf(0.0f, flow) * transferRate, {0.0f, -1.0f}, remaining);
                }
            }
        }

        for (int index = 0; index < static_cast<int>(field.cells.size()); index++) {
            FluidCell& cell = field.cells[index];
            if (cell.solid) {
                cell.velocity = {};
                continue;
            }
            cell.mass = nextMass[index] < 0.0001f ? 0.0f : nextMass[index];
            if (cell.mass <= 0.0f) {
                cell.velocity = {};
                continue;
            }
            Vector2 flowVelocity{
                flux[index].x / fmaxf(0.1f, cell.mass) * field.cellSize / stepDt,
                flux[index].y / fmaxf(0.1f, cell.mass) * field.cellSize / stepDt
            };
            Vector2 wind = index < static_cast<int>(externalFlow.size()) ? externalFlow[index] : Vector2{};
            constexpr float retainedVelocity = 0.42f;
            constexpr float flowResponse = 0.18f;
            cell.velocity.x = cell.velocity.x * retainedVelocity + flowVelocity.x * flowResponse + wind.x * 0.002f;
            cell.velocity.y = cell.velocity.y * retainedVelocity + flowVelocity.y * flowResponse + wind.y * 0.002f;
            ClampMagnitude(cell.velocity, 520.0f);
        }
        }
    }

    void SimulateTileWater(
        FluidField& field,
        const std::vector<Rectangle>& obstacles,
        float dt
    ) {
        UpdateCellSolids(field, obstacles);
        RelaxCellOverflow(field, 1.0f);
        int substeps = std::clamp(static_cast<int>(roundf(field.flowSpeed)), 1, 2);
        float substepDt = dt / static_cast<float>(substeps);

        for (int substep = 0; substep < substeps; substep++) {
            bool scanLeftToRight = ((field.simulationStep + static_cast<unsigned long long>(substep)) & 1ULL) == 0;
            for (int row = field.gridRows - 1; row >= 0; row--) {
                for (int offset = 0; offset < field.gridColumns; offset++) {
                    int column = scanLeftToRight ? offset : field.gridColumns - 1 - offset;
                    int index = CellIndex(field, column, row);
                    FluidCell& source = field.cells[index];
                    if (source.solid || source.mass <= 0.0001f) {
                        continue;
                    }

                    source.velocity.y = fminf(420.0f, source.velocity.y + 640.0f * substepDt);
                    auto transfer = [&](int destinationIndex, float requestedMass, Vector2 direction) {
                        if (destinationIndex < 0 || field.cells[destinationIndex].solid || requestedMass <= 0.0001f) {
                            return 0.0f;
                        }
                        FluidCell& destination = field.cells[destinationIndex];
                        float capacity = fmaxf(0.0f, 1.0f - destination.mass);
                        float moved = fminf(source.mass, fminf(requestedMass, capacity));
                        if (moved <= 0.0001f) {
                            return 0.0f;
                        }
                        destination.mass += moved;
                        source.mass -= moved;
                        float blend = moved / fmaxf(destination.mass, 0.0001f);
                        destination.velocity.x = destination.velocity.x * (1.0f - blend) +
                            (source.velocity.x + direction.x * field.cellSize / substepDt) * blend;
                        destination.velocity.y = destination.velocity.y * (1.0f - blend) +
                            (source.velocity.y + direction.y * field.cellSize / substepDt) * blend;
                        return moved;
                    };

                    if (row + 1 < field.gridRows) {
                        int below = CellIndex(field, column, row + 1);
                        float fallBias = 0.72f + fmaxf(0.0f, source.velocity.y) * substepDt / field.cellSize * 0.20f;
                        transfer(below, source.mass * std::clamp(fallBias * field.flowSpeed, 0.15f, 1.0f), {0.0f, 1.0f});
                    }

                    if (source.mass > 0.0001f) {
                        int firstDirection = ((field.simulationStep + static_cast<unsigned long long>(row)) & 1ULL) == 0 ? -1 : 1;
                        for (int pass = 0; pass < 2 && source.mass > 0.0001f; pass++) {
                            int direction = pass == 0 ? firstDirection : -firstDirection;
                            int adjacentColumn = column + direction;
                            if (adjacentColumn < 0 || adjacentColumn >= field.gridColumns) continue;
                            int adjacent = CellIndex(field, adjacentColumn, row);
                            float levelDifference = source.mass - field.cells[adjacent].mass;
                            float velocityBias = source.velocity.x * static_cast<float>(direction) * substepDt /
                                field.cellSize * 0.16f;
                            float flow = fmaxf(0.0f, levelDifference * 0.35f + velocityBias);
                            transfer(adjacent, flow * std::clamp(field.flowSpeed, 0.20f, 1.0f),
                                {static_cast<float>(direction), 0.0f});
                        }
                    }

                    if (source.mass <= 0.0001f) {
                        source.mass = 0.0f;
                        source.velocity = {};
                    }
                    else {
                        source.velocity.x *= 0.74f;
                        source.velocity.y *= 0.58f;
                        ClampMagnitude(source.velocity, 420.0f);
                    }
                }
            }
        }
    }

    void SimulateSand(
        FluidField& field,
        const std::vector<Rectangle>& obstacles,
        float dt
    ) {
        UpdateCellSolids(field, obstacles);
        int substeps = std::clamp(static_cast<int>(roundf(field.flowSpeed * 2.0f)), 1, 4);
        float substepDt = dt / static_cast<float>(substeps);

        for (int substep = 0; substep < substeps; substep++) {
            std::vector<unsigned char> movedUpward(field.cells.size(), 0);
            bool scanLeftToRight = ((field.simulationStep + static_cast<unsigned long long>(substep)) & 1ULL) == 0;
            for (int row = field.gridRows - 2; row >= 0; row--) {
                for (int offset = 0; offset < field.gridColumns; offset++) {
                    int column = scanLeftToRight ? offset : field.gridColumns - 1 - offset;
                    int index = CellIndex(field, column, row);
                    FluidCell& source = field.cells[index];
                    if (source.solid || source.mass <= 0.0001f || movedUpward[index] != 0) {
                        continue;
                    }

                    source.velocity.y = fminf(360.0f, source.velocity.y + 920.0f * substepDt);
                    int preferredDirection = source.velocity.x < -4.0f ? -1 :
                        (source.velocity.x > 4.0f ? 1 :
                        (((field.simulationStep + static_cast<unsigned long long>(index)) & 1ULL) == 0 ? -1 : 1));

                    auto moveMass = [&](int destinationIndex, float requestedMass, Vector2 movement) {
                        if (destinationIndex < 0 || field.cells[destinationIndex].solid || requestedMass <= 0.0001f) {
                            return 0.0f;
                        }
                        FluidCell& destination = field.cells[destinationIndex];
                        float capacity = fmaxf(0.0f, 1.0f - destination.mass);
                        float moved = fminf(source.mass, fminf(requestedMass, capacity));
                        if (moved <= 0.0001f) {
                            return 0.0f;
                        }
                        destination.mass += moved;
                        source.mass -= moved;
                        float blend = moved / fmaxf(destination.mass, 0.0001f);
                        destination.velocity.x = destination.velocity.x * (1.0f - blend) +
                            (source.velocity.x + movement.x * field.cellSize / substepDt) * blend;
                        destination.velocity.y = destination.velocity.y * (1.0f - blend) +
                            (source.velocity.y + movement.y * field.cellSize / substepDt) * blend;
                        return moved;
                    };

                    // Strong upward impulses eject loose grains ballistically. A
                    // received grain is marked so it cannot cross many rows in one pass.
                    if (source.velocity.y < -45.0f && row > 0) {
                        int upwardColumn = std::clamp(column +
                            (fabsf(source.velocity.x) > 55.0f ? preferredDirection : 0),
                            0, field.gridColumns - 1);
                        int upward = CellIndex(field, upwardColumn, row - 1);
                        float ejected = moveMass(
                            upward,
                            source.mass * std::clamp(-source.velocity.y / 360.0f, 0.22f, 1.0f),
                            {static_cast<float>(upwardColumn - column), -1.0f}
                        );
                        if (ejected > 0.0001f) {
                            movedUpward[upward] = 1;
                        }
                    }

                    int below = CellIndex(field, column, row + 1);
                    float moved = moveMass(below, source.mass, {0.0f, 1.0f});
                    if (source.mass > 0.0001f && moved <= 0.0001f) {
                        for (int pass = 0; pass < 2 && source.mass > 0.0001f; pass++) {
                            int direction = pass == 0 ? preferredDirection : -preferredDirection;
                            int diagonalColumn = column + direction;
                            if (diagonalColumn < 0 || diagonalColumn >= field.gridColumns) continue;
                            int diagonal = CellIndex(field, diagonalColumn, row + 1);
                            moved += moveMass(diagonal, source.mass, {static_cast<float>(direction), 1.0f});
                            if (moved > 0.0001f) break;
                        }
                    }

                    // Occasional surface creep lowers the idealized 45-degree CA pile
                    // toward dry sand's roughly 30-35 degree angle of repose.
                    if (source.mass > 0.0001f && moved <= 0.0001f &&
                        (field.simulationStep + static_cast<unsigned long long>(index * 17)) % 240ULL == 0) {
                        for (int pass = 0; pass < 2; pass++) {
                            int direction = pass == 0 ? preferredDirection : -preferredDirection;
                            int sideColumn = column + direction;
                            if (sideColumn < 0 || sideColumn >= field.gridColumns) continue;
                            int side = CellIndex(field, sideColumn, row);
                            int sideSupport = CellIndex(field, sideColumn, row + 1);
                            if (!field.cells[sideSupport].solid && field.cells[sideSupport].mass < 0.90f) continue;
                            if (moveMass(side, source.mass, {static_cast<float>(direction), 0.0f}) > 0.0001f) break;
                        }
                    }

                    if (source.mass <= 0.0001f) {
                        source.mass = 0.0f;
                        source.velocity = {};
                    }
                    else if (moved <= 0.0001f) {
                        source.velocity.x *= 0.36f;
                        source.velocity.y = 0.0f;
                    }
                }
            }
        }
    }

    void SimulateGas(
        FluidField& field,
        const std::vector<Rectangle>& obstacles,
        const std::vector<Vector2>& externalFlow,
        float dt
    ) {
        float windResponse = 1.0f - expf(-4.2f * dt);
        float damping = expf(-0.95f * dt);
        for (int index = 0; index < static_cast<int>(field.particles.size()); index++) {
            FluidParticle& particle = field.particles[index];
            particle.previousPosition = particle.position;
            Vector2 target = index < static_cast<int>(externalFlow.size())
                ? Vector2{externalFlow[index].x * 0.78f, externalFlow[index].y * 0.78f}
                : Vector2{};
            float phase = static_cast<float>(field.simulationStep) * 0.11f + static_cast<float>(index) * 2.17f;
            particle.velocity.x += (target.x - particle.velocity.x) * windResponse;
            particle.velocity.y += (target.y - particle.velocity.y) * windResponse;
            float secondaryPhase = static_cast<float>(field.simulationStep) * 0.073f +
                static_cast<float>(index) * 1.37f;
            particle.velocity.x += (sinf(phase) * 22.0f + cosf(secondaryPhase) * 10.0f) * dt;
            particle.velocity.y += sinf(secondaryPhase * 1.31f) * 12.0f * dt;
            float buoyancy = 72.0f + (1.0f - particle.density) * 74.0f;
            particle.velocity.y -= buoyancy * dt;
            particle.velocity.x *= damping;
            particle.velocity.y *= damping;
            ClampMagnitude(particle.velocity, 520.0f);
            particle.position.x += particle.velocity.x * dt;
            particle.position.y += particle.velocity.y * dt;
            ResolveParticleCollisions(particle, field, obstacles);
        }

        ApplyGasPressure(field, obstacles);
        for (FluidParticle& particle : field.particles) {
            particle.velocity = {
                (particle.position.x - particle.previousPosition.x) / dt,
                (particle.position.y - particle.previousPosition.y) / dt
            };
            ClampMagnitude(particle.velocity, 520.0f);
        }
        ApplyVelocitySmoothing(field, 0.014f * field.flowSpeed);
        UpdateParticleDensities(field);
    }
}

void InitializeFluidField(
    FluidField& field,
    const std::vector<Rectangle>& obstacles,
    FluidSimulationMode mode
) {
    field.bounds.width = fmaxf(1.0f, field.bounds.width);
    field.bounds.height = fmaxf(1.0f, field.bounds.height);
    field.particleSpacing = IsCellularFluid(field.type) ?
        std::clamp(field.particleSpacing, 1.0f, 12.0f) :
        std::clamp(field.particleSpacing, 6.0f, 48.0f);
    field.initialFill = std::clamp(field.initialFill, 0.0f, 1.0f);
    field.flowSpeed = std::clamp(field.flowSpeed, 0.1f, 4.0f);

    field.accumulator = 0.0f;
    field.simulationStep = 0;
    field.particles.clear();
    field.gelBonds.clear();
    field.cells.clear();

    if (IsCellularFluid(field.type)) {
        field.cellSize = DesiredCellSize(field, mode);
        field.gridColumns = std::max(1, static_cast<int>(ceilf(field.bounds.width / field.cellSize)));
        field.gridRows = std::max(1, static_cast<int>(ceilf(field.bounds.height / field.cellSize)));
        field.cells.resize(static_cast<size_t>(field.gridColumns * field.gridRows));
        UpdateCellSolids(field, obstacles);

        int openCellCount = 0;
        for (const FluidCell& cell : field.cells) {
            if (!cell.solid) openCellCount++;
        }
        float remainingMass = static_cast<float>(openCellCount) * field.initialFill;
        for (int row = field.gridRows - 1; row >= 0 && remainingMass > 0.0f; row--) {
            for (int column = 0; column < field.gridColumns && remainingMass > 0.0f; column++) {
                FluidCell& cell = field.cells[CellIndex(field, column, row)];
                if (cell.solid) continue;
                cell.mass = fminf(1.0f, remainingMass);
                remainingMass -= cell.mass;
            }
        }
        field.initialized = true;
        return;
    }

    float requestedParticleArea = field.bounds.width * field.bounds.height * fmaxf(0.05f, field.initialFill);
    float budgetSpacing = sqrtf(requestedParticleArea / (static_cast<float>(MaximumParticles) * 0.84f));
    field.particleSpacing = fmaxf(field.particleSpacing, budgetSpacing);
    field.particleRadius = field.particleSpacing * (field.type == FluidType::Gel ? 0.60f : 0.48f);
    field.interactionRadius = field.particleSpacing * (field.type == FluidType::Gel ? 2.0f : 2.25f);

    float verticalSpacing = field.particleSpacing * 0.8660254f;
    std::vector<Vector2> candidates;
    int row = 0;
    for (float y = field.bounds.y + field.bounds.height - field.particleRadius;
         y >= field.bounds.y + field.particleRadius;
         y -= verticalSpacing, row++) {
        float offset = (row & 1) != 0 ? field.particleSpacing * 0.5f : 0.0f;
        for (float x = field.bounds.x + field.particleRadius + offset;
             x <= field.bounds.x + field.bounds.width - field.particleRadius;
             x += field.particleSpacing) {
            Vector2 position{x, y};
            if (!ParticleOverlapsObstacles(position, field.particleRadius, obstacles)) {
                candidates.push_back(position);
            }
        }
    }

    int targetCount = std::clamp(
        static_cast<int>(roundf(static_cast<float>(candidates.size()) * field.initialFill)),
        0,
        MaximumParticles
    );
    field.particles.reserve(static_cast<size_t>(targetCount));
    for (int index = 0; index < targetCount; index++) {
        FluidParticle particle{};
        particle.position = candidates[index];
        particle.previousPosition = particle.position;
        field.particles.push_back(particle);
    }
    field.initialized = true;
    BuildGelBonds(field);
    UpdateParticleDensities(field);
}

void UpdateFluidField(
    FluidField& field,
    const std::vector<Rectangle>& obstacles,
    const std::vector<Vector2>& externalFlow,
    float dt,
    FluidSimulationMode mode
) {
    if (!field.initialized ||
        (IsCellularFluid(field.type) && fabsf(field.cellSize - DesiredCellSize(field, mode)) > 0.01f)) {
        InitializeFluidField(field, obstacles, mode);
    }

    field.accumulator = fminf(field.accumulator + fmaxf(0.0f, dt), FixedTimeStep * MaximumStepsPerFrame);
    int stepCount = 0;
    while (field.accumulator + 0.000001f >= FixedTimeStep && stepCount < MaximumStepsPerFrame) {
        if (field.type == FluidType::Water) {
            if (mode == FluidSimulationMode::Tile) {
                SimulateTileWater(field, obstacles, FixedTimeStep);
            }
            else {
                SimulateWater(field, obstacles, externalFlow, FixedTimeStep);
            }
        }
        else if (field.type == FluidType::Sand) {
            SimulateSand(field, obstacles, FixedTimeStep);
        }
        else if (field.type == FluidType::Gel) {
            SimulateGel(field, obstacles, externalFlow, FixedTimeStep);
        }
        else {
            SimulateGas(field, obstacles, externalFlow, FixedTimeStep);
        }
        field.accumulator -= FixedTimeStep;
        field.simulationStep++;
        stepCount++;
    }
}

void AddFluidImpulse(FluidField& field, Vector2 point, float radius, Vector2 velocityChange) {
    if (radius <= 0.0f) {
        return;
    }
    if (field.type == FluidType::Water || field.type == FluidType::Sand) {
        for (int index = 0; index < static_cast<int>(field.cells.size()); index++) {
            FluidCell& cell = field.cells[index];
            if (cell.solid || cell.mass <= 0.0001f) continue;
            float distanceSquared = DistanceSquared(point, CellCenter(field, index));
            if (distanceSquared >= radius * radius) continue;
            float weight = 1.0f - sqrtf(distanceSquared) / radius;
            cell.velocity.x += velocityChange.x * weight;
            cell.velocity.y += velocityChange.y * weight;
            ClampMagnitude(cell.velocity, 520.0f);
        }
        return;
    }
    float influenceRadius = radius + field.particleRadius;
    float radiusSquared = influenceRadius * influenceRadius;
    for (FluidParticle& particle : field.particles) {
        float distanceSquared = DistanceSquared(point, particle.position);
        if (distanceSquared >= radiusSquared) {
            continue;
        }
        float weight = 1.0f - sqrtf(distanceSquared) / influenceRadius;
        particle.velocity.x += velocityChange.x * weight;
        particle.velocity.y += velocityChange.y * weight;
        ClampMagnitude(particle.velocity, field.type == FluidType::Gel ? 470.0f : 520.0f);
    }
}

float AddCellularFluidMass(FluidField& field, float amount, Vector2 velocity) {
    if (!IsCellularFluid(field.type) || field.cells.empty() || field.gridColumns <= 0 || amount <= 0.0f) {
        return 0.0f;
    }

    float remaining = amount;
    int startColumn = static_cast<int>(field.simulationStep % static_cast<unsigned long long>(field.gridColumns));
    for (int row = field.gridRows - 1; row >= 0 && remaining > 0.0001f; row--) {
        for (int offset = 0; offset < field.gridColumns && remaining > 0.0001f; offset++) {
            int column = (startColumn + offset) % field.gridColumns;
            FluidCell& cell = field.cells[CellIndex(field, column, row)];
            if (cell.solid) {
                continue;
            }

            float capacity = fmaxf(0.0f, 1.0f - cell.mass);
            float added = fminf(remaining, capacity);
            if (added <= 0.0001f) {
                continue;
            }

            cell.mass += added;
            float blend = added / cell.mass;
            cell.velocity.x = cell.velocity.x * (1.0f - blend) + velocity.x * blend;
            cell.velocity.y = cell.velocity.y * (1.0f - blend) + velocity.y * blend;
            remaining -= added;
        }
    }

    return amount - remaining;
}

int GetFluidSimulationPointCount(const FluidField& field) {
    return (field.type == FluidType::Water || field.type == FluidType::Sand) ? static_cast<int>(field.cells.size()) :
        static_cast<int>(field.particles.size());
}

Vector2 GetFluidSimulationPoint(const FluidField& field, int index) {
    if (field.type == FluidType::Water || field.type == FluidType::Sand) {
        return index >= 0 && index < static_cast<int>(field.cells.size()) ? CellCenter(field, index) : Vector2{};
    }
    if (index < 0 || index >= static_cast<int>(field.particles.size())) {
        return {};
    }
    return field.particles[index].position;
}

FluidSample SampleFluid(const FluidField& field, Vector2 point) {
    if (!CheckCollisionPointRec(point, field.bounds)) {
        return {};
    }

    if (field.type == FluidType::Water || field.type == FluidType::Sand) {
        if (field.cells.empty()) return {};
        int centerColumn = static_cast<int>(roundf((point.x - field.bounds.x) / field.cellSize - 0.5f));
        int centerRow = static_cast<int>(roundf((point.y - field.bounds.y) / field.cellSize - 0.5f));
        FluidSample sample{};
        float weightSum = 0.0f;
        for (int row = centerRow - 1; row <= centerRow + 1; row++) {
            for (int column = centerColumn - 1; column <= centerColumn + 1; column++) {
                if (column < 0 || column >= field.gridColumns || row < 0 || row >= field.gridRows) continue;
                int index = CellIndex(field, column, row);
                const FluidCell& cell = field.cells[index];
                if (cell.solid || cell.mass <= 0.0f) continue;
                float distance = sqrtf(DistanceSquared(point, CellCenter(field, index)));
                float weight = fmaxf(0.0f, 1.0f - distance / (field.cellSize * 1.75f)) * cell.mass;
                sample.density += weight;
                sample.velocity.x += cell.velocity.x * weight;
                sample.velocity.y += cell.velocity.y * weight;
                weightSum += weight;
            }
        }
        if (weightSum > 0.0001f) {
            sample.velocity.x /= weightSum;
            sample.velocity.y /= weightSum;
        }
        sample.density = std::clamp(sample.density / 1.65f, 0.0f, 1.0f);
        return sample;
    }

    if (field.particles.empty()) return {};

    FluidSample sample{};
    float weightSum = 0.0f;
    for (const FluidParticle& particle : field.particles) {
        float distanceSquared = DistanceSquared(point, particle.position);
        if (distanceSquared >= field.interactionRadius * field.interactionRadius) {
            continue;
        }
        float distance = sqrtf(distanceSquared);
        float q = 1.0f - distance / field.interactionRadius;
        float weight = q * q;
        weightSum += weight;
        sample.velocity.x += particle.velocity.x * weight;
        sample.velocity.y += particle.velocity.y * weight;
    }

    if (weightSum > 0.0001f) {
        sample.velocity.x /= weightSum;
        sample.velocity.y /= weightSum;
    }
    float densityScale = field.type == FluidType::Gel ? 1.45f : 0.85f;
    sample.density = 1.0f - expf(-weightSum * densityScale);
    sample.density = std::clamp(sample.density, 0.0f, 1.0f);
    return sample;
}

FluidSample SampleFluid(const std::vector<FluidField>& fields, FluidType type, Vector2 point) {
    FluidSample combined{};
    float velocityWeight = 0.0f;
    for (const FluidField& field : fields) {
        if (field.type != type) {
            continue;
        }
        FluidSample sample = SampleFluid(field, point);
        combined.density += sample.density;
        combined.velocity.x += sample.velocity.x * sample.density;
        combined.velocity.y += sample.velocity.y * sample.density;
        velocityWeight += sample.density;
    }

    if (velocityWeight > 0.0001f) {
        combined.velocity.x /= velocityWeight;
        combined.velocity.y /= velocityWeight;
    }
    combined.density = std::clamp(combined.density, 0.0f, 1.0f);
    return combined;
}

float GetFluidMass(const FluidField& field) {
    if (field.type == FluidType::Water || field.type == FluidType::Sand) {
        float mass = 0.0f;
        for (const FluidCell& cell : field.cells) mass += cell.mass;
        return mass;
    }
    return static_cast<float>(field.particles.size());
}
