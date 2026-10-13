#include "EditorConnections.h"
#include "GuideObjects.h"
#include "EditorLoops.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace {
Vector2 Center(Rectangle bounds) { return {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f}; }
bool PowerSource(const GuideObject& object) {
    return object.active && !object.broken &&
        (((object.type == GuideObjectType::Battery || object.type == GuideObjectType::Generator) && object.power.maximumPower > 0.01f) ||
        object.type == GuideObjectType::LimitSwitch || object.type == GuideObjectType::SpeedSensor || object.type == GuideObjectType::BeamSensor);
}
bool LinearMotion(GuideObjectType type) {
    return type == GuideObjectType::MovingPlatform || type == GuideObjectType::Elevator ||
        type == GuideObjectType::CrushingBlock || type == GuideObjectType::Piston ||
        type == GuideObjectType::HydraulicCylinder || type == GuideObjectType::SawBlade;
}
}

EditorControlTarget PickEditorControlTarget(const Level& level, Vector2 point, bool valve) {
    const auto hit = [&](Rectangle bounds, const char* type, int index) -> EditorControlTarget {
        return bounds.width > 0 && bounds.height > 0 && CheckCollisionPointRec(point, bounds)
            ? EditorControlTarget{type, index, bounds} : EditorControlTarget{};
    };
    if (valve) {
        for (int i = static_cast<int>(level.fluids.size()) - 1; i >= 0; --i) {
            auto target = hit(level.fluids[i].bounds, "fluid", i);
            if (target.index >= 0) return target;
        }
        return {};
    }
    for (int i = static_cast<int>(level.buttonPlatformLoops.size()) - 1; i >= 0; --i)
        for (const auto& platform : level.buttonPlatformLoops[i].platforms) {
            auto target = hit(platform, "buttonPlatformLoop", i);
            if (target.index >= 0) return target;
        }
    for (int i = static_cast<int>(level.trapDoors.size()) - 1; i >= 0; --i) {
        const auto& door = level.trapDoors[i];
        const Vector2 end{door.hinge.x + cosf(door.angle * DEG2RAD) * door.length,
            door.hinge.y + sinf(door.angle * DEG2RAD) * door.length};
        const float margin = std::max(4.0f, door.thickness * 0.5f);
        auto target = hit({std::min(door.hinge.x, end.x) - margin, std::min(door.hinge.y, end.y) - margin,
            fabsf(end.x - door.hinge.x) + margin * 2, fabsf(end.y - door.hinge.y) + margin * 2}, "trapDoor", i);
        if (target.index >= 0) return target;
    }
    for (int i = static_cast<int>(level.fans.size()) - 1; i >= 0; --i) {
        const auto p = level.fans[i].center;
        auto target = hit({p.x - 28, p.y - 28, 56, 56}, "fan", i);
        if (target.index >= 0) return target;
    }
    for (int i = static_cast<int>(level.directionalSpikeHazards.size()) - 1; i >= 0; --i) {
        auto target = hit(level.directionalSpikeHazards[i].rect, "directionalSpikeHazard", i);
        if (target.index >= 0) return target;
    }
    for (const auto& [bounds, type] : {std::pair{level.spikeHazard, "spikeHazard"}, std::pair{level.exitTrigger, "exit"}}) {
        auto target = hit(bounds, type, 0);
        if (target.index >= 0) return target;
    }
    for (int i = static_cast<int>(level.ladders.size()) - 1; i >= 0; --i) {
        auto target = hit(level.ladders[i], "ladder", i);
        if (target.index >= 0) return target;
    }
    for (int i = static_cast<int>(level.pitPlatforms.size()) - 1; i >= 0; --i) {
        auto target = hit(level.pitPlatforms[i], "platform", i);
        if (target.index >= 0) return target;
    }
    return {};
}

