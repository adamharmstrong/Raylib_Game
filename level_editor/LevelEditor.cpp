#include "LevelEditor.h"

#include "GuideObjects.h"
#include "Render.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <queue>
#include <system_error>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#include <commdlg.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#endif

namespace {
constexpr int InitialWindowWidth = 1440;
constexpr int InitialWindowHeight = 900;
constexpr float ToolbarHeight = 64.0f;
constexpr float StatusBarHeight = 26.0f;
constexpr float AssetRowHeight = 38.0f;
constexpr float ContentFolderWidth = 170.0f;

constexpr Color WindowBackground{20, 23, 29, 255};
constexpr Color PanelBackground{29, 33, 41, 255};
constexpr Color PanelBorder{55, 62, 74, 255};
constexpr Color CanvasBackground{15, 18, 23, 255};
constexpr Color PrimaryText{226, 231, 239, 255};
constexpr Color SecondaryText{145, 154, 171, 255};
constexpr Color Accent{69, 154, 255, 255};
constexpr Color Warning{235, 184, 87, 255};
constexpr Color PanelHeader{35, 40, 49, 255};
constexpr Color PanelInset{23, 26, 32, 255};

unsigned long long TileCellKey(int x, int y) {
    return (static_cast<unsigned long long>(static_cast<unsigned int>(x)) << 32) |
        static_cast<unsigned int>(y);
}

int TileLayerBit(TileLayer layer) {
    return 1 << static_cast<int>(layer);
}

const char* TileLayerName(TileLayer layer) {
    switch (layer) {
    case TileLayer::FarBackground: return "farBackground";
    case TileLayer::Background: return "background";
    case TileLayer::Foreground: return "foreground";
    case TileLayer::Collision: return "collision";
    case TileLayer::User1: return "user1";
    case TileLayer::User2: return "user2";
    case TileLayer::User3: return "user3";
    case TileLayer::User4: return "user4";
    case TileLayer::User5: return "user5";
    case TileLayer::User6: return "user6";
    case TileLayer::User7: return "user7";
    case TileLayer::User8: return "user8";
    }
    return "foreground";
}

TileLayer TileLayerFromId(int id) {
    return id >= 0 && id <= 11 ? static_cast<TileLayer>(id) : TileLayer::Foreground;
}

struct AssetEntry {
    const char* name;
    const char* category;
    LevelEditor::EditTool tool;
    Color color;
};

constexpr AssetEntry Assets[] = {
    {"Solid", "GEOMETRY", LevelEditor::EditTool::Solid, Color{83, 218, 157, 255}},
    {"Platform", "", LevelEditor::EditTool::Platform, Color{91, 190, 235, 255}},
    {"Ladder", "", LevelEditor::EditTool::Ladder, Color{226, 166, 83, 255}},
    {"Camera Zone", "REGIONS", LevelEditor::EditTool::CameraZone, Color{69, 154, 255, 255}},
    {"Darkness", "", LevelEditor::EditTool::Darkness, Color{133, 113, 184, 255}},
    {"Player Start", "MARKERS", LevelEditor::EditTool::PlayerStart, Color{255, 207, 76, 255}},
    {"Exit", "", LevelEditor::EditTool::Exit, Color{240, 91, 109, 255}},
};

struct CatalogEntry {
    std::string name;
    std::string category;
    LevelEditor::EditTool tool{LevelEditor::EditTool::Select};
    bool placeable{false};
};

const std::vector<CatalogEntry>& AssetCatalog() {
    static const std::vector<CatalogEntry> catalog = [] {
        std::vector<CatalogEntry> entries{
            {"Solid", "Geometry", LevelEditor::EditTool::Solid, true},
            {"Platform", "Geometry", LevelEditor::EditTool::Platform, true},
            {"Ladder", "Geometry", LevelEditor::EditTool::Ladder, true},
            {"Visual Tile", "Geometry"}, {"Breakable Tile", "Geometry"}, {"Ramp", "Geometry"},
            {"Player Start", "Markers", LevelEditor::EditTool::PlayerStart, true},
            {"Exit", "Markers", LevelEditor::EditTool::Exit, true},
            {"Label", "Markers"}, {"Enemy", "Characters"},
            {"Camera Zone", "Volumes", LevelEditor::EditTool::CameraZone, true},
            {"Darkness", "Volumes", LevelEditor::EditTool::Darkness, true},
            {"Water", "Fluids"}, {"Sand", "Fluids"}, {"Gel", "Fluids"}, {"Gas", "Fluids"},
            {"Water Pit", "Fluids"}, {"Valve", "Machines"}, {"Pulley", "Machines"},
            {"Hanging Weight", "Machines"}, {"Rotary Latch", "Machines"}, {"Gear", "Machines"},
            {"Mounted Gear", "Machines"}, {"Flywheel", "Machines"}, {"Steering Wheel", "Machines"},
            {"Screw", "Machines"}, {"Fan", "Machines"}, {"Pinwheel", "Machines"},
            {"Stone Block", "Physics"}, {"Boulder", "Physics"}, {"Physics Wheel", "Physics"},
            {"See Saw", "Physics"}, {"Trap Door", "Physics"}, {"Chain", "Physics"},
            {"Physics Rope", "Physics"}, {"Button", "Logic"}, {"Button Trap Door Link", "Logic"},
            {"Button Ladder Link", "Logic"}, {"Button Exit Link", "Logic"}, {"Button Fan Link", "Logic"},
            {"Button Platform Link", "Logic"}, {"Button Platform Loop", "Logic"},
            {"Platform Loop Button Link", "Logic"}, {"Button Spike Link", "Logic"},
            {"Portal Pair", "Gameplay"}, {"Spike Hazard", "Hazards"},
            {"Directional Spike Hazard", "Hazards"}, {"Arrow Trap", "Hazards"},
            {"Clock Face", "Set Dressing"}, {"Toxin Leak", "Fluids"}, {"Valve Fluid Fill", "Logic"}
        };
        for (int value = static_cast<int>(GuideObjectType::Ball);
            value <= static_cast<int>(GuideObjectType::ExplosiveBarrel); ++value) {
            const GuideObjectType type = static_cast<GuideObjectType>(value);
            entries.push_back({GetGuideObjectName(type), "Guide Objects"});
        }
        std::stable_sort(entries.begin(), entries.end(), [](const CatalogEntry& left, const CatalogEntry& right) {
            if (left.category != right.category) return left.category < right.category;
            return left.name < right.name;
        });
        return entries;
    }();
    return catalog;
}

#if defined(_WIN32)
enum NativeMenuCommand : unsigned int {
    MenuFileOpen = 1001,
    MenuFileSave,
    MenuFileSaveAs,
    MenuFileReload,
    MenuFileExit,
    MenuEditSelect,
    MenuEditDelete,
    MenuViewFrame,
    MenuViewResetLayout,
    MenuWindowAssetLibrary,
    MenuSnap16,
    MenuSnap32,
    MenuSnap64,
    MenuHelpAbout
};

WNDPROC OriginalWindowProcedure = nullptr;
HMENU EditorMenu = nullptr;
HMENU PreferencesMenu = nullptr;
unsigned int PendingMenuCommand = 0;
bool AllowWindowClose = false;
constexpr UINT ViewportHostCommandMessage = WM_APP + 120;
std::array<int, 37> PendingViewportValues{};
std::array<bool, 37> PendingViewportCommands{};
std::string PendingDocumentReload;

LRESULT CALLBACK EditorWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COPYDATA) {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (data && data->dwData == 1 && data->lpData && data->cbData > 1) {
            PendingDocumentReload.assign(static_cast<const char*>(data->lpData), data->cbData - 1);
            return TRUE;
        }
    }
    if (message == ViewportHostCommandMessage) {
        const int command = static_cast<int>(wParam);
        if (command > 0 && command < static_cast<int>(PendingViewportCommands.size())) {
            PendingViewportValues[static_cast<size_t>(command)] = static_cast<int>(lParam);
            PendingViewportCommands[static_cast<size_t>(command)] = true;
        }
        return 0;
    }
    if (message == WM_COMMAND) {
        PendingMenuCommand = LOWORD(wParam);
        return 0;
    }
    if (message == WM_CLOSE && !AllowWindowClose) {
        PendingMenuCommand = MenuFileExit;
        return 0;
    }
    return CallWindowProcW(OriginalWindowProcedure, window, message, wParam, lParam);
}

void AddMenuItem(HMENU menu, unsigned int command, const wchar_t* label) {
    AppendMenuW(menu, MF_STRING, command, label);
}
#endif

const char* ToolName(LevelEditor::EditTool tool) {
    switch (tool) {
    case LevelEditor::EditTool::Select: return "Select";
    case LevelEditor::EditTool::Solid: return "Solid";
    case LevelEditor::EditTool::Platform: return "Platform";
    case LevelEditor::EditTool::Ladder: return "Ladder";
    case LevelEditor::EditTool::CameraZone: return "Camera";
    case LevelEditor::EditTool::Darkness: return "Dark";
    case LevelEditor::EditTool::PlayerStart: return "Player";
    case LevelEditor::EditTool::Exit: return "Exit";
    case LevelEditor::EditTool::Water: return "Water";
    case LevelEditor::EditTool::Sand: return "Sand";
    case LevelEditor::EditTool::Gel: return "Gel";
    case LevelEditor::EditTool::Gas: return "Gas";
    case LevelEditor::EditTool::Pulley: return "Pulley";
    case LevelEditor::EditTool::HangingWeight: return "Hanging Weight";
    case LevelEditor::EditTool::RotaryLatch: return "Rotary Latch";
    case LevelEditor::EditTool::StoneBlock: return "Stone Block";
    case LevelEditor::EditTool::Boulder: return "Boulder";
    case LevelEditor::EditTool::PhysicsWheel: return "Physics Wheel";
    case LevelEditor::EditTool::Gear: return "Gear";
    case LevelEditor::EditTool::Flywheel: return "Flywheel";
    case LevelEditor::EditTool::SteeringWheel: return "Steering Wheel";
    case LevelEditor::EditTool::Screw: return "Screw";
    case LevelEditor::EditTool::Fan: return "Fan";
    case LevelEditor::EditTool::Pinwheel: return "Pinwheel";
    case LevelEditor::EditTool::Ramp: return "Ramp";
    case LevelEditor::EditTool::SeeSaw: return "See Saw";
    case LevelEditor::EditTool::TrapDoor: return "Trap Door";
    case LevelEditor::EditTool::Chain: return "Chain";
    case LevelEditor::EditTool::PhysicsRope: return "Physics Rope";
    case LevelEditor::EditTool::Button: return "Button";
    case LevelEditor::EditTool::Portal: return "Portal";
    case LevelEditor::EditTool::DirectionalSpikes: return "Directional Spikes";
    case LevelEditor::EditTool::ArrowTrap: return "Arrow Trap";
    case LevelEditor::EditTool::BreakableTile: return "Breakable Tile";
    case LevelEditor::EditTool::Enemy: return "Enemy";
    case LevelEditor::EditTool::Label: return "Label";
    case LevelEditor::EditTool::Valve: return "Valve";
    case LevelEditor::EditTool::Checkpoint: return "Checkpoint";
    case LevelEditor::EditTool::Collectible: return "Collectible";
    case LevelEditor::EditTool::Ball: return "Ball";
    case LevelEditor::EditTool::Barrel: return "Barrel";
    case LevelEditor::EditTool::MovingPlatform: return "Moving Platform";
    case LevelEditor::EditTool::Elevator: return "Elevator";
    case LevelEditor::EditTool::PendulumBob: return "Pendulum Bob";
    case LevelEditor::EditTool::OneWayPlatform: return "One-Way Platform";
    case LevelEditor::EditTool::CeilingHook: return "Ceiling Hook";
    case LevelEditor::EditTool::GuideRail: return "Guide Rail";
    case LevelEditor::EditTool::Spring: return "Spring";
    case LevelEditor::EditTool::CompressionSpring: return "Compression Spring";
    case LevelEditor::EditTool::ExtensionSpring: return "Extension Spring";
    case LevelEditor::EditTool::TorsionSpring: return "Torsion Spring";
    case LevelEditor::EditTool::GarterSpring: return "Garter Spring";
    case LevelEditor::EditTool::VoluteSpring: return "Volute Spring";
    case LevelEditor::EditTool::SpiralSpring: return "Spiral Spring";
    case LevelEditor::EditTool::ConstantForceSpring: return "Constant-Force Spring";
    case LevelEditor::EditTool::ConstantTorqueSpring: return "Constant-Torque Spring";
    case LevelEditor::EditTool::LeafSpring: return "Leaf Spring";
    case LevelEditor::EditTool::BeamSpring: return "Beam Spring";
    case LevelEditor::EditTool::DiscSpring: return "Disc Spring";
    case LevelEditor::EditTool::WaveSpring: return "Wave Spring";
    case LevelEditor::EditTool::WaveWasher: return "Wave Washer";
    case LevelEditor::EditTool::TorsionBar: return "Torsion Bar";
    case LevelEditor::EditTool::RingSpring: return "Ring Spring";
    case LevelEditor::EditTool::ElastomerSpring: return "Elastomer Spring";
    case LevelEditor::EditTool::PneumaticSpring: return "Pneumatic Spring";
    case LevelEditor::EditTool::GasSpring: return "Gas Spring";
    case LevelEditor::EditTool::HydropneumaticSpring: return "Hydropneumatic Spring";
    case LevelEditor::EditTool::MagneticSpring: return "Magnetic Spring";
    case LevelEditor::EditTool::CompositeSpring: return "Composite Spring";
    case LevelEditor::EditTool::Rod: return "Rod";
    case LevelEditor::EditTool::FixedJoint: return "Fixed Joint";
    case LevelEditor::EditTool::Crank: return "Crank";
    case LevelEditor::EditTool::Ratchet: return "Ratchet";
    case LevelEditor::EditTool::Clutch: return "Clutch";
    case LevelEditor::EditTool::Brake: return "Brake";
    }
    return "Select";
}

int AssetCategoryIndex(LevelEditor::EditTool tool) {
    switch (tool) {
    case LevelEditor::EditTool::Solid:
    case LevelEditor::EditTool::Platform:
    case LevelEditor::EditTool::Ladder: return 1;
    case LevelEditor::EditTool::CameraZone:
    case LevelEditor::EditTool::Darkness: return 2;
    case LevelEditor::EditTool::PlayerStart:
    case LevelEditor::EditTool::Exit: return 3;
    case LevelEditor::EditTool::Water:
    case LevelEditor::EditTool::Sand:
    case LevelEditor::EditTool::Gel:
    case LevelEditor::EditTool::Gas: return 4;
    case LevelEditor::EditTool::Pulley:
    case LevelEditor::EditTool::HangingWeight:
    case LevelEditor::EditTool::RotaryLatch:
    case LevelEditor::EditTool::StoneBlock:
    case LevelEditor::EditTool::Boulder:
    case LevelEditor::EditTool::PhysicsWheel:
    case LevelEditor::EditTool::Gear:
    case LevelEditor::EditTool::Flywheel:
    case LevelEditor::EditTool::SteeringWheel:
    case LevelEditor::EditTool::Screw:
    case LevelEditor::EditTool::Fan:
    case LevelEditor::EditTool::Pinwheel:
    case LevelEditor::EditTool::Ramp:
    case LevelEditor::EditTool::SeeSaw:
    case LevelEditor::EditTool::TrapDoor:
    case LevelEditor::EditTool::Chain:
    case LevelEditor::EditTool::PhysicsRope:
    case LevelEditor::EditTool::Button:
    case LevelEditor::EditTool::Portal:
    case LevelEditor::EditTool::DirectionalSpikes:
    case LevelEditor::EditTool::ArrowTrap:
    case LevelEditor::EditTool::BreakableTile:
    case LevelEditor::EditTool::Enemy:
    case LevelEditor::EditTool::Label:
    case LevelEditor::EditTool::Valve:
    case LevelEditor::EditTool::Checkpoint:
    case LevelEditor::EditTool::Collectible: return 1;
    case LevelEditor::EditTool::Ball:
    case LevelEditor::EditTool::Barrel:
    case LevelEditor::EditTool::MovingPlatform:
    case LevelEditor::EditTool::Elevator: return 1;
    case LevelEditor::EditTool::PendulumBob:
    case LevelEditor::EditTool::OneWayPlatform:
    case LevelEditor::EditTool::CeilingHook:
    case LevelEditor::EditTool::GuideRail: return 1;
    case LevelEditor::EditTool::Spring:
    case LevelEditor::EditTool::CompressionSpring:
    case LevelEditor::EditTool::ExtensionSpring:
    case LevelEditor::EditTool::TorsionSpring: return 1;
    case LevelEditor::EditTool::GarterSpring:
    case LevelEditor::EditTool::VoluteSpring:
    case LevelEditor::EditTool::SpiralSpring:
    case LevelEditor::EditTool::ConstantForceSpring: return 1;
    case LevelEditor::EditTool::ConstantTorqueSpring:
    case LevelEditor::EditTool::LeafSpring:
    case LevelEditor::EditTool::BeamSpring:
    case LevelEditor::EditTool::DiscSpring: return 1;
    case LevelEditor::EditTool::WaveSpring:
    case LevelEditor::EditTool::WaveWasher:
    case LevelEditor::EditTool::TorsionBar:
    case LevelEditor::EditTool::RingSpring: return 1;
    case LevelEditor::EditTool::ElastomerSpring:
    case LevelEditor::EditTool::PneumaticSpring:
    case LevelEditor::EditTool::GasSpring:
    case LevelEditor::EditTool::HydropneumaticSpring: return 1;
    case LevelEditor::EditTool::MagneticSpring:
    case LevelEditor::EditTool::CompositeSpring:
    case LevelEditor::EditTool::Rod:
    case LevelEditor::EditTool::FixedJoint: return 1;
    case LevelEditor::EditTool::Crank:
    case LevelEditor::EditTool::Ratchet:
    case LevelEditor::EditTool::Clutch:
    case LevelEditor::EditTool::Brake: return 1;
    case LevelEditor::EditTool::Select: return 0;
    }
    return 0;
}

std::vector<int> ContentAssetIndices(int category) {
    std::vector<int> indices;
    for (int index = 0; index < static_cast<int>(std::size(Assets)); ++index) {
        if (category == 0 || AssetCategoryIndex(Assets[index].tool) == category) indices.push_back(index);
    }
    return indices;
}

const char* ScriptName(LevelScript script) {
    switch (script) {
    case LevelScript::RotaryLatchLab: return "rotary_latch_lab";
    case LevelScript::FloodedFoundry: return "flooded_foundry";
    case LevelScript::CounterweightRow: return "counterweight_row";
    case LevelScript::ButtonSequence: return "button_sequence";
    case LevelScript::PortalLift: return "portal_lift";
    case LevelScript::WaterEscape: return "water_escape";
    case LevelScript::NeurotoxinMaze: return "neurotoxin_maze";
    case LevelScript::ClocktowerCore: return "clocktower_core";
    case LevelScript::TilesetReference: return "tileset_reference";
    case LevelScript::PowerPulleyPanic: return "power_pulley_panic";
    }
    return "power_pulley_panic";
}

const char* FluidName(FluidType type) {
    switch (type) {
    case FluidType::Water: return "water";
    case FluidType::Sand: return "sand";
    case FluidType::Gel: return "gel";
    case FluidType::Gas: return "gas";
    }
    return "water";
}

const char* GearVisualName(GearVisualType type) {
    switch (type) {
    case GearVisualType::LanternPinion: return "lantern";
    case GearVisualType::Ratchet: return "ratchet";
    case GearVisualType::Escape: return "escape";
    case GearVisualType::Bevel: return "bevel";
    case GearVisualType::Sector: return "sector";
    case GearVisualType::Count: return "count";
    case GearVisualType::Spur: return "spur";
    }
    return "spur";
}

const char* ClockHandName(ClockHandType hand) {
    switch (hand) {
    case ClockHandType::Hour: return "hour";
    case ClockHandType::Minute: return "minute";
    case ClockHandType::Second: return "second";
    case ClockHandType::None: return "none";
    }
    return "none";
}

std::vector<FluidField> FluidDefinitions(const std::vector<FluidField>& fluids) {
    std::vector<FluidField> definitions = fluids;
    for (FluidField& fluid : definitions) {
        fluid.particles.clear();
        fluid.gelBonds.clear();
        fluid.cells.clear();
        fluid.initialized = false;
        fluid.gridColumns = 0;
        fluid.gridRows = 0;
        fluid.accumulator = 0.0f;
        fluid.simulationStep = 0;
    }
    return definitions;
}

std::vector<Chain> ChainDefinitions(const std::vector<Chain>& chains) {
    std::vector<Chain> definitions = chains;
    for (Chain& chain : definitions) {
        chain.points.clear();
        chain.previousPoints.clear();
    }
    return definitions;
}

std::vector<PhysicsRope> RopeDefinitions(const std::vector<PhysicsRope>& ropes) {
    std::vector<PhysicsRope> definitions = ropes;
    for (PhysicsRope& rope : definitions) {
        rope.points.clear();
        rope.previousPoints.clear();
    }
    return definitions;
}

std::vector<ArrowTrap> ArrowTrapDefinitions(const std::vector<ArrowTrap>& traps) {
    std::vector<ArrowTrap> definitions = traps;
    for (ArrowTrap& trap : definitions) {
        trap.timer = 0.0f;
        trap.arrows.clear();
    }
    return definitions;
}

const char* SpikeDirectionName(SpikeDirection direction) {
    switch (direction) {
    case SpikeDirection::Up: return "up";
    case SpikeDirection::Down: return "down";
    case SpikeDirection::Left: return "left";
    case SpikeDirection::Right: return "right";
    }
    return "up";
}

void WriteRect(std::ostream& output, const char* command, Rectangle rect) {
    output << command << ' ' << rect.x << ' ' << rect.y << ' ' << rect.width << ' ' << rect.height << '\n';
}

bool IsUsableRectangle(Rectangle rect) {
    return rect.width > 0.0f && rect.height > 0.0f;
}

float MeasureUiText(Font font, const char* text, float fontSize) {
    return MeasureTextEx(font, text, fontSize, 1.0f).x;
}

void DrawUiText(Font font, const char* text, float x, float y, float fontSize, Color color) {
    DrawTextEx(font, text, {x, y}, fontSize, 1.0f, color);
}

void DrawToolbarButton(Rectangle bounds, const char* label, Font font, bool active = false, bool enabled = true) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    const Color fill = !enabled ? Color{34, 38, 46, 255} :
        (active ? Color{51, 91, 133, 255} : (hovered ? Color{59, 69, 84, 255} : Color{43, 49, 60, 255}));
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, active || hovered ? Accent : PanelBorder);
    const float textWidth = MeasureUiText(font, label, 18.0f);
    DrawUiText(font, label, bounds.x + (bounds.width - textWidth) * 0.5f,
        bounds.y + 7.0f, 18.0f, enabled ? PrimaryText : SecondaryText);
}

void DrawToolbarIconButton(Rectangle bounds, const char* label, Font font, Texture2D icons, int iconIndex,
    bool active = false) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    const Color fill = active ? Color{51, 91, 133, 255} :
        (hovered ? Color{59, 69, 84, 255} : Color{43, 49, 60, 255});
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, active || hovered ? Accent : PanelBorder);
    if (icons.id != 0 && iconIndex >= 0 && iconIndex < 5) {
        // The generated sheet has generous transparent margins, so use tight artwork bounds.
        constexpr Rectangle iconSources[] = {
            {82.0f, 218.0f, 300.0f, 330.0f},
            {444.0f, 216.0f, 340.0f, 340.0f},
            {824.0f, 218.0f, 360.0f, 330.0f},
            {1200.0f, 216.0f, 370.0f, 340.0f},
            {1584.0f, 216.0f, 374.0f, 340.0f}
        };
        const Rectangle destination{bounds.x + 8.0f, bounds.y + 6.0f, 26.0f, 26.0f};
        const Rectangle source = iconSources[iconIndex];
        DrawTexturePro(icons, source, destination, {}, 0.0f, WHITE);
    }
    DrawUiText(font, label, bounds.x + 41.0f, bounds.y + 10.0f, 15.0f, PrimaryText);
}

bool HasSolidTouchingTop(Rectangle rect, const std::vector<Rectangle>& solids) {
    constexpr float epsilon = 0.5f;
    std::vector<std::pair<float, float>> intervals;
    for (const Rectangle& other : solids) {
        if (fabsf((other.y + other.height) - rect.y) > epsilon) continue;
        const float start = fmaxf(rect.x, other.x);
        const float end = fminf(rect.x + rect.width, other.x + other.width);
        if (end > start + epsilon) intervals.emplace_back(start, end);
    }
    if (intervals.empty()) return false;
    std::sort(intervals.begin(), intervals.end());
    float covered = 0.0f;
    float start = intervals.front().first;
    float end = intervals.front().second;
    for (size_t index = 1; index < intervals.size(); ++index) {
        if (intervals[index].first <= end + epsilon) end = fmaxf(end, intervals[index].second);
        else {
            covered += end - start;
            start = intervals[index].first;
            end = intervals[index].second;
        }
    }
    return covered + end - start >= rect.width * 0.80f;
}

bool IsCeilingSolid(Rectangle rect) {
    return rect.y <= 0.5f && rect.width > rect.height * 2.0f;
}

bool IsWallSolid(Rectangle rect) {
    return rect.height > rect.width * 2.0f;
}
}

int LevelEditor::Run(const std::filesystem::path& initialLevel, bool useViewportOnly) {
    viewportOnly = useViewportOnly;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT |
        (viewportOnly ? FLAG_WINDOW_UNDECORATED : 0));
    InitWindow(InitialWindowWidth, InitialWindowHeight, "Power Pulley Panic - Level Editor");
    SetWindowMinSize(1100, 700);
    SetTargetFPS(120);
    if (!viewportOnly) InstallNativeMenu();
#if defined(_WIN32)
    else {
        HWND window = static_cast<HWND>(GetWindowHandle());
        OriginalWindowProcedure = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorWindowProcedure)));
    }
#endif

    uiFont = GetFontDefault();
    const std::filesystem::path fontCandidates[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    for (const std::filesystem::path& fontPath : fontCandidates) {
        std::error_code fontError;
        if (!std::filesystem::is_regular_file(fontPath, fontError)) {
            continue;
        }

        Font candidate = LoadFontEx(fontPath.string().c_str(), 32, nullptr, 0);
        if (candidate.texture.id != 0 && candidate.texture.id != GetFontDefault().texture.id && candidate.glyphCount > 0) {
            uiFont = candidate;
            ownsUiFont = true;
            SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }

    industrialTiles = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png");
    industrialBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png");
    industrialFarBackground = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/3_Far_Background_Tile.png");
    playerSprites = LoadTexture("assets/first_party/characters/Player_Sprites.png");
    enemySprites = LoadTexture("assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/6. Character Animations 32p/Anim_Robot_Walk1_v1.1_spritesheet.png");
    chainLinks = LoadTexture("assets/first_party/machines/chain_links.png");
    toolbarIcons = LoadTexture("assets/first_party/ui/editor_transform_icons.png");
    if (viewportOnly) GenerateContentThumbnails();

    camera.zoom = 1.0f;
    camera.rotation = 0.0f;
    levelDirectory = FindLevelDirectory();
    DiscoverLevels();

    std::filesystem::path levelToLoad = initialLevel;
    if (!levelToLoad.empty() && levelToLoad.is_relative() && !std::filesystem::exists(levelToLoad)) {
        levelToLoad = levelDirectory / levelToLoad;
    }

    if (!levelToLoad.empty()) {
        LoadLevel(levelToLoad);
    }
    else {
        level = Level{};
        currentLevelPath.clear();
        FrameLevel();
        statusText = "Blank level. Use File > Open to load a level, or File > New to create one.";
        SetWindowTitle("Power Pulley Panic - Level Editor - Untitled");
    }

    while (!WindowShouldClose()) {
        Update();
        if (viewportOnly && hostEditTransaction && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            !draggingTransform && !draggingCreate && !tileDragActive && !movingTileSelection) {
            NotifyFullTileState();
            NotifyEditableObjectState();
            hostEditTransaction = false;
            NotifyTileOperation("historyend");
        }
        Draw();
    }

    if (ownsUiFont) {
        UnloadFont(uiFont);
    }
    if (industrialTiles.id != 0) UnloadTexture(industrialTiles);
    if (industrialBackground.id != 0) UnloadTexture(industrialBackground);
    if (industrialFarBackground.id != 0) UnloadTexture(industrialFarBackground);
    if (playerSprites.id != 0) UnloadTexture(playerSprites);
    if (enemySprites.id != 0) UnloadTexture(enemySprites);
    if (chainLinks.id != 0) UnloadTexture(chainLinks);
    if (toolbarIcons.id != 0) UnloadTexture(toolbarIcons);
    RemoveNativeMenu();
    CloseWindow();
    return 0;
}

void LevelEditor::GenerateContentThumbnails() const {
    constexpr int CellWidth = 112;
    constexpr int CellHeight = 84;
    constexpr int Columns = 5;
    constexpr const char* names[] = {
        "Solid", "Platform", "Ladder", "Camera Zone", "Darkness Region", "Player Start", "Exit Trigger",
        "Water", "Sand", "Gel", "Gas", "Pulley", "Hanging Weight", "Rotary Latch", "Stone Block",
        "Boulder", "Physics Wheel", "Gear", "Flywheel", "Steering Wheel", "Screw", "Fan", "Pinwheel",
        "Ramp", "See Saw", "Trap Door", "Chain", "Physics Rope", "Button", "Portal", "Directional Spikes",
        "Arrow Trap", "Breakable Tile", "Enemy", "Visual Tile", "Label", "Valve", "Checkpoint", "Collectible",
        "Ball", "Barrel", "Moving Platform", "Elevator", "Pendulum Bob", "One-Way Platform",
        "Ceiling Hook", "Guide Rail", "Spring", "Compression Spring", "Extension Spring", "Torsion Spring",
        "Garter Spring", "Volute Spring", "Spiral Spring", "Constant-Force Spring", "Constant-Torque Spring",
        "Leaf Spring", "Beam Spring", "Disc Spring", "Wave Spring", "Wave Washer", "Torsion Bar", "Ring Spring",
        "Elastomer Spring", "Pneumatic Spring", "Gas Spring", "Hydropneumatic Spring",
        "Magnetic Spring", "Composite Spring", "Rod", "Fixed Joint", "Crank", "Ratchet", "Clutch", "Brake"
    };
    constexpr int Count = static_cast<int>(std::size(names));
    constexpr int Rows = (Count + Columns - 1) / Columns;
    RenderTexture2D atlas = LoadRenderTexture(CellWidth * Columns, CellHeight * Rows);
    if (atlas.id == 0) return;

    BeginTextureMode(atlas);
    ClearBackground(Color{23, 26, 32, 255});
    for (int index = 0; index < Count; ++index) {
        const float left = static_cast<float>((index % Columns) * CellWidth);
        const float top = static_cast<float>((index / Columns) * CellHeight);
        const Vector2 center{left + CellWidth * 0.5f, top + CellHeight * 0.5f};
        BeginScissorMode(static_cast<int>(left), static_cast<int>(top), CellWidth, CellHeight);
        const std::string name = names[index];
        if (name == "Solid" || name == "Platform") {
            DrawTilesetSolid(industrialTiles, {left + 12.0f, top + 31.0f, 88.0f, name == "Solid" ? 38.0f : 18.0f}, WHITE);
        }
        else if (name == "Ladder") {
            DrawLineEx({center.x - 18.0f, top + 12.0f}, {center.x - 18.0f, top + 72.0f}, 5.0f, Color{115, 82, 48, 255});
            DrawLineEx({center.x + 18.0f, top + 12.0f}, {center.x + 18.0f, top + 72.0f}, 5.0f, Color{115, 82, 48, 255});
            for (float y = top + 16.0f; y < top + 72.0f; y += 13.0f) DrawLineEx({center.x - 18.0f, y}, {center.x + 18.0f, y}, 4.0f, Color{156, 111, 59, 255});
        }
        else if (name == "Camera Zone") DrawRectangleLinesEx({left + 12.0f, top + 14.0f, 88.0f, 56.0f}, 3.0f, Color{86, 190, 230, 255});
        else if (name == "Darkness Region") DrawRectangleRec({left + 12.0f, top + 14.0f, 88.0f, 56.0f}, Fade(BLACK, 0.82f));
        else if (name == "Player Start") { Player player{}; player.rect = {center.x - 16.0f, center.y - 22.0f, 32.0f, 44.0f}; DrawPlayer(player, playerSprites, 0); }
        else if (name == "Exit Trigger") DrawExitDoor({center.x - 24.0f, top + 12.0f, 48.0f, 60.0f}, top + 72.0f);
        else if (name == "Water" || name == "Sand" || name == "Gel" || name == "Gas") {
            FluidField fluid{}; fluid.bounds = {left + 10.0f, top + 18.0f, 92.0f, 54.0f}; fluid.initialFill = 0.72f;
            fluid.type = name == "Sand" ? FluidType::Sand : name == "Gel" ? FluidType::Gel : name == "Gas" ? FluidType::Gas : FluidType::Water;
            InitializeFluidField(fluid, {}, FluidSimulationMode::Tile);
            DrawFluidBackground(fluid);
            DrawFluidField(fluid);
        }
        else if (name == "Pulley") DrawPulley(center, 28.0f, 0.0f, BLACK);
        else if (name == "Hanging Weight") { HangingWeight value{}; value.pulley = {center.x, top + 20.0f}; value.pulleyRadius = 14.0f; value.rect = {center.x - 19.0f, top + 43.0f, 38.0f, 30.0f}; DrawHazardWeight(value); }
        else if (name == "Rotary Latch") { RotaryLatch value{}; value.center = center; value.radius = 28.0f; DrawRotaryLatch(value, false, ""); }
        else if (name == "Stone Block") { StoneBlock value{}; value.rect = {center.x - 27.0f, center.y - 24.0f, 54.0f, 48.0f}; DrawStoneBlock(value); }
        else if (name == "Boulder") { Boulder value{}; value.center = center; value.radius = 28.0f; DrawBoulder(value); }
        else if (name == "Physics Wheel") { PhysicsWheel value{}; value.center = center; value.radius = 28.0f; DrawPhysicsWheel(value); }
        else if (name == "Gear") { Gear value{}; value.center = center; value.radius = 28.0f; DrawGear(value); }
        else if (name == "Flywheel") { Flywheel value{}; value.center = center; value.radius = 31.0f; DrawFlywheel(value); }
        else if (name == "Steering Wheel") { SteeringWheel value{}; value.center = center; value.radius = 29.0f; DrawSteeringWheel(value); }
        else if (name == "Screw") { Screw value{}; value.center = center; value.length = 82.0f; value.radius = 10.0f; DrawScrew(value); }
        else if (name == "Fan") { Fan value{}; value.center = {left + 25.0f, center.y}; value.length = 76.0f; value.width = 48.0f; DrawFan(value); }
        else if (name == "Pinwheel") { Pinwheel value{}; value.center = center; value.radius = 29.0f; DrawPinwheel(value); }
        else if (name == "Ramp") { Ramp value{}; value.center = center; value.length = 92.0f; value.thickness = 14.0f; value.angle = -18.0f; DrawRamp(value); }
        else if (name == "See Saw") { SeeSaw value{}; value.pivot = center; value.length = 92.0f; value.thickness = 12.0f; value.angle = -8.0f; DrawSeeSaw(value); }
        else if (name == "Trap Door") { TrapDoor value{}; value.hinge = {left + 18.0f, center.y}; value.length = 78.0f; value.thickness = 13.0f; DrawTrapDoor(value); }
        else if (name == "Chain") { Chain value{}; value.start = {center.x, top + 8.0f}; value.end = {center.x, top + 76.0f}; value.spacing = 11.0f; DrawChain(value, chainLinks); }
        else if (name == "Physics Rope") { PhysicsRope value{}; value.start = {left + 17.0f, top + 18.0f}; value.end = {left + 95.0f, top + 67.0f}; value.length = 92.0f; DrawPhysicsRope(value); }
        else if (name == "Button") { Button value{}; value.rect = {center.x - 30.0f, center.y - 8.0f, 60.0f, 16.0f}; DrawButton(value); }
        else if (name == "Portal") DrawPortal({center.x - 21.0f, top + 10.0f, 42.0f, 64.0f}, Color{63, 143, 255, 255});
        else if (name == "Directional Spikes") { DirectionalSpikeHazard value{}; value.rect = {left + 12.0f, center.y - 12.0f, 88.0f, 24.0f}; DrawDirectionalSpikes(value); }
        else if (name == "Arrow Trap") { ArrowTrap value{}; value.position = center; value.direction = {1.0f, 0.0f}; DrawArrowTrap(value); }
        else if (name == "Breakable Tile") { BreakableTile value{}; value.rect = {left + 19.0f, center.y - 16.0f, 74.0f, 32.0f}; DrawBreakableTile(industrialTiles, value); }
        else if (name == "Enemy") { Enemy value{}; value.rect = {center.x - 18.0f, center.y - 22.0f, 36.0f, 44.0f}; DrawEnemy(value, enemySprites); }
        else if (name == "Visual Tile") DrawTilesetTile(industrialTiles, 1, 0, {center.x - 16.0f, center.y - 16.0f}, WHITE);
        else if (name == "Label") { DrawRectangleRounded({left + 10.0f, center.y - 16.0f, 92.0f, 32.0f}, 0.2f, 4, Color{22, 28, 34, 255}); DrawText("LABEL", static_cast<int>(left + 28.0f), static_cast<int>(center.y - 8.0f), 16, RAYWHITE); }
        else if (name == "Valve") { Valve value{}; value.center = center; value.radius = 29.0f; DrawValveBody(value, false); }
        else if (name == "Checkpoint" || name == "Collectible") { GuideObject value{}; std::istringstream stream(name == "Checkpoint" ? "56 14 30 56" : "56 42 12"); ParseGuideObject(name == "Checkpoint" ? "checkpoint" : "collectible", stream, value); value.transform.position.x += left; value.transform.position.y += top; DrawGuideObject(value); }
        else if (name == "Ball" || name == "Barrel" || name == "Moving Platform" || name == "Elevator") {
            GuideObject value{};
            const char* command = name == "Ball" ? "ball" : name == "Barrel" ? "barrel" :
                name == "Moving Platform" ? "movingPlatform" : "elevator";
            const char* arguments = name == "Ball" ? "56 42 25 1" : name == "Barrel" ? "56 42 42 56 2" :
                name == "Moving Platform" ? "56 42 88 22 1 0 120 1" : "56 42 70 24 0 -1 100 1";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.transform.position.x += left;
            value.transform.position.y += top;
            value.origin = value.transform.position;
            DrawGuideObject(value);
        }
        else if (name == "Pendulum Bob" || name == "One-Way Platform" || name == "Ceiling Hook" || name == "Guide Rail") {
            GuideObject value{};
            const char* command = name == "Pendulum Bob" ? "pendulumBob" :
                name == "One-Way Platform" ? "oneWayPlatform" : name == "Ceiling Hook" ? "ceilingHook" : "guideRail";
            const char* arguments = name == "Pendulum Bob" ? "56 12 52 13 0 1" :
                name == "Ceiling Hook" ? "56 42 15" : name == "One-Way Platform" ? "56 42 88 18" : "56 42 88 14";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.origin.x += left;
            value.origin.y += top;
            value.transform.position = value.origin;
            if (name == "Pendulum Bob") value.transform.position.y += value.length;
            DrawGuideObject(value);
        }
        else if (name == "Spring" || name == "Compression Spring" || name == "Extension Spring" || name == "Torsion Spring") {
            GuideObject value{};
            const char* command = name == "Spring" ? "spring" : name == "Compression Spring" ? "compressionSpring" :
                name == "Extension Spring" ? "extensionSpring" : "torsionSpring";
            std::istringstream stream(name == "Torsion Spring" ? "12 42 100 42 7 10 1.8" : "12 42 100 42 7 8 1");
            ParseGuideObject(command, stream, value);
            value.constraint.anchorA.x += left;
            value.constraint.anchorA.y += top;
            value.constraint.anchorB.x += left;
            value.constraint.anchorB.y += top;
            DrawGuideObject(value);
        }
        else if (name == "Garter Spring" || name == "Volute Spring" || name == "Spiral Spring" || name == "Constant-Force Spring") {
            GuideObject value{};
            const char* command = name == "Garter Spring" ? "garterSpring" : name == "Volute Spring" ? "voluteSpring" :
                name == "Spiral Spring" ? "spiralSpring" : "constantForceSpring";
            const char* arguments = name == "Garter Spring" ? "30 42 82 42 7 9 1" :
                name == "Volute Spring" ? "12 42 100 42 7 14 1" :
                name == "Spiral Spring" ? "12 42 100 42 7 9 1.4" : "12 42 100 42 7 8 1";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.constraint.anchorA.x += left;
            value.constraint.anchorA.y += top;
            value.constraint.anchorB.x += left;
            value.constraint.anchorB.y += top;
            DrawGuideObject(value);
        }
        else if (name == "Constant-Torque Spring" || name == "Leaf Spring" || name == "Beam Spring" || name == "Disc Spring") {
            GuideObject value{};
            const char* command = name == "Constant-Torque Spring" ? "constantTorqueSpring" :
                name == "Leaf Spring" ? "leafSpring" : name == "Beam Spring" ? "beamSpring" : "discSpring";
            const char* arguments = name == "Constant-Torque Spring" ? "12 42 100 42 7 8 1.2" :
                name == "Leaf Spring" ? "12 42 100 42 7 12 2" :
                name == "Beam Spring" ? "12 42 100 42 7 10 1.7" : "12 42 100 42 7 18 1";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.constraint.anchorA.x += left;
            value.constraint.anchorA.y += top;
            value.constraint.anchorB.x += left;
            value.constraint.anchorB.y += top;
            DrawGuideObject(value);
        }
        else if (name == "Wave Spring" || name == "Wave Washer" || name == "Torsion Bar" || name == "Ring Spring") {
            GuideObject value{};
            const char* command = name == "Wave Spring" ? "waveSpring" : name == "Wave Washer" ? "waveWasher" :
                name == "Torsion Bar" ? "torsionBar" : "ringSpring";
            const char* arguments = name == "Wave Spring" ? "12 42 100 42 7 10 1" :
                name == "Wave Washer" ? "12 42 100 42 7 7 1" :
                name == "Torsion Bar" ? "12 42 100 42 7 15 2.2" : "12 42 100 42 7 13 4.2";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.constraint.anchorA.x += left;
            value.constraint.anchorA.y += top;
            value.constraint.anchorB.x += left;
            value.constraint.anchorB.y += top;
            DrawGuideObject(value);
        }
        else if (name == "Elastomer Spring" || name == "Pneumatic Spring" || name == "Gas Spring" || name == "Hydropneumatic Spring") {
            GuideObject value{};
            const char* command = name == "Elastomer Spring" ? "elastomerSpring" :
                name == "Pneumatic Spring" ? "pneumaticSpring" : name == "Gas Spring" ? "gasSpring" : "hydropneumaticSpring";
            const char* arguments = name == "Elastomer Spring" ? "12 42 100 42 7 9 4.8" :
                name == "Pneumatic Spring" ? "12 42 100 42 7 11 2.4" :
                name == "Gas Spring" ? "12 42 100 42 7 10 3.4" : "12 42 100 42 7 14 5.2";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.constraint.anchorA.x += left;
            value.constraint.anchorA.y += top;
            value.constraint.anchorB.x += left;
            value.constraint.anchorB.y += top;
            DrawGuideObject(value);
        }
        else if (name == "Magnetic Spring" || name == "Composite Spring" || name == "Rod" || name == "Fixed Joint") {
            GuideObject value{};
            const char* command = name == "Magnetic Spring" ? "magneticSpring" :
                name == "Composite Spring" ? "compositeSpring" : name == "Rod" ? "rod" : "fixedJoint";
            const char* arguments = name == "Magnetic Spring" ? "12 42 100 42 7 12 0.4" :
                name == "Composite Spring" ? "12 42 100 42 7 10 1.6" :
                name == "Rod" ? "12 42 100 42 6" : "56 42 10";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            if (name == "Fixed Joint") {
                value.transform.position.x += left;
                value.transform.position.y += top;
                value.origin = value.transform.position;
            }
            else {
                value.constraint.anchorA.x += left;
                value.constraint.anchorA.y += top;
                value.constraint.anchorB.x += left;
                value.constraint.anchorB.y += top;
            }
            DrawGuideObject(value);
        }
        else if (name == "Crank" || name == "Ratchet" || name == "Clutch" || name == "Brake") {
            GuideObject value{};
            const char* command = name == "Crank" ? "crank" : name == "Ratchet" ? "ratchet" :
                name == "Clutch" ? "clutch" : "brake";
            const char* arguments = name == "Crank" ? "56 42 24 105" : name == "Ratchet" ? "56 42 24 90" :
                name == "Clutch" ? "56 42 24 100 1" : "56 42 66 42 0.72";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, value);
            value.transform.position.x += left;
            value.transform.position.y += top;
            value.origin = value.transform.position;
            DrawGuideObject(value);
        }
        EndScissorMode();
    }
    EndTextureMode();

    std::filesystem::create_directories("editor_thumbnails");
    Image image = LoadImageFromTexture(atlas.texture);
    ImageFlipVertical(&image);
    ExportImage(image, "editor_thumbnails/content_atlas.png");
    UnloadImage(image);
    UnloadRenderTexture(atlas);
}

std::filesystem::path LevelEditor::FindLevelDirectory() const {
    std::error_code error;
    std::filesystem::path candidate = std::filesystem::current_path(error);
    if (error) {
        return "game_data/levels";
    }

    for (int depth = 0; depth < 5; depth++) {
        const std::filesystem::path levelPath = candidate / "game_data" / "levels";
        if (std::filesystem::is_directory(levelPath, error)) {
            return levelPath;
        }
        candidate = candidate.parent_path();
    }

    return std::filesystem::current_path() / "game_data" / "levels";
}

void LevelEditor::DiscoverLevels() {
    levelFiles.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(levelDirectory, error)) {
        return;
    }

    for (std::filesystem::directory_iterator iterator(levelDirectory, error), end;
        iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && iterator->path().extension() == ".level") {
            levelFiles.push_back(iterator->path());
        }
    }

    std::sort(levelFiles.begin(), levelFiles.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });
}

bool LevelEditor::LoadLevel(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || path.extension() != ".level") {
        statusText = "Could not load: choose an existing .level file.";
        return false;
    }

    level = LoadLevelFromFile(path.string(), Level{});
    currentLevelPath = std::filesystem::weakly_canonical(path, error);
    if (error) {
        currentLevelPath = path;
    }

    levelDirectory = currentLevelPath.parent_path();
    DiscoverLevels();
    selection = {};
    groupSelection.clear();
    tileSelectionActive = false;
    tileSelectionCells.clear();
    tileUndoHistory.clear();
    tileRedoHistory.clear();
    draggingCreate = false;
    dirty = false;
    FrameLevel();
    statusText = "Loaded " + currentLevelPath.filename().string() + ". Pick a tool, drag on the canvas, Ctrl+S to save.";
    const std::string title = "Power Pulley Panic - Level Editor - " + currentLevelPath.filename().string();
    SetWindowTitle(title.c_str());
    return true;
}

bool LevelEditor::CanSaveCurrentLevel() const {
    return !currentLevelPath.empty() && level.guideObjects.empty();
}

bool LevelEditor::SaveLevel() {
    if (!CanSaveCurrentLevel()) {
        statusText = level.guideObjects.empty()
            ? "Choose a level file before saving."
            : "Save is disabled for levels with guide objects until full round-trip support lands.";
        return false;
    }

    std::ofstream output(currentLevelPath);
    if (!output) {
        statusText = "Could not write " + currentLevelPath.filename().string() + ".";
        return false;
    }

    output << std::fixed << std::setprecision(2);
    output << "# Saved by Power Pulley Panic Level Editor\n";
    output << "script " << ScriptName(level.script) << "\n";
    WriteRect(output, "bounds", level.worldBounds);
    output << "playerStart " << level.playerStart.x << ' ' << level.playerStart.y << "\n";
    WriteRect(output, "exit", level.exitTrigger);
    output << "\n";

    for (Rectangle rect : level.cameraZones) WriteRect(output, "cameraZone", rect);
    for (Rectangle rect : level.darknessAreas) WriteRect(output, "darkness", rect);
    for (Rectangle rect : level.baseSolids) WriteRect(output, "solid", rect);
    for (Rectangle rect : level.pitPlatforms) WriteRect(output, "platform", rect);
    for (Rectangle rect : level.ladders) WriteRect(output, "ladder", rect);
    for (const FluidField& fluid : level.fluids) {
        output << FluidName(fluid.type) << ' ' << fluid.bounds.x << ' ' << fluid.bounds.y << ' ' <<
            fluid.bounds.width << ' ' << fluid.bounds.height << ' ' << fluid.particleSpacing << ' ' <<
            fluid.initialFill << ' ' << fluid.flowSpeed << "\n";
    }
    for (const LevelLabel& label : level.labels) {
        output << "labelSized " << label.position.x << ' ' << label.position.y << ' ' <<
            label.fontSize << ' ' << label.text << "\n";
    }

    dirty = false;
    statusText = "Saved " + currentLevelPath.filename().string() + ".";
    return true;
}

bool LevelEditor::OpenLevelDialog() {
#if defined(_WIN32)
    if (!ConfirmDiscardChanges()) return false;

    wchar_t pathBuffer[32768]{};
    const std::wstring initialDirectory = levelDirectory.wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(GetWindowHandle());
    dialog.lpstrFilter = L"Power Pulley Panic levels (*.level)\0*.level\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = pathBuffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(pathBuffer));
    dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&dialog) != FALSE && LoadLevel(std::filesystem::path(pathBuffer));
#else
    statusText = "Open dialogs are currently available on Windows.";
    return false;
#endif
}

bool LevelEditor::SaveLevelAs() {
#if defined(_WIN32)
    if (!level.guideObjects.empty()) {
        statusText = "Save is disabled for levels with guide objects until full round-trip support lands.";
        return false;
    }

    wchar_t pathBuffer[32768]{};
    if (!currentLevelPath.empty()) {
        const std::wstring currentPath = currentLevelPath.wstring();
        currentPath.copy(pathBuffer, std::size(pathBuffer) - 1);
    }
    const std::wstring initialDirectory = levelDirectory.wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(GetWindowHandle());
    dialog.lpstrFilter = L"Power Pulley Panic levels (*.level)\0*.level\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = pathBuffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(pathBuffer));
    dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    dialog.lpstrDefExt = L"level";
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&dialog) == FALSE) {
        return false;
    }

    const std::filesystem::path previousPath = currentLevelPath;
    const std::filesystem::path previousDirectory = levelDirectory;
    currentLevelPath = std::filesystem::path(pathBuffer);
    levelDirectory = currentLevelPath.parent_path();
    if (!SaveLevel()) {
        currentLevelPath = previousPath;
        levelDirectory = previousDirectory;
        return false;
    }
    DiscoverLevels();
    const std::string title = "Power Pulley Panic - Level Editor - " + currentLevelPath.filename().string();
    SetWindowTitle(title.c_str());
    return true;
#else
    statusText = "Save As dialogs are currently available on Windows.";
    return false;
#endif
}

bool LevelEditor::ConfirmDiscardChanges() {
    if (!dirty) return true;
#if defined(_WIN32)
    const int result = MessageBoxW(static_cast<HWND>(GetWindowHandle()),
        L"Save changes to the current level?", L"Unsaved Changes",
        MB_YESNOCANCEL | MB_ICONWARNING);
    if (result == IDCANCEL) return false;
    if (result == IDYES) return SaveLevel();
#endif
    return true;
}

void LevelEditor::InstallNativeMenu() {
#if defined(_WIN32)
    HWND window = static_cast<HWND>(GetWindowHandle());
    if (window == nullptr) return;

    EditorMenu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AddMenuItem(fileMenu, MenuFileOpen, L"&Open...\tCtrl+O");
    AddMenuItem(fileMenu, MenuFileSave, L"&Save\tCtrl+S");
    AddMenuItem(fileMenu, MenuFileSaveAs, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(fileMenu, MenuFileReload, L"&Reload\tCtrl+R");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(fileMenu, MenuFileExit, L"E&xit");

    HMENU editMenu = CreatePopupMenu();
    AddMenuItem(editMenu, MenuEditSelect, L"&Select Tool\t1");
    AddMenuItem(editMenu, MenuEditDelete, L"&Delete Selection\tDel");

    HMENU viewMenu = CreatePopupMenu();
    AddMenuItem(viewMenu, MenuViewFrame, L"&Frame Level\tF");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(viewMenu, MenuViewResetLayout, L"&Reset Layout");

    PreferencesMenu = CreatePopupMenu();
    AddMenuItem(PreferencesMenu, MenuSnap16, L"Snap: 16 px");
    AddMenuItem(PreferencesMenu, MenuSnap32, L"Snap: 32 px");
    AddMenuItem(PreferencesMenu, MenuSnap64, L"Snap: 64 px");
    CheckMenuRadioItem(PreferencesMenu, MenuSnap16, MenuSnap64, MenuSnap32, MF_BYCOMMAND);

    HMENU helpMenu = CreatePopupMenu();
    AddMenuItem(helpMenu, MenuHelpAbout, L"&About Level Editor");
    HMENU windowMenu = CreatePopupMenu();
    AddMenuItem(windowMenu, MenuWindowAssetLibrary, L"&Asset Library");

    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(editMenu), L"&Edit");
    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");
    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(windowMenu), L"&Window");
    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(PreferencesMenu), L"&Preferences");
    AppendMenuW(EditorMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");
    SetMenu(window, EditorMenu);
    DrawMenuBar(window);
    OriginalWindowProcedure = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorWindowProcedure)));
#endif
}

void LevelEditor::RemoveNativeMenu() {
#if defined(_WIN32)
    HWND window = static_cast<HWND>(GetWindowHandle());
    if (window != nullptr && OriginalWindowProcedure != nullptr) {
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalWindowProcedure));
    }
    if (window != nullptr) SetMenu(window, nullptr);
    if (EditorMenu != nullptr) DestroyMenu(EditorMenu);
    EditorMenu = nullptr;
    PreferencesMenu = nullptr;
    OriginalWindowProcedure = nullptr;
    AllowWindowClose = false;
#endif
}

void LevelEditor::ProcessNativeMenuCommand() {
#if defined(_WIN32)
    const unsigned int command = std::exchange(PendingMenuCommand, 0u);
    if (command == 0) return;

    switch (command) {
    case MenuFileOpen: OpenLevelDialog(); break;
    case MenuFileSave: SaveLevel(); break;
    case MenuFileSaveAs: SaveLevelAs(); break;
    case MenuFileReload:
        if (!currentLevelPath.empty() && ConfirmDiscardChanges()) LoadLevel(currentLevelPath);
        break;
    case MenuFileExit:
        if (ConfirmDiscardChanges()) {
            AllowWindowClose = true;
            PostMessageW(static_cast<HWND>(GetWindowHandle()), WM_CLOSE, 0, 0);
        }
        break;
    case MenuEditSelect:
        activeTool = EditTool::Select;
        transformMode = TransformMode::Select;
        draggingCreate = false;
        statusText = "Tool: Select";
        break;
    case MenuEditDelete: DeleteSelection(); break;
    case MenuViewFrame: FrameLevel(); break;
    case MenuViewResetLayout:
        dockPanels = {
            DockPanel{DockSlot::Left, {40.0f, 110.0f, 250.0f, 470.0f}},
            DockPanel{DockSlot::Bottom, {180.0f, 520.0f, 720.0f, 220.0f}},
            DockPanel{DockSlot::RightTop, {980.0f, 110.0f, 310.0f, 330.0f}},
            DockPanel{DockSlot::RightBottom, {980.0f, 450.0f, 310.0f, 300.0f}}
        };
        leftDockWidth = 250.0f;
        rightDockWidth = 310.0f;
        bottomDockHeight = 220.0f;
        rightDockSplit = 0.52f;
        FrameLevel();
        statusText = "Editor layout reset.";
        break;
    case MenuWindowAssetLibrary: {
        DockPanel& library = dockPanels[static_cast<size_t>(PanelId::PlaceAssets)];
        library.slot = DockSlot::Floating;
        library.floatingBounds = {
            std::max(20.0f, (GetScreenWidth() - 340.0f) * 0.5f),
            ToolbarHeight + 28.0f,
            340.0f,
            std::max(420.0f, static_cast<float>(GetScreenHeight()) - ToolbarHeight - StatusBarHeight - 56.0f)
        };
        statusText = "Asset Library opened.";
        break;
    }
    case MenuSnap16:
    case MenuSnap32:
    case MenuSnap64: {
        gridSnap = command == MenuSnap16 ? 16.0f : (command == MenuSnap64 ? 64.0f : 32.0f);
        CheckMenuRadioItem(PreferencesMenu, MenuSnap16, MenuSnap64, command, MF_BYCOMMAND);
        statusText = "Grid snap: " + std::to_string(static_cast<int>(gridSnap)) + " px.";
        break;
    }
    case MenuHelpAbout:
        MessageBoxW(static_cast<HWND>(GetWindowHandle()),
            L"Power Pulley Panic Level Editor\n\nBuild and arrange levels using the asset library and canvas tools.",
            L"About Level Editor", MB_OK | MB_ICONINFORMATION);
        break;
    default: break;
    }
#endif
}

Rectangle LevelEditor::GetCanvasBounds() const {
    if (viewportOnly) {
        return {0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
    }
    const bool hasLeft = PanelInSlot(DockSlot::Left) != PanelId::Count;
    const bool hasBottom = PanelInSlot(DockSlot::Bottom) != PanelId::Count;
    const bool hasRight = PanelInSlot(DockSlot::RightTop) != PanelId::Count ||
        PanelInSlot(DockSlot::RightBottom) != PanelId::Count;
    const float left = hasLeft ? leftDockWidth : 0.0f;
    const float right = hasRight ? rightDockWidth : 0.0f;
    const float bottom = hasBottom ? bottomDockHeight : 0.0f;
    return {
        left,
        ToolbarHeight,
        std::max(1.0f, static_cast<float>(GetScreenWidth()) - left - right),
        std::max(1.0f, static_cast<float>(GetScreenHeight()) - ToolbarHeight - StatusBarHeight - bottom)
    };
}

Rectangle LevelEditor::GetContentBrowserBounds() const {
    return GetPanelBounds(PanelId::ContentBrowser);
}

LevelEditor::PanelId LevelEditor::PanelInSlot(DockSlot slot) const {
    for (size_t index = 0; index < dockPanels.size(); ++index) {
        if (dockPanels[index].slot == slot) return static_cast<PanelId>(index);
    }
    return PanelId::Count;
}

Rectangle LevelEditor::GetPanelBounds(PanelId panel) const {
    if (panel == PanelId::Count) return {};
    const DockPanel& state = dockPanels[static_cast<size_t>(panel)];
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float bottom = static_cast<float>(GetScreenHeight()) - StatusBarHeight;
    const bool hasBottom = PanelInSlot(DockSlot::Bottom) != PanelId::Count;
    const float dockBottom = hasBottom ? bottom - bottomDockHeight : bottom;
    switch (state.slot) {
    case DockSlot::Left:
        return {0.0f, ToolbarHeight, leftDockWidth, dockBottom - ToolbarHeight};
    case DockSlot::Bottom: {
        const bool hasRight = PanelInSlot(DockSlot::RightTop) != PanelId::Count ||
            PanelInSlot(DockSlot::RightBottom) != PanelId::Count;
        return {0.0f, bottom - bottomDockHeight, screenWidth - (hasRight ? rightDockWidth : 0.0f), bottomDockHeight};
    }
    case DockSlot::RightTop: {
        const float height = (bottom - ToolbarHeight) * rightDockSplit;
        return {screenWidth - rightDockWidth, ToolbarHeight, rightDockWidth, height};
    }
    case DockSlot::RightBottom: {
        const float y = ToolbarHeight + (bottom - ToolbarHeight) * rightDockSplit;
        return {screenWidth - rightDockWidth, y, rightDockWidth, bottom - y};
    }
    case DockSlot::Floating:
        return state.floatingBounds;
    }
    return {};
}

Rectangle LevelEditor::GetPanelHeaderBounds(PanelId panel) const {
    Rectangle bounds = GetPanelBounds(panel);
    bounds.height = std::min(38.0f, bounds.height);
    return bounds;
}

bool LevelEditor::IsMouseOverPanel(Vector2 mouse) const {
    for (size_t index = 0; index < dockPanels.size(); ++index) {
        if (CheckCollisionPointRec(mouse, GetPanelBounds(static_cast<PanelId>(index)))) return true;
    }
    return false;
}

void LevelEditor::DockPanelAt(PanelId panel, DockSlot slot) {
    if (panel == PanelId::Count || slot == DockSlot::Floating) return;
    DockPanel& moving = dockPanels[static_cast<size_t>(panel)];
    const DockSlot previous = dragOrigin;
    const PanelId occupant = PanelInSlot(slot);
    if (occupant != PanelId::Count && occupant != panel) {
        dockPanels[static_cast<size_t>(occupant)].slot = previous == DockSlot::Floating ? DockSlot::Floating : previous;
    }
    moving.slot = slot;
}

Rectangle LevelEditor::GetFitButtonBounds() const {
    const Rectangle scale = GetTransformButtonBounds(3);
    return {scale.x + scale.width + 6.0f, 12.0f, 98.0f, 38.0f};
}

Rectangle LevelEditor::GetSelectButtonBounds() const {
    return GetTransformButtonBounds(0);
}

Rectangle LevelEditor::GetTransformButtonBounds(int index) const {
    constexpr float widths[] = {94.0f, 90.0f, 100.0f, 92.0f};
    float x = GetCanvasBounds().x + 14.0f;
    for (int item = 0; item < index; ++item) x += widths[item] + 6.0f;
    return {x, 12.0f, widths[index], 38.0f};
}

Rectangle LevelEditor::GetAssetRowBounds(int index) const {
    const Rectangle panel = GetPanelBounds(PanelId::PlaceAssets);
    return {panel.x + 8.0f, panel.y + 70.0f + index * 30.0f, panel.width - 16.0f, 28.0f};
}

Rectangle LevelEditor::GetContentAssetBounds(int index) const {
    const Rectangle browser = GetContentBrowserBounds();
    constexpr float tileWidth = 80.0f;
    constexpr float tileGap = 6.0f;
    return {
        browser.x + ContentFolderWidth + 10.0f + index * (tileWidth + tileGap) - contentBrowserScroll,
        browser.y + 76.0f,
        tileWidth,
        112.0f
    };
}

void LevelEditor::FrameLevel() {
    const Rectangle canvas = GetCanvasBounds();
    const Rectangle bounds = IsUsableRectangle(level.worldBounds)
        ? level.worldBounds
        : Rectangle{0.0f, 0.0f, 1600.0f, 900.0f};

    camera.offset = {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
    camera.target = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
    camera.zoom = std::clamp(
        std::min(canvas.width / bounds.width, canvas.height / bounds.height) * 0.9f,
        0.05f,
        8.0f
    );
    NotifyTileOperation("zoom " + std::to_string(static_cast<int>(std::round(camera.zoom * 100.0f))));
}

void LevelEditor::FrameSelection() {
    Rectangle bounds = groupSelection.size() > 1 ? SelectionGroupBounds() : Rectangle{};
    if (groupSelection.size() <= 1) {
        if (const Rectangle* rect = SelectedRectangle()) bounds = *rect;
        else if (selection.kind == SelectionKind::PlayerStart)
            bounds = {level.playerStart.x - 48.0f, level.playerStart.y - 48.0f, 96.0f, 96.0f};
        else if (selection.kind == SelectionKind::SceneObject) bounds = selectedSceneBounds;
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) { FrameLevel(); return; }
    const Rectangle canvas = GetCanvasBounds();
    camera.offset = {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
    camera.target = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
    camera.zoom = std::clamp(std::min(canvas.width / bounds.width, canvas.height / bounds.height) * 0.72f,
        0.05f, 8.0f);
    NotifyTileOperation("zoom " + std::to_string(static_cast<int>(std::round(camera.zoom * 100.0f))));
    statusText = groupSelection.size() > 1 ? "Framed selected objects." : "Framed selection.";
}

void LevelEditor::DuplicateSelection() {
    std::vector<Selection> source = groupSelection;
    if (source.empty() && selection.kind != SelectionKind::None) source.push_back(selection);
    if (source.empty()) { statusText = "Nothing selected to duplicate."; return; }
    PushTileUndo();
    if (source.size() == 1 && source.front().kind == SelectionKind::SceneObject) {
        if (!DuplicateSelectedSceneObject()) {
            if (!viewportOnly) tileUndoHistory.pop_back();
            statusText = "This selection cannot be duplicated.";
            return;
        }
        MarkDirty("Duplicated selection.");
        if (viewportOnly) NotifySelectionChanged();
        return;
    }
    std::vector<Selection> duplicates;
    const Vector2 offset{gridSnap, gridSnap};
    for (const Selection& item : source) {
        if (item.kind == SelectionKind::SceneObject) {
            SetPrimarySelection(item);
            if (DuplicateSelectedSceneObject()) duplicates.push_back(selection);
            continue;
        }
        const Rectangle* original = RectangleForSelection(item);
        if (original == nullptr || item.kind == SelectionKind::Exit) continue;
        Rectangle copy = *original;
        copy.x += offset.x;
        copy.y += offset.y;
        switch (item.kind) {
        case SelectionKind::Solid:
            level.baseSolids.push_back(copy); duplicates.push_back({item.kind, static_cast<int>(level.baseSolids.size()) - 1}); break;
        case SelectionKind::Platform:
            level.pitPlatforms.push_back(copy); duplicates.push_back({item.kind, static_cast<int>(level.pitPlatforms.size()) - 1}); break;
        case SelectionKind::Ladder:
            level.ladders.push_back(copy); duplicates.push_back({item.kind, static_cast<int>(level.ladders.size()) - 1}); break;
        case SelectionKind::CameraZone:
            level.cameraZones.push_back(copy); duplicates.push_back({item.kind, static_cast<int>(level.cameraZones.size()) - 1}); break;
        case SelectionKind::Darkness:
            level.darknessAreas.push_back(copy); duplicates.push_back({item.kind, static_cast<int>(level.darknessAreas.size()) - 1}); break;
        default: break;
        }
    }
    if (duplicates.empty()) { if (!viewportOnly) tileUndoHistory.pop_back(); statusText = "This selection cannot be duplicated yet."; return; }
    groupSelection = std::move(duplicates);
    SetPrimarySelection(groupSelection.back());
    MarkDirty(groupSelection.size() > 1 ? "Duplicated selected objects." : "Duplicated selection.");
    if (viewportOnly) { NotifyEditableObjectState(); NotifySelectionChanged(); }
}

Vector2 LevelEditor::SnapWorldPoint(Vector2 point) const {
    return {
        roundf(point.x / gridSnap) * gridSnap,
        roundf(point.y / gridSnap) * gridSnap
    };
}

Rectangle LevelEditor::MakeRect(Vector2 a, Vector2 b) const {
    Rectangle rect{
        fminf(a.x, b.x),
        fminf(a.y, b.y),
        fabsf(a.x - b.x),
        fabsf(a.y - b.y)
    };
    rect.width = fmaxf(gridSnap, rect.width);
    rect.height = fmaxf(gridSnap, rect.height);
    return rect;
}

void LevelEditor::MarkDirty(const std::string& message) {
    dirty = true;
    statusText = message;
    if (viewportOnly) NotifyEditableObjectState();
}

LevelEditor::Selection LevelEditor::HitTest(Vector2 world) {
    auto hitRects = [&](const std::vector<Rectangle>& rects, SelectionKind kind) {
        for (int index = static_cast<int>(rects.size()) - 1; index >= 0; --index) {
            if (CheckCollisionPointRec(world, rects[static_cast<size_t>(index)])) {
                return Selection{kind, index};
            }
        }
        return Selection{};
    };

    selectedSceneName.clear();
    selectedSceneBounds = {};

    if (CheckCollisionPointCircle(world, level.playerStart, 18.0f)) return {SelectionKind::PlayerStart, 0};
    if (CheckCollisionPointRec(world, level.exitTrigger)) return {SelectionKind::Exit, 0};

    Selection hit = hitRects(level.pitPlatforms, SelectionKind::Platform);
    if (hit.kind != SelectionKind::None) return hit;
    hit = hitRects(level.baseSolids, SelectionKind::Solid);
    if (hit.kind != SelectionKind::None) return hit;
    hit = hitRects(level.ladders, SelectionKind::Ladder);
    if (hit.kind != SelectionKind::None) return hit;
    hit = hitRects(level.cameraZones, SelectionKind::CameraZone);
    if (hit.kind != SelectionKind::None) return hit;
    hit = hitRects(level.darknessAreas, SelectionKind::Darkness);
    if (hit.kind != SelectionKind::None) return hit;

    auto sceneRect = [&](Rectangle bounds, const std::string& name, int index,
        const std::string& outlinerName = std::string{}, int outlinerIndex = -1) -> Selection {
        if (bounds.width > 0.0f && bounds.height > 0.0f && CheckCollisionPointRec(world, bounds)) {
            selectedSceneBounds = bounds;
            selectedSceneName = name;
            return {SelectionKind::SceneObject, index, name, bounds,
                outlinerName.empty() ? name : outlinerName, outlinerIndex < 0 ? index : outlinerIndex};
        }
        return {};
    };
    auto sceneCircle = [&](Vector2 center, float radius, const std::string& name, int index) -> Selection {
        if (radius > 0.0f && CheckCollisionPointCircle(world, center, radius)) {
            selectedSceneBounds = {center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f};
            selectedSceneName = name;
            return {SelectionKind::SceneObject, index, name,
                {center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f}, name, index};
        }
        return {};
    };
    auto indexedName = [](const char* name, int index) {
        return std::string(name) + " " + std::to_string(index + 1);
    };

#define HIT_SCENE_RECTS(collection, boundsExpression, label) \
    for (int index = static_cast<int>((collection).size()) - 1; index >= 0; --index) { \
        const auto& item = (collection)[static_cast<size_t>(index)]; \
        hit = sceneRect((boundsExpression), indexedName((label), index), index); \
        if (hit.kind != SelectionKind::None) return hit; \
    }
#define HIT_SCENE_CIRCLES(collection, centerExpression, radiusExpression, label) \
    for (int index = static_cast<int>((collection).size()) - 1; index >= 0; --index) { \
        const auto& item = (collection)[static_cast<size_t>(index)]; \
        hit = sceneCircle((centerExpression), (radiusExpression), indexedName((label), index), index); \
        if (hit.kind != SelectionKind::None) return hit; \
    }

    HIT_SCENE_RECTS(level.enemies, item.rect, "Enemy");
    for (int index = static_cast<int>(level.guideObjects.size()) - 1; index >= 0; --index) {
        const GuideObject& item = level.guideObjects[static_cast<size_t>(index)];
        int typeIndex = 0;
        for (int prior = 0; prior <= index; ++prior)
            if (level.guideObjects[static_cast<size_t>(prior)].type == item.type) ++typeIndex;
        hit = sceneRect(GetGuideObjectBounds(item), indexedName("Guide Object", index), index,
            GetGuideObjectName(item.type), typeIndex - 1);
        if (hit.kind != SelectionKind::None) return hit;
    }
    HIT_SCENE_RECTS(level.breakableTiles, item.rect, "Breakable Tile");
    HIT_SCENE_RECTS(level.directionalSpikeHazards, item.rect, "Directional Spikes");
    HIT_SCENE_RECTS(level.buttons, item.rect, "Button");
    HIT_SCENE_RECTS(level.fluids, item.bounds, "Fluid Field");
    HIT_SCENE_RECTS(level.stoneBlocks, item.rect, "Stone Block");
    HIT_SCENE_RECTS(level.weights, item.rect, "Hanging Weight");
    HIT_SCENE_CIRCLES(level.rotaryLatches, item.center, item.radius, "Rotary Latch");
    HIT_SCENE_CIRCLES(level.boulders, item.center, item.radius, "Boulder");
    HIT_SCENE_CIRCLES(level.physicsWheels, item.center, item.radius, "Physics Wheel");
    HIT_SCENE_CIRCLES(level.gears, item.center, item.radius * GearOuterRadiusScale, "Gear");
    HIT_SCENE_CIRCLES(level.flywheels, item.center, item.radius, "Flywheel");
    HIT_SCENE_CIRCLES(level.steeringWheels, item.center, item.radius, "Steering Wheel");
    HIT_SCENE_CIRCLES(level.pinwheels, item.center, item.radius, "Pinwheel");

    for (int index = static_cast<int>(level.portalPairs.size()) - 1; index >= 0; --index) {
        const PortalPair& item = level.portalPairs[static_cast<size_t>(index)];
        hit = sceneRect(item.exit, indexedName("Portal Exit", index), index, "Portal Pair", index);
        if (hit.kind != SelectionKind::None) return hit;
        hit = sceneRect(item.entrance, indexedName("Portal Entrance", index), index, "Portal Pair", index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.pulleys.size()) - 1; index >= 0; --index) {
        hit = sceneCircle(level.pulleys[static_cast<size_t>(index)], 30.0f, indexedName("Pulley", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.screws.size()) - 1; index >= 0; --index) {
        const Screw& item = level.screws[static_cast<size_t>(index)];
        const float extent = item.length * 0.5f + item.radius;
        hit = sceneRect({item.center.x - extent, item.center.y - extent, extent * 2.0f, extent * 2.0f},
            indexedName("Screw", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.fans.size()) - 1; index >= 0; --index) {
        const Fan& item = level.fans[static_cast<size_t>(index)];
        hit = sceneRect({item.center.x - item.width * 0.5f, item.center.y - item.width * 0.5f,
            item.length + item.width, item.width}, indexedName("Fan", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    auto hitBeam = [&](Vector2 center, float length, float thickness, const char* label, int index) {
        const float extent = length * 0.5f + thickness;
        return sceneRect({center.x - extent, center.y - extent, extent * 2.0f, extent * 2.0f},
            indexedName(label, index), index);
    };
    for (int index = static_cast<int>(level.ramps.size()) - 1; index >= 0; --index) {
        const Ramp& item = level.ramps[static_cast<size_t>(index)];
        hit = hitBeam(item.center, item.length, item.thickness, "Ramp", index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.seeSaws.size()) - 1; index >= 0; --index) {
        const SeeSaw& item = level.seeSaws[static_cast<size_t>(index)];
        hit = hitBeam(item.pivot, item.length, item.thickness, "See Saw", index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.trapDoors.size()) - 1; index >= 0; --index) {
        const TrapDoor& item = level.trapDoors[static_cast<size_t>(index)];
        const float extent = item.length + item.thickness;
        hit = sceneRect({item.hinge.x - extent, item.hinge.y - extent, extent * 2.0f, extent * 2.0f},
            indexedName("Trap Door", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    auto hitFlexible = [&](const auto& collection, const char* label) -> Selection {
        for (int index = static_cast<int>(collection.size()) - 1; index >= 0; --index) {
            const auto& item = collection[static_cast<size_t>(index)];
            const float padding = 10.0f;
            Rectangle bounds{fminf(item.start.x, item.end.x) - padding, fminf(item.start.y, item.end.y) - padding,
                fabsf(item.end.x - item.start.x) + padding * 2.0f,
                fabsf(item.end.y - item.start.y) + padding * 2.0f};
            Selection result = sceneRect(bounds, indexedName(label, index), index);
            if (result.kind != SelectionKind::None) return result;
        }
        return {};
    };
    hit = hitFlexible(level.chains, "Chain");
    if (hit.kind != SelectionKind::None) return hit;
    hit = hitFlexible(level.physicsRopes, "Physics Rope");
    if (hit.kind != SelectionKind::None) return hit;
    for (int index = static_cast<int>(level.arrowTraps.size()) - 1; index >= 0; --index) {
        const Vector2 position = level.arrowTraps[static_cast<size_t>(index)].position;
        hit = sceneRect({position.x - 18.0f, position.y - 18.0f, 36.0f, 36.0f},
            indexedName("Arrow Trap", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.visualTiles.size()) - 1; index >= 0; --index) {
        const Vector2 position = level.visualTiles[static_cast<size_t>(index)].position;
        hit = sceneRect({position.x, position.y, 32.0f, 32.0f}, indexedName("Visual Tile", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    for (int index = static_cast<int>(level.labels.size()) - 1; index >= 0; --index) {
        const LevelLabel& item = level.labels[static_cast<size_t>(index)];
        const int padding = std::max(5, static_cast<int>(roundf(item.fontSize * 0.42f)));
        const float width = static_cast<float>(MeasureText(item.text.c_str(), item.fontSize) + padding * 2);
        const float height = static_cast<float>(item.fontSize + padding * 2);
        hit = sceneRect({item.position.x, item.position.y, width, height}, indexedName("Label", index), index);
        if (hit.kind != SelectionKind::None) return hit;
    }
    hit = sceneCircle(level.valve.center, level.valve.radius, "Valve", 0);
    if (hit.kind != SelectionKind::None) return hit;
    hit = sceneRect(level.waterPit.bounds, "Water Pit", 0);
    if (hit.kind != SelectionKind::None) return hit;
    hit = sceneRect(level.spikeHazard, "Spike Hazard", 0);
    if (hit.kind != SelectionKind::None) return hit;
    hit = sceneCircle(level.clockFaceCenter, level.clockFaceRadius, "Clock Face", 0);

#undef HIT_SCENE_CIRCLES
#undef HIT_SCENE_RECTS
    return hit;
}

Rectangle* LevelEditor::SelectedRectangle() {
    return RectangleForSelection(selection);
}

Rectangle* LevelEditor::RectangleForSelection(const Selection& item) {
    switch (item.kind) {
    case SelectionKind::Solid:
        return item.index >= 0 && item.index < static_cast<int>(level.baseSolids.size())
            ? &level.baseSolids[static_cast<size_t>(item.index)] : nullptr;
    case SelectionKind::Platform:
        return item.index >= 0 && item.index < static_cast<int>(level.pitPlatforms.size())
            ? &level.pitPlatforms[static_cast<size_t>(item.index)] : nullptr;
    case SelectionKind::Ladder:
        return item.index >= 0 && item.index < static_cast<int>(level.ladders.size())
            ? &level.ladders[static_cast<size_t>(item.index)] : nullptr;
    case SelectionKind::CameraZone:
        return item.index >= 0 && item.index < static_cast<int>(level.cameraZones.size())
            ? &level.cameraZones[static_cast<size_t>(item.index)] : nullptr;
    case SelectionKind::Darkness:
        return item.index >= 0 && item.index < static_cast<int>(level.darknessAreas.size())
            ? &level.darknessAreas[static_cast<size_t>(item.index)] : nullptr;
    case SelectionKind::Exit:
        return &level.exitTrigger;
    case SelectionKind::None:
    case SelectionKind::PlayerStart:
    case SelectionKind::SceneObject:
        return nullptr;
    }
    return nullptr;
}

const Rectangle* LevelEditor::SelectedRectangle() const {
    return const_cast<LevelEditor*>(this)->SelectedRectangle();
}

const Rectangle* LevelEditor::RectangleForSelection(const Selection& item) const {
    return const_cast<LevelEditor*>(this)->RectangleForSelection(item);
}

bool LevelEditor::IsGroupSelected(const Selection& item) const {
    return std::any_of(groupSelection.begin(), groupSelection.end(), [&](const Selection& selected) {
        return selected.kind == item.kind && selected.index == item.index &&
            (item.kind != SelectionKind::SceneObject || selected.sceneName == item.sceneName);
    });
}

void LevelEditor::SetPrimarySelection(const Selection& item) {
    selection = item;
    if (item.kind == SelectionKind::SceneObject) {
        selectedSceneName = item.sceneName;
        selectedSceneBounds = item.sceneBounds;
    }
}

Rectangle LevelEditor::SelectionGroupBounds() const {
    Rectangle bounds{};
    bool initialized = false;
    for (const Selection& item : groupSelection) {
        Rectangle itemBounds{};
        if (const Rectangle* rect = RectangleForSelection(item)) itemBounds = *rect;
        else if (item.kind == SelectionKind::PlayerStart)
            itemBounds = {level.playerStart.x - 12.0f, level.playerStart.y - 12.0f, 24.0f, 24.0f};
        else if (item.kind == SelectionKind::SceneObject) itemBounds = item.sceneBounds;
        else continue;
        if (!initialized) { bounds = itemBounds; initialized = true; continue; }
        const float right = std::max(bounds.x + bounds.width, itemBounds.x + itemBounds.width);
        const float bottom = std::max(bounds.y + bounds.height, itemBounds.y + itemBounds.height);
        bounds.x = std::min(bounds.x, itemBounds.x);
        bounds.y = std::min(bounds.y, itemBounds.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
    }
    return bounds;
}

float* LevelEditor::SelectedRotation() {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0) return nullptr;
    const size_t index = static_cast<size_t>(selection.index);
    if (selectedSceneName.rfind("Boulder ", 0) == 0 && index < level.boulders.size()) return &level.boulders[index].rotation;
    if (selectedSceneName.rfind("Physics Wheel ", 0) == 0 && index < level.physicsWheels.size()) return &level.physicsWheels[index].rotation;
    if (selectedSceneName.rfind("Rotary Latch ", 0) == 0 && index < level.rotaryLatches.size()) return &level.rotaryLatches[index].angle;
    if (selectedSceneName.rfind("Gear ", 0) == 0 && index < level.gears.size()) return &level.gears[index].rotation;
    if (selectedSceneName.rfind("Flywheel ", 0) == 0 && index < level.flywheels.size()) return &level.flywheels[index].rotation;
    if (selectedSceneName.rfind("Steering Wheel ", 0) == 0 && index < level.steeringWheels.size())
        return &level.steeringWheels[index].rotation;
    if (selectedSceneName.rfind("Screw ", 0) == 0 && index < level.screws.size()) return &level.screws[index].angle;
    if (selectedSceneName.rfind("Fan ", 0) == 0 && index < level.fans.size()) return &level.fans[index].rotation;
    if (selectedSceneName.rfind("Pinwheel ", 0) == 0 && index < level.pinwheels.size()) return &level.pinwheels[index].rotation;
    if (selectedSceneName.rfind("See Saw ", 0) == 0 && index < level.seeSaws.size()) return &level.seeSaws[index].angle;
    if (selectedSceneName.rfind("Ramp ", 0) == 0 && index < level.ramps.size()) return &level.ramps[index].angle;
    if (selectedSceneName.rfind("Trap Door ", 0) == 0 && index < level.trapDoors.size()) return &level.trapDoors[index].angle;
    return nullptr;
}

const float* LevelEditor::SelectedRotation() const {
    return const_cast<LevelEditor*>(this)->SelectedRotation();
}

Vector2 LevelEditor::SelectedRotationCenter() const {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0)
        return {selectedSceneBounds.x + selectedSceneBounds.width * 0.5f,
            selectedSceneBounds.y + selectedSceneBounds.height * 0.5f};
    const size_t index = static_cast<size_t>(selection.index);
    if (selectedSceneName.rfind("Boulder ", 0) == 0 && index < level.boulders.size()) return level.boulders[index].center;
    if (selectedSceneName.rfind("Physics Wheel ", 0) == 0 && index < level.physicsWheels.size()) return level.physicsWheels[index].center;
    if (selectedSceneName.rfind("Rotary Latch ", 0) == 0 && index < level.rotaryLatches.size()) return level.rotaryLatches[index].center;
    if (selectedSceneName.rfind("Gear ", 0) == 0 && index < level.gears.size()) return level.gears[index].center;
    if (selectedSceneName.rfind("Flywheel ", 0) == 0 && index < level.flywheels.size()) return level.flywheels[index].center;
    if (selectedSceneName.rfind("Steering Wheel ", 0) == 0 && index < level.steeringWheels.size()) return level.steeringWheels[index].center;
    if (selectedSceneName.rfind("Screw ", 0) == 0 && index < level.screws.size()) return level.screws[index].center;
    if (selectedSceneName.rfind("Fan ", 0) == 0 && index < level.fans.size()) return level.fans[index].center;
    if (selectedSceneName.rfind("Pinwheel ", 0) == 0 && index < level.pinwheels.size()) return level.pinwheels[index].center;
    if (selectedSceneName.rfind("See Saw ", 0) == 0 && index < level.seeSaws.size()) return level.seeSaws[index].pivot;
    if (selectedSceneName.rfind("Ramp ", 0) == 0 && index < level.ramps.size()) return level.ramps[index].center;
    if (selectedSceneName.rfind("Trap Door ", 0) == 0 && index < level.trapDoors.size()) return level.trapDoors[index].hinge;
    return {selectedSceneBounds.x + selectedSceneBounds.width * 0.5f,
        selectedSceneBounds.y + selectedSceneBounds.height * 0.5f};
}

bool LevelEditor::TranslateSelectedScene(Vector2 delta) {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0) return false;
    const size_t index = static_cast<size_t>(selection.index);
    const auto movePoint = [&](Vector2& point) { point.x += delta.x; point.y += delta.y; };
    const auto moveRect = [&](Rectangle& rect) { rect.x += delta.x; rect.y += delta.y; };
    bool moved = true;
    if (selectedSceneName.rfind("Enemy ", 0) == 0 && index < level.enemies.size()) moveRect(level.enemies[index].rect);
    else if (selectedSceneName.rfind("Guide Object ", 0) == 0 && index < level.guideObjects.size()) {
        GuideObject& object = level.guideObjects[index];
        movePoint(object.transform.position); movePoint(object.origin);
        movePoint(object.constraint.anchorA); movePoint(object.constraint.anchorB); moveRect(object.sensor.bounds);
    }
    else if (selectedSceneName.rfind("Breakable Tile ", 0) == 0 && index < level.breakableTiles.size()) moveRect(level.breakableTiles[index].rect);
    else if (selectedSceneName.rfind("Directional Spikes ", 0) == 0 && index < level.directionalSpikeHazards.size()) moveRect(level.directionalSpikeHazards[index].rect);
    else if (selectedSceneName.rfind("Button ", 0) == 0 && index < level.buttons.size()) moveRect(level.buttons[index].rect);
    else if (selectedSceneName.rfind("Fluid Field ", 0) == 0 && index < level.fluids.size()) moveRect(level.fluids[index].bounds);
    else if (selectedSceneName.rfind("Stone Block ", 0) == 0 && index < level.stoneBlocks.size()) moveRect(level.stoneBlocks[index].rect);
    else if (selectedSceneName.rfind("Hanging Weight ", 0) == 0 && index < level.weights.size()) {
        HangingWeight& weight = level.weights[index];
        size_t pulleyIndex = level.pulleys.size();
        float nearestDistance = INFINITY;
        for (size_t pulley = 0; pulley < level.pulleys.size(); ++pulley) {
            const float dx = level.pulleys[pulley].x - weight.pulley.x;
            const float dy = level.pulleys[pulley].y - weight.pulley.y;
            const float distance = dx * dx + dy * dy;
            if (distance < nearestDistance) { nearestDistance = distance; pulleyIndex = pulley; }
        }
        if (pulleyIndex < level.pulleys.size()) movePoint(level.pulleys[pulleyIndex]);
        movePoint(weight.pulley);
        moveRect(weight.rect);
    }
    else if (selectedSceneName.rfind("Rotary Latch ", 0) == 0 && index < level.rotaryLatches.size()) movePoint(level.rotaryLatches[index].center);
    else if (selectedSceneName.rfind("Boulder ", 0) == 0 && index < level.boulders.size()) movePoint(level.boulders[index].center);
    else if (selectedSceneName.rfind("Physics Wheel ", 0) == 0 && index < level.physicsWheels.size()) movePoint(level.physicsWheels[index].center);
    else if (selectedSceneName.rfind("Gear ", 0) == 0 && index < level.gears.size()) movePoint(level.gears[index].center);
    else if (selectedSceneName.rfind("Flywheel ", 0) == 0 && index < level.flywheels.size()) movePoint(level.flywheels[index].center);
    else if (selectedSceneName.rfind("Steering Wheel ", 0) == 0 && index < level.steeringWheels.size()) movePoint(level.steeringWheels[index].center);
    else if (selectedSceneName.rfind("Pinwheel ", 0) == 0 && index < level.pinwheels.size()) movePoint(level.pinwheels[index].center);
    else if (selectedSceneName.rfind("Portal Entrance ", 0) == 0 && index < level.portalPairs.size()) moveRect(level.portalPairs[index].entrance);
    else if (selectedSceneName.rfind("Portal Exit ", 0) == 0 && index < level.portalPairs.size()) moveRect(level.portalPairs[index].exit);
    else if (selectedSceneName.rfind("Pulley ", 0) == 0 && index < level.pulleys.size()) movePoint(level.pulleys[index]);
    else if (selectedSceneName.rfind("Screw ", 0) == 0 && index < level.screws.size()) movePoint(level.screws[index].center);
    else if (selectedSceneName.rfind("Fan ", 0) == 0 && index < level.fans.size()) movePoint(level.fans[index].center);
    else if (selectedSceneName.rfind("Ramp ", 0) == 0 && index < level.ramps.size()) movePoint(level.ramps[index].center);
    else if (selectedSceneName.rfind("See Saw ", 0) == 0 && index < level.seeSaws.size()) movePoint(level.seeSaws[index].pivot);
    else if (selectedSceneName.rfind("Trap Door ", 0) == 0 && index < level.trapDoors.size()) movePoint(level.trapDoors[index].hinge);
    else if (selectedSceneName.rfind("Chain ", 0) == 0 && index < level.chains.size()) { movePoint(level.chains[index].start); movePoint(level.chains[index].end); }
    else if (selectedSceneName.rfind("Physics Rope ", 0) == 0 && index < level.physicsRopes.size()) { movePoint(level.physicsRopes[index].start); movePoint(level.physicsRopes[index].end); }
    else if (selectedSceneName.rfind("Arrow Trap ", 0) == 0 && index < level.arrowTraps.size()) movePoint(level.arrowTraps[index].position);
    else if (selectedSceneName.rfind("Visual Tile ", 0) == 0 && index < level.visualTiles.size()) movePoint(level.visualTiles[index].position);
    else if (selectedSceneName.rfind("Label ", 0) == 0 && index < level.labels.size()) movePoint(level.labels[index].position);
    else if (selectedSceneName == "Valve") movePoint(level.valve.center);
    else if (selectedSceneName == "Water Pit") {
        moveRect(level.waterPit.bounds);
        level.waterPit.surfaceY += delta.y;
        level.waterPit.targetSurfaceY += delta.y;
    }
    else if (selectedSceneName == "Spike Hazard") moveRect(level.spikeHazard);
    else if (selectedSceneName == "Clock Face") movePoint(level.clockFaceCenter);
    else moved = false;
    if (moved) {
        selectedSceneBounds.x += delta.x; selectedSceneBounds.y += delta.y;
        selection.sceneBounds = selectedSceneBounds;
    }
    return moved;
}

bool LevelEditor::ScaleSelectedScene(Vector2 delta) {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0) return false;
    if ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) &&
        selectedSceneBounds.width > 0.0f && selectedSceneBounds.height > 0.0f) {
        const float scaleX = delta.x / selectedSceneBounds.width;
        const float scaleY = delta.y / selectedSceneBounds.height;
        const float uniformScale = fabsf(scaleX) >= fabsf(scaleY) ? scaleX : scaleY;
        delta = {selectedSceneBounds.width * uniformScale, selectedSceneBounds.height * uniformScale};
    }
    const size_t index = static_cast<size_t>(selection.index);
    const float radialDelta = (delta.x + delta.y) * 0.25f;
    const auto scaleRect = [&](Rectangle& rect) {
        const bool centerScale = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        rect.width = std::max(1.0f, rect.width + delta.x);
        rect.height = std::max(1.0f, rect.height + delta.y);
        if (centerScale) {
            rect.x -= delta.x * 0.5f;
            rect.y -= delta.y * 0.5f;
        }
    };
    const auto scaleRadius = [&](float& radius) { radius = std::max(1.0f, radius + radialDelta); };
    bool scaled = true;
    if (selectedSceneName.rfind("Enemy ", 0) == 0 && index < level.enemies.size()) scaleRect(level.enemies[index].rect);
    else if (selectedSceneName.rfind("Guide Object ", 0) == 0 && index < level.guideObjects.size()) {
        GuideObject& object = level.guideObjects[index];
        if (object.collider.shape == ColliderShape::Circle) scaleRadius(object.collider.radius);
        else if (object.collider.shape == ColliderShape::Rectangle) {
            object.collider.size.x = std::max(1.0f, object.collider.size.x + delta.x);
            object.collider.size.y = std::max(1.0f, object.collider.size.y + delta.y);
        }
        else { object.length = std::max(1.0f, object.length + delta.x); object.width = std::max(1.0f, object.width + delta.y); }
    }
    else if (selectedSceneName.rfind("Breakable Tile ", 0) == 0 && index < level.breakableTiles.size()) scaleRect(level.breakableTiles[index].rect);
    else if (selectedSceneName.rfind("Directional Spikes ", 0) == 0 && index < level.directionalSpikeHazards.size()) scaleRect(level.directionalSpikeHazards[index].rect);
    else if (selectedSceneName.rfind("Button ", 0) == 0 && index < level.buttons.size()) scaleRect(level.buttons[index].rect);
    else if (selectedSceneName.rfind("Fluid Field ", 0) == 0 && index < level.fluids.size()) scaleRect(level.fluids[index].bounds);
    else if (selectedSceneName.rfind("Stone Block ", 0) == 0 && index < level.stoneBlocks.size()) scaleRect(level.stoneBlocks[index].rect);
    else if (selectedSceneName.rfind("Rotary Latch ", 0) == 0 && index < level.rotaryLatches.size()) scaleRadius(level.rotaryLatches[index].radius);
    else if (selectedSceneName.rfind("Boulder ", 0) == 0 && index < level.boulders.size()) scaleRadius(level.boulders[index].radius);
    else if (selectedSceneName.rfind("Physics Wheel ", 0) == 0 && index < level.physicsWheels.size()) scaleRadius(level.physicsWheels[index].radius);
    else if (selectedSceneName.rfind("Gear ", 0) == 0 && index < level.gears.size()) scaleRadius(level.gears[index].radius);
    else if (selectedSceneName.rfind("Flywheel ", 0) == 0 && index < level.flywheels.size()) scaleRadius(level.flywheels[index].radius);
    else if (selectedSceneName.rfind("Steering Wheel ", 0) == 0 && index < level.steeringWheels.size()) scaleRadius(level.steeringWheels[index].radius);
    else if (selectedSceneName.rfind("Pinwheel ", 0) == 0 && index < level.pinwheels.size()) scaleRadius(level.pinwheels[index].radius);
    else if (selectedSceneName.rfind("Portal Entrance ", 0) == 0 && index < level.portalPairs.size()) scaleRect(level.portalPairs[index].entrance);
    else if (selectedSceneName.rfind("Portal Exit ", 0) == 0 && index < level.portalPairs.size()) scaleRect(level.portalPairs[index].exit);
    else if (selectedSceneName.rfind("Screw ", 0) == 0 && index < level.screws.size()) { level.screws[index].length = std::max(1.0f, level.screws[index].length + delta.x); scaleRadius(level.screws[index].radius); }
    else if (selectedSceneName.rfind("Fan ", 0) == 0 && index < level.fans.size()) { level.fans[index].length = std::max(1.0f, level.fans[index].length + delta.x); level.fans[index].width = std::max(1.0f, level.fans[index].width + delta.y); }
    else if (selectedSceneName.rfind("Ramp ", 0) == 0 && index < level.ramps.size()) { level.ramps[index].length = std::max(1.0f, level.ramps[index].length + delta.x); level.ramps[index].thickness = std::max(1.0f, level.ramps[index].thickness + delta.y); }
    else if (selectedSceneName.rfind("See Saw ", 0) == 0 && index < level.seeSaws.size()) { level.seeSaws[index].length = std::max(1.0f, level.seeSaws[index].length + delta.x); level.seeSaws[index].thickness = std::max(1.0f, level.seeSaws[index].thickness + delta.y); }
    else if (selectedSceneName.rfind("Trap Door ", 0) == 0 && index < level.trapDoors.size()) { level.trapDoors[index].length = std::max(1.0f, level.trapDoors[index].length + delta.x); level.trapDoors[index].thickness = std::max(1.0f, level.trapDoors[index].thickness + delta.y); }
    else if (selectedSceneName.rfind("Chain ", 0) == 0 && index < level.chains.size())
        level.chains[index].scale = std::max(0.1f, level.chains[index].scale + radialDelta / 24.0f);
    else if (selectedSceneName.rfind("Physics Rope ", 0) == 0 && index < level.physicsRopes.size())
        level.physicsRopes[index].thickness = std::max(1.0f, level.physicsRopes[index].thickness + radialDelta);
    else if (selectedSceneName.rfind("Label ", 0) == 0 && index < level.labels.size()) level.labels[index].fontSize = std::clamp(level.labels[index].fontSize + static_cast<int>(roundf(radialDelta)), 8, 72);
    else if (selectedSceneName.rfind("Hanging Weight ", 0) == 0 && index < level.weights.size()) {
        scaleRect(level.weights[index].rect);
        scaleRadius(level.weights[index].pulleyRadius);
    }
    else if (selectedSceneName == "Water Pit") {
        WaterPit& pit = level.waterPit;
        const float centerY = pit.bounds.y + pit.bounds.height * 0.5f;
        const float scaleY = std::max(1.0f, pit.bounds.height + delta.y) / pit.bounds.height;
        scaleRect(pit.bounds);
        if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
            pit.surfaceY = centerY + (pit.surfaceY - centerY) * scaleY;
            pit.targetSurfaceY = centerY + (pit.targetSurfaceY - centerY) * scaleY;
        }
    }
    else if (selectedSceneName == "Valve") scaleRadius(level.valve.radius);
    else if (selectedSceneName == "Spike Hazard") scaleRect(level.spikeHazard);
    else if (selectedSceneName == "Clock Face") scaleRadius(level.clockFaceRadius);
    else scaled = false;
    if (scaled) {
        const bool centerScale = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        const Vector2 originalLabelCenter{selectedSceneBounds.x + selectedSceneBounds.width * 0.5f,
            selectedSceneBounds.y + selectedSceneBounds.height * 0.5f};
        if (centerScale) {
            const bool rectBacked = selectedSceneName.rfind("Enemy ", 0) == 0 ||
                selectedSceneName.rfind("Breakable Tile ", 0) == 0 ||
                selectedSceneName.rfind("Directional Spikes ", 0) == 0 ||
                selectedSceneName.rfind("Button ", 0) == 0 ||
                selectedSceneName.rfind("Fluid Field ", 0) == 0 ||
                selectedSceneName.rfind("Stone Block ", 0) == 0 ||
                selectedSceneName.rfind("Hanging Weight ", 0) == 0 ||
                selectedSceneName.rfind("Portal Entrance ", 0) == 0 ||
                selectedSceneName.rfind("Portal Exit ", 0) == 0 ||
                selectedSceneName == "Spike Hazard" || selectedSceneName == "Water Pit";
            if (rectBacked) {
                selectedSceneBounds.x -= delta.x * 0.5f;
                selectedSceneBounds.y -= delta.y * 0.5f;
            }
        }
        selectedSceneBounds.width = std::max(1.0f, selectedSceneBounds.width + delta.x);
        selectedSceneBounds.height = std::max(1.0f, selectedSceneBounds.height + delta.y);
        if (centerScale && selectedSceneName.rfind("Label ", 0) == 0 && index < level.labels.size()) {
            const LevelLabel& label = level.labels[index];
            const int padding = std::max(5, static_cast<int>(roundf(label.fontSize * 0.42f)));
            selectedSceneBounds.width = static_cast<float>(MeasureText(label.text.c_str(), label.fontSize) + padding * 2);
            selectedSceneBounds.height = static_cast<float>(label.fontSize + padding * 2);
            selectedSceneBounds.x = originalLabelCenter.x - selectedSceneBounds.width * 0.5f;
            selectedSceneBounds.y = originalLabelCenter.y - selectedSceneBounds.height * 0.5f;
            level.labels[index].position = {selectedSceneBounds.x, selectedSceneBounds.y};
        }
        selection.sceneBounds = selectedSceneBounds;
    }
    return scaled;
}

bool LevelEditor::DeleteSelectedSceneObject() {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0) return false;
    const size_t index = static_cast<size_t>(selection.index);
    const auto eraseAt = [index](auto& collection) {
        if (index >= collection.size()) return false;
        collection.erase(collection.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    };
    if (selectedSceneName.rfind("Enemy ", 0) == 0) return eraseAt(level.enemies);
    if (selectedSceneName.rfind("Guide Object ", 0) == 0) return eraseAt(level.guideObjects);
    if (selectedSceneName.rfind("Breakable Tile ", 0) == 0) return eraseAt(level.breakableTiles);
    if (selectedSceneName.rfind("Directional Spikes ", 0) == 0) return eraseAt(level.directionalSpikeHazards);
    if (selectedSceneName.rfind("Button ", 0) == 0) return eraseAt(level.buttons);
    if (selectedSceneName.rfind("Fluid Field ", 0) == 0) return eraseAt(level.fluids);
    if (selectedSceneName.rfind("Stone Block ", 0) == 0) return eraseAt(level.stoneBlocks);
    if (selectedSceneName.rfind("Hanging Weight ", 0) == 0) return eraseAt(level.weights);
    if (selectedSceneName.rfind("Rotary Latch ", 0) == 0) return eraseAt(level.rotaryLatches);
    if (selectedSceneName.rfind("Boulder ", 0) == 0) return eraseAt(level.boulders);
    if (selectedSceneName.rfind("Physics Wheel ", 0) == 0) return eraseAt(level.physicsWheels);
    if (selectedSceneName.rfind("Gear ", 0) == 0) return eraseAt(level.gears);
    if (selectedSceneName.rfind("Flywheel ", 0) == 0) return eraseAt(level.flywheels);
    if (selectedSceneName.rfind("Steering Wheel ", 0) == 0) return eraseAt(level.steeringWheels);
    if (selectedSceneName.rfind("Pinwheel ", 0) == 0) return eraseAt(level.pinwheels);
    if (selectedSceneName.rfind("Portal ", 0) == 0) return eraseAt(level.portalPairs);
    if (selectedSceneName.rfind("Pulley ", 0) == 0) return eraseAt(level.pulleys);
    if (selectedSceneName.rfind("Screw ", 0) == 0) return eraseAt(level.screws);
    if (selectedSceneName.rfind("Fan ", 0) == 0) return eraseAt(level.fans);
    if (selectedSceneName.rfind("Ramp ", 0) == 0) return eraseAt(level.ramps);
    if (selectedSceneName.rfind("See Saw ", 0) == 0) return eraseAt(level.seeSaws);
    if (selectedSceneName.rfind("Trap Door ", 0) == 0) return eraseAt(level.trapDoors);
    if (selectedSceneName.rfind("Chain ", 0) == 0) return eraseAt(level.chains);
    if (selectedSceneName.rfind("Physics Rope ", 0) == 0) return eraseAt(level.physicsRopes);
    if (selectedSceneName.rfind("Arrow Trap ", 0) == 0) return eraseAt(level.arrowTraps);
    if (selectedSceneName.rfind("Label ", 0) == 0) return eraseAt(level.labels);
    if (selectedSceneName == "Valve") { level.valve = {}; return true; }
    if (selectedSceneName == "Water Pit") { level.waterPit = {}; return true; }
    if (selectedSceneName == "Spike Hazard") { level.spikeHazard = {}; return true; }
    if (selectedSceneName == "Clock Face") {
        level.clockFaceCenter = {};
        level.clockFaceRadius = 0.0f;
        return true;
    }
    return false;
}

bool LevelEditor::DuplicateSelectedSceneObject() {
    if (selection.kind != SelectionKind::SceneObject || selection.index < 0) return false;
    const size_t index = static_cast<size_t>(selection.index);
    const auto duplicateAt = [this, index](auto& collection) {
        if (index >= collection.size()) return false;
        collection.push_back(collection[index]);
        selection.index = static_cast<int>(collection.size()) - 1;
        return true;
    };
    bool duplicated = false;
    bool alreadyOffset = false;
    if (selectedSceneName.rfind("Enemy ", 0) == 0) duplicated = duplicateAt(level.enemies);
    else if (selectedSceneName.rfind("Guide Object ", 0) == 0) duplicated = duplicateAt(level.guideObjects);
    else if (selectedSceneName.rfind("Breakable Tile ", 0) == 0) duplicated = duplicateAt(level.breakableTiles);
    else if (selectedSceneName.rfind("Directional Spikes ", 0) == 0) duplicated = duplicateAt(level.directionalSpikeHazards);
    else if (selectedSceneName.rfind("Button ", 0) == 0) duplicated = duplicateAt(level.buttons);
    else if (selectedSceneName.rfind("Fluid Field ", 0) == 0) duplicated = duplicateAt(level.fluids);
    else if (selectedSceneName.rfind("Stone Block ", 0) == 0) duplicated = duplicateAt(level.stoneBlocks);
    else if (selectedSceneName.rfind("Rotary Latch ", 0) == 0) duplicated = duplicateAt(level.rotaryLatches);
    else if (selectedSceneName.rfind("Boulder ", 0) == 0) duplicated = duplicateAt(level.boulders);
    else if (selectedSceneName.rfind("Physics Wheel ", 0) == 0) duplicated = duplicateAt(level.physicsWheels);
    else if (selectedSceneName.rfind("Gear ", 0) == 0) duplicated = duplicateAt(level.gears);
    else if (selectedSceneName.rfind("Flywheel ", 0) == 0) duplicated = duplicateAt(level.flywheels);
    else if (selectedSceneName.rfind("Steering Wheel ", 0) == 0) duplicated = duplicateAt(level.steeringWheels);
    else if (selectedSceneName.rfind("Pinwheel ", 0) == 0) duplicated = duplicateAt(level.pinwheels);
    else if (selectedSceneName.rfind("Portal ", 0) == 0) duplicated = duplicateAt(level.portalPairs);
    else if (selectedSceneName.rfind("Pulley ", 0) == 0) duplicated = duplicateAt(level.pulleys);
    else if (selectedSceneName.rfind("Hanging Weight ", 0) == 0 && index < level.weights.size()) {
        HangingWeight weight = level.weights[index];
        weight.pulley.x += gridSnap;
        weight.pulley.y += gridSnap;
        weight.rect.x += gridSnap;
        weight.rect.y += gridSnap;
        level.pulleys.push_back(weight.pulley);
        level.weights.push_back(weight);
        selection.index = static_cast<int>(level.weights.size()) - 1;
        selectedSceneBounds = weight.rect;
        selection.sceneBounds = selectedSceneBounds;
        duplicated = true;
        alreadyOffset = true;
    }
    else if (selectedSceneName.rfind("Screw ", 0) == 0) duplicated = duplicateAt(level.screws);
    else if (selectedSceneName.rfind("Fan ", 0) == 0) duplicated = duplicateAt(level.fans);
    else if (selectedSceneName.rfind("Ramp ", 0) == 0) duplicated = duplicateAt(level.ramps);
    else if (selectedSceneName.rfind("See Saw ", 0) == 0) duplicated = duplicateAt(level.seeSaws);
    else if (selectedSceneName.rfind("Trap Door ", 0) == 0) duplicated = duplicateAt(level.trapDoors);
    else if (selectedSceneName.rfind("Chain ", 0) == 0) duplicated = duplicateAt(level.chains);
    else if (selectedSceneName.rfind("Physics Rope ", 0) == 0) duplicated = duplicateAt(level.physicsRopes);
    else if (selectedSceneName.rfind("Arrow Trap ", 0) == 0) duplicated = duplicateAt(level.arrowTraps);
    else if (selectedSceneName.rfind("Label ", 0) == 0) duplicated = duplicateAt(level.labels);
    if (!duplicated) return false;
    selectedSceneName = selectedSceneName.substr(0, selectedSceneName.find_last_of(' ') + 1) +
        std::to_string(selection.index + 1);
    if (selectedSceneName.rfind("Guide Object ", 0) == 0 && selection.index < static_cast<int>(level.guideObjects.size())) {
        const GuideObjectType type = level.guideObjects[static_cast<size_t>(selection.index)].type;
        selection.outlinerName = GetGuideObjectName(type);
        selection.outlinerIndex = 0;
        for (int index = 0; index <= selection.index; ++index)
            if (level.guideObjects[static_cast<size_t>(index)].type == type) ++selection.outlinerIndex;
        --selection.outlinerIndex;
    }
    else {
        selection.outlinerIndex = selection.index;
    }
    if (!alreadyOffset) TranslateSelectedScene({gridSnap, gridSnap});
    groupSelection = {selection};
    return true;
}

LevelEditor::TransformHandle LevelEditor::TransformHandleAt(Vector2 world) const {
    if (transformMode == TransformMode::Rotate && SelectedRotation() != nullptr) {
        const Vector2 center = SelectedRotationCenter();
        const float radius = std::max(selectedSceneBounds.width, selectedSceneBounds.height) * 0.62f;
        const float dx = world.x - center.x;
        const float dy = world.y - center.y;
        const float distance = sqrtf(dx * dx + dy * dy);
        if (fabsf(distance - radius) <= 10.0f / camera.zoom) return TransformHandle::RotateRing;
        return TransformHandle::None;
    }
    const Rectangle* rect = SelectedRectangle();
    Vector2 center{};
    const Rectangle groupBounds = groupSelection.size() > 1 ? SelectionGroupBounds() : Rectangle{};
    if (groupSelection.size() > 1) center = {groupBounds.x + groupBounds.width * 0.5f,
        groupBounds.y + groupBounds.height * 0.5f};
    else if (rect != nullptr) center = {rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f};
    else if (selection.kind == SelectionKind::PlayerStart) center = level.playerStart;
    else if (selection.kind == SelectionKind::SceneObject &&
        const_cast<LevelEditor*>(this)->TranslateSelectedScene({0.0f, 0.0f}))
        center = {selectedSceneBounds.x + selectedSceneBounds.width * 0.5f,
            selectedSceneBounds.y + selectedSceneBounds.height * 0.5f};
    else return TransformHandle::None;

    const float hitRadius = 10.0f / camera.zoom;
    if (transformMode == TransformMode::Move) {
        const float axisLength = 30.0f / camera.zoom;
        const Vector2 xHandle{center.x + axisLength, center.y};
        const Vector2 yHandle{center.x, center.y - axisLength};
        if (CheckCollisionPointCircle(world, xHandle, hitRadius)) return TransformHandle::MoveX;
        if (CheckCollisionPointCircle(world, yHandle, hitRadius)) return TransformHandle::MoveY;
        if (CheckCollisionPointCircle(world, center, hitRadius)) return TransformHandle::MoveCenter;
    }
    else if (transformMode == TransformMode::Scale && (rect != nullptr || groupSelection.size() > 1 ||
        (selection.kind == SelectionKind::SceneObject &&
            const_cast<LevelEditor*>(this)->ScaleSelectedScene({0.0f, 0.0f})))) {
        if (selection.kind == SelectionKind::Exit && groupSelection.size() <= 1) return TransformHandle::None;
        const Rectangle bounds = groupSelection.size() > 1 ? groupBounds :
            (rect != nullptr ? *rect : selectedSceneBounds);
        const Vector2 corner{bounds.x + bounds.width, bounds.y + bounds.height};
        if (CheckCollisionPointCircle(world, corner, hitRadius)) return TransformHandle::ScaleBottomRight;
    }
    return TransformHandle::None;
}

int LevelEditor::OutlinerItemCount() const {
    return 2 + static_cast<int>(level.baseSolids.size() + level.pitPlatforms.size() + level.ladders.size() +
        level.cameraZones.size() + level.darknessAreas.size());
}

LevelEditor::Selection LevelEditor::OutlinerSelectionAt(int row) const {
    if (row == 0) return {SelectionKind::PlayerStart, 0};
    if (row == 1) return {SelectionKind::Exit, 0};
    row -= 2;
    const auto consume = [&](size_t count, SelectionKind kind, int& value) -> Selection {
        if (value < static_cast<int>(count)) return {kind, value};
        value -= static_cast<int>(count);
        return {};
    };
    Selection result = consume(level.baseSolids.size(), SelectionKind::Solid, row);
    if (result.kind != SelectionKind::None) return result;
    result = consume(level.pitPlatforms.size(), SelectionKind::Platform, row);
    if (result.kind != SelectionKind::None) return result;
    result = consume(level.ladders.size(), SelectionKind::Ladder, row);
    if (result.kind != SelectionKind::None) return result;
    result = consume(level.cameraZones.size(), SelectionKind::CameraZone, row);
    if (result.kind != SelectionKind::None) return result;
    return consume(level.darknessAreas.size(), SelectionKind::Darkness, row);
}

std::string LevelEditor::OutlinerLabelAt(int row) const {
    const Selection item = OutlinerSelectionAt(row);
    const int displayIndex = item.index + 1;
    switch (item.kind) {
    case SelectionKind::PlayerStart: return "Player Start";
    case SelectionKind::Exit: return "Exit Trigger";
    case SelectionKind::Solid: return "Solid " + std::to_string(displayIndex);
    case SelectionKind::Platform: return "Platform " + std::to_string(displayIndex);
    case SelectionKind::Ladder: return "Ladder " + std::to_string(displayIndex);
    case SelectionKind::CameraZone: return "Camera Zone " + std::to_string(displayIndex);
    case SelectionKind::Darkness: return "Darkness " + std::to_string(displayIndex);
    case SelectionKind::SceneObject: return selectedSceneName.empty() ? "Scene Object" : selectedSceneName;
    case SelectionKind::None: return "Unknown";
    }
    return "Unknown";
}

void LevelEditor::CommitDetailEdit() {
    if (activeDetailProperty < 0 || detailEditText.empty()) return;
    try {
        const float value = std::stof(detailEditText);
        if (selection.kind == SelectionKind::PlayerStart || SelectedRectangle() != nullptr) PushTileUndo();
        if (selection.kind == SelectionKind::PlayerStart) {
            if (activeDetailProperty == 0) level.playerStart.x = value;
            else if (activeDetailProperty == 1) level.playerStart.y = value;
        }
        else if (Rectangle* rect = SelectedRectangle()) {
            if (activeDetailProperty == 0) rect->x = value;
            else if (activeDetailProperty == 1) rect->y = value;
            else if (activeDetailProperty == 2) rect->width = fmaxf(1.0f, value);
            else if (activeDetailProperty == 3) rect->height = fmaxf(1.0f, value);
        }
        MarkDirty("Updated selection properties.");
    }
    catch (...) {
        statusText = "Enter a valid numeric property value.";
    }
}

void LevelEditor::UpdateEditorPanels() {
    const Vector2 mouse = GetMousePosition();
    const float wheel = GetMouseWheelMove();
    const Rectangle outliner = GetPanelBounds(PanelId::WorldOutliner);
    const int visibleRows = std::max(1, static_cast<int>((outliner.height - 52.0f) / 22.0f));
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, outliner)) {
        outlinerScroll = std::clamp(outlinerScroll - static_cast<int>(wheel), 0,
            std::max(0, OutlinerItemCount() - visibleRows));
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y >= outliner.y + 44.0f &&
        CheckCollisionPointRec(mouse, outliner)) {
        const int visibleRow = static_cast<int>((mouse.y - outliner.y - 44.0f) / 22.0f);
        const int row = outlinerScroll + visibleRow;
        if (visibleRow >= 0 && visibleRow < visibleRows && row < OutlinerItemCount()) {
            selection = OutlinerSelectionAt(row);
            activeTool = EditTool::Select;
            transformMode = TransformMode::Select;
            statusText = "Selected " + OutlinerLabelAt(row) + ".";
        }
    }

    const Rectangle content = GetPanelBounds(PanelId::ContentBrowser);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, content)) {
        for (int category = 0; category < 4; ++category) {
            const Rectangle row{content.x + 8.0f, content.y + 70.0f + category * 26.0f,
                ContentFolderWidth - 16.0f, 24.0f};
            if (CheckCollisionPointRec(mouse, row)) contentCategory = category;
        }
    }

    const Rectangle details = GetPanelBounds(PanelId::Details);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, details)) {
        const int propertyCount = selection.kind == SelectionKind::PlayerStart || selection.kind == SelectionKind::Exit ? 2 :
            (SelectedRectangle() != nullptr ? 4 : 0);
        for (int property = 0; property < propertyCount; ++property) {
            const Rectangle field{details.x + 106.0f, details.y + 89.0f + property * 28.0f,
                details.width - 120.0f, 23.0f};
            if (CheckCollisionPointRec(mouse, field)) {
                activeDetailProperty = property;
                const Rectangle* rect = SelectedRectangle();
                const float value = selection.kind == SelectionKind::PlayerStart ?
                    (property == 0 ? level.playerStart.x : level.playerStart.y) :
                    (property == 0 ? rect->x : property == 1 ? rect->y : property == 2 ? rect->width : rect->height);
                detailEditText = TextFormat("%.0f", value);
            }
        }
    }
    if (activeDetailProperty >= 0) {
        int character = GetCharPressed();
        while (character > 0) {
            if ((character >= '0' && character <= '9') || character == '-' || character == '.') {
                detailEditText.push_back(static_cast<char>(character));
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !detailEditText.empty()) detailEditText.pop_back();
        if (IsKeyPressed(KEY_ENTER)) {
            CommitDetailEdit();
            activeDetailProperty = -1;
        }
        else if (IsKeyPressed(KEY_ESCAPE)) activeDetailProperty = -1;
    }
}

void LevelEditor::UpdateDocking() {
    const Vector2 mouse = GetMousePosition();
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float panelBottom = static_cast<float>(GetScreenHeight()) - StatusBarHeight;
    constexpr float ResizeMargin = 6.0f;

    PanelId hoveredFloatingPanel = PanelId::Count;
    auto floatingResizeTarget = [&](PanelId panel) {
        const Rectangle bounds = GetPanelBounds(panel);
        if (dockPanels[static_cast<size_t>(panel)].slot != DockSlot::Floating ||
            mouse.x < bounds.x - ResizeMargin || mouse.x > bounds.x + bounds.width + ResizeMargin ||
            mouse.y < bounds.y - ResizeMargin || mouse.y > bounds.y + bounds.height + ResizeMargin) {
            return ResizeTarget::None;
        }
        const bool left = fabsf(mouse.x - bounds.x) <= ResizeMargin;
        const bool right = fabsf(mouse.x - (bounds.x + bounds.width)) <= ResizeMargin;
        const bool top = fabsf(mouse.y - bounds.y) <= ResizeMargin;
        const bool bottom = fabsf(mouse.y - (bounds.y + bounds.height)) <= ResizeMargin;
        if (top && left) return ResizeTarget::FloatingTopLeft;
        if (top && right) return ResizeTarget::FloatingTopRight;
        if (bottom && left) return ResizeTarget::FloatingBottomLeft;
        if (bottom && right) return ResizeTarget::FloatingBottomRight;
        if (left) return ResizeTarget::FloatingLeft;
        if (right) return ResizeTarget::FloatingRight;
        if (top) return ResizeTarget::FloatingTop;
        if (bottom) return ResizeTarget::FloatingBottom;
        return ResizeTarget::None;
    };

    ResizeTarget hoveredResize = ResizeTarget::None;
    for (int index = static_cast<int>(PanelId::Count) - 1; index >= 0; --index) {
        const PanelId panel = static_cast<PanelId>(index);
        const ResizeTarget target = floatingResizeTarget(panel);
        if (target != ResizeTarget::None) {
            hoveredResize = target;
            hoveredFloatingPanel = panel;
            break;
        }
    }

    if (hoveredResize == ResizeTarget::None) {
        const PanelId leftPanel = PanelInSlot(DockSlot::Left);
        const PanelId bottomPanel = PanelInSlot(DockSlot::Bottom);
        const bool hasRight = PanelInSlot(DockSlot::RightTop) != PanelId::Count ||
            PanelInSlot(DockSlot::RightBottom) != PanelId::Count;
        if (leftPanel != PanelId::Count && mouse.y >= ToolbarHeight && mouse.y <= panelBottom &&
            fabsf(mouse.x - leftDockWidth) <= ResizeMargin) hoveredResize = ResizeTarget::Left;
        else if (hasRight && mouse.y >= ToolbarHeight && mouse.y <= panelBottom &&
            fabsf(mouse.x - (screenWidth - rightDockWidth)) <= ResizeMargin) hoveredResize = ResizeTarget::Right;
        else if (bottomPanel != PanelId::Count && mouse.x >= 0.0f &&
            mouse.x <= GetPanelBounds(bottomPanel).width &&
            fabsf(mouse.y - (panelBottom - bottomDockHeight)) <= ResizeMargin)
            hoveredResize = ResizeTarget::Bottom;
        else if (PanelInSlot(DockSlot::RightTop) != PanelId::Count &&
            PanelInSlot(DockSlot::RightBottom) != PanelId::Count && mouse.x >= screenWidth - rightDockWidth &&
            fabsf(mouse.y - (ToolbarHeight + (panelBottom - ToolbarHeight) * rightDockSplit)) <= ResizeMargin)
            hoveredResize = ResizeTarget::RightSplit;
    }

    const ResizeTarget cursorTarget = resizeTarget != ResizeTarget::None ? resizeTarget : hoveredResize;
    switch (cursorTarget) {
    case ResizeTarget::Left:
    case ResizeTarget::Right:
    case ResizeTarget::FloatingLeft:
    case ResizeTarget::FloatingRight:
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        break;
    case ResizeTarget::Bottom:
    case ResizeTarget::RightSplit:
    case ResizeTarget::FloatingTop:
    case ResizeTarget::FloatingBottom:
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
        break;
    case ResizeTarget::FloatingTopLeft:
    case ResizeTarget::FloatingBottomRight:
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
        break;
    case ResizeTarget::FloatingTopRight:
    case ResizeTarget::FloatingBottomLeft:
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NESW);
        break;
    case ResizeTarget::None:
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        break;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        resizeTarget = hoveredResize;
        resizedFloatingPanel = hoveredFloatingPanel;
        if (resizeTarget != ResizeTarget::None) {
            resizeDragStart = mouse;
            if (resizedFloatingPanel != PanelId::Count) {
                resizeOriginalBounds = GetPanelBounds(resizedFloatingPanel);
            }
        }

        if (resizeTarget == ResizeTarget::None) {
            for (int index = static_cast<int>(PanelId::Count) - 1; index >= 0; --index) {
                const PanelId panel = static_cast<PanelId>(index);
                const Rectangle bounds = GetPanelBounds(panel);
                if (CheckCollisionPointRec(mouse, GetPanelHeaderBounds(panel))) {
                    draggedPanel = panel;
                    dragOrigin = dockPanels[static_cast<size_t>(panel)].slot;
                    dragOffset = {mouse.x - bounds.x, mouse.y - bounds.y};
                    dockPanels[static_cast<size_t>(panel)].floatingBounds = bounds;
                    dockPanels[static_cast<size_t>(panel)].slot = DockSlot::Floating;
                    break;
                }
            }
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (resizeTarget == ResizeTarget::Left) leftDockWidth = std::clamp(mouse.x, 190.0f, screenWidth * 0.42f);
        else if (resizeTarget == ResizeTarget::Right) rightDockWidth = std::clamp(screenWidth - mouse.x, 240.0f, screenWidth * 0.45f);
        else if (resizeTarget == ResizeTarget::Bottom) bottomDockHeight = std::clamp(panelBottom - mouse.y, 150.0f, panelBottom * 0.55f);
        else if (resizeTarget == ResizeTarget::RightSplit)
            rightDockSplit = std::clamp((mouse.y - ToolbarHeight) / (panelBottom - ToolbarHeight), 0.25f, 0.75f);
        else if (resizedFloatingPanel != PanelId::Count) {
            Rectangle& bounds = dockPanels[static_cast<size_t>(resizedFloatingPanel)].floatingBounds;
            const Vector2 delta{mouse.x - resizeDragStart.x, mouse.y - resizeDragStart.y};
            const bool adjustLeft = resizeTarget == ResizeTarget::FloatingLeft ||
                resizeTarget == ResizeTarget::FloatingTopLeft || resizeTarget == ResizeTarget::FloatingBottomLeft;
            const bool adjustRight = resizeTarget == ResizeTarget::FloatingRight ||
                resizeTarget == ResizeTarget::FloatingTopRight || resizeTarget == ResizeTarget::FloatingBottomRight;
            const bool adjustTop = resizeTarget == ResizeTarget::FloatingTop ||
                resizeTarget == ResizeTarget::FloatingTopLeft || resizeTarget == ResizeTarget::FloatingTopRight;
            const bool adjustBottom = resizeTarget == ResizeTarget::FloatingBottom ||
                resizeTarget == ResizeTarget::FloatingBottomLeft || resizeTarget == ResizeTarget::FloatingBottomRight;
            if (adjustLeft) {
                const float right = resizeOriginalBounds.x + resizeOriginalBounds.width;
                bounds.x = std::clamp(resizeOriginalBounds.x + delta.x, 0.0f, right - 220.0f);
                bounds.width = right - bounds.x;
            }
            if (adjustRight) {
                bounds.width = std::clamp(resizeOriginalBounds.width + delta.x, 220.0f,
                    screenWidth - resizeOriginalBounds.x);
            }
            if (adjustTop) {
                const float bottom = resizeOriginalBounds.y + resizeOriginalBounds.height;
                bounds.y = std::clamp(resizeOriginalBounds.y + delta.y, ToolbarHeight, bottom - 160.0f);
                bounds.height = bottom - bounds.y;
            }
            if (adjustBottom) {
                bounds.height = std::clamp(resizeOriginalBounds.height + delta.y, 160.0f,
                    panelBottom - resizeOriginalBounds.y);
            }
        }
        else if (draggedPanel != PanelId::Count) {
            Rectangle& bounds = dockPanels[static_cast<size_t>(draggedPanel)].floatingBounds;
            bounds.x = std::clamp(mouse.x - dragOffset.x, 0.0f, screenWidth - bounds.width);
            bounds.y = std::clamp(mouse.y - dragOffset.y, ToolbarHeight, panelBottom - bounds.height);
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (draggedPanel != PanelId::Count) {
            DockSlot target = DockSlot::Floating;
            if (mouse.x < 120.0f) target = DockSlot::Left;
            else if (mouse.x > screenWidth - 120.0f)
                target = mouse.y < ToolbarHeight + (panelBottom - ToolbarHeight) * 0.5f ?
                    DockSlot::RightTop : DockSlot::RightBottom;
            else if (mouse.y > panelBottom - 120.0f) target = DockSlot::Bottom;
            if (target != DockSlot::Floating) DockPanelAt(draggedPanel, target);
        }
        draggedPanel = PanelId::Count;
        resizedFloatingPanel = PanelId::Count;
        resizeTarget = ResizeTarget::None;
    }
}

void LevelEditor::Update() {
    if (viewportOnly) ProcessViewportHostCommand();
    if (!viewportOnly) {
        ProcessNativeMenuCommand();
        UpdateDocking();
        UpdateEditorPanels();
    }
    const Rectangle canvas = GetCanvasBounds();
    camera.offset = {canvas.x + canvas.width * 0.5f, canvas.y + canvas.height * 0.5f};
    const Vector2 mouse = GetMousePosition();
    if (activeDetailProperty >= 0) return;
    if (showConnections && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) &&
        CheckCollisionPointRec(mouse, canvas)) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) PickConnectionEndpoint(GetScreenToWorld2D(mouse, camera));
        return;
    }

    const bool controlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
#if defined(_WIN32)
    if (viewportOnly && controlDown && IsKeyPressed(KEY_Z)) {
        PendingViewportValues[15] = 1;
        PendingViewportCommands[15] = true;
    }
    else if (viewportOnly && controlDown && IsKeyPressed(KEY_Y)) {
        PendingViewportValues[16] = 1;
        PendingViewportCommands[16] = true;
    }
    if (viewportOnly && (tilePaintTool == TilePaintTool::TileSelect || tilePaintTool == TilePaintTool::MagicWand ||
        tilePaintTool == TilePaintTool::SelectMatching)) {
        if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
            PendingViewportValues[6] = 1;
            PendingViewportCommands[6] = true;
        }
        else if (controlDown && IsKeyPressed(KEY_C)) {
            PendingViewportValues[6] = 2;
            PendingViewportCommands[6] = true;
        }
        else if (controlDown && IsKeyPressed(KEY_V)) {
            PendingViewportValues[6] = 3;
            PendingViewportCommands[6] = true;
        }
        else if (controlDown && IsKeyPressed(KEY_D)) {
            PendingViewportValues[6] = 8;
            PendingViewportCommands[6] = true;
        }
        else if (!controlDown && IsKeyPressed(KEY_LEFT)) {
            PendingViewportValues[6] = 9;
            PendingViewportCommands[6] = true;
        }
        else if (!controlDown && IsKeyPressed(KEY_RIGHT)) {
            PendingViewportValues[6] = 10;
            PendingViewportCommands[6] = true;
        }
        else if (!controlDown && IsKeyPressed(KEY_UP)) {
            PendingViewportValues[6] = 11;
            PendingViewportCommands[6] = true;
        }
        else if (!controlDown && IsKeyPressed(KEY_DOWN)) {
            PendingViewportValues[6] = 12;
            PendingViewportCommands[6] = true;
        }
    }
#endif
    if (IsKeyPressed(KEY_F) || (!viewportOnly && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, GetFitButtonBounds()))) {
        FrameSelection();
    }
    if (controlDown && IsKeyPressed(KEY_O)) {
        if (viewportOnly) NotifyTileOperation("historyopen");
        else OpenLevelDialog();
    }
    if (controlDown && IsKeyPressed(KEY_S)) {
        if (viewportOnly) NotifyTileOperation(shiftDown ? "historysaveas" : "historysave");
        else if (shiftDown) SaveLevelAs();
        else SaveLevel();
    }
    if (controlDown && !currentLevelPath.empty() && IsKeyPressed(KEY_R)) {
        if (viewportOnly) NotifyTileOperation("historyreload");
        else if (ConfirmDiscardChanges()) {
            LoadLevel(currentLevelPath);
        }
    }
    const TransformMode transformModes[] = {
        TransformMode::Select, TransformMode::Move, TransformMode::Rotate, TransformMode::Scale
    };
    const char* transformNames[] = {"Select", "Move", "Rotate", "Scale"};
    const KeyboardKey transformKeys[] = {KEY_Q, KEY_W, KEY_E, KEY_R};
    for (int index = 0; index < 4; ++index) {
        if ((!controlDown && IsKeyPressed(transformKeys[index])) || (!viewportOnly && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, GetTransformButtonBounds(index)))) {
            activeTool = EditTool::Select;
            transformMode = transformModes[index];
            draggingCreate = false;
            draggingTransform = false;
            statusText = std::string("Transform: ") + transformNames[index];
        }
    }

    const EditTool tools[] = {
        EditTool::Select, EditTool::Solid, EditTool::Platform, EditTool::Ladder,
        EditTool::CameraZone, EditTool::Darkness, EditTool::PlayerStart, EditTool::Exit
    };
    for (int index = 0; index < static_cast<int>(std::size(tools)); ++index) {
        if (IsKeyPressed(KEY_ONE + index)) {
            activeTool = tools[index];
            if (activeTool == EditTool::Select) transformMode = TransformMode::Select;
            draggingCreate = false;
            statusText = std::string("Tool: ") + ToolName(activeTool);
        }
    }

    const float wheel = GetMouseWheelMove();
    const Rectangle libraryBounds = GetPanelBounds(PanelId::PlaceAssets);
    const int visibleCatalogRows = std::max(1, static_cast<int>((libraryBounds.height - 78.0f) / 30.0f));
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, libraryBounds)) {
        assetLibraryScroll = std::clamp(assetLibraryScroll - static_cast<int>(wheel), 0,
            std::max(0, static_cast<int>(AssetCatalog().size()) - visibleCatalogRows));
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const std::vector<int> contentAssets = ContentAssetIndices(contentCategory);
        for (int visibleIndex = 0; visibleIndex < static_cast<int>(contentAssets.size()); ++visibleIndex) {
            const int assetIndex = contentAssets[static_cast<size_t>(visibleIndex)];
            if (CheckCollisionPointRec(mouse, GetContentAssetBounds(visibleIndex))) {
                activeTool = Assets[assetIndex].tool;
                draggingCreate = false;
                statusText = std::string("Asset: ") + Assets[assetIndex].name;
                break;
            }
        }
        const int row = static_cast<int>((mouse.y - libraryBounds.y - 70.0f) / 30.0f);
        const int catalogIndex = assetLibraryScroll + row;
        if (CheckCollisionPointRec(mouse, libraryBounds) && mouse.y >= libraryBounds.y + 70.0f &&
            row >= 0 && row < visibleCatalogRows &&
            catalogIndex >= 0 && catalogIndex < static_cast<int>(AssetCatalog().size())) {
            const CatalogEntry& entry = AssetCatalog()[static_cast<size_t>(catalogIndex)];
            if (entry.placeable) {
                activeTool = entry.tool;
                draggingCreate = false;
                statusText = "Asset: " + entry.name;
            }
            else {
                statusText = entry.name + " is available in the catalog; placement editing is not implemented yet.";
            }
        }
    }

    if (CheckCollisionPointRec(mouse, canvas) && !IsMouseOverPanel(mouse)) {
        if (wheel != 0.0f) {
            const Vector2 worldBeforeZoom = GetScreenToWorld2D(mouse, camera);
            camera.zoom = std::clamp(camera.zoom * (1.0f + wheel * 0.12f), 0.05f, 8.0f);
            NotifyTileOperation("zoom " + std::to_string(static_cast<int>(std::round(camera.zoom * 100.0f))));
            const Vector2 worldAfterZoom = GetScreenToWorld2D(mouse, camera);
            camera.target.x += worldBeforeZoom.x - worldAfterZoom.x;
            camera.target.y += worldBeforeZoom.y - worldAfterZoom.y;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 delta = GetMouseDelta();
            camera.target.x -= delta.x / camera.zoom;
            camera.target.y -= delta.y / camera.zoom;
        }
    }

    if (viewportOnly && tilePaintTool != TilePaintTool::None) UpdateTilePainting(canvas);
    else UpdateEditing(canvas);

    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        for (unsigned int index = 0; index < droppedFiles.count; index++) {
            const std::filesystem::path droppedPath = droppedFiles.paths[index];
            if (droppedPath.extension() == ".level" && ConfirmDiscardChanges() && LoadLevel(droppedPath)) {
                break;
            }
        }
        UnloadDroppedFiles(droppedFiles);
    }
}

void LevelEditor::ProcessViewportHostCommand() {
#if defined(_WIN32)
    if (!PendingDocumentReload.empty()) {
        const Camera2D savedCamera = camera;
        LoadLevel(std::filesystem::u8path(std::exchange(PendingDocumentReload, {})));
        camera = savedCamera;
        hostEditTransaction = false;
        draggingTransform = false;
        tileDragActive = false;
        movingTileSelection = false;
    }
    for (int command = 1; command < static_cast<int>(PendingViewportCommands.size()); ++command) {
        if (!std::exchange(PendingViewportCommands[static_cast<size_t>(command)], false)) continue;
        const int value = PendingViewportValues[static_cast<size_t>(command)];
        switch (command) {
        case 1:
            tilePaintTool = value >= 1 && value <= 18 ? static_cast<TilePaintTool>(value) : TilePaintTool::None;
            tileDragActive = false;
            draggingCreate = false;
            draggingTransform = false;
            break;
        case 2:
            paintTileColumn = value & 0xffff;
            paintTileRow = (value >> 16) & 0xffff;
            break;
        case 3:
            paintTileSheet = std::clamp(value, 0, 2);
            break;
        case 4:
            paintBrushSize = std::clamp(value, 1, 9);
            break;
        case 5: {
            const short x = static_cast<short>(value & 0xffff);
            const short y = static_cast<short>((value >> 16) & 0xffff);
            tilePaintTool = TilePaintTool::None;
            activeTool = EditTool::Select;
            transformMode = TransformMode::Select;
            selection = HitTest({static_cast<float>(x), static_cast<float>(y)});
            groupSelection.clear();
            if (selection.kind != SelectionKind::None) groupSelection.push_back(selection);
            break;
        }
        case 6: {
            if (!tileSelectionActive) break;
            const int activeLayerBit = TileLayerBit(paintTileLayer);
            if (value != 2 && (lockedTileLayerMask & activeLayerBit) != 0) {
                NotifyTileOperation("toolstatus Active tile layer is locked");
                break;
            }
            if (value == 1 || (value == 3 && !tileClipboard.empty()) || (value >= 4 && value <= 12)) PushTileUndo();
            if (value == 2) {
                tileClipboard.clear();
                for (const VisualTile& tile : level.visualTiles) {
                    const int x = static_cast<int>(tile.position.x) / 32;
                    const int y = static_cast<int>(tile.position.y) / 32;
                    if (tile.layer == paintTileLayer && IsTileCellSelected(x, y)) {
                        VisualTile copy = tile;
                        copy.position.x -= static_cast<float>(tileSelectionLeft * 32);
                        copy.position.y -= static_cast<float>(tileSelectionTop * 32);
                        tileClipboard.push_back(copy);
                    }
                }
            }
            else if (value == 1) {
                for (auto iterator = level.visualTiles.begin(); iterator != level.visualTiles.end();) {
                    const int x = static_cast<int>(iterator->position.x) / 32;
                    const int y = static_cast<int>(iterator->position.y) / 32;
                    if (iterator->layer == paintTileLayer && IsTileCellSelected(x, y)) {
                        NotifyTileErased(iterator->layer, iterator->position);
                        iterator = level.visualTiles.erase(iterator);
                    }
                    else ++iterator;
                }
                tileSelectionActive = false;
                tileSelectionCells.clear();
                dirty = true;
            }
            else if (value == 3 && !tileClipboard.empty()) {
                const Vector2 world = GetScreenToWorld2D(GetMousePosition(), camera);
                const int originX = static_cast<int>(floorf(world.x / 32.0f));
                const int originY = static_cast<int>(floorf(world.y / 32.0f));
                const int selectionWidth = tileSelectionRight - tileSelectionLeft;
                const int selectionHeight = tileSelectionBottom - tileSelectionTop;
                tileSelectionCells.clear();
                for (VisualTile tile : tileClipboard) {
                    tile.position.x += static_cast<float>(originX * 32);
                    tile.position.y += static_cast<float>(originY * 32);
                    auto existing = std::find_if(level.visualTiles.begin(), level.visualTiles.end(), [&](const VisualTile& item) {
                        return item.layer == tile.layer && item.position.x == tile.position.x && item.position.y == tile.position.y;
                    });
                    if (existing == level.visualTiles.end()) level.visualTiles.push_back(tile); else *existing = tile;
                    NotifyTilePainted(tile);
                    tileSelectionCells.insert(TileCellKey(static_cast<int>(tile.position.x) / 32,
                        static_cast<int>(tile.position.y) / 32));
                }
                tileSelectionLeft = originX;
                tileSelectionTop = originY;
                tileSelectionRight = originX + selectionWidth;
                tileSelectionBottom = originY + selectionHeight;
                tileSelectionActive = true;
                UpdateTileSelectionBounds();
                dirty = true;
            }
            else if (value >= 4 && value <= 12) {
                const bool duplicate = value == 8;
                std::vector<VisualTile> transformed;
                for (auto iterator = level.visualTiles.begin(); iterator != level.visualTiles.end();) {
                    const int x = static_cast<int>(iterator->position.x) / 32;
                    const int y = static_cast<int>(iterator->position.y) / 32;
                    if (iterator->layer != paintTileLayer || !IsTileCellSelected(x, y)) { ++iterator; continue; }
                    transformed.push_back(*iterator);
                    if (duplicate) ++iterator;
                    else {
                        NotifyTileErased(iterator->layer, iterator->position);
                        iterator = level.visualTiles.erase(iterator);
                    }
                }
                tileSelectionCells.clear();
                for (VisualTile& tile : transformed) {
                    const int x = static_cast<int>(tile.position.x) / 32;
                    const int y = static_cast<int>(tile.position.y) / 32;
                    int targetX = x;
                    int targetY = y;
                    if (value == 4) {
                        targetX = tileSelectionLeft + (y - tileSelectionTop);
                        targetY = tileSelectionTop + (tileSelectionRight - x);
                        tile.quarterTurns = (tile.quarterTurns + 3) % 4;
                    }
                    else if (value == 5) {
                        targetX = tileSelectionLeft + (tileSelectionBottom - y);
                        targetY = tileSelectionTop + (x - tileSelectionLeft);
                        tile.quarterTurns = (tile.quarterTurns + 1) % 4;
                    }
                    else if (value == 6) { targetX = tileSelectionLeft + tileSelectionRight - x; tile.flipX = !tile.flipX; }
                    else if (value == 7) { targetY = tileSelectionTop + tileSelectionBottom - y; tile.flipY = !tile.flipY; }
                    else if (value == 8 || value == 10) ++targetX;
                    else if (value == 9) --targetX;
                    else if (value == 11) --targetY;
                    else if (value == 12) ++targetY;
                    tile.position = {static_cast<float>(targetX * 32), static_cast<float>(targetY * 32)};
                    auto destination = std::find_if(level.visualTiles.begin(), level.visualTiles.end(), [&](const VisualTile& item) {
                        return item.layer == tile.layer && item.position.x == tile.position.x && item.position.y == tile.position.y;
                    });
                    if (destination != level.visualTiles.end()) level.visualTiles.erase(destination);
                    level.visualTiles.push_back(tile);
                    NotifyTilePainted(tile);
                    tileSelectionCells.insert(TileCellKey(targetX, targetY));
                }
                UpdateTileSelectionBounds();
                NotifyTileSelection();
                dirty |= !transformed.empty();
            }
            break;
        }
        case 7:
            visibleTileLayerMask = value & 0xFFF;
            break;
        case 8:
            lockedTileLayerMask = value & 0xFFF;
            break;
        case 28:
            paintTileSelectionWidth = std::clamp(value & 0xffff, 1, 64);
            paintTileSelectionHeight = std::clamp((value >> 16) & 0xffff, 1, 64);
            break;
        case 29:
            paintTileLayer = TileLayerFromId(value);
            break;
        case 30: {
            const TileLayer removedLayer = TileLayerFromId(value);
            if (static_cast<int>(removedLayer) < static_cast<int>(TileLayer::User1)) break;
            PushTileUndo();
            for (auto iterator = level.visualTiles.begin(); iterator != level.visualTiles.end();) {
                if (iterator->layer == removedLayer) iterator = level.visualTiles.erase(iterator);
                else ++iterator;
            }
            if (paintTileLayer == removedLayer) paintTileLayer = TileLayer::Foreground;
            tileSelectionActive = false;
            tileSelectionCells.clear();
            dirty = true;
            NotifyFullTileState();
            break;
        }
        case 9:
            showGrid = value != 0;
            break;
        case 10:
            tileSymmetryMask = value & 3;
            break;
        case 11: {
            const int activeLayerBit = TileLayerBit(paintTileLayer);
            if ((lockedTileLayerMask & activeLayerBit) != 0) {
                NotifyTileOperation("toolstatus Active tile layer is locked");
                break;
            }
            PushTileUndo();
            for (auto iterator = level.visualTiles.begin(); iterator != level.visualTiles.end();) {
                if (iterator->layer == paintTileLayer) {
                    NotifyTileErased(iterator->layer, iterator->position);
                    iterator = level.visualTiles.erase(iterator);
                }
                else ++iterator;
            }
            tileSelectionActive = false;
            tileSelectionCells.clear();
            dirty = true;
            NotifyTileOperation("toolstatus Active tile layer cleared");
            break;
        }
        case 12:
            paintAnimationFrames = std::clamp(value, 1, 64);
            break;
        case 13:
            paintAnimationFrameSeconds = std::clamp(value / 1000.0f, 0.01f, 10.0f);
            break;
        case 14:
            previewTileAnimations = value != 0;
            break;
        case 15:
            UndoTileEdit();
            break;
        case 16:
            RedoTileEdit();
            break;
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23: {
            if (!tileSelectionActive) break;
            const int activeLayerBit = TileLayerBit(paintTileLayer);
            if ((lockedTileLayerMask & activeLayerBit) != 0) {
                NotifyTileOperation("toolstatus Active tile layer is locked");
                break;
            }
            PushTileUndo();
            const Texture2D activeSheet = paintTileSheet == 2 ? industrialFarBackground :
                (paintTileSheet == 1 ? industrialBackground : industrialTiles);
            for (VisualTile& tile : level.visualTiles) {
                const int x = static_cast<int>(tile.position.x) / 32;
                const int y = static_cast<int>(tile.position.y) / 32;
                if (tile.layer != paintTileLayer || !IsTileCellSelected(x, y)) continue;
                if (command == 17) tile.column = std::clamp(value, 0, std::max(0, activeSheet.width / 32 - 1));
                else if (command == 18) tile.row = std::clamp(value, 0, std::max(0, activeSheet.height / 32 - 1));
                else if (command == 19) tile.quarterTurns = ((value % 4) + 4) % 4;
                else if (command == 20) tile.flipX = value != 0;
                else if (command == 21) tile.flipY = value != 0;
                else if (command == 22) tile.animationFrames = std::clamp(value, 1, 64);
                else tile.animationFrameSeconds = std::clamp(value / 1000.0f, 0.01f, 10.0f);
                NotifyTilePainted(tile);
            }
            dirty = true;
            NotifyTileSelection();
            break;
        }
        case 24:
            if (value == 8 || value == 16 || value == 32 || value == 64) gridSnap = static_cast<float>(value);
            break;
        case 25:
            FrameSelection();
            break;
        case 26:
            DuplicateSelection();
            break;
        case 27:
            if (value == 0) activeTool = EditTool::Solid;
            else if (value == 1) activeTool = EditTool::Platform;
            else if (value == 2) activeTool = EditTool::Ladder;
            else if (value == 3) activeTool = EditTool::CameraZone;
            else if (value == 4) activeTool = EditTool::Darkness;
            else if (value == 5) activeTool = EditTool::PlayerStart;
            else if (value == 6) activeTool = EditTool::Exit;
            else if (value >= 7 && value <= 10) {
                const EditTool fluidTools[] = {EditTool::Water, EditTool::Sand, EditTool::Gel, EditTool::Gas};
                activeTool = fluidTools[value - 7];
            }
            else if (value == 11) activeTool = EditTool::Pulley;
            else if (value == 12) activeTool = EditTool::HangingWeight;
            else if (value == 13) activeTool = EditTool::RotaryLatch;
            else if (value == 14) activeTool = EditTool::StoneBlock;
            else if (value == 15) activeTool = EditTool::Boulder;
            else if (value == 16) activeTool = EditTool::PhysicsWheel;
            else if (value == 17) activeTool = EditTool::Gear;
            else if (value == 18) activeTool = EditTool::Flywheel;
            else if (value == 19) activeTool = EditTool::SteeringWheel;
            else if (value == 20) activeTool = EditTool::Screw;
            else if (value == 21) activeTool = EditTool::Fan;
            else if (value == 22) activeTool = EditTool::Pinwheel;
            else if (value == 23) activeTool = EditTool::Ramp;
            else if (value == 24) activeTool = EditTool::SeeSaw;
            else if (value == 25) activeTool = EditTool::TrapDoor;
            else if (value == 26) activeTool = EditTool::Chain;
            else if (value == 27) activeTool = EditTool::PhysicsRope;
            else if (value == 28) activeTool = EditTool::Button;
            else if (value == 29) activeTool = EditTool::Portal;
            else if (value == 30) activeTool = EditTool::DirectionalSpikes;
            else if (value == 31) activeTool = EditTool::ArrowTrap;
            else if (value == 32) activeTool = EditTool::BreakableTile;
            else if (value == 33) activeTool = EditTool::Enemy;
            else if (value == 35) activeTool = EditTool::Label;
            else if (value == 36) activeTool = EditTool::Valve;
            else if (value == 37) activeTool = EditTool::Checkpoint;
            else if (value == 38) activeTool = EditTool::Collectible;
            else if (value == 39) activeTool = EditTool::Ball;
            else if (value == 40) activeTool = EditTool::Barrel;
            else if (value == 41) activeTool = EditTool::MovingPlatform;
            else if (value == 42) activeTool = EditTool::Elevator;
            else if (value == 43) activeTool = EditTool::PendulumBob;
            else if (value == 44) activeTool = EditTool::OneWayPlatform;
            else if (value == 45) activeTool = EditTool::CeilingHook;
            else if (value == 46) activeTool = EditTool::GuideRail;
            else if (value == 47) activeTool = EditTool::Spring;
            else if (value == 48) activeTool = EditTool::CompressionSpring;
            else if (value == 49) activeTool = EditTool::ExtensionSpring;
            else if (value == 50) activeTool = EditTool::TorsionSpring;
            else if (value == 51) activeTool = EditTool::GarterSpring;
            else if (value == 52) activeTool = EditTool::VoluteSpring;
            else if (value == 53) activeTool = EditTool::SpiralSpring;
            else if (value == 54) activeTool = EditTool::ConstantForceSpring;
            else if (value == 55) activeTool = EditTool::ConstantTorqueSpring;
            else if (value == 56) activeTool = EditTool::LeafSpring;
            else if (value == 57) activeTool = EditTool::BeamSpring;
            else if (value == 58) activeTool = EditTool::DiscSpring;
            else if (value == 59) activeTool = EditTool::WaveSpring;
            else if (value == 60) activeTool = EditTool::WaveWasher;
            else if (value == 61) activeTool = EditTool::TorsionBar;
            else if (value == 62) activeTool = EditTool::RingSpring;
            else if (value == 63) activeTool = EditTool::ElastomerSpring;
            else if (value == 64) activeTool = EditTool::PneumaticSpring;
            else if (value == 65) activeTool = EditTool::GasSpring;
            else if (value == 66) activeTool = EditTool::HydropneumaticSpring;
            else if (value == 67) activeTool = EditTool::MagneticSpring;
            else if (value == 68) activeTool = EditTool::CompositeSpring;
            else if (value == 69) activeTool = EditTool::Rod;
            else if (value == 70) activeTool = EditTool::FixedJoint;
            else if (value == 71) activeTool = EditTool::Crank;
            else if (value == 72) activeTool = EditTool::Ratchet;
            else if (value == 73) activeTool = EditTool::Clutch;
            else if (value == 74) activeTool = EditTool::Brake;
            else break;
            {
                tilePaintTool = TilePaintTool::None;
                draggingCreate = false;
                statusText = std::string("Placement tool: ") + ToolName(activeTool);
            }
            break;
        case 31:
            if (value < 0 || value > 3) break;
            tilePaintTool = TilePaintTool::None;
            activeTool = EditTool::Select;
            transformMode = static_cast<TransformMode>(value);
            draggingCreate = false;
            draggingTransform = false;
            statusText = std::string("Transform: ") +
                (value == 0 ? "Select" : value == 1 ? "Move" : value == 2 ? "Rotate" : "Scale");
            break;
        case 32:
            camera.zoom = std::clamp(value / 100.0f, 0.05f, 8.0f);
            NotifyTileOperation("zoom " + std::to_string(static_cast<int>(std::round(camera.zoom * 100.0f))));
            break;
        case 33:
            NotifyTileOperation("zoom " + std::to_string(static_cast<int>(std::round(camera.zoom * 100.0f))));
            break;
        case 34: showConnections = value != 0; break;
        case 35: focusConnections = value != 0; break;
        case 36: SelectConnectedObjects(); break;
        default:
            break;
        }
    }
#endif
}

void LevelEditor::NotifyTilePainted(const VisualTile& tile) const {
#if defined(_WIN32)
    HWND window = static_cast<HWND>(GetWindowHandle());
    HWND parent = GetParent(window);
    if (parent == nullptr) return;
    const char* layer = TileLayerName(tile.layer);
    const std::string message = std::string("paint ") + layer + ' ' +
        std::to_string(tile.column) + ' ' + std::to_string(tile.row) + ' ' +
        std::to_string(static_cast<int>(tile.position.x)) + ' ' +
        std::to_string(static_cast<int>(tile.position.y)) + ' ' +
        std::to_string(tile.quarterTurns) + ' ' + std::to_string(tile.flipX ? 1 : 0) + ' ' +
        std::to_string(tile.flipY ? 1 : 0) + ' ' + std::to_string(tile.animationFrames) + ' ' +
        std::to_string(tile.animationFrameSeconds) + ' ' +
        std::to_string(tile.sheetIndex >= 0 ? tile.sheetIndex :
            (tile.layer == TileLayer::FarBackground ? 2 : tile.layer == TileLayer::Background ? 1 : 0));
    COPYDATASTRUCT data{};
    data.dwData = 1;
    data.cbData = static_cast<DWORD>(message.size() + 1);
    data.lpData = const_cast<char*>(message.c_str());
    SendMessageW(parent, WM_COPYDATA, reinterpret_cast<WPARAM>(window), reinterpret_cast<LPARAM>(&data));
#endif
}

void LevelEditor::NotifyTileErased(TileLayer layer, Vector2 position) const {
#if defined(_WIN32)
    HWND parent = GetParent(static_cast<HWND>(GetWindowHandle()));
    if (parent == nullptr) return;
    const char* layerName = TileLayerName(layer);
    const std::string message = std::string("erase ") + layerName + ' ' +
        std::to_string(static_cast<int>(position.x)) + ' ' + std::to_string(static_cast<int>(position.y));
    COPYDATASTRUCT data{};
    data.dwData = 1;
    data.cbData = static_cast<DWORD>(message.size() + 1);
    data.lpData = const_cast<char*>(message.c_str());
    SendMessageW(parent, WM_COPYDATA, reinterpret_cast<WPARAM>(GetWindowHandle()), reinterpret_cast<LPARAM>(&data));
#endif
}

void LevelEditor::NotifyTileOperation(const std::string& message) const {
#if defined(_WIN32)
    HWND window = static_cast<HWND>(GetWindowHandle());
    HWND parent = GetParent(window);
    if (parent == nullptr) return;
    COPYDATASTRUCT data{};
    data.dwData = 1;
    data.cbData = static_cast<DWORD>(message.size() + 1);
    data.lpData = const_cast<char*>(message.c_str());
    SendMessageW(parent, WM_COPYDATA, reinterpret_cast<WPARAM>(window), reinterpret_cast<LPARAM>(&data));
#endif
}

bool LevelEditor::IsTileCellSelected(int x, int y) const {
    return tileSelectionCells.find(TileCellKey(x, y)) != tileSelectionCells.end();
}

void LevelEditor::UpdateTileSelectionBounds() {
    if (tileSelectionCells.empty()) {
        tileSelectionActive = false;
        return;
    }
    tileSelectionLeft = INT_MAX;
    tileSelectionTop = INT_MAX;
    tileSelectionRight = INT_MIN;
    tileSelectionBottom = INT_MIN;
    for (const unsigned long long key : tileSelectionCells) {
        const int x = static_cast<int>(static_cast<int32_t>(key >> 32));
        const int y = static_cast<int>(static_cast<int32_t>(key));
        tileSelectionLeft = std::min(tileSelectionLeft, x);
        tileSelectionTop = std::min(tileSelectionTop, y);
        tileSelectionRight = std::max(tileSelectionRight, x);
        tileSelectionBottom = std::max(tileSelectionBottom, y);
    }
    tileSelectionActive = true;
}

void LevelEditor::NotifyTileSelection() const {
    if (!tileSelectionActive) {
        NotifyTileOperation("toolstatus No tiles selected");
        return;
    }
    NotifyTileOperation("tileselect " + std::to_string(tileSelectionLeft) + ' ' +
        std::to_string(tileSelectionTop) + ' ' + std::to_string(tileSelectionRight) + ' ' +
        std::to_string(tileSelectionBottom) + ' ' + std::to_string(tileSelectionCells.size()));
    for (const VisualTile& tile : level.visualTiles) {
        const int x = static_cast<int>(tile.position.x) / 32;
        const int y = static_cast<int>(tile.position.y) / 32;
        if (tile.layer != paintTileLayer || !IsTileCellSelected(x, y)) continue;
        const char* layer = TileLayerName(tile.layer);
        NotifyTileOperation(std::string("tileinspect ") + layer + ' ' + std::to_string(tile.column) + ' ' +
            std::to_string(tile.row) + ' ' + std::to_string(x * 32) + ' ' + std::to_string(y * 32) + ' ' +
            std::to_string(tile.quarterTurns) + ' ' + std::to_string(tile.flipX ? 1 : 0) + ' ' +
            std::to_string(tile.flipY ? 1 : 0) + ' ' + std::to_string(tile.animationFrames) + ' ' +
            std::to_string(tile.animationFrameSeconds) + ' ' + std::to_string(tileSelectionCells.size()));
        break;
    }
}

void LevelEditor::PushTileUndo() {
    if (viewportOnly) {
        if (!hostEditTransaction) {
            hostEditTransaction = true;
            NotifyTileOperation("historybegin");
        }
        return;
    }
    tileUndoHistory.push_back({level.visualTiles, level.baseSolids, level.pitPlatforms, level.ladders,
        level.cameraZones, level.darknessAreas, level.exitTrigger, level.playerStart, FluidDefinitions(level.fluids),
        level.pulleys, level.weights, level.rotaryLatches,
        level.stoneBlocks, level.boulders, level.physicsWheels, level.gears, level.flywheels,
        level.steeringWheels, level.screws, level.fans, level.pinwheels, level.ramps, level.seeSaws,
        level.trapDoors, ChainDefinitions(level.chains), RopeDefinitions(level.physicsRopes),
        level.buttons, level.portalPairs, level.directionalSpikeHazards, ArrowTrapDefinitions(level.arrowTraps),
        level.breakableTiles, level.enemies, level.labels, level.valve, level.guideObjects});
    if (tileUndoHistory.size() > 64) tileUndoHistory.erase(tileUndoHistory.begin());
    tileRedoHistory.clear();
}

void LevelEditor::RestoreTileEdit(const TileEditSnapshot& snapshot) {
    level.visualTiles = snapshot.visualTiles;
    level.baseSolids = snapshot.solids;
    level.pitPlatforms = snapshot.platforms;
    level.ladders = snapshot.ladders;
    level.cameraZones = snapshot.cameraZones;
    level.darknessAreas = snapshot.darknessAreas;
    level.exitTrigger = snapshot.exitTrigger;
    level.playerStart = snapshot.playerStart;
    level.fluids = snapshot.fluids;
    for (FluidField& fluid : level.fluids) InitializeFluidField(fluid, BuildSolids(level), FluidSimulationMode::Tile);
    level.pulleys = snapshot.pulleys;
    level.weights = snapshot.weights;
    level.rotaryLatches = snapshot.rotaryLatches;
    level.stoneBlocks = snapshot.stoneBlocks;
    level.boulders = snapshot.boulders;
    level.physicsWheels = snapshot.physicsWheels;
    level.gears = snapshot.gears;
    level.flywheels = snapshot.flywheels;
    level.steeringWheels = snapshot.steeringWheels;
    level.screws = snapshot.screws;
    level.fans = snapshot.fans;
    level.pinwheels = snapshot.pinwheels;
    level.ramps = snapshot.ramps;
    level.seeSaws = snapshot.seeSaws;
    level.trapDoors = snapshot.trapDoors;
    level.chains = snapshot.chains;
    for (Chain& chain : level.chains) InitializeChain(chain);
    level.physicsRopes = snapshot.physicsRopes;
    for (PhysicsRope& rope : level.physicsRopes) InitializePhysicsRope(rope);
    level.buttons = snapshot.buttons;
    level.portalPairs = snapshot.portalPairs;
    level.directionalSpikeHazards = snapshot.directionalSpikeHazards;
    level.arrowTraps = snapshot.arrowTraps;
    level.breakableTiles = snapshot.breakableTiles;
    level.enemies = snapshot.enemies;
    level.labels = snapshot.labels;
    level.valve = snapshot.valve;
    level.guideObjects = snapshot.guideObjects;
    selection = {};
    groupSelection.clear();
    tileSelectionActive = false;
    tileSelectionCells.clear();
    dirty = true;
    NotifyFullTileState();
    NotifyEditableObjectState();
}

void LevelEditor::UndoTileEdit() {
    if (viewportOnly) { NotifyTileOperation("historyundo"); return; }
    if (tileUndoHistory.empty()) {
        NotifyTileOperation("toolstatus Nothing to undo");
        return;
    }
    tileRedoHistory.push_back({level.visualTiles, level.baseSolids, level.pitPlatforms, level.ladders,
        level.cameraZones, level.darknessAreas, level.exitTrigger, level.playerStart, FluidDefinitions(level.fluids),
        level.pulleys, level.weights, level.rotaryLatches,
        level.stoneBlocks, level.boulders, level.physicsWheels, level.gears, level.flywheels,
        level.steeringWheels, level.screws, level.fans, level.pinwheels, level.ramps, level.seeSaws,
        level.trapDoors, ChainDefinitions(level.chains), RopeDefinitions(level.physicsRopes),
        level.buttons, level.portalPairs, level.directionalSpikeHazards, ArrowTrapDefinitions(level.arrowTraps),
        level.breakableTiles, level.enemies, level.labels, level.valve, level.guideObjects});
    const TileEditSnapshot snapshot = std::move(tileUndoHistory.back());
    tileUndoHistory.pop_back();
    RestoreTileEdit(snapshot);
    NotifyTileOperation("toolstatus Edit undone");
}

void LevelEditor::RedoTileEdit() {
    if (viewportOnly) { NotifyTileOperation("historyredo"); return; }
    if (tileRedoHistory.empty()) {
        NotifyTileOperation("toolstatus Nothing to redo");
        return;
    }
    tileUndoHistory.push_back({level.visualTiles, level.baseSolids, level.pitPlatforms, level.ladders,
        level.cameraZones, level.darknessAreas, level.exitTrigger, level.playerStart, FluidDefinitions(level.fluids),
        level.pulleys, level.weights, level.rotaryLatches,
        level.stoneBlocks, level.boulders, level.physicsWheels, level.gears, level.flywheels,
        level.steeringWheels, level.screws, level.fans, level.pinwheels, level.ramps, level.seeSaws,
        level.trapDoors, ChainDefinitions(level.chains), RopeDefinitions(level.physicsRopes),
        level.buttons, level.portalPairs, level.directionalSpikeHazards, ArrowTrapDefinitions(level.arrowTraps),
        level.breakableTiles, level.enemies, level.labels, level.valve, level.guideObjects});
    const TileEditSnapshot snapshot = std::move(tileRedoHistory.back());
    tileRedoHistory.pop_back();
    RestoreTileEdit(snapshot);
    NotifyTileOperation("toolstatus Edit redone");
}

void LevelEditor::NotifyFullTileState() const {
    NotifyTileOperation("syncbegin");
    for (const VisualTile& tile : level.visualTiles) NotifyTilePainted(tile);
    for (const Rectangle& solid : level.baseSolids) {
        NotifyTileOperation("solidstate " + std::to_string(solid.x) + ' ' + std::to_string(solid.y) + ' ' +
            std::to_string(solid.width) + ' ' + std::to_string(solid.height));
    }
    NotifyTileOperation("syncend");
}

void LevelEditor::NotifyEditableObjectState() const {
#if defined(_WIN32)
    const auto notifyRect = [this](const char* type, Rectangle rect) {
        NotifyTileOperation(std::string("objectstate ") + type + ' ' +
            std::to_string(rect.x) + ' ' + std::to_string(rect.y) + ' ' +
            std::to_string(rect.width) + ' ' + std::to_string(rect.height));
    };
    NotifyTileOperation("objectsyncbegin");
    NotifyTileOperation(std::string("objectstate playerStart ") +
        std::to_string(level.playerStart.x) + ' ' + std::to_string(level.playerStart.y) + " 0 0");
    if (IsUsableRectangle(level.exitTrigger)) notifyRect("exit", level.exitTrigger);
    for (Rectangle rect : level.baseSolids) notifyRect("solid", rect);
    for (Rectangle rect : level.pitPlatforms) notifyRect("platform", rect);
    for (Rectangle rect : level.ladders) notifyRect("ladder", rect);
    for (Rectangle rect : level.cameraZones) notifyRect("cameraZone", rect);
    for (Rectangle rect : level.darknessAreas) notifyRect("darkness", rect);
    for (const VisualTile& tile : level.visualTiles) {
        const char* layer = TileLayerName(tile.layer);
        NotifyTileOperation(std::string("actorstate visualTile ") + layer + ' ' +
            std::to_string(tile.column) + ' ' + std::to_string(tile.row) + ' ' +
            std::to_string(tile.position.x) + ' ' + std::to_string(tile.position.y) + ' ' +
            std::to_string(tile.quarterTurns) + ' ' + (tile.flipX ? "1" : "0") + ' ' +
            (tile.flipY ? "1" : "0") + ' ' + std::to_string(tile.animationFrames) + ' ' +
            std::to_string(tile.animationFrameSeconds));
    }
    if (IsUsableRectangle(level.waterPit.bounds)) {
        NotifyTileOperation(std::string("actorstate waterPit ") +
            std::to_string(level.waterPit.bounds.x) + ' ' + std::to_string(level.waterPit.bounds.y) + ' ' +
            std::to_string(level.waterPit.bounds.width) + ' ' + std::to_string(level.waterPit.bounds.height) + ' ' +
            std::to_string(level.waterPit.surfaceY) + ' ' + std::to_string(level.waterPit.targetSurfaceY) + ' ' +
            std::to_string(level.waterPit.fillRate));
    }
    if (IsUsableRectangle(level.spikeHazard)) {
        NotifyTileOperation(std::string("actorstate spikeHazard ") +
            std::to_string(level.spikeHazard.x) + ' ' + std::to_string(level.spikeHazard.y) + ' ' +
            std::to_string(level.spikeHazard.width) + ' ' + std::to_string(level.spikeHazard.height));
    }
    if (level.clockFaceRadius > 0.0f) {
        NotifyTileOperation(std::string("actorstate clockFace ") + std::to_string(level.clockFaceCenter.x) + ' ' +
            std::to_string(level.clockFaceCenter.y) + ' ' + std::to_string(level.clockFaceRadius));
    }
    for (const FluidField& fluid : level.fluids) {
        NotifyTileOperation(std::string("fluidstate ") + FluidName(fluid.type) + ' ' +
            std::to_string(fluid.bounds.x) + ' ' + std::to_string(fluid.bounds.y) + ' ' +
            std::to_string(fluid.bounds.width) + ' ' + std::to_string(fluid.bounds.height) + ' ' +
            std::to_string(fluid.particleSpacing) + ' ' + std::to_string(fluid.initialFill) + ' ' +
            std::to_string(fluid.flowSpeed));
    }
    for (const Vector2 pulley : level.pulleys) {
        NotifyTileOperation(std::string("actorstate pulley ") + std::to_string(pulley.x) + ' ' +
            std::to_string(pulley.y));
    }
    for (const HangingWeight& weight : level.weights) {
        int pulleyIndex = 0;
        float closestDistance = INFINITY;
        for (int index = 0; index < static_cast<int>(level.pulleys.size()); ++index) {
            const float dx = level.pulleys[static_cast<size_t>(index)].x - weight.pulley.x;
            const float dy = level.pulleys[static_cast<size_t>(index)].y - weight.pulley.y;
            const float distance = dx * dx + dy * dy;
            if (distance < closestDistance) {
                closestDistance = distance;
                pulleyIndex = index;
            }
        }
        NotifyTileOperation(std::string("actorstate weight ") + std::to_string(pulleyIndex) + ' ' +
            std::to_string(weight.pulleyRadius) + ' ' + std::to_string(weight.phase) + ' ' +
            std::to_string(weight.speed) + ' ' + std::to_string(weight.rect.width) + ' ' +
            std::to_string(weight.rect.height));
    }
    for (const RotaryLatch& latch : level.rotaryLatches) {
        NotifyTileOperation(std::string("actorstate rotaryLatch ") + std::to_string(latch.center.x) + ' ' +
            std::to_string(latch.center.y) + ' ' + std::to_string(latch.radius) + ' ' +
            std::to_string(latch.angle) + ' ' + std::to_string(latch.targetAngle) + ' ' +
            std::to_string(latch.tolerance) + ' ' + std::to_string(latch.spinSpeed));
    }
    for (const StoneBlock& block : level.stoneBlocks) {
        NotifyTileOperation(std::string("actorstate stoneBlock ") + std::to_string(block.rect.x) + ' ' +
            std::to_string(block.rect.y) + ' ' + std::to_string(block.rect.width) + ' ' +
            std::to_string(block.rect.height) + ' ' + std::to_string(block.mass));
    }
    for (const Boulder& boulder : level.boulders) {
        NotifyTileOperation(std::string("actorstate boulder ") + std::to_string(boulder.center.x) + ' ' +
            std::to_string(boulder.center.y) + ' ' + std::to_string(boulder.radius) + ' ' +
            std::to_string(boulder.mass) + ' ' + std::to_string(boulder.rotation));
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        NotifyTileOperation(std::string("actorstate physicsWheel ") + std::to_string(wheel.center.x) + ' ' +
            std::to_string(wheel.center.y) + ' ' + std::to_string(wheel.radius) + ' ' +
            std::to_string(wheel.mass) + ' ' + std::to_string(wheel.rotation));
    }
    for (const Gear& gear : level.gears) {
        NotifyTileOperation(std::string("actorstate gear ") + GearVisualName(gear.visualType) + ' ' +
            (gear.mounting == GearMounting::Mounted ? "mounted" : "dynamic") + ' ' +
            (gear.orientation == GearOrientation::Horizontal ? "horizontal" : "vertical") + ' ' +
            std::to_string(gear.center.x) + ' ' + std::to_string(gear.center.y) + ' ' +
            std::to_string(gear.radius) + ' ' + std::to_string(gear.mass) + ' ' +
            std::to_string(gear.toothCount) + ' ' + std::to_string(gear.rotation) + ' ' +
            std::to_string(gear.angularVelocity) + ' ' + std::to_string(gear.driveSpeed) + ' ' +
            ClockHandName(gear.clockHand));
    }
    for (const Flywheel& flywheel : level.flywheels) {
        NotifyTileOperation(std::string("actorstate flywheel ") + std::to_string(flywheel.center.x) + ' ' +
            std::to_string(flywheel.center.y) + ' ' + std::to_string(flywheel.radius) + ' ' +
            std::to_string(flywheel.mass) + ' ' + std::to_string(flywheel.angularVelocity) + ' ' +
            std::to_string(flywheel.rotation));
    }
    for (const SteeringWheel& wheel : level.steeringWheels) {
        NotifyTileOperation(std::string("actorstate steeringWheel ") + std::to_string(wheel.center.x) + ' ' +
            std::to_string(wheel.center.y) + ' ' + std::to_string(wheel.radius) + ' ' +
            std::to_string(wheel.rotation));
    }
    for (const Screw& screw : level.screws) {
        NotifyTileOperation(std::string("actorstate screw ") + std::to_string(screw.center.x) + ' ' +
            std::to_string(screw.center.y) + ' ' + std::to_string(screw.length) + ' ' +
            std::to_string(screw.radius) + ' ' + std::to_string(screw.angle) + ' ' +
            std::to_string(screw.spinSpeed));
    }
    for (const Fan& fan : level.fans) {
        NotifyTileOperation(std::string("actorstate fan ") + std::to_string(fan.center.x) + ' ' +
            std::to_string(fan.center.y) + ' ' + std::to_string(fan.direction.x) + ' ' +
            std::to_string(fan.direction.y) + ' ' + std::to_string(fan.length) + ' ' +
            std::to_string(fan.width) + ' ' + std::to_string(fan.strength) + ' ' +
            std::to_string(fan.power) + ' ' + std::to_string(fan.rotation));
    }
    for (const Pinwheel& pinwheel : level.pinwheels) {
        NotifyTileOperation(std::string("actorstate pinwheel ") + std::to_string(pinwheel.center.x) + ' ' +
            std::to_string(pinwheel.center.y) + ' ' + std::to_string(pinwheel.radius) + ' ' +
            std::to_string(pinwheel.rotation));
    }
    for (const Ramp& ramp : level.ramps) {
        NotifyTileOperation(std::string("actorstate ramp ") + std::to_string(ramp.center.x) + ' ' +
            std::to_string(ramp.center.y) + ' ' + std::to_string(ramp.length) + ' ' +
            std::to_string(ramp.thickness) + ' ' + std::to_string(ramp.angle) + ' ' +
            std::to_string(ramp.segmentCount));
    }
    for (const SeeSaw& seeSaw : level.seeSaws) {
        NotifyTileOperation(std::string("actorstate seeSaw ") + std::to_string(seeSaw.pivot.x) + ' ' +
            std::to_string(seeSaw.pivot.y) + ' ' + std::to_string(seeSaw.length) + ' ' +
            std::to_string(seeSaw.thickness) + ' ' + std::to_string(seeSaw.minAngle) + ' ' +
            std::to_string(seeSaw.maxAngle) + ' ' + std::to_string(seeSaw.response) + ' ' +
            std::to_string(seeSaw.angle));
    }
    for (const TrapDoor& trapDoor : level.trapDoors) {
        NotifyTileOperation(std::string("actorstate trapDoor ") + std::to_string(trapDoor.hinge.x) + ' ' +
            std::to_string(trapDoor.hinge.y) + ' ' + std::to_string(trapDoor.length) + ' ' +
            std::to_string(trapDoor.thickness) + ' ' + std::to_string(trapDoor.angle) + ' ' +
            (trapDoor.minimal ? "minimal" : "standard"));
    }
    for (const Chain& chain : level.chains) {
        NotifyTileOperation(std::string("actorstate chain ") + std::to_string(chain.start.x) + ' ' +
            std::to_string(chain.start.y) + ' ' + std::to_string(chain.end.x) + ' ' +
            std::to_string(chain.end.y) + ' ' + std::to_string(chain.spacing) + ' ' +
            std::to_string(chain.scale) + ' ' + (chain.pinStart ? "1" : "0") + ' ' +
            (chain.pinEnd ? "1" : "0"));
    }
    for (const PhysicsRope& rope : level.physicsRopes) {
        NotifyTileOperation(std::string("actorstate physicsRope ") + std::to_string(rope.start.x) + ' ' +
            std::to_string(rope.start.y) + ' ' + std::to_string(rope.end.x) + ' ' +
            std::to_string(rope.end.y) + ' ' + std::to_string(rope.length) + ' ' +
            std::to_string(rope.thickness) + ' ' + (rope.pinStart ? "1" : "0") + ' ' +
            (rope.pinEnd ? "1" : "0"));
    }
    for (const Button& button : level.buttons) {
        NotifyTileOperation(std::string("actorstate button ") + std::to_string(button.rect.x) + ' ' +
            std::to_string(button.rect.y) + ' ' + std::to_string(button.rect.width) + ' ' +
            std::to_string(button.rect.height));
    }
    for (const PortalPair& pair : level.portalPairs) {
        NotifyTileOperation(std::string("actorstate portalPair ") + std::to_string(pair.entrance.x) + ' ' +
            std::to_string(pair.entrance.y) + ' ' + std::to_string(pair.entrance.width) + ' ' +
            std::to_string(pair.entrance.height) + ' ' + std::to_string(pair.exit.x) + ' ' +
            std::to_string(pair.exit.y) + ' ' + std::to_string(pair.exit.width) + ' ' +
            std::to_string(pair.exit.height));
    }
    for (const DirectionalSpikeHazard& hazard : level.directionalSpikeHazards) {
        NotifyTileOperation(std::string("actorstate directionalSpikeHazard ") +
            SpikeDirectionName(hazard.direction) + ' ' + std::to_string(hazard.rect.x) + ' ' +
            std::to_string(hazard.rect.y) + ' ' + std::to_string(hazard.rect.width) + ' ' +
            std::to_string(hazard.rect.height));
    }
    for (const ArrowTrap& trap : level.arrowTraps) {
        NotifyTileOperation(std::string("actorstate arrowTrap ") + std::to_string(trap.position.x) + ' ' +
            std::to_string(trap.position.y) + ' ' + std::to_string(trap.direction.x) + ' ' +
            std::to_string(trap.direction.y) + ' ' + std::to_string(trap.interval) + ' ' +
            std::to_string(trap.speed));
    }
    for (const LevelLabel& label : level.labels) {
        NotifyTileOperation(std::string("actorstate labelSized ") + std::to_string(label.position.x) + ' ' +
            std::to_string(label.position.y) + ' ' + std::to_string(label.fontSize) + ' ' + label.text);
    }
    if (level.valve.center.x != 0.0f || level.valve.center.y != 0.0f) {
        NotifyTileOperation(std::string("actorstate valve ") + std::to_string(level.valve.center.x) + ' ' +
            std::to_string(level.valve.center.y) + ' ' + std::to_string(level.valve.radius));
    }
    for (const GuideObject& object : level.guideObjects) {
        if (object.type == GuideObjectType::Checkpoint) {
            NotifyTileOperation(std::string("actorstate checkpoint ") +
                std::to_string(object.transform.position.x) + ' ' + std::to_string(object.transform.position.y) + ' ' +
                std::to_string(object.collider.size.x) + ' ' + std::to_string(object.collider.size.y));
        }
        else if (object.type == GuideObjectType::Collectible) {
            NotifyTileOperation(std::string("actorstate collectible ") +
                std::to_string(object.transform.position.x) + ' ' + std::to_string(object.transform.position.y) + ' ' +
                std::to_string(object.collider.radius));
        }
        else if (object.type == GuideObjectType::Ball) {
            NotifyTileOperation(std::string("actorstate ball ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.radius) + ' ' +
                std::to_string(object.body.mass));
        }
        else if (object.type == GuideObjectType::Barrel) {
            NotifyTileOperation(std::string("actorstate barrel ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.size.x) + ' ' +
                std::to_string(object.collider.size.y) + ' ' + std::to_string(object.body.mass));
        }
        else if (object.type == GuideObjectType::MovingPlatform || object.type == GuideObjectType::Elevator) {
            const char* type = object.type == GuideObjectType::MovingPlatform ? "movingPlatform" : "elevator";
            NotifyTileOperation(std::string("actorstate ") + type + ' ' + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.size.x) + ' ' +
                std::to_string(object.collider.size.y) + ' ' + std::to_string(object.direction.x) + ' ' +
                std::to_string(object.direction.y) + ' ' + std::to_string(object.length) + ' ' +
                std::to_string(object.speed));
        }
        else if (object.type == GuideObjectType::PendulumBob) {
            NotifyTileOperation(std::string("actorstate pendulumBob ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.length) + ' ' +
                std::to_string(object.collider.radius) + ' ' + std::to_string(object.phase) + ' ' +
                std::to_string(object.speed));
        }
        else if (object.type == GuideObjectType::OneWayPlatform || object.type == GuideObjectType::GuideRail) {
            const char* type = object.type == GuideObjectType::OneWayPlatform ? "oneWayPlatform" : "guideRail";
            NotifyTileOperation(std::string("actorstate ") + type + ' ' + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.size.x) + ' ' +
                std::to_string(object.collider.size.y));
        }
        else if (object.type == GuideObjectType::CeilingHook) {
            NotifyTileOperation(std::string("actorstate ceilingHook ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.radius));
        }
        else if (object.type == GuideObjectType::Spring || object.type == GuideObjectType::CompressionSpring ||
            object.type == GuideObjectType::ExtensionSpring || object.type == GuideObjectType::TorsionSpring ||
            object.type == GuideObjectType::GarterSpring || object.type == GuideObjectType::VoluteSpring ||
            object.type == GuideObjectType::SpiralSpring || object.type == GuideObjectType::ConstantForceSpring ||
            object.type == GuideObjectType::ConstantTorqueSpring || object.type == GuideObjectType::LeafSpring ||
            object.type == GuideObjectType::BeamSpring || object.type == GuideObjectType::DiscSpring ||
            object.type == GuideObjectType::WaveSpring || object.type == GuideObjectType::WaveWasher ||
            object.type == GuideObjectType::TorsionBar || object.type == GuideObjectType::RingSpring ||
            object.type == GuideObjectType::ElastomerSpring || object.type == GuideObjectType::PneumaticSpring ||
            object.type == GuideObjectType::GasSpring || object.type == GuideObjectType::HydropneumaticSpring ||
            object.type == GuideObjectType::MagneticSpring || object.type == GuideObjectType::CompositeSpring) {
            const char* type = object.type == GuideObjectType::Spring ? "spring" :
                object.type == GuideObjectType::CompressionSpring ? "compressionSpring" :
                object.type == GuideObjectType::ExtensionSpring ? "extensionSpring" :
                object.type == GuideObjectType::TorsionSpring ? "torsionSpring" :
                object.type == GuideObjectType::GarterSpring ? "garterSpring" :
                object.type == GuideObjectType::VoluteSpring ? "voluteSpring" :
                object.type == GuideObjectType::SpiralSpring ? "spiralSpring" :
                object.type == GuideObjectType::ConstantForceSpring ? "constantForceSpring" :
                object.type == GuideObjectType::ConstantTorqueSpring ? "constantTorqueSpring" :
                object.type == GuideObjectType::LeafSpring ? "leafSpring" :
                object.type == GuideObjectType::BeamSpring ? "beamSpring" :
                object.type == GuideObjectType::DiscSpring ? "discSpring" :
                object.type == GuideObjectType::WaveSpring ? "waveSpring" :
                object.type == GuideObjectType::WaveWasher ? "waveWasher" :
                object.type == GuideObjectType::TorsionBar ? "torsionBar" :
                object.type == GuideObjectType::RingSpring ? "ringSpring" :
                object.type == GuideObjectType::ElastomerSpring ? "elastomerSpring" :
                object.type == GuideObjectType::PneumaticSpring ? "pneumaticSpring" :
                object.type == GuideObjectType::GasSpring ? "gasSpring" :
                object.type == GuideObjectType::HydropneumaticSpring ? "hydropneumaticSpring" :
                object.type == GuideObjectType::MagneticSpring ? "magneticSpring" : "compositeSpring";
            NotifyTileOperation(std::string("actorstate ") + type + ' ' +
                std::to_string(object.constraint.anchorA.x) + ' ' + std::to_string(object.constraint.anchorA.y) + ' ' +
                std::to_string(object.constraint.anchorB.x) + ' ' + std::to_string(object.constraint.anchorB.y) + ' ' +
                std::to_string(object.width) + ' ' + std::to_string(object.constraint.stiffness) + ' ' +
                std::to_string(object.constraint.damping));
        }
        else if (object.type == GuideObjectType::Rod) {
            NotifyTileOperation(std::string("actorstate rod ") + std::to_string(object.constraint.anchorA.x) + ' ' +
                std::to_string(object.constraint.anchorA.y) + ' ' + std::to_string(object.constraint.anchorB.x) + ' ' +
                std::to_string(object.constraint.anchorB.y) + ' ' + std::to_string(object.width));
        }
        else if (object.type == GuideObjectType::FixedJoint) {
            NotifyTileOperation(std::string("actorstate fixedJoint ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.radius));
        }
        else if (object.type == GuideObjectType::Crank || object.type == GuideObjectType::Ratchet ||
            object.type == GuideObjectType::Clutch) {
            const char* type = object.type == GuideObjectType::Crank ? "crank" :
                object.type == GuideObjectType::Ratchet ? "ratchet" : "clutch";
            std::string arguments = std::to_string(object.origin.x) + ' ' + std::to_string(object.origin.y) + ' ' +
                std::to_string(object.collider.radius) + ' ' + std::to_string(object.speed) + ' ';
            if (object.type == GuideObjectType::Clutch) arguments += std::string(object.engaged ? "1 " : "0 ");
            arguments += std::to_string(object.power.channel);
            NotifyTileOperation(std::string("actorstate ") + type + ' ' + arguments);
        }
        else if (object.type == GuideObjectType::Brake) {
            NotifyTileOperation(std::string("actorstate brake ") + std::to_string(object.origin.x) + ' ' +
                std::to_string(object.origin.y) + ' ' + std::to_string(object.collider.size.x) + ' ' +
                std::to_string(object.collider.size.y) + ' ' + std::to_string(object.strength) + ' ' +
                std::to_string(object.power.channel));
        }
    }
    for (const BreakableTile& tile : level.breakableTiles) {
        NotifyTileOperation(std::string("actorstate breakableTile ") + std::to_string(tile.rect.x) + ' ' +
            std::to_string(tile.rect.y) + ' ' + std::to_string(tile.rect.width) + ' ' +
            std::to_string(tile.rect.height) + ' ' + std::to_string(tile.breakDelay));
    }
    for (const Enemy& enemy : level.enemies) {
        NotifyTileOperation(std::string("actorstate enemy ") + std::to_string(enemy.rect.x) + ' ' +
            std::to_string(enemy.rect.y) + ' ' + std::to_string(enemy.rect.width) + ' ' +
            std::to_string(enemy.rect.height) + ' ' + std::to_string(enemy.patrolMinX) + ' ' +
            std::to_string(enemy.patrolMaxX) + ' ' + std::to_string(enemy.speed));
    }
    NotifyTileOperation("objectsyncend");
#endif
}

void LevelEditor::NotifySelectionChanged() const {
#if defined(_WIN32)
    HWND parent = GetParent(static_cast<HWND>(GetWindowHandle()));
    if (parent == nullptr) return;
    const auto kindName = [&](const Selection& item) -> const char* {
        switch (item.kind) {
        case SelectionKind::Solid: return "Solid";
        case SelectionKind::Platform: return "Platform";
        case SelectionKind::Ladder: return "Ladder";
        case SelectionKind::CameraZone: return "Camera Zone";
        case SelectionKind::Darkness: return "Darkness";
        case SelectionKind::PlayerStart: return "Player Start";
        case SelectionKind::Exit: return "Exit Trigger";
        case SelectionKind::SceneObject: return item.outlinerName.empty() ? item.sceneName.c_str() : item.outlinerName.c_str();
        case SelectionKind::None: return "None";
        }
        return "None";
    };
    if (groupSelection.size() > 1) {
        std::string message{"multiselect\t"};
        for (size_t index = 0; index < groupSelection.size(); ++index) {
            if (index > 0) message += ';';
            message += kindName(groupSelection[index]);
            message += ':';
            message += std::to_string(groupSelection[index].kind == SelectionKind::SceneObject &&
                groupSelection[index].outlinerIndex >= 0 ? groupSelection[index].outlinerIndex + 1 :
                groupSelection[index].index + 1);
        }
        COPYDATASTRUCT data{};
        data.dwData = 2;
        data.cbData = static_cast<DWORD>(message.size() + 1);
        data.lpData = message.data();
        SendMessageW(parent, WM_COPYDATA, reinterpret_cast<WPARAM>(GetWindowHandle()), reinterpret_cast<LPARAM>(&data));
        return;
    }
    const char* kind = "None";
    switch (selection.kind) {
    case SelectionKind::Solid: kind = "Solid"; break;
    case SelectionKind::Platform: kind = "Platform"; break;
    case SelectionKind::Ladder: kind = "Ladder"; break;
    case SelectionKind::CameraZone: kind = "Camera Zone"; break;
    case SelectionKind::Darkness: kind = "Darkness"; break;
    case SelectionKind::PlayerStart: kind = "Player Start"; break;
    case SelectionKind::Exit: kind = "Exit Trigger"; break;
    case SelectionKind::SceneObject: kind = selection.outlinerName.empty() ? selectedSceneName.c_str() : selection.outlinerName.c_str(); break;
    case SelectionKind::None: break;
    }
    const std::string message = std::string("select\t") + kind + '\t' +
        std::to_string(selection.kind == SelectionKind::SceneObject && selection.outlinerIndex >= 0 ?
            selection.outlinerIndex + 1 : selection.index + 1);
    COPYDATASTRUCT data{};
    data.dwData = 2;
    data.cbData = static_cast<DWORD>(message.size() + 1);
    data.lpData = const_cast<char*>(message.c_str());
    SendMessageW(parent, WM_COPYDATA, reinterpret_cast<WPARAM>(GetWindowHandle()), reinterpret_cast<LPARAM>(&data));
#endif
}

void LevelEditor::UpdateTilePainting(Rectangle canvas) {
    const Vector2 mouse = GetMousePosition();
    const Vector2 world = GetScreenToWorld2D(mouse, camera);
    const int gridX = static_cast<int>(floorf(world.x / 32.0f));
    const int gridY = static_cast<int>(floorf(world.y / 32.0f));
    const int activeLayerBit = TileLayerBit(paintTileLayer);
    const bool layerLocked = (lockedTileLayerMask & activeLayerBit) != 0;
    const auto tileAt = [&](TileLayer layer, int x, int y) {
        return std::find_if(level.visualTiles.begin(), level.visualTiles.end(), [&](const VisualTile& tile) {
            return tile.layer == layer && static_cast<int>(tile.position.x) == x * 32 &&
                static_cast<int>(tile.position.y) == y * 32;
        });
    };
    const auto symmetricCells = [&](int x, int y) {
        std::vector<std::pair<int, int>> cells{{x, y}};
        const int left = static_cast<int>(floorf(level.worldBounds.x / 32.0f));
        const int top = static_cast<int>(floorf(level.worldBounds.y / 32.0f));
        const int right = static_cast<int>(ceilf((level.worldBounds.x + level.worldBounds.width) / 32.0f)) - 1;
        const int bottom = static_cast<int>(ceilf((level.worldBounds.y + level.worldBounds.height) / 32.0f)) - 1;
        if ((tileSymmetryMask & 1) != 0) cells.push_back({left + right - x, y});
        if ((tileSymmetryMask & 2) != 0) cells.push_back({x, top + bottom - y});
        if (tileSymmetryMask == 3) cells.push_back({left + right - x, top + bottom - y});
        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        return cells;
    };
    const auto paintCell = [&](int x, int y) {
        const int sourceColumn = paintTileColumn + ((x % paintTileSelectionWidth) + paintTileSelectionWidth) % paintTileSelectionWidth;
        const int sourceRow = paintTileRow + ((y % paintTileSelectionHeight) + paintTileSelectionHeight) % paintTileSelectionHeight;
        for (const auto [targetX, targetY] : symmetricCells(x, y)) {
            VisualTile tile{paintTileLayer, sourceColumn, sourceRow,
                {static_cast<float>(targetX * 32), static_cast<float>(targetY * 32)}};
            tile.sheetIndex = paintTileSheet;
            tile.animationFrames = paintAnimationFrames;
            tile.animationFrameSeconds = paintAnimationFrameSeconds;
            auto existing = tileAt(tile.layer, targetX, targetY);
            if (existing == level.visualTiles.end()) level.visualTiles.push_back(tile);
            else *existing = tile;
            NotifyTilePainted(tile);
        }
    };
    const bool readOnlyTool = tilePaintTool == TilePaintTool::Eyedropper ||
        tilePaintTool == TilePaintTool::TileSelect || tilePaintTool == TilePaintTool::Measure ||
        tilePaintTool == TilePaintTool::MagicWand || tilePaintTool == TilePaintTool::SelectMatching;
    if ((tilePaintTool == TilePaintTool::CollisionBrush && paintTileLayer != TileLayer::Collision) ||
        (paintTileLayer == TileLayer::Collision && !readOnlyTool &&
            tilePaintTool != TilePaintTool::CollisionBrush && tilePaintTool != TilePaintTool::Eraser)) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            NotifyTileOperation("toolstatus Select the Collision layer to edit collision cells");
        tileDragActive = false;
        lastPaintGridX = INT_MIN;
        lastPaintGridY = INT_MIN;
        return;
    }
    if (layerLocked && !readOnlyTool) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) NotifyTileOperation("toolstatus Active tile layer is locked");
        tileDragActive = false;
        lastPaintGridX = INT_MIN;
        lastPaintGridY = INT_MIN;
        return;
    }
    const bool mutatingTool = tilePaintTool != TilePaintTool::Eyedropper &&
        tilePaintTool != TilePaintTool::TileSelect && tilePaintTool != TilePaintTool::Measure &&
        tilePaintTool != TilePaintTool::MagicWand && tilePaintTool != TilePaintTool::SelectMatching;
    if (mutatingTool && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, canvas)) PushTileUndo();

    if (tileDragActive && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (tilePaintTool == TilePaintTool::Measure) {
            const int columns = std::abs(gridX - tileDragStartX) + 1;
            const int rows = std::abs(gridY - tileDragStartY) + 1;
            NotifyTileOperation("toolstatus Measure: " + std::to_string(columns) + " x " + std::to_string(rows) +
                " tiles (" + std::to_string(columns * 32) + " x " + std::to_string(rows * 32) + " px)");
        }
        else if (tilePaintTool == TilePaintTool::RectangleFill) {
            for (int y = std::min(tileDragStartY, gridY); y <= std::max(tileDragStartY, gridY); ++y)
                for (int x = std::min(tileDragStartX, gridX); x <= std::max(tileDragStartX, gridX); ++x)
                    paintCell(x, y);
            dirty = true;
        }
        else if (tilePaintTool == TilePaintTool::RectangleOutline) {
            const int left = std::min(tileDragStartX, gridX);
            const int top = std::min(tileDragStartY, gridY);
            const int right = std::max(tileDragStartX, gridX);
            const int bottom = std::max(tileDragStartY, gridY);
            for (int x = left; x <= right; ++x) { paintCell(x, top); if (bottom != top) paintCell(x, bottom); }
            for (int y = top + 1; y < bottom; ++y) { paintCell(left, y); if (right != left) paintCell(right, y); }
            dirty = true;
        }
        else if (tilePaintTool == TilePaintTool::Ellipse) {
            const int left = std::min(tileDragStartX, gridX);
            const int top = std::min(tileDragStartY, gridY);
            const int right = std::max(tileDragStartX, gridX);
            const int bottom = std::max(tileDragStartY, gridY);
            const float centerX = (left + right) * 0.5f;
            const float centerY = (top + bottom) * 0.5f;
            const float radiusX = std::max(0.5f, (right - left) * 0.5f);
            const float radiusY = std::max(0.5f, (bottom - top) * 0.5f);
            std::unordered_set<unsigned long long> cells;
            const int samples = std::max(32, std::max(right - left + 1, bottom - top + 1) * 12);
            for (int index = 0; index < samples; ++index) {
                const float angle = 2.0f * PI * static_cast<float>(index) / static_cast<float>(samples);
                const int x = static_cast<int>(roundf(centerX + cosf(angle) * radiusX));
                const int y = static_cast<int>(roundf(centerY + sinf(angle) * radiusY));
                const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(x)) << 32) |
                    static_cast<unsigned int>(y);
                if (cells.insert(key).second) paintCell(x, y);
            }
            dirty = true;
        }
        else if (tilePaintTool == TilePaintTool::Line) {
            int x = tileDragStartX;
            int y = tileDragStartY;
            const int dx = std::abs(gridX - x);
            const int sx = x < gridX ? 1 : -1;
            const int dy = -std::abs(gridY - y);
            const int sy = y < gridY ? 1 : -1;
            int error = dx + dy;
            while (true) {
                paintCell(x, y);
                if (x == gridX && y == gridY) break;
                const int doubled = error * 2;
                if (doubled >= dy) { error += dy; x += sx; }
                if (doubled <= dx) { error += dx; y += sy; }
            }
            dirty = true;
        }
        else if (tilePaintTool == TilePaintTool::TileSelect) {
            if (movingTileSelection) {
                const int deltaX = gridX - tileDragStartX;
                const int deltaY = gridY - tileDragStartY;
                if (deltaX != 0 || deltaY != 0) {
                    std::vector<VisualTile> moving;
                    for (auto iterator = level.visualTiles.begin(); iterator != level.visualTiles.end();) {
                        const int x = static_cast<int>(iterator->position.x) / 32;
                        const int y = static_cast<int>(iterator->position.y) / 32;
                        if (iterator->layer == paintTileLayer && IsTileCellSelected(x, y)) {
                            moving.push_back(*iterator);
                            NotifyTileErased(iterator->layer, iterator->position);
                            iterator = level.visualTiles.erase(iterator);
                        }
                        else ++iterator;
                    }
                    for (VisualTile& tile : moving) {
                        tile.position.x += static_cast<float>(deltaX * 32);
                        tile.position.y += static_cast<float>(deltaY * 32);
                        const int x = static_cast<int>(tile.position.x) / 32;
                        const int y = static_cast<int>(tile.position.y) / 32;
                        auto existing = tileAt(tile.layer, x, y);
                        if (existing == level.visualTiles.end()) level.visualTiles.push_back(tile); else *existing = tile;
                        NotifyTilePainted(tile);
                    }
                    std::unordered_set<unsigned long long> movedCells;
                    for (const unsigned long long key : tileSelectionCells) {
                        const int x = static_cast<int>(static_cast<int32_t>(key >> 32));
                        const int y = static_cast<int>(static_cast<int32_t>(key));
                        movedCells.insert(TileCellKey(x + deltaX, y + deltaY));
                    }
                    tileSelectionCells = std::move(movedCells);
                    UpdateTileSelectionBounds();
                    dirty = true;
                }
            }
            else {
                tileSelectionLeft = std::min(tileDragStartX, gridX);
                tileSelectionTop = std::min(tileDragStartY, gridY);
                tileSelectionRight = std::max(tileDragStartX, gridX);
                tileSelectionBottom = std::max(tileDragStartY, gridY);
                tileSelectionCells.clear();
                for (const VisualTile& tile : level.visualTiles) {
                    const int x = static_cast<int>(tile.position.x) / 32;
                    const int y = static_cast<int>(tile.position.y) / 32;
                    if (tile.layer == paintTileLayer && x >= tileSelectionLeft && x <= tileSelectionRight &&
                        y >= tileSelectionTop && y <= tileSelectionBottom)
                        tileSelectionCells.insert(TileCellKey(x, y));
                }
                UpdateTileSelectionBounds();
            }
            movingTileSelection = false;
            NotifyTileSelection();
        }
        tileDragActive = false;
    }

    if (!CheckCollisionPointRec(mouse, canvas)) return;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (tilePaintTool == TilePaintTool::RectangleFill || tilePaintTool == TilePaintTool::RectangleOutline ||
            tilePaintTool == TilePaintTool::Ellipse || tilePaintTool == TilePaintTool::Line || tilePaintTool == TilePaintTool::Measure ||
            tilePaintTool == TilePaintTool::TileSelect) {
            tileDragActive = true;
            tileDragStartX = gridX;
            tileDragStartY = gridY;
            movingTileSelection = tilePaintTool == TilePaintTool::TileSelect && tileSelectionActive &&
                IsTileCellSelected(gridX, gridY);
            if (movingTileSelection) PushTileUndo();
            return;
        }
        if (tilePaintTool == TilePaintTool::Eyedropper) {
            const auto existing = tileAt(paintTileLayer, gridX, gridY);
            if (existing != level.visualTiles.end()) {
                const char* layer = TileLayerName(existing->layer);
                NotifyTileOperation(std::string("pick ") + layer + ' ' + std::to_string(existing->column) + ' ' +
                    std::to_string(existing->row) + ' ' + std::to_string(existing->quarterTurns) + ' ' +
                    std::to_string(existing->flipX ? 1 : 0) + ' ' + std::to_string(existing->flipY ? 1 : 0) + ' ' +
                    std::to_string(existing->animationFrames) + ' ' + std::to_string(existing->animationFrameSeconds) + ' ' +
                    std::to_string(existing->sheetIndex >= 0 ? existing->sheetIndex :
                        (existing->layer == TileLayer::FarBackground ? 2 : existing->layer == TileLayer::Background ? 1 : 0)));
            }
            return;
        }
        if (tilePaintTool == TilePaintTool::MagicWand || tilePaintTool == TilePaintTool::SelectMatching) {
            const auto start = tileAt(paintTileLayer, gridX, gridY);
            tileSelectionCells.clear();
            if (start == level.visualTiles.end()) {
                UpdateTileSelectionBounds();
                NotifyTileSelection();
                return;
            }
            const int targetColumn = start->column;
            const int targetRow = start->row;
            if (tilePaintTool == TilePaintTool::SelectMatching) {
                for (const VisualTile& tile : level.visualTiles) {
                    if (tile.layer == paintTileLayer && tile.column == targetColumn && tile.row == targetRow)
                        tileSelectionCells.insert(TileCellKey(static_cast<int>(tile.position.x) / 32,
                            static_cast<int>(tile.position.y) / 32));
                }
            }
            else {
                std::queue<std::pair<int, int>> pending;
                std::unordered_set<unsigned long long> visited;
                pending.push({gridX, gridY});
                while (!pending.empty()) {
                    const auto [x, y] = pending.front();
                    pending.pop();
                    const unsigned long long key = TileCellKey(x, y);
                    if (!visited.insert(key).second) continue;
                    const auto tile = tileAt(paintTileLayer, x, y);
                    if (tile == level.visualTiles.end() || tile->column != targetColumn || tile->row != targetRow) continue;
                    tileSelectionCells.insert(key);
                    pending.push({x + 1, y}); pending.push({x - 1, y});
                    pending.push({x, y + 1}); pending.push({x, y - 1});
                }
            }
            UpdateTileSelectionBounds();
            NotifyTileSelection();
            return;
        }
        if (tilePaintTool == TilePaintTool::FloodFill) {
            const auto start = tileAt(paintTileLayer, gridX, gridY);
            if (start == level.visualTiles.end()) return;
            const int targetColumn = start->column;
            const int targetRow = start->row;
            if (targetColumn == paintTileColumn && targetRow == paintTileRow) return;
            std::queue<std::pair<int, int>> pending;
            std::unordered_set<unsigned long long> visited;
            pending.push({gridX, gridY});
            while (!pending.empty()) {
                const auto [x, y] = pending.front();
                pending.pop();
                const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(x)) << 32) |
                    static_cast<unsigned int>(y);
                if (!visited.insert(key).second) continue;
                const auto current = tileAt(paintTileLayer, x, y);
                if (current == level.visualTiles.end() || current->column != targetColumn || current->row != targetRow) continue;
                paintCell(x, y);
                pending.push({x + 1, y}); pending.push({x - 1, y});
                pending.push({x, y + 1}); pending.push({x, y - 1});
            }
            dirty = !visited.empty();
            return;
        }
        if (tilePaintTool == TilePaintTool::Replace) {
            const auto target = tileAt(paintTileLayer, gridX, gridY);
            if (target == level.visualTiles.end()) return;
            const int targetColumn = target->column;
            const int targetRow = target->row;
            if (targetColumn == paintTileColumn && targetRow == paintTileRow) return;
            for (VisualTile& tile : level.visualTiles) {
                if (tile.layer != paintTileLayer || tile.column != targetColumn || tile.row != targetRow) continue;
                tile.column = paintTileColumn;
                tile.row = paintTileRow;
                NotifyTilePainted(tile);
            }
            dirty = true;
            return;
        }
        if (tilePaintTool == TilePaintTool::Stamp) {
            if (tileClipboard.empty()) {
                NotifyTileOperation("toolstatus Copy a tile selection before using Stamp");
                return;
            }
            for (const auto [originX, originY] : symmetricCells(gridX, gridY)) {
                for (VisualTile tile : tileClipboard) {
                    tile.position.x += static_cast<float>(originX * 32);
                    tile.position.y += static_cast<float>(originY * 32);
                    const int x = static_cast<int>(tile.position.x) / 32;
                    const int y = static_cast<int>(tile.position.y) / 32;
                    auto existing = tileAt(tile.layer, x, y);
                    if (existing == level.visualTiles.end()) level.visualTiles.push_back(tile); else *existing = tile;
                    NotifyTilePainted(tile);
                }
            }
            dirty = true;
            return;
        }
        if (tilePaintTool == TilePaintTool::CollisionBrush) {
            collisionStrokeErase = tileAt(TileLayer::Collision, gridX, gridY) != level.visualTiles.end();
        }
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || tilePaintTool == TilePaintTool::Eyedropper ||
        tilePaintTool == TilePaintTool::MagicWand || tilePaintTool == TilePaintTool::SelectMatching ||
        tilePaintTool == TilePaintTool::FloodFill || tilePaintTool == TilePaintTool::Replace ||
        tilePaintTool == TilePaintTool::Stamp || tilePaintTool == TilePaintTool::RectangleFill ||
        tilePaintTool == TilePaintTool::RectangleOutline || tilePaintTool == TilePaintTool::Ellipse ||
        tilePaintTool == TilePaintTool::Line || tilePaintTool == TilePaintTool::Measure ||
        tilePaintTool == TilePaintTool::TileSelect) {
        lastPaintGridX = INT_MIN;
        lastPaintGridY = INT_MIN;
        return;
    }
    if (gridX == lastPaintGridX && gridY == lastPaintGridY) return;
    lastPaintGridX = gridX;
    lastPaintGridY = gridY;

    if (tilePaintTool == TilePaintTool::AutoTile) {
        const Texture2D activeSheet = paintTileSheet == 2 ? industrialFarBackground :
            (paintTileSheet == 1 ? industrialBackground : industrialTiles);
        const int baseColumn = std::clamp(paintTileColumn, 0, std::max(0, activeSheet.width / 32 - 4));
        const int baseRow = std::clamp(paintTileRow, 0, std::max(0, activeSheet.height / 32 - 4));
        std::vector<std::pair<int, int>> changed;
        for (const auto [seedX, seedY] : symmetricCells(gridX, gridY)) {
            VisualTile seed{paintTileLayer, baseColumn, baseRow,
                {static_cast<float>(seedX * 32), static_cast<float>(seedY * 32)}};
            seed.sheetIndex = paintTileSheet;
            seed.animationFrames = paintAnimationFrames;
            seed.animationFrameSeconds = paintAnimationFrameSeconds;
            auto existing = tileAt(seed.layer, seedX, seedY);
            if (existing == level.visualTiles.end()) level.visualTiles.push_back(seed); else *existing = seed;
            changed.push_back({seedX, seedY});
        }
        for (const auto [seedX, seedY] : changed) {
            const std::pair<int, int> neighbors[] = {{seedX, seedY}, {seedX, seedY - 1}, {seedX + 1, seedY},
                {seedX, seedY + 1}, {seedX - 1, seedY}};
            for (const auto [x, y] : neighbors) {
                auto tile = tileAt(paintTileLayer, x, y);
                if (tile == level.visualTiles.end() || tile->column < baseColumn || tile->column > baseColumn + 3 ||
                    tile->row < baseRow || tile->row > baseRow + 3) continue;
                int mask = 0;
                const auto belongs = [&](int neighborX, int neighborY) {
                    const auto neighbor = tileAt(paintTileLayer, neighborX, neighborY);
                    return neighbor != level.visualTiles.end() && neighbor->column >= baseColumn &&
                        neighbor->column <= baseColumn + 3 && neighbor->row >= baseRow && neighbor->row <= baseRow + 3;
                };
                if (belongs(x, y - 1)) mask |= 1;
                if (belongs(x + 1, y)) mask |= 2;
                if (belongs(x, y + 1)) mask |= 4;
                if (belongs(x - 1, y)) mask |= 8;
                tile->column = baseColumn + mask % 4;
                tile->row = baseRow + mask / 4;
                NotifyTilePainted(*tile);
            }
        }
        dirty = true;
        return;
    }

    const int brushSize = tilePaintTool == TilePaintTool::Pencil ? 1 : paintBrushSize;
    const bool stampSelection = tilePaintTool == TilePaintTool::Pencil || tilePaintTool == TilePaintTool::Brush ||
        tilePaintTool == TilePaintTool::RandomBrush || tilePaintTool == TilePaintTool::Eraser;
    const int tileWidth = stampSelection ? paintTileSelectionWidth : 1;
    const int tileHeight = stampSelection ? paintTileSelectionHeight : 1;
    const int footprintWidth = tileWidth * brushSize;
    const int footprintHeight = tileHeight * brushSize;
    const int offsetX = (footprintWidth - 1) / 2;
    const int offsetY = (footprintHeight - 1) / 2;
    for (int brushY = 0; brushY < brushSize; ++brushY) {
        for (int brushX = 0; brushX < brushSize; ++brushX) {
          for (int y = 0; y < tileHeight; ++y) {
            for (int x = 0; x < tileWidth; ++x) {
            if (tilePaintTool == TilePaintTool::CollisionBrush) {
                for (const auto [targetX, targetY] : symmetricCells(gridX + brushX * tileWidth + x - offsetX,
                    gridY + brushY * tileHeight + y - offsetY)) {
                    auto existing = tileAt(TileLayer::Collision, targetX, targetY);
                    if (collisionStrokeErase) {
                        if (existing != level.visualTiles.end()) {
                            const Vector2 position = existing->position;
                            level.visualTiles.erase(existing);
                            NotifyTileErased(TileLayer::Collision, position);
                        }
                    }
                    else if (existing == level.visualTiles.end()) {
                        VisualTile tile{TileLayer::Collision, 0, 0,
                            {static_cast<float>(targetX * 32), static_cast<float>(targetY * 32)}};
                        level.visualTiles.push_back(tile);
                        NotifyTilePainted(tile);
                    }
                }
                continue;
            }
            VisualTile tile{};
            tile.layer = paintTileLayer;
            tile.sheetIndex = paintTileSheet;
            tile.animationFrames = paintAnimationFrames;
            tile.animationFrameSeconds = paintAnimationFrameSeconds;
            const Texture2D activeSheet = paintTileSheet == 2 ? industrialFarBackground :
                (paintTileSheet == 1 ? industrialBackground : industrialTiles);
            const int variantX = tilePaintTool == TilePaintTool::RandomBrush ? GetRandomValue(0, tileWidth - 1) : x;
            const int variantY = tilePaintTool == TilePaintTool::RandomBrush ? GetRandomValue(0, tileHeight - 1) : y;
            tile.column = std::clamp(paintTileColumn + variantX,
                0, std::max(0, activeSheet.width / 32 - 1));
            tile.row = std::clamp(paintTileRow + variantY,
                0, std::max(0, activeSheet.height / 32 - 1));
            const int sourceX = gridX + brushX * tileWidth + x - offsetX;
            const int sourceY = gridY + brushY * tileHeight + y - offsetY;
            for (const auto [targetX, targetY] : symmetricCells(sourceX, sourceY)) {
                tile.position = {static_cast<float>(targetX * 32), static_cast<float>(targetY * 32)};
                auto existing = tileAt(tile.layer, targetX, targetY);
                if (tilePaintTool == TilePaintTool::Eraser) {
                    if (existing != level.visualTiles.end()) {
                        const Vector2 erasedPosition = existing->position;
                        level.visualTiles.erase(existing);
                        NotifyTileErased(tile.layer, erasedPosition);
                    }
                }
                else {
                    if (existing == level.visualTiles.end()) level.visualTiles.push_back(tile);
                    else *existing = tile;
                    NotifyTilePainted(tile);
                }
            }
            }
          }
        }
    }
    dirty = true;
}

void LevelEditor::DeleteSelection() {
    if (groupSelection.size() > 1) {
        PushTileUndo();
        std::vector<Selection> sceneItems;
        for (const Selection& item : groupSelection)
            if (item.kind == SelectionKind::SceneObject) sceneItems.push_back(item);
        const auto collectionName = [](const std::string& name) {
            if (name.rfind("Portal ", 0) == 0) return std::string("Portal");
            const size_t separator = name.find_last_of(' ');
            return separator == std::string::npos ? name : name.substr(0, separator);
        };
        std::sort(sceneItems.begin(), sceneItems.end(), [&](const Selection& left, const Selection& right) {
            const std::string leftCollection = collectionName(left.sceneName);
            const std::string rightCollection = collectionName(right.sceneName);
            return leftCollection == rightCollection ? left.index > right.index : leftCollection < rightCollection;
        });
        std::string previousCollection;
        int previousIndex = -1;
        for (const Selection& item : sceneItems) {
            const std::string collection = collectionName(item.sceneName);
            if (collection == previousCollection && item.index == previousIndex) continue;
            SetPrimarySelection(item);
            DeleteSelectedSceneObject();
            previousCollection = collection;
            previousIndex = item.index;
        }
        const auto eraseSelected = [&](auto& values, SelectionKind kind) {
            std::vector<int> indices;
            for (const Selection& item : groupSelection) if (item.kind == kind) indices.push_back(item.index);
            std::sort(indices.rbegin(), indices.rend());
            for (const int index : indices)
                if (index >= 0 && index < static_cast<int>(values.size())) values.erase(values.begin() + index);
        };
        eraseSelected(level.baseSolids, SelectionKind::Solid);
        eraseSelected(level.pitPlatforms, SelectionKind::Platform);
        eraseSelected(level.ladders, SelectionKind::Ladder);
        eraseSelected(level.cameraZones, SelectionKind::CameraZone);
        eraseSelected(level.darknessAreas, SelectionKind::Darkness);
        if (IsGroupSelected({SelectionKind::Exit, 0})) level.exitTrigger = {};
        MarkDirty("Deleted selected objects.");
        selection = {};
        groupSelection.clear();
        if (viewportOnly) NotifySelectionChanged();
        return;
    }
    switch (selection.kind) {
    case SelectionKind::Solid:
        PushTileUndo();
        level.baseSolids.erase(level.baseSolids.begin() + selection.index);
        MarkDirty("Deleted solid.");
        break;
    case SelectionKind::Platform:
        PushTileUndo();
        level.pitPlatforms.erase(level.pitPlatforms.begin() + selection.index);
        MarkDirty("Deleted platform.");
        break;
    case SelectionKind::Ladder:
        PushTileUndo();
        level.ladders.erase(level.ladders.begin() + selection.index);
        MarkDirty("Deleted ladder.");
        break;
    case SelectionKind::CameraZone:
        PushTileUndo();
        level.cameraZones.erase(level.cameraZones.begin() + selection.index);
        MarkDirty("Deleted camera zone.");
        break;
    case SelectionKind::Darkness:
        PushTileUndo();
        level.darknessAreas.erase(level.darknessAreas.begin() + selection.index);
        MarkDirty("Deleted darkness region.");
        break;
    case SelectionKind::Exit:
        PushTileUndo();
        level.exitTrigger = {};
        MarkDirty("Deleted exit trigger.");
        break;
    case SelectionKind::SceneObject:
        PushTileUndo();
        if (DeleteSelectedSceneObject()) MarkDirty("Deleted selection.");
        else {
            if (!viewportOnly) tileUndoHistory.pop_back();
            statusText = "This object is a singleton or cannot be deleted.";
        }
        break;
    case SelectionKind::None:
    case SelectionKind::PlayerStart:
        statusText = "Nothing selected to delete.";
        break;
    }
    selection = {};
    groupSelection.clear();
    if (viewportOnly) NotifySelectionChanged();
}

void LevelEditor::UpdateEditing(Rectangle canvas) {
    const Vector2 mouse = GetMousePosition();
    const Vector2 world = SnapWorldPoint(GetScreenToWorld2D(mouse, camera));

    if (IsKeyPressed(KEY_ESCAPE)) {
        draggingCreate = false;
        draggingMarquee = false;
        selection = {};
        groupSelection.clear();
        statusText = "Selection cleared.";
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        DeleteSelection();
    }

    Vector2 nudge{};
    if (IsKeyPressed(KEY_LEFT)) nudge.x -= gridSnap;
    if (IsKeyPressed(KEY_RIGHT)) nudge.x += gridSnap;
    if (IsKeyPressed(KEY_UP)) nudge.y -= gridSnap;
    if (IsKeyPressed(KEY_DOWN)) nudge.y += gridSnap;
    if (nudge.x != 0.0f || nudge.y != 0.0f) {
        if (groupSelection.size() > 1) {
            PushTileUndo();
            for (Selection& item : groupSelection) {
                if (item.kind == SelectionKind::PlayerStart) {
                    level.playerStart.x += nudge.x;
                    level.playerStart.y += nudge.y;
                }
                else if (Rectangle* rect = RectangleForSelection(item)) {
                    rect->x += nudge.x;
                    rect->y += nudge.y;
                }
                else if (item.kind == SelectionKind::SceneObject) {
                    SetPrimarySelection(item);
                    if (TranslateSelectedScene(nudge)) item.sceneBounds = selectedSceneBounds;
                }
            }
            SetPrimarySelection(groupSelection.back());
            MarkDirty("Moved selected objects.");
        }
        else if (selection.kind == SelectionKind::PlayerStart) {
            PushTileUndo();
            level.playerStart.x += nudge.x;
            level.playerStart.y += nudge.y;
            MarkDirty("Moved player start.");
        }
        else if (Rectangle* rect = SelectedRectangle()) {
            PushTileUndo();
            rect->x += nudge.x;
            rect->y += nudge.y;
            MarkDirty("Moved selection.");
        }
        else if (selection.kind == SelectionKind::SceneObject && TranslateSelectedScene({0.0f, 0.0f})) {
            PushTileUndo();
            TranslateSelectedScene(nudge);
            MarkDirty("Moved selection.");
        }
    }

    if (activeTool == EditTool::Select && !draggingTransform) SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (!CheckCollisionPointRec(mouse, canvas) || (!viewportOnly && IsMouseOverPanel(mouse)) ||
        draggedPanel != PanelId::Count ||
        resizeTarget != ResizeTarget::None) {
        return;
    }

    if (activeTool == EditTool::Select) {
        const Vector2 unsnappedWorld = GetScreenToWorld2D(mouse, camera);
        const TransformHandle hoveredHandle = TransformHandleAt(unsnappedWorld);
        if (!draggingTransform) {
            if (hoveredHandle == TransformHandle::MoveCenter) SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
            else if (hoveredHandle == TransformHandle::MoveX) SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            else if (hoveredHandle == TransformHandle::MoveY) SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
            else if (hoveredHandle == TransformHandle::ScaleBottomRight) SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
            else if (hoveredHandle == TransformHandle::RotateRing) SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
            else SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            activeTransformHandle = hoveredHandle;
            if (activeTransformHandle == TransformHandle::None) {
                const Selection hit = HitTest(unsnappedWorld);
                const bool additive = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                if (additive && hit.kind != SelectionKind::None) {
                    const auto existing = std::find_if(groupSelection.begin(), groupSelection.end(), [&](const Selection& item) {
                        return item.kind == hit.kind && item.index == hit.index &&
                            (hit.kind != SelectionKind::SceneObject || item.sceneName == hit.sceneName);
                    });
                    if (existing == groupSelection.end()) groupSelection.push_back(hit);
                    else groupSelection.erase(existing);
                    SetPrimarySelection(groupSelection.empty() ? Selection{} : groupSelection.back());
                    statusText = std::to_string(groupSelection.size()) + " objects selected.";
                }
                else {
                    selection = hit;
                    groupSelection.clear();
                    if (selection.kind != SelectionKind::None) groupSelection.push_back(selection);
                }
                if (hit.kind == SelectionKind::None && transformMode == TransformMode::Select) {
                    draggingMarquee = true;
                    marqueeStart = unsnappedWorld;
                    marqueeCurrent = unsnappedWorld;
                }
                activeTransformHandle = TransformHandleAt(unsnappedWorld);
                if (viewportOnly) NotifySelectionChanged();
            }
            if (selection.kind != SelectionKind::None) {
                transformDragStart = world;
                transformLastWorld = world;
                transformOriginalPoint = level.playerStart;
                if (const Rectangle* rect = SelectedRectangle()) transformOriginalRect = *rect;
                if ((transformMode == TransformMode::Move &&
                    (selection.kind == SelectionKind::PlayerStart || SelectedRectangle() != nullptr ||
                        (selection.kind == SelectionKind::SceneObject && TranslateSelectedScene({0.0f, 0.0f})))) ||
                    (transformMode == TransformMode::Scale &&
                        ((SelectedRectangle() != nullptr && selection.kind != SelectionKind::Exit) || groupSelection.size() > 1 ||
                            (selection.kind == SelectionKind::SceneObject && ScaleSelectedScene({0.0f, 0.0f})))) ||
                    (transformMode == TransformMode::Rotate && SelectedRotation() != nullptr &&
                        activeTransformHandle == TransformHandle::RotateRing)) {
                    PushTileUndo();
                    draggingTransform = true;
                    if (const float* rotation = SelectedRotation()) {
                        transformOriginalRotation = *rotation;
                        const Vector2 center = SelectedRotationCenter();
                        transformDragStartAngle = atan2f(unsnappedWorld.y - center.y,
                            unsnappedWorld.x - center.x) * RAD2DEG;
                    }
                    transformOriginalGroupRects.clear();
                    transformOriginalGroupPoints.clear();
                    transformOriginalGroupBounds = SelectionGroupBounds();
                    for (const Selection& item : groupSelection) {
                        transformOriginalGroupRects.push_back(RectangleForSelection(item) ? *RectangleForSelection(item) :
                            (item.kind == SelectionKind::SceneObject ? item.sceneBounds : Rectangle{}));
                        transformOriginalGroupPoints.push_back(item.kind == SelectionKind::PlayerStart ? level.playerStart : Vector2{});
                    }
                }
                else if (selection.kind == SelectionKind::SceneObject && transformMode == TransformMode::Scale) {
                    statusText = "Selected " + selectedSceneName + ". Scaling support depends on object shape.";
                }
                else if (transformMode == TransformMode::Scale) {
                    statusText = selection.kind == SelectionKind::Exit
                        ? "Exit Door size is fixed at the standard dimensions."
                        : "Player Start has no scalable bounds.";
                }
                else if (transformMode == TransformMode::Rotate) {
                    statusText = "This object type does not store rotation. Select an angled object when rotation support is added.";
                }
            }
        }
        if (draggingMarquee && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) marqueeCurrent = unsnappedWorld;
        if (draggingMarquee && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingMarquee = false;
            const Rectangle marquee = MakeRect(marqueeStart, marqueeCurrent);
            groupSelection.clear();
            const auto collect = [&](const std::vector<Rectangle>& rects, SelectionKind kind) {
                for (int index = 0; index < static_cast<int>(rects.size()); ++index)
                    if (CheckCollisionRecs(marquee, rects[static_cast<size_t>(index)])) groupSelection.push_back({kind, index});
            };
            collect(level.baseSolids, SelectionKind::Solid);
            collect(level.pitPlatforms, SelectionKind::Platform);
            collect(level.ladders, SelectionKind::Ladder);
            collect(level.cameraZones, SelectionKind::CameraZone);
            collect(level.darknessAreas, SelectionKind::Darkness);
            if (CheckCollisionPointRec(level.playerStart, marquee)) groupSelection.push_back({SelectionKind::PlayerStart, 0});
            if (CheckCollisionRecs(marquee, level.exitTrigger)) groupSelection.push_back({SelectionKind::Exit, 0});
            const auto collectScene = [&](Rectangle bounds, std::string name, int nativeIndex,
                std::string outlinerName = {}, int outlinerIndex = -1) {
                if (bounds.width <= 0.0f || bounds.height <= 0.0f || !CheckCollisionRecs(marquee, bounds)) return;
                if (outlinerName.empty()) outlinerName = name;
                if (outlinerIndex < 0) outlinerIndex = nativeIndex;
                groupSelection.push_back({SelectionKind::SceneObject, nativeIndex, std::move(name), bounds,
                    std::move(outlinerName), outlinerIndex});
            };
            const auto indexed = [](const char* name, int index) {
                return std::string(name) + " " + std::to_string(index + 1);
            };
            for (int index = 0; index < static_cast<int>(level.enemies.size()); ++index)
                collectScene(level.enemies[index].rect, indexed("Enemy", index), index);
            for (int index = 0; index < static_cast<int>(level.guideObjects.size()); ++index) {
                const GuideObject& object = level.guideObjects[static_cast<size_t>(index)];
                int typedIndex = 0;
                for (int prior = 0; prior <= index; ++prior)
                    if (level.guideObjects[static_cast<size_t>(prior)].type == object.type) ++typedIndex;
                collectScene(GetGuideObjectBounds(object), indexed("Guide Object", index), index,
                    GetGuideObjectName(object.type), typedIndex - 1);
            }
            for (int index = 0; index < static_cast<int>(level.breakableTiles.size()); ++index)
                collectScene(level.breakableTiles[index].rect, indexed("Breakable Tile", index), index);
            for (int index = 0; index < static_cast<int>(level.directionalSpikeHazards.size()); ++index)
                collectScene(level.directionalSpikeHazards[index].rect, indexed("Directional Spikes", index), index);
            for (int index = 0; index < static_cast<int>(level.buttons.size()); ++index)
                collectScene(level.buttons[index].rect, indexed("Button", index), index);
            for (int index = 0; index < static_cast<int>(level.fluids.size()); ++index)
                collectScene(level.fluids[index].bounds, indexed("Fluid Field", index), index, "Fluid Field", index);
            for (int index = 0; index < static_cast<int>(level.stoneBlocks.size()); ++index)
                collectScene(level.stoneBlocks[index].rect, indexed("Stone Block", index), index);
            for (int index = 0; index < static_cast<int>(level.weights.size()); ++index)
                collectScene(level.weights[index].rect, indexed("Hanging Weight", index), index);
            for (int index = 0; index < static_cast<int>(level.rotaryLatches.size()); ++index) {
                const auto& value = level.rotaryLatches[index];
                collectScene({value.center.x - value.radius, value.center.y - value.radius,
                    value.radius * 2.0f, value.radius * 2.0f}, indexed("Rotary Latch", index), index);
            }
            const auto collectCircles = [&](const auto& objects, const char* label, auto centerOf, auto radiusOf) {
                for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
                    const Vector2 center = centerOf(objects[static_cast<size_t>(index)]);
                    const float radius = radiusOf(objects[static_cast<size_t>(index)]);
                    collectScene({center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f},
                        indexed(label, index), index);
                }
            };
            collectCircles(level.boulders, "Boulder", [](const Boulder& v) { return v.center; }, [](const Boulder& v) { return v.radius; });
            collectCircles(level.physicsWheels, "Physics Wheel", [](const PhysicsWheel& v) { return v.center; }, [](const PhysicsWheel& v) { return v.radius; });
            collectCircles(level.gears, "Gear", [](const Gear& v) { return v.center; }, [](const Gear& v) { return v.radius * GearOuterRadiusScale; });
            collectCircles(level.flywheels, "Flywheel", [](const Flywheel& v) { return v.center; }, [](const Flywheel& v) { return v.radius; });
            collectCircles(level.steeringWheels, "Steering Wheel", [](const SteeringWheel& v) { return v.center; }, [](const SteeringWheel& v) { return v.radius; });
            collectCircles(level.pinwheels, "Pinwheel", [](const Pinwheel& v) { return v.center; }, [](const Pinwheel& v) { return v.radius; });
            for (int index = 0; index < static_cast<int>(level.portalPairs.size()); ++index) {
                const auto& pair = level.portalPairs[index];
                if (CheckCollisionRecs(marquee, pair.entrance))
                    collectScene(pair.entrance, indexed("Portal Entrance", index), index, "Portal Pair", index);
                else if (CheckCollisionRecs(marquee, pair.exit))
                    collectScene(pair.exit, indexed("Portal Exit", index), index, "Portal Pair", index);
            }
            for (int index = 0; index < static_cast<int>(level.pulleys.size()); ++index) {
                const Vector2 point = level.pulleys[static_cast<size_t>(index)];
                collectScene({point.x - 30.0f, point.y - 30.0f, 60.0f, 60.0f}, indexed("Pulley", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.screws.size()); ++index) {
                const auto& v = level.screws[index]; const float extent = v.length * 0.5f + v.radius;
                collectScene({v.center.x - extent, v.center.y - extent, extent * 2, extent * 2}, indexed("Screw", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.fans.size()); ++index) {
                const auto& v = level.fans[index];
                collectScene({v.center.x - v.width * 0.5f, v.center.y - v.width * 0.5f,
                    v.length + v.width, v.width}, indexed("Fan", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.ramps.size()); ++index) {
                const auto& v = level.ramps[index]; const float extent = v.length * 0.5f + v.thickness;
                collectScene({v.center.x - extent, v.center.y - extent, extent * 2, extent * 2}, indexed("Ramp", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.seeSaws.size()); ++index) {
                const auto& v = level.seeSaws[index]; const float extent = v.length * 0.5f + v.thickness;
                collectScene({v.pivot.x - extent, v.pivot.y - extent, extent * 2, extent * 2}, indexed("See Saw", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.trapDoors.size()); ++index) {
                const auto& v = level.trapDoors[index]; const float extent = v.length + v.thickness;
                collectScene({v.hinge.x - extent, v.hinge.y - extent, extent * 2, extent * 2}, indexed("Trap Door", index), index);
            }
            const auto collectFlexible = [&](const auto& objects, const char* label) {
                for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
                    const auto& v = objects[static_cast<size_t>(index)];
                    const float padding = 10.0f;
                    collectScene({std::min(v.start.x, v.end.x) - padding, std::min(v.start.y, v.end.y) - padding,
                        std::fabs(v.end.x - v.start.x) + padding * 2.0f,
                        std::fabs(v.end.y - v.start.y) + padding * 2.0f}, indexed(label, index), index);
                }
            };
            collectFlexible(level.chains, "Chain");
            collectFlexible(level.physicsRopes, "Physics Rope");
            for (int index = 0; index < static_cast<int>(level.arrowTraps.size()); ++index) {
                const Vector2 point = level.arrowTraps[static_cast<size_t>(index)].position;
                collectScene({point.x - 18, point.y - 18, 36, 36}, indexed("Arrow Trap", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.visualTiles.size()); ++index) {
                const Vector2 point = level.visualTiles[static_cast<size_t>(index)].position;
                collectScene({point.x, point.y, 32, 32}, indexed("Visual Tile", index), index);
            }
            for (int index = 0; index < static_cast<int>(level.labels.size()); ++index) {
                const auto& label = level.labels[static_cast<size_t>(index)];
                const int padding = std::max(5, static_cast<int>(roundf(label.fontSize * 0.42f)));
                collectScene({label.position.x, label.position.y,
                    static_cast<float>(MeasureText(label.text.c_str(), label.fontSize) + padding * 2),
                    static_cast<float>(label.fontSize + padding * 2)}, indexed("Label", index), index);
            }
            collectScene({level.valve.center.x - level.valve.radius, level.valve.center.y - level.valve.radius,
                level.valve.radius * 2.0f, level.valve.radius * 2.0f}, "Valve", 0);
            collectScene(level.waterPit.bounds, "Water Pit", 0);
            collectScene(level.spikeHazard, "Spike Hazard", 0);
            collectScene({level.clockFaceCenter.x - level.clockFaceRadius, level.clockFaceCenter.y - level.clockFaceRadius,
                level.clockFaceRadius * 2.0f, level.clockFaceRadius * 2.0f}, "Clock Face", 0);
            SetPrimarySelection(groupSelection.empty() ? Selection{} : groupSelection.back());
            statusText = std::to_string(groupSelection.size()) + " objects selected.";
            if (viewportOnly) NotifySelectionChanged();
        }
        if (draggingTransform && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Vector2 delta{world.x - transformDragStart.x, world.y - transformDragStart.y};
            if (transformMode == TransformMode::Move) {
                Vector2 constrainedDelta = delta;
                if (activeTransformHandle == TransformHandle::MoveX) constrainedDelta.y = 0.0f;
                else if (activeTransformHandle == TransformHandle::MoveY) constrainedDelta.x = 0.0f;
                if (groupSelection.size() > 1) {
                    for (size_t index = 0; index < groupSelection.size(); ++index) {
                        Selection& item = groupSelection[index];
                        if (item.kind == SelectionKind::PlayerStart) {
                            level.playerStart = {transformOriginalGroupPoints[index].x + constrainedDelta.x,
                                transformOriginalGroupPoints[index].y + constrainedDelta.y};
                        }
                        else if (Rectangle* rect = RectangleForSelection(item)) {
                            rect->x = transformOriginalGroupRects[index].x + constrainedDelta.x;
                            rect->y = transformOriginalGroupRects[index].y + constrainedDelta.y;
                        }
                        else if (item.kind == SelectionKind::SceneObject) {
                            SetPrimarySelection(item);
                            const Rectangle original = transformOriginalGroupRects[index];
                            const Vector2 currentOffset{original.x + constrainedDelta.x - selectedSceneBounds.x,
                                original.y + constrainedDelta.y - selectedSceneBounds.y};
                            if (TranslateSelectedScene(currentOffset)) item.sceneBounds = selectedSceneBounds;
                        }
                    }
                    SetPrimarySelection(groupSelection.back());
                }
                else if (selection.kind == SelectionKind::PlayerStart) {
                    level.playerStart = {transformOriginalPoint.x + constrainedDelta.x,
                        transformOriginalPoint.y + constrainedDelta.y};
                }
                else if (Rectangle* rect = SelectedRectangle(); rect != nullptr && selection.kind != SelectionKind::Exit) {
                    rect->x = transformOriginalRect.x + constrainedDelta.x;
                    rect->y = transformOriginalRect.y + constrainedDelta.y;
                }
                else if (selection.kind == SelectionKind::SceneObject) {
                    Vector2 incremental{world.x - transformLastWorld.x, world.y - transformLastWorld.y};
                    if (activeTransformHandle == TransformHandle::MoveX) incremental.y = 0.0f;
                    else if (activeTransformHandle == TransformHandle::MoveY) incremental.x = 0.0f;
                    TranslateSelectedScene(incremental);
                    transformLastWorld = world;
                }
            }
            else if (transformMode == TransformMode::Scale) {
                if (groupSelection.size() > 1 && transformOriginalGroupBounds.width > 0.0f &&
                    transformOriginalGroupBounds.height > 0.0f) {
                    const float newWidth = std::max(gridSnap, transformOriginalGroupBounds.width + delta.x);
                    const float newHeight = std::max(gridSnap, transformOriginalGroupBounds.height + delta.y);
                    float scaleX = newWidth / transformOriginalGroupBounds.width;
                    float scaleY = newHeight / transformOriginalGroupBounds.height;
                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                        const float factorX = delta.x / transformOriginalGroupBounds.width;
                        const float factorY = delta.y / transformOriginalGroupBounds.height;
                        const float uniformScale = fabsf(factorX) >= fabsf(factorY) ? factorX : factorY;
                        scaleX = scaleY = std::max(gridSnap / transformOriginalGroupBounds.width,
                            1.0f + uniformScale);
                    }
                    const bool centerScale = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
                    const float anchorX = transformOriginalGroupBounds.x +
                        (centerScale ? transformOriginalGroupBounds.width * 0.5f : 0.0f);
                    const float anchorY = transformOriginalGroupBounds.y +
                        (centerScale ? transformOriginalGroupBounds.height * 0.5f : 0.0f);
                    for (size_t index = 0; index < groupSelection.size(); ++index) {
                        Selection& item = groupSelection[index];
                        if (item.kind == SelectionKind::PlayerStart) {
                            level.playerStart = {
                                anchorX + (transformOriginalGroupPoints[index].x - anchorX) * scaleX,
                                anchorY + (transformOriginalGroupPoints[index].y - anchorY) * scaleY
                            };
                        }
                        else if (Rectangle* rect = RectangleForSelection(item)) {
                            const Rectangle original = transformOriginalGroupRects[index];
                            rect->x = anchorX + (original.x - anchorX) * scaleX;
                            rect->y = anchorY + (original.y - anchorY) * scaleY;
                            if (item.kind != SelectionKind::Exit) {
                                rect->width = std::max(1.0f, original.width * scaleX);
                                rect->height = std::max(1.0f, original.height * scaleY);
                            }
                        }
                        else if (item.kind == SelectionKind::SceneObject) {
                            const Rectangle original = transformOriginalGroupRects[index];
                            const Rectangle target{
                                anchorX + (original.x - anchorX) * scaleX,
                                anchorY + (original.y - anchorY) * scaleY,
                                std::max(1.0f, original.width * scaleX),
                                std::max(1.0f, original.height * scaleY)
                            };
                            SetPrimarySelection(item);
                            ScaleSelectedScene({target.width - selectedSceneBounds.width,
                                target.height - selectedSceneBounds.height});
                            TranslateSelectedScene({target.x - selectedSceneBounds.x, target.y - selectedSceneBounds.y});
                            item.sceneBounds = selectedSceneBounds;
                        }
                    }
                    SetPrimarySelection(groupSelection.back());
                }
                else if (Rectangle* rect = SelectedRectangle()) {
                    float scaleX = std::max(gridSnap / transformOriginalRect.width,
                        (transformOriginalRect.width + delta.x) / transformOriginalRect.width);
                    float scaleY = std::max(gridSnap / transformOriginalRect.height,
                        (transformOriginalRect.height + delta.y) / transformOriginalRect.height);
                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                        const float factorX = delta.x / transformOriginalRect.width;
                        const float factorY = delta.y / transformOriginalRect.height;
                        const float uniformScale = fabsf(factorX) >= fabsf(factorY) ? factorX : factorY;
                        scaleX = scaleY = std::max(gridSnap / transformOriginalRect.width,
                            1.0f + uniformScale);
                    }
                    rect->width = transformOriginalRect.width * scaleX;
                    rect->height = transformOriginalRect.height * scaleY;
                    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
                        rect->x = transformOriginalRect.x - (rect->width - transformOriginalRect.width) * 0.5f;
                        rect->y = transformOriginalRect.y - (rect->height - transformOriginalRect.height) * 0.5f;
                    }
                }
                else if (selection.kind == SelectionKind::SceneObject) {
                    const Vector2 incremental{world.x - transformLastWorld.x, world.y - transformLastWorld.y};
                    ScaleSelectedScene(incremental);
                    transformLastWorld = world;
                }
            }
            else if (transformMode == TransformMode::Rotate) {
                if (float* rotation = SelectedRotation()) {
                    const Vector2 center = SelectedRotationCenter();
                    const float currentAngle = atan2f(unsnappedWorld.y - center.y,
                        unsnappedWorld.x - center.x) * RAD2DEG;
                    float result = transformOriginalRotation + currentAngle - transformDragStartAngle;
                    const bool limitedAngle = selectedSceneName.rfind("Ramp ", 0) == 0 ||
                        selectedSceneName.rfind("Trap Door ", 0) == 0 ||
                        selectedSceneName.rfind("See Saw ", 0) == 0;
                    if (selectedSceneName.rfind("Ramp ", 0) == 0 || selectedSceneName.rfind("Trap Door ", 0) == 0)
                        result = std::clamp(result, -80.0f, 80.0f);
                    else if (selectedSceneName.rfind("See Saw ", 0) == 0 && selection.index >= 0 &&
                        selection.index < static_cast<int>(level.seeSaws.size())) {
                        const SeeSaw& seeSaw = level.seeSaws[static_cast<size_t>(selection.index)];
                        result = std::clamp(result, seeSaw.minAngle, seeSaw.maxAngle);
                    }
                    else {
                        result = fmodf(result, 360.0f);
                        if (result < 0.0f) result += 360.0f;
                    }
                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                        result = roundf(result / 15.0f) * 15.0f;
                        if (limitedAngle && (selectedSceneName.rfind("Ramp ", 0) == 0 ||
                            selectedSceneName.rfind("Trap Door ", 0) == 0)) result = std::clamp(result, -80.0f, 80.0f);
                        else if (limitedAngle && selection.index >= 0 &&
                            selection.index < static_cast<int>(level.seeSaws.size())) {
                            const SeeSaw& seeSaw = level.seeSaws[static_cast<size_t>(selection.index)];
                            result = std::clamp(result, seeSaw.minAngle, seeSaw.maxAngle);
                        }
                    }
                    *rotation = result;
                    if (selectedSceneName.rfind("Fan ", 0) == 0 && selection.index >= 0 &&
                        selection.index < static_cast<int>(level.fans.size())) {
                        const float radians = result * DEG2RAD;
                        level.fans[static_cast<size_t>(selection.index)].direction = {cosf(radians), sinf(radians)};
                    }
                    statusText = "Rotation: " + std::to_string(static_cast<int>(roundf(result))) + " degrees";
                }
            }
        }
        if (draggingTransform && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingTransform = false;
            if (groupSelection.size() == 1 && selection.kind == SelectionKind::SceneObject)
                groupSelection.front() = selection;
            activeTransformHandle = TransformHandle::None;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            MarkDirty(transformMode == TransformMode::Move ? "Moved selection." :
                (transformMode == TransformMode::Rotate ? "Rotated selection." : "Scaled selection."));
        }
        return;
    }

    if (activeTool == EditTool::PlayerStart && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        PushTileUndo();
        level.playerStart = world;
        selection = {SelectionKind::PlayerStart, 0};
        MarkDirty("Placed player start.");
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        draggingCreate = true;
        dragStart = world;
        dragCurrent = world;
    }
    if (draggingCreate && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        dragCurrent = world;
    }
    if (draggingCreate && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        draggingCreate = false;
        const Rectangle rect = MakeRect(dragStart, world);
        if (activeTool != EditTool::Select && activeTool != EditTool::PlayerStart) PushTileUndo();
        switch (activeTool) {
        case EditTool::Solid:
            level.baseSolids.push_back(rect);
            selection = {SelectionKind::Solid, static_cast<int>(level.baseSolids.size()) - 1};
            MarkDirty("Added solid.");
            break;
        case EditTool::Platform:
            level.pitPlatforms.push_back(rect);
            selection = {SelectionKind::Platform, static_cast<int>(level.pitPlatforms.size()) - 1};
            MarkDirty("Added platform.");
            break;
        case EditTool::Ladder:
            level.ladders.push_back(rect);
            selection = {SelectionKind::Ladder, static_cast<int>(level.ladders.size()) - 1};
            MarkDirty("Added ladder.");
            break;
        case EditTool::CameraZone:
            level.cameraZones.push_back(rect);
            selection = {SelectionKind::CameraZone, static_cast<int>(level.cameraZones.size()) - 1};
            MarkDirty("Added camera zone.");
            break;
        case EditTool::Darkness:
            level.darknessAreas.push_back(rect);
            selection = {SelectionKind::Darkness, static_cast<int>(level.darknessAreas.size()) - 1};
            MarkDirty("Added darkness region.");
            break;
        case EditTool::Exit:
            level.exitTrigger = {dragStart.x, dragStart.y, StandardExitDoorWidth, StandardExitDoorHeight};
            selection = {SelectionKind::Exit, 0};
            MarkDirty("Placed exit trigger.");
            break;
        case EditTool::Water:
        case EditTool::Sand:
        case EditTool::Gel:
        case EditTool::Gas: {
            FluidField fluid{};
            fluid.type = activeTool == EditTool::Sand ? FluidType::Sand :
                (activeTool == EditTool::Gel ? FluidType::Gel :
                (activeTool == EditTool::Gas ? FluidType::Gas : FluidType::Water));
            fluid.bounds = rect;
            fluid.particleSpacing = fluid.type == FluidType::Gas ? 18.0f :
                (fluid.type == FluidType::Gel ? 14.0f : 8.0f);
            fluid.initialFill = 0.65f;
            fluid.flowSpeed = 1.0f;
            InitializeFluidField(fluid, BuildSolids(level), FluidSimulationMode::Tile);
            level.fluids.push_back(std::move(fluid));
            selection = {SelectionKind::SceneObject, static_cast<int>(level.fluids.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.fluids.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Pulley:
            level.pulleys.push_back(dragStart);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.pulleys.size()) - 1};
            selectedSceneName = "Pulley " + std::to_string(level.pulleys.size());
            MarkDirty("Added Pulley.");
            break;
        case EditTool::HangingWeight: {
            auto pulley = std::find_if(level.pulleys.begin(), level.pulleys.end(), [&](Vector2 point) {
                return fabsf(point.x - dragStart.x) < 0.5f && fabsf(point.y - dragStart.y) < 0.5f;
            });
            if (pulley == level.pulleys.end()) {
                level.pulleys.push_back(dragStart);
                pulley = level.pulleys.end() - 1;
            }
            HangingWeight weight{};
            weight.pulley = *pulley;
            weight.pulleyRadius = 42.0f;
            weight.phase = 0.0f;
            weight.speed = 1.0f;
            weight.rect = {world.x - 22.0f, world.y - 27.5f, 44.0f, 55.0f};
            level.weights.push_back(weight);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.weights.size()) - 1};
            selectedSceneName = "Hanging Weight " + std::to_string(level.weights.size());
            MarkDirty("Added Hanging Weight.");
            break;
        }
        case EditTool::RotaryLatch: {
            RotaryLatch latch{};
            latch.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            latch.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            latch.angle = 0.0f;
            latch.targetAngle = 270.0f;
            latch.tolerance = 8.0f;
            latch.spinSpeed = 120.0f;
            level.rotaryLatches.push_back(latch);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.rotaryLatches.size()) - 1};
            selectedSceneName = "Rotary Latch " + std::to_string(level.rotaryLatches.size());
            MarkDirty("Added Rotary Latch.");
            break;
        }
        case EditTool::StoneBlock: {
            StoneBlock block{};
            block.rect = rect;
            block.mass = 3.0f;
            level.stoneBlocks.push_back(block);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.stoneBlocks.size()) - 1};
            selectedSceneName = "Stone Block " + std::to_string(level.stoneBlocks.size());
            MarkDirty("Added Stone Block.");
            break;
        }
        case EditTool::Boulder: {
            Boulder boulder{};
            boulder.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            boulder.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            boulder.mass = 2.6f;
            level.boulders.push_back(boulder);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.boulders.size()) - 1};
            selectedSceneName = "Boulder " + std::to_string(level.boulders.size());
            MarkDirty("Added Boulder.");
            break;
        }
        case EditTool::PhysicsWheel: {
            PhysicsWheel wheel{};
            wheel.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            wheel.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            wheel.mass = 1.4f;
            level.physicsWheels.push_back(wheel);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.physicsWheels.size()) - 1};
            selectedSceneName = "Physics Wheel " + std::to_string(level.physicsWheels.size());
            MarkDirty("Added Physics Wheel.");
            break;
        }
        case EditTool::Gear: {
            Gear gear{};
            gear.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            gear.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            gear.mass = 1.8f;
            gear.toothCount = std::max(8, static_cast<int>(gear.radius * 1.25f));
            level.gears.push_back(gear);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.gears.size()) - 1};
            selectedSceneName = "Gear " + std::to_string(level.gears.size());
            MarkDirty("Added Gear.");
            break;
        }
        case EditTool::Flywheel: {
            Flywheel flywheel{};
            flywheel.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            flywheel.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            flywheel.mass = 4.0f;
            flywheel.angularVelocity = 80.0f;
            level.flywheels.push_back(flywheel);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.flywheels.size()) - 1};
            selectedSceneName = "Flywheel " + std::to_string(level.flywheels.size());
            MarkDirty("Added Flywheel.");
            break;
        }
        case EditTool::SteeringWheel: {
            SteeringWheel wheel{};
            wheel.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            wheel.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            level.steeringWheels.push_back(wheel);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.steeringWheels.size()) - 1};
            selectedSceneName = "Steering Wheel " + std::to_string(level.steeringWheels.size());
            MarkDirty("Added Steering Wheel.");
            break;
        }
        case EditTool::Screw: {
            Screw screw{};
            screw.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            screw.length = std::max(gridSnap, rect.width);
            screw.radius = std::max(8.0f, rect.height * 0.5f);
            screw.angle = 0.0f;
            screw.spinSpeed = 180.0f;
            level.screws.push_back(screw);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.screws.size()) - 1};
            selectedSceneName = "Screw " + std::to_string(level.screws.size());
            MarkDirty("Added Screw.");
            break;
        }
        case EditTool::Fan: {
            Fan fan{};
            fan.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            fan.direction = {1.0f, 0.0f};
            fan.length = std::max(gridSnap, rect.width);
            fan.width = std::max(gridSnap, rect.height);
            fan.strength = 360.0f;
            fan.power = 1.0f;
            level.fans.push_back(fan);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.fans.size()) - 1};
            selectedSceneName = "Fan " + std::to_string(level.fans.size());
            MarkDirty("Added Fan.");
            break;
        }
        case EditTool::Pinwheel: {
            Pinwheel pinwheel{};
            pinwheel.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            pinwheel.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            level.pinwheels.push_back(pinwheel);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.pinwheels.size()) - 1};
            selectedSceneName = "Pinwheel " + std::to_string(level.pinwheels.size());
            MarkDirty("Added Pinwheel.");
            break;
        }
        case EditTool::Ramp: {
            Ramp ramp{};
            ramp.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            ramp.length = std::max(gridSnap, rect.width);
            ramp.thickness = std::max(8.0f, rect.height);
            ramp.angle = -20.0f;
            ramp.segmentCount = 4;
            level.ramps.push_back(ramp);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.ramps.size()) - 1};
            selectedSceneName = "Ramp " + std::to_string(level.ramps.size());
            MarkDirty("Added Ramp.");
            break;
        }
        case EditTool::SeeSaw: {
            SeeSaw seeSaw{};
            seeSaw.pivot = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            seeSaw.length = std::max(gridSnap, rect.width);
            seeSaw.thickness = std::max(8.0f, rect.height);
            seeSaw.minAngle = -18.0f;
            seeSaw.maxAngle = 18.0f;
            seeSaw.response = 7.0f;
            level.seeSaws.push_back(seeSaw);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.seeSaws.size()) - 1};
            selectedSceneName = "See Saw " + std::to_string(level.seeSaws.size());
            MarkDirty("Added See Saw.");
            break;
        }
        case EditTool::TrapDoor: {
            TrapDoor trapDoor{};
            trapDoor.hinge = {rect.x, rect.y + rect.height * 0.5f};
            trapDoor.length = std::max(gridSnap, rect.width);
            trapDoor.thickness = std::max(8.0f, rect.height);
            trapDoor.angle = 0.0f;
            level.trapDoors.push_back(trapDoor);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.trapDoors.size()) - 1};
            selectedSceneName = "Trap Door " + std::to_string(level.trapDoors.size());
            MarkDirty("Added Trap Door.");
            break;
        }
        case EditTool::Chain: {
            Chain chain{};
            chain.start = dragStart;
            chain.end = world;
            chain.spacing = 12.0f;
            chain.scale = 1.0f;
            chain.pinStart = true;
            chain.pinEnd = true;
            InitializeChain(chain);
            level.chains.push_back(std::move(chain));
            selection = {SelectionKind::SceneObject, static_cast<int>(level.chains.size()) - 1};
            selectedSceneName = "Chain " + std::to_string(level.chains.size());
            MarkDirty("Added Chain.");
            break;
        }
        case EditTool::PhysicsRope: {
            PhysicsRope rope{};
            rope.start = dragStart;
            rope.end = world;
            const float dx = world.x - dragStart.x;
            const float dy = world.y - dragStart.y;
            rope.length = std::max(gridSnap, sqrtf(dx * dx + dy * dy));
            rope.thickness = 4.0f;
            rope.pinStart = true;
            rope.pinEnd = true;
            InitializePhysicsRope(rope);
            level.physicsRopes.push_back(std::move(rope));
            selection = {SelectionKind::SceneObject, static_cast<int>(level.physicsRopes.size()) - 1};
            selectedSceneName = "Physics Rope " + std::to_string(level.physicsRopes.size());
            MarkDirty("Added Physics Rope.");
            break;
        }
        case EditTool::Button: {
            Button button{};
            button.rect = rect;
            level.buttons.push_back(button);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.buttons.size()) - 1};
            selectedSceneName = "Button " + std::to_string(level.buttons.size());
            MarkDirty("Added Button.");
            break;
        }
        case EditTool::Portal: {
            constexpr float portalWidth = 48.0f;
            constexpr float portalHeight = 64.0f;
            PortalPair pair{};
            pair.entrance = {dragStart.x - portalWidth * 0.5f, dragStart.y - portalHeight * 0.5f,
                portalWidth, portalHeight};
            pair.exit = {world.x - portalWidth * 0.5f, world.y - portalHeight * 0.5f,
                portalWidth, portalHeight};
            level.portalPairs.push_back(pair);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.portalPairs.size()) - 1};
            selectedSceneName = "Portal " + std::to_string(level.portalPairs.size());
            MarkDirty("Added Portal pair.");
            break;
        }
        case EditTool::DirectionalSpikes: {
            DirectionalSpikeHazard hazard{};
            hazard.rect = rect;
            hazard.direction = SpikeDirection::Up;
            level.directionalSpikeHazards.push_back(hazard);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.directionalSpikeHazards.size()) - 1};
            selectedSceneName = "Directional Spikes " + std::to_string(level.directionalSpikeHazards.size());
            MarkDirty("Added Directional Spikes.");
            break;
        }
        case EditTool::ArrowTrap: {
            const Vector2 delta{world.x - dragStart.x, world.y - dragStart.y};
            ArrowTrap trap{};
            trap.position = dragStart;
            if (fabsf(delta.x) >= fabsf(delta.y)) trap.direction = {delta.x < 0.0f ? -1.0f : 1.0f, 0.0f};
            else trap.direction = {0.0f, delta.y < 0.0f ? -1.0f : 1.0f};
            trap.interval = 1.5f;
            trap.speed = 420.0f;
            level.arrowTraps.push_back(trap);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.arrowTraps.size()) - 1};
            selectedSceneName = "Arrow Trap " + std::to_string(level.arrowTraps.size());
            MarkDirty("Added Arrow Trap.");
            break;
        }
        case EditTool::BreakableTile: {
            BreakableTile tile{};
            tile.rect = rect;
            tile.breakDelay = 2.0f;
            level.breakableTiles.push_back(tile);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.breakableTiles.size()) - 1};
            selectedSceneName = "Breakable Tile " + std::to_string(level.breakableTiles.size());
            MarkDirty("Added Breakable Tile.");
            break;
        }
        case EditTool::Enemy: {
            Enemy enemy{};
            enemy.rect = rect;
            enemy.patrolMinX = rect.x - 128.0f;
            enemy.patrolMaxX = rect.x + rect.width + 128.0f;
            enemy.speed = 70.0f;
            level.enemies.push_back(enemy);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.enemies.size()) - 1};
            selectedSceneName = "Enemy " + std::to_string(level.enemies.size());
            MarkDirty("Added Enemy.");
            break;
        }
        case EditTool::Label: {
            LevelLabel label{};
            label.position = dragStart;
            label.fontSize = 24;
            label.text = "New Label";
            level.labels.push_back(label);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.labels.size()) - 1};
            selectedSceneName = "Label " + std::to_string(level.labels.size());
            MarkDirty("Added Label.");
            break;
        }
        case EditTool::Valve: {
            level.valve.center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            level.valve.radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
            level.valve.turnDegrees = 0.0f;
            level.valve.opened = false;
            selection = {SelectionKind::SceneObject, 0};
            selectedSceneName = "Valve 1";
            MarkDirty("Placed Valve.");
            break;
        }
        case EditTool::Checkpoint:
        case EditTool::Collectible: {
            GuideObject object{};
            std::ostringstream arguments;
            if (activeTool == EditTool::Checkpoint) {
                arguments << rect.x << ' ' << rect.y << ' ' << rect.width << ' ' << rect.height;
            }
            else {
                const float radius = std::max(gridSnap * 0.25f, std::min(rect.width, rect.height) * 0.5f);
                arguments << rect.x + rect.width * 0.5f << ' ' << rect.y + rect.height * 0.5f << ' ' << radius;
            }
            std::istringstream stream(arguments.str());
            ParseGuideObject(activeTool == EditTool::Checkpoint ? "checkpoint" : "collectible", stream, object);
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Ball:
        case EditTool::Barrel:
        case EditTool::MovingPlatform:
        case EditTool::Elevator: {
            GuideObject object{};
            const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            std::ostringstream arguments;
            const char* command = "ball";
            if (activeTool == EditTool::Ball) {
                const float radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
                arguments << center.x << ' ' << center.y << ' ' << radius << " 1";
            }
            else if (activeTool == EditTool::Barrel) {
                command = "barrel";
                arguments << center.x << ' ' << center.y << ' ' << rect.width << ' ' << rect.height << " 2";
            }
            else {
                const bool elevator = activeTool == EditTool::Elevator;
                command = elevator ? "elevator" : "movingPlatform";
                arguments << center.x << ' ' << center.y << ' ' << rect.width << ' ' << rect.height << ' ' <<
                    (elevator ? 0 : 1) << ' ' << (elevator ? -1 : 0) << ' ' <<
                    std::max(96.0f, elevator ? rect.height * 3.0f : rect.width * 2.0f) << " 1";
            }
            std::istringstream stream(arguments.str());
            ParseGuideObject(command, stream, object);
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::PendulumBob:
        case EditTool::OneWayPlatform:
        case EditTool::CeilingHook:
        case EditTool::GuideRail: {
            GuideObject object{};
            std::ostringstream arguments;
            const char* command = "pendulumBob";
            if (activeTool == EditTool::PendulumBob) {
                const float dx = world.x - dragStart.x;
                const float dy = world.y - dragStart.y;
                const float length = std::max(gridSnap, sqrtf(dx * dx + dy * dy));
                const float swingAngle = std::clamp(atan2f(dx, dy), -0.78f, 0.78f);
                const float phase = asinf(std::clamp(swingAngle / 0.78f, -1.0f, 1.0f));
                arguments << dragStart.x << ' ' << dragStart.y << ' ' << length << " 16 " << phase << " 1";
            }
            else if (activeTool == EditTool::CeilingHook) {
                command = "ceilingHook";
                arguments << dragStart.x << ' ' << dragStart.y << " 16";
            }
            else {
                command = activeTool == EditTool::OneWayPlatform ? "oneWayPlatform" : "guideRail";
                const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
                arguments << center.x << ' ' << center.y << ' ' << rect.width << ' ' << rect.height;
            }
            std::istringstream stream(arguments.str());
            ParseGuideObject(command, stream, object);
            if (activeTool == EditTool::PendulumBob) object.transform.position = world;
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Spring:
        case EditTool::CompressionSpring:
        case EditTool::ExtensionSpring:
        case EditTool::TorsionSpring:
        case EditTool::GarterSpring:
        case EditTool::VoluteSpring:
        case EditTool::SpiralSpring:
        case EditTool::ConstantForceSpring:
        case EditTool::ConstantTorqueSpring:
        case EditTool::LeafSpring:
        case EditTool::BeamSpring:
        case EditTool::DiscSpring:
        case EditTool::WaveSpring:
        case EditTool::WaveWasher:
        case EditTool::TorsionBar:
        case EditTool::RingSpring:
        case EditTool::ElastomerSpring:
        case EditTool::PneumaticSpring:
        case EditTool::GasSpring:
        case EditTool::HydropneumaticSpring:
        case EditTool::MagneticSpring:
        case EditTool::CompositeSpring: {
            const char* command = activeTool == EditTool::Spring ? "spring" :
                activeTool == EditTool::CompressionSpring ? "compressionSpring" :
                activeTool == EditTool::ExtensionSpring ? "extensionSpring" :
                activeTool == EditTool::TorsionSpring ? "torsionSpring" :
                activeTool == EditTool::GarterSpring ? "garterSpring" :
                activeTool == EditTool::VoluteSpring ? "voluteSpring" :
                activeTool == EditTool::SpiralSpring ? "spiralSpring" :
                activeTool == EditTool::ConstantForceSpring ? "constantForceSpring" :
                activeTool == EditTool::ConstantTorqueSpring ? "constantTorqueSpring" :
                activeTool == EditTool::LeafSpring ? "leafSpring" :
                activeTool == EditTool::BeamSpring ? "beamSpring" :
                activeTool == EditTool::DiscSpring ? "discSpring" :
                activeTool == EditTool::WaveSpring ? "waveSpring" :
                activeTool == EditTool::WaveWasher ? "waveWasher" :
                activeTool == EditTool::TorsionBar ? "torsionBar" :
                activeTool == EditTool::RingSpring ? "ringSpring" :
                activeTool == EditTool::ElastomerSpring ? "elastomerSpring" :
                activeTool == EditTool::PneumaticSpring ? "pneumaticSpring" :
                activeTool == EditTool::GasSpring ? "gasSpring" :
                activeTool == EditTool::HydropneumaticSpring ? "hydropneumaticSpring" :
                activeTool == EditTool::MagneticSpring ? "magneticSpring" : "compositeSpring";
            const float stiffness = activeTool == EditTool::DiscSpring ? 18.0f :
                (activeTool == EditTool::TorsionBar ? 15.0f :
                (activeTool == EditTool::RingSpring ? 13.0f :
                (activeTool == EditTool::HydropneumaticSpring ? 14.0f :
                (activeTool == EditTool::PneumaticSpring ? 11.0f :
                (activeTool == EditTool::MagneticSpring ? 12.0f :
                (activeTool == EditTool::CompositeSpring ? 10.0f :
                (activeTool == EditTool::VoluteSpring ? 14.0f :
                (activeTool == EditTool::LeafSpring ? 12.0f :
                (activeTool == EditTool::BeamSpring || activeTool == EditTool::WaveSpring || activeTool == EditTool::GasSpring ? 10.0f :
                (activeTool == EditTool::TorsionSpring ? 10.0f :
                (activeTool == EditTool::GarterSpring || activeTool == EditTool::SpiralSpring || activeTool == EditTool::ElastomerSpring ? 9.0f :
                (activeTool == EditTool::WaveWasher ? 7.0f : 8.0f))))))))))));
            const float damping = activeTool == EditTool::TorsionSpring ? 1.8f :
                (activeTool == EditTool::LeafSpring ? 2.0f :
                (activeTool == EditTool::BeamSpring ? 1.7f :
                (activeTool == EditTool::SpiralSpring ? 1.4f :
                (activeTool == EditTool::ConstantTorqueSpring ? 1.2f :
                (activeTool == EditTool::TorsionBar ? 2.2f :
                (activeTool == EditTool::RingSpring ? 4.2f :
                (activeTool == EditTool::ElastomerSpring ? 4.8f :
                (activeTool == EditTool::PneumaticSpring ? 2.4f :
                (activeTool == EditTool::GasSpring ? 3.4f :
                (activeTool == EditTool::HydropneumaticSpring ? 5.2f :
                (activeTool == EditTool::MagneticSpring ? 0.4f :
                (activeTool == EditTool::CompositeSpring ? 1.6f : 1.0f))))))))))));
            std::ostringstream arguments;
            arguments << dragStart.x << ' ' << dragStart.y << ' ' << world.x << ' ' << world.y <<
                " 7 " << stiffness << ' ' << damping;
            GuideObject object{};
            std::istringstream stream(arguments.str());
            ParseGuideObject(command, stream, object);
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Rod:
        case EditTool::FixedJoint: {
            const char* command = activeTool == EditTool::Rod ? "rod" : "fixedJoint";
            std::ostringstream arguments;
            if (activeTool == EditTool::Rod) {
                arguments << dragStart.x << ' ' << dragStart.y << ' ' << world.x << ' ' << world.y << " 6";
            }
            else {
                arguments << dragStart.x << ' ' << dragStart.y << " 10";
            }
            GuideObject object{};
            std::istringstream stream(arguments.str());
            ParseGuideObject(command, stream, object);
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Crank:
        case EditTool::Ratchet:
        case EditTool::Clutch:
        case EditTool::Brake: {
            GuideObject object{};
            const Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            std::ostringstream arguments;
            const char* command = activeTool == EditTool::Crank ? "crank" :
                activeTool == EditTool::Ratchet ? "ratchet" :
                activeTool == EditTool::Clutch ? "clutch" : "brake";
            if (activeTool == EditTool::Brake) {
                arguments << center.x << ' ' << center.y << ' ' << rect.width << ' ' << rect.height << " 0.72 0";
            }
            else {
                const float radius = std::max(gridSnap * 0.5f, std::min(rect.width, rect.height) * 0.5f);
                const float speed = activeTool == EditTool::Crank ? 105.0f :
                    (activeTool == EditTool::Ratchet ? 90.0f : 100.0f);
                arguments << center.x << ' ' << center.y << ' ' << radius << ' ' << speed;
                if (activeTool == EditTool::Clutch) arguments << " 1";
                arguments << " 0";
            }
            std::istringstream stream(arguments.str());
            ParseGuideObject(command, stream, object);
            level.guideObjects.push_back(object);
            selection = {SelectionKind::SceneObject, static_cast<int>(level.guideObjects.size()) - 1};
            selectedSceneName = std::string(ToolName(activeTool)) + " " + std::to_string(level.guideObjects.size());
            MarkDirty(std::string("Added ") + ToolName(activeTool) + ".");
            break;
        }
        case EditTool::Select:
        case EditTool::PlayerStart:
            break;
        }
    }
}

void LevelEditor::Draw() const {
    BeginDrawing();
    ClearBackground(WindowBackground);

    DrawCanvas(GetCanvasBounds());
    if (viewportOnly) {
        EndDrawing();
        return;
    }
    DrawToolbar();
    const auto drawPanel = [&](PanelId panel) {
        const Rectangle bounds = GetPanelBounds(panel);
        switch (panel) {
        case PanelId::PlaceAssets: DrawAssetLibrary(bounds); break;
        case PanelId::ContentBrowser: DrawContentBrowser(bounds); break;
        case PanelId::WorldOutliner: DrawWorldOutliner(bounds); break;
        case PanelId::Details: DrawDetails(bounds); break;
        case PanelId::Count: break;
        }
    };
    for (size_t index = 0; index < dockPanels.size(); ++index) {
        if (dockPanels[index].slot != DockSlot::Floating) drawPanel(static_cast<PanelId>(index));
    }
    for (size_t index = 0; index < dockPanels.size(); ++index) {
        if (dockPanels[index].slot == DockSlot::Floating) drawPanel(static_cast<PanelId>(index));
    }
    DrawDockingOverlay();
    DrawStatusBar();

    EndDrawing();
}

void LevelEditor::DrawCanvas(Rectangle canvas) const {
    DrawRectangleRec(canvas, CanvasBackground);
    BeginScissorMode(static_cast<int>(canvas.x), static_cast<int>(canvas.y),
        static_cast<int>(canvas.width), static_cast<int>(canvas.height));
    BeginMode2D(camera);

    const Vector2 topLeft = GetScreenToWorld2D({canvas.x, canvas.y}, camera);
    const Vector2 bottomRight = GetScreenToWorld2D({canvas.x + canvas.width, canvas.y + canvas.height}, camera);
    const float gridSize = camera.zoom < 0.18f ? 160.0f : (camera.zoom < 0.4f ? 64.0f : 32.0f);
    const float lineWidth = 1.0f / camera.zoom;
    const float startX = floorf(topLeft.x / gridSize) * gridSize;
    const float startY = floorf(topLeft.y / gridSize) * gridSize;

    if (showGrid) {
        for (float x = startX; x <= bottomRight.x; x += gridSize) {
            const bool major = static_cast<int>(roundf(x / gridSize)) % 5 == 0;
            DrawLineEx({x, topLeft.y}, {x, bottomRight.y}, lineWidth,
                major ? Color{48, 55, 66, 170} : Color{35, 40, 49, 150});
        }
        for (float y = startY; y <= bottomRight.y; y += gridSize) {
            const bool major = static_cast<int>(roundf(y / gridSize)) % 5 == 0;
            DrawLineEx({topLeft.x, y}, {bottomRight.x, y}, lineWidth,
                major ? Color{48, 55, 66, 170} : Color{35, 40, 49, 150});
        }
    }

    DrawLevelPreview();
    DrawEditOverlay();
    EndMode2D();
    EndScissorMode();
    DrawRectangleLinesEx(canvas, 1.0f, PanelBorder);

    const Rectangle viewportStrip{canvas.x + 8.0f, canvas.y + 8.0f, 166.0f, 30.0f};
    DrawRectangleRounded(viewportStrip, 0.16f, 5, Color{31, 35, 42, 238});
    DrawUiText(uiFont, "LIT", viewportStrip.x + 10.0f, viewportStrip.y + 7.0f, 14.0f, SecondaryText);
    DrawUiText(uiFont, "SHOW", viewportStrip.x + 50.0f, viewportStrip.y + 7.0f, 14.0f, SecondaryText);
    DrawUiText(uiFont, TextFormat("SNAP %.0f", gridSnap), viewportStrip.x + 102.0f, viewportStrip.y + 7.0f,
        14.0f, Warning);
}

int LevelEditor::SelectedConnectionNode(const EditorConnections& graph) const {
    if (selection.kind != SelectionKind::SceneObject) return -1;
    for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node) {
        const auto& item = graph.nodes[node];
        if (item.index != selection.index) continue;
        const char* prefix = item.kind == ConnectionNodeKind::GuideObject ? "Guide Object " :
            item.kind == ConnectionNodeKind::Pulley ? "Pulley " : "Hanging Weight ";
        if (selectedSceneName.rfind(prefix, 0) == 0) return node;
    }
    return -1;
}

LevelEditor::Selection LevelEditor::ConnectionSelection(const ConnectionNode& node) const {
    const std::string prefix = node.kind == ConnectionNodeKind::GuideObject ? "Guide Object" :
        node.kind == ConnectionNodeKind::Pulley ? "Pulley" : "Hanging Weight";
    std::string outliner = prefix;
    int ordinal = node.index;
    if (node.kind == ConnectionNodeKind::GuideObject) {
        const auto type = level.guideObjects[node.index].type;
        outliner = GetGuideObjectName(type);
        ordinal = -1;
        for (int index = 0; index <= node.index; ++index)
            if (level.guideObjects[index].type == type) ++ordinal;
    }
    return {SelectionKind::SceneObject, node.index, prefix + " " + std::to_string(node.index + 1),
        node.bounds, outliner, ordinal};
}

void LevelEditor::SelectConnectedObjects() {
    const auto graph = BuildEditorConnections(level);
    const int node = SelectedConnectionNode(graph);
    const auto connected = ConnectedEditorNodes(graph, node);
    if (connected.empty()) { NotifyTileOperation("toolstatus Select a power, pulley, or attachment object first"); return; }
    groupSelection.clear();
    for (int member : connected) groupSelection.push_back(ConnectionSelection(graph.nodes[member]));
    SetPrimarySelection(groupSelection.front());
    showConnections = true;
    NotifySelectionChanged();
    NotifyTileOperation("toolstatus Selected " + std::to_string(connected.size()) + " connected objects");
}

bool LevelEditor::PickConnectionEndpoint(Vector2 world) {
    const auto graph = BuildEditorConnections(level);
    const int selected = SelectedConnectionNode(graph);
    const auto members = ConnectedEditorNodes(graph, selected);
    const auto visible = [&](int node) {
        return !focusConnections || selected < 0 || std::find(members.begin(), members.end(), node) != members.end();
    };
    int picked = -1;
    float bestDistanceSq = powf(12.0f / camera.zoom, 2.0f);
    const auto consider = [&](Vector2 point, int node) {
        if (!visible(node)) return;
        const float dx = point.x - world.x, dy = point.y - world.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq <= bestDistanceSq) { bestDistanceSq = distanceSq; picked = node; }
    };
    for (const auto& link : graph.links) { consider(link.start, link.from); consider(link.end, link.to); }
    for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node) consider(graph.nodes[node].position, node);
    if (picked < 0) return false;
    groupSelection.clear();
    SetPrimarySelection(ConnectionSelection(graph.nodes[picked]));
    activeTool = EditTool::Select;
    tilePaintTool = TilePaintTool::None;
    transformMode = TransformMode::Select;
    NotifySelectionChanged();
    NotifyTileOperation("toolstatus " + graph.nodes[picked].label +
        (graph.nodes[picked].warning.empty() ? "" : " - " + graph.nodes[picked].warning));
    return true;
}

void LevelEditor::DrawConnectionOverlay() const {
    const auto graph = BuildEditorConnections(level);
    const int selected = SelectedConnectionNode(graph);
    const auto members = ConnectedEditorNodes(graph, selected);
    const auto visible = [&](int node) {
        return !focusConnections || selected < 0 || std::find(members.begin(), members.end(), node) != members.end();
    };
    const float unit = 1.0f / camera.zoom;
    const Color powerColor{225, 192, 83, 255}, attachmentColor{83, 214, 180, 255}, pathColor{97, 172, 255, 255};
    const Color warningColor{255, 99, 99, 255};
    const auto dashed = [&](Vector2 start, Vector2 end, Color color) {
        const float dx = end.x - start.x, dy = end.y - start.y;
        const float distance = sqrtf(dx * dx + dy * dy);
        if (distance < 0.001f) return;
        for (float step = 0; step < distance; step += 14.0f * unit) {
            const float finish = std::min(distance, step + 8.0f * unit);
            DrawLineEx({start.x + dx * step / distance, start.y + dy * step / distance},
                {start.x + dx * finish / distance, start.y + dy * finish / distance}, 2.0f * unit, color);
        }
    };
    for (const auto& link : graph.links) {
        if (!visible(link.from) && !visible(link.to)) continue;
        const Color color = link.kind == ConnectionLinkKind::Power
            ? (graph.nodes[link.to].warning.empty() ? powerColor : warningColor)
            : link.kind == ConnectionLinkKind::Attachment ? attachmentColor : pathColor;
        dashed(link.start, link.end, Fade(color, 0.85f));
        DrawCircleLinesV(link.start, 6 * unit, color);
        DrawCircleLinesV(link.end, 6 * unit, color);
        if (link.kind == ConnectionLinkKind::Power) {
            const Vector2 midpoint{(link.start.x + link.end.x) * 0.5f, (link.start.y + link.end.y) * 0.5f};
            const auto text = "CH " + std::to_string(link.channel);
            DrawUiText(uiFont, text.c_str(), midpoint.x + 5 * unit, midpoint.y, 12 * unit, color);
        }
    }
    for (const auto& path : graph.paths) {
        if (!visible(path.node)) continue;
        const bool attachment = IsGuideAttachmentConstraint(level.guideObjects[path.node].type);
        const Color color = attachment ? attachmentColor : pathColor;
        for (size_t index = 1; index < path.points.size(); ++index) dashed(path.points[index - 1], path.points[index], color);
        if (!path.points.empty()) {
            DrawCircleLinesV(path.points.front(), 5 * unit, color);
            DrawCircleLinesV(path.points.back(), 5 * unit,
                graph.nodes[path.node].warning.empty() ? color : warningColor);
            const auto point = path.points.back();
            DrawUiText(uiFont, attachment ? "Anchor" : "Travel", point.x + 8 * unit, point.y, 12 * unit, color);
        }
    }
    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node) {
        if (!visible(node)) continue;
        const auto& item = graph.nodes[node];
        if (node == selected && item.kind == ConnectionNodeKind::GuideObject) {
            const auto& object = level.guideObjects[item.index];
            if (IsGuideAttachmentConstraint(object.type)) {
                const auto anchor = object.type == GuideObjectType::FixedJoint
                    ? object.transform.position : object.constraint.anchorB;
                DrawCircleLinesV(anchor, 42.0f, Fade(item.warning.empty() ? attachmentColor : warningColor, 0.6f));
            }
        }
        // Unrelated guide objects do not need connection badges.
        bool relevant = !item.warning.empty() || item.kind != ConnectionNodeKind::GuideObject ||
            level.guideObjects[item.index].power.channel != 0;
        if (!relevant)
            for (const auto& path : graph.paths) if (path.node == node) relevant = true;
        if (!relevant)
            for (const auto& link : graph.links) if (link.from == node || link.to == node) relevant = true;
        if (!relevant) continue;
        const Color color = item.warning.empty() ? attachmentColor : warningColor;
        DrawCircleV(item.position, 4 * unit, color);
        DrawCircleLinesV(item.position, 8 * unit, color);
        if (!item.warning.empty()) DrawUiText(uiFont, "!", item.position.x + 9 * unit, item.position.y - 8 * unit, 15 * unit, warningColor);
        if (node == selected || CheckCollisionPointCircle(mouse, item.position, 12 * unit)) {
            std::string text = item.label;
            if (!item.warning.empty()) text += " - " + item.warning;
            const float width = MeasureTextEx(uiFont, text.c_str(), 13 * unit, 0).x;
            DrawRectangleRec({item.position.x + 12 * unit, item.position.y - 25 * unit, width + 12 * unit, 21 * unit}, Color{15, 18, 23, 240});
            DrawUiText(uiFont, text.c_str(), item.position.x + 17 * unit, item.position.y - 22 * unit, 13 * unit, color);
        }
    }
    const auto canvas = GetCanvasBounds();
    const Vector2 legend = GetScreenToWorld2D({canvas.x + 12, canvas.y + 12}, camera);
    DrawRectangleRec({legend.x - 4 * unit, legend.y - 3 * unit, 440 * unit, 42 * unit}, Color{15, 18, 23, 225});
    DrawUiText(uiFont, "Connections: gold = channel, green = attachment, blue = path", legend.x, legend.y, 12 * unit, PrimaryText);
    DrawUiText(uiFont, "Red = unresolved. Alt+click an endpoint to select it.", legend.x, legend.y + 19 * unit, 12 * unit, SecondaryText);
}

void LevelEditor::DrawEditOverlay() const {
    if (showConnections) DrawConnectionOverlay();
    const float lineWidth = std::max(2.0f / camera.zoom, 1.0f);
    if (viewportOnly && tilePaintTool != TilePaintTool::None) {
        const Vector2 mouse = GetMousePosition();
        const Rectangle canvas = GetCanvasBounds();
        if (CheckCollisionPointRec(mouse, canvas)) {
            const Vector2 world = GetScreenToWorld2D(mouse, camera);
            const int gridX = static_cast<int>(floorf(world.x / 32.0f));
            const int gridY = static_cast<int>(floorf(world.y / 32.0f));
            const bool stampSelection = tilePaintTool == TilePaintTool::Pencil || tilePaintTool == TilePaintTool::Brush ||
                tilePaintTool == TilePaintTool::RandomBrush || tilePaintTool == TilePaintTool::Eraser;
            const int brushSize = tilePaintTool == TilePaintTool::Pencil ? 1 : paintBrushSize;
            const int footprintWidth = (stampSelection ? paintTileSelectionWidth : 1) * brushSize;
            const int footprintHeight = (stampSelection ? paintTileSelectionHeight : 1) * brushSize;
            const int offsetX = (footprintWidth - 1) / 2;
            const int offsetY = (footprintHeight - 1) / 2;
            const bool collisionCursor = paintTileLayer == TileLayer::Collision && tilePaintTool != TilePaintTool::TileSelect &&
                tilePaintTool != TilePaintTool::Eyedropper && tilePaintTool != TilePaintTool::Measure &&
                tilePaintTool != TilePaintTool::MagicWand && tilePaintTool != TilePaintTool::SelectMatching;
            const Color footprintColor = collisionCursor || tilePaintTool == TilePaintTool::Eraser || tilePaintTool == TilePaintTool::CollisionBrush
                ? Color{255, 92, 92, 255} : (tilePaintTool == TilePaintTool::TileSelect ? Color{83, 214, 180, 255} :
                    (tilePaintTool == TilePaintTool::Eyedropper ? Color{184, 132, 255, 255} :
                        (tilePaintTool == TilePaintTool::Measure ? Color{76, 205, 255, 255} : Color{255, 196, 74, 255})));
            const Texture2D sheet = paintTileSheet == 2 ? industrialFarBackground :
                (paintTileSheet == 1 ? industrialBackground : industrialTiles);

            for (int y = 0; y < footprintHeight; ++y) {
                for (int x = 0; x < footprintWidth; ++x) {
                    const Vector2 position{
                        static_cast<float>((gridX + x - offsetX) * 32),
                        static_cast<float>((gridY + y - offsetY) * 32)
                    };
                    const bool showsTile = paintTileLayer != TileLayer::Collision &&
                        (tilePaintTool == TilePaintTool::Pencil || tilePaintTool == TilePaintTool::Brush ||
                        tilePaintTool == TilePaintTool::RandomBrush ||
                        tilePaintTool == TilePaintTool::AutoTile || tilePaintTool == TilePaintTool::RectangleFill ||
                        tilePaintTool == TilePaintTool::FloodFill);
                    if (showsTile) {
                        DrawTilesetTile(sheet,
                            paintTileSheet == 2 ? 0 : paintTileColumn + (stampSelection ? x % paintTileSelectionWidth : 0),
                            paintTileSheet == 2 ? 0 : paintTileRow + (stampSelection ? y % paintTileSelectionHeight : 0),
                            position, Fade(WHITE, 0.62f), 0, false, false,
                            paintAnimationFrames, paintAnimationFrameSeconds, previewTileAnimations);
                    }
                    const Rectangle cell{position.x, position.y, 32.0f, 32.0f};
                    DrawRectangleRec(cell, Fade(footprintColor, 0.16f));
                    DrawRectangleLinesEx(cell, lineWidth, Fade(footprintColor, 0.78f));
                }
            }

            const Rectangle footprint{
                static_cast<float>((gridX - offsetX) * 32),
                static_cast<float>((gridY - offsetY) * 32),
                static_cast<float>(footprintWidth * 32),
                static_cast<float>(footprintHeight * 32)
            };
            DrawRectangleLinesEx(footprint, lineWidth * 2.5f, footprintColor);
            DrawCircleLinesV({gridX * 32.0f + 16.0f, gridY * 32.0f + 16.0f},
                5.0f / camera.zoom, RAYWHITE);
            if (tileSymmetryMask != 0) {
                const int left = static_cast<int>(floorf(level.worldBounds.x / 32.0f));
                const int top = static_cast<int>(floorf(level.worldBounds.y / 32.0f));
                const int right = static_cast<int>(ceilf((level.worldBounds.x + level.worldBounds.width) / 32.0f)) - 1;
                const int bottom = static_cast<int>(ceilf((level.worldBounds.y + level.worldBounds.height) / 32.0f)) - 1;
                if ((tileSymmetryMask & 1) != 0)
                    DrawRectangleLinesEx({static_cast<float>((left + right - gridX - (footprintWidth - 1 - offsetX)) * 32),
                        static_cast<float>((gridY - offsetY) * 32), static_cast<float>(footprintWidth * 32), static_cast<float>(footprintHeight * 32)},
                        lineWidth * 2.0f, Fade(footprintColor, 0.82f));
                if ((tileSymmetryMask & 2) != 0)
                    DrawRectangleLinesEx({static_cast<float>((gridX - offsetX) * 32),
                        static_cast<float>((top + bottom - gridY - (footprintHeight - 1 - offsetY)) * 32), static_cast<float>(footprintWidth * 32), static_cast<float>(footprintHeight * 32)},
                        lineWidth * 2.0f, Fade(footprintColor, 0.82f));
                if (tileSymmetryMask == 3)
                    DrawRectangleLinesEx({static_cast<float>((left + right - gridX - (footprintWidth - 1 - offsetX)) * 32),
                        static_cast<float>((top + bottom - gridY - (footprintHeight - 1 - offsetY)) * 32), static_cast<float>(footprintWidth * 32), static_cast<float>(footprintHeight * 32)},
                        lineWidth * 2.0f, Fade(footprintColor, 0.82f));
            }

            if (tileDragActive && tilePaintTool == TilePaintTool::Line) {
                int x = tileDragStartX;
                int y = tileDragStartY;
                const int dx = std::abs(gridX - x);
                const int sx = x < gridX ? 1 : -1;
                const int dy = -std::abs(gridY - y);
                const int sy = y < gridY ? 1 : -1;
                int error = dx + dy;
                while (true) {
                    const Rectangle cell{static_cast<float>(x * 32), static_cast<float>(y * 32), 32.0f, 32.0f};
                    DrawRectangleRec(cell, Fade(footprintColor, 0.18f));
                    DrawRectangleLinesEx(cell, lineWidth, footprintColor);
                    if (x == gridX && y == gridY) break;
                    const int doubled = error * 2;
                    if (doubled >= dy) { error += dy; x += sx; }
                    if (doubled <= dx) { error += dx; y += sy; }
                }
            }
            if (tileDragActive && (tilePaintTool == TilePaintTool::RectangleFill ||
                tilePaintTool == TilePaintTool::RectangleOutline || tilePaintTool == TilePaintTool::Ellipse ||
                tilePaintTool == TilePaintTool::TileSelect || tilePaintTool == TilePaintTool::Measure)) {
                const int moveX = movingTileSelection ? gridX - tileDragStartX : 0;
                const int moveY = movingTileSelection ? gridY - tileDragStartY : 0;
                const int left = movingTileSelection ? tileSelectionLeft + moveX : std::min(tileDragStartX, gridX);
                const int top = movingTileSelection ? tileSelectionTop + moveY : std::min(tileDragStartY, gridY);
                const int right = movingTileSelection ? tileSelectionRight + moveX : std::max(tileDragStartX, gridX);
                const int bottom = movingTileSelection ? tileSelectionBottom + moveY : std::max(tileDragStartY, gridY);
                const Rectangle dragBounds{static_cast<float>(left * 32), static_cast<float>(top * 32),
                    static_cast<float>((right - left + 1) * 32), static_cast<float>((bottom - top + 1) * 32)};
                if (tilePaintTool == TilePaintTool::Ellipse) {
                    DrawEllipseLines(static_cast<int>(dragBounds.x + dragBounds.width * 0.5f),
                        static_cast<int>(dragBounds.y + dragBounds.height * 0.5f),
                        dragBounds.width * 0.5f, dragBounds.height * 0.5f, footprintColor);
                }
                else {
                    if (tilePaintTool != TilePaintTool::RectangleOutline) DrawRectangleRec(dragBounds, Fade(footprintColor, 0.12f));
                    DrawRectangleLinesEx(dragBounds, lineWidth * 2.5f, footprintColor);
                }
                if (tilePaintTool == TilePaintTool::Measure) {
                    const std::string label = std::to_string(right - left + 1) + " x " +
                        std::to_string(bottom - top + 1) + " tiles  |  " +
                        std::to_string((right - left + 1) * 32) + " x " + std::to_string((bottom - top + 1) * 32) + " px";
                    DrawTextEx(uiFont, label.c_str(), {dragBounds.x + 4.0f / camera.zoom,
                        dragBounds.y - 20.0f / camera.zoom}, 14.0f / camera.zoom, 0.0f, footprintColor);
                }
            }
            if (tilePaintTool == TilePaintTool::Stamp && !tileClipboard.empty()) {
                for (const VisualTile& tile : tileClipboard) {
                    const Vector2 position{tile.position.x + gridX * 32.0f, tile.position.y + gridY * 32.0f};
                    const Texture2D stampSheet = tile.layer == TileLayer::FarBackground ? industrialFarBackground :
                        (tile.layer == TileLayer::Background ? industrialBackground : industrialTiles);
                    DrawTilesetTile(stampSheet, tile.layer == TileLayer::FarBackground ? 0 : tile.column,
                        tile.layer == TileLayer::FarBackground ? 0 : tile.row, position, Fade(WHITE, 0.68f),
                        tile.quarterTurns, tile.flipX, tile.flipY, tile.animationFrames, tile.animationFrameSeconds,
                        previewTileAnimations);
                    DrawRectangleLinesEx({position.x, position.y, 32.0f, 32.0f}, lineWidth, footprintColor);
                }
            }
        }
        if ((tilePaintTool == TilePaintTool::TileSelect || tilePaintTool == TilePaintTool::MagicWand ||
            tilePaintTool == TilePaintTool::SelectMatching) && tileSelectionActive && !tileDragActive) {
            for (const unsigned long long key : tileSelectionCells) {
                const int x = static_cast<int>(static_cast<int32_t>(key >> 32));
                const int y = static_cast<int>(static_cast<int32_t>(key));
                const Rectangle cell{static_cast<float>(x * 32), static_cast<float>(y * 32), 32.0f, 32.0f};
                DrawRectangleRec(cell, Color{83, 214, 180, 34});
                DrawRectangleLinesEx(cell, lineWidth, Color{83, 214, 180, 190});
            }
            const Rectangle selectedTiles{static_cast<float>(tileSelectionLeft * 32), static_cast<float>(tileSelectionTop * 32),
                static_cast<float>((tileSelectionRight - tileSelectionLeft + 1) * 32),
                static_cast<float>((tileSelectionBottom - tileSelectionTop + 1) * 32)};
            DrawRectangleLinesEx(selectedTiles, lineWidth * 2.5f, Color{83, 214, 180, 255});
        }
    }
    if (groupSelection.size() > 1) {
        for (const Selection& item : groupSelection) {
            if (const Rectangle* itemRect = RectangleForSelection(item))
                DrawRectangleLinesEx(*itemRect, lineWidth, Color{83, 214, 180, 220});
            else if (item.kind == SelectionKind::PlayerStart)
                DrawCircleLinesV(level.playerStart, 25.0f, Color{83, 214, 180, 220});
            else if (item.kind == SelectionKind::SceneObject)
                DrawRectangleLinesEx(item.sceneBounds, lineWidth, Color{83, 214, 180, 220});
        }
        const Rectangle groupBounds = SelectionGroupBounds();
        if (groupBounds.width > 0.0f && groupBounds.height > 0.0f) {
            DrawRectangleRec(groupBounds, Color{83, 214, 180, 18});
            DrawRectangleLinesEx(groupBounds, lineWidth * 2.0f, Color{83, 214, 180, 255});
            if (transformMode == TransformMode::Scale) {
                const TransformHandle hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera));
                const float baseSize = 10.0f / camera.zoom;
                const float size = baseSize * (hovered == TransformHandle::ScaleBottomRight ? 1.35f : 1.0f);
                DrawRectangleRec({groupBounds.x + groupBounds.width - size * 0.5f,
                    groupBounds.y + groupBounds.height - size * 0.5f, size, size}, Accent);
            }
        }
    }
    if (draggingMarquee) {
        const Rectangle marquee = MakeRect(marqueeStart, marqueeCurrent);
        DrawRectangleRec(marquee, Color{70, 151, 220, 34});
        DrawRectangleLinesEx(marquee, lineWidth * 1.5f, Color{70, 151, 220, 235});
    }
    const Rectangle* selected = SelectedRectangle();
    if (selection.kind == SelectionKind::SceneObject && selectedSceneBounds.width > 0.0f &&
        selectedSceneBounds.height > 0.0f) {
        DrawRectangleRec(selectedSceneBounds, Color{255, 236, 126, 28});
        DrawRectangleLinesEx(selectedSceneBounds, lineWidth * 2.0f, Color{255, 236, 126, 255});
        if (transformMode == TransformMode::Move &&
            const_cast<LevelEditor*>(this)->TranslateSelectedScene({0.0f, 0.0f})) {
            const Vector2 center{selectedSceneBounds.x + selectedSceneBounds.width * 0.5f,
                selectedSceneBounds.y + selectedSceneBounds.height * 0.5f};
            const float gizmoSize = 30.0f / camera.zoom;
            const float handleRadius = 5.5f / camera.zoom;
            const Vector2 xHandle{center.x + gizmoSize, center.y};
            const Vector2 yHandle{center.x, center.y - gizmoSize};
            const TransformHandle hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera));
            DrawLineEx(center, xHandle, lineWidth * 2.0f, RED);
            DrawLineEx(center, yHandle, lineWidth * 2.0f, GREEN);
            DrawCircleV(xHandle, handleRadius * (hovered == TransformHandle::MoveX ? 1.35f : 1.0f), RED);
            DrawCircleV(yHandle, handleRadius * (hovered == TransformHandle::MoveY ? 1.35f : 1.0f), GREEN);
            DrawCircleV(center, handleRadius * (hovered == TransformHandle::MoveCenter ? 1.35f : 1.0f), RAYWHITE);
        }
        if (transformMode == TransformMode::Scale &&
            const_cast<LevelEditor*>(this)->ScaleSelectedScene({0.0f, 0.0f})) {
            const TransformHandle hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera));
            const float baseSize = 10.0f / camera.zoom;
            const float size = baseSize * (hovered == TransformHandle::ScaleBottomRight ? 1.35f : 1.0f);
            DrawRectangleRec({selectedSceneBounds.x + selectedSceneBounds.width - size * 0.5f,
                selectedSceneBounds.y + selectedSceneBounds.height - size * 0.5f, size, size}, Accent);
        }
        if (transformMode == TransformMode::Rotate && SelectedRotation() != nullptr) {
            const Vector2 center = SelectedRotationCenter();
            const float radius = std::max(selectedSceneBounds.width, selectedSceneBounds.height) * 0.62f;
            const bool hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera)) ==
                TransformHandle::RotateRing;
            DrawCircleLinesV(center, radius, hovered ? Color{255, 215, 96, 255} : Warning);
            const float angle = *SelectedRotation() * DEG2RAD;
            DrawLineEx(center, {center.x + cosf(angle) * radius, center.y + sinf(angle) * radius},
                lineWidth * 2.0f, hovered ? RAYWHITE : Warning);
        }
    }
    if (selected != nullptr && selected->width > 0.0f && selected->height > 0.0f) {
        DrawRectangleLinesEx(*selected, lineWidth * 2.0f, Color{255, 236, 126, 255});
        const Vector2 center{selected->x + selected->width * 0.5f, selected->y + selected->height * 0.5f};
        const float gizmoSize = 30.0f / camera.zoom;
        const float handleRadius = 5.5f / camera.zoom;
        const TransformHandle hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera));
        if (transformMode == TransformMode::Move) {
            const Vector2 xHandle{center.x + gizmoSize, center.y};
            const Vector2 yHandle{center.x, center.y - gizmoSize};
            DrawLineEx(center, xHandle, lineWidth * 2.0f, RED);
            DrawLineEx(center, yHandle, lineWidth * 2.0f, GREEN);
            DrawCircleV(xHandle, handleRadius * (hovered == TransformHandle::MoveX ? 1.35f : 1.0f), RED);
            DrawCircleV(yHandle, handleRadius * (hovered == TransformHandle::MoveY ? 1.35f : 1.0f), GREEN);
            DrawCircleV(center, handleRadius * (hovered == TransformHandle::MoveCenter ? 1.35f : 1.0f), RAYWHITE);
        }
        else if (transformMode == TransformMode::Scale && selection.kind != SelectionKind::Exit) {
            const float handle = 10.0f / camera.zoom;
            const float hoveredScale = hovered == TransformHandle::ScaleBottomRight ? 1.35f : 1.0f;
            const float size = handle * hoveredScale;
            DrawRectangleRec({selected->x + selected->width - size * 0.5f,
                selected->y + selected->height - size * 0.5f, size, size}, Accent);
        }
        else if (transformMode == TransformMode::Rotate) {
            DrawCircleLinesV(center, std::max(selected->width, selected->height) * 0.55f, Warning);
        }
    }
    if (selection.kind == SelectionKind::PlayerStart) {
        DrawCircleLinesV(level.playerStart, 25.0f, Color{255, 236, 126, 255});
        if (transformMode == TransformMode::Move) {
            const float gizmoSize = 30.0f / camera.zoom;
            const float handleRadius = 5.5f / camera.zoom;
            const Vector2 xHandle{level.playerStart.x + gizmoSize, level.playerStart.y};
            const Vector2 yHandle{level.playerStart.x, level.playerStart.y - gizmoSize};
            const TransformHandle hovered = TransformHandleAt(GetScreenToWorld2D(GetMousePosition(), camera));
            DrawLineEx(level.playerStart, xHandle, lineWidth * 2.0f, RED);
            DrawLineEx(level.playerStart, yHandle, lineWidth * 2.0f, GREEN);
            DrawCircleV(xHandle, handleRadius * (hovered == TransformHandle::MoveX ? 1.35f : 1.0f), RED);
            DrawCircleV(yHandle, handleRadius * (hovered == TransformHandle::MoveY ? 1.35f : 1.0f), GREEN);
            DrawCircleV(level.playerStart,
                handleRadius * (hovered == TransformHandle::MoveCenter ? 1.35f : 1.0f), RAYWHITE);
        }
    }

    if (draggingCreate) {
        if (activeTool == EditTool::Exit) {
            const Rectangle draft{dragStart.x, dragStart.y, StandardExitDoorWidth, StandardExitDoorHeight};
            DrawExitDoor(draft, draft.y + draft.height);
            DrawRectangleLinesEx(draft, lineWidth * 2.0f, Color{255, 236, 126, 230});
        }
        else {
            const Rectangle draft = MakeRect(dragStart, dragCurrent);
            DrawRectangleRec(draft, Color{255, 236, 126, 55});
            DrawRectangleLinesEx(draft, lineWidth * 2.0f, Color{255, 236, 126, 230});
        }
    }
}

void LevelEditor::DrawLevelPreview() const {
    const float lineWidth = std::max(1.0f / camera.zoom, 1.5f / camera.zoom);

    if ((visibleTileLayerMask & TileLayerBit(TileLayer::Background)) != 0)
        DrawTilesetBackgroundFill(industrialBackground, level.worldBounds, Fade(WHITE, 0.68f), 0.08f);
    DrawRectangleRec(level.worldBounds, Fade(Color{13, 20, 28, 255}, 0.16f));

    for (int layerId = 0; layerId < 12; ++layerId) {
        const auto layer = static_cast<TileLayer>(layerId);
        if (layer == TileLayer::Collision || (visibleTileLayerMask & TileLayerBit(layer)) == 0) continue;
        for (const VisualTile& tile : level.visualTiles) {
            if (tile.layer != layer) continue;
            const int sheetIndex = tile.sheetIndex >= 0 ? tile.sheetIndex :
                (tile.layer == TileLayer::FarBackground ? 2 : tile.layer == TileLayer::Background ? 1 : 0);
            const Texture2D sheet = sheetIndex == 2 ? industrialFarBackground :
                sheetIndex == 1 ? industrialBackground : industrialTiles;
            DrawTilesetTile(sheet, sheetIndex == 2 ? 0 : tile.column, sheetIndex == 2 ? 0 : tile.row,
                tile.position, WHITE, tile.quarterTurns, tile.flipX, tile.flipY,
                tile.animationFrames, tile.animationFrameSeconds, previewTileAnimations);
        }
    }
    const bool hasRenderableTiles = std::any_of(level.visualTiles.begin(), level.visualTiles.end(),
        [](const VisualTile& tile) { return tile.layer != TileLayer::Collision; });
    for (const FluidField& fluid : level.fluids) {
        DrawFluidBackground(fluid);
    }
    for (const Rectangle solid : level.baseSolids) {
        if (hasRenderableTiles) continue;
        if (IsCeilingSolid(solid)) DrawTilesetCeiling(industrialTiles, solid, WHITE);
        else if (IsWallSolid(solid)) DrawTilesetWall(industrialTiles, solid, WHITE);
        else if (HasSolidTouchingTop(solid, level.baseSolids)) DrawTilesetSolidFill(industrialTiles, solid, WHITE);
        else DrawTilesetSolid(industrialTiles, solid, WHITE);
        DrawRectangleLinesEx(solid, 2.0f, BLACK);
    }
    for (const Rectangle platform : level.pitPlatforms) {
        DrawTilesetSolid(industrialTiles, platform, WHITE);
        DrawRectangleLinesEx(platform, 2.0f, BLACK);
    }
    for (const Rectangle ladder : level.ladders) {
        DrawLineEx({ladder.x + 8.0f, ladder.y}, {ladder.x + 8.0f, ladder.y + ladder.height}, 4.0f, BLACK);
        DrawLineEx({ladder.x + ladder.width - 8.0f, ladder.y},
            {ladder.x + ladder.width - 8.0f, ladder.y + ladder.height}, 4.0f, BLACK);
        const int rungCount = static_cast<int>(ceilf(ladder.height / 30.0f));
        for (int index = 0; index <= rungCount; ++index) {
            const float y = fminf(ladder.y + index * 30.0f, ladder.y + ladder.height);
            DrawLineEx({ladder.x + 8.0f, y}, {ladder.x + ladder.width - 8.0f, y}, 3.0f, BLACK);
        }
    }
    for (const Rectangle zone : level.cameraZones) {
        DrawRectangleLinesEx(zone, lineWidth * 1.5f, Color{86, 190, 230, 210});
    }

    DrawSpikes(level.spikeHazard);
    for (const DirectionalSpikeHazard& hazard : level.directionalSpikeHazards) DrawDirectionalSpikes(hazard);
    for (const PortalPair& pair : level.portalPairs) {
        DrawPortal(pair.entrance, Color{255, 146, 52, 255});
        DrawPortal(pair.exit, Color{63, 143, 255, 255});
    }
    if (level.waterPit.bounds.width > 0.0f && level.waterPit.bounds.height > 0.0f) DrawWaterPit(level.waterPit);
    DrawExitDoor(level.exitTrigger, level.exitTrigger.y + level.exitTrigger.height);
    Player previewPlayer{};
    previewPlayer.rect.x = level.playerStart.x;
    previewPlayer.rect.y = level.playerStart.y;
    DrawPlayer(previewPlayer, playerSprites, 0);

    for (const Vector2 pulley : level.pulleys) {
        DrawPulley(pulley, 42.0f, 0.0f, BLACK);
    }
    for (const HangingWeight& weight : level.weights) {
        DrawHazardWeight(weight);
    }
    for (const RotaryLatch& latch : level.rotaryLatches) {
        DrawRotaryLatch(latch, false, "");
    }
    for (const StoneBlock& block : level.stoneBlocks) {
        DrawStoneBlock(block);
    }
    for (const Boulder& boulder : level.boulders) {
        DrawBoulder(boulder);
    }
    for (const PhysicsWheel& wheel : level.physicsWheels) {
        DrawPhysicsWheel(wheel);
    }
    for (const Gear& gear : level.gears) {
        DrawGear(gear);
    }
    if (level.clockFaceRadius > 0.0f) {
        DrawCircleV(level.clockFaceCenter, level.clockFaceRadius + 5.0f, Color{171, 129, 53, 255});
        DrawCircleV(level.clockFaceCenter, level.clockFaceRadius, Color{219, 210, 181, 255});
        DrawCircleLinesV(level.clockFaceCenter, level.clockFaceRadius, Color{34, 39, 42, 255});
        const auto handAngle = [&](ClockHandType hand) {
            for (const Gear& gear : level.gears) {
                if (gear.clockHand == hand) return gear.rotation;
            }
            return 270.0f;
        };
        const auto drawHand = [&](ClockHandType hand, float length, float thickness, Color color) {
            const float angle = handAngle(hand) * DEG2RAD;
            DrawLineEx(level.clockFaceCenter,
                {level.clockFaceCenter.x + cosf(angle) * length, level.clockFaceCenter.y + sinf(angle) * length},
                thickness, color);
        };
        drawHand(ClockHandType::Hour, level.clockFaceRadius * 0.48f, lineWidth * 5.0f, Color{45, 52, 55, 255});
        drawHand(ClockHandType::Minute, level.clockFaceRadius * 0.69f, lineWidth * 3.5f, Color{45, 52, 55, 255});
        drawHand(ClockHandType::Second, level.clockFaceRadius * 0.82f, lineWidth * 2.0f, Color{180, 48, 40, 255});
    }
    for (const Flywheel& flywheel : level.flywheels) {
        DrawFlywheel(flywheel);
    }
    for (const SteeringWheel& wheel : level.steeringWheels) {
        DrawSteeringWheel(wheel);
    }
    for (const Screw& screw : level.screws) {
        DrawScrew(screw);
    }
    for (const Fan& fan : level.fans) {
        DrawFan(fan);
    }
    for (const Pinwheel& pinwheel : level.pinwheels) {
        DrawPinwheel(pinwheel);
    }
    for (const Ramp& ramp : level.ramps) {
        DrawRamp(ramp);
    }
    for (const SeeSaw& seeSaw : level.seeSaws) {
        DrawSeeSaw(seeSaw);
    }
    for (const TrapDoor& door : level.trapDoors) {
        DrawTrapDoor(door);
    }
    for (const Chain& chain : level.chains) {
        DrawChain(chain, chainLinks);
    }
    for (const PhysicsRope& rope : level.physicsRopes) {
        DrawPhysicsRope(rope);
    }
    for (const Button& button : level.buttons) {
        DrawButton(button);
    }
    for (const ArrowTrap& trap : level.arrowTraps) {
        DrawArrowTrap(trap);
    }
    if (level.valve.center.x != 0.0f || level.valve.center.y != 0.0f) DrawValveBody(level.valve, false);
    for (const BreakableTile& tile : level.breakableTiles) {
        DrawBreakableTile(industrialTiles, tile);
    }
    for (const Enemy& enemy : level.enemies) {
        DrawEnemy(enemy, enemySprites);
    }
    for (const GuideObject& object : level.guideObjects) {
        DrawGuideObject(object);
    }
    for (const LevelLabel& label : level.labels) {
        const int padding = std::max(5, static_cast<int>(roundf(label.fontSize * 0.42f)));
        const int textWidth = MeasureText(label.text.c_str(), label.fontSize);
        const Rectangle sign{label.position.x, label.position.y,
            static_cast<float>(textWidth + padding * 2), static_cast<float>(label.fontSize + padding * 2)};
        DrawRectangleRounded(sign, 0.18f, 6, Fade(Color{22, 28, 34, 255}, 0.90f));
        DrawRectangleRoundedLinesEx(sign, 0.18f, 6, 2.0f, Fade(ORANGE, 0.90f));
        DrawText(label.text.c_str(), static_cast<int>(sign.x) + padding, static_cast<int>(sign.y) + padding,
            label.fontSize, RAYWHITE);
    }

    for (const FluidField& fluid : level.fluids) DrawFluidField(fluid);
    for (const Rectangle darkness : level.darknessAreas) DrawRectangleRec(darkness, Fade(BLACK, 0.38f));

    if ((visibleTileLayerMask & TileLayerBit(TileLayer::Collision)) != 0) {
    for (const VisualTile& tile : level.visualTiles) {
        if (tile.layer != TileLayer::Collision) continue;
            const Rectangle cell{tile.position.x, tile.position.y, 32.0f, 32.0f};
            DrawRectangleRec(cell, Color{235, 55, 62, 105});
            DrawRectangleLinesEx(cell, lineWidth, Color{255, 82, 88, 235});
        }
    }

    DrawRectangleLinesEx(level.worldBounds, lineWidth * 2.0f, Color{218, 225, 235, 235});
}

void LevelEditor::DrawToolbar() const {
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(ToolbarHeight), PanelBackground);
    DrawLine(0, static_cast<int>(ToolbarHeight - 1.0f), GetScreenWidth(),
        static_cast<int>(ToolbarHeight - 1.0f), PanelBorder);
    DrawUiText(uiFont, "PPP", 16.0f, 11.0f, 23.0f, Warning);
    DrawUiText(uiFont, "LEVEL EDITOR", 62.0f, 18.0f, 17.0f, PrimaryText);
    const TransformMode modes[] = {
        TransformMode::Select, TransformMode::Move, TransformMode::Rotate, TransformMode::Scale
    };
    const char* labels[] = {"Select", "Move", "Rotate", "Scale"};
    for (int index = 0; index < 4; ++index) {
        DrawToolbarIconButton(GetTransformButtonBounds(index), labels[index], uiFont, toolbarIcons, index,
            activeTool == EditTool::Select && transformMode == modes[index]);
    }
    DrawToolbarIconButton(GetFitButtonBounds(), "Frame", uiFont, toolbarIcons, 4);
    const std::string mode = activeTool == EditTool::Select ? "Transform mode" : std::string("Place: ") + ToolName(activeTool);
    if (GetCanvasBounds().width > 820.0f) {
        const Rectangle frame = GetFitButtonBounds();
        DrawUiText(uiFont, mode.c_str(), frame.x + frame.width + 16.0f, 23.0f, 16.0f, SecondaryText);
    }
    const std::string levelName = currentLevelPath.empty() ? "Untitled" : currentLevelPath.stem().string();
    const float nameWidth = MeasureUiText(uiFont, levelName.c_str(), 16.0f);
    const Rectangle canvas = GetCanvasBounds();
    DrawUiText(uiFont, levelName.c_str(), canvas.x + canvas.width - nameWidth - 16.0f, 21.0f, 16.0f,
        dirty ? Warning : SecondaryText);
}

void LevelEditor::DrawAssetLibrary(Rectangle panel) const {
    DrawRectangleRec(panel, PanelBackground);
    DrawRectangleLinesEx(panel, 1.0f, PanelBorder);
    DrawRectangle(static_cast<int>(panel.x), static_cast<int>(panel.y), static_cast<int>(panel.width), 38, PanelHeader);
    DrawUiText(uiFont, "ASSET LIBRARY", panel.x + 14.0f, panel.y + 11.0f, 16.0f, PrimaryText);
    const std::string objectCount = std::to_string(AssetCatalog().size()) + " game objects";
    DrawUiText(uiFont, objectCount.c_str(), panel.x + 14.0f, panel.y + 48.0f, 13.0f, SecondaryText);

    const int visibleRows = std::max(1, static_cast<int>((panel.height - 78.0f) / 30.0f));
    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(panel.y + 68.0f),
        static_cast<int>(panel.width), static_cast<int>(panel.height - 68.0f));
    for (int visibleIndex = 0; visibleIndex < visibleRows; ++visibleIndex) {
        const int catalogIndex = assetLibraryScroll + visibleIndex;
        if (catalogIndex >= static_cast<int>(AssetCatalog().size())) break;
        const CatalogEntry& asset = AssetCatalog()[static_cast<size_t>(catalogIndex)];
        const Rectangle row = GetAssetRowBounds(visibleIndex);
        const bool selected = asset.placeable && activeTool == asset.tool;
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), row);
        if (selected || hovered) {
            DrawRectangleRounded(row, 0.12f, 4, selected ? Color{48, 91, 139, 255} : Color{42, 47, 57, 255});
        }
        const unsigned int categoryHash = static_cast<unsigned int>(std::hash<std::string>{}(asset.category));
        const Color categoryColor{static_cast<unsigned char>(80 + categoryHash % 120),
            static_cast<unsigned char>(90 + (categoryHash / 7) % 110),
            static_cast<unsigned char>(100 + (categoryHash / 17) % 100), 255};
        DrawRectangleRounded({row.x + 6.0f, row.y + 7.0f, 14.0f, 14.0f}, 0.18f, 4, categoryColor);
        DrawUiText(uiFont, asset.name.c_str(), row.x + 28.0f, row.y + 2.0f, 14.0f,
            selected ? RAYWHITE : PrimaryText);
        DrawUiText(uiFont, asset.category.c_str(), row.x + 28.0f, row.y + 16.0f, 10.0f, SecondaryText);
    }
    EndScissorMode();

    if (static_cast<int>(AssetCatalog().size()) > visibleRows) {
        const float trackHeight = panel.height - 78.0f;
        const float thumbHeight = std::max(24.0f, trackHeight * visibleRows / AssetCatalog().size());
        const float progress = static_cast<float>(assetLibraryScroll) /
            std::max(1, static_cast<int>(AssetCatalog().size()) - visibleRows);
        DrawRectangleRec({panel.x + panel.width - 5.0f, panel.y + 70.0f + progress * (trackHeight - thumbHeight),
            3.0f, thumbHeight}, SecondaryText);
    }
}

void LevelEditor::DrawContentBrowser(Rectangle browser) const {
    DrawRectangleRec(browser, PanelBackground);
    DrawRectangle(static_cast<int>(browser.x), static_cast<int>(browser.y), static_cast<int>(browser.width), 38, PanelHeader);
    DrawRectangleLinesEx(browser, 1.0f, PanelBorder);
    DrawUiText(uiFont, "CONTENT BROWSER", browser.x + 14.0f, browser.y + 11.0f, 16.0f, PrimaryText);
    DrawUiText(uiFont, "Content  >  Editor Assets", browser.x + ContentFolderWidth + 12.0f,
        browser.y + 48.0f, 15.0f, SecondaryText);

    const Rectangle folders{browser.x, browser.y + 38.0f, ContentFolderWidth, browser.height - 38.0f};
    DrawRectangleRec(folders, PanelInset);
    DrawLine(static_cast<int>(folders.x + folders.width), static_cast<int>(folders.y),
        static_cast<int>(folders.x + folders.width), static_cast<int>(browser.y + browser.height), PanelBorder);
    DrawUiText(uiFont, "Content", folders.x + 14.0f, folders.y + 14.0f, 16.0f, PrimaryText);
    const char* categories[] = {"All Assets", "Geometry", "Regions", "Markers"};
    for (int category = 0; category < 4; ++category) {
        const Rectangle row{folders.x + 8.0f, browser.y + 70.0f + category * 26.0f,
            folders.width - 16.0f, 24.0f};
        if (contentCategory == category) DrawRectangleRec(row, Color{48, 91, 139, 255});
        DrawUiText(uiFont, categories[category], row.x + 10.0f, row.y + 5.0f, 14.0f,
            contentCategory == category ? RAYWHITE : SecondaryText);
    }

    const std::vector<int> visibleAssets = ContentAssetIndices(contentCategory);
    for (int visibleIndex = 0; visibleIndex < static_cast<int>(visibleAssets.size()); ++visibleIndex) {
        const AssetEntry& asset = Assets[visibleAssets[static_cast<size_t>(visibleIndex)]];
        const Rectangle tile = GetContentAssetBounds(visibleIndex);
        const bool selected = activeTool == asset.tool;
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), tile);
        DrawRectangleRounded(tile, 0.05f, 4, selected ? Color{43, 72, 104, 255} :
            (hovered ? Color{46, 52, 62, 255} : Color{35, 39, 47, 255}));
        DrawRectangleRoundedLinesEx(tile, 0.05f, 4, 1.0f, selected ? Accent : PanelBorder);
        const Rectangle preview{tile.x + 8.0f, tile.y + 8.0f, tile.width - 16.0f, 66.0f};
        DrawRectangleRec(preview, PanelInset);
        const Rectangle sample{preview.x + 7.0f, preview.y + 12.0f, preview.width - 14.0f, preview.height - 22.0f};
        switch (asset.tool) {
        case EditTool::Solid:
        case EditTool::Platform:
            DrawTilesetSolid(industrialTiles, sample, WHITE);
            break;
        case EditTool::Ladder:
            DrawLineEx({sample.x + 9.0f, sample.y}, {sample.x + 9.0f, sample.y + sample.height}, 3.0f, BLACK);
            DrawLineEx({sample.x + sample.width - 9.0f, sample.y},
                {sample.x + sample.width - 9.0f, sample.y + sample.height}, 3.0f, BLACK);
            for (float y = sample.y; y <= sample.y + sample.height; y += 11.0f) {
                DrawLineEx({sample.x + 9.0f, y}, {sample.x + sample.width - 9.0f, y}, 2.0f, BLACK);
            }
            break;
        case EditTool::CameraZone:
            DrawRectangleLinesEx(sample, 2.0f, Assets[3].color);
            break;
        case EditTool::Darkness:
            DrawRectangleRec(sample, Fade(BLACK, 0.72f));
            break;
        case EditTool::PlayerStart: {
            Player player{};
            player.rect = {sample.x + 8.0f, sample.y + 2.0f, 31.0f, 40.0f};
            DrawPlayer(player, playerSprites, 0);
            break;
        }
        case EditTool::Exit:
            DrawExitDoor({sample.x + 7.0f, sample.y + 5.0f, sample.width - 14.0f, sample.height - 5.0f},
                sample.y + sample.height);
            break;
        case EditTool::Water:
            DrawRectangleRec(sample, Color{44, 145, 207, 220});
            break;
        case EditTool::Sand:
            DrawRectangleRec(sample, Color{194, 145, 66, 220});
            break;
        case EditTool::Gel:
            DrawRectangleRec(sample, Color{84, 170, 84, 220});
            break;
        case EditTool::Gas:
            DrawRectangleRec(sample, Color{80, 150, 205, 150});
            break;
        case EditTool::Pulley:
            DrawPulley({sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f},
                std::min(sample.width, sample.height) * 0.42f, 0.0f, BLACK);
            break;
        case EditTool::HangingWeight: {
            HangingWeight weight{};
            weight.pulley = {sample.x + sample.width * 0.5f, sample.y + 8.0f};
            weight.pulleyRadius = 10.0f;
            weight.rect = {sample.x + sample.width * 0.5f - 18.0f, sample.y + sample.height - 31.0f, 36.0f, 29.0f};
            DrawHazardWeight(weight);
            break;
        }
        case EditTool::RotaryLatch: {
            RotaryLatch latch{};
            latch.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            latch.radius = std::min(sample.width, sample.height) * 0.42f;
            latch.targetAngle = 270.0f;
            latch.tolerance = 8.0f;
            DrawRotaryLatch(latch, false, "");
            break;
        }
        case EditTool::StoneBlock: {
            StoneBlock block{};
            block.rect = sample;
            DrawStoneBlock(block);
            break;
        }
        case EditTool::Boulder: {
            Boulder boulder{};
            boulder.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            boulder.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawBoulder(boulder);
            break;
        }
        case EditTool::PhysicsWheel: {
            PhysicsWheel wheel{};
            wheel.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            wheel.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawPhysicsWheel(wheel);
            break;
        }
        case EditTool::Gear: {
            Gear gear{};
            gear.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            gear.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawGear(gear);
            break;
        }
        case EditTool::Flywheel: {
            Flywheel flywheel{};
            flywheel.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            flywheel.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawFlywheel(flywheel);
            break;
        }
        case EditTool::SteeringWheel: {
            SteeringWheel wheel{};
            wheel.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            wheel.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawSteeringWheel(wheel);
            break;
        }
        case EditTool::Screw: {
            Screw screw{};
            screw.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            screw.length = sample.width * 0.9f;
            screw.radius = std::max(5.0f, sample.height * 0.16f);
            DrawScrew(screw);
            break;
        }
        case EditTool::Fan: {
            Fan fan{};
            fan.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            fan.length = sample.width * 0.8f;
            fan.width = sample.height * 0.75f;
            DrawFan(fan);
            break;
        }
        case EditTool::Pinwheel: {
            Pinwheel pinwheel{};
            pinwheel.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            pinwheel.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawPinwheel(pinwheel);
            break;
        }
        case EditTool::Ramp: {
            Ramp ramp{};
            ramp.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            ramp.length = sample.width * 0.9f;
            ramp.thickness = std::max(5.0f, sample.height * 0.18f);
            ramp.angle = -20.0f;
            DrawRamp(ramp);
            break;
        }
        case EditTool::SeeSaw: {
            SeeSaw seeSaw{};
            seeSaw.pivot = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            seeSaw.length = sample.width * 0.9f;
            seeSaw.thickness = std::max(5.0f, sample.height * 0.18f);
            DrawSeeSaw(seeSaw);
            break;
        }
        case EditTool::TrapDoor: {
            TrapDoor trapDoor{};
            trapDoor.hinge = {sample.x + 4.0f, sample.y + sample.height * 0.5f};
            trapDoor.length = sample.width * 0.85f;
            trapDoor.thickness = std::max(5.0f, sample.height * 0.18f);
            DrawTrapDoor(trapDoor);
            break;
        }
        case EditTool::Chain: {
            Chain chain{};
            chain.start = {sample.x + sample.width * 0.5f, sample.y + 2.0f};
            chain.end = {sample.x + sample.width * 0.5f, sample.y + sample.height - 2.0f};
            chain.spacing = 11.0f;
            InitializeChain(chain);
            DrawChain(chain, chainLinks);
            break;
        }
        case EditTool::PhysicsRope: {
            PhysicsRope rope{};
            rope.start = {sample.x + 5.0f, sample.y + 5.0f};
            rope.end = {sample.x + sample.width - 5.0f, sample.y + sample.height - 5.0f};
            rope.length = sample.width;
            InitializePhysicsRope(rope);
            DrawPhysicsRope(rope);
            break;
        }
        case EditTool::Button: {
            Button button{};
            button.rect = {sample.x + 8.0f, sample.y + sample.height * 0.5f - 8.0f,
                sample.width - 16.0f, 16.0f};
            DrawButton(button);
            break;
        }
        case EditTool::Portal:
            DrawPortal({sample.x + 7.0f, sample.y + 5.0f, 25.0f, sample.height - 10.0f},
                Color{255, 146, 52, 255});
            DrawPortal({sample.x + sample.width - 32.0f, sample.y + 5.0f, 25.0f, sample.height - 10.0f},
                Color{63, 143, 255, 255});
            break;
        case EditTool::DirectionalSpikes: {
            DirectionalSpikeHazard hazard{};
            hazard.rect = {sample.x + 4.0f, sample.y + sample.height * 0.5f - 10.0f,
                sample.width - 8.0f, 20.0f};
            DrawDirectionalSpikes(hazard);
            break;
        }
        case EditTool::ArrowTrap: {
            ArrowTrap trap{};
            trap.position = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            trap.direction = {1.0f, 0.0f};
            DrawArrowTrap(trap);
            break;
        }
        case EditTool::BreakableTile: {
            BreakableTile tile{};
            tile.rect = sample;
            DrawBreakableTile(industrialTiles, tile);
            break;
        }
        case EditTool::Enemy: {
            Enemy enemy{};
            enemy.rect = sample;
            DrawEnemy(enemy, enemySprites);
            break;
        }
        case EditTool::Label: {
            DrawRectangleRounded(sample, 0.18f, 6, Color{22, 28, 34, 255});
            DrawText("LABEL", static_cast<int>(sample.x + 8.0f), static_cast<int>(sample.y + sample.height * 0.5f - 8.0f),
                16, RAYWHITE);
            break;
        }
        case EditTool::Valve: {
            Valve valve{};
            valve.center = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            valve.radius = std::min(sample.width, sample.height) * 0.42f;
            DrawValveBody(valve, false);
            break;
        }
        case EditTool::Checkpoint:
        case EditTool::Collectible: {
            GuideObject object{};
            std::istringstream stream(asset.tool == EditTool::Checkpoint ?
                "0 0 30 56" : "0 0 12");
            ParseGuideObject(asset.tool == EditTool::Checkpoint ? "checkpoint" : "collectible", stream, object);
            object.transform.position = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            object.origin = object.transform.position;
            DrawGuideObject(object);
            break;
        }
        case EditTool::Ball:
        case EditTool::Barrel:
        case EditTool::MovingPlatform:
        case EditTool::Elevator: {
            GuideObject object{};
            const char* command = asset.tool == EditTool::Ball ? "ball" :
                asset.tool == EditTool::Barrel ? "barrel" :
                asset.tool == EditTool::MovingPlatform ? "movingPlatform" : "elevator";
            const char* arguments = asset.tool == EditTool::Ball ? "0 0 25 1" :
                asset.tool == EditTool::Barrel ? "0 0 42 56 2" :
                asset.tool == EditTool::MovingPlatform ? "0 0 88 22 1 0 120 1" : "0 0 70 24 0 -1 100 1";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, object);
            object.transform.position = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            object.origin = object.transform.position;
            DrawGuideObject(object);
            break;
        }
        case EditTool::PendulumBob:
        case EditTool::OneWayPlatform:
        case EditTool::CeilingHook:
        case EditTool::GuideRail: {
            GuideObject object{};
            const char* command = asset.tool == EditTool::PendulumBob ? "pendulumBob" :
                asset.tool == EditTool::OneWayPlatform ? "oneWayPlatform" :
                asset.tool == EditTool::CeilingHook ? "ceilingHook" : "guideRail";
            const char* arguments = asset.tool == EditTool::PendulumBob ? "0 0 48 13 0 1" :
                asset.tool == EditTool::CeilingHook ? "0 0 15" :
                asset.tool == EditTool::OneWayPlatform ? "0 0 88 18" : "0 0 88 14";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, object);
            object.origin = {sample.x + sample.width * 0.5f,
                sample.y + (asset.tool == EditTool::PendulumBob ? 5.0f : sample.height * 0.5f)};
            object.transform.position = object.origin;
            if (asset.tool == EditTool::PendulumBob) object.transform.position.y += object.length;
            DrawGuideObject(object);
            break;
        }
        case EditTool::Spring:
        case EditTool::CompressionSpring:
        case EditTool::ExtensionSpring:
        case EditTool::TorsionSpring:
        case EditTool::GarterSpring:
        case EditTool::VoluteSpring:
        case EditTool::SpiralSpring:
        case EditTool::ConstantForceSpring:
        case EditTool::ConstantTorqueSpring:
        case EditTool::LeafSpring:
        case EditTool::BeamSpring:
        case EditTool::DiscSpring:
        case EditTool::WaveSpring:
        case EditTool::WaveWasher:
        case EditTool::TorsionBar:
        case EditTool::RingSpring:
        case EditTool::ElastomerSpring:
        case EditTool::PneumaticSpring:
        case EditTool::GasSpring:
        case EditTool::HydropneumaticSpring:
        case EditTool::MagneticSpring:
        case EditTool::CompositeSpring: {
            const char* command = asset.tool == EditTool::Spring ? "spring" :
                asset.tool == EditTool::CompressionSpring ? "compressionSpring" :
                asset.tool == EditTool::ExtensionSpring ? "extensionSpring" :
                asset.tool == EditTool::TorsionSpring ? "torsionSpring" :
                asset.tool == EditTool::GarterSpring ? "garterSpring" :
                asset.tool == EditTool::VoluteSpring ? "voluteSpring" :
                asset.tool == EditTool::SpiralSpring ? "spiralSpring" :
                asset.tool == EditTool::ConstantForceSpring ? "constantForceSpring" :
                asset.tool == EditTool::ConstantTorqueSpring ? "constantTorqueSpring" :
                asset.tool == EditTool::LeafSpring ? "leafSpring" :
                asset.tool == EditTool::BeamSpring ? "beamSpring" :
                asset.tool == EditTool::DiscSpring ? "discSpring" :
                asset.tool == EditTool::WaveSpring ? "waveSpring" :
                asset.tool == EditTool::WaveWasher ? "waveWasher" :
                asset.tool == EditTool::TorsionBar ? "torsionBar" :
                asset.tool == EditTool::RingSpring ? "ringSpring" :
                asset.tool == EditTool::ElastomerSpring ? "elastomerSpring" :
                asset.tool == EditTool::PneumaticSpring ? "pneumaticSpring" :
                asset.tool == EditTool::GasSpring ? "gasSpring" :
                asset.tool == EditTool::HydropneumaticSpring ? "hydropneumaticSpring" :
                asset.tool == EditTool::MagneticSpring ? "magneticSpring" : "compositeSpring";
            const char* arguments = asset.tool == EditTool::TorsionSpring ? "0 0 80 0 7 10 1.8" :
                asset.tool == EditTool::GarterSpring ? "0 0 80 0 7 9 1" :
                asset.tool == EditTool::VoluteSpring ? "0 0 80 0 7 14 1" :
                asset.tool == EditTool::SpiralSpring ? "0 0 80 0 7 9 1.4" :
                asset.tool == EditTool::ConstantTorqueSpring ? "0 0 80 0 7 8 1.2" :
                asset.tool == EditTool::LeafSpring ? "0 0 80 0 7 12 2" :
                asset.tool == EditTool::BeamSpring ? "0 0 80 0 7 10 1.7" :
                asset.tool == EditTool::DiscSpring ? "0 0 80 0 7 18 1" :
                asset.tool == EditTool::WaveSpring ? "0 0 80 0 7 10 1" :
                asset.tool == EditTool::WaveWasher ? "0 0 80 0 7 7 1" :
                asset.tool == EditTool::TorsionBar ? "0 0 80 0 7 15 2.2" :
                asset.tool == EditTool::RingSpring ? "0 0 80 0 7 13 4.2" :
                asset.tool == EditTool::ElastomerSpring ? "0 0 80 0 7 9 4.8" :
                asset.tool == EditTool::PneumaticSpring ? "0 0 80 0 7 11 2.4" :
                asset.tool == EditTool::GasSpring ? "0 0 80 0 7 10 3.4" :
                asset.tool == EditTool::HydropneumaticSpring ? "0 0 80 0 7 14 5.2" :
                asset.tool == EditTool::MagneticSpring ? "0 0 80 0 7 12 0.4" :
                asset.tool == EditTool::CompositeSpring ? "0 0 80 0 7 10 1.6" : "0 0 80 0 7 8 1";
            std::istringstream stream(arguments);
            GuideObject object{};
            ParseGuideObject(command, stream, object);
            const Vector2 offset{sample.x + 6.0f, sample.y + sample.height * 0.5f};
            object.constraint.anchorA = offset;
            object.constraint.anchorB = {offset.x + sample.width - 12.0f, offset.y};
            DrawGuideObject(object);
            break;
        }
        case EditTool::Rod:
        case EditTool::FixedJoint: {
            GuideObject object{};
            std::istringstream stream(asset.tool == EditTool::Rod ? "0 0 80 0 6" : "0 0 10");
            ParseGuideObject(asset.tool == EditTool::Rod ? "rod" : "fixedJoint", stream, object);
            if (asset.tool == EditTool::Rod) {
                object.constraint.anchorA = {sample.x + 6.0f, sample.y + sample.height * 0.5f};
                object.constraint.anchorB = {sample.x + sample.width - 6.0f, sample.y + sample.height * 0.5f};
            }
            else {
                object.transform.position = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
                object.origin = object.transform.position;
            }
            DrawGuideObject(object);
            break;
        }
        case EditTool::Crank:
        case EditTool::Ratchet:
        case EditTool::Clutch:
        case EditTool::Brake: {
            GuideObject object{};
            const char* command = asset.tool == EditTool::Crank ? "crank" :
                asset.tool == EditTool::Ratchet ? "ratchet" :
                asset.tool == EditTool::Clutch ? "clutch" : "brake";
            const char* arguments = asset.tool == EditTool::Crank ? "0 0 24 105" :
                asset.tool == EditTool::Ratchet ? "0 0 24 90" :
                asset.tool == EditTool::Clutch ? "0 0 24 100 1" : "0 0 66 42 0.72";
            std::istringstream stream(arguments);
            ParseGuideObject(command, stream, object);
            object.transform.position = {sample.x + sample.width * 0.5f, sample.y + sample.height * 0.5f};
            object.origin = object.transform.position;
            DrawGuideObject(object);
            break;
        }
        case EditTool::Select:
            break;
        }
        DrawUiText(uiFont, ToolName(asset.tool), tile.x + 7.0f, tile.y + 84.0f, 13.0f,
            selected ? RAYWHITE : PrimaryText);
    }
}

void LevelEditor::DrawWorldOutliner(Rectangle panel) const {
    const int panelX = static_cast<int>(panel.x);
    const int x = panelX + 14;
    DrawRectangleRec(panel, PanelBackground);
    DrawRectangleLinesEx(panel, 1.0f, PanelBorder);
    DrawRectangle(panelX, static_cast<int>(panel.y), static_cast<int>(panel.width), 38, PanelHeader);
    DrawUiText(uiFont, "WORLD OUTLINER", static_cast<float>(x), panel.y + 11.0f, 16.0f, PrimaryText);
    const int visibleRows = std::max(1, static_cast<int>((panel.height - 52.0f) / 22.0f));
    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(panel.y + 42.0f),
        static_cast<int>(panel.width), static_cast<int>(panel.height - 42.0f));
    for (int visibleRow = 0; visibleRow < visibleRows; ++visibleRow) {
        const int row = outlinerScroll + visibleRow;
        if (row >= OutlinerItemCount()) break;
        const Selection item = OutlinerSelectionAt(row);
        const Rectangle rowBounds{panel.x + 6.0f, panel.y + 44.0f + visibleRow * 22.0f,
            panel.width - 12.0f, 21.0f};
        const bool selected = item.kind == selection.kind && item.index == selection.index;
        if (selected || CheckCollisionPointRec(GetMousePosition(), rowBounds)) {
            DrawRectangleRec(rowBounds, selected ? Color{48, 91, 139, 255} : Color{42, 47, 57, 255});
        }
        DrawCircleV({rowBounds.x + 11.0f, rowBounds.y + 10.0f}, 3.0f,
            selected ? Warning : SecondaryText);
        const std::string label = OutlinerLabelAt(row);
        DrawUiText(uiFont, label.c_str(), rowBounds.x + 22.0f, rowBounds.y + 3.0f, 14.0f,
            selected ? RAYWHITE : PrimaryText);
    }
    EndScissorMode();
}

void LevelEditor::DrawDetails(Rectangle panel) const {
    const int panelX = static_cast<int>(panel.x);
    const int x = panelX + 14;
    DrawRectangleRec(panel, PanelBackground);
    DrawRectangleLinesEx(panel, 1.0f, PanelBorder);
    DrawRectangle(panelX, static_cast<int>(panel.y), static_cast<int>(panel.width), 38, PanelHeader);
    DrawUiText(uiFont, "DETAILS", static_cast<float>(x), panel.y + 11.0f, 16.0f, PrimaryText);
    int y = static_cast<int>(panel.y) + 54;

    const char* selectedName = "No selection";
    switch (selection.kind) {
    case SelectionKind::Solid: selectedName = "Solid"; break;
    case SelectionKind::Platform: selectedName = "Platform"; break;
    case SelectionKind::Ladder: selectedName = "Ladder"; break;
    case SelectionKind::CameraZone: selectedName = "Camera Zone"; break;
    case SelectionKind::Darkness: selectedName = "Darkness Region"; break;
    case SelectionKind::PlayerStart: selectedName = "Player Start"; break;
    case SelectionKind::Exit: selectedName = "Exit Trigger"; break;
    case SelectionKind::SceneObject:
        selectedName = selectedSceneName.empty() ? "Scene Object" : selectedSceneName.c_str();
        break;
    case SelectionKind::None: break;
    }
    DrawUiText(uiFont, selectedName, static_cast<float>(x), static_cast<float>(y), 18.0f,
        selection.kind == SelectionKind::None ? SecondaryText : PrimaryText);
    y += 38;

    int propertyIndex = 0;
    auto property = [&](const char* label, const char* value) {
        DrawUiText(uiFont, label, static_cast<float>(x), static_cast<float>(y), 15.0f, SecondaryText);
        const Rectangle field{panel.x + 106.0f, static_cast<float>(y - 3), panel.width - 120.0f, 23.0f};
        DrawRectangleRounded(field, 0.10f, 4, PanelInset);
        DrawRectangleRoundedLinesEx(field, 0.10f, 4, 1.0f,
            activeDetailProperty == propertyIndex ? Accent : PanelBorder);
        const char* displayedValue = activeDetailProperty == propertyIndex ? detailEditText.c_str() : value;
        DrawUiText(uiFont, displayedValue, field.x + 7.0f, field.y + 4.0f, 14.0f, PrimaryText);
        ++propertyIndex;
        y += 28;
    };
    if (selection.kind == SelectionKind::PlayerStart) {
        property("Position X", TextFormat("%.0f", level.playerStart.x));
        property("Position Y", TextFormat("%.0f", level.playerStart.y));
    }
    else if (selection.kind == SelectionKind::Exit) {
        property("Position X", TextFormat("%.0f", level.exitTrigger.x));
        property("Position Y", TextFormat("%.0f", level.exitTrigger.y));
        DrawUiText(uiFont, "Standard size: 85 x 210 (fixed)", static_cast<float>(x), static_cast<float>(y),
            14.0f, SecondaryText);
    }
    else if (const Rectangle* rect = SelectedRectangle()) {
        property("Position X", TextFormat("%.0f", rect->x));
        property("Position Y", TextFormat("%.0f", rect->y));
        property("Width", TextFormat("%.0f", rect->width));
        property("Height", TextFormat("%.0f", rect->height));
    }
    else if (selection.kind == SelectionKind::SceneObject) {
        property("Position X", TextFormat("%.0f", selectedSceneBounds.x));
        property("Position Y", TextFormat("%.0f", selectedSceneBounds.y));
        property("Width", TextFormat("%.0f", selectedSceneBounds.width));
        property("Height", TextFormat("%.0f", selectedSceneBounds.height));
        y += 12;
        DrawUiText(uiFont, "Runtime object (read-only)", static_cast<float>(x), static_cast<float>(y),
            14.0f, SecondaryText);
    }
    else {
        property("World Width", TextFormat("%.0f", level.worldBounds.width));
        property("World Height", TextFormat("%.0f", level.worldBounds.height));
        property("Grid Snap", TextFormat("%.0f px", gridSnap));
        y += 12;
        DrawUiText(uiFont, "Select an actor to inspect", static_cast<float>(x), static_cast<float>(y), 14.0f,
            SecondaryText);
    }
}

void LevelEditor::DrawDockingOverlay() const {
    for (size_t index = 0; index < dockPanels.size(); ++index) {
        const PanelId panel = static_cast<PanelId>(index);
        const Rectangle bounds = GetPanelBounds(panel);
        DrawUiText(uiFont, "::", bounds.x + bounds.width - 25.0f, bounds.y + 10.0f, 13.0f, SecondaryText);
        if (dockPanels[index].slot == DockSlot::Floating) {
            DrawTriangle({bounds.x + bounds.width, bounds.y + bounds.height - 12.0f},
                {bounds.x + bounds.width, bounds.y + bounds.height},
                {bounds.x + bounds.width - 12.0f, bounds.y + bounds.height}, SecondaryText);
        }
    }

    if (draggedPanel == PanelId::Count) return;
    const float width = static_cast<float>(GetScreenWidth());
    const float bottom = static_cast<float>(GetScreenHeight()) - StatusBarHeight;
    const Rectangle leftTarget{0.0f, ToolbarHeight, 120.0f, bottom - ToolbarHeight};
    const Rectangle bottomTarget{0.0f, bottom - 120.0f, width, 120.0f};
    const Rectangle rightTop{width - 120.0f, ToolbarHeight, 120.0f, (bottom - ToolbarHeight) * 0.5f};
    const Rectangle rightBottom{width - 120.0f, rightTop.y + rightTop.height, 120.0f, rightTop.height};
    for (Rectangle target : {leftTarget, bottomTarget, rightTop, rightBottom}) {
        DrawRectangleRec(target, Fade(Accent, 0.12f));
        DrawRectangleLinesEx(target, 2.0f, Fade(Accent, 0.75f));
    }
}

void LevelEditor::DrawStatusBar() const {
    const int y = GetScreenHeight() - static_cast<int>(StatusBarHeight);
    DrawRectangle(0, y, GetScreenWidth(), static_cast<int>(StatusBarHeight), Color{23, 27, 34, 255});
    DrawLine(0, y, GetScreenWidth(), y, PanelBorder);
    DrawUiText(uiFont, statusText.c_str(), 12.0f, static_cast<float>(y + 7), 15.0f, SecondaryText);

    const Rectangle canvas = GetCanvasBounds();
    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, canvas)) {
        const Vector2 world = GetScreenToWorld2D(mouse, camera);
        const char* coordinates = TextFormat("x %.0f   y %.0f   zoom %.0f%%", world.x, world.y, camera.zoom * 100.0f);
        DrawUiText(uiFont, coordinates, GetScreenWidth() - MeasureUiText(uiFont, coordinates, 15.0f) - 12.0f,
            static_cast<float>(y + 7), 15.0f, PrimaryText);
    }
}
