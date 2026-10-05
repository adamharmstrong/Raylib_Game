#pragma once

#include "Level.h"
#include "EditorConnections.h"

#include <filesystem>
#include <array>
#include <climits>
#include <string>
#include <unordered_set>
#include <vector>

class LevelEditor {
public:
    enum class EditTool {
        Select,
        Solid,
        Platform,
        Ladder,
        CameraZone,
        Darkness,
        PlayerStart,
        Exit,
        Water,
        Sand,
        Gel,
        Gas,
        Pulley,
        HangingWeight,
        RotaryLatch,
        StoneBlock,
        Boulder,
        PhysicsWheel,
        Gear,
        Flywheel,
        SteeringWheel,
        Screw,
        Fan,
        Pinwheel,
        Ramp,
        SeeSaw,
        TrapDoor,
        Chain,
        PhysicsRope,
        Button,
        Portal,
        DirectionalSpikes,
        ArrowTrap,
        BreakableTile,
        Enemy,
        Label,
        Valve,
        Checkpoint,
        Collectible,
        Ball,
        Barrel,
        MovingPlatform,
        Elevator,
        PendulumBob,
        OneWayPlatform,
        CeilingHook,
        GuideRail,
        Spring,
        CompressionSpring,
        ExtensionSpring,
        TorsionSpring,
        GarterSpring,
        VoluteSpring,
        SpiralSpring,
        ConstantForceSpring,
        ConstantTorqueSpring,
        LeafSpring,
        BeamSpring,
        DiscSpring,
        WaveSpring,
        WaveWasher,
        TorsionBar,
        RingSpring,
        ElastomerSpring,
        PneumaticSpring,
        GasSpring,
        HydropneumaticSpring,
        MagneticSpring,
        CompositeSpring,
        Rod,
        FixedJoint,
        Crank,
        Ratchet,
        Clutch,
        Brake
    };

    enum class TransformMode { Select, Move, Rotate, Scale };

    int Run(const std::filesystem::path& initialLevel = {}, bool viewportOnly = false);

private:
    enum class SelectionKind {
        None,
        Solid,
        Platform,
        Ladder,
        CameraZone,
        Darkness,
        PlayerStart,
        Exit,
        SceneObject
    };
    enum class TilePaintTool { None, Pencil, Brush, Eraser, Eyedropper, RectangleFill, FloodFill, TileSelect, CollisionBrush, Line, Replace, Stamp, RandomBrush, RectangleOutline, Ellipse, Measure, AutoTile, MagicWand, SelectMatching };
    enum class TransformHandle { None, MoveCenter, MoveX, MoveY, ScaleBottomRight, RotateRing };

    struct Selection {
        SelectionKind kind{SelectionKind::None};
        int index{-1};
        std::string sceneName;
        Rectangle sceneBounds{};
        std::string outlinerName;
        int outlinerIndex{-1};
    };

    struct TileEditSnapshot {
        std::vector<VisualTile> visualTiles;
        std::vector<Rectangle> solids;
        std::vector<Rectangle> platforms;
        std::vector<Rectangle> ladders;
        std::vector<Rectangle> cameraZones;
        std::vector<Rectangle> darknessAreas;
        Rectangle exitTrigger{};
        Vector2 playerStart{};
        std::vector<FluidField> fluids;
        std::vector<Vector2> pulleys;
        std::vector<HangingWeight> weights;
        std::vector<RotaryLatch> rotaryLatches;
        std::vector<StoneBlock> stoneBlocks;
        std::vector<Boulder> boulders;
        std::vector<PhysicsWheel> physicsWheels;
        std::vector<Gear> gears;
        std::vector<Flywheel> flywheels;
        std::vector<SteeringWheel> steeringWheels;
        std::vector<Screw> screws;
        std::vector<Fan> fans;
        std::vector<Pinwheel> pinwheels;
        std::vector<Ramp> ramps;
        std::vector<SeeSaw> seeSaws;
        std::vector<TrapDoor> trapDoors;
        std::vector<Chain> chains;
        std::vector<PhysicsRope> physicsRopes;
        std::vector<Button> buttons;
        std::vector<PortalPair> portalPairs;
        std::vector<DirectionalSpikeHazard> directionalSpikeHazards;
        std::vector<ArrowTrap> arrowTraps;
        std::vector<BreakableTile> breakableTiles;
        std::vector<Enemy> enemies;
        std::vector<LevelLabel> labels;
        Valve valve;
        std::vector<GuideObject> guideObjects;
    };

