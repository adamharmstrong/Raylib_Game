#pragma once

#include "Level.h"
#include "Player.h"
#include "raylib.h"

void DrawPulley(Vector2 center, float radius, float rotation, Color color);
void DrawWinch(const Winch& winch);
void DrawSpikes(float startX, float baseY, int count);
void DrawHazardWeight(const HangingWeight& weight);
void DrawOutdoorDoorway(Rectangle doorway);
void DrawMachineBox(Rectangle rect, float gearRotation, bool running);
void DrawPlayer(const Player& player, Texture2D texture, int frameCount = 3, Rectangle frameCrop = {0, 0, 0, 0});
void DrawEnemy(const Enemy& enemy, Texture2D texture);
void DrawRotaryLatch(const RotaryLatch& latch, bool playerNear, const char* interactPrompt);
void DrawValve(const Valve& valve, bool playerNear, const char* interactPrompt);
void DrawWaterPit(const WaterPit& waterPit);
void DrawStoneBlock(const StoneBlock& block);
void DrawSeeSaw(const SeeSaw& seeSaw);
void DrawChain(const Chain& chain, Texture2D texture);
void DrawTiledTextureRect(Texture2D texture, Rectangle sourceTile, Rectangle dest, Color tint);
void DrawRepeatingTexture(Texture2D texture, Rectangle dest, Color tint);
void DrawAtomicRealmBackgroundFill(Texture2D texture, Rectangle dest, Color tint);
void DrawAtomicRealmSolidFill(Texture2D texture, Rectangle dest, Color tint);
void DrawAtomicRealmPitWalls(Texture2D texture, Rectangle pit, Color tint);
void DrawAtomicRealmPitFoundation(Texture2D texture, Rectangle dest, Color tint);
void DrawAtomicRealmSolid(Texture2D texture, Rectangle dest, Color tint);
