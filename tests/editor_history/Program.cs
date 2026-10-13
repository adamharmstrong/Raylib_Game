using PowerPulleyPanic.LevelEditor;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}

var fittedExitDocument = new LevelDocument();
fittedExitDocument.RestoreSnapshot("exit 64 690 85 160");
Check(fittedExitDocument.Objects.Single(item => item.Type == "exit").Arguments == "64 690 85 160",
    "Loading a fitted exit must preserve its authored dimensions.");
var fittedExitSnapshot = fittedExitDocument.CaptureSnapshot();
fittedExitDocument.RestoreSnapshot(fittedExitSnapshot);
Check(fittedExitDocument.CaptureSnapshot() == fittedExitSnapshot,
    "Exit dimensions must survive an editor snapshot round trip.");

var document = new LevelDocument();
var scriptDocument = new LevelDocument();
scriptDocument.AddEditableRecord("stoneBlock", "10 20 32 32 1");
scriptDocument.AddEditableRecord("stoneBlock", "80 20 32 32 1");
scriptDocument.UpsertVisualTile("foreground", 0, 0, 32, 64);
scriptDocument.Objects.First(item => item.Type == "stoneBlock").Scripts = ["../scripts/moving platform.cpp", "C:/scripts/日本語.cpp"];
scriptDocument.Objects.Single(item => item.Type == "visualTile").Scripts = ["../scripts/tile.cpp"];
var scriptSnapshot = scriptDocument.CaptureSnapshot();
scriptDocument.RestoreSnapshot(scriptSnapshot);
Check(scriptDocument.CaptureSnapshot() == scriptSnapshot, "Multiple script paths, spaces and Unicode must round-trip.");
var scriptedBlock = scriptDocument.Objects.First(item => item.Type == "stoneBlock");
var scriptAdapter = new ObjectDetailsAdapter(scriptedBlock);
var scriptProperty = scriptAdapter.GetProperties()["Scripts"]!;
scriptProperty.SetValue(scriptAdapter, new[] { "scripts/replaced.cpp" });
Check(scriptedBlock.Scripts.Single() == "scripts/replaced.cpp", "Details must edit the selected instance's scripts.");
var scriptHistory = new EditorHistory();
scriptHistory.Reset(scriptSnapshot);
scriptHistory.Record(scriptDocument.CaptureSnapshot());
scriptDocument.RestoreSnapshot(scriptHistory.Undo()!);
Check(scriptDocument.CaptureSnapshot() == scriptSnapshot, "Undo must restore attachment edits.");
scriptDocument.ReplaceEditableObjects(new Dictionary<string, List<string>> { ["stoneBlock"] = ["80 20 32 32 1"] });
Check(scriptDocument.Objects.Single(item => item.Type == "stoneBlock").Scripts.Length == 0,
    "Deleting a scripted object must not attach its scripts to a surviving neighbor.");
scriptDocument.RestoreSnapshot(scriptSnapshot);
scriptDocument.ReplaceEditableObjects(new Dictionary<string, List<string>> { ["stoneBlock"] = ["10 20 32 32 1"] });
Check(scriptDocument.Objects.Single(item => item.Type == "stoneBlock").Scripts.Length == 2,
    "Deleting a neighbor must preserve the surviving object's scripts.");
Console.WriteLine("Script attachment tests passed: details, path round-trip, undo and deletion.");
var luaDocument = new LevelDocument();
luaDocument.UpsertVisualTile("foreground", 0, 0, 32, 64);
luaDocument.Objects.Single(item => item.Type == "visualTile").Scripts = ["../scripts/bounce.lua", "../scripts/native.cpp"];
var luaLevelPath = Path.Combine(Path.GetTempPath(), "lua-source-level.level");
var luaPlaytestPath = Path.Combine(Path.GetTempPath(), "lua-playtest-snapshot.level");
luaDocument.Save(luaLevelPath);
var beforePlaytest = luaDocument.CaptureSnapshot();
luaDocument.WriteSnapshot(luaPlaytestPath, resolveScriptPaths: true);
var playtestDocument = new LevelDocument(); playtestDocument.Load(luaPlaytestPath);
Check(playtestDocument.Objects.Single(item => item.Type == "visualTile").Scripts.All(Path.IsPathFullyQualified),
    "Temporary playtests must use resolved source paths for both languages.");