    enum class PanelId { PlaceAssets, ContentBrowser, WorldOutliner, Details, Count };
    enum class DockSlot { Left, Bottom, RightTop, RightBottom, Floating };
    enum class ScrollbarTarget { None, AssetLibrary, ContentBrowser, WorldOutliner };
    enum class ResizeTarget {
        None,
        Left,
        Right,
        Bottom,
        RightSplit,
        FloatingLeft,
        FloatingRight,
        FloatingTop,
        FloatingBottom,
        FloatingTopLeft,
        FloatingTopRight,
        FloatingBottomLeft,
        FloatingBottomRight
    };

    struct DockPanel {
        DockSlot slot{DockSlot::Floating};
        Rectangle floatingBounds{};
    };

    void DiscoverLevels();
    bool LoadLevel(const std::filesystem::path& path);
    bool SaveLevel();
    bool SaveLevelAs();
    bool OpenLevelDialog();
    bool ConfirmDiscardChanges();
    void FrameLevel();
    void FrameSelection();
    void DuplicateSelection();
    void InstallNativeMenu();
    void RemoveNativeMenu();
    void ProcessNativeMenuCommand();
    void DeleteSelection();
    void Update();
    void UpdateDocking();
    void UpdateEditorPanels();
    int OutlinerItemCount() const;
    Selection OutlinerSelectionAt(int row) const;
    std::string OutlinerLabelAt(int row) const;
    void CommitDetailEdit();
    void UpdateEditing(Rectangle canvas);
    void ProcessViewportHostCommand();
    void UpdateTilePainting(Rectangle canvas);
    void NotifyTilePainted(const VisualTile& tile) const;
    void NotifyTileErased(TileLayer layer, Vector2 position) const;
    void NotifyTileOperation(const std::string& message) const;
    bool IsTileCellSelected(int x, int y) const;
    void UpdateTileSelectionBounds();
    void NotifyTileSelection() const;
    void PushTileUndo();
    void UndoTileEdit();
    void RedoTileEdit();
    void RestoreTileEdit(const TileEditSnapshot& snapshot);
    void NotifyFullTileState() const;
    void NotifyEditableObjectState() const;
    void NotifySelectionChanged() const;
    void GenerateContentThumbnails() const;
    void Draw() const;
    void DrawCanvas(Rectangle canvas) const;
    void DrawLevelPreview() const;
    void DrawEditOverlay() const;
    void DrawConnectionOverlay() const;
    int SelectedConnectionNode(const EditorConnections& graph) const;
    Selection ConnectionSelection(const ConnectionNode& node) const;
    void SelectConnectedObjects();
    bool PickConnectionEndpoint(Vector2 world);
    void DrawAssetLibrary(Rectangle bounds) const;
    void DrawContentBrowser(Rectangle bounds) const;
    void DrawWorldOutliner(Rectangle bounds) const;
    void DrawDetails(Rectangle bounds) const;
    void DrawDockingOverlay() const;
    void DrawToolbar() const;
    void DrawStatusBar() const;

    Rectangle GetCanvasBounds() const;
    Rectangle GetFitButtonBounds() const;
    Rectangle GetSelectButtonBounds() const;
    Rectangle GetTransformButtonBounds(int index) const;
    Rectangle GetAssetRowBounds(int index) const;
    Rectangle GetContentAssetBounds(int index) const;
    Rectangle GetContentBrowserBounds() const;
    Rectangle GetPanelBounds(PanelId panel) const;
    Rectangle GetPanelHeaderBounds(PanelId panel) const;
    bool IsMouseOverPanel(Vector2 mouse) const;
    PanelId PanelInSlot(DockSlot slot) const;
    void DockPanelAt(PanelId panel, DockSlot slot);
    std::filesystem::path FindLevelDirectory() const;
    Vector2 SnapWorldPoint(Vector2 point) const;
    Rectangle MakeRect(Vector2 a, Vector2 b) const;
    Selection HitTest(Vector2 world);
    Rectangle* SelectedRectangle();
    const Rectangle* SelectedRectangle() const;
    Rectangle* RectangleForSelection(const Selection& item);
    const Rectangle* RectangleForSelection(const Selection& item) const;
    Rectangle SelectionGroupBounds() const;
    bool IsGroupSelected(const Selection& item) const;
    void SetPrimarySelection(const Selection& item);
    float* SelectedRotation();
    const float* SelectedRotation() const;
    Vector2 SelectedRotationCenter() const;
    bool TranslateSelectedScene(Vector2 delta);
    bool ScaleSelectedScene(Vector2 delta);
    bool DeleteSelectedSceneObject();
    bool DuplicateSelectedSceneObject();
    TransformHandle TransformHandleAt(Vector2 world) const;
    bool CanSaveCurrentLevel() const;
    void MarkDirty(const std::string& message);

