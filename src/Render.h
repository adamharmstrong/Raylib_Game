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
void DrawPlayer(const Player& player, Texture2D texture);
void DrawRotaryLatch(const RotaryLatch& latch, bool playerNear);
void DrawTiledTextureRect(Texture2D texture, Rectangle sourceTile, Rectangle dest, Color tint);
