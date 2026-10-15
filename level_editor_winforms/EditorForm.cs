using System.ComponentModel;
using System.Diagnostics;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class EditorForm : Form
{
    private enum OutlinerRootKind { Objects, Tiles, LevelSettings }

    private sealed record OutlinerRootTag(OutlinerRootKind Kind);

    private sealed record TileLayerEntry(int Id, string Key, string Label)
    {
        public override string ToString() => Label;
    }

    private static readonly string[] AssetNames =
    [
        "Solid", "Platform", "Ladder", "Camera Zone", "Darkness Region", "Player Start", "Exit Trigger",
        "Water", "Sand", "Gel", "Gas", "Pulley", "Hanging Weight", "Rotary Latch", "Stone Block",
        "Boulder", "Physics Wheel", "Gear", "Flywheel", "Steering Wheel", "Screw", "Fan", "Pinwheel",
        "Ramp", "See Saw", "Trap Door", "Chain", "Physics Rope", "Button", "Portal", "Directional Spikes",
        "Arrow Trap", "Breakable Tile", "Enemy", "Label", "Valve", "Checkpoint", "Collectible",
        "Ball", "Barrel", "Moving Platform", "Elevator", "Pendulum Bob", "One-Way Platform",
        "Ceiling Hook", "Guide Rail", "Spring", "Compression Spring", "Extension Spring", "Torsion Spring",
        "Garter Spring", "Volute Spring", "Spiral Spring", "Constant-Force Spring", "Constant-Torque Spring",
        "Leaf Spring", "Beam Spring", "Disc Spring", "Wave Spring", "Wave Washer", "Torsion Bar", "Ring Spring",
        "Elastomer Spring", "Pneumatic Spring", "Gas Spring", "Hydropneumatic Spring",
        "Magnetic Spring", "Composite Spring", "Rod", "Fixed Joint", "Crank", "Ratchet", "Clutch", "Brake",
        "Cam", "Conveyor Belt", "Turntable", "Battery", "Electric Motor", "Generator", "Limit Switch", "Relay", "Fuse", "Magnet", "Piston", "Hydraulic Cylinder", "Rocket Thruster", "Crushing Block", "Swinging Hammer", "Saw Blade", "Spinner Trap", "Steam Vent", "Electrical Arc", "Oil", "Mud", "Speed Sensor", "Beam Sensor", "Key", "Gas Mask", "Breakable Crate", "Explosive Barrel", "Water Pit", "Toxin Leak", "Clock Face", "Mounted Gear", "Spike Hazard", "Anti-Gravity Pad", "Tripod Derrick", "Chain Hoist", "Block and Tackle", "Come-Along", "Manual Bench Grinder", "Load Cart", "Revolving Stone Gate", "Rock Hammer", "Field Coil", "Spoke Press", "Polaris Telescope", "Sundial", "Perpetual Motion Holder",
        "Bellows", "Trampoline", "Boxing Glove", "Vacuum", "Magnifying Glass", "Light Source", "Ignitable Fuse",
        "Programmable Ball", "Trans-Roto Converter", "Roto-Trans Converter", "Accelerator Tube", "Pinball Bumper",
        "Pinball Flipper", "Laser Emitter", "Laser Mixer", "Laser Detector", "Hot-Air Balloon", "Blimp",
        "Leaky Bucket", "Destructible Wood", "Destructible Brick", "Destructible Glass", "Glass Pane", "Glass Block",
        "Ice Block", "Ice Sheet", "Heater", "Campfire", "Torch", "Bucket of Water", "Signal Dust",
        "Ternary Trace", "Ternary Switch", "TNT", "Wedge", "Scuba Gear", "Teleforce Projector", "Tesla Coil", "Rock", "Platform Loop", "Thorny Vines"
    ];

    private readonly LevelDocument document = new();
    private readonly ViewportHost viewport = new();
    private readonly TreeView outliner = new()
    {
        Dock = DockStyle.Fill,
        HideSelection = false,
        AllowDrop = true,
        DrawMode = TreeViewDrawMode.OwnerDrawText
    };
    private readonly ContextMenuStrip outlinerMenu = new();
    private readonly PropertyGrid details = new() { Dock = DockStyle.Fill, HelpVisible = true, ToolbarVisible = false };
    private readonly Panel anchorPickerPanel = new() { Dock = DockStyle.Top, Height = 88, Visible = false };
    private readonly Button[] anchorPointButtons = new Button[9];
    private readonly ToolTip anchorPointToolTip = new();
    private readonly ListView contentBrowser = CreateListView(View.LargeIcon);
    private readonly Panel contentBrowserTabHost = new()
    {
        Dock = DockStyle.Top,
        Height = 31,
        BackColor = DarkTheme.Field
    };
    private readonly ContentCategoryStrip contentBrowserTabs = new();
    private readonly TilePalette tilePalette = new();
    private readonly RowStyle spritePreviewRow = new(SizeType.Absolute, 112);
    private TableLayoutPanel? tilePaintingLayout;
    private readonly ComboBox tileSheetSelector = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 210 };
    private readonly List<int> tileSheetIds = [0, 1, 2];
    private readonly ToolTip sheetToolTip = new();
    private readonly ToolStripStatusLabel statusLabel = new("Ready");
    private readonly ToolStrip toolStrip = new() { GripStyle = ToolStripGripStyle.Hidden, ImageScalingSize = new Size(24, 24) };
    private readonly Dictionary<Keys, ToolStripButton> transformButtons = [];
    private readonly ToolStripButton pencilButton = new("Pencil") { CheckOnClick = true, DisplayStyle = ToolStripItemDisplayStyle.Text };
    private readonly ToolStripButton brushButton = new("Brush") { CheckOnClick = true, DisplayStyle = ToolStripItemDisplayStyle.Text };
    private readonly ToolStripButton eraserButton = new("Eraser") { CheckOnClick = true, DisplayStyle = ToolStripItemDisplayStyle.Text };
    private readonly ToolStripButton eyedropperButton = new("Eyedropper") { CheckOnClick = true };
    private readonly ToolStripButton rectangleButton = new("Rectangle") { CheckOnClick = true };
    private readonly ToolStripButton fillButton = new("Fill") { CheckOnClick = true };
    private readonly ToolStripButton tileSelectButton = new("Tile Select") { CheckOnClick = true };
    private readonly ToolStripButton collisionButton = new("Collision") { CheckOnClick = true };
    private readonly ToolStripButton lineButton = new("Line") { CheckOnClick = true };
    private readonly ToolStripButton replaceButton = new("Replace") { CheckOnClick = true };
    private readonly ToolStripButton stampButton = new("Stamp") { CheckOnClick = true };
    private readonly ToolStripButton randomBrushButton = new("Random Brush") { CheckOnClick = true };
    private readonly ToolStripButton rectangleOutlineButton = new("Rectangle Outline") { CheckOnClick = true };
    private readonly ToolStripButton ellipseButton = new("Ellipse") { CheckOnClick = true };
    private readonly ToolStripButton measureButton = new("Measure") { CheckOnClick = true };
    private readonly ToolStripButton autoTileButton = new("Auto Tile") { CheckOnClick = true };
    private readonly ToolStripButton magicWandButton = new("Magic Wand") { CheckOnClick = true };
    private readonly ToolStripButton selectMatchingButton = new("Select Matching") { CheckOnClick = true };
    private readonly ToolStripButton mirrorXButton = new("Mirror horizontally") { CheckOnClick = true };
    private readonly ToolStripButton mirrorYButton = new("Mirror vertically") { CheckOnClick = true };
    private readonly ToolStripDropDownButton brushSizeButton = new("1x1") { ToolTipText = "Brush and eraser size" };
    private readonly LayerList visibleLayers = new() { Dock = DockStyle.Fill, BorderStyle = BorderStyle.FixedSingle, IntegralHeight = false };
    private readonly RowStyle layerListRow = new(SizeType.Absolute);
    private int layerListHeightAdjustment;
    private readonly ToolStripButton renameLayer = new() { Text = "Rename Layer", ToolTipText = "Rename selected user layer" };
    private readonly ToolStripButton moveLayerUp = new("↑ Move Up");
    private readonly ToolStripButton moveLayerDown = new("↓ Move Down");
    private readonly NumericUpDown animationFrames = new() { Minimum = 1, Maximum = 64, Value = 1, Width = 54 };
    private readonly NumericUpDown animationMilliseconds = new() { Minimum = 10, Maximum = 10000, Increment = 10, Value = 120, Width = 72 };
    private readonly CheckBox previewAnimations = new() { Text = "Preview", Checked = true, AutoSize = true };
    private readonly ToolStripLabel activeToolIndicator = new("Active: Select") { Font = new Font(SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont, FontStyle.Bold) };
    private readonly ToolStripLabel cursorSizeIndicator = new("Cursor: Object bounds");
    private readonly MenuStrip menuStrip = new();
    private readonly ToolStripMenuItem recentLevelsMenu = new("Open &Recent");
    private readonly Panel topChrome = new() { Dock = DockStyle.Top };
    private readonly SplitContainer rootSplit = NewSplit(Orientation.Vertical, 240);
    private readonly SplitContainer workspaceSplit = NewSplit(Orientation.Vertical, 900);
    private readonly SplitContainer centerSplit = NewSplit(Orientation.Horizontal, 610);
    private readonly SplitContainer inspectorSplit = NewSplit(Orientation.Horizontal, 350);
    private readonly System.Windows.Forms.Timer outlinerRefreshTimer = new() { Interval = 120 };
    private readonly System.Windows.Forms.Timer thumbnailRefreshTimer = new() { Interval = 300 };
    private readonly System.Windows.Forms.Timer detailsPreviewTimer = new() { Interval = 140 };
    private readonly ImageList contentImages = new() { ImageSize = new Size(112, 84), ColorDepth = ColorDepth.Depth32Bit };
    private string? initialPath;
    private bool dirty;
    private int paintTool;
    private int paintBrushSize = 1;
    private int paintTileColumn;
    private int paintTileRow;
    private int paintTileWidth = 1;
    private int paintTileHeight = 1;
    private int paintQuarterTurns;
    private bool paintFlipX;
    private bool paintFlipY;
    private bool transformTileBrush = true;
    private int lockedLayerMask;
    private int collisionLayerMask = 0x1FFF;
    private bool synchronizingSelection;
    private LevelObject? pendingConnectionSource;
    private bool selectingConnectionRecord;
    private static readonly string[] EditableConnectionTypes = ["buttonTrapDoor", "buttonFan", "buttonExit",
        "buttonLadder", "buttonPlatform", "buttonSpikeHazard", "valveFluidFill", "buttonPlatformLoop", "platformLoopButton"];
    private bool synchronizingTileState;
    private bool synchronizingObjectState;
    private readonly Dictionary<string, List<string>> synchronizedObjectRecords = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<string> recentLevelPaths = [];
    private int gridSnap = 32;
    private string? detailsPreviewPath;
    private readonly EditorHistory editHistory = new();
    private readonly ToolStripButton connectionsButton = new("Connections")
    {
        CheckOnClick = true,
        ToolTipText = "Show power channels, attachments, pulley links, and movement paths. Alt+click an endpoint to select."
    };
    private readonly ToolStripMenuItem focusConnectionsMenu = new("Focus on Selected Network") { Checked = true, CheckOnClick = true };
    private bool viewportHistoryTransaction;
    private bool restoringHistory;
    private readonly ToolStripButton playButton = new("Play") { ToolTipText = "Play current level (F5)" };
    private readonly ToolStripButton pauseButton = new("Pause") { Enabled = false, ToolTipText = "Pause or resume playtesting" };
    private readonly ToolStripButton buildScriptsButton = new("Build Scripts") { ToolTipText = "Compile attached C++ scripts" };
    private CancellationTokenSource? scriptBuildCancellation;
    private readonly ToolStripButton stopButton = new("Stop") { Enabled = false, ToolTipText = "Stop playtesting (Shift+F5)" };
    private readonly TrackBar zoomSlider = new() { Minimum = 5, Maximum = 800, Value = 100, TickStyle = TickStyle.None, AutoSize = false, Width = 132, Height = 27, BackColor = DarkTheme.Header, ForeColor = DarkTheme.Text };
    private readonly NumericUpDown zoomInput = new() { Minimum = 5, Maximum = 800, Increment = 5, Value = 100, Width = 68, TextAlign = HorizontalAlignment.Right, BackColor = DarkTheme.Field, ForeColor = DarkTheme.Text };
    private bool synchronizingZoomControls;
    private readonly System.Windows.Forms.Timer playtestTimer = new() { Interval = 200 };
    private Process? playtestProcess;
    private string? playtestSnapshotPath;
    private bool playtestPaused;

    internal EditorForm(string? initialPath)
    {
        this.initialPath = initialPath;
        Text = "Power Pulley Panic - Level Editor";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(1100, 700);
        ClientSize = new Size(1440, 900);
        AutoScaleMode = AutoScaleMode.Dpi;
        KeyPreview = true;

        BuildMenus();
        BuildToolbar();
        BuildOutlinerMenu();
        playtestTimer.Tick += (_, _) =>
        {
            if (playtestProcess is { HasExited: true }) FinishPlaytest();
        };
        BuildLayout();
        PopulateAssets();
        DarkTheme.Apply(this);

        Shown += (_, _) =>
        {
            ApplyDefaultLayout();
            OpenInitialLevel();
            BeginInvoke(ApplyDefaultLayout);
        };
        FormClosing += OnFormClosing;
        KeyDown += OnEditorKeyDown;
        outliner.AfterSelect += OnOutlinerSelectionChanged;
        outliner.DrawNode += DrawOutlinerNode;
        outliner.NodeMouseClick += (_, eventArgs) =>
        {
            if (eventArgs.Button == MouseButtons.Right) outliner.SelectedNode = eventArgs.Node;
        };
        outliner.NodeMouseDoubleClick += (_, eventArgs) =>
        {
            if (eventArgs.Node?.Tag is LevelObject) RenameOutlinerNode(eventArgs.Node);
        };
        outliner.ItemDrag += (_, eventArgs) =>
        {
            if (eventArgs.Item is TreeNode node && node.Tag is LevelObject or OutlinerCategory)
                outliner.DoDragDrop(node, DragDropEffects.Move);
        };
        outliner.DragEnter += OnOutlinerDragEnter;
        outliner.DragOver += OnOutlinerDragOver;
        outliner.DragDrop += OnOutlinerDragDrop;
        details.PropertyValueChanged += OnDetailsPropertyValueChanged;
        contentBrowser.SelectedIndexChanged += (_, _) =>
        {
            if (contentBrowser.SelectedItems.Count > 0)
                SelectBrowserAsset(contentBrowser.SelectedItems[0].Text);
        };
        contentBrowser.ItemDrag += (_, args) =>
        {
            if (args.Item is not ListViewItem item || item.Tag is not int assetIndex) return;
            contentBrowser.DoDragDrop(CreateAssetDragData(assetIndex), DragDropEffects.Copy);
        };
        contentBrowserTabs.SelectedCategoryChanged += (_, _) => FilterContentBrowser();
        tilePalette.PassageAt = (column, row) => document.GetTilePassage(SelectedSheetId(), column, row);
        tilePalette.TileSelected += (column, row, width, height) =>
        {
            paintTileColumn = column;
            paintTileRow = row;
            paintTileWidth = width;
            paintTileHeight = height;
            SendPaintConfiguration();
            UpdateToolIndicators();
            statusLabel.Text = $"Tiles selected: {width}x{height} at {column}, {row}";
        };
        tileSheetSelector.SelectedIndexChanged += (_, _) =>
        {
            LoadSelectedTileSheet();
            SendPaintConfiguration();
        };
        visibleLayers.SelectedIndexChanged += (_, _) => OnActiveLayerChanged();
        viewport.TilePainted += OnTilePainted;
        viewport.SelectionChangedInViewport += OnViewportSelectionChanged;
        outlinerRefreshTimer.Tick += (_, _) =>
        {
            outlinerRefreshTimer.Stop();
            RebuildOutliner();
        };
        thumbnailRefreshTimer.Tick += (_, _) =>
        {
            if (LoadContentThumbnails()) thumbnailRefreshTimer.Stop();
        };
        detailsPreviewTimer.Tick += (_, _) => RefreshViewportFromDetails();
        // PopulateAssets selects the default sheet before selection events are wired.
        LoadSelectedTileSheet();
    }

    private static ListView CreateListView(View view) => new()
    {
        Dock = DockStyle.Fill,
        View = view,
        MultiSelect = false,
        HideSelection = false,
        FullRowSelect = true,
        AutoArrange = true,
        BorderStyle = BorderStyle.None
    };

    private static SplitContainer NewSplit(Orientation orientation, int _) => new()
    {
        Dock = DockStyle.Fill,
        Orientation = orientation,
        SplitterWidth = 5,
        FixedPanel = FixedPanel.None
    };

    private void BuildMenus()
    {
        var file = new ToolStripMenuItem("&File");
        file.DropDownItems.Add(MenuItem("&New Level...", Keys.Control | Keys.N, (_, _) => CreateLevel()));
        file.DropDownItems.Add(MenuItem("&Open...", Keys.Control | Keys.O, (_, _) => OpenLevel()));
        recentLevelsMenu.DropDownOpening += (_, _) => PopulateRecentLevelsMenu();
        file.DropDownItems.Add(recentLevelsMenu);
        file.DropDownItems.Add(new ToolStripSeparator());
        file.DropDownItems.Add(MenuItem("Choose Level Music...", Keys.None, (_, _) => ImportAudio(true)));
        file.DropDownItems.Add(MenuItem("Remove Level Music", Keys.None, (_, _) => { document.SetMusic(null); SetDirty(true, "Level music removed"); RefreshViewportFromDetails(); }));
        file.DropDownItems.Add(MenuItem("&Save", Keys.Control | Keys.S, (_, _) => SaveLevel()));
        file.DropDownItems.Add(MenuItem("Save &As...", Keys.Control | Keys.Shift | Keys.S, (_, _) => SaveLevelAs()));
        file.DropDownItems.Add(new ToolStripSeparator());
        file.DropDownItems.Add(MenuItem("&Reload", Keys.Control | Keys.R, (_, _) => ReloadLevel()));
        file.DropDownItems.Add(new ToolStripSeparator());
        file.DropDownItems.Add(MenuItem("E&xit", Keys.Alt | Keys.F4, (_, _) => Close()));

        var edit = new ToolStripMenuItem("&Edit");
        edit.DropDownItems.Add(MenuItem("&Undo", Keys.Control | Keys.Z, (_, _) => UndoEditorChange()));
        edit.DropDownItems.Add(MenuItem("&Redo", Keys.Control | Keys.Y, (_, _) => RedoEditorChange()));
        edit.DropDownItems.Add(new ToolStripSeparator());
        edit.DropDownItems.Add(MenuItem("&Select", Keys.Q, (_, _) => SelectTransform(Keys.Q, "Select")));
        edit.DropDownItems.Add(MenuItem("&Move", Keys.W, (_, _) => SelectTransform(Keys.W, "Move")));
        edit.DropDownItems.Add(MenuItem("&Rotate", Keys.E, (_, _) => SelectTransform(Keys.E, "Rotate")));
        edit.DropDownItems.Add(MenuItem("&Scale", Keys.R, (_, _) => SelectTransform(Keys.R, "Scale")));
        edit.DropDownItems.Add(new ToolStripSeparator());
        edit.DropDownItems.Add(MenuItem("&Copy Tiles", Keys.Control | Keys.C, (_, _) => viewport.SendCommand(6, 2)));
        edit.DropDownItems.Add(MenuItem("&Paste Tiles", Keys.Control | Keys.V, (_, _) => viewport.SendCommand(6, 3)));
        edit.DropDownItems.Add(MenuItem("&Duplicate Tiles", Keys.Control | Keys.D, (_, _) => viewport.SendCommand(6, 8)));
        edit.DropDownItems.Add(MenuItem("&Delete", Keys.Delete, (_, _) => viewport.SendKey(Keys.Delete)));
        var nudge = new ToolStripMenuItem("&Nudge Tile Selection");
        nudge.DropDownItems.Add(MenuItem("Left", Keys.None, (_, _) => NudgeTileSelection(9, "left"), "Left Arrow"));
        nudge.DropDownItems.Add(MenuItem("Right", Keys.None, (_, _) => NudgeTileSelection(10, "right"), "Right Arrow"));
        nudge.DropDownItems.Add(MenuItem("Up", Keys.None, (_, _) => NudgeTileSelection(11, "up"), "Up Arrow"));
        nudge.DropDownItems.Add(MenuItem("Down", Keys.None, (_, _) => NudgeTileSelection(12, "down"), "Down Arrow"));
        edit.DropDownItems.Add(nudge);

        var view = new ToolStripMenuItem("&View");
        view.DropDownItems.Add(MenuItem("&Frame Selection", Keys.F, (_, _) => FrameSelection()));
        var showGrid = new ToolStripMenuItem("Show &Grid") { Checked = true, CheckOnClick = true };
        showGrid.CheckedChanged += (_, _) => viewport.SendCommand(9, showGrid.Checked ? 1 : 0);
        view.DropDownItems.Add(showGrid);
        var connections = new ToolStripMenuItem("Connections");
        var showConnections = new ToolStripMenuItem("Show Connections");
        showConnections.Click += (_, _) => connectionsButton.PerformClick();
        connections.DropDownOpening += (_, _) => showConnections.Checked = connectionsButton.Checked;
        focusConnectionsMenu.CheckedChanged += (_, _) => viewport.SendCommand(35, focusConnectionsMenu.Checked ? 1 : 0);
        connections.DropDownItems.Add(showConnections);
        connections.DropDownItems.Add(focusConnectionsMenu);
        connections.DropDownItems.Add(MenuItem("Select Connected Objects", Keys.Control | Keys.Shift | Keys.L, (_, _) => SelectConnectedObjects()));
        connections.DropDownItems.Add(MenuItem("Link Selected Object", Keys.None, (_, _) => BeginViewportLink()));
        connections.DropDownItems.Add(MenuItem("Choose Target from List...", Keys.None, (_, _) => LinkSelectedObject()));
        connections.DropDownItems.Add(MenuItem("Remove Selected Link", Keys.None, (_, _) => RemoveSelectedLink()));
        view.DropDownItems.Add(connections);
        view.DropDownItems.Add(MenuItem("Refresh Asset &Previews", Keys.None, (_, _) => QueueThumbnailLoad()));
        view.DropDownItems.Add(new ToolStripSeparator());
        view.DropDownItems.Add(MenuItem("&Expand World Outliner", Keys.None, (_, _) => outliner.ExpandAll()));
        view.DropDownItems.Add(MenuItem("&Collapse World Outliner", Keys.None, (_, _) => outliner.CollapseAll()));
        view.DropDownItems.Add(new ToolStripSeparator());
        view.DropDownItems.Add(PanelMenu("Tile Painting", rootSplit.Panel1));
        view.DropDownItems.Add(PanelMenu("Content Browser", centerSplit.Panel2));
        view.DropDownItems.Add(PanelMenu("World Outliner", inspectorSplit.Panel1));
        view.DropDownItems.Add(PanelMenu("Details", inspectorSplit.Panel2));

        var tile = new ToolStripMenuItem("&Tile");
        var tileTools = new ToolStripMenuItem("&Tools");
        foreach (var button in new[] { pencilButton, brushButton, eraserButton, eyedropperButton, rectangleButton,
                     rectangleOutlineButton, ellipseButton, lineButton, fillButton, replaceButton, stampButton,
                     randomBrushButton, autoTileButton, tileSelectButton, magicWandButton, selectMatchingButton,
                     collisionButton, measureButton })
            tileTools.DropDownItems.Add(TileToolMenuItem(button));
        tileTools.DropDownOpening += (_, _) => UpdateTileToolMenuChecks(tileTools.DropDownItems);
        tile.DropDownItems.Add(tileTools);

        var brushSize = new ToolStripMenuItem("Brush &Size");
        foreach (var size in new[] { 1, 2, 3, 5 })
        {
            var sizeItem = new ToolStripMenuItem($"{size} x {size}") { Tag = size };
            sizeItem.Click += (_, _) => SetBrushSize((int)sizeItem.Tag!);
            brushSize.DropDownItems.Add(sizeItem);
        }
        brushSize.DropDownOpening += (_, _) =>
        {
            foreach (ToolStripMenuItem item in brushSize.DropDownItems)
                item.Checked = (int)item.Tag! == paintBrushSize;
        };
        tile.DropDownItems.Add(brushSize);
        tile.DropDownItems.Add(MenuItem("Load Sprite &Sheet...", Keys.None, (_, _) => ImportSpriteSheet()));
        var brushTransforms = new ToolStripMenuItem("Transform Tile &Brush");
        foreach (var (label, command) in new[] { ("Rotate Left", 4), ("Rotate Right", 5), ("Flip Horizontally", 6), ("Flip Vertically", 7) })
            brushTransforms.DropDownItems.Add(MenuItem(label, Keys.None, (_, _) => ApplyTileBrushCommand(command)));
        tile.DropDownItems.Add(brushTransforms);
        tile.DropDownItems.Add(new ToolStripSeparator());
        tile.DropDownItems.Add(MenuItem("Rotate Selection &Left", Keys.None, (_, _) => ApplyTileSelectionCommand(4, "Rotate left")));
        tile.DropDownItems.Add(MenuItem("Rotate Selection &Right", Keys.None, (_, _) => ApplyTileSelectionCommand(5, "Rotate right")));
        tile.DropDownItems.Add(MenuItem("Flip Selection &Horizontally", Keys.None, (_, _) => ApplyTileSelectionCommand(6, "Flip horizontally")));
        tile.DropDownItems.Add(MenuItem("Flip Selection &Vertically", Keys.None, (_, _) => ApplyTileSelectionCommand(7, "Flip vertically")));
        tile.DropDownItems.Add(MenuItem("Duplicate Selected Tiles", Keys.None, (_, _) => ApplyTileSelectionCommand(8, "Duplicate")));
        tile.DropDownItems.Add(new ToolStripSeparator());
        var mirrorHorizontal = new ToolStripMenuItem("Mirror Brush &Horizontally") { CheckOnClick = true };
        var mirrorVertical = new ToolStripMenuItem("Mirror Brush &Vertically") { CheckOnClick = true };
        mirrorHorizontal.Click += (_, _) => mirrorXButton.PerformClick();
        mirrorVertical.Click += (_, _) => mirrorYButton.PerformClick();
        tile.DropDownOpening += (_, _) =>
        {
            mirrorHorizontal.Checked = mirrorXButton.Checked;
            mirrorVertical.Checked = mirrorYButton.Checked;
        };
        tile.DropDownItems.Add(mirrorHorizontal);
        tile.DropDownItems.Add(mirrorVertical);
        tile.DropDownItems.Add(new ToolStripSeparator());
        var animation = new ToolStripMenuItem("&Animation");
        var frameCount = new ToolStripMenuItem("Frame &Count");
        foreach (var count in new[] { 1, 2, 4, 8 })
        {
            var frameItem = new ToolStripMenuItem(count.ToString()) { Tag = count };
            frameItem.Click += (_, _) => animationFrames.Value = (int)frameItem.Tag!;
            frameCount.DropDownItems.Add(frameItem);
        }
        frameCount.DropDownOpening += (_, _) =>
        {
            foreach (ToolStripMenuItem item in frameCount.DropDownItems)
                item.Checked = (int)item.Tag! == (int)animationFrames.Value;
        };
        var frameDuration = new ToolStripMenuItem("Frame &Duration");
        foreach (var milliseconds in new[] { 60, 120, 240, 500 })
        {
            var durationItem = new ToolStripMenuItem($"{milliseconds} ms") { Tag = milliseconds };
            durationItem.Click += (_, _) => animationMilliseconds.Value = (int)durationItem.Tag!;
            frameDuration.DropDownItems.Add(durationItem);
        }
        frameDuration.DropDownOpening += (_, _) =>
        {
            foreach (ToolStripMenuItem item in frameDuration.DropDownItems)
                item.Checked = (int)item.Tag! == (int)animationMilliseconds.Value;
        };
        var previewAnimation = new ToolStripMenuItem("&Preview Animations") { CheckOnClick = true };
        previewAnimation.Click += (_, _) => previewAnimations.Checked = previewAnimation.Checked;
        animation.DropDownOpening += (_, _) => previewAnimation.Checked = previewAnimations.Checked;
        animation.DropDownItems.Add(frameCount);
        animation.DropDownItems.Add(frameDuration);
        animation.DropDownItems.Add(new ToolStripSeparator());
        animation.DropDownItems.Add(previewAnimation);
        tile.DropDownItems.Add(animation);

        var layers = new ToolStripMenuItem("&Layers");
        var activeLayer = new ToolStripMenuItem("&Active Layer");
        var visibleLayer = new ToolStripMenuItem("&Visibility");
        activeLayer.DropDownOpening += (_, _) =>
        {
            activeLayer.DropDownItems.Clear();
            foreach (TileLayerEntry entry in visibleLayers.Items)
            {
                var item = new ToolStripMenuItem(entry.Label) { Tag = entry, Checked = ReferenceEquals(entry, visibleLayers.SelectedItem) };
                item.Click += (_, _) => SelectActiveLayer(entry);
                activeLayer.DropDownItems.Add(item);
            }
        };
        visibleLayer.DropDownOpening += (_, _) =>
        {
            visibleLayer.DropDownItems.Clear();
            for (var index = 0; index < visibleLayers.Items.Count; ++index)
            {
                var entry = (TileLayerEntry)visibleLayers.Items[index]!;
                var item = new ToolStripMenuItem(entry.Label) { Tag = index, CheckOnClick = true,
                    Checked = visibleLayers.GetItemChecked(index) };
                item.Click += (_, _) => visibleLayers.SetItemChecked((int)item.Tag!, item.Checked);
                visibleLayer.DropDownItems.Add(item);
            }
        };
        layers.DropDownItems.Add(activeLayer);
        layers.DropDownItems.Add(visibleLayer);
        layers.DropDownItems.Add(new ToolStripSeparator());
        var lockLayer = new ToolStripMenuItem("&Lock Active Layer") { CheckOnClick = true };
        lockLayer.Click += (_, _) => ToggleLayerLock(ActiveLayerId());
        layers.DropDownOpening += (_, _) => lockLayer.Checked = (lockedLayerMask & (1 << ActiveLayerId())) != 0;
        layers.DropDownItems.Add(lockLayer);
        layers.DropDownItems.Add(MenuItem("&Clear Active Layer...", Keys.None, (_, _) => ClearActiveLayer()));

        var objects = new ToolStripMenuItem("&Object");
        var placeObject = new ToolStripMenuItem("&Place");
        objects.DropDownItems.Add(MenuItem("Attach Riding Button...", Keys.None, (_, _) => AttachRidingButton()));
        objects.DropDownItems.Add(MenuItem("Attach C++ Script...", Keys.None, (_, _) => AttachCppScript()));
        objects.DropDownItems.Add(MenuItem("Attach Sound Effect...", Keys.None, (_, _) => ImportAudio(false)));
        objects.DropDownItems.Add(MenuItem("Attach Lua Script...", Keys.None, (_, _) => AttachCppScript(true)));
        objects.DropDownItems.Add(MenuItem("Delete Selected Loop", Keys.None, (_, _) => DeleteSelectedLoop()));
        for (var index = 0; index <= 10; ++index)
        {
            var assetIndex = index;
            var item = MenuItem(AssetNames[index], Keys.None, (_, _) => ActivateAsset(AssetNames[assetIndex]));
            if (assetIndex <= 6) item.ShortcutKeyDisplayString = (assetIndex + 1).ToString();
            placeObject.DropDownItems.Add(item);
        }
        placeObject.DropDownItems.Add(new ToolStripSeparator());
        for (var index = 11; index < AssetNames.Length; ++index)
        {
            if (AssetNames[index] == "Platform Loop") continue;
            var assetIndex = index;
            placeObject.DropDownItems.Add(MenuItem(AssetNames[index], Keys.None,
                (_, _) => ActivateAsset(AssetNames[assetIndex])));
        }
        objects.DropDownItems.Add(placeObject);
        objects.DropDownItems.Add(new ToolStripSeparator());
        objects.DropDownItems.Add(MenuItem("&Select Mode", Keys.Q, (_, _) => SelectTransform(Keys.Q, "Select")));
        objects.DropDownItems.Add(MenuItem("D&uplicate Selected", Keys.Control | Keys.D, (_, _) => DuplicateSelectedObjects()));
        objects.DropDownItems.Add(MenuItem("&Delete Selected", Keys.Delete, (_, _) => viewport.SendKey(Keys.Delete)));
        objects.DropDownItems.Add(MenuItem("&Clear Selection", Keys.Escape, (_, _) => viewport.SendKey(Keys.Escape)));
        var nudgeObject = new ToolStripMenuItem("&Nudge Selected Object");
        nudgeObject.DropDownItems.Add(MenuItem("Left", Keys.None, (_, _) => NudgeSelectedObject(Keys.Left, "left"), "Left Arrow"));
        nudgeObject.DropDownItems.Add(MenuItem("Right", Keys.None, (_, _) => NudgeSelectedObject(Keys.Right, "right"), "Right Arrow"));
        nudgeObject.DropDownItems.Add(MenuItem("Up", Keys.None, (_, _) => NudgeSelectedObject(Keys.Up, "up"), "Up Arrow"));
        nudgeObject.DropDownItems.Add(MenuItem("Down", Keys.None, (_, _) => NudgeSelectedObject(Keys.Down, "down"), "Down Arrow"));
        objects.DropDownItems.Add(nudgeObject);
        objects.DropDownItems.Add(new ToolStripSeparator());
        objects.DropDownItems.Add(MenuItem("Show Selected Object &Details", Keys.None,
            (_, _) => FocusPanel(inspectorSplit.Panel2, details)));

        var preferences = new ToolStripMenuItem("&Preferences");
        var snap = new ToolStripMenuItem("Grid &Snap");
        foreach (var size in new[] { 8, 16, 32, 64 })
        {
            var snapItem = new ToolStripMenuItem($"{size} px") { Tag = size };
            snapItem.Click += (_, _) => SetGridSnap((int)snapItem.Tag!);
            snap.DropDownItems.Add(snapItem);
        }
        snap.DropDownOpening += (_, _) =>
        {
            foreach (ToolStripMenuItem item in snap.DropDownItems)
                item.Checked = (int)item.Tag! == gridSnap;
        };
        preferences.DropDownItems.Add(snap);
        preferences.DropDownItems.Add(new ToolStripSeparator());
        preferences.DropDownItems.Add(MenuItem("&Reset Tile Tool Settings", Keys.None, (_, _) => ResetTileToolSettings()));

        var help = new ToolStripMenuItem("&Help");
        help.DropDownItems.Add(MenuItem("&Keyboard Shortcuts", Keys.None, (_, _) => ShowKeyboardShortcuts()));
        help.DropDownItems.Add(new ToolStripSeparator());
        help.DropDownItems.Add(MenuItem("&About", Keys.None, (_, _) => MessageBox.Show(this,
            "Power Pulley Panic Level Editor\nWinForms editor shell with the native game viewport.",
            "About", MessageBoxButtons.OK, MessageBoxIcon.Information)));

        var window = new ToolStripMenuItem("&Window");
        window.DropDownItems.Add(MenuItem("Focus Tile &Painting", Keys.None, (_, _) => FocusPanel(rootSplit.Panel1, tilePalette)));
        window.DropDownItems.Add(MenuItem("Focus &Content Browser", Keys.None, (_, _) => FocusPanel(centerSplit.Panel2, contentBrowser)));
        window.DropDownItems.Add(MenuItem("Focus &World Outliner", Keys.None, (_, _) => FocusPanel(inspectorSplit.Panel1, outliner)));
        window.DropDownItems.Add(MenuItem("Focus &Details", Keys.None, (_, _) => FocusPanel(inspectorSplit.Panel2, details)));
        window.DropDownItems.Add(new ToolStripSeparator());
        window.DropDownItems.Add(MenuItem("&Reset Layout", Keys.None, (_, _) => ApplyDefaultLayout()));
        var play = new ToolStripMenuItem("&Play");
        play.DropDownItems.Add(MenuItem("&Play Level", Keys.F5, (_, _) => StartPlaytest()));
        play.DropDownItems.Add(MenuItem("Pause / Resume", Keys.None, (_, _) => TogglePlaytestPause()));
        play.DropDownItems.Add(MenuItem("Build Scripts", Keys.None, async (_, _) => await BuildScriptsAsync(true)));
        play.DropDownItems.Add(MenuItem("&Stop Playtest", Keys.Shift | Keys.F5, (_, _) => StopPlaytest()));
        menuStrip.Items.AddRange([file, edit, view, tile, layers, objects, play, window, preferences, help]);
        menuStrip.Padding = new Padding(6, 4, 6, 4);
        foreach (ToolStripMenuItem item in menuStrip.Items)
        {
            item.Padding = new Padding(7, 3, 7, 3);
            item.Margin = new Padding(1, 0, 1, 0);
        }
        MainMenuStrip = menuStrip;
        menuStrip.Dock = DockStyle.Top;
        topChrome.Controls.Add(menuStrip);
    }

    private void BuildOutlinerMenu()
    {
        var addCategory = new ToolStripMenuItem("New Category", null, (_, _) => AddOutlinerCategory(null));
        var addSubcategory = new ToolStripMenuItem("New Subcategory", null, (_, _) =>
        {
            if (outliner.SelectedNode?.Tag is OutlinerCategory category) AddOutlinerCategory(category.Id);
        });
        var rename = new ToolStripMenuItem("Rename", null, (_, _) => RenameOutlinerNode(outliner.SelectedNode));
        var moveToObjects = new ToolStripMenuItem("Move to Objects", null, (_, _) =>
        {
            if (outliner.SelectedNode?.Tag is not LevelObject item) return;
            document.SetOutlinerCategory(item, null);
            SetDirty(true, "Moved object to Objects");
            RebuildOutliner(item);
        });
        var deleteCategory = new ToolStripMenuItem("Delete Category", null, (_, _) => DeleteSelectedOutlinerCategory());
        outlinerMenu.Items.AddRange([addCategory, addSubcategory, new ToolStripSeparator(), rename, moveToObjects,
            new ToolStripSeparator(), deleteCategory]);
        outlinerMenu.Opening += (_, eventArgs) =>
        {
            var tag = outliner.SelectedNode?.Tag;
            addCategory.Enabled = true;
            addSubcategory.Enabled = tag is OutlinerCategory;
            rename.Enabled = tag is LevelObject or OutlinerCategory;
            moveToObjects.Enabled = tag is LevelObject item && document.OutlinerCategoryId(item) != null;
            deleteCategory.Enabled = tag is OutlinerCategory;
        };
        outliner.ContextMenuStrip = outlinerMenu;
    }

    private void BuildToolbar()
    {
        AddTool("Select", Keys.Q, 0);
        AddTool("Move", Keys.W, 1);
        AddTool("Rotate", Keys.E, 2);
        AddTool("Scale", Keys.R, 3);
        toolStrip.Items.Add(new ToolStripSeparator());
        AddTool("Frame", Keys.F, 4);
        toolStrip.Items.Add(new ToolStripSeparator());
        playButton.Click += (_, _) => StartPlaytest();
        pauseButton.Click += (_, _) => TogglePlaytestPause();
        stopButton.Click += (_, _) => StopPlaytest();
        toolStrip.Items.Add(playButton);
        toolStrip.Items.Add(pauseButton);
        toolStrip.Items.Add(stopButton);
        toolStrip.Items.Add(new ToolStripSeparator());
        buildScriptsButton.Click += async (_, _) => {
            if (scriptBuildCancellation != null) scriptBuildCancellation.Cancel();
            else await BuildScriptsAsync(true);
        };
        toolStrip.Items.Add(buildScriptsButton);
        toolStrip.Items.Add(new ToolStripSeparator());
        connectionsButton.CheckedChanged += (_, _) =>
        {
            viewport.SendCommand(34, connectionsButton.Checked ? 1 : 0);
            if (connectionsButton.Checked)
                statusLabel.Text = "Connections shown — Alt+click an endpoint to select; View > Connections changes focus";
        };
        toolStrip.Items.Add(connectionsButton);
        var linkObject = new ToolStripButton("Link") { ToolTipText = "Select a button or valve, then click its target in the viewport. Escape cancels." };
        linkObject.Click += (_, _) => BeginViewportLink();
        toolStrip.Items.Add(linkObject);
        var selectConnected = new ToolStripButton("Select Connected") { ToolTipText = "Select the current object's connected network (Ctrl+Shift+L)" };
        selectConnected.Click += (_, _) => SelectConnectedObjects();
        toolStrip.Items.Add(selectConnected);
        toolStrip.Items.Add(new ToolStripSeparator());
        toolStrip.Items.Add(new ToolStripLabel("Zoom"));
        toolStrip.Items.Add(new ToolStripControlHost(zoomSlider) { AutoSize = false, Width = 138, Height = 29, Margin = new Padding(0, 1, 0, 1) });
        toolStrip.Items.Add(new ToolStripControlHost(zoomInput) { AutoSize = false, Width = 72, Height = 25, Margin = new Padding(0, 2, 4, 2) });
        toolStrip.Items.Add(new ToolStripLabel("%"));
        zoomSlider.ValueChanged += (_, _) => SetZoomFromControl(zoomSlider.Value, true);
        zoomInput.ValueChanged += (_, _) => SetZoomFromControl((int)zoomInput.Value, true);
        SetActiveTransformButton(Keys.Q);
        toolStrip.Items.Add(new ToolStripSeparator());
        toolStrip.Items.Add(activeToolIndicator);
        toolStrip.Items.Add(new ToolStripSeparator());
        toolStrip.Items.Add(cursorSizeIndicator);
        toolStrip.Dock = DockStyle.Top;
        topChrome.Height = menuStrip.GetPreferredSize(Size.Empty).Height + 1;
    }

    private void BuildLayout()
    {
        rootSplit.FixedPanel = FixedPanel.Panel1;
        workspaceSplit.FixedPanel = FixedPanel.Panel2;
        centerSplit.FixedPanel = FixedPanel.Panel2;

        var tilePaintingPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 6,
            Padding = new Padding(7, 6, 7, 7)
        };
        tilePaintingLayout = tilePaintingPanel;
        tilePaintingPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100.0f));
        tilePaintingPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        tilePaintingPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 28.0f));
        tilePaintingPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 34.0f));
        tilePaintingPanel.RowStyles.Add(spritePreviewRow);
        tilePaintingPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 28.0f));
        tilePaintingPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100.0f));
        tilePaintingPanel.Controls.Add(BuildTileToolStrip(), 0, 0);
        tilePaintingPanel.Controls.Add(new Label { Text = "Sprite sheet", Dock = DockStyle.Fill }, 0, 1);
        tileSheetSelector.Dock = DockStyle.Fill;
        var sheetPicker = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Margin = Padding.Empty };
        sheetPicker.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100.0f));
        sheetPicker.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 32.0f));
        var loadSheet = new Button { Dock = DockStyle.Fill, Image = CreateFolderIcon(),
            AccessibleName = "Load sprite sheet", Margin = new Padding(3, 3, 0, 3) };
        loadSheet.Click += (_, _) => ImportSpriteSheet();
        sheetToolTip.SetToolTip(loadSheet, "Load image as a sprite sheet");
        sheetPicker.Controls.Add(tileSheetSelector, 0, 0);
        sheetPicker.Controls.Add(loadSheet, 1, 0);
        tilePaintingPanel.Controls.Add(sheetPicker, 0, 2);
        tilePaintingPanel.Controls.Add(tilePalette, 0, 3);
        tilePaintingPanel.Controls.Add(new Label { Text = "Layers", Dock = DockStyle.Fill }, 0, 4);
        var layerScroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true, Margin = Padding.Empty };
        layerScroll.Controls.Add(BuildLayerControls());
        tilePaintingPanel.Controls.Add(layerScroll, 0, 5);
        rootSplit.Panel1.Controls.Add(tilePaintingPanel);
        rootSplit.Panel1.Controls.Add(Header("Tile Painting"));
        tilePalette.SheetSizeChanged += UpdateSpritePreviewHeight;
        tilePalette.SizeChanged += (_, _) => UpdateSpritePreviewHeight();
        tilePaintingPanel.SizeChanged += (_, _) => UpdateSpritePreviewHeight();
        UpdateSpritePreviewHeight();
        rootSplit.Panel2.Controls.Add(workspaceSplit);

        var viewportPanel = new Panel { Dock = DockStyle.Fill, Padding = Padding.Empty };
        viewportPanel.Controls.Add(viewport);
        viewportPanel.Controls.Add(toolStrip);
        centerSplit.Panel1.Controls.Add(viewportPanel);
        centerSplit.Panel2.Controls.Add(contentBrowser);
        contentBrowserTabs.Dock = DockStyle.Top;
        contentBrowserTabHost.Controls.Add(contentBrowserTabs);
        contentBrowserTabs.SizeChanged += (_, _) =>
        {
            contentBrowserTabHost.Height = contentBrowserTabs.Height;
        };
        centerSplit.Panel2.Controls.Add(contentBrowserTabHost);
        centerSplit.Panel2.Controls.Add(Header("Content Browser"));
        workspaceSplit.Panel1.Controls.Add(centerSplit);

        inspectorSplit.Panel1.Controls.Add(outliner);
        inspectorSplit.Panel1.Controls.Add(Header("World Outliner"));
        inspectorSplit.Panel2.Controls.Add(details);
        inspectorSplit.Panel2.Controls.Add(BuildAnchorPicker());
        inspectorSplit.Panel2.Controls.Add(Header("Details"));
        workspaceSplit.Panel2.Controls.Add(inspectorSplit);

        var status = new StatusStrip();
        status.Items.Add(statusLabel);
        var frame = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
            Margin = Padding.Empty,
            Padding = Padding.Empty
        };
        frame.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100.0f));
        frame.RowStyles.Add(new RowStyle(SizeType.Absolute, topChrome.Height));
        frame.RowStyles.Add(new RowStyle(SizeType.Percent, 100.0f));
        frame.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        topChrome.Dock = DockStyle.Fill;
        rootSplit.Dock = DockStyle.Fill;
        status.Dock = DockStyle.Fill;
        frame.Controls.Add(topChrome, 0, 0);
        frame.Controls.Add(rootSplit, 0, 1);
        frame.Controls.Add(status, 0, 2);
        Controls.Add(frame);
    }

    private void ApplyDefaultLayout()
    {
        SuspendLayout();
        ConfigureSplit(rootSplit, 350, 290, 500);
        ConfigureSplit(workspaceSplit, workspaceSplit.Width - 360, 420, 280);
        ConfigureSplit(centerSplit, Math.Max(360, centerSplit.Height - 270), 300, 180);
        ConfigureSplit(inspectorSplit, (int)(inspectorSplit.Height * 0.58f), 160, 160);
        ResumeLayout(true);
    }

    private void UpdateSpritePreviewHeight()
    {
        if (tilePaintingLayout == null) return;
        var rows = tilePaintingLayout.GetRowHeights();
        if (rows.Length < 6) return;
        var available = tilePaintingLayout.ClientSize.Height - tilePaintingLayout.Padding.Vertical
            - rows[0] - rows[1] - rows[2] - rows[4];
        var maximum = Math.Max(32, available / 2);
        var desired = Math.Max(32, tilePalette.SheetSize.Height) + tilePalette.Margin.Vertical;
        if (tilePalette.SheetSize.Width > tilePalette.ClientSize.Width)
            desired += SystemInformation.HorizontalScrollBarHeight;
        var height = Math.Min(desired, maximum);
        if (spritePreviewRow.Height != height) spritePreviewRow.Height = height;
    }

    private static void ConfigureSplit(SplitContainer split, int desiredDistance, int panel1Minimum, int panel2Minimum)
    {
        split.Panel1MinSize = 0;
        split.Panel2MinSize = 0;
        var extent = split.Orientation == Orientation.Vertical ? split.ClientSize.Width : split.ClientSize.Height;
        var available = Math.Max(0, extent - split.SplitterWidth);
        var minimumTotal = panel1Minimum + panel2Minimum;
        if (minimumTotal > available && minimumTotal > 0)
        {
            var scale = available / (float)minimumTotal;
            panel1Minimum = (int)(panel1Minimum * scale);
            panel2Minimum = Math.Max(0, available - panel1Minimum);
        }
        split.Panel1MinSize = panel1Minimum;
        split.Panel2MinSize = panel2Minimum;
        split.SplitterDistance = Math.Clamp(desiredDistance, panel1Minimum, Math.Max(panel1Minimum, available - panel2Minimum));
    }

    private static Control Header(string text) => new Label
    {
        Text = text,
        Dock = DockStyle.Top,
        AutoSize = true,
        Padding = new Padding(10, 7, 10, 7),
        TextAlign = ContentAlignment.MiddleLeft,
        Font = new Font(SystemFonts.MessageBoxFont ?? SystemFonts.DefaultFont, FontStyle.Bold),
        BackColor = DarkTheme.Header,
        ForeColor = DarkTheme.Text
    };

    private static ToolStripMenuItem MenuItem(string text, Keys shortcut, EventHandler handler)
    {
        var item = new ToolStripMenuItem(text, null, handler);
        if (shortcut != Keys.None)
        {
            if ((shortcut & Keys.Modifiers) != Keys.None) item.ShortcutKeys = shortcut;
            else item.ShortcutKeyDisplayString = shortcut.ToString();
        }
        return item;
    }

    private static ToolStripMenuItem MenuItem(string text, Keys shortcut, EventHandler handler, string shortcutDisplay)
    {
        var item = MenuItem(text, shortcut, handler);
        item.ShortcutKeyDisplayString = shortcutDisplay;
        return item;
    }

    private static ToolStripMenuItem PanelMenu(string text, Control panel)
    {
        var item = new ToolStripMenuItem(text) { Checked = true, CheckOnClick = true };
        item.CheckedChanged += (_, _) => panel.Visible = item.Checked;
        return item;
    }

    private static ToolStripMenuItem TileToolMenuItem(ToolStripButton button)
    {
        var item = new ToolStripMenuItem(button.Text) { Tag = button };
        item.Click += (_, _) => button.PerformClick();
        return item;
    }

    private static void UpdateTileToolMenuChecks(ToolStripItemCollection items)
    {
        foreach (ToolStripMenuItem item in items)
            item.Checked = item.Tag is ToolStripButton button && button.Checked;
    }

    private ToolStrip BuildTileToolStrip()
    {
        var strip = new ToolStrip { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(3),
            GripStyle = ToolStripGripStyle.Hidden, ImageScalingSize = new Size(28, 28), LayoutStyle = ToolStripLayoutStyle.Flow };
        pencilButton.DisplayStyle = ToolStripItemDisplayStyle.Image;
        brushButton.DisplayStyle = ToolStripItemDisplayStyle.Image;
        eraserButton.DisplayStyle = ToolStripItemDisplayStyle.Image;
        pencilButton.ToolTipText = "Pencil - paint one tile";
        brushButton.ToolTipText = "Brush - paint with the selected size";
        eraserButton.ToolTipText = "Eraser - remove tiles with the selected size";
        pencilButton.Image = CreateTileToolIcon(0);
        brushButton.Image = CreateTileToolIcon(1);
        eraserButton.Image = CreateTileToolIcon(2);
        ConfigureTileTool(eyedropperButton, 3, "Eyedropper - pick a tile from the viewport");
        ConfigureTileTool(rectangleButton, 4, "Rectangle Fill - drag to paint a rectangular area");
        ConfigureTileTool(fillButton, 5, "Flood Fill - replace a connected tile region");
        ConfigureTileTool(tileSelectButton, 6, "Tile Selection - drag to select a tile region");
        ConfigureTileTool(collisionButton, 7, "Collision Brush - paint or remove collision cells");
        ConfigureTileTool(lineButton, 8, "Line - drag to paint a straight tile line");
        ConfigureTileTool(replaceButton, 9, "Replace - replace matching tiles on the active layer");
        ConfigureTileTool(stampButton, 10, "Stamp - place the copied tile selection");
        ConfigureTileTool(randomBrushButton, 11, "Random Brush - paint variations from a 2x2 palette region");
        ConfigureTileTool(rectangleOutlineButton, 12, "Rectangle Outline - drag to paint a hollow rectangle");
        ConfigureTileTool(ellipseButton, 13, "Ellipse - drag to paint an ellipse outline");
        ConfigureTileTool(measureButton, 14, "Measure - drag to measure tile and pixel dimensions");
        ConfigureTileTool(autoTileButton, 15, "Auto Tile - paint from a 4x4 neighbor-mask tile set");
        ConfigureTileTool(magicWandButton, 16, "Magic Wand - select connected matching tiles");
        ConfigureTileTool(selectMatchingButton, 17, "Select Matching - select every matching tile on the active layer");
        pencilButton.Click += (_, _) => SetPaintTool(1, pencilButton);
        brushButton.Click += (_, _) => SetPaintTool(2, brushButton);
        eraserButton.Click += (_, _) => SetPaintTool(3, eraserButton);
        strip.Items.Add(pencilButton);
        strip.Items.Add(brushButton);
        strip.Items.Add(eraserButton);
        strip.Items.Add(eyedropperButton);
        strip.Items.Add(rectangleButton);
        strip.Items.Add(fillButton);
        strip.Items.Add(tileSelectButton);
        strip.Items.Add(collisionButton);
        strip.Items.Add(lineButton);
        strip.Items.Add(replaceButton);
        strip.Items.Add(stampButton);
        strip.Items.Add(randomBrushButton);
        strip.Items.Add(rectangleOutlineButton);
        strip.Items.Add(ellipseButton);
        strip.Items.Add(measureButton);
        strip.Items.Add(autoTileButton);
        strip.Items.Add(magicWandButton);
        strip.Items.Add(selectMatchingButton);
        strip.Items.Add(new ToolStripSeparator());
        mirrorXButton.DisplayStyle = ToolStripItemDisplayStyle.Image;
        mirrorYButton.DisplayStyle = ToolStripItemDisplayStyle.Image;
        mirrorXButton.ToolTipText = "Mirror painting horizontally across the level";
        mirrorYButton.ToolTipText = "Mirror painting vertically across the level";
        mirrorXButton.Image = CreateSymmetryIcon(true);
        mirrorYButton.Image = CreateSymmetryIcon(false);
        mirrorXButton.CheckedChanged += (_, _) => SendSymmetryConfiguration();
        mirrorYButton.CheckedChanged += (_, _) => SendSymmetryConfiguration();
        strip.Items.Add(mirrorXButton);
        strip.Items.Add(mirrorYButton);
        strip.Items.Add(new ToolStripSeparator());
        foreach (var size in new[] { 1, 2, 3, 5 })
        {
            var item = new ToolStripMenuItem($"{size} x {size}") { Tag = size };
            item.Click += (_, _) => SetBrushSize((int)item.Tag!);
            brushSizeButton.DropDownItems.Add(item);
        }
        strip.Items.Add(brushSizeButton);
        var passage = new ToolStripButton("Passage") { ToolTipText = "Set passability for selected source tiles" };
        passage.Click += (_, _) => EditTilePassage();
        strip.Items.Add(passage);
        foreach (ToolStripItem item in strip.Items)
        {
            item.Margin = new Padding(2);
            item.Padding = new Padding(3);
        }
        return strip;
    }

    private void SendSymmetryConfiguration() => viewport.SendCommand(10,
        (mirrorXButton.Checked ? 1 : 0) | (mirrorYButton.Checked ? 2 : 0));

    private void EditTilePassage()
    {
        using var dialog = new Form { Text = "Tile Passage", StartPosition = FormStartPosition.CenterParent,
            AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            AutoScaleMode = AutoScaleMode.Dpi, FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false, MinimizeBox = false };
        // A docked panel's width is constrained by the form, so it cannot
        // contribute its natural width to the form's preferred size.
        var layout = new FlowLayoutPanel { AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.TopDown,
            WrapContents = false, Padding = new Padding(12) };
        layout.Controls.Add(new Label { Text = $"Source tiles: {paintTileWidth} × {paintTileHeight}\nBlocked sides (in artwork coordinates):",
            AutoSize = true });
        var mask = document.GetTilePassage(SelectedSheetId(), paintTileColumn, paintTileRow) ?? 0;
        var faces = new[] { "Top — land from above", "Right — approach from right",
            "Bottom — approach from below", "Left — approach from left" };
        var checks = faces.Select((text, index) => new CheckBox { Text = text, AutoSize = true,
            Checked = (mask & (1 << index)) != 0 }).ToArray();
        foreach (var check in checks) layout.Controls.Add(check);
        var buttons = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            WrapContents = false };
        var passable = new Button { Text = "○ Passable", AutoSize = true };
        var solid = new Button { Text = "× Solid", AutoSize = true };
        passable.Click += (_, _) => { foreach (var check in checks) check.Checked = false; };
        solid.Click += (_, _) => { foreach (var check in checks) check.Checked = true; };
        buttons.Controls.AddRange([passable, solid]);
        layout.Controls.Add(buttons);
        var reset = new Button { Text = "Use layer default", AutoSize = true, DialogResult = DialogResult.Retry };
        layout.Controls.Add(reset);
        var apply = new Button { Text = "Apply", AutoSize = true, DialogResult = DialogResult.OK };
        layout.Controls.Add(apply);
        dialog.AcceptButton = apply;
        dialog.Controls.Add(layout);
        DarkTheme.Apply(dialog);
        var result = dialog.ShowDialog(this);
        if (result != DialogResult.OK && result != DialogResult.Retry) return;
        mask = checks.Select((check, index) => check.Checked ? 1 << index : 0).Aggregate(0, (a, b) => a | b);
        for (var row = paintTileRow; row < paintTileRow + paintTileHeight; ++row)
        for (var column = paintTileColumn; column < paintTileColumn + paintTileWidth; ++column)
            document.SetTilePassage(SelectedSheetId(), column, row, result == DialogResult.Retry ? null : mask);
        SetDirty(true, "Tile passage updated");
        RefreshViewportFromDetails();
        tilePalette.Invalidate();
    }

    private Control BuildLayerControls()
    {
        RefreshTileLayers();
        visibleLayers.ItemCheck += (_, _) => BeginInvoke(() => viewport.SendCommand(7, VisibleLayerMask()));
        visibleLayers.IsLocked = index => (lockedLayerMask & (1 << ((TileLayerEntry)visibleLayers.Items[index]!).Id)) != 0;
        visibleLayers.ShowsCollision = index => (collisionLayerMask & (1 << ((TileLayerEntry)visibleLayers.Items[index]!).Id)) != 0;
        visibleLayers.CollisionClicked += index =>
        {
            collisionLayerMask ^= 1 << ((TileLayerEntry)visibleLayers.Items[index]!).Id;
            visibleLayers.Invalidate();
            viewport.SendCommand(45, collisionLayerMask);
            statusLabel.Text = "Tile collision overlay toggled";
        };
        visibleLayers.CanRename = index => visibleLayers.Items[index] is TileLayerEntry { Id: >= 4 and <= 11 };
        visibleLayers.LockClicked += index => ToggleLayerLock(((TileLayerEntry)visibleLayers.Items[index]!).Id);
        visibleLayers.RenameRequested = RenameLayer;
        var layerActions = new ToolStrip { Dock = DockStyle.Top, AutoSize = true, Margin = new Padding(3, 4, 3, 4),
            Padding = new Padding(4, 3, 4, 3),
            GripStyle = ToolStripGripStyle.Hidden, CanOverflow = false, LayoutStyle = ToolStripLayoutStyle.Flow,
            ImageScalingSize = new Size(16, 16), BackColor = DarkTheme.Panel };
        var addLayer = new ToolStripButton { Text = "+ Add Layer", ToolTipText = "Add visual layer" };
        addLayer.Click += (_, _) => AddTileLayer();
        renameLayer.Click += (_, _) => visibleLayers.BeginRename(visibleLayers.SelectedIndex);
        var removeLayer = new ToolStripButton { Text = "- Delete Layer", ToolTipText = "Delete selected user layer" };
        removeLayer.Click += (_, _) => RemoveSelectedTileLayer();
        layerActions.Items.Add(addLayer);
        layerActions.Items.Add(removeLayer);
        moveLayerUp.Click += (_, _) => MoveSelectedLayer(-1);
        moveLayerDown.Click += (_, _) => MoveSelectedLayer(1);
        layerActions.Items.Add(moveLayerUp);
        layerActions.Items.Add(moveLayerDown);
        layerActions.Items.Add(renameLayer);
        foreach (ToolStripItem item in layerActions.Items)
        {
            item.Padding = new Padding(3, 2, 3, 2);
            item.Margin = new Padding(0, 1, 4, 1);
        }
        var transforms = new ToolStrip { Dock = DockStyle.Top, AutoSize = true, Margin = new Padding(3, 0, 3, 5),
            Padding = new Padding(4, 3, 4, 3), GripStyle = ToolStripGripStyle.Hidden, LayoutStyle = ToolStripLayoutStyle.Flow,
            ImageScalingSize = new Size(18, 18) };
        transforms.Items.Add(TileTransformButton("Rotate left", 0, 4));
        transforms.Items.Add(TileTransformButton("Rotate right", 1, 5));
        transforms.Items.Add(TileTransformButton("Flip horizontally", 2, 6));
        transforms.Items.Add(TileTransformButton("Flip vertically", 3, 7));
        var duplicateTiles = TileTransformButton("Duplicate selected tiles beside the selection", 4, 8);
        duplicateTiles.Enabled = !transformTileBrush;
        transforms.Items.Add(duplicateTiles);
        var transformTarget = new ToolStripDropDownButton("Tile brush") { ToolTipText = "Choose what Rotate and Flip change" };
        var brushTarget = new ToolStripMenuItem("Tile brush") { Checked = true };
        var selectionTarget = new ToolStripMenuItem("Selected tiles");
        void SetTransformTarget(bool brush)
        {
            transformTileBrush = brush;
            transformTarget.Text = brush ? "Tile brush" : "Selected tiles";
            brushTarget.Checked = brush;
            selectionTarget.Checked = !brush;
            duplicateTiles.Enabled = !brush;
        }
        brushTarget.Click += (_, _) => SetTransformTarget(true);
        selectionTarget.Click += (_, _) => SetTransformTarget(false);
        transformTarget.DropDownItems.AddRange([brushTarget, selectionTarget]);
        transforms.Items.Insert(0, transformTarget);
        var clearLayer = new ToolStripButton { DisplayStyle = ToolStripItemDisplayStyle.Image, ToolTipText = "Clear active layer", Image = CreateTrashIcon() };
        clearLayer.Click += (_, _) =>
            ClearActiveLayer();
        transforms.Items.Add(new ToolStripSeparator());
        transforms.Items.Add(clearLayer);
        foreach (ToolStripItem item in transforms.Items)
        {
            if (item is not ToolStripButton) continue;
            item.Padding = new Padding(3);
            item.Margin = new Padding(0, 1, 3, 1);
        }
        var animationPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Margin = new Padding(3, 2, 3, 4),
            Padding = new Padding(4, 3, 4, 3),
            ColumnCount = 3,
            RowCount = 3
        };
        animationPanel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        animationPanel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        animationPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100.0f));
        animationPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        animationPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        animationPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        animationFrames.Margin = new Padding(6, 4, 8, 4);
        animationMilliseconds.Margin = new Padding(6, 4, 8, 4);
        previewAnimations.Margin = new Padding(4, 5, 3, 4);
        animationPanel.Controls.Add(new Label { Text = "Frames", AutoSize = true, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft }, 0, 0);
        animationPanel.Controls.Add(animationFrames, 1, 0);
        animationPanel.Controls.Add(previewAnimations, 0, 2);
        animationPanel.SetColumnSpan(previewAnimations, 3);
        animationPanel.Controls.Add(new Label { Text = "Frame ms", AutoSize = true, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft }, 0, 1);
        animationPanel.Controls.Add(animationMilliseconds, 1, 1);
        animationFrames.ValueChanged += (_, _) => SendAnimationConfiguration();
        animationMilliseconds.ValueChanged += (_, _) => SendAnimationConfiguration();
        previewAnimations.CheckedChanged += (_, _) => SendAnimationConfiguration();
        var panel = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            RowCount = 5, ColumnCount = 1, Margin = Padding.Empty };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100.0f));
        panel.RowStyles.Add(layerListRow);
        panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 8.0f));
        panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        // Keep the grip flush with the list so its bottom edge is the resize target.
        visibleLayers.Margin = new Padding(3, 3, 3, 0);
        panel.Controls.Add(visibleLayers, 0, 0);
        var resizeHandle = new Panel { Dock = DockStyle.Fill, Margin = new Padding(3, 0, 3, 0), Cursor = Cursors.SizeNS,
            BackColor = DarkTheme.Header };
        resizeHandle.Paint += (_, e) =>
        {
            using var pen = new Pen(DarkTheme.Text);
            var centerX = resizeHandle.ClientSize.Width / 2;
            var centerY = resizeHandle.ClientSize.Height / 2;
            e.Graphics.DrawLine(pen, centerX - 14, centerY, centerX + 14, centerY);
        };
        var dragStartY = 0;
        var dragStartAdjustment = 0;
        resizeHandle.MouseDown += (_, e) =>
        {
            if (e.Button != MouseButtons.Left) return;
            dragStartY = resizeHandle.PointToScreen(e.Location).Y;
            dragStartAdjustment = layerListHeightAdjustment;
            resizeHandle.Capture = true;
        };
        resizeHandle.MouseMove += (_, e) =>
        {
            if (!resizeHandle.Capture) return;
            var delta = resizeHandle.PointToScreen(e.Location).Y - dragStartY;
            var minimumAdjustment = -Math.Max(0, visibleLayers.Items.Count - 1) * visibleLayers.ItemHeight;
            layerListHeightAdjustment = Math.Max(minimumAdjustment, dragStartAdjustment + delta);
            UpdateLayerListHeight();
        };
        resizeHandle.MouseUp += (_, e) =>
        {
            if (e.Button == MouseButtons.Left) resizeHandle.Capture = false;
        };
        resizeHandle.DoubleClick += (_, _) =>
        {
            layerListHeightAdjustment = 0;
            UpdateLayerListHeight();
        };
        visibleLayers.FontChanged += (_, _) => UpdateLayerListHeight();
        panel.Controls.Add(resizeHandle, 0, 1);
        panel.Controls.Add(layerActions, 0, 2);
        panel.Controls.Add(transforms, 0, 3);
        panel.Controls.Add(animationPanel, 0, 4);
        UpdateLayerListHeight();
        return panel;
    }

    private void UpdateLayerListHeight()
    {
        var rowHeight = visibleLayers.ItemHeight;
        var contentHeight = Math.Max(1, visibleLayers.Items.Count) * rowHeight;
        layerListRow.Height = Math.Max(rowHeight, contentHeight + layerListHeightAdjustment)
            + SystemInformation.BorderSize.Height * 2 + visibleLayers.Margin.Vertical;
    }

    private void SendAnimationConfiguration()
    {
        viewport.SendCommand(12, (int)animationFrames.Value);
        viewport.SendCommand(13, (int)animationMilliseconds.Value);
        viewport.SendCommand(14, previewAnimations.Checked ? 1 : 0);
    }

    private ToolStripButton TileTransformButton(string tooltip, int icon, int command)
    {
        var button = new ToolStripButton { DisplayStyle = ToolStripItemDisplayStyle.Image, ToolTipText = tooltip, Image = CreateTileTransformIcon(icon) };
        button.Click += (_, _) =>
        {
            if (command is >= 4 and <= 7 && transformTileBrush) ApplyTileBrushCommand(command);
            else ApplyTileSelectionCommand(command, tooltip);
        };
        return button;
    }

    private void SetBrushSize(int size)
    {
        paintBrushSize = size;
        brushSizeButton.Text = $"{paintBrushSize}x{paintBrushSize}";
        SendPaintConfiguration();
        UpdateToolIndicators();
    }

    private void SetGridSnap(int size)
    {
        gridSnap = size;
        viewport.SendCommand(24, gridSnap);
        statusLabel.Text = $"Grid snap: {gridSnap} px";
    }

    private void NudgeTileSelection(int command, string direction)
    {
        viewport.SendCommand(6, command);
        statusLabel.Text = $"Nudged tile selection {direction}";
    }

    private void NudgeSelectedObject(Keys key, string direction)
    {
        DeactivatePaintTools();
        viewport.SendKey(key);
        statusLabel.Text = $"Nudged selected object {direction}";
    }

    private void FrameSelection()
    {
        viewport.SendCommand(25, 1);
        statusLabel.Text = "Framed selection";
    }

    private void SetZoomFromControl(int percent, bool sendToViewport)
    {
        if (synchronizingZoomControls) return;
        percent = Math.Clamp(percent, 5, 800);
        synchronizingZoomControls = true;
        zoomSlider.Value = percent;
        zoomInput.Value = percent;
        synchronizingZoomControls = false;
        if (sendToViewport) viewport.SendCommand(32, percent);
    }

    private void DuplicateSelectedObjects()
    {
        DeactivatePaintTools();
        viewport.SendCommand(26, 1);
        statusLabel.Text = "Duplicated selected objects";
    }

    private void ResetTileToolSettings()
    {
        SetBrushSize(1);
        if (mirrorXButton.Checked) mirrorXButton.PerformClick();
        if (mirrorYButton.Checked) mirrorYButton.PerformClick();
        animationFrames.Value = 1;
        animationMilliseconds.Value = 120;
        previewAnimations.Checked = true;
        statusLabel.Text = "Tile tool settings reset";
    }

    private static void FocusPanel(Control panel, Control target)
    {
        panel.Visible = true;
        target.Focus();
    }

    private void ShowKeyboardShortcuts()
    {
        const string shortcuts =
            "File\n" +
            "  Ctrl+O  Open level\n" +
            "  Ctrl+S  Save level\n" +
            "  Ctrl+Shift+S  Save level as\n\n" +
            "Editing\n" +
            "  Q / W / E / R  Select, move, rotate, scale\n" +
            "  F  Frame level\n" +
            "  Delete  Delete selection\n" +
            "  Arrow keys  Nudge selection\n" +
            "  Ctrl+Z / Ctrl+Y  Undo or redo tile edit\n" +
            "  Ctrl+C / Ctrl+V / Ctrl+D  Copy, paste, or duplicate tiles\n\n" +
            "Object Placement\n" +
            "  1-7  Solid, platform, ladder, camera zone, darkness, player start, exit";
        MessageBox.Show(this, shortcuts, "Keyboard Shortcuts", MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    private void ApplyTileSelectionCommand(int command, string description)
    {
        viewport.SendCommand(6, command);
        statusLabel.Text = $"{description}: updating selected tiles";
    }

    private void ApplyTileBrushCommand(int command)
    {
        if (command == 4) paintQuarterTurns = (paintQuarterTurns + 3) % 4;
        else if (command == 5) paintQuarterTurns = (paintQuarterTurns + 1) % 4;
        else if (command == 6) paintFlipX = !paintFlipX;
        else if (command == 7) paintFlipY = !paintFlipY;
        tilePalette.SetBrushTransform(paintQuarterTurns, paintFlipX, paintFlipY);
        viewport.SendCommand(42, paintQuarterTurns | (paintFlipX ? 4 : 0) | (paintFlipY ? 8 : 0));
        statusLabel.Text = $"Tile brush: {paintQuarterTurns * 90}° rotation, horizontal flip {(paintFlipX ? "on" : "off")}, vertical flip {(paintFlipY ? "on" : "off")}";
    }

    private void ClearActiveLayer()
    {
        var layerName = (visibleLayers.SelectedItem as TileLayerEntry)?.Label ?? "active layer";
        if (MessageBox.Show(this, $"Remove every tile from {layerName}?", "Clear Active Layer",
            MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) == DialogResult.OK)
            viewport.SendCommand(11, 1);
    }

    private static Bitmap CreateTileTransformIcon(int icon)
    {
        var bitmap = new Bitmap(18, 18);
        using var graphics = Graphics.FromImage(bitmap);
        graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        using var pen = new Pen(Color.FromArgb(230, 234, 241), 2.0f);
        using var accent = new SolidBrush(DarkTheme.Accent);
        if (icon < 2)
        {
            graphics.DrawArc(pen, 3, 3, 12, 12, icon == 0 ? 35 : 145, 270);
            var points = icon == 0
                ? new[] { new Point(2, 3), new Point(7, 3), new Point(3, 8) }
                : [new Point(16, 3), new Point(11, 3), new Point(15, 8)];
            graphics.FillPolygon(accent, points);
        }
        else if (icon < 4)
        {
            graphics.DrawRectangle(pen, 3, 3, 12, 12);
            if (icon == 2) graphics.DrawLine(pen, 9, 2, 9, 16);
            else graphics.DrawLine(pen, 2, 9, 16, 9);
            Point[] arrow = icon == 2
                ? [new Point(4, 9), new Point(7, 6), new Point(7, 12)]
                : [new Point(9, 4), new Point(6, 7), new Point(12, 7)];
            graphics.FillPolygon(accent, arrow);
        }
        else
        {
            graphics.DrawRectangle(pen, 3, 3, 9, 9);
            graphics.DrawRectangle(pen, 6, 6, 9, 9);
        }
        return bitmap;
    }

    private static Bitmap CreateSymmetryIcon(bool horizontal)
    {
        var bitmap = new Bitmap(20, 20);
        using var graphics = Graphics.FromImage(bitmap);
        using var line = new Pen(Color.FromArgb(230, 234, 241), 2.0f) { DashStyle = System.Drawing.Drawing2D.DashStyle.Dash };
        using var fill = new SolidBrush(DarkTheme.Accent);
        if (horizontal)
        {
            graphics.DrawLine(line, 10, 2, 10, 18);
            graphics.FillPolygon(fill, [new Point(2, 10), new Point(8, 5), new Point(8, 15)]);
            graphics.FillPolygon(fill, [new Point(18, 10), new Point(12, 5), new Point(12, 15)]);
        }
        else
        {
            graphics.DrawLine(line, 2, 10, 18, 10);
            graphics.FillPolygon(fill, [new Point(10, 2), new Point(5, 8), new Point(15, 8)]);
            graphics.FillPolygon(fill, [new Point(10, 18), new Point(5, 12), new Point(15, 12)]);
        }
        return bitmap;
    }

    private static Bitmap CreateTrashIcon()
    {
        var bitmap = new Bitmap(18, 18);
        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(Color.FromArgb(255, 112, 112), 2.0f);
        graphics.DrawLine(pen, 4, 5, 14, 5);
        graphics.DrawLine(pen, 7, 2, 11, 2);
        graphics.DrawRectangle(pen, 5, 6, 8, 9);
        return bitmap;
    }

    private int VisibleLayerMask()
    {
        var mask = 0;
        for (var index = 0; index < visibleLayers.Items.Count; ++index)
            if (visibleLayers.GetItemChecked(index)) mask |= 1 << ((TileLayerEntry)visibleLayers.Items[index]!).Id;
        return mask;
    }

    private static TileLayerEntry[] DefaultTileLayers =>
    [
        new(2, "foreground", "Foreground"),
        new(12, "middleground", "Middleground"),
        new(1, "background", "Background"),
        new(0, "farBackground", "Far Background"),
        new(3, "collision", "Collision")
    ];

    private int ActiveLayerId() => visibleLayers.SelectedItem is TileLayerEntry entry ? entry.Id : 2;

    private void RefreshTileLayers()
    {
        var previousChecks = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
        foreach (var item in visibleLayers.Items.OfType<TileLayerEntry>())
            previousChecks[item.Key] = visibleLayers.GetItemChecked(visibleLayers.Items.IndexOf(item));
        var selectedKey = (visibleLayers.SelectedItem as TileLayerEntry)?.Key ?? "foreground";
        visibleLayers.BeginUpdate();
        visibleLayers.ClearCheckStates();
        visibleLayers.Items.Clear();
        var customEntries = new List<TileLayerEntry>();
        foreach (var name in document.CustomTileLayers)
        {
            if (name.Length != 5 || !int.TryParse(name.AsSpan(4), out var slot) || slot is < 1 or > 8) continue;
            customEntries.Add(new TileLayerEntry(slot + 3, name, document.TileLayerLabel(name)));
        }
        var entries = customEntries.Where(entry => document.IsLayerAboveDefaults(entry.Key)).Concat(DefaultTileLayers)
            .Concat(customEntries.Where(entry => !document.IsLayerAboveDefaults(entry.Key)));
        foreach (var entry in entries)
        {
            var index = visibleLayers.Items.Add(entry);
            visibleLayers.SetItemChecked(index, previousChecks.TryGetValue(entry.Key, out var wasChecked) ? wasChecked : true);
            if (entry.Key.Equals(selectedKey, StringComparison.OrdinalIgnoreCase)) visibleLayers.SelectedIndex = index;
        }
        if (visibleLayers.SelectedIndex < 0) visibleLayers.SelectedIndex = 0;
        visibleLayers.EndUpdate();
        UpdateLayerListHeight();
        OnActiveLayerChanged();
    }

    private void SelectActiveLayer(TileLayerEntry entry)
    {
        var index = visibleLayers.Items.IndexOf(entry);
        if (index >= 0) visibleLayers.SelectedIndex = index;
    }

    private void OnActiveLayerChanged()
    {
        var layerId = ActiveLayerId();
        var index = visibleLayers.SelectedItem is TileLayerEntry { Id: >= 4 and <= 11 } entry
            ? document.CustomTileLayers.ToList().IndexOf(entry.Key) : -1;
        var selectedKey = index >= 0 ? ((TileLayerEntry)visibleLayers.SelectedItem!).Key : string.Empty;
        renameLayer.Enabled = index >= 0;
        moveLayerUp.Enabled = document.CanMoveTileLayer(selectedKey, -1);
        moveLayerDown.Enabled = document.CanMoveTileLayer(selectedKey, 1);
        if (IsHandleCreated) SendPaintConfiguration();
    }

    private void AddTileLayer()
    {
        var used = document.CustomTileLayers.ToHashSet(StringComparer.OrdinalIgnoreCase);
        var slot = Enumerable.Range(1, 8).FirstOrDefault(value => !used.Contains($"user{value}"));
        if (slot == 0)
        {
            statusLabel.Text = "All user layer slots are in use";
            return;
        }
        var name = $"user{slot}";
        document.AddTileLayer(name);
        RefreshTileLayers();
        SelectActiveLayer(visibleLayers.Items.OfType<TileLayerEntry>().First(entry => entry.Key == name));
        SetDirty(true, $"Added User Layer {slot}");
    }

    private void ToggleLayerLock(int id)
    {
        lockedLayerMask ^= 1 << id;
        visibleLayers.Invalidate();
        viewport.SendCommand(8, lockedLayerMask);
        statusLabel.Text = (lockedLayerMask & (1 << id)) != 0 ? "Layer locked" : "Layer unlocked";
    }

    private bool RenameLayer(int index, string label)
    {
        if (visibleLayers.Items[index] is not TileLayerEntry { Id: >= 4 and <= 11 } entry) return false;
        try
        {
            document.RenameTileLayer(entry.Key, label);
            RefreshTileLayers();
            SetDirty(true, $"Renamed layer to {label.Trim()}");
            return true;
        }
        catch (ArgumentException exception)
        {
            statusLabel.Text = exception.Message;
            return false;
        }
    }

    private void MoveSelectedLayer(int direction)
    {
        if (visibleLayers.SelectedItem is not TileLayerEntry { Id: >= 4 and <= 11 } entry || !document.MoveTileLayer(entry.Key, direction)) return;
        RefreshTileLayers();
        RefreshViewportFromDetails();
        SetDirty(true, $"Moved {entry.Label} {(direction < 0 ? "up" : "down")}");
    }

    private void RemoveSelectedTileLayer()
    {
        if (visibleLayers.SelectedItem is not TileLayerEntry { Id: >= 4 and <= 11 } entry)
        {
            statusLabel.Text = "Select a user layer to remove";
            return;
        }
        document.RemoveTileLayer(entry.Key);
        viewport.SendCommand(30, entry.Id);
        RefreshTileLayers();
        SetDirty(true, $"Removed {entry.Label}");
    }

    private void ConfigureTileTool(ToolStripButton button, int icon, string tooltip)
    {
        button.DisplayStyle = ToolStripItemDisplayStyle.Image;
        button.ToolTipText = tooltip;
        button.Image = CreateTileToolIcon(icon);
        button.Click += (_, _) => SetPaintTool(icon + 1, button);
    }

    private static Bitmap CreateTileToolIcon(int tool)
    {
        var bitmap = new Bitmap(28, 28);
        using var graphics = Graphics.FromImage(bitmap);
        graphics.ScaleTransform(1.4f, 1.4f);
        graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        using var light = new Pen(Color.FromArgb(230, 234, 241), 3.0f) { StartCap = System.Drawing.Drawing2D.LineCap.Round, EndCap = System.Drawing.Drawing2D.LineCap.Round };
        using var accent = new Pen(tool == 2 ? Color.FromArgb(255, 112, 112) : DarkTheme.Accent, 2.0f);
        if (tool == 0)
        {
            graphics.DrawLine(light, 4, 16, 15, 5);
            graphics.DrawLine(accent, 3, 17, 7, 16);
        }
        else if (tool == 1)
        {
            graphics.DrawLine(light, 12, 3, 7, 12);
            using var brushHead = new SolidBrush(DarkTheme.Accent);
            graphics.FillEllipse(brushHead, 3, 11, 8, 6);
        }
        else if (tool == 2)
        {
            graphics.TranslateTransform(10, 10);
            graphics.RotateTransform(-35);
            graphics.DrawRectangle(accent, -6, -4, 12, 8);
            using var eraserFill = new SolidBrush(Color.FromArgb(210, 220, 232));
            graphics.FillRectangle(eraserFill, -5, -3, 10, 6);
        }
        else if (tool == 3)
        {
            graphics.DrawEllipse(light, 3, 3, 9, 9);
            graphics.DrawLine(light, 11, 11, 17, 17);
            graphics.DrawLine(accent, 7, 6, 7, 10);
        }
        else if (tool == 4)
        {
            graphics.DrawRectangle(accent, 3, 4, 14, 12);
            graphics.DrawRectangle(light, 6, 7, 8, 6);
        }
        else if (tool == 5)
        {
            var iconTransform = graphics.Save();
            graphics.RotateTransform(-25);
            graphics.DrawRectangle(light, 5, 4, 10, 9);
            graphics.Restore(iconTransform);
            using var fill = new SolidBrush(DarkTheme.Accent);
            graphics.FillEllipse(fill, 12, 13, 5, 5);
        }
        else if (tool == 6)
        {
            using var dash = new Pen(DarkTheme.Accent, 2) { DashStyle = System.Drawing.Drawing2D.DashStyle.Dash };
            graphics.DrawRectangle(dash, 3, 3, 14, 14);
        }
        else if (tool == 7)
        {
            using var fill = new SolidBrush(Color.FromArgb(220, 255, 94, 94));
            graphics.FillRectangle(fill, 3, 3, 6, 6);
            graphics.FillRectangle(fill, 11, 3, 6, 6);
            graphics.FillRectangle(fill, 3, 11, 6, 6);
            graphics.FillRectangle(fill, 11, 11, 6, 6);
        }
        else if (tool == 8)
        {
            graphics.DrawLine(light, 3, 16, 17, 4);
            graphics.FillRectangle(Brushes.White, 2, 14, 4, 4);
            graphics.FillRectangle(Brushes.White, 14, 2, 4, 4);
        }
        else if (tool == 9)
        {
            graphics.DrawArc(light, 3, 3, 14, 14, 35, 245);
            using var arrow = new SolidBrush(DarkTheme.Accent);
            graphics.FillPolygon(arrow, [new Point(15, 2), new Point(18, 7), new Point(12, 7)]);
        }
        else if (tool == 10)
        {
            using var fill = new SolidBrush(DarkTheme.Accent);
            graphics.FillRectangle(fill, 5, 3, 10, 8);
            graphics.DrawLine(light, 10, 10, 10, 17);
            graphics.DrawLine(light, 6, 17, 14, 17);
        }
        else if (tool == 11)
        {
            using var fill = new SolidBrush(DarkTheme.Accent);
            graphics.FillRectangle(fill, 3, 3, 5, 5);
            graphics.FillRectangle(fill, 12, 4, 4, 4);
            graphics.FillRectangle(fill, 5, 12, 4, 4);
            graphics.FillRectangle(fill, 13, 12, 5, 5);
        }
        else if (tool == 12)
        {
            using var dash = new Pen(DarkTheme.Accent, 2);
            graphics.DrawRectangle(dash, 3, 4, 14, 12);
        }
        else if (tool == 13)
        {
            graphics.DrawEllipse(accent, 3, 4, 14, 12);
        }
        else if (tool == 14)
        {
            graphics.DrawLine(light, 3, 15, 16, 4);
            graphics.DrawLine(accent, 2, 12, 6, 16);
            graphics.DrawLine(accent, 13, 3, 17, 7);
        }
        else if (tool == 15)
        {
            for (var y = 0; y < 4; ++y)
                for (var x = 0; x < 4; ++x)
                    graphics.FillRectangle((x + y) % 2 == 0 ? Brushes.White : Brushes.DodgerBlue, 2 + x * 4, 2 + y * 4, 3, 3);
        }
        else if (tool == 16)
        {
            graphics.DrawLine(light, 4, 16, 13, 7);
            using var sparkle = new SolidBrush(DarkTheme.Accent);
            graphics.FillPolygon(sparkle, [new Point(14, 2), new Point(16, 6), new Point(19, 7), new Point(16, 9), new Point(14, 13), new Point(12, 9), new Point(9, 7), new Point(12, 6)]);
        }
        else
        {
            using var fill = new SolidBrush(DarkTheme.Accent);
            graphics.FillRectangle(fill, 2, 3, 5, 5);
            graphics.FillRectangle(fill, 12, 3, 5, 5);
            graphics.FillRectangle(fill, 2, 12, 5, 5);
            graphics.DrawRectangle(light, 11, 11, 7, 7);
        }
        return bitmap;
    }

    private void AddTool(string name, Keys key, int iconIndex)
    {
        var button = new ToolStripButton(name)
        {
            DisplayStyle = ToolStripItemDisplayStyle.ImageAndText,
            AutoSize = true,
            CheckOnClick = false
        };
        button.Image = LoadToolbarIcon(iconIndex);
        button.Click += (_, _) => SelectTransform(key, name);
        transformButtons[key] = button;
        toolStrip.Items.Add(button);
    }

    private void SetActiveTransformButton(Keys key)
    {
        foreach (var pair in transformButtons) pair.Value.Checked = pair.Key == key;
    }

    private void ClearActiveTransformButton()
    {
        foreach (var button in transformButtons.Values) button.Checked = false;
    }

    private static Image? LoadToolbarIcon(int index)
    {
        var path = Path.Combine(AppContext.BaseDirectory, "assets", "first_party", "ui", "editor_transform_icons.png");
        if (!File.Exists(path)) return null;
        using var sheet = new Bitmap(path);
        Rectangle[] sources =
        [
            new(82, 218, 300, 330), new(444, 216, 340, 340), new(824, 218, 360, 330),
            new(1200, 216, 370, 340), new(1584, 216, 374, 340)
        ];
        var result = new Bitmap(24, 24);
        using var graphics = Graphics.FromImage(result);
        graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
        graphics.DrawImage(sheet, new Rectangle(0, 0, 24, 24), sources[index], GraphicsUnit.Pixel);
        return result;
    }

    private void PopulateAssets()
    {
        contentBrowser.LargeImageList = contentImages;
        foreach (var category in new[]
        {
            "All", "Geometry", "Regions", "Markers", "Fluids", "Materials", "Physics", "Springs",
            "Simple Machines", "Mechanical", "Rigging", "Power", "Logic", "Sensors", "Forces", "Transport", "Hazards",
            "Optics", "Thermal", "Gameplay", "Characters", "Set Dressing"
        })
        {
            contentBrowserTabs.Categories.Add(category);
        }
        contentBrowserTabs.RefreshLayout();
        contentBrowserTabs.SelectedCategory = "All";
        FilterContentBrowser();
        RefreshSpriteSheets();
    }

    private void FilterContentBrowser()
    {
        var selectedCategory = contentBrowserTabs.SelectedCategory;
        contentBrowser.BeginUpdate();
        contentBrowser.Items.Clear();
        for (var index = 0; index < AssetNames.Length; ++index)
        {
            if (selectedCategory != "All" && GetAssetCategory(index) != selectedCategory) continue;
            contentBrowser.Items.Add(new ListViewItem(AssetNames[index])
            {
                ImageIndex = index,
                Tag = index
            });
        }
        contentBrowser.EndUpdate();
    }

    private static string GetAssetCategory(int index) => GetAssetCommandIndex(index) switch
    {
        160 => "Mechanical",
        161 => "Hazards",
        11 or 16 or 17 or 18 or 19 or 20 or 22 or 23 or 24 or 71 or 105 or 155 => "Simple Machines",
        0 or 1 or 2 or 44 or 46 or 159 => "Geometry",
        3 or 4 => "Regions",
        5 or 6 or 35 or 37 => "Markers",
        >= 7 and <= 10 or 36 or 94 or 95 or 102 or 139 or 150 => "Fluids",
        32 or 100 or >= 140 and <= 144 => "Materials",
        14 or 15 or 39 or 40 or 43 or 115 or 122 or 128 or 132 => "Physics",
        >= 47 and <= 68 => "Springs",
        13 or 25 or >= 72 and <= 75 or 112 or 114 or 117 or 123 or 129 or 130 or 133 => "Mechanical",
        12 or 26 or 27 or 45 or 69 or 70 or >= 108 and <= 111 => "Rigging",
        78 or 79 or 80 or 83 or 116 or 120 or 158 => "Power",
        28 or 82 or 127 or >= 151 and <= 153 => "Logic",
        81 or 96 or 97 or 118 or 119 or 136 => "Sensors",
        21 or 84 or 107 or 121 or 124 or 131 => "Forces",
        41 or 42 or 76 or 77 or 113 or 137 or 138 => "Transport",
        30 or 31 or >= 88 and <= 93 or 101 or 103 or 106 => "Hazards",
        154 or 157 => "Hazards",
        125 or 126 or 134 or 135 => "Optics",
        >= 145 and <= 149 => "Thermal",
        29 or 38 or 98 or 99 => "Gameplay",
        33 => "Characters",
        104 => "Set Dressing",
        85 or 86 => "Mechanical",
        87 => "Hazards",
        _ => "Gameplay"
    };

    private static int GetAssetCommandIndex(int browserIndex) => browserIndex < 34 ? browserIndex : browserIndex + 1;

    private static DataObject CreateAssetDragData(int browserIndex)
    {
        var data = new DataObject();
        var assetCommand = GetAssetCommandIndex(browserIndex);
        data.SetData(DataFormats.UnicodeText, assetCommand.ToString(System.Globalization.CultureInfo.InvariantCulture));
        return data;
    }

    private void OpenInitialLevel()
    {
        var path = initialPath;
        if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
        {
            LoadLevel(path);
            return;
        }

        RebuildOutliner();
        viewport.OpenBlank();
        QueueThumbnailLoad();
        SendPaintConfiguration();
        SetGridSnap(gridSnap);
        Text = "Power Pulley Panic - Level Editor - Untitled";
        editHistory.Reset(document.CaptureSnapshot());
        viewportHistoryTransaction = false;
        SetDirty(false, "Blank level");
    }

    private void CreateLevel()
    {
        if (!ConfirmDiscard()) return;
        using var dialog = new SaveFileDialog
        {
            Filter = "Power Pulley Panic levels (*.level)|*.level",
            DefaultExt = "level",
            FileName = "new_level.level",
            InitialDirectory = document.FilePath == null ? null : Path.GetDirectoryName(document.FilePath)
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        File.WriteAllLines(dialog.FileName,
        [
            "# New Power Pulley Panic level.",
            "script power_pulley_panic",
            "bounds 0 0 1600 900",
            "playerStart 64 736",
            "exit 1472 704 64 128",
            "solid 0 832 1600 32"
        ]);
        LoadLevel(dialog.FileName);
    }

    private void PopulateRecentLevelsMenu()
    {
        recentLevelsMenu.DropDownItems.Clear();
        foreach (var path in recentLevelPaths.Where(File.Exists))
        {
            var recentPath = path;
            var item = new ToolStripMenuItem(Path.GetFileName(path)) { ToolTipText = path };
            item.Click += (_, _) =>
            {
                if (ConfirmDiscard()) LoadLevel(recentPath);
            };
            recentLevelsMenu.DropDownItems.Add(item);
        }
        if (recentLevelsMenu.DropDownItems.Count == 0)
            recentLevelsMenu.DropDownItems.Add(new ToolStripMenuItem("(No recent levels)") { Enabled = false });
    }

    private void OpenLevel()
    {
        using var dialog = new OpenFileDialog { Filter = "Power Pulley Panic levels (*.level)|*.level|All files (*.*)|*.*" };
        if (document.FilePath != null) dialog.InitialDirectory = Path.GetDirectoryName(document.FilePath);
        if (dialog.ShowDialog(this) == DialogResult.OK && ConfirmDiscard()) LoadLevel(dialog.FileName);
    }

    private void LoadLevel(string path)
    {
        CancelViewportLink();
        path = Path.GetFullPath(path);
        document.Load(path);
        RefreshSpriteSheets();
        RefreshTileLayers();
        editHistory.Reset(document.CaptureSnapshot());
        viewportHistoryTransaction = false;
        recentLevelPaths.RemoveAll(recent => string.Equals(recent, path, StringComparison.OrdinalIgnoreCase));
        recentLevelPaths.Insert(0, path);
        if (recentLevelPaths.Count > 10) recentLevelPaths.RemoveRange(10, recentLevelPaths.Count - 10);
        RebuildOutliner();
        viewport.OpenLevel(document.FilePath!);
        QueueThumbnailLoad();
        SendPaintConfiguration();
        SetGridSnap(gridSnap);
        Text = $"Power Pulley Panic - Level Editor - {Path.GetFileName(document.FilePath)}";
        SetDirty(false, $"Loaded {Path.GetFileName(document.FilePath)}");
    }

    private void RebuildOutliner(LevelObject? preferredSelection = null)
    {
        preferredSelection ??= outliner.SelectedNode?.Tag as LevelObject;
        var expandedCategories = FlattenOutlinerNodes(outliner.Nodes)
            .Where(node => node.IsExpanded && node.Tag is OutlinerCategory)
            .Select(node => ((OutlinerCategory)node.Tag!).Id)
            .ToHashSet(StringComparer.Ordinal);

        outliner.BeginUpdate();
        outliner.Nodes.Clear();
        var objectsRoot = new TreeNode("Objects") { Tag = new OutlinerRootTag(OutlinerRootKind.Objects) };
        var tilesRoot = new TreeNode("Tiles") { Tag = new OutlinerRootTag(OutlinerRootKind.Tiles) };
        var settingsRoot = new TreeNode("Level Settings") { Tag = new OutlinerRootTag(OutlinerRootKind.LevelSettings) };
        var categoryNodes = document.OutlinerCategories.ToDictionary(category => category.Id,
            category => new TreeNode(category.Name) { Tag = category }, StringComparer.Ordinal);

        foreach (var category in document.OutlinerCategories)
        {
            var node = categoryNodes[category.Id];
            if (category.ParentId != null && categoryNodes.TryGetValue(category.ParentId, out var parent) &&
                !CategoryDescendsFrom(parent, node))
                parent.Nodes.Add(node);
            else
                objectsRoot.Nodes.Add(node);
        }

        TreeNode? selectedNode = null;
        foreach (var item in document.Objects)
        {
            if (IsHiddenOutlinerRecord(item)) continue;
            var node = CreateOutlinerObjectNode(item);
            if (ReferenceEquals(item, preferredSelection)) selectedNode = node;
            if (IsTileOutlinerItem(item))
                tilesRoot.Nodes.Add(node);
            else if (IsLevelSettingOutlinerItem(item))
                settingsRoot.Nodes.Add(node);
            else if (document.OutlinerCategoryId(item) is { } categoryId && categoryNodes.TryGetValue(categoryId, out var category))
                category.Nodes.Add(node);
            else
                objectsRoot.Nodes.Add(node);
        }

        outliner.Nodes.AddRange([objectsRoot, tilesRoot, settingsRoot]);
        objectsRoot.Expand();
        tilesRoot.Expand();
        settingsRoot.Expand();
        foreach (var node in categoryNodes.Values)
            if (expandedCategories.Contains(((OutlinerCategory)node.Tag!).Id)) node.Expand();
        if (selectedNode != null) outliner.SelectedNode = selectedNode;
        outliner.EndUpdate();
    }

    private TreeNode CreateOutlinerObjectNode(LevelObject item)
    {
        var primary = OutlinerObjectLabel(item);
        var official = OfficialOutlinerObjectLabel(item);
        var showOfficial = !primary.Equals(official, StringComparison.Ordinal);
        return new TreeNode(showOfficial ? $"{primary}    {official}" : primary) { Tag = item };
    }

    private string OutlinerObjectLabel(LevelObject item) => document.OutlinerName(item) ?? OfficialOutlinerObjectLabel(item);

    private static string OfficialOutlinerObjectLabel(LevelObject item) => $"{HumanizeObjectType(item.Type)} {item.Index}";

    private static bool IsTileOutlinerItem(LevelObject item) =>
        item.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) ||
        item.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase);

    private static bool IsLevelSettingOutlinerItem(LevelObject item) =>
        item.Type.Equals("author", StringComparison.OrdinalIgnoreCase) ||
        item.Type.Equals("music", StringComparison.OrdinalIgnoreCase) ||
        item.Type.Equals("script", StringComparison.OrdinalIgnoreCase) ||
        item.Type.Equals("bounds", StringComparison.OrdinalIgnoreCase) ||
        item.Type.Equals("spikePitTop", StringComparison.OrdinalIgnoreCase);

    private static bool IsHiddenOutlinerRecord(LevelObject item) => item.Type is
        "tileLayer" or "tileLayerName" or "tileLayerAbove" or "spriteSheet";

    private static bool CategoryDescendsFrom(TreeNode possibleDescendant, TreeNode category)
    {
        for (var node = possibleDescendant; node != null; node = node.Parent)
            if (ReferenceEquals(node, category)) return true;
        return false;
    }

    private static IEnumerable<TreeNode> FlattenOutlinerNodes(TreeNodeCollection nodes)
    {
        foreach (TreeNode node in nodes)
        {
            yield return node;
            foreach (var child in FlattenOutlinerNodes(node.Nodes)) yield return child;
        }
    }

    private void DrawOutlinerNode(object? sender, DrawTreeNodeEventArgs eventArgs)
    {
        if (eventArgs.Node?.Tag is not LevelObject item)
        {
            eventArgs.DrawDefault = true;
            return;
        }
        var selected = (eventArgs.State & TreeNodeStates.Selected) != 0;
        var row = new Rectangle(eventArgs.Bounds.X, eventArgs.Bounds.Y,
            Math.Max(0, outliner.ClientSize.Width - eventArgs.Bounds.X), eventArgs.Bounds.Height);
        using var background = new SolidBrush(selected ? DarkTheme.Selection : outliner.BackColor);
        eventArgs.Graphics.FillRectangle(background, row);
        var primary = OutlinerObjectLabel(item);
        var official = OfficialOutlinerObjectLabel(item);
        var flags = TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine;
        TextRenderer.DrawText(eventArgs.Graphics, primary, outliner.Font, row, selected ? Color.White : DarkTheme.Text, flags);
        if (!primary.Equals(official, StringComparison.Ordinal))
        {
            var width = TextRenderer.MeasureText(eventArgs.Graphics, primary, outliner.Font, Size.Empty,
                TextFormatFlags.NoPadding | TextFormatFlags.SingleLine).Width;
            var secondary = new Rectangle(row.X + width + 10, row.Y, Math.Max(0, row.Width - width - 10), row.Height);
            TextRenderer.DrawText(eventArgs.Graphics, official, outliner.Font, secondary, DarkTheme.MutedText, flags);
        }
        if ((eventArgs.State & TreeNodeStates.Focused) != 0) ControlPaint.DrawFocusRectangle(eventArgs.Graphics, row);
    }

    private void AddOutlinerCategory(string? parentId)
    {
        if (!PromptForOutlinerText("New Category", "Category name", string.Empty, false, out var name)) return;
        try
        {
            var category = document.AddOutlinerCategory(name, parentId);
            SetDirty(true, $"Created category {category.Name}");
            RebuildOutliner();
            outliner.SelectedNode = FlattenOutlinerNodes(outliner.Nodes)
                .FirstOrDefault(node => node.Tag is OutlinerCategory value && value.Id == category.Id);
        }
        catch (ArgumentException exception)
        {
            MessageBox.Show(this, exception.Message, "Cannot create category", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void RenameOutlinerNode(TreeNode? node)
    {
        if (node?.Tag is LevelObject item)
        {
            var current = document.OutlinerName(item) ?? OfficialOutlinerObjectLabel(item);
            if (!PromptForOutlinerText("Rename Item", "Custom name (leave empty to restore the official name)", current,
                true, out var name)) return;
            var customName = name.Trim();
            if (customName.Equals(OfficialOutlinerObjectLabel(item), StringComparison.Ordinal)) customName = string.Empty;
            try
            {
                document.SetOutlinerName(item, customName);
                SetDirty(true, customName.Length == 0 ? "Restored official item name" : $"Renamed item to {customName}");
                RebuildOutliner(item);
            }
            catch (ArgumentException exception)
            {
                MessageBox.Show(this, exception.Message, "Cannot rename item", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
            return;
        }
        if (node?.Tag is not OutlinerCategory category) return;
        if (!PromptForOutlinerText("Rename Category", "Category name", category.Name, false, out var categoryName)) return;
        try
        {
            document.RenameOutlinerCategory(category.Id, categoryName);
            SetDirty(true, $"Renamed category to {categoryName.Trim()}");
            RebuildOutliner();
        }
        catch (ArgumentException exception)
        {
            MessageBox.Show(this, exception.Message, "Cannot rename category", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void DeleteSelectedOutlinerCategory()
    {
        if (outliner.SelectedNode?.Tag is not OutlinerCategory category) return;
        if (MessageBox.Show(this, $"Delete '{category.Name}' and its sub-categories? Objects will return to Objects.",
            "Delete Category", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
        document.DeleteOutlinerCategory(category.Id);
        SetDirty(true, $"Deleted category {category.Name}");
        RebuildOutliner();
    }

    private static bool PromptForOutlinerText(string title, string prompt, string initialValue, bool allowEmpty,
        out string value)
    {
        using var dialog = new Form
        {
            Text = title,
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MinimizeBox = false,
            MaximizeBox = false,
            ShowInTaskbar = false,
            AutoScaleMode = AutoScaleMode.Dpi,
            ClientSize = new Size(540, 152)
        };
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(12),
            ColumnCount = 1,
            RowCount = 3
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        var label = new Label
        {
            Text = prompt,
            AutoSize = true,
            MaximumSize = new Size(492, 0),
            BackColor = Color.Transparent,
            Margin = new Padding(0, 0, 0, 8)
        };
        var input = new TextBox { Dock = DockStyle.Top, Text = initialValue, MaxLength = 80, Margin = new Padding(0, 0, 0, 4) };
        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false,
            Margin = new Padding(0, 10, 0, 0)
        };
        var okay = new Button { Text = "OK", Width = 82, Height = 28, DialogResult = DialogResult.OK, Margin = new Padding(6, 0, 0, 0) };
        var cancel = new Button { Text = "Cancel", Width = 82, Height = 28, DialogResult = DialogResult.Cancel, Margin = new Padding(6, 0, 0, 0) };
        buttons.Controls.Add(cancel);
        buttons.Controls.Add(okay);
        layout.Controls.Add(label, 0, 0);
        layout.Controls.Add(input, 0, 1);
        layout.Controls.Add(buttons, 0, 2);
        dialog.Controls.Add(layout);
        dialog.AcceptButton = okay;
        dialog.CancelButton = cancel;
        dialog.Shown += (_, _) => { input.SelectAll(); input.Focus(); };
        DarkTheme.Apply(dialog);
        if (dialog.ShowDialog() != DialogResult.OK || (!allowEmpty && string.IsNullOrWhiteSpace(input.Text)))
        {
            value = string.Empty;
            return false;
        }
        value = input.Text;
        return true;
    }

    private void OnOutlinerDragEnter(object? sender, DragEventArgs eventArgs) =>
        eventArgs.Effect = eventArgs.Data?.GetDataPresent(typeof(TreeNode)) == true ? DragDropEffects.Move : DragDropEffects.None;

    private void OnOutlinerDragOver(object? sender, DragEventArgs eventArgs)
    {
        if (eventArgs.Data?.GetData(typeof(TreeNode)) is not TreeNode source)
        {
            eventArgs.Effect = DragDropEffects.None;
            return;
        }
        var point = outliner.PointToClient(new Point(eventArgs.X, eventArgs.Y));
        var target = outliner.GetNodeAt(point);
        outliner.SelectedNode = target;
        eventArgs.Effect = CanDropOutlinerNode(source, target) ? DragDropEffects.Move : DragDropEffects.None;
    }

    private bool CanDropOutlinerNode(TreeNode source, TreeNode? target)
    {
        if (!TryGetOutlinerDropCategory(target, out _)) return false;
        if (source.Tag is LevelObject item) return !IsTileOutlinerItem(item) && !IsLevelSettingOutlinerItem(item);
        if (source.Tag is not OutlinerCategory sourceCategory) return false;
        if (target?.Tag is OutlinerCategory targetCategory)
        {
            if (sourceCategory.Id == targetCategory.Id) return false;
            var categories = document.OutlinerCategories.ToDictionary(category => category.Id, StringComparer.Ordinal);
            for (var id = targetCategory.Id; categories.TryGetValue(id, out var category) && category.ParentId != null; id = category.ParentId)
                if (category.ParentId == sourceCategory.Id) return false;
        }
        return true;
    }

    private static bool TryGetOutlinerDropCategory(TreeNode? target, out string? categoryId)
    {
        if (target?.Tag is LevelObject) target = target.Parent;
        if (target?.Tag is OutlinerCategory category)
        {
            categoryId = category.Id;
            return true;
        }
        if (target?.Tag is OutlinerRootTag { Kind: OutlinerRootKind.Objects })
        {
            categoryId = null;
            return true;
        }
        categoryId = null;
        return false;
    }

    private void OnOutlinerDragDrop(object? sender, DragEventArgs eventArgs)
    {
        if (eventArgs.Data?.GetData(typeof(TreeNode)) is not TreeNode source) return;
        var point = outliner.PointToClient(new Point(eventArgs.X, eventArgs.Y));
        var target = outliner.GetNodeAt(point);
        if (!CanDropOutlinerNode(source, target) || !TryGetOutlinerDropCategory(target, out var categoryId)) return;
        try
        {
            if (source.Tag is LevelObject item)
            {
                document.SetOutlinerCategory(item, categoryId);
                SetDirty(true, categoryId == null ? "Moved object to Objects" : "Moved object to category");
                RebuildOutliner(item);
            }
            else if (source.Tag is OutlinerCategory category)
            {
                document.MoveOutlinerCategory(category.Id, categoryId);
                SetDirty(true, "Moved category");
                RebuildOutliner();
            }
        }
        catch (ArgumentException exception)
        {
            MessageBox.Show(this, exception.Message, "Cannot move item", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private static string HumanizeObjectType(string value)
    {
        if (string.IsNullOrEmpty(value)) return "Object";
        var result = new System.Text.StringBuilder(value.Length + 8);
        result.Append(char.ToUpperInvariant(value[0]));
        for (var index = 1; index < value.Length; ++index)
        {
            if (char.IsUpper(value[index]) && !char.IsUpper(value[index - 1])) result.Append(' ');
            result.Append(value[index]);
        }
        return result.ToString();
    }

    private async void StartPlaytest()
    {
        CancelViewportLink();
        if (playtestProcess != null || scriptBuildCancellation != null) return;
        var executable = Path.Combine(AppContext.BaseDirectory, "RaylibGame.exe");
        if (!File.Exists(executable))
        {
            MessageBox.Show(this, "RaylibGame.exe is missing. Build the LevelEditor target to include the game.",
                "Cannot playtest", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }
        try
        {
            var snapshot = document.CaptureSnapshot();
            if (!await BuildScriptsAsync(false)) return;
            if (snapshot != document.CaptureSnapshot()) {
                statusLabel.Text = "Level changed during compilation. Press Play again to use the latest attachments.";
                return;
            }
            // Property edits already update the document. Leave the viewport intact
            // so its camera and selection survive the test session.
            var directory = Path.Combine(Path.GetTempPath(), "PowerPulleyPanic", "Playtests");
            Directory.CreateDirectory(directory);
            playtestSnapshotPath = Path.Combine(directory, $"playtest-{Guid.NewGuid():N}.level");
            document.WriteSnapshot(playtestSnapshotPath, resolveScriptPaths: true);
            var start = new ProcessStartInfo(executable)
            {
                WorkingDirectory = AppContext.BaseDirectory,
                UseShellExecute = false
            };
            start.ArgumentList.Add("--playtest");
            start.ArgumentList.Add(playtestSnapshotPath);
            playtestProcess = Process.Start(start) ?? throw new IOException("The game could not be started.");
            playButton.Enabled = false;
            pauseButton.Enabled = true;
            playtestPaused = false;
            pauseButton.Text = "Pause";
            stopButton.Enabled = true;
            playtestTimer.Start();
            statusLabel.Text = "Playtesting current edits — F5 in the game returns to the editor";
        }
        catch (Exception exception) when (exception is IOException or System.ComponentModel.Win32Exception or UnauthorizedAccessException)
        {
            FinishPlaytest();
            MessageBox.Show(this, exception.Message, "Cannot playtest", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private async Task<bool> BuildScriptsAsync(bool showSuccess)
    {
        if (scriptBuildCancellation != null) return false;
        string[] sources;
        try {
            var root = document.FilePath == null ? Environment.CurrentDirectory : Path.GetDirectoryName(Path.GetFullPath(document.FilePath))!;
            sources = document.Objects.SelectMany(item => item.Scripts).Where(ScriptBuilder.IsCpp)
                .Select(path => Path.GetFullPath(path, root)).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        }
        catch (Exception error) when (error is ArgumentException or IOException or NotSupportedException) {
            MessageBox.Show(this, error.Message, "Invalid script path", MessageBoxButtons.OK, MessageBoxIcon.Error); return false;
        }
        if (sources.Length == 0) { if (showSuccess) statusLabel.Text = "No C++ attachments to build. Lua runs directly."; return true; }
        StopPlaytest();
        var compiler = ScriptBuilder.FindCompiler();
        if (compiler == null || !File.Exists(compiler)) {
            using var picker = new OpenFileDialog { Title = "Select a 64-bit MinGW C++ compiler (g++.exe)", Filter = "C++ compiler (g++.exe)|g++.exe" };
            if (picker.ShowDialog(this) != DialogResult.OK) return false;
            compiler = picker.FileName;
            Environment.SetEnvironmentVariable("PPP_CPP_COMPILER", compiler);
        }
        var sdk = Path.Combine(AppContext.BaseDirectory, "script_sdk");
        if (!File.Exists(Path.Combine(sdk, "CppScript.h"))) {
            var header = FindRepositoryFile("script_sdk/CppScript.h");
            if (header != null) sdk = Path.GetDirectoryName(header)!;
        }
        using var cancellation = new CancellationTokenSource();
        scriptBuildCancellation = cancellation;
        buildScriptsButton.Text = "Cancel Build";
        playButton.Enabled = false;
        statusLabel.Text = $"Building {sources.Length} C++ script(s)…";
        try {
            var result = await ScriptBuilder.BuildAsync(sources, compiler, sdk, cancellation.Token);
            statusLabel.Text = result.Success ? $"Built {result.Built} C++ script(s) — ready to Play" : "Script build failed — see compiler diagnostics";
            if (!result.Success || showSuccess) {
                using var diagnostics = new Form { Text = result.Success ? "Scripts built" : "Script build failed", Width = 900, Height = 550,
                    StartPosition = FormStartPosition.CenterParent, MinimizeBox = false };
                diagnostics.Controls.Add(new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                    ScrollBars = ScrollBars.Both, WordWrap = false, Font = new Font("Consolas", 10),
                    Text = result.Diagnostics.Replace("\r\n", "\n").Replace("\n", Environment.NewLine) });
                diagnostics.ShowDialog(this);
            }
            return result.Success;
        }
        catch (OperationCanceledException) { statusLabel.Text = "Script build cancelled"; return false; }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or ArgumentException) {
            MessageBox.Show(this, error.Message, "Cannot build scripts", MessageBoxButtons.OK, MessageBoxIcon.Error); return false;
        }
        finally { scriptBuildCancellation = null; buildScriptsButton.Text = "Build Scripts"; playButton.Enabled = true; }
    }

    private void StopPlaytest()
    {
        if (playtestProcess is { HasExited: false }) playtestProcess.Kill(true);
        if (playtestProcess != null) playtestProcess.WaitForExit();
        FinishPlaytest();
    }

    private void TogglePlaytestPause()
    {
        if (playtestProcess is not { HasExited: false } process || process.MainWindowHandle == IntPtr.Zero) return;
        if (!NativeMethods.PostMessage(process.MainWindowHandle, NativeMethods.WmKeyDown, (IntPtr)Keys.Escape, IntPtr.Zero) ||
            !NativeMethods.PostMessage(process.MainWindowHandle, NativeMethods.WmKeyUp, (IntPtr)Keys.Escape, IntPtr.Zero))
        {
            statusLabel.Text = "Could not send the pause command to the game";
            return;
        }
        playtestPaused = !playtestPaused;
        pauseButton.Text = playtestPaused ? "Resume" : "Pause";
        pauseButton.ToolTipText = playtestPaused ? "Resume playtesting" : "Pause playtesting";
        statusLabel.Text = playtestPaused ? "Playtest paused" : "Playtesting current edits";
    }

    private void FinishPlaytest()
    {
        playtestTimer.Stop();
        playtestProcess?.Dispose();
        playtestProcess = null;
        if (playtestSnapshotPath != null)
        {
            try { File.Delete(playtestSnapshotPath); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            playtestSnapshotPath = null;
        }
        playButton.Enabled = true;
        pauseButton.Enabled = false;
        playtestPaused = false;
        pauseButton.Text = "Pause";
        pauseButton.ToolTipText = "Pause or resume playtesting";
        stopButton.Enabled = false;
        statusLabel.Text = "Playtest finished — ready to edit";
        Activate();
    }

    private void SaveLevel()
    {
        CancelViewportLink();
        if (document.FilePath == null) { SaveLevelAs(); return; }
        document.Save();
        editHistory.MarkSaved(document.CaptureSnapshot());
        viewport.ReloadDocument(document.FilePath);
        QueueThumbnailLoad();
        SendPaintConfiguration();
        SetDirty(false, $"Saved {Path.GetFileName(document.FilePath)}");
    }

    private void SaveLevelAs()
    {
        CancelViewportLink();
        using var dialog = new SaveFileDialog { Filter = "Power Pulley Panic levels (*.level)|*.level", DefaultExt = "level" };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        document.Save(dialog.FileName);
        editHistory.MarkSaved(document.CaptureSnapshot());
        viewport.ReloadDocument(dialog.FileName);
        QueueThumbnailLoad();
        SendPaintConfiguration();
        SetDirty(false, $"Saved {Path.GetFileName(dialog.FileName)}");
    }

    private void ReloadLevel()
    {
        if (document.FilePath != null && ConfirmDiscard()) LoadLevel(document.FilePath);
    }

    private void ActivateAsset(string? name)
    {
        CancelViewportLink();
        DeactivatePaintTools();
        ClearActiveTransformButton();
        var index = Array.IndexOf(AssetNames, name);
        var placeable = index >= 0;
        var boundsTool = index is >= 7 and <= 10 or 101 or 102 ||
            index >= 0 && GetAssetCommandIndex(index) == 161;
        if (placeable) viewport.SendCommand(27, GetAssetCommandIndex(index));
        statusLabel.Text = placeable
            ? boundsTool
                ? $"{name} bounds: drag in the viewport to set its size."
                : $"Placement tool: {name}. Click the viewport to place it."
            : $"Selected asset: {name}";
        activeToolIndicator.Text = placeable ? $"Active: {name}" : "Active: Select";
        cursorSizeIndicator.Text = boundsTool ? "Cursor: Fluid bounds" : "Cursor: Object bounds";
    }

    private void SelectBrowserAsset(string? name)
    {
        var index = Array.IndexOf(AssetNames, name);
        if (index is >= 7 and <= 10 or 101 or 102 ||
            index >= 0 && GetAssetCommandIndex(index) == 161)
        {
            ActivateAsset(name);
            return;
        }

        DeactivatePaintTools();
        SetActiveTransformButton(Keys.Q);
        viewport.SendCommand(31, 0);
        activeToolIndicator.Text = "Active: Select";
        cursorSizeIndicator.Text = "Cursor: Object bounds";
        statusLabel.Text = index >= 0
            ? $"Drag {name} into the viewport to place it."
            : $"Selected asset: {name}";
    }

    private void SelectTransform(Keys key, string name)
    {
        CancelViewportLink();
        DeactivatePaintTools();
        SetActiveTransformButton(key);
        if (key == Keys.F)
        {
            viewport.SendCommand(25, 1);
        }
        else
        {
            var mode = key switch
            {
                Keys.Q => 0,
                Keys.W => 1,
                Keys.E => 2,
                Keys.R => 3,
                _ => 0
            };
            viewport.SendCommand(31, mode);
        }
        statusLabel.Text = $"Transform: {name}";
        activeToolIndicator.Text = $"Active: {name}";
        cursorSizeIndicator.Text = "Cursor: Object bounds";
    }

    private void DeactivatePaintTools()
    {
        paintTool = 0;
        pencilButton.Checked = false;
        brushButton.Checked = false;
        eraserButton.Checked = false;
        eyedropperButton.Checked = false;
        rectangleButton.Checked = false;
        fillButton.Checked = false;
        tileSelectButton.Checked = false;
        collisionButton.Checked = false;
        lineButton.Checked = false;
        replaceButton.Checked = false;
        stampButton.Checked = false;
        randomBrushButton.Checked = false;
        rectangleOutlineButton.Checked = false;
        ellipseButton.Checked = false;
        measureButton.Checked = false;
        autoTileButton.Checked = false;
        magicWandButton.Checked = false;
        selectMatchingButton.Checked = false;
        viewport.SendCommand(1, 0);
    }

    private void SetPaintTool(int tool, ToolStripButton active)
    {
        CancelViewportLink();
        ClearActiveTransformButton();
        paintTool = active.Checked ? tool : 0;
        foreach (var other in new[] { pencilButton, brushButton, eraserButton, eyedropperButton, rectangleButton, fillButton, tileSelectButton, collisionButton, lineButton, replaceButton, stampButton, randomBrushButton, rectangleOutlineButton, ellipseButton, measureButton, autoTileButton, magicWandButton, selectMatchingButton })
            if (other != active) other.Checked = false;
        SendPaintConfiguration();
        statusLabel.Text = paintTool switch
        {
            1 => "Tile Pencil",
            2 => $"Tile Brush {paintBrushSize}x{paintBrushSize}",
            3 => $"Tile Eraser {paintBrushSize}x{paintBrushSize}",
            4 => "Tile Eyedropper",
            5 => "Rectangle Fill",
            6 => "Flood Fill",
            7 => "Tile Selection",
            8 => "Collision Brush",
            9 => "Tile Line",
            10 => "Replace Matching Tiles",
            11 => "Tile Stamp",
            12 => $"Random Brush {paintBrushSize}x{paintBrushSize}",
            13 => "Rectangle Outline",
            14 => "Ellipse Outline",
            15 => "Measure",
            16 => "Auto Tile Brush",
            17 => "Magic Wand Selection",
            18 => "Select Matching Tiles",
            _ => "Select"
        };
        UpdateToolIndicators();
    }

    private void SendPaintConfiguration()
    {
        viewport.SendCommand(34, connectionsButton.Checked ? 1 : 0);
        viewport.SendCommand(35, focusConnectionsMenu.Checked ? 1 : 0);
        viewport.SendCommand(1, paintTool);
        viewport.SendCommand(2, (paintTileRow << 16) | (paintTileColumn & 0xffff));
        viewport.SendCommand(28, (paintTileHeight << 16) | (paintTileWidth & 0xffff));
        viewport.SendCommand(3, SelectedSheetId());
        viewport.SendCommand(42, paintQuarterTurns | (paintFlipX ? 4 : 0) | (paintFlipY ? 8 : 0));
        viewport.SendCommand(4, paintBrushSize);
        viewport.SendCommand(29, ActiveLayerId());
        viewport.SendCommand(7, VisibleLayerMask());
        viewport.SendCommand(45, collisionLayerMask);
        viewport.SendCommand(8, lockedLayerMask);
        SendSymmetryConfiguration();
        SendAnimationConfiguration();
    }

    private void SelectConnectedObjects()
    {
        DeactivatePaintTools();
        connectionsButton.Checked = true;
        viewport.SendCommand(36, 1);
    }

    private void UpdateToolIndicators()
    {
        if (paintTool == 0)
        {
            activeToolIndicator.Text = "Active: Select";
            cursorSizeIndicator.Text = "Cursor: Object bounds";
            return;
        }
        var usesTileSelection = paintTool is 1 or 2 or 3 or 12;
        var brushSize = paintTool is 2 or 3 or 8 or 12 ? paintBrushSize : 1;
        var width = usesTileSelection ? paintTileWidth * brushSize : brushSize;
        var height = usesTileSelection ? paintTileHeight * brushSize : brushSize;
        activeToolIndicator.Text = paintTool switch
        {
            1 => "Active: Pencil",
            3 => "Active: Eraser",
            4 => "Active: Eyedropper",
            5 => "Active: Rectangle Fill",
            6 => "Active: Flood Fill",
            7 => "Active: Tile Selection",
            8 => "Active: Collision Brush",
            9 => "Active: Line",
            10 => "Active: Replace",
            11 => "Active: Stamp",
            12 => "Active: Random Brush",
            13 => "Active: Rectangle Outline",
            14 => "Active: Ellipse",
            15 => "Active: Measure",
            16 => "Active: Auto Tile",
            17 => "Active: Magic Wand",
            18 => "Active: Select Matching",
            _ => "Active: Brush"
        };
        cursorSizeIndicator.Text = $"Cursor: {width}x{height} tiles ({width * 32}x{height * 32} px)";
    }

    private void LoadSelectedTileSheet()
    {
        if (tileSheetSelector.SelectedIndex < 0) return;
        if (SelectedSheetId() >= 3)
        {
            var path = document.SpriteSheets[SelectedSheetId()];
            try { tilePalette.LoadSheet(path); }
            catch (Exception exception) when (exception is ArgumentException or IOException or OutOfMemoryException)
            {
                tilePalette.LoadSheet(string.Empty);
                statusLabel.Text = $"Could not load sprite sheet: {exception.Message}";
            }
            return;
        }
        string relativePath = SelectedSheetId() switch
        {
            1 => "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png",
            2 => "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/3_Far_Background_Tile.png",
            _ => "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png"
        };
        tilePalette.LoadSheet(Path.Combine(AppContext.BaseDirectory, relativePath.Replace('/', Path.DirectorySeparatorChar)));
    }

    private int SelectedSheetId() => tileSheetSelector.SelectedIndex >= 0 && tileSheetSelector.SelectedIndex < tileSheetIds.Count
        ? tileSheetIds[tileSheetSelector.SelectedIndex] : 0;

    private void RefreshSpriteSheets(int? selectedId = null)
    {
        var previousId = selectedId ?? SelectedSheetId();
        tileSheetSelector.SelectedIndex = -1;
        tileSheetSelector.Items.Clear();
        tileSheetIds.Clear();
        tileSheetIds.AddRange([0, 1, 2]);
        tileSheetSelector.Items.AddRange(["Industrial Foreground", "Industrial Background", "Industrial Far Background"]);
        foreach (var entry in document.SpriteSheets.OrderBy(entry => entry.Key))
        {
            tileSheetIds.Add(entry.Key);
            tileSheetSelector.Items.Add(Path.GetFileNameWithoutExtension(entry.Value));
        }
        tileSheetSelector.SelectedIndex = Math.Max(0, tileSheetIds.IndexOf(previousId));
    }

    private void ImportSpriteSheet()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Load image as a sprite sheet",
            Filter = "Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico)|*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico|All files (*.*)|*.*"
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        try
        {
            using var image = new Bitmap(dialog.FileName);
            // Normalize decoded images to PNG so both renderers support every imported format.
            var directory = Path.Combine(AppContext.BaseDirectory, "assets", "imported_sprite_sheets", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            var imagePath = Path.Combine(directory, $"{Path.GetFileNameWithoutExtension(dialog.FileName)}.png");
            image.Save(imagePath, System.Drawing.Imaging.ImageFormat.Png);
            var id = document.AddSpriteSheet(imagePath);
            RefreshSpriteSheets(id);
            RefreshViewportFromDetails();
            SetDirty(true, $"Loaded sprite sheet {Path.GetFileName(dialog.FileName)}");
        }
        catch (Exception exception) when (exception is ArgumentException or IOException or UnauthorizedAccessException
            or OutOfMemoryException or System.Runtime.InteropServices.ExternalException or InvalidOperationException)
        {
            MessageBox.Show(this, $"Could not load this image. {exception.Message}", "Load Sprite Sheet",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private static Bitmap CreateFolderIcon()
    {
        var image = new Bitmap(18, 18);
        using var graphics = Graphics.FromImage(image);
        using var fill = new SolidBrush(Color.FromArgb(224, 180, 86));
        graphics.FillRectangle(fill, 2, 3, 6, 4);
        graphics.FillRectangle(fill, 2, 6, 14, 9);
        using var outline = new Pen(Color.FromArgb(255, 220, 140));
        graphics.DrawRectangle(outline, 2, 6, 14, 9);
        return image;
    }

    private void OnTilePainted(string operation)
    {
        if (operation == "historybegin") { viewportHistoryTransaction = true; return; }
        if (operation == "historyend")
        {
            viewportHistoryTransaction = false;
            editHistory.Record(document.CaptureSnapshot());
            SetDirty(editHistory.IsDirty, "Edit recorded");
            return;
        }
        if (operation == "historyundo") { UndoEditorChange(); return; }
        if (operation == "historyredo") { RedoEditorChange(); return; }
        if (operation == "historysave") { SaveLevel(); return; }
        if (operation == "historysaveas") { SaveLevelAs(); return; }
        if (operation == "historyopen") { OpenLevel(); return; }
        if (operation == "historyreload") { ReloadLevel(); return; }
        var values = operation.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (operation == "tileneedselection")
        {
            tileSelectButton.Checked = true;
            SetPaintTool(7, tileSelectButton);
            statusLabel.Text = "Select tiles in the current layer in the viewport, then apply the transform";
            return;
        }
        if (operation == "linkcancel") { CancelViewportLink(); return; }
        if (operation == "linkdelete") { RemoveSelectedLink(); return; }
        if (values.Length == 3 && values[0] == "linkselect" && int.TryParse(values[2], out var linkIndex)) {
            SelectConnectionRecord(values[1], linkIndex);
            return;
        }
        if (values.Length == 3 && values[0] == "linktarget" && int.TryParse(values[2], out var targetIndex)) {
            CompleteViewportLink(values[1], targetIndex);
            return;
        }
        if (values.Length == 2 && values[0] == "zoom" && int.TryParse(values[1], out var zoomPercent))
        {
            SetZoomFromControl(zoomPercent, false);
            return;
        }
        if (values.Length == 2 && values[0] == "anchorpoint" && int.TryParse(values[1], out var anchorPoint) &&
            details.SelectedObject is ObjectDetailsAdapter anchorAdapter &&
            outliner.SelectedNode?.Tag is LevelObject anchoredItem)
        {
            anchorAdapter.SetAnchorPointIndex(anchorPoint);
            ApplyAnchorPoint(anchoredItem, anchorAdapter);
            return;
        }
        if (values.Length == 1 && values[0] == "objectsyncbegin")
        {
            synchronizingObjectState = true;
            synchronizedObjectRecords.Clear();
            return;
        }
        if (values.Length == 1 && values[0] == "objectsyncend")
        {
            synchronizingObjectState = false;
            document.ReplaceEditableObjects(synchronizedObjectRecords);
            RebuildOutliner();
            SetDirty(true, "Object changes synchronized");
            return;
        }
        if (synchronizingObjectState && values.Length is 6 or 7 && values[0] == "objectstate" &&
            float.TryParse(values[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var objectX) &&
            float.TryParse(values[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var objectY) &&
            float.TryParse(values[4], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var objectWidth) &&
            float.TryParse(values[5], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var objectHeight))
        {
            AddSynchronizedObjectRecord(values[1], values[1].Equals("playerStart", StringComparison.OrdinalIgnoreCase)
                ? FormattableString.Invariant($"{objectX:0.###} {objectY:0.###} {(values.Length == 7 ? values[6] : "0")}")
                : FormattableString.Invariant($"{objectX:0.###} {objectY:0.###} {objectWidth:0.###} {objectHeight:0.###}"));
            return;
        }
        if (synchronizingObjectState && values.Length == 9 && values[0] == "fluidstate" &&
            float.TryParse(values[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var fluidX) &&
            float.TryParse(values[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var fluidY) &&
            float.TryParse(values[4], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var fluidWidth) &&
            float.TryParse(values[5], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var fluidHeight) &&
            float.TryParse(values[6], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var particleSpacing) &&
            float.TryParse(values[7], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var initialFill) &&
            float.TryParse(values[8], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var flowSpeed))
        {
            AddSynchronizedObjectRecord(values[1], FormattableString.Invariant(
                $"{fluidX:0.###} {fluidY:0.###} {fluidWidth:0.###} {fluidHeight:0.###} {particleSpacing:0.###} {initialFill:0.###} {flowSpeed:0.###}"));
            return;
        }
        if (synchronizingObjectState && values.Length >= 3 && values[0] == "actorstate")
        {
            AddSynchronizedObjectRecord(values[1], string.Join(' ', values[2..]));
            return;
        }
        if (values.Length == 1 && values[0] == "syncbegin")
        {
            synchronizingTileState = true;
            document.ClearTileAndSolidData();
            return;
        }
        if (values.Length == 1 && values[0] == "syncend")
        {
            synchronizingTileState = false;
            RebuildOutliner();
            SetDirty(true, "Tile history restored");
            return;
        }
        if (values.Length == 5 && values[0] == "solidstate" &&
            float.TryParse(values[1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var solidStateX) &&
            float.TryParse(values[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var solidStateY) &&
            float.TryParse(values[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var solidStateWidth) &&
            float.TryParse(values[4], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var solidStateHeight))
        {
            document.AddSolid(solidStateX, solidStateY, solidStateWidth, solidStateHeight);
            return;
        }
        if (values.Length >= 2 && values[0] == "toolstatus")
        {
            statusLabel.Text = operation["toolstatus ".Length..];
            return;
        }
        if (values.Length is 4 or 9 or 10 && values[0] == "pick" && int.TryParse(values[2], out var pickedColumn) && int.TryParse(values[3], out var pickedRow))
        {
            var pickedSheet = values.Length == 10 && int.TryParse(values[9], out var parsedSheet)
                ? Math.Clamp(parsedSheet, 0, 255)
                : values[1] == "farBackground" ? 2 : values[1] == "background" ? 1 : 0;
            tileSheetSelector.SelectedIndex = Math.Max(0, tileSheetIds.IndexOf(pickedSheet));
            paintTileColumn = pickedColumn;
            paintTileRow = pickedRow;
            paintTileWidth = 1;
            paintTileHeight = 1;
            UpdateToolIndicators();
            tilePalette.SelectTile(pickedColumn, pickedRow);
            var pickedLayer = visibleLayers.Items.OfType<TileLayerEntry>()
                .FirstOrDefault(entry => entry.Key.Equals(values[1], StringComparison.OrdinalIgnoreCase));
            if (pickedLayer != null) SelectActiveLayer(pickedLayer);
            if (values.Length is 9 or 10)
            {
                if (int.TryParse(values[7], out var pickedFrames)) animationFrames.Value = Math.Clamp(pickedFrames, 1, 64);
                if (float.TryParse(values[8], System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture, out var pickedSeconds))
                    animationMilliseconds.Value = Math.Clamp((decimal)(pickedSeconds * 1000.0f), 10, 10000);
            }
            statusLabel.Text = $"Picked tile {pickedColumn}, {pickedRow}";
            return;
        }
        if (values.Length == 12 && values[0] == "tileinspect" &&
            int.TryParse(values[2], out var inspectColumn) && int.TryParse(values[3], out var inspectRow) &&
            int.TryParse(values[4], out var inspectX) && int.TryParse(values[5], out var inspectY) &&
            int.TryParse(values[6], out var inspectTurns) && int.TryParse(values[9], out var inspectFrames) &&
            float.TryParse(values[10], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var inspectSeconds) &&
            int.TryParse(values[11], out var inspectCount))
        {
            details.SelectedObject = new TileSelectionProperties
            {
                Layer = values[1],
                Position = $"{inspectX}, {inspectY}",
                SelectedTiles = inspectCount,
                Column = inspectColumn,
                Row = inspectRow,
                QuarterTurns = inspectTurns,
                FlipHorizontal = values[7] == "1",
                FlipVertical = values[8] == "1",
                AnimationFrames = inspectFrames,
                FrameMilliseconds = (int)MathF.Round(inspectSeconds * 1000.0f),
                ScriptTarget = inspectCount == 1 ? document.Objects.FirstOrDefault(item =>
                    item.Type == "visualTile" && item.Arguments.Split(' ', StringSplitOptions.RemoveEmptyEntries) is var parts &&
                    parts.Length >= 5 && parts[0] == values[1] && parts[3] == values[4] && parts[4] == values[5]) : null
            };
            return;
        }
        if (values.Length == 4 && values[0] == "solid" && int.TryParse(values[2], out var solidX) && int.TryParse(values[3], out var solidY))
        {
            if (values[1] == "add") document.UpsertSolidCell(solidX, solidY);
            else document.RemoveSolidCell(solidX, solidY);
            outlinerRefreshTimer.Stop();
            outlinerRefreshTimer.Start();
            SetDirty(true, $"Collision {(values[1] == "add" ? "painted" : "removed")} at {solidX}, {solidY}");
            return;
        }
        if (values.Length is 5 or 6 && values[0] == "tileselect")
        {
            statusLabel.Text = values.Length == 6
                ? $"Selected {values[5]} tiles: ({values[1]}, {values[2]}) to ({values[3]}, {values[4]})"
                : $"Selected tiles: ({values[1]}, {values[2]}) to ({values[3]}, {values[4]})";
            return;
        }
        if (values.Length == 4 && values[0] == "erase" &&
            int.TryParse(values[2], out var eraseX) && int.TryParse(values[3], out var eraseY))
        {
            document.RemoveVisualTile(values[1], eraseX, eraseY);
            outlinerRefreshTimer.Stop();
            outlinerRefreshTimer.Start();
            SetDirty(true, $"Erased tile at {eraseX}, {eraseY}");
            return;
        }

        if (values.Length is not (6 or 9 or 11 or 12) || values[0] != "paint" ||
            !int.TryParse(values[2], out var column) || !int.TryParse(values[3], out var row) ||
            !int.TryParse(values[4], out var x) || !int.TryParse(values[5], out var y)) return;
        var quarterTurns = values.Length >= 9 && int.TryParse(values[6], out var parsedTurns) ? parsedTurns : 0;
        var flipX = values.Length >= 9 && values[7] == "1";
        var flipY = values.Length >= 9 && values[8] == "1";
        var frames = values.Length >= 11 && int.TryParse(values[9], out var parsedFrames) ? parsedFrames : 1;
        var frameSeconds = values.Length >= 11 && float.TryParse(values[10], System.Globalization.NumberStyles.Float,
            System.Globalization.CultureInfo.InvariantCulture, out var parsedSeconds) ? parsedSeconds : 0.12f;
        var sheetIndex = values.Length == 12 && int.TryParse(values[11], out var parsedSheetIndex)
            ? parsedSheetIndex : values[1] == "farBackground" ? 2 : values[1] == "background" ? 1 : 0;
        document.UpsertVisualTile(values[1], column, row, x, y, quarterTurns, flipX, flipY, frames, frameSeconds, sheetIndex);
        if (synchronizingTileState) return;
        outlinerRefreshTimer.Stop();
        outlinerRefreshTimer.Start();
        SetDirty(true, $"Painted tile at {x}, {y}");
    }

    private void AddSynchronizedObjectRecord(string type, string arguments)
    {
        if (!synchronizedObjectRecords.TryGetValue(type, out var records))
        {
            records = [];
            synchronizedObjectRecords[type] = records;
        }
        records.Add(arguments);
    }

    private void QueueThumbnailLoad()
    {
        thumbnailRefreshTimer.Stop();
        if (!LoadContentThumbnails()) thumbnailRefreshTimer.Start();
    }

    private bool LoadContentThumbnails()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "editor_thumbnails", "content_atlas.png");
        if (!File.Exists(path)) return false;
        try
        {
            using var atlas = new Bitmap(path);
            if (atlas.Width < 560 || atlas.Height < ((AssetNames.Length + 1 + 4) / 5) * 84) return false;
            contentImages.Images.Clear();
            for (var index = 0; index < AssetNames.Length; ++index)
            {
                var atlasIndex = GetAssetCommandIndex(index);
                var source = new Rectangle((atlasIndex % 5) * 112, (atlasIndex / 5) * 84, 112, 84);
                contentImages.Images.Add(atlas.Clone(source, System.Drawing.Imaging.PixelFormat.Format32bppArgb));
            }
            contentBrowser.Invalidate();
            return true;
        }
        catch (IOException)
        {
            return false;
        }
    }

    private void OnOutlinerSelectionChanged(object? sender, TreeViewEventArgs eventArgs)
    {
        if (eventArgs.Node?.Tag is LevelObject selectedItem)
            ShowObjectDetails(selectedItem);
        else details.SelectedObject = eventArgs.Node?.Tag;
        if (!synchronizingSelection) {
            selectingConnectionRecord = eventArgs.Node?.Tag is LevelObject link && link.Type != "buttonPlatformLoop" &&
                Array.IndexOf(EditableConnectionTypes, link.Type) >= 0;
            SyncSelectedConnection();
            if (selectingConnectionRecord) {
                CancelViewportLink(); DeactivatePaintTools();
                activeToolIndicator.Text = "Active: Connection";
                statusLabel.Text = "Edit the connection in Details, or press Delete to remove it.";
                return;
            }
        }
        if (!synchronizingSelection && eventArgs.Node?.Tag is LevelObject loop && loop.Type == "buttonPlatformLoop") {
            CancelViewportLink();
            DeactivatePaintTools();
            viewport.SendCommand(43, loop.Index - 1);
            RestoreSelectedTransformTool();
            cursorSizeIndicator.Text = "Cursor: Loop bounds";
            return;
        }
        if (synchronizingSelection || eventArgs.Node?.Tag is not LevelObject item ||
            !TryGetObjectPosition(item, out var x, out var y)) return;
        DeactivatePaintTools();
        var packed = ((Math.Clamp(y, short.MinValue, short.MaxValue) & 0xffff) << 16) |
            (Math.Clamp(x, short.MinValue, short.MaxValue) & 0xffff);
        viewport.SendCommand(5, packed);
        RestoreSelectedTransformTool();
        cursorSizeIndicator.Text = "Cursor: Object bounds";
    }

    private void RestoreSelectedTransformTool()
    {
        var key = transformButtons.FirstOrDefault(pair => pair.Value.Checked).Key;
        if (key == Keys.None) key = Keys.Q;
        var mode = key == Keys.W ? 1 : key == Keys.E ? 2 : key == Keys.R ? 3 : 0;
        viewport.SendCommand(31, mode);
        SetActiveTransformButton(key);
        activeToolIndicator.Text = $"Active: {(mode == 1 ? "Move" : mode == 2 ? "Rotate" : mode == 3 ? "Scale" : "Select")}";
    }

    private void OnDetailsPropertyValueChanged(object? sender, PropertyValueChangedEventArgs eventArgs)
    {
        if (details.SelectedObject is ObjectDetailsAdapter adapter)
        {
            if (eventArgs.ChangedItem?.PropertyDescriptor?.Name == "Anchor Point" &&
                outliner.SelectedNode?.Tag is LevelObject anchoredItem)
            {
                ApplyAnchorPoint(anchoredItem, adapter);
                return;
            }
            if (outliner.SelectedNode?.Tag is LevelObject item)
                outliner.SelectedNode.Text = OutlinerObjectLabel(item);
            detailsPreviewTimer.Stop();
            SetDirty(true, "Object properties updated");
            RefreshViewportFromDetails();
            return;
        }
        if (details.SelectedObject is not TileSelectionProperties properties)
        {
            SetDirty(true, "Property changed. Save to apply it to the viewport.");
            return;
        }
        if (eventArgs.ChangedItem?.PropertyDescriptor?.Name == nameof(TileSelectionProperties.Scripts))
        {
            SetDirty(true, "Tile scripts updated");
            RefreshViewportFromDetails();
            return;
        }
        var command = eventArgs.ChangedItem?.PropertyDescriptor?.Name switch
        {
            nameof(TileSelectionProperties.Column) => 17,
            nameof(TileSelectionProperties.Row) => 18,
            nameof(TileSelectionProperties.QuarterTurns) => 19,
            nameof(TileSelectionProperties.FlipHorizontal) => 20,
            nameof(TileSelectionProperties.FlipVertical) => 21,
            nameof(TileSelectionProperties.AnimationFrames) => 22,
            nameof(TileSelectionProperties.FrameMilliseconds) => 23,
            _ => 0
        };
        if (command == 0) return;
        var value = command switch
        {
            17 => properties.Column,
            18 => properties.Row,
            19 => properties.QuarterTurns,
            20 => properties.FlipHorizontal ? 1 : 0,
            21 => properties.FlipVertical ? 1 : 0,
            22 => properties.AnimationFrames,
            _ => properties.FrameMilliseconds
        };
        viewport.SendCommand(command, value);
        SetDirty(true, "Tile selection properties updated");
    }

    private void RefreshViewportFromDetails()
    {
        CancelViewportLink();
        detailsPreviewTimer.Stop();
        try
        {
            var previewDirectory = Path.Combine(Path.GetTempPath(), "PowerPulleyPanic", "LevelEditorPreview");
            Directory.CreateDirectory(previewDirectory);
            detailsPreviewPath = Path.Combine(previewDirectory, $"preview-{Environment.ProcessId}.level");
            document.WriteSnapshot(detailsPreviewPath, resolveScriptPaths: true);
            viewport.ReloadDocument(detailsPreviewPath);
            SendPaintConfiguration();
            SetGridSnap(gridSnap);

            if (outliner.SelectedNode?.Tag is LevelObject loop && loop.Type == "buttonPlatformLoop")
                viewport.SendCommand(43, loop.Index - 1);
            if (outliner.SelectedNode?.Tag is LevelObject item && TryGetObjectPosition(item, out var x, out var y))
            {
                var packed = ((Math.Clamp(y, short.MinValue, short.MaxValue) & 0xffff) << 16) |
                    (Math.Clamp(x, short.MinValue, short.MaxValue) & 0xffff);
                viewport.SendCommand(5, packed);
            }
            QueueThumbnailLoad();
            SyncSelectedConnection();
            statusLabel.Text = "Viewport updated from object properties";
        }
        catch (IOException exception)
        {
            statusLabel.Text = $"Could not refresh viewport: {exception.Message}";
        }
    }

    private void LinkSelectedObject()
    {
        CancelViewportLink();
        if (outliner.SelectedNode?.Tag is not LevelObject source || source.Type is not ("button" or "valve"))
        {
            statusLabel.Text = "Select a button or valve in the viewport or outliner, then choose Link.";
            return;
        }
        var targets = document.ConnectionTargets(source);
        if (targets.Count == 0) { statusLabel.Text = "Place a compatible target before linking this object."; return; }
        using var dialog = new Form { Text = $"Link {HumanizeObjectType(source.Type)} {source.Index}",
            Size = new Size(490, 225), StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog, MinimizeBox = false, MaximizeBox = false };
        var label = new Label { Text = source.Type == "valve" ? "Choose the fluid region this valve fills:" :
            "Choose what this button controls:", Left = 16, Top = 16, Width = 440 };
        var picker = new ComboBox { Left = 16, Top = 45, Width = 440, DropDownStyle = ComboBoxStyle.DropDownList };
        foreach (var target in targets) picker.Items.Add(OutlinerObjectLabel(target));
        picker.SelectedIndex = 0;
        var hint = new Label { Left = 16, Top = 80, Width = 440, Height = 45,
            Text = "Ladders, platforms, and spikes become button-controlled geometry. Link settings appear in the outliner and Details." };
        var apply = new Button { Text = "Create Link", Left = 250, Top = 135, Width = 100, DialogResult = DialogResult.OK };
        var cancel = new Button { Text = "Cancel", Left = 360, Top = 135, Width = 95, DialogResult = DialogResult.Cancel };
        dialog.Controls.AddRange([label, picker, hint, apply, cancel]);
        dialog.AcceptButton = apply; dialog.CancelButton = cancel;
        DarkTheme.Apply(dialog);
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        try {
            document.ConnectObjects(source, targets[picker.SelectedIndex]);
            SetDirty(true, "Connection created");
            RebuildOutliner();
            RefreshViewportFromDetails();
            connectionsButton.Checked = true;
            statusLabel.Text = "Connection created — select its link record in the outliner to edit settings.";
        }
        catch (InvalidOperationException exception) { statusLabel.Text = exception.Message; }
    }

    private void BeginViewportLink()
    {
        if ((VisibleLayerMask() & (1 << 12)) == 0 || (lockedLayerMask & (1 << 12)) != 0) {
            statusLabel.Text = "Show and unlock Middleground before linking gameplay objects.";
            return;
        }
        if (pendingConnectionSource != null) { CancelViewportLink(); return; }
        if (outliner.SelectedNode?.Tag is not LevelObject source || source.Type is not ("button" or "valve")) {
            statusLabel.Text = "Select a button or valve first, then click Link.";
            return;
        }
        if (document.ConnectionTargets(source).Count == 0) {
            statusLabel.Text = "Place a compatible target before linking this object.";
            return;
        }
        DeactivatePaintTools();
        ClearActiveTransformButton();
        pendingConnectionSource = source;
        connectionsButton.Checked = true;
        viewport.SendCommand(39, source.Type == "valve" ? -1 : document.RuntimeButtonIndex(source) + 1);
        activeToolIndicator.Text = "Active: Link";
        cursorSizeIndicator.Text = "Cursor: Target";
        statusLabel.Text = "Click a compatible target in the viewport. Escape or right-click cancels.";
        viewport.Focus();
    }

    private void CancelViewportLink()
    {
        if (pendingConnectionSource == null) return;
        pendingConnectionSource = null;
        viewport.SendCommand(39, 0);
        activeToolIndicator.Text = "Active: Select";
        cursorSizeIndicator.Text = "Cursor: Object bounds";
        statusLabel.Text = "Linking canceled";
    }

    private void SelectConnectionRecord(string type, int index)
    {
        var item = document.Objects.FirstOrDefault(item => item.Type == type && item.Index == index + 1);
        if (item == null) return;
        var node = FindNodeForObject(outliner.Nodes, item);
        if (node == null) return;
        CancelViewportLink(); DeactivatePaintTools();
        synchronizingSelection = true;
        try {
            node.Parent?.Expand(); outliner.SelectedNode = node; node.EnsureVisible();
        } finally { synchronizingSelection = false; }
        selectingConnectionRecord = true;
        ShowObjectDetails(item); SyncSelectedConnection();
        activeToolIndicator.Text = "Active: Connection";
        statusLabel.Text = "Connection selected — edit Details or press Delete to remove it.";
        outliner.Focus();
    }

    private void SyncSelectedConnection()
    {
        var item = outliner.SelectedNode?.Tag as LevelObject;
        var type = item == null ? -1 : Array.IndexOf(EditableConnectionTypes, item.Type);
        viewport.SendCommand(44, selectingConnectionRecord && type >= 0 ? ((item!.Index - 1) << 4) | (type + 1) : 0);
    }

    private void CompleteViewportLink(string targetType, int index)
    {
        var source = pendingConnectionSource;
        if (source == null) return;
        var targets = targetType == "fluid"
            ? document.Objects.Where(item => item.Type is "water" or "sand" or "gel" or "gas").ToList()
            : document.Objects.Where(item => item.Type == targetType).ToList();
        if (index < 0 || index >= targets.Count || !document.Objects.Contains(source)) {
            CancelViewportLink();
            statusLabel.Text = "The source or target changed. Select the source and link again.";
            return;
        }
        try {
            document.ConnectObjects(source, targets[index]);
            pendingConnectionSource = null;
            activeToolIndicator.Text = "Active: Select";
            cursorSizeIndicator.Text = "Cursor: Object bounds";
            SetDirty(true, "Connection created");
            RebuildOutliner();
            RefreshViewportFromDetails();
            statusLabel.Text = "Connection created — select its link record in the outliner to edit settings.";
        } catch (InvalidOperationException exception) {
            CancelViewportLink(); statusLabel.Text = exception.Message;
        }
    }

    private void ImportAudio(bool music)
    {
        var target = (details.SelectedObject as ObjectDetailsAdapter)?.Item;
        if (!music && target == null) { statusLabel.Text = "Select a game object first."; return; }
        if (document.FilePath == null) { SaveLevelAs(); if (document.FilePath == null) return; }
        using var dialog = new OpenFileDialog { Title = music ? "Choose Level Music" : "Attach Sound Effect",
            Filter = "Audio (*.wav;*.ogg;*.mp3;*.flac)|*.wav;*.ogg;*.mp3;*.flac" };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        try {
            var folder = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(document.FilePath!))!, "audio");
            Directory.CreateDirectory(folder);
            var destination = Path.Combine(folder, Path.GetFileName(dialog.FileName));
            if (!Path.GetFullPath(dialog.FileName).Equals(destination, StringComparison.OrdinalIgnoreCase)) {
                if (File.Exists(destination)) destination = Path.Combine(folder, Path.GetFileNameWithoutExtension(destination) + "_" + Guid.NewGuid().ToString("N")[..8] + Path.GetExtension(destination));
                File.Copy(dialog.FileName, destination);
            }
            var path = Path.GetRelativePath(Path.GetDirectoryName(Path.GetFullPath(document.FilePath!))!, destination).Replace('\\', '/');
            if (music) document.SetMusic(path); else target!.Sound = path;
            SetDirty(true, music ? "Level music added" : "Sound effect added — plays on player contact");
            details.Refresh(); RebuildOutliner(); RefreshViewportFromDetails();
        } catch (Exception exception) when (exception is IOException or UnauthorizedAccessException) {
            MessageBox.Show(this, exception.Message, "Could not import audio", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void AttachCppScript(bool lua = false)
    {
        var target = details.SelectedObject switch
        {
            ObjectDetailsAdapter adapter => adapter.Item,
            TileSelectionProperties tile => tile.ScriptTarget,
            _ => null
        };
        if (target == null)
        {
            statusLabel.Text = "Select one tile or a game object to attach a script.";
            return;
        }
        using var dialog = new OpenFileDialog { Title = lua ? "Attach Lua Script" : "Attach C++ Script", Multiselect = true,
            Filter = lua ? "Lua scripts (*.lua)|*.lua" : "C++ source (*.cpp;*.cc;*.cxx)|*.cpp;*.cc;*.cxx" };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        var root = document.FilePath == null ? null : Path.GetDirectoryName(Path.GetFullPath(document.FilePath));
        var paths = dialog.FileNames.Select(path => (root == null ? Path.GetFullPath(path) : Path.GetRelativePath(root, path)).Replace('\\', '/'));
        target.Scripts = target.Scripts.Concat(paths).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        details.Refresh();
        SetDirty(true, lua ? "Lua scripts attached — Play to run them" : "C++ scripts attached");
        RefreshViewportFromDetails();
    }

    private void AttachRidingButton()
    {
        var loops = document.Objects.Where(item => item.Type == "buttonPlatformLoop").ToList();
        if (loops.Count == 0) { statusLabel.Text = "Create a platform loop first."; return; }
        var selected = outliner.SelectedNode?.Tag as LevelObject;
        var button = selected?.Type == "button" ? selected : null;
        using var dialog = new Form { Text = "Attach Riding Button", Size = new Size(480, 230),
            StartPosition = FormStartPosition.CenterParent, FormBorderStyle = FormBorderStyle.FixedDialog,
            MinimizeBox = false, MaximizeBox = false };
        var hint = new Label { Left = 16, Top = 12, Width = 430, Height = 35,
            Text = button == null ? "Create a button that rides on the chosen platform." : $"Attach Button {button.Index} to the chosen platform." };
        var loopPicker = new ComboBox { Left = 16, Top = 50, Width = 430, DropDownStyle = ComboBoxStyle.DropDownList };
        foreach (var loop in loops) loopPicker.Items.Add(OutlinerObjectLabel(loop));
        var platformLabel = new Label { Left = 16, Top = 88, Width = 120, Text = "Platform number" };
        var platform = new NumericUpDown { Left = 145, Top = 85, Minimum = 1, Maximum = 16, Value = 1 };
        loopPicker.SelectedIndexChanged += (_, _) => {
            var args = loops[loopPicker.SelectedIndex].Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            platform.Maximum = args.Length >= 9 && int.TryParse(args[8], out var count) ? Math.Clamp(count, 1, 16) : 1;
        };
        loopPicker.SelectedIndex = Math.Max(0, loops.IndexOf(selected!));
        var apply = new Button { Text = "Attach", Left = 260, Top = 140, DialogResult = DialogResult.OK };
        var cancel = new Button { Text = "Cancel", Left = 355, Top = 140, DialogResult = DialogResult.Cancel };
        dialog.Controls.AddRange([hint, loopPicker, platformLabel, platform, apply, cancel]);
        dialog.AcceptButton = apply; dialog.CancelButton = cancel; DarkTheme.Apply(dialog);
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        try {
            document.AttachLoopButton(loops[loopPicker.SelectedIndex], (int)platform.Value - 1, button);
            SetDirty(true, "Riding button attached"); RebuildOutliner(); RefreshViewportFromDetails();
            connectionsButton.Checked = true;
            statusLabel.Text = "Riding button attached — select the button and use Link to choose what it controls.";
        } catch (InvalidOperationException exception) { statusLabel.Text = exception.Message; }
    }

    private void DeleteSelectedLoop()
    {
        if (outliner.SelectedNode?.Tag is not LevelObject loop || loop.Type != "buttonPlatformLoop") {
            statusLabel.Text = "Select a platform loop in the outliner first."; return;
        }
        document.DeletePlatformLoop(loop);
        SetDirty(true, "Platform loop deleted"); RebuildOutliner(); RefreshViewportFromDetails();
    }

    private void RemoveSelectedLink()
    {
        if (outliner.SelectedNode?.Tag is not LevelObject item) return;
        var before = document.CaptureSnapshot();
        document.RemoveConnection(item);
        if (before == document.CaptureSnapshot()) { statusLabel.Text = "Select a link record in the outliner to remove it."; return; }
        SetDirty(true, "Connection removed");
        RebuildOutliner();
        RefreshViewportFromDetails();
    }

    private void UndoEditorChange()
    {
        if (viewportHistoryTransaction) return;
        RestoreHistory(editHistory.Undo(), "Edit undone");
    }

    private void RedoEditorChange()
    {
        if (viewportHistoryTransaction) return;
        RestoreHistory(editHistory.Redo(), "Edit redone");
    }

    private void RestoreHistory(string? snapshot, string status)
    {
        if (snapshot == null) { statusLabel.Text = "No more history"; return; }
        restoringHistory = true;
        try
        {
            document.RestoreSnapshot(snapshot);
            RefreshSpriteSheets();
            details.SelectedObject = null;
            RebuildOutliner();
            RefreshTileLayers();
            RefreshViewportFromDetails();
            SetDirty(editHistory.IsDirty, status);
        }
        finally { restoringHistory = false; }
    }

    private void OnViewportSelectionChanged(string message)
    {
        selectingConnectionRecord = false;
        viewport.SendCommand(44, 0);
        var values = message.Split('\t');
        ClearOutlinerMultiSelection(outliner.Nodes);
        if (values.Length == 2 && values[0] == "multiselect")
        {
            TreeNode? primaryNode = null;
            foreach (var entry in values[1].Split(';', StringSplitOptions.RemoveEmptyEntries))
            {
                var separator = entry.LastIndexOf(':');
                if (separator <= 0 || !int.TryParse(entry[(separator + 1)..], out var selectedIndex)) continue;
                var selectedItem = FindOutlinerObject(entry[..separator], selectedIndex);
                if (selectedItem == null) continue;
                var selectedNode = FindNodeForObject(outliner.Nodes, selectedItem);
                if (selectedNode == null) continue;
                selectedNode.BackColor = DarkTheme.Selection;
                selectedNode.ForeColor = Color.White;
                primaryNode = selectedNode;
            }
            if (primaryNode != null)
            {
                synchronizingSelection = true;
                primaryNode.Parent?.Expand();
                outliner.SelectedNode = primaryNode;
                primaryNode.EnsureVisible();
                synchronizingSelection = false;
                if (primaryNode.Tag is LevelObject primaryItem) ShowObjectDetails(primaryItem);
            }
            return;
        }
        if (values.Length != 3 || !int.TryParse(values[2], out var index)) return;
        if (values[1] == "None")
        {
            synchronizingSelection = true;
            outliner.SelectedNode = null;
            synchronizingSelection = false;
            details.SelectedObject = null;
            RefreshAnchorPicker();
            viewport.SendCommand(38, 4);
            return;
        }
        var item = FindOutlinerObject(values[1], index);
        if (item == null) return;
        var node = FindNodeForObject(outliner.Nodes, item);
        if (node == null) return;
        synchronizingSelection = true;
        node.Parent?.Expand();
        outliner.SelectedNode = node;
        node.EnsureVisible();
        synchronizingSelection = false;
        ShowObjectDetails(item);
    }

    private void ShowObjectDetails(LevelObject item)
    {
        var anchorPoint = document.GetAnchorPoint(item);
        details.SelectedObject = new ObjectDetailsAdapter(item, anchorPoint);
        RefreshAnchorPicker();
        viewport.SendCommand(38, anchorPoint);
    }

    private Control BuildAnchorPicker()
    {
        var panel = anchorPickerPanel;
        panel.Height = 88;
        panel.BackColor = DarkTheme.Panel;
        panel.Padding = new Padding(8, 2, 8, 4);
        panel.Controls.Add(new Label { Text = "Transform Anchor", Dock = DockStyle.Top, Height = 21, TextAlign = ContentAlignment.MiddleLeft });
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.None,
            Location = new Point(8, 23),
            Width = 108,
            Height = 60,
            ColumnCount = 3,
            RowCount = 3,
            Margin = Padding.Empty,
            Padding = Padding.Empty,
            BackColor = DarkTheme.Panel
        };
        for (var index = 0; index < 3; ++index)
        {
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / 3f));
            grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100f / 3f));
        }
        string[] symbols = ["↖", "↑", "↗", "←", "•", "→", "↙", "↓", "↘"];
        string[] names = ["Top Left", "Top Center", "Top Right", "Middle Left", "Center", "Middle Right", "Bottom Left", "Bottom Center", "Bottom Right"];
        for (var index = 0; index < anchorPointButtons.Length; ++index)
        {
            var button = new Button
            {
                Text = symbols[index],
                Dock = DockStyle.Fill,
                Margin = new Padding(1),
                FlatStyle = FlatStyle.Flat,
                BackColor = DarkTheme.Header,
                ForeColor = DarkTheme.Text,
                TabStop = false
            };
            button.FlatAppearance.BorderColor = DarkTheme.Border;
            button.Click += (_, _) => SetAnchorPointFromPicker(Array.IndexOf(anchorPointButtons, button));
            anchorPointToolTip.SetToolTip(button, names[index]);
            anchorPointButtons[index] = button;
            grid.Controls.Add(button, index % 3, index / 3);
        }
        panel.Controls.Add(grid);
        return panel;
    }

    private void SetAnchorPointFromPicker(int index)
    {
        if (details.SelectedObject is not ObjectDetailsAdapter adapter ||
            outliner.SelectedNode?.Tag is not LevelObject item || index < 0) return;
        adapter.SetAnchorPointIndex(index);
        ApplyAnchorPoint(item, adapter);
    }

    private void ApplyAnchorPoint(LevelObject item, ObjectDetailsAdapter adapter)
    {
        document.SetAnchorPoint(item, adapter.AnchorPointIndex);
        viewport.SendCommand(38, adapter.AnchorPointIndex);
        details.Refresh();
        RefreshAnchorPicker();
        statusLabel.Text = $"Transform anchor: {adapter.AnchorPointIndex switch
        {
            0 => "Top Left", 1 => "Top Center", 2 => "Top Right",
            3 => "Middle Left", 4 => "Center", 5 => "Middle Right",
            6 => "Bottom Left", 7 => "Bottom Center", _ => "Bottom Right"
        }}";
        SetDirty(true, "Transform anchor updated");
    }

    private void RefreshAnchorPicker()
    {
        var selected = details.SelectedObject is ObjectDetailsAdapter adapter ? adapter.AnchorPointIndex : -1;
        anchorPickerPanel.Visible = selected >= 0;
        for (var index = 0; index < anchorPointButtons.Length; ++index)
        {
            var button = anchorPointButtons[index];
            if (button == null) continue;
            button.BackColor = index == selected ? DarkTheme.Selection : DarkTheme.Header;
            button.FlatAppearance.BorderColor = index == selected ? DarkTheme.Accent : DarkTheme.Border;
        }
    }

    private static void ClearOutlinerMultiSelection(TreeNodeCollection nodes)
    {
        foreach (TreeNode node in nodes)
        {
            node.BackColor = Color.Empty;
            node.ForeColor = Color.Empty;
            ClearOutlinerMultiSelection(node.Nodes);
        }
    }

    private LevelObject? FindOutlinerObject(string viewportKind, int index)
    {
        var objects = document.Objects;
        var baseKind = viewportKind.TrimEnd(' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9');
        var aliases = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["Camera Zone"] = "cameraZone",
            ["Darkness"] = "darkness",
            ["Player Start"] = "playerStart",
            ["Exit Trigger"] = "exit",
            ["Directional Spikes"] = "directionalSpikeHazard",
            ["Water Pit"] = "waterPit",
            ["Portal Pair"] = "portalPair"
        };
        if (aliases.TryGetValue(baseKind, out var type))
            return objects.FirstOrDefault(item => item.Type.Equals(type, StringComparison.OrdinalIgnoreCase) && item.Index == index);
        if (baseKind.Equals("Fluid Field", StringComparison.OrdinalIgnoreCase))
            return objects.Where(item => item.Type is "water" or "sand" or "gel" or "gas").ElementAtOrDefault(index - 1);

        var normalized = NormalizeObjectName(baseKind);
        return objects.FirstOrDefault(item => NormalizeObjectName(HumanizeObjectType(item.Type)) == normalized && item.Index == index);
    }

    private static TreeNode? FindNodeForObject(TreeNodeCollection nodes, LevelObject item)
    {
        foreach (TreeNode node in nodes)
        {
            if (ReferenceEquals(node.Tag, item)) return node;
            var child = FindNodeForObject(node.Nodes, item);
            if (child != null) return child;
        }
        return null;
    }

    private static string NormalizeObjectName(string value) =>
        new(value.Where(char.IsLetterOrDigit).Select(char.ToLowerInvariant).ToArray());

    private static bool TryGetObjectPosition(LevelObject item, out int x, out int y)
    {
        x = 0;
        y = 0;
        var values = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (item.Type is "buttonTrapDoor" or "buttonFan" or "buttonExit" or "valveFluidFill" or "platformLoopButton" or "buttonPlatformLoop") return false;
        var coordinateIndex = item.Type == "buttonSpikeHazard" ? 2 : item.Type is "buttonLadder" or "buttonPlatform" ? 1 :
            item.Type.Equals("toxinLeak", StringComparison.OrdinalIgnoreCase) ? 1 :
            item.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) ||
            item.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase) ? 3 : 0;
        if (values.Length <= coordinateIndex + 1 ||
            !float.TryParse(values[coordinateIndex], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var parsedX) ||
            !float.TryParse(values[coordinateIndex + 1], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var parsedY)) return false;
        x = (int)MathF.Round(parsedX);
        y = (int)MathF.Round(parsedY);
        return true;
    }

    private void OnEditorKeyDown(object? sender, KeyEventArgs eventArgs)
    {
        if (eventArgs.KeyCode == Keys.Insert) {
            viewport.SendCommand(46, 1);
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (eventArgs.KeyCode == Keys.Escape && selectingConnectionRecord) {
            selectingConnectionRecord = false;
            viewport.SendCommand(44, 0);
            synchronizingSelection = true;
            try { outliner.SelectedNode = null; } finally { synchronizingSelection = false; }
            details.SelectedObject = null;
            activeToolIndicator.Text = "Active: Select";
            statusLabel.Text = "Connection selection cleared";
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (visibleLayers.IsRenaming) return;
        if (outliner.ContainsFocus && eventArgs.KeyCode == Keys.F2)
        {
            RenameOutlinerNode(outliner.SelectedNode);
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (outliner.ContainsFocus && eventArgs.KeyCode == Keys.Delete && outliner.SelectedNode?.Tag is OutlinerCategory)
        {
            DeleteSelectedOutlinerCategory();
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (eventArgs.KeyCode == Keys.Escape && pendingConnectionSource != null) {
            CancelViewportLink(); eventArgs.SuppressKeyPress = true; return;
        }
        if (eventArgs.Control && eventArgs.KeyCode is Keys.Z or Keys.Y)
        {
            if (eventArgs.KeyCode == Keys.Z) UndoEditorChange();
            else RedoEditorChange();
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (paintTool is 7 or 17 or 18 && eventArgs.Control && eventArgs.KeyCode is Keys.C or Keys.V)
        {
            viewport.SendCommand(6, eventArgs.KeyCode == Keys.C ? 2 : 3);
            statusLabel.Text = eventArgs.KeyCode == Keys.C ? "Copied tile selection" : "Pasted tile selection";
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (paintTool is 7 or 17 or 18 && eventArgs.Control && eventArgs.KeyCode == Keys.D)
        {
            viewport.SendCommand(6, 8);
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (paintTool is 7 or 17 or 18 && eventArgs.KeyCode is Keys.Left or Keys.Right or Keys.Up or Keys.Down)
        {
            var command = eventArgs.KeyCode switch
            {
                Keys.Left => 9,
                Keys.Right => 10,
                Keys.Up => 11,
                _ => 12
            };
            viewport.SendCommand(6, command);
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (paintTool is 7 or 17 or 18 && eventArgs.KeyCode == Keys.Delete)
        {
            viewport.SendCommand(6, 1);
            eventArgs.SuppressKeyPress = true;
            return;
        }
        if (eventArgs.KeyCode is Keys.Q or Keys.W or Keys.E or Keys.R)
        {
            var name = eventArgs.KeyCode switch
            {
                Keys.W => "Move",
                Keys.E => "Rotate",
                Keys.R => "Scale",
                _ => "Select"
            };
            SelectTransform(eventArgs.KeyCode, name);
            eventArgs.Handled = true;
        }
        else if (eventArgs.KeyCode == Keys.F)
        {
            FrameSelection();
            eventArgs.Handled = true;
        }
        else if (eventArgs.Control && eventArgs.KeyCode == Keys.D)
        {
            DuplicateSelectedObjects();
            eventArgs.SuppressKeyPress = true;
        }
        else if (eventArgs.KeyCode == Keys.Delete)
        {
            if (selectingConnectionRecord) RemoveSelectedLink();
            else if (outliner.SelectedNode?.Tag is LevelObject loop && loop.Type == "buttonPlatformLoop") DeleteSelectedLoop();
            else if (outliner.SelectedNode?.Tag is LevelObject item && item.Type is
            "buttonTrapDoor" or "buttonFan" or "buttonExit" or "buttonLadder" or "buttonPlatform" or "buttonSpikeHazard" or "valveFluidFill" or "platformLoopButton")
                RemoveSelectedLink();
            else viewport.SendKey(eventArgs.KeyCode);
            eventArgs.Handled = true;
        }
    }

    private void SetDirty(bool value, string status)
    {
        if (value && !restoringHistory && !viewportHistoryTransaction &&
            !synchronizingObjectState && !synchronizingTileState)
        {
            editHistory.Record(document.CaptureSnapshot());
            value = editHistory.IsDirty;
        }
        dirty = value;
        statusLabel.Text = status;
        if (value && !Text.EndsWith('*')) Text += " *";
        else if (!value) Text = Text.TrimEnd(' ', '*');
    }

    private bool ConfirmDiscard()
    {
        if (!dirty) return true;
        var result = MessageBox.Show(this, "Save changes to the current level?", "Unsaved Changes",
            MessageBoxButtons.YesNoCancel, MessageBoxIcon.Warning);
        if (result == DialogResult.Cancel) return false;
        if (result == DialogResult.Yes) SaveLevel();
        return true;
    }

    private void OnFormClosing(object? sender, FormClosingEventArgs eventArgs)
    {
        if (scriptBuildCancellation != null) {
            scriptBuildCancellation.Cancel(); eventArgs.Cancel = true;
            statusLabel.Text = "Cancelling script build — close again when it finishes"; return;
        }
        if (!ConfirmDiscard())
        {
            eventArgs.Cancel = true;
            return;
        }
        StopPlaytest();
        playtestTimer.Dispose();
        sheetToolTip.Dispose();
        if (detailsPreviewPath != null)
        {
            try { File.Delete(detailsPreviewPath); }
            catch (IOException) { }
        }
    }

    private static string? FindRepositoryFile(string relativePath)
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        for (var depth = 0; depth < 7 && directory != null; ++depth, directory = directory.Parent)
        {
            var candidate = Path.Combine(directory.FullName, relativePath);
            if (File.Exists(candidate)) return candidate;
        }
        return null;
    }
}

internal sealed class TileSelectionProperties
{
    [Browsable(false)]
    public LevelObject? ScriptTarget { get; init; }

    [Category("Scripts"), Description("Lua or C++ paths for a single selected tile. Lua runs during play; C++ requires compilation.")]
    public string[] Scripts
    {
        get => ScriptTarget?.Scripts ?? [];
        set { if (ScriptTarget != null) ScriptTarget.Scripts = value ?? []; }
    }

    [Category("Selection"), ReadOnly(true)]
    public string Layer { get; init; } = string.Empty;

    [Category("Selection"), ReadOnly(true)]
    public string Position { get; init; } = string.Empty;

    [Category("Selection"), ReadOnly(true)]
    public int SelectedTiles { get; init; }

    [Category("Sprite")]
    public int Column { get; set; }

    [Category("Sprite")]
    public int Row { get; set; }

    [Category("Transform"), Description("Clockwise quarter turns from the source sprite.")]
    public int QuarterTurns { get; set; }

    [Category("Transform")]
    public bool FlipHorizontal { get; set; }

    [Category("Transform")]
    public bool FlipVertical { get; set; }

    [Category("Animation")]
    public int AnimationFrames { get; set; } = 1;

    [Category("Animation")]
    public int FrameMilliseconds { get; set; } = 120;
}
