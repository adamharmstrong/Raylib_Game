#pragma once

#include "raylib.h"

float Clamp01(float value);
float LerpFloat(float start, float end, float amount);
float NormalizeAngle(float angle);
bool IsAngleAligned(float angle, float targetAngle, float tolerance);
bool NearRect(Rectangle a, Rectangle b, float padding);
