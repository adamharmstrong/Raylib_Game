#pragma once
#include "Level.h"
#include <string>
#include <vector>

enum class ConnectionNodeKind { GuideObject, Pulley, Weight, Button, TrapDoor, Fan, Valve, Fluid, Exit, AuthoredTarget, PlatformLoop };
enum class ConnectionLinkKind { Power, Attachment, Pulley, Control };
struct ConnectionNode {
    ConnectionNodeKind kind;
    int index;
    Vector2 position;
    Rectangle bounds;
    std::string label;
    std::string warning;
};
struct ConnectionLink {
    int from, to;
    Vector2 start, end;
    ConnectionLinkKind kind;
    int channel{0};
    std::string recordType;
    int recordIndex{-1};
};
struct ConnectionPath { int node; std::vector<Vector2> points; };
struct EditorConnections {
    std::vector<ConnectionNode> nodes;
    std::vector<ConnectionLink> links;
    std::vector<ConnectionPath> paths;
};
EditorConnections BuildEditorConnections(const Level& level);
std::vector<int> ConnectedEditorNodes(const EditorConnections& graph, int node);

struct EditorControlTarget {
    std::string type;
    int index{-1};
    Rectangle bounds{};
};
EditorControlTarget PickEditorControlTarget(const Level& level, Vector2 point, bool valve);
inline constexpr const char* EditableConnectionTypes[] = {"buttonTrapDoor", "buttonFan", "buttonExit",
    "buttonLadder", "buttonPlatform", "buttonSpikeHazard", "valveFluidFill", "buttonPlatformLoop", "platformLoopButton"};
int PickEditorConnection(const EditorConnections& graph, Vector2 point, float tolerance);