Check(luaDocument.CaptureSnapshot() == beforePlaytest, "Playtest export must not mutate attachment history or saved paths.");
luaDocument.Load(luaLevelPath);
Check(luaDocument.Objects.Single(item => item.Type == "visualTile").Scripts.SequenceEqual(new[] { "../scripts/bounce.lua", "../scripts/native.cpp" }),
    "Lua and C++ attachments must coexist across save and reopen.");
File.Delete(luaLevelPath); File.Delete(luaPlaytestPath);
Console.WriteLine("Lua attachment checks passed: mixed sources, save/reopen and temporary playtest resolution.");
var layerDocument = new LevelDocument();
layerDocument.AddTileLayer("user1");
layerDocument.AddTileLayer("user3");
layerDocument.UpsertVisualTile("user1", 2, 1, 32, 64);
layerDocument.RenameTileLayer("user1", "Machinery Front");
Check(layerDocument.TileLayerLabel("user1") == "Machinery Front", "Custom layer names must support spaces.");
Check(layerDocument.MoveTileLayer("user3", -1) && layerDocument.CustomTileLayers.SequenceEqual(new[] { "user3", "user1" }),
    "Custom layers must reorder independently of their stable tile IDs.");
Check(!layerDocument.MoveTileLayer("collision", 1) && layerDocument.MoveTileLayer("user3", -1) && layerDocument.IsLayerAboveDefaults("user3"),
    "Custom layers must jump over the default block without moving default layers.");
var rejectedDefaultRename = false;
try { layerDocument.RenameTileLayer("foreground", "Changed"); } catch (ArgumentException) { rejectedDefaultRename = true; }
Check(rejectedDefaultRename, "Default layer names must be immutable.");
var layerSnapshot = layerDocument.CaptureSnapshot();
layerDocument.RestoreSnapshot(layerSnapshot);
Check(layerDocument.TileLayerLabel("user1") == "Machinery Front" && layerDocument.CustomTileLayers.SequenceEqual(new[] { "user3", "user1" }) && layerDocument.IsLayerAboveDefaults("user3"),
    "Layer names and order must survive save/reopen and history snapshots.");
Check(layerDocument.Objects.Single(item => item.Type == "visualTile").Arguments.StartsWith("user1 "),
    "Renaming and reordering must leave painted tile references intact.");
layerDocument.RemoveTileLayer("user1");
Check(!layerDocument.Objects.Any(item => item.Type == "tileLayerName"), "Deleting a layer must remove its display name.");
var sheetDocument = new LevelDocument();
var sheetPath = Path.Combine(Path.GetTempPath(), "sprite sheets", "custom artwork.png");
var sheetId = sheetDocument.AddSpriteSheet(sheetPath);
Check(sheetId == 3 && sheetDocument.AddSpriteSheet(sheetPath) == sheetId, "Custom sheet IDs must be stable and deduplicate paths.");
sheetDocument.UpsertVisualTile("foreground", 2, 1, 32, 64, sheetIndex: sheetId);
var sheetSnapshot = sheetDocument.CaptureSnapshot();
sheetDocument.RestoreSnapshot(sheetSnapshot);
Check(sheetDocument.SpriteSheets[sheetId] == Path.GetFullPath(sheetPath).Replace('\\', '/'),
    "Custom image paths with spaces must survive save and reopen.");