EditorConnections BuildEditorConnections(const Level& level) {
    EditorConnections graph;
    std::map<int, std::vector<int>> channels;
    std::map<GuideObjectType, int> ordinals;
    for (int index = 0; index < static_cast<int>(level.guideObjects.size()); ++index) {
        const auto& object = level.guideObjects[index];
        const auto bounds = GetGuideObjectBounds(object);
        std::string label = std::string(GetGuideObjectName(object.type)) + " " + std::to_string(++ordinals[object.type]);
        if (object.power.channel != 0) {
            label += " [CH " + std::to_string(object.power.channel) + "]";
            channels[object.power.channel].push_back(index);
        }
        graph.nodes.push_back({ConnectionNodeKind::GuideObject, index, Center(bounds), bounds, label, {}});
        const Vector2 centerOffset{bounds.width * 0.5f, bounds.height * 0.5f};
        if (LinearMotion(object.type)) {
            // Circular objects store a center; rectangles store their top left.
            Vector2 start = object.origin;
            if (object.collider.shape == ColliderShape::Rectangle) {
                start.x += centerOffset.x; start.y += centerOffset.y;
            }
            graph.paths.push_back({index, {start, {start.x + object.direction.x * object.length,
                start.y + object.direction.y * object.length}}});
        }
        if (object.type == GuideObjectType::PendulumBob || object.type == GuideObjectType::SwingingHammer) {
            ConnectionPath path{index, {}};
            const float extent = object.type == GuideObjectType::SwingingHammer ? 1.05f : 0.78f;
            for (int sample = 0; sample <= 32; ++sample) {
                const float angle = -extent + extent * 2.0f * sample / 32.0f;
                path.points.push_back({object.origin.x + sinf(angle) * object.length,
                    object.origin.y + cosf(angle) * object.length});
            }
            graph.paths.push_back(std::move(path));
        }
    }
    for (const auto& [channel, members] : channels) {
        const auto source = std::find_if(members.begin(), members.end(), [&](int node) { return PowerSource(level.guideObjects[node]); });
        const int hub = source == members.end() ? members.front() : *source;
        for (int node : members) {
            if (source == members.end()) graph.nodes[node].warning = "No power source on CH " + std::to_string(channel);
            if (node != hub) graph.links.push_back({hub, node, graph.nodes[hub].position, graph.nodes[node].position,
                ConnectionLinkKind::Power, channel});
        }
    }
    for (int index = 0; index < static_cast<int>(level.guideObjects.size()); ++index) {
        const auto& object = level.guideObjects[index];
        if (!IsGuideAttachmentConstraint(object.type)) continue;
        const int target = FindGuideAttachmentTarget(level.guideObjects, index);
        if (target < 0) {
            graph.nodes[index].warning = "No attachment body within 42 px on this layer";
        }
        else graph.links.push_back({index, target,
            object.type == GuideObjectType::FixedJoint ? object.transform.position : object.constraint.anchorB,
            graph.nodes[target].position, ConnectionLinkKind::Attachment});
        // Always show both spring/rod anchors, including unresolved endpoints.
        if (object.type != GuideObjectType::FixedJoint)
            graph.paths.push_back({index, {object.constraint.anchorA, object.constraint.anchorB}});
    }
    const int pulleyOffset = static_cast<int>(graph.nodes.size());
    for (int index = 0; index < static_cast<int>(level.pulleys.size()); ++index) {
        const auto point = level.pulleys[index];
        graph.nodes.push_back({ConnectionNodeKind::Pulley, index, point,
            {point.x - 20, point.y - 20, 40, 40}, "Pulley " + std::to_string(index + 1), {}});
    }
    for (int index = 0; index < static_cast<int>(level.weights.size()); ++index) {
        const auto& weight = level.weights[index];
        const int node = static_cast<int>(graph.nodes.size());
        graph.nodes.push_back({ConnectionNodeKind::Weight, index, Center(weight.rect), weight.rect,
            "Hanging Weight " + std::to_string(index + 1), {}});
        int pulley = -1;
        for (int candidate = 0; candidate < static_cast<int>(level.pulleys.size()); ++candidate) {
            const auto point = level.pulleys[candidate];
            if (fabsf(point.x - weight.pulley.x) < 0.01f && fabsf(point.y - weight.pulley.y) < 0.01f) { pulley = candidate; break; }
        }
        if (pulley < 0) graph.nodes[node].warning = "Missing pulley at weight's anchor";
        else graph.links.push_back({pulleyOffset + pulley, node, weight.pulley, graph.nodes[node].position, ConnectionLinkKind::Pulley});
    }
    const auto addNode = [&](ConnectionNodeKind kind, int index, Rectangle bounds, std::string label) {
        const int node = static_cast<int>(graph.nodes.size());
        graph.nodes.push_back({kind, index, Center(bounds), bounds, std::move(label), {}});
        return node;
    };
    std::vector<int> buttons, doors, fans, fluids;
    for (int i = 0; i < static_cast<int>(level.buttons.size()); ++i)
        buttons.push_back(addNode(ConnectionNodeKind::Button, i, level.buttons[i].rect, "Button " + std::to_string(i + 1)));
    for (int i = 0; i < static_cast<int>(level.trapDoors.size()); ++i) {
        const auto& door = level.trapDoors[i];
        doors.push_back(addNode(ConnectionNodeKind::TrapDoor, i,
            {door.hinge.x, door.hinge.y - door.thickness * 0.5f, door.length, door.thickness}, "Trap Door " + std::to_string(i + 1)));
    }
    for (int i = 0; i < static_cast<int>(level.fans.size()); ++i) {
        const auto p = level.fans[i].center;
        fans.push_back(addNode(ConnectionNodeKind::Fan, i, {p.x - 20, p.y - 20, 40, 40}, "Fan " + std::to_string(i + 1)));
    }
    for (int i = 0; i < static_cast<int>(level.fluids.size()); ++i)
        fluids.push_back(addNode(ConnectionNodeKind::Fluid, i, level.fluids[i].bounds, "Fluid Field " + std::to_string(i + 1)));
    const int exit = addNode(ConnectionNodeKind::Exit, 0, level.exitTrigger, "Exit Trigger");
    const auto link = [&](int source, int target, const char* recordType, int recordIndex) {
        if (source < 0 || target < 0) return;
        graph.links.push_back({source, target, graph.nodes[source].position, graph.nodes[target].position,
            ConnectionLinkKind::Control, 0, recordType, recordIndex});
    };
    const auto indexed = [&](const std::vector<int>& nodes, int index) {
        return index >= 0 && index < static_cast<int>(nodes.size()) ? nodes[index] : -1;
    };
    const auto control = [&](int buttonIndex, int target, const char* recordType, int recordIndex) {
        const int source = indexed(buttons, buttonIndex);
        if (source < 0) graph.nodes[target].warning = "Missing button";
        else link(source, target, recordType, recordIndex);
    };
    for (int i = 0; i < static_cast<int>(level.buttonTrapDoorLinks.size()); ++i) {
        const auto& item = level.buttonTrapDoorLinks[i];
        const int target = indexed(doors, item.trapDoorIndex);
        if (target >= 0) control(item.buttonIndex, target, "buttonTrapDoor", i);
        else if (const int source = indexed(buttons, item.buttonIndex); source >= 0) graph.nodes[source].warning = "Missing trap door";
    }
    for (int i = 0; i < static_cast<int>(level.buttonFanLinks.size()); ++i) {
        const auto& item = level.buttonFanLinks[i];
        const int target = indexed(fans, item.fanIndex);
        if (target >= 0) control(item.buttonIndex, target, "buttonFan", i);
        else if (const int source = indexed(buttons, item.buttonIndex); source >= 0) graph.nodes[source].warning = "Missing fan";
    }
    if (level.buttonExitLink.buttonIndex >= 0) control(level.buttonExitLink.buttonIndex, exit, "buttonExit", 0);
    for (int i = 0; i < static_cast<int>(level.buttonLadderLinks.size()); ++i) {
        const auto& item = level.buttonLadderLinks[i];
        control(item.buttonIndex, addNode(ConnectionNodeKind::AuthoredTarget, i, item.ladder, "Button Ladder " + std::to_string(i + 1)), "buttonLadder", i);
    }
    for (int i = 0; i < static_cast<int>(level.buttonPlatformLinks.size()); ++i) {
        const auto& item = level.buttonPlatformLinks[i];
        control(item.buttonIndex, addNode(ConnectionNodeKind::AuthoredTarget, i, item.platform, "Button Platform " + std::to_string(i + 1)), "buttonPlatform", i);
    }
    for (int i = 0; i < static_cast<int>(level.buttonSpikeLinks.size()); ++i) {
        const auto& item = level.buttonSpikeLinks[i];
        control(item.buttonIndex, addNode(ConnectionNodeKind::AuthoredTarget, i, item.hazard.rect, "Button Spikes " + std::to_string(i + 1)), "buttonSpikeHazard", i);
    }
    std::vector<int> loops;
    for (int i = 0; i < static_cast<int>(level.buttonPlatformLoops.size()); ++i) {
        const auto& loop = level.buttonPlatformLoops[i];
        const int node = addNode(ConnectionNodeKind::PlatformLoop, i,
            EditorLoopBounds(loop), "Platform Loop " + std::to_string(i + 1));
        loops.push_back(node);
        ConnectionPath path{node, {}};
        for (int sample = 0; sample <= 64; ++sample)
            path.points.push_back(GetButtonPlatformLoopPoint(loop, sample / 64.0f));
        graph.paths.push_back(std::move(path));
        if (loop.buttonIndex >= 0) control(loop.buttonIndex, node, "buttonPlatformLoop", i);
    }
    for (int i = 0; i < static_cast<int>(level.platformLoopButtonLinks.size()); ++i) {
        const auto& rider = level.platformLoopButtonLinks[i];
        const int source = indexed(buttons, rider.buttonIndex);
        const int target = indexed(loops, rider.loopIndex);
        if (source < 0) continue;
        if (target < 0 || rider.platformIndex < 0 || rider.platformIndex >= level.buttonPlatformLoops[rider.loopIndex].platformCount)
            graph.nodes[source].warning = "Missing loop platform";
        else link(source, target, "platformLoopButton", i);
    }
    if (level.valveFluidFill.fluidIndex >= 0) {
        const auto p = level.valve.center;
        const int valve = addNode(ConnectionNodeKind::Valve, 0, {p.x - 16, p.y - 16, 32, 32}, "Valve");
        const int target = indexed(fluids, level.valveFluidFill.fluidIndex);
        if (target < 0) graph.nodes[valve].warning = "Missing fluid region";
        else link(valve, target, "valveFluidFill", 0);
    }
    return graph;
}

