#pragma once
#include "Level.h"
bool ScriptTarget(Level& level, const std::string& type, int ordinal,
    Rectangle& bounds, const Vector2* delta = nullptr);
bool ScriptOverlap(Rectangle a, Rectangle b);
