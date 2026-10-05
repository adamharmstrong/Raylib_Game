#include "EditorConnections.h"
#include "GuideObjects.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
GuideObject Object(const char* record) {
    std::istringstream stream(record);
    std::string command;
    stream >> command;
    GuideObject object;
    Require(ParseGuideObject(command, stream, object), "Test record must parse.");
    return object;
}
bool Near(float a, float b) { return fabsf(a - b) < 0.01f; }
}

int main() {
    Level level{};
    level.pulleys.clear();
    level.weights.clear();
    level.guideObjects = {
        Object("battery 0 0 32 32 1 7"),
        Object("electricMotor 200 100 24 120 7"),
        Object("electricMotor 400 100 24 120 0"),
        Object("electricMotor 600 100 24 120 9"),
        Object("relay 800 100 32 32 7")
    };
    level.guideObjects[1].layer = WorldLayer::Foreground;
    auto graph = BuildEditorConnections(level);
    Require(graph.links.size() == 2, "Only shared nonzero channels should have links.");
    Require(ConnectedEditorNodes(graph, 1).size() == 3, "Channel selection should include source, motor, and relay across layers.");
    Require(ConnectedEditorNodes(graph, 2).size() == 1, "Channel zero should operate independently.");
    Require(graph.nodes[3].warning.find("CH 9") != std::string::npos, "An unsupplied motor should identify its channel.");
    Require(graph.nodes[1].warning.empty(), "A conditional supply should count as a source without claiming it is powered.");
    level.guideObjects[0].broken = true;
    Require(!BuildEditorConnections(level).nodes[1].warning.empty(), "A broken supply must not count as a valid power source.");
    level.guideObjects[0] = Object("limitSwitch 0 0 32 32 7");
    Require(BuildEditorConnections(level).nodes[1].warning.empty(), "Sensors should be recognized as conditional channel sources.");

    level.guideObjects = {
        Object("spring 0 100 100 100 8 10 1"),
        Object("ball 142 100 8 1"),
        Object("ball 101 100 8 1"),
        Object("ball 105 100 8 1")
    };
    level.guideObjects[2].layer = WorldLayer::Foreground;
    level.guideObjects[3].broken = true;
    Require(FindGuideAttachmentTarget(level.guideObjects, 0) == 1, "Attachments must include the 42 px boundary and ignore other layers and broken bodies.");
    graph = BuildEditorConnections(level);
    Require(graph.links.size() == 1 && graph.links[0].to == 1, "Overlay attachment must use the runtime resolver.");
    Require(Near(graph.links[0].start.x, 100) && Near(graph.links[0].end.x, 142), "Attachment link should run from authored endpoint to actual body center.");
    level.guideObjects[1].transform.position.x = 143;
    Require(FindGuideAttachmentTarget(level.guideObjects, 0) == -1, "Bodies beyond 42 px must not attach.");
    Require(!BuildEditorConnections(level).nodes[0].warning.empty(), "Unresolved attachments should be highlighted.");
    level.guideObjects[1] = Object("ball 110 100 8 1");
    level.guideObjects[3] = Object("ball 90 100 8 1");
    Require(FindGuideAttachmentTarget(level.guideObjects, 0) == 3, "Equal-distance ties must retain the runtime's last-match rule.");
    level.guideObjects[0] = Object("fixedJoint 100 100 8");
    Require(FindGuideAttachmentTarget(level.guideObjects, 0) == 3, "Fixed joints should resolve from their position.");
    Require(FindGuideAttachmentTarget(level.guideObjects, -1) == -1 && FindGuideAttachmentTarget(level.guideObjects, 1) == -1,
        "Invalid indices and non-constraint objects must not resolve attachments.");

    level.guideObjects = {Object("movingPlatform 10 20 80 16 1 0 100 2"), Object("pendulumBob 200 50 100 12 0 1")};
    graph = BuildEditorConnections(level);
    Require(graph.paths.size() == 2, "Both linear travel and pendulum swing should have paths.");
    Require(Near(graph.paths[0].points.front().x, 50) && Near(graph.paths[0].points.back().x, 150),
        "Rectangle paths should show center travel from origin through the complete authored distance.");
    Require(graph.paths[1].points.size() == 33 && Near(graph.paths[1].points[16].x, 200) && Near(graph.paths[1].points[16].y, 150),
        "Pendulum swing arc should include the bottom of the runtime trajectory.");

    level.guideObjects.clear();
    level.pulleys = {{20, 20}};
    HangingWeight weight{};
    weight.pulley = {20, 20};
    weight.rect = {10, 90, 20, 20};
    level.weights = {weight};
    graph = BuildEditorConnections(level);
    Require(graph.links.size() == 1 && ConnectedEditorNodes(graph, 0).size() == 2, "Pulley networks must include their hanging weights.");
    level.weights[0].pulley = {999, 999};
    graph = BuildEditorConnections(level);
    Require(graph.links.empty() && !graph.nodes[1].warning.empty(), "Weights with missing pulley anchors should be highlighted.");
    Require(ConnectedEditorNodes(graph, -1).empty(), "No selection must not resolve a network.");
    std::cout << "Editor connection tests passed.\n";
}
