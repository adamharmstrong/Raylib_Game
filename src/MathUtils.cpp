#include "MathUtils.h"

#include <cmath>

float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float LerpFloat(float start, float end, float amount) {
    return start + (end - start) * amount;
}

float NormalizeAngle(float angle) {
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}

bool IsAngleAligned(float angle, float targetAngle, float tolerance) {
    angle = NormalizeAngle(angle);
    targetAngle = NormalizeAngle(targetAngle);

    float difference = fabsf(angle - targetAngle);
    if (difference > 180.0f) {
        difference = 360.0f - difference;
    }

    return difference <= tolerance;
}

bool NearRect(Rectangle a, Rectangle b, float padding) {
    Rectangle expanded{
        b.x - padding,
        b.y - padding,
        b.width + padding * 2.0f,
        b.height + padding * 2.0f
    };

    return CheckCollisionRecs(a, expanded);
}