Check(sheetDocument.CaptureSnapshot() == sheetSnapshot, "Custom artwork and tile references must survive history snapshots.");
var outlinerDocument = new LevelDocument();
outlinerDocument.RestoreSnapshot("script power_pulley_panic\nbounds 0 0 1600 900\nsolid 32 64 96 32\nvisualTile foreground 1 2 32 64 0 0 0 1 0.12 -1");
var machinery = outlinerDocument.AddOutlinerCategory("Machinery", null);
var driveTrain = outlinerDocument.AddOutlinerCategory("Drive Train", machinery.Id);
var solid = outlinerDocument.Objects.Single(item => item.Type == "solid");
var visualTile = outlinerDocument.Objects.Single(item => item.Type == "visualTile");
outlinerDocument.SetOutlinerCategory(solid, driveTrain.Id);
outlinerDocument.SetOutlinerName(solid, "West counterweight");
outlinerDocument.SetOutlinerName(visualTile, "Cracked wall tile");
var outlinerSnapshot = outlinerDocument.CaptureSnapshot();
outlinerDocument.RestoreSnapshot(outlinerSnapshot);
solid = outlinerDocument.Objects.Single(item => item.Type == "solid");
visualTile = outlinerDocument.Objects.Single(item => item.Type == "visualTile");
Check(outlinerDocument.OutlinerCategories.Count == 2 &&
    outlinerDocument.OutlinerCategories.Single(item => item.Id == driveTrain.Id).ParentId == machinery.Id,
    "Nested outliner categories must survive save/reopen and history snapshots.");
Check(outlinerDocument.OutlinerCategoryId(solid) == driveTrain.Id && outlinerDocument.OutlinerName(solid) == "West counterweight",
    "Object grouping and custom names must survive save/reopen.");
Check(outlinerDocument.OutlinerName(visualTile) == "Cracked wall tile", "Visual tiles must support persistent custom names.");
outlinerDocument.MoveOutlinerCategory(driveTrain.Id, null);
Check(outlinerDocument.OutlinerCategories.Single(item => item.Id == driveTrain.Id).ParentId == null,
    "Categories must move out of parent categories.");
outlinerDocument.DeleteOutlinerCategory(driveTrain.Id);
Check(outlinerDocument.OutlinerCategoryId(solid) == null && outlinerDocument.OutlinerName(solid) == "West counterweight",
    "Deleting a category must return its objects to Objects without discarding their names.");
Console.WriteLine("World outliner tests passed: nested categories, grouping, custom object and tile names, persistence, deletion.");
document.RestoreSnapshot("# Keep this comment\nscript power_pulley_panic\nbounds 0 0 1600 900\nplayerStart 80 600\ncustomRecord preserved\ntileLayer collision");
var history = new EditorHistory();
var original = document.CaptureSnapshot();
history.Reset(original);
document.Objects.Single(item => item.Type == "playerStart").Arguments = "160 600";
var propertyEdit = document.CaptureSnapshot();
history.Record(propertyEdit);
document.UpsertVisualTile("foreground", 1, 2, 32, 64);
document.UpsertVisualTile("foreground", 1, 2, 64, 64);
var paintStroke = document.CaptureSnapshot();
history.Record(paintStroke);
document.AddSolid(0, 800, 1600, 32);
var objectEdit = document.CaptureSnapshot();
history.Record(objectEdit);
Check(history.Undo() == paintStroke, "Object edits must undo before earlier painting.");
Check(history.Undo() == propertyEdit, "An entire painting stroke must undo together.");
Check(history.Undo() == original, "Earlier property edits must remain undoable.");
Check(!history.IsDirty && history.Undo() == null, "Undo to the saved state must clear dirty and stop at the beginning.");
Check(history.Redo() == propertyEdit && history.Redo() == paintStroke && history.Redo() == objectEdit,
    "Redo must follow chronological order.");
