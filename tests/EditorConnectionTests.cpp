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
    level = Level{};
    level.buttons.push_back({{10, 20, 32, 16}});
    TrapDoor door{}; door.hinge = {200, 100}; door.length = 80; door.thickness = 16;
    level.trapDoors.push_back(door);
    Fan fan{}; fan.center = {300, 100}; level.fans.push_back(fan);
    level.buttonTrapDoorLinks.push_back({0, 0, 75, 120});
    level.buttonFanLinks.push_back({0, 0, 1});
    level.buttonExitLink.buttonIndex = 0;
    level.buttonLadderLinks.push_back({0, {200, 200, 32, 100}});
    level.buttonPlatformLinks.push_back({0, {300, 200, 80, 16}});
    ButtonSpikeLink spikes{}; spikes.buttonIndex = 0; spikes.hazard.rect = {400, 200, 80, 16};
    level.buttonSpikeLinks.push_back(spikes);
    FluidField fluid{}; fluid.bounds = {500, 100, 100, 100}; level.fluids.push_back(fluid);
    level.valve.center = {450, 100}; level.valveFluidFill = {0, 1, 86};
    graph = BuildEditorConnections(level);
    Require(graph.links.size() == 7, "Every button target and valve fill must produce a control link.");
    for (const auto& link : graph.links)
        Require(link.kind == ConnectionLinkKind::Control, "Authored controls must use their distinct overlay kind.");
    int buttonNode = -1;
    for (int i = 0; i < static_cast<int>(graph.nodes.size()); ++i)
        if (graph.nodes[i].kind == ConnectionNodeKind::Button) buttonNode = i;
    Require(ConnectedEditorNodes(graph, buttonNode).size() == 7, "Button networks must include all six controlled targets.");
    level.buttonFanLinks[0].fanIndex = 99;
    level.valveFluidFill.fluidIndex = 99;
    graph = BuildEditorConnections(level);
    Require(!graph.nodes[buttonNode].warning.empty(), "Broken indexed targets must show a warning.");
    Require(!graph.nodes.back().warning.empty(), "Missing valve fluid targets must show a warning.");
    level = Level{};
    level.buttons.push_back({{10, 20, 32, 16}});
    ButtonPlatformLoop loop{};
    loop.center = {400, 300}; loop.buttonIndex = 0;
    UpdateButtonPlatformLoopPositions(loop);
    level.buttonPlatformLoops.push_back(loop);
    level.platformLoopButtonLinks.push_back({0, 0, 2});
    UpdatePlatformLoopButtonPositions(level);
    graph = BuildEditorConnections(level);
    Require(graph.paths.size() == 1 && graph.paths[0].points.size() == 65,
        "The full loop path must be visible in the editor.");
    Require(graph.links.size() == 2, "Loop activation and riding membership must both appear.");
    level.platformLoopButtonLinks[0].platformIndex = 99;
    graph = BuildEditorConnections(level);
    Require(!graph.nodes[0].warning.empty(), "Invalid riding-platform indices must show a warning.");

    level = Level{};
    level.exitTrigger = {500, 100, 32, 64};
    level.pitPlatforms = {{100, 100, 80, 16}};
    level.ladders = {{200, 100, 32, 100}};
    level.spikeHazard = {300, 100, 80, 16};
    DirectionalSpikeHazard hazard{}; hazard.rect = {400, 100, 80, 16};
    level.directionalSpikeHazards.push_back(hazard);
    Fan pickFan{}; pickFan.center = {600, 100}; level.fans.push_back(pickFan);
    TrapDoor pickDoor{}; pickDoor.hinge = {700, 100}; pickDoor.length = 80; pickDoor.thickness = 16;
    level.trapDoors.push_back(pickDoor);
    for (const auto& [point, expected] : std::vector<std::pair<Vector2, std::string>>{
        {{110, 105}, "platform"}, {{210, 150}, "ladder"}, {{310, 105}, "spikeHazard"},
        {{410, 105}, "directionalSpikeHazard"}, {{510, 120}, "exit"}, {{600, 100}, "fan"}, {{730, 100}, "trapDoor"}}) {
        const auto target = PickEditorControlTarget(level, point, false);
        Require(target.type == expected && target.index == 0, "Viewport target picking must identify each supported button target.");
        Require(PickEditorControlTarget(level, point, true).index == -1, "Valves must reject non-fluid targets.");
    }
    FluidField pickFluid{}; pickFluid.bounds = {100, 300, 100, 100};
    level.fluids = {pickFluid, pickFluid};
    Require(PickEditorControlTarget(level, {150, 350}, true).index == 1,
        "Overlapping fluid regions must pick the last authored region consistently.");
    Require(PickEditorControlTarget(level, {150, 350}, false).index == -1,
        "Buttons must reject fluid-only regions.");
    ButtonPlatformLoop pickLoop{}; pickLoop.center = {1000, 500};
    UpdateButtonPlatformLoopPositions(pickLoop); level.buttonPlatformLoops.push_back(pickLoop);
    const auto platformBounds = pickLoop.platforms[2];
    auto pickedLoop = PickEditorControlTarget(level, {platformBounds.x + 1, platformBounds.y + 1}, false);
    Require(pickedLoop.type == "buttonPlatformLoop" && pickedLoop.index == 0,
        "Clicking any loop platform must resolve its owning loop.");
    Require(PickEditorControlTarget(level, {-100, -100}, false).index == -1,
        "Empty canvas must not create a target.");
    EditorConnections wireGraph;
    wireGraph.links.push_back({0, 1, {0, 0}, {100, 0}, ConnectionLinkKind::Control, 0, "buttonFan", 2});
    wireGraph.links.push_back({0, 2, {0, 20}, {100, 20}, ConnectionLinkKind::Control, 0, "buttonTrapDoor", 1});
    wireGraph.links.push_back({0, 3, {0, 10}, {100, 10}, ConnectionLinkKind::Power, 7});
    Require(PickEditorConnection(wireGraph, {50, 3}, 6) == 0, "Near-line clicks must select the nearest authored control.");
    Require(wireGraph.links[0].recordType == "buttonFan" && wireGraph.links[0].recordIndex == 2,
        "Selected lines must identify the exact saved record, including its ordinal.");
    Require(PickEditorConnection(wireGraph, {50, 17}, 6) == 1, "Parallel links must resolve independently.");
    Require(PickEditorConnection(wireGraph, {50, 10}, 2) == -1, "Power overlays must not masquerade as removable link records.");
    Require(PickEditorConnection(wireGraph, {1, 0}, 6) == -1, "Endpoint clicks must remain available for object selection.");
    Require(PickEditorConnection(wireGraph, {50, 40}, 6) == -1, "Empty canvas must not select a wire.");
    wireGraph.links.push_back({0, 1, {50, 0}, {50, 0}, ConnectionLinkKind::Control, 0, "buttonExit", 0});
    Require(PickEditorConnection(wireGraph, {50, 0}, 6) == 0, "Zero-length links must be ignored safely.");
    std::cout << "Editor connection tests passed.\n";
}
