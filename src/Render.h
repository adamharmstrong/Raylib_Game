#pragma once

#include "Level.h"
#include "Player.h"
#include "raylib.h"

#include <vector>

void DrawPulley(Vector2 center, float radius, float rotation, Color color);
void DrawRope(Vector2 start, Vector2 end, float thickness, float patternOffset = 0.0f);
void DrawWinch(const Winch& winch);
void DrawSpikes(float startX, float baseY, int count);
void DrawHazardWeight(const HangingWeight& weight, float ropePatternOffset = 0.0f);
void DrawOutdoorDoorway(Rectangle doorway);
void DrawMachineBox(Rectangle rect, float gearRotation, bool running);
void DrawPlayer(const Player& player, Texture2D texture, int spriteRow);
void DrawEnemy(const Enemy& enemy, Texture2D texture);
void DrawRotaryLatch(const RotaryLatch& latch, bool playerNear, const char* interactPrompt);
void DrawValve(const Valve& valve, bool playerNear, const char* interactPrompt);
void DrawWaterPit(const WaterPit& waterPit);
void DrawFluidBackground(const FluidField& fluid);
void DrawFluidField(const FluidField& fluid, const std::vector<Rectangle>& splashSources = {});
void DrawStoneBlock(const StoneBlock& block);
void DrawBoulder(const Boulder& boulder);
void DrawPhysicsWheel(const PhysicsWheel& wheel);
void DrawGear(const Gear& gear);
void DrawFlywheel(const Flywheel& flywheel);
void DrawSteeringWheel(const SteeringWheel& steeringWheel);
void DrawScrew(const Screw& screw);
void DrawFan(const Fan& fan);
void DrawPinwheel(const Pinwheel& pinwheel);
void DrawWindStreaks(const Fan& fan);
void DrawRamp(const Ramp& ramp);
void DrawTrapDoor(const TrapDoor& trapDoor);
void DrawSeeSaw(const SeeSaw& seeSaw);
void DrawChain(const Chain& chain, Texture2D texture);
void DrawPhysicsRope(const PhysicsRope& rope);
void DrawButton(const Button& button);
void DrawArrowTrap(const ArrowTrap& trap);
void DrawArrowProjectile(const ArrowProjectile& arrow);
void DrawBreakableTile(Texture2D texture, const BreakableTile& tile);
void DrawTiledTextureRect(Texture2D texture, Rectangle sourceTile, Rectangle dest, Color tint);
void DrawRepeatingTexture(Texture2D texture, Rectangle dest, Color tint);
void DrawTilesetTile(Texture2D texture, int column, int row, Vector2 position, Color tint);
void DrawTilesetBackgroundFill(Texture2D texture, Rectangle dest, Color tint, float detailOpacity = 0.28f);
void DrawTilesetSolidFill(Texture2D texture, Rectangle dest, Color tint);
void DrawTilesetWall(Texture2D texture, Rectangle dest, Color tint);
void DrawTilesetCeiling(Texture2D texture, Rectangle dest, Color tint);
void DrawTilesetPitWalls(Texture2D texture, Rectangle pit, Color tint);
void DrawTilesetPitFoundation(Texture2D texture, Rectangle dest, Color tint);
void DrawTilesetSolid(Texture2D texture, Rectangle dest, Color tint);