history.MarkSaved(objectEdit);
Check(!history.IsDirty, "Saving must mark the current state clean.");
Check(history.Undo() == paintStroke && history.IsDirty, "Saving must preserve previous history.");
history.Record(paintStroke);
Check(history.Redo() == objectEdit, "Recording a no-op must preserve redo.");
history.Undo();
history.Record(propertyEdit);
Check(history.Redo() == null, "A new edit after undo must discard the redo branch.");
document.RestoreSnapshot(original);
Check(document.CaptureSnapshot() == original, "Snapshots must preserve comments and unknown records.");
history.Reset(original);
Check(history.Undo() == null && history.Redo() == null && !history.IsDirty, "Opening a new level must reset history.");
for (var i = 0; i < 150; i++) history.Record(i.ToString());
var undoCount = 0;
while (history.Undo() != null) undoCount++;
Check(undoCount == 100, "History must retain the most recent 100 edits.");
Console.WriteLine("Editor history tests passed: mixed edits, stroke grouping, save state, branching, snapshots, reset, limit.");

var anchorDocument = new LevelDocument();
anchorDocument.RestoreSnapshot("# Anchor history test\nscript power_pulley_panic\nbounds 0 0 1600 900\nplayerStart 80 600\nwater 320 192 160 128 8 0.65 1");
var anchorHistory = new EditorHistory();
var defaultAnchorState = anchorDocument.CaptureSnapshot();
anchorHistory.Reset(defaultAnchorState);
var water = anchorDocument.Objects.Single(item => item.Type == "water");
anchorDocument.SetAnchorPoint(water, 8);
var changedAnchorState = anchorDocument.CaptureSnapshot();
anchorHistory.Record(changedAnchorState);
Check(anchorHistory.Undo() == defaultAnchorState, "Undo must restore the prior transform anchor state.");
anchorDocument.RestoreSnapshot(defaultAnchorState);
Check(anchorDocument.GetAnchorPoint(anchorDocument.Objects.Single(item => item.Type == "water")) == 4,
    "Undo must restore the default center anchor.");
Check(anchorHistory.Redo() == changedAnchorState, "Redo must restore the selected transform anchor state.");
anchorDocument.RestoreSnapshot(changedAnchorState);
Check(anchorDocument.GetAnchorPoint(anchorDocument.Objects.Single(item => item.Type == "water")) == 8,
    "Redo must restore the selected bottom-right anchor.");
Console.WriteLine("Transform anchor history tests passed: anchor metadata undo and redo.");

var playerStart = new LevelObject("playerStart", 1, "80 600 0");
var playerDetails = new ObjectDetailsAdapter(playerStart);
Check((string)playerDetails.GetValue(2) == "Character 1", "Player Start must display its configured character name.");
playerDetails.SetValue(2, "Character 3");
Check(playerStart.Arguments == "80 600 2", "Player character selection must persist as the expected character index.");
var playerDocument = new LevelDocument();
playerDocument.RestoreSnapshot($"playerStart {playerStart.Arguments}\ntileLayer collision");
Check(playerDocument.CaptureSnapshot().Contains("playerStart 80 600 2"),
    "Player character selection must survive document snapshots.");
Console.WriteLine("Player character detail tests passed: choice labels and persisted character indices.");

foreach (var (type, arguments, channelIndex) in new[] {
    ("battery", "0 0 32 32 1 7", 5),
    ("generator", "0 0 32 32 180 1 0.85 7", 7),
    ("electricMotor", "0 0 24 120 7", 4),
    ("limitSwitch", "0 0 32 32 7", 4),
    ("beamSensor", "0 0 1 0 100 7", 5),
    ("hydraulicCylinder", "0 0 32 32 1 0 100 2 7", 8)
})
{
    var item = new LevelObject(type, 1, arguments);
    var adapter = new ObjectDetailsAdapter(item);
    Check((int)adapter.GetValue(channelIndex) == 7, $"{type} must expose the authored channel.");
    adapter.SetValue(channelIndex, 9);
    var tokens = item.Arguments.Split(' ');
    Check(tokens[channelIndex] == "9", $"{type} must write the channel in the correct record position.");
    Check(tokens.Length == arguments.Split(' ').Length, $"{type} channel edit must preserve the other field positions.");
}
Console.WriteLine("Connection property schema tests passed.");

