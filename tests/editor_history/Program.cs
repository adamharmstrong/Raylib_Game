using PowerPulleyPanic.LevelEditor;

static void Check(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}

var document = new LevelDocument();
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
