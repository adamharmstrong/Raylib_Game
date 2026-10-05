#pragma once
#include "Level.h"
#include <string>
#include <vector>

enum class ConnectionNodeKind { GuideObject, Pulley, Weight };
enum class ConnectionLinkKind { Power, Attachment, Pulley };
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
};
struct ConnectionPath { int node; std::vector<Vector2> points; };
struct EditorConnections {
    std::vector<ConnectionNode> nodes;
    std::vector<ConnectionLink> links;
    std::vector<ConnectionPath> paths;
};
EditorConnections BuildEditorConnections(const Level& level);
std::vector<int> ConnectedEditorNodes(const EditorConnections& graph, int node);
