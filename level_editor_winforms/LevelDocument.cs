using System.ComponentModel;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class LevelDocument
{
    private const float StandardExitDoorWidth = 85.0f;
    private const float StandardExitDoorHeight = 210.0f;
    private readonly List<LineRecord> lines = [];

    internal LevelDocument()
    {
        lines.Add(new LineRecord("# New Power Pulley Panic level.", null));
        lines.Add(new LineRecord("script power_pulley_panic", new LevelObject("script", 1, "power_pulley_panic")));
        lines.Add(new LineRecord("bounds 0 0 1600 900", new LevelObject("bounds", 1, "0 0 1600 900")));
        lines.Add(new LineRecord("playerStart 80 600", new LevelObject("playerStart", 1, "80 600")));
        EnsureCollisionLayer();
    }

    internal string? FilePath { get; private set; }
    internal IReadOnlyList<LevelObject> Objects => lines.Where(line => line.Object != null).Select(line => line.Object!).ToList();

    internal void Load(string path)
    {
        FilePath = path;
        RestoreSnapshot(string.Join('\n', File.ReadAllLines(path)));
    }

    internal string CaptureSnapshot() => string.Join('\n', lines.Select(line => line.Object == null
        ? line.OriginalText
        : string.IsNullOrWhiteSpace(line.Object.Arguments) ? line.Object.Type : $"{line.Object.Type} {line.Object.Arguments}"));

    internal void RestoreSnapshot(string snapshot)
    {
        lines.Clear();
        var counts = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var text in snapshot.Split('\n'))
        {
            var trimmed = text.Trim();
            if (trimmed.Length == 0 || trimmed.StartsWith('#'))
            {
                lines.Add(new LineRecord(text, null));
                continue;
            }

            var separator = trimmed.IndexOfAny([' ', '\t']);
            var type = separator < 0 ? trimmed : trimmed[..separator];
            var arguments = separator < 0 ? string.Empty : trimmed[(separator + 1)..].Trim();
            if (type.Equals("exit", StringComparison.OrdinalIgnoreCase))
                arguments = NormalizeExitArguments(arguments);
            counts.TryGetValue(type, out var count);
            counts[type] = ++count;
            lines.Add(new LineRecord(text, new LevelObject(type, count, arguments)));
        }
        EnsureCollisionLayer();
    }

    internal IReadOnlyList<string> CustomTileLayers => Objects
        .Where(item => item.Type.Equals("tileLayer", StringComparison.OrdinalIgnoreCase))
        .Select(item => item.Arguments.Trim())
        .Where(name => name.StartsWith("user", StringComparison.OrdinalIgnoreCase))
        .Distinct(StringComparer.OrdinalIgnoreCase)
        .ToList();

    internal void EnsureCollisionLayer()
    {
        if (Objects.Any(item => item.Type.Equals("tileLayer", StringComparison.OrdinalIgnoreCase) &&
            item.Arguments.Trim().Equals("collision", StringComparison.OrdinalIgnoreCase))) return;
        lines.Add(new LineRecord("tileLayer collision", new LevelObject("tileLayer", 1, "collision")));
    }

    internal void AddTileLayer(string name)
    {
        if (Objects.Any(item => item.Type.Equals("tileLayer", StringComparison.OrdinalIgnoreCase) &&
            item.Arguments.Trim().Equals(name, StringComparison.OrdinalIgnoreCase))) return;
        lines.Add(new LineRecord($"tileLayer {name}", new LevelObject("tileLayer", 1, name)));
    }

    internal void RemoveTileLayer(string name)
    {
        if (name.Equals("collision", StringComparison.OrdinalIgnoreCase)) return;
        lines.RemoveAll(line => line.Object?.Type.Equals("tileLayer", StringComparison.OrdinalIgnoreCase) == true &&
            line.Object.Arguments.Trim().Equals(name, StringComparison.OrdinalIgnoreCase));
        lines.RemoveAll(line =>
        {
            var item = line.Object;
            if (item?.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) == true)
                return item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault()
                    ?.Equals(name, StringComparison.OrdinalIgnoreCase) == true;
            if (item?.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase) == true)
                return item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault()
                    ?.Equals(name, StringComparison.OrdinalIgnoreCase) == true;
            return false;
        });
    }

    private static string NormalizeExitArguments(string arguments)
    {
        var values = arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (values.Length < 2 ||
            !float.TryParse(values[0], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var x) ||
            !float.TryParse(values[1], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var y)) return arguments;
        return FormattableString.Invariant($"{x:0.###} {y:0.###} {StandardExitDoorWidth:0.###} {StandardExitDoorHeight:0.###}");
    }

    internal void Save(string? path = null)
    {
        FilePath = path ?? FilePath ?? throw new InvalidOperationException("The level has no file path.");
        WriteSnapshot(FilePath);
    }

    internal void WriteSnapshot(string path)
    {
        File.WriteAllLines(path, lines.Select(line => line.Object == null
            ? line.OriginalText
            : string.IsNullOrWhiteSpace(line.Object.Arguments) ? line.Object.Type : $"{line.Object.Type} {line.Object.Arguments}"));
    }

    internal void UpsertVisualTile(string layer, int column, int row, int x, int y,
        int quarterTurns = 0, bool flipX = false, bool flipY = false,
        int animationFrames = 1, float animationFrameSeconds = 0.12f, int sheetIndex = -1)
    {
        foreach (var line in lines)
        {
            if (line.Object?.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) != true) continue;
            var values = line.Object.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (values.Length >= 5 && values[0].Equals(layer, StringComparison.OrdinalIgnoreCase) &&
                int.TryParse(values[3], out var tileX) && int.TryParse(values[4], out var tileY) &&
                tileX == x && tileY == y)
            {
                line.Object.Arguments = VisualTileArguments(layer, column, row, x, y, quarterTurns, flipX, flipY,
                    animationFrames, animationFrameSeconds, sheetIndex);
                return;
            }
        }

        // Painting or transforming one cell inside a compact rectangle expands that rectangle
        // so the edited cell can carry independent sprite and transform data.
        RemoveVisualTile(layer, x, y);
        var index = Objects.Count(item => item.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase)) + 1;
        lines.Add(new LineRecord(string.Empty, new LevelObject("visualTile", index,
            VisualTileArguments(layer, column, row, x, y, quarterTurns, flipX, flipY,
                animationFrames, animationFrameSeconds, sheetIndex))));
    }

    private static string VisualTileArguments(string layer, int column, int row, int x, int y,
        int quarterTurns, bool flipX, bool flipY, int animationFrames = 1, float animationFrameSeconds = 0.12f,
        int sheetIndex = -1) =>
        FormattableString.Invariant($"{layer} {column} {row} {x} {y} {quarterTurns} {(flipX ? 1 : 0)} {(flipY ? 1 : 0)} {animationFrames} {animationFrameSeconds:0.###} {sheetIndex}");

    internal void RemoveVisualTile(string layer, int x, int y)
    {
        for (var lineIndex = lines.Count - 1; lineIndex >= 0; --lineIndex)
        {
            var item = lines[lineIndex].Object;
            if (item == null) continue;
            var values = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (item.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) && values.Length >= 5 &&
                values[0].Equals(layer, StringComparison.OrdinalIgnoreCase) &&
                int.TryParse(values[3], out var tileX) && int.TryParse(values[4], out var tileY) &&
                tileX == x && tileY == y)
            {
                lines.RemoveAt(lineIndex);
                continue;
            }

            if (!item.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase) || values.Length < 7 ||
                !values[0].Equals(layer, StringComparison.OrdinalIgnoreCase) ||
                !int.TryParse(values[1], out var column) || !int.TryParse(values[2], out var row) ||
                !int.TryParse(values[3], out var startX) || !int.TryParse(values[4], out var startY) ||
                !int.TryParse(values[5], out var columns) || !int.TryParse(values[6], out var rows) ||
                x < startX || y < startY || x >= startX + columns * 32 || y >= startY + rows * 32 ||
                (x - startX) % 32 != 0 || (y - startY) % 32 != 0) continue;

            lines.RemoveAt(lineIndex);
            var insertAt = lineIndex;
            var quarterTurns = values.Length >= 10 && int.TryParse(values[7], out var parsedTurns) ? parsedTurns : 0;
            var flipX = values.Length >= 10 && values[8] == "1";
            var flipY = values.Length >= 10 && values[9] == "1";
            var animationFrames = values.Length >= 12 && int.TryParse(values[10], out var parsedFrames) ? parsedFrames : 1;
            var animationFrameSeconds = values.Length >= 12 && float.TryParse(values[11],
                System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var parsedSeconds)
                ? parsedSeconds : 0.12f;
            for (var tileRow = 0; tileRow < rows; ++tileRow)
            {
                for (var tileColumn = 0; tileColumn < columns; ++tileColumn)
                {
                    var positionX = startX + tileColumn * 32;
                    var positionY = startY + tileRow * 32;
                    if (positionX == x && positionY == y) continue;
                    var replacement = new LevelObject("visualTile", 0,
                        VisualTileArguments(layer, column, row, positionX, positionY, quarterTurns, flipX, flipY,
                            animationFrames, animationFrameSeconds, values.Length >= 13 && int.TryParse(values[12], out var sheet)
                                ? sheet : -1));
                    lines.Insert(insertAt++, new LineRecord(string.Empty, replacement));
                }
            }
        }
    }

    internal void UpsertSolidCell(int x, int y)
    {
        foreach (var item in Objects)
        {
            if (!item.Type.Equals("solid", StringComparison.OrdinalIgnoreCase)) continue;
            var values = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (values.Length >= 4 && int.TryParse(values[0], out var solidX) && int.TryParse(values[1], out var solidY) &&
                int.TryParse(values[2], out var width) && int.TryParse(values[3], out var height) &&
                solidX == x && solidY == y && width == 32 && height == 32) return;
        }
        var index = Objects.Count(item => item.Type.Equals("solid", StringComparison.OrdinalIgnoreCase)) + 1;
        lines.Add(new LineRecord(string.Empty, new LevelObject("solid", index, $"{x} {y} 32 32")));
    }

    internal void RemoveSolidCell(int x, int y)
    {
        for (var index = lines.Count - 1; index >= 0; --index)
        {
            var item = lines[index].Object;
            if (item?.Type.Equals("solid", StringComparison.OrdinalIgnoreCase) != true) continue;
            var values = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (values.Length >= 4 && int.TryParse(values[0], out var solidX) && int.TryParse(values[1], out var solidY) &&
                int.TryParse(values[2], out var width) && int.TryParse(values[3], out var height) &&
                solidX == x && solidY == y && width == 32 && height == 32)
                lines.RemoveAt(index);
        }
    }

    internal void ClearTileAndSolidData() => lines.RemoveAll(line => line.Object != null &&
        (line.Object.Type.Equals("visualTile", StringComparison.OrdinalIgnoreCase) ||
         line.Object.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase) ||
         line.Object.Type.Equals("solid", StringComparison.OrdinalIgnoreCase)));

    internal void AddSolid(float x, float y, float width, float height)
    {
        var index = Objects.Count(item => item.Type.Equals("solid", StringComparison.OrdinalIgnoreCase)) + 1;
        var arguments = FormattableString.Invariant($"{x:0.###} {y:0.###} {width:0.###} {height:0.###}");
        lines.Add(new LineRecord(string.Empty, new LevelObject("solid", index, arguments)));
    }

    internal void ClearEditableObjects()
    {
        var editableTypes = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "playerStart", "exit", "solid", "platform", "ladder", "cameraZone", "darkness",
            "water", "sand", "gel", "gas", "breakableTile", "enemy"
        };
        lines.RemoveAll(line => line.Object != null && editableTypes.Contains(line.Object.Type));
    }

    internal void AddEditableObject(string type, float x, float y, float width, float height)
    {
        var index = Objects.Count(item => item.Type.Equals(type, StringComparison.OrdinalIgnoreCase)) + 1;
        var arguments = type.Equals("playerStart", StringComparison.OrdinalIgnoreCase)
            ? FormattableString.Invariant($"{x:0.###} {y:0.###}")
            : FormattableString.Invariant($"{x:0.###} {y:0.###} {width:0.###} {height:0.###}");
        lines.Add(new LineRecord(string.Empty, new LevelObject(type, index, arguments)));
    }

    internal void AddFluid(string type, float x, float y, float width, float height,
        float particleSpacing, float initialFill, float flowSpeed)
    {
        var index = Objects.Count(item => item.Type.Equals(type, StringComparison.OrdinalIgnoreCase)) + 1;
        var arguments = FormattableString.Invariant(
            $"{x:0.###} {y:0.###} {width:0.###} {height:0.###} {particleSpacing:0.###} {initialFill:0.###} {flowSpeed:0.###}");
        lines.Add(new LineRecord(string.Empty, new LevelObject(type, index, arguments)));
    }

    internal void AddEditableRecord(string type, string arguments)
    {
        var index = Objects.Count(item => item.Type.Equals(type, StringComparison.OrdinalIgnoreCase)) + 1;
        lines.Add(new LineRecord(string.Empty, new LevelObject(type, index, arguments)));
    }

    internal void ReplaceEditableObjects(IReadOnlyDictionary<string, List<string>> records)
    {
        var synchronizedTypes = new[]
        {
            "playerStart", "exit", "solid", "platform", "ladder", "cameraZone", "darkness",
            "visualTile", "visualTileRect", "water", "sand", "gel", "gas", "pulley", "weight", "rotaryLatch",
            "stoneBlock", "boulder", "physicsWheel", "gear", "flywheel",
            "steeringWheel", "screw", "fan", "pinwheel", "ramp", "seeSaw", "trapDoor", "chain",
            "physicsRope", "button", "portalPair", "directionalSpikeHazard", "arrowTrap",
            "breakableTile", "enemy", "labelSized", "valve", "waterPit", "spikeHazard", "clockFace",
            "checkpoint", "collectible",
            "ball", "barrel", "movingPlatform", "elevator", "pendulumBob", "oneWayPlatform",
            "ceilingHook", "guideRail", "spring", "compressionSpring", "extensionSpring", "torsionSpring",
            "garterSpring", "voluteSpring", "spiralSpring", "constantForceSpring", "constantTorqueSpring",
            "leafSpring", "beamSpring", "discSpring", "waveSpring", "waveWasher", "torsionBar", "ringSpring",
            "elastomerSpring", "pneumaticSpring", "gasSpring", "hydropneumaticSpring",
            "magneticSpring", "compositeSpring", "rod", "fixedJoint", "crank", "ratchet", "clutch", "brake"
        };
        ReplaceLabels(records);
        foreach (var type in synchronizedTypes)
        {
            var replacements = records.TryGetValue(type, out var values) ? values : [];
            var existing = lines.Select((line, index) => (line, index))
                .Where(entry => entry.line.Object?.Type.Equals(type, StringComparison.OrdinalIgnoreCase) == true)
                .ToList();
            if (type == "visualTile")
            {
                var rectangles = lines.Select((line, index) => (line, index))
                    .Where(entry => entry.line.Object?.Type.Equals("visualTileRect", StringComparison.OrdinalIgnoreCase) == true)
                    .Select(entry => entry.index).OrderDescending().ToList();
                foreach (var index in rectangles) lines.RemoveAt(index);
                existing = lines.Select((line, index) => (line, index))
                    .Where(entry => entry.line.Object?.Type.Equals(type, StringComparison.OrdinalIgnoreCase) == true)
                    .ToList();
            }
            var sharedCount = Math.Min(existing.Count, replacements.Count);
            for (var index = 0; index < sharedCount; ++index)
                existing[index].line.Object!.Arguments = replacements[index];
            for (var index = existing.Count - 1; index >= replacements.Count; --index)
                lines.RemoveAt(existing[index].index);
            if (replacements.Count <= existing.Count) continue;
            if (type is "stoneBlock" or "boulder" or "physicsWheel" or "gear" or "flywheel" or "screw")
                lines.Add(new LineRecord("layer middleground", null));
            for (var index = existing.Count; index < replacements.Count; ++index)
                lines.Add(new LineRecord(string.Empty, new LevelObject(type, index + 1, replacements[index])));
        }
    }

    private void ReplaceLabels(IReadOnlyDictionary<string, List<string>> records)
    {
        if (!records.TryGetValue("labelSized", out var replacements)) return;
        var existing = lines.Select((line, index) => (line, index))
            .Where(entry => entry.line.Object?.Type is "label" or "labelSized")
            .ToList();
        var shared = Math.Min(existing.Count, replacements.Count);
        for (var index = 0; index < shared; ++index)
        {
            existing[index].line.Object!.Type = "labelSized";
            existing[index].line.Object!.Arguments = replacements[index];
        }
        for (var index = existing.Count - 1; index >= replacements.Count; --index)
            lines.RemoveAt(existing[index].index);
        for (var index = existing.Count; index < replacements.Count; ++index)
            lines.Add(new LineRecord(string.Empty, new LevelObject("labelSized", index + 1, replacements[index])));
        var ordinal = 1;
        foreach (var line in lines.Select(value => value.Object)
            .Where(value => value?.Type.Equals("labelSized", StringComparison.OrdinalIgnoreCase) == true))
            line!.Index = ordinal++;
    }

    private sealed record LineRecord(string OriginalText, LevelObject? Object);
}

internal sealed class LevelObject
{
    internal LevelObject(string type, int index, string arguments)
    {
        Type = type;
        Index = index;
        Arguments = arguments;
    }

    [Category("Object"), ReadOnly(true)]
    public string Type { get; internal set; }

    [Category("Object"), ReadOnly(true)]
    public int Index { get; internal set; }

    [Category("Level Data"), Description("Space-separated values stored in the .level file.")]
    public string Arguments { get; set; }

    [Browsable(false)]
    public string DisplayName => $"{Type} {Index}";
}
