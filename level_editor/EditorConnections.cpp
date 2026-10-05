#include "EditorConnections.h"
#include "GuideObjects.h"
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