var placementTypes = new[] {
    "cam", "conveyorBelt", "turntable", "battery", "electricMotor", "generator",
    "limitSwitch", "relay", "fuse", "magnet", "piston", "hydraulicCylinder", "rocketThruster",
    "crushingBlock", "swingingHammer", "sawBlade", "spinnerTrap", "steamVent", "electricalArc",
    "oil", "mud", "speedSensor", "beamSensor", "key", "gasMask", "breakableCrate", "explosiveBarrel", "tnt",
    "waterPit", "toxinLeak", "clockFace", "spikeHazard"
};
document.RestoreSnapshot("# Preserve this comment\ncustomRecord preserved");
history.Reset(document.CaptureSnapshot());
var placedRecords = placementTypes.ToDictionary(type => type, _ => new List<string> { "320 192 48 48" },
    StringComparer.OrdinalIgnoreCase);
document.ReplaceEditableObjects(placedRecords);
var placementsSnapshot = document.CaptureSnapshot();
history.Record(placementsSnapshot);
foreach (var type in placementTypes)
    Check(document.Objects.Count(item => item.Type == type) == 1, $"{type} must synchronize into the document.");
document.ReplaceEditableObjects(placedRecords);
Check(document.CaptureSnapshot() == placementsSnapshot, "Repeated synchronization must not duplicate placements.");
document.ReplaceEditableObjects(new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase));
Check(!document.Objects.Any(item => placementTypes.Contains(item.Type)), "Deleting placements must remove their saved records.");
history.Record(document.CaptureSnapshot());
document.RestoreSnapshot(history.Undo()!);
Check(document.CaptureSnapshot() == placementsSnapshot, "Undo must restore all added object records.");
document.RestoreSnapshot(history.Redo()!);
Check(document.CaptureSnapshot().Contains("customRecord preserved"), "Object synchronization must preserve unrelated records.");
Console.WriteLine("Placement document tests passed: synchronization, duplicate prevention, deletion, undo/redo, preservation.");

foreach (var (targetType, targetArgs, expectedType, expectedArgs) in new[] {
    ("trapDoor", "200 300 80 16 0", "buttonTrapDoor", "0 0 75 120"),
    ("fan", "200 300 1 0 160 40 300 1", "buttonFan", "0 0 1"),
    ("exit", "200 300 32 64", "buttonExit", "0"),
    ("ladder", "200 300 32 160", "buttonLadder", "0 200 300 32 160"),
    ("platform", "200 300 80 16", "buttonPlatform", "0 200 300 80 16"),
    ("directionalSpikeHazard", "left 200 300 80 16", "buttonSpikeHazard", "0 left 200 300 80 16"),
    ("spikeHazard", "200 300 80 16", "buttonSpikeHazard", "0 up 200 300 80 16"),
    ("water", "200 300 160 160 8 0.5 1", "valveFluidFill", "0 1 86")
}) {
    var sourceType = expectedType == "valveFluidFill" ? "valve" : "button";
    document.RestoreSnapshot($"# Keep\n{sourceType} 64 64 32 16\n{targetType} {targetArgs}");
    var beforeLink = document.CaptureSnapshot();
    history.Reset(beforeLink);
    var source = document.Objects.Single(item => item.Type == sourceType);
    var target = document.Objects.Single(item => item.Type == targetType);
    Check(document.ConnectionTargets(source).Contains(target), $"{targetType} must be offered as a target.");
    document.ConnectObjects(source, target);
    var linked = document.CaptureSnapshot();
    history.Record(linked);
    var record = document.Objects.Single(item => item.Type == expectedType);
    Check(record.Arguments == expectedArgs, $"{targetType} must use the runtime record format.");
    if (targetType is "ladder" or "platform" or "spikeHazard" or "directionalSpikeHazard")
        Check(!document.Objects.Contains(target), "Controlled geometry must replace the ordinary target.");
    else {
        document.ConnectObjects(source, target);
        Check(document.CaptureSnapshot() == linked, "Creating the same link twice must be a no-op.");
    }
    document.RestoreSnapshot(history.Undo()!);
    Check(document.CaptureSnapshot() == beforeLink, "Undo must restore the unlinked geometry and records.");
    document.RestoreSnapshot(history.Redo()!);
    Check(document.CaptureSnapshot() == linked, "Redo must restore the connection.");
    document.RemoveConnection(document.Objects.Single(item => item.Type == expectedType));
    Check(!document.Objects.Any(item => item.Type == expectedType), "Removing a connection must remove its record.");
    if (targetType is "ladder" or "platform" or "directionalSpikeHazard")
        Check(document.Objects.Any(item => item.Type == targetType && item.Arguments == targetArgs), "Unlink must restore ordinary geometry.");
}
Console.WriteLine("Connection authoring tests passed: all target kinds, runtime formats, geometry conversion, unlink, duplicate prevention, undo/redo.");