std::vector<int> ConnectedEditorNodes(const EditorConnections& graph, int node) {
    if (node < 0 || node >= static_cast<int>(graph.nodes.size())) return {};
    std::vector<int> found{node};
    for (size_t cursor = 0; cursor < found.size(); ++cursor) {
        for (const auto& link : graph.links) {
            const int other = link.from == found[cursor] ? link.to : link.to == found[cursor] ? link.from : -1;
            if (other >= 0 && std::find(found.begin(), found.end(), other) == found.end()) found.push_back(other);
        }
    }
    return found;
}

int PickEditorConnection(const EditorConnections& graph, Vector2 point, float tolerance) {
    int picked = -1;
    float best = tolerance * tolerance;
    for (int i = 0; i < static_cast<int>(graph.links.size()); ++i) {
        const auto& link = graph.links[i];
        if (link.recordType.empty() || link.recordIndex < 0) continue;
        const Vector2 delta{link.end.x - link.start.x, link.end.y - link.start.y};
        const float lengthSq = delta.x * delta.x + delta.y * delta.y;
        if (lengthSq < 0.001f) continue;
        const float t = std::clamp(((point.x - link.start.x) * delta.x + (point.y - link.start.y) * delta.y) / lengthSq, 0.0f, 1.0f);
        // Keep endpoint clicks available for selecting the objects themselves.
        if (t * std::sqrt(lengthSq) < tolerance * 1.5f || (1 - t) * std::sqrt(lengthSq) < tolerance * 1.5f) continue;
        const float dx = point.x - link.start.x - delta.x * t, dy = point.y - link.start.y - delta.y * t;
        const float distance = dx * dx + dy * dy;
        if (distance <= best) { picked = i; best = distance; }
    }
    return picked;
}