    Level level{};
    Camera2D camera{};
    Font uiFont{};
    Texture2D industrialTiles{};
    Texture2D industrialBackground{};
    Texture2D industrialFarBackground{};
    Texture2D playerSprites{};
    Texture2D enemySprites{};
    Texture2D chainLinks{};
    Texture2D toolbarIcons{};
    std::filesystem::path levelDirectory;
    std::filesystem::path currentLevelPath;
    std::vector<std::filesystem::path> levelFiles;
    std::string statusText{"Use File > Open to choose a level."};
    bool ownsUiFont{false};
    bool viewportOnly{false};
    bool hostEditTransaction{false};
    bool showConnections{false};
    bool focusConnections{true};
    TilePaintTool tilePaintTool{TilePaintTool::None};
    TileLayer paintTileLayer{TileLayer::Foreground};
    int paintTileSheet{0};
    int paintTileColumn{0};
    int paintTileRow{0};
    int paintTileSelectionWidth{1};
    int paintTileSelectionHeight{1};
    int paintBrushSize{1};
    int lastPaintGridX{INT_MIN};
    int lastPaintGridY{INT_MIN};
    bool tileDragActive{false};
    int tileDragStartX{0};
    int tileDragStartY{0};
    bool collisionStrokeErase{false};
    bool tileSelectionActive{false};
    bool movingTileSelection{false};
    int tileSelectionLeft{0};
    int tileSelectionTop{0};
    int tileSelectionRight{0};
    int tileSelectionBottom{0};
    std::vector<VisualTile> tileClipboard;
    std::unordered_set<unsigned long long> tileSelectionCells;
    int visibleTileLayerMask{0xFFF};
    int lockedTileLayerMask{0};
    bool showGrid{true};
    int tileSymmetryMask{0};
    int paintAnimationFrames{1};
    float paintAnimationFrameSeconds{0.12f};
    bool previewTileAnimations{true};
    std::vector<TileEditSnapshot> tileUndoHistory;
    std::vector<TileEditSnapshot> tileRedoHistory;
    EditTool activeTool{EditTool::Select};
    Selection selection{};
    std::vector<Selection> groupSelection;
    bool draggingMarquee{false};
    Vector2 marqueeStart{};
    Vector2 marqueeCurrent{};
    std::vector<Rectangle> transformOriginalGroupRects;
    std::vector<Vector2> transformOriginalGroupPoints;
    Rectangle transformOriginalGroupBounds{};
    float transformOriginalRotation{0.0f};
    float transformDragStartAngle{0.0f};
    Vector2 transformLastWorld{};
    bool draggingCreate{false};
    Vector2 dragStart{};
    Vector2 dragCurrent{};
    bool dirty{false};
    float gridSnap{32.0f};
    TransformMode transformMode{TransformMode::Select};
    bool draggingTransform{false};
    Vector2 transformDragStart{};
    Rectangle transformOriginalRect{};
    Vector2 transformOriginalPoint{};
    TransformHandle activeTransformHandle{TransformHandle::None};
    Rectangle selectedSceneBounds{};
    std::string selectedSceneName;
    int assetLibraryScroll{0};
    int outlinerScroll{0};
    float contentBrowserScroll{0.0f};
    int contentCategory{0};
    ScrollbarTarget scrollbarTarget{ScrollbarTarget::None};
    float scrollbarDragOffset{0.0f};
    int activeDetailProperty{-1};
    std::string detailEditText;
    std::array<DockPanel, static_cast<size_t>(PanelId::Count)> dockPanels{
        DockPanel{DockSlot::Left, {40.0f, 110.0f, 250.0f, 470.0f}},
        DockPanel{DockSlot::Bottom, {180.0f, 520.0f, 720.0f, 220.0f}},
        DockPanel{DockSlot::RightTop, {980.0f, 110.0f, 310.0f, 330.0f}},
        DockPanel{DockSlot::RightBottom, {980.0f, 450.0f, 310.0f, 300.0f}}
    };
    float leftDockWidth{250.0f};
    float rightDockWidth{310.0f};
    float bottomDockHeight{220.0f};
    float rightDockSplit{0.52f};
    PanelId draggedPanel{PanelId::Count};
    DockSlot dragOrigin{DockSlot::Floating};
    Vector2 dragOffset{};
    ResizeTarget resizeTarget{ResizeTarget::None};
    PanelId resizedFloatingPanel{PanelId::Count};
    Vector2 resizeDragStart{};
    Rectangle resizeOriginalBounds{};
};