document.RestoreSnapshot("# loop test\nbutton 64 64 32 16\nbuttonExit 0");
history.Reset(document.CaptureSnapshot());
var platformLoop = document.AddPlatformLoop(480, 360, 120, 200, 80, 28, 28, 4);
Check(platformLoop.Arguments == "-1 480 360 120 200 80 28 28 4", "New loops must run continuously with authored settings.");
var loopButton = document.Objects.Single(item => item.Type == "button");
document.ConnectObjects(loopButton, platformLoop);
Check(platformLoop.Arguments.StartsWith("0 "), "Linking a button must set the loop's activator.");
var rider = document.AttachLoopButton(platformLoop, 2, loopButton);
Check(rider.Arguments == "0 2 32 16 0", "Riders must reuse existing buttons through an explicit index.");
document.AttachLoopButton(platformLoop, 1, loopButton);
Check(document.Objects.Count(item => item.Type == "button") == 1 && document.Objects.Count(item => item.Type == "platformLoopButton") == 1,
    "Reattaching a button must move its riding link without creating duplicates.");
document.AttachLoopButton(platformLoop, 3);
Check(document.Objects.Count(item => item.Type == "button") == 2, "Adding a new rider must create exactly one button.");
var linkedLoops = document.CaptureSnapshot(); history.Record(linkedLoops);
document.RemoveConnection(platformLoop);
Check(platformLoop.Arguments.StartsWith("-1 "), "Unlinking an activator must keep the loop running independently.");
var secondLoop = document.AddPlatformLoop(800, 360, 100, 150, 80, 28, 20, 3);
document.AttachLoopButton(secondLoop, 0);
document.DeletePlatformLoop(platformLoop);
Check(document.Objects.Count(item => item.Type == "buttonPlatformLoop") == 1 &&
    document.Objects.Single(item => item.Type == "platformLoopButton").Arguments.StartsWith("0 "),
    "Deleting a loop must detach its riders and remap later loop references.");
Check(document.Objects.Count(item => item.Type == "button") == 3 && document.Objects.Any(item => item.Type == "buttonExit"),
    "Deleting loops must preserve buttons and their other connections.");
history.Record(document.CaptureSnapshot());
document.RestoreSnapshot(history.Undo()!);
Check(document.CaptureSnapshot() == linkedLoops, "Undo must restore loop and riding-button records together.");
try { document.AttachLoopButton(document.Objects.Single(item => item.Type == "buttonPlatformLoop"), 99); throw new Exception("Invalid platform accepted."); }
catch (InvalidOperationException) { }
Console.WriteLine("Loop authoring tests passed: create, activate, attach, reattach, unlink, delete/remap, preserve controls, undo, validation.");

document.RestoreSnapshot("button 64 64 32 16\nbuttonPlatformLoop -1 480 360 120 200 80 28 28 4\nplatformLoopButton 0 0 24 12\nbutton 100 100 32 16\nbuttonExit 1");
var legacyRider = document.Objects.Single(item => item.Type == "platformLoopButton");
document.RemoveConnection(legacyRider);
Check(document.Objects.Count(item => item.Type == "button") == 3, "Detaching a legacy rider must preserve its implicit button.");
Check(document.Objects.Single(item => item.Type == "button" && item.Index == 2).Arguments == "588 338 24 12",
    "Detached riders must stay at the runtime's capsule-path position.");
Check(document.Objects.Single(item => item.Type == "buttonExit").Arguments == "1",
    "Legacy normalization must preserve runtime indices of controlled buttons.");
Console.WriteLine("Legacy riding-button detach and position-preservation tests passed.");

document.RestoreSnapshot("visualTile foreground 2 1 32 64 0 0 0 1 0.12 3");
var beforePassage = document.CaptureSnapshot();
history.Reset(beforePassage);
document.SetTilePassage(3, 2, 1, 1);
history.Record(document.CaptureSnapshot());
Check(document.GetTilePassage(3, 2, 1) == 1, "Directional passage must be stored by sheet and tile.");
var passageSnapshot = document.CaptureSnapshot();
document.RestoreSnapshot(passageSnapshot);
Check(document.GetTilePassage(3, 2, 1) == 1, "Passage must survive save/load.");
document.RestoreSnapshot(history.Undo()!);
Check(document.GetTilePassage(3, 2, 1) == null, "Undo must remove the passage change.");
document.RestoreSnapshot(history.Redo()!);
document.SetTilePassage(3, 2, 1, 0);
Check(document.GetTilePassage(3, 2, 1) == 0 && document.Objects.Count(item => item.Type == "tilePassage") == 1,
    "Passable override must replace rather than duplicate the setting.");
document.SetTilePassage(3, 2, 1, null);
Check(document.GetTilePassage(3, 2, 1) == null, "Reset must restore the layer default.");
Console.WriteLine("Tile passage persistence and history checks passed.");

var audioDocument = new LevelDocument();
audioDocument.RestoreSnapshot("music \"audio/my music.ogg\"\nstoneBlock 10 20 32 32 1\nstoneBlock 80 20 32 32 1\nobjectSound stoneBlock 2 \"audio/hit sound.wav\"");
Check(audioDocument.Objects.Last(item => item.Type == "stoneBlock").Sound == "audio/hit sound.wav", "Sound paths with spaces must load.");
var audioSnapshot = audioDocument.CaptureSnapshot();
audioDocument.RestoreSnapshot(audioSnapshot);
Check(audioDocument.CaptureSnapshot() == audioSnapshot, "Audio must survive undo and save snapshots.");
audioDocument.ReplaceEditableObjects(new Dictionary<string, List<string>> { ["stoneBlock"] = ["80 20 32 32 1"] });
Check(audioDocument.Objects.Single(item => item.Type == "stoneBlock").Sound == "audio/hit sound.wav", "Sound must follow surviving objects after deletion.");

var audioFolder = Path.Combine(Path.GetTempPath(), "level-audio-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(audioFolder);
try {
    var levelPath = Path.Combine(audioFolder, "source.level");
    audioDocument.Save(levelPath);
    var beforePreview = audioDocument.CaptureSnapshot();
    var previewPath = Path.Combine(audioFolder, "preview.level");
    audioDocument.WriteSnapshot(previewPath, resolveScriptPaths: true);
    var preview = File.ReadAllText(previewPath);
    Check(preview.Contains(Path.Combine(audioFolder, "audio", "my music.ogg").Replace('\\', '/')),
        "Temporary playtests must resolve music against the authored level.");
    Check(preview.Contains(Path.Combine(audioFolder, "audio", "hit sound.wav").Replace('\\', '/')),
        "Temporary playtests must resolve sound effects against the authored level.");
    Check(audioDocument.CaptureSnapshot() == beforePreview, "Playtest path resolution must not edit the document.");
    var movedFolder = Path.Combine(audioFolder, "other"); Directory.CreateDirectory(movedFolder);
    audioDocument.Save(Path.Combine(movedFolder, "copy.level"));
    Check(audioDocument.Objects.Single(item => item.Type == "stoneBlock").Sound == "../audio/hit sound.wav",
        "Save As must retain the original audio reference.");
} finally { Directory.Delete(audioFolder, recursive: true); }
Console.WriteLine("Audio tests passed: attachment round trip, deletion, playtest paths and Save As.");
