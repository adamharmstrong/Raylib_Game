using System.ComponentModel;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class LevelDocument
{
    private const string OutlinerCategoryPrefix = "# editor-outliner-category ";
    private const string OutlinerItemPrefix = "# editor-outliner-item ";
    private const float StandardExitDoorWidth = 85.0f;
    private const float StandardExitDoorHeight = 210.0f;
    private readonly List<LineRecord> lines = [];

    internal LevelDocument()
    {
        lines.Add(new LineRecord("# New Power Pulley Panic level.", null));
        lines.Add(new LineRecord("script power_pulley_panic", new LevelObject("script", 1, "power_pulley_panic")));
        lines.Add(new LineRecord("bounds 0 0 1600 900", new LevelObject("bounds", 1, "0 0 1600 900")));
        lines.Add(new LineRecord("playerStart 80 600 0", new LevelObject("playerStart", 1, "80 600 0")));
        EnsureCollisionLayer();
    }

    internal string? FilePath { get; private set; }
    private static string QuoteAudio(string path) => System.Text.Json.JsonSerializer.Serialize(path,
        new System.Text.Json.JsonSerializerOptions { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping });

    internal void SetMusic(string? path)
    {
        lines.RemoveAll(line => line.Object?.Type == "music");
        if (!string.IsNullOrEmpty(path)) AddEditableRecord("music", QuoteAudio(path));
    }

    internal int? GetTilePassage(int sheet, int column, int row)
    {
        var prefix = $"{sheet} {column} {row} ";
        for (var index = lines.Count - 1; index >= 0; --index)
        {
            var record = lines[index].Object;
            if (record?.Type == "tilePassage" && record.Arguments.StartsWith(prefix) &&
                int.TryParse(record.Arguments[prefix.Length..], out var mask)) return mask;
        }
        return null;
    }

    internal void SetTilePassage(int sheet, int column, int row, int? mask)
    {
        var prefix = $"{sheet} {column} {row} ";
        lines.RemoveAll(line => line.Object?.Type == "tilePassage" && line.Object.Arguments.StartsWith(prefix));
        if (mask.HasValue)
            lines.Add(new LineRecord(string.Empty, new LevelObject("tilePassage", 1, prefix + Math.Clamp(mask.Value, 0, 15))));
    }
    internal IReadOnlyList<LevelObject> Objects => lines.Where(line => line.Object != null).Select(line => line.Object!).ToList();

    internal IReadOnlyList<OutlinerCategory> OutlinerCategories => lines
        .Select(line => ParseOutlinerCategory(line.OriginalText))
        .Where(category => category != null)
        .Select(category => category!)
        .ToList();

    internal string? OutlinerName(LevelObject item) => ParseOutlinerItem(item)?.Name;

    internal string? OutlinerCategoryId(LevelObject item) => ParseOutlinerItem(item)?.CategoryId;

    internal OutlinerCategory AddOutlinerCategory(string name, string? parentId)
    {
        name = ValidateOutlinerLabel(name, "category");
        if (parentId != null && !OutlinerCategories.Any(category => category.Id == parentId))
            throw new ArgumentException("Choose an existing parent category.");
        var category = new OutlinerCategory(Guid.NewGuid().ToString("N"), parentId, name);
        lines.Add(new LineRecord(SerializeOutlinerCategory(category), null));
        return category;
    }

    internal void RenameOutlinerCategory(string id, string name)
    {
        var category = OutlinerCategories.FirstOrDefault(value => value.Id == id) ??
            throw new ArgumentException("The category no longer exists.");
        ReplaceOutlinerCategory(category with { Name = ValidateOutlinerLabel(name, "category") });
    }

    internal void MoveOutlinerCategory(string id, string? parentId)
    {
        var categories = OutlinerCategories;
        var category = categories.FirstOrDefault(value => value.Id == id) ??
            throw new ArgumentException("The category no longer exists.");
        if (id == parentId) throw new ArgumentException("A category cannot contain itself.");
        if (parentId != null && !categories.Any(value => value.Id == parentId))
            throw new ArgumentException("Choose an existing parent category.");
        for (var ancestor = parentId; ancestor != null;)
        {
            if (ancestor == id) throw new ArgumentException("A category cannot be moved inside one of its sub-categories.");
            ancestor = categories.FirstOrDefault(value => value.Id == ancestor)?.ParentId;
        }
        ReplaceOutlinerCategory(category with { ParentId = parentId });
    }

    internal void DeleteOutlinerCategory(string id)
    {
        var removed = new HashSet<string>(StringComparer.Ordinal);
        removed.Add(id);
        var categories = OutlinerCategories;
        while (true)
        {
            var added = false;
            foreach (var category in categories)
                if (category.ParentId != null && removed.Contains(category.ParentId))
                    added |= removed.Add(category.Id);
            if (!added) break;
        }
        lines.RemoveAll(line => ParseOutlinerCategory(line.OriginalText) is { } category && removed.Contains(category.Id));
        foreach (var item in Objects)
            if (OutlinerCategoryId(item) is { } categoryId && removed.Contains(categoryId))
                SetOutlinerItemMetadata(item, null, OutlinerName(item));
    }

    internal void SetOutlinerName(LevelObject item, string? name)
    {
        name = string.IsNullOrWhiteSpace(name) ? null : ValidateOutlinerLabel(name, "name");
        SetOutlinerItemMetadata(item, OutlinerCategoryId(item), name);
    }

    internal void SetOutlinerCategory(LevelObject item, string? categoryId)
    {
        if (categoryId != null && !OutlinerCategories.Any(category => category.Id == categoryId))
            throw new ArgumentException("Choose an existing category.");
        SetOutlinerItemMetadata(item, categoryId, OutlinerName(item));
    }

    private OutlinerItemMetadata? ParseOutlinerItem(LevelObject item)
    {
        foreach (var line in lines)
        {
            var parts = line.OriginalText.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 7 || parts[0] != "#" || parts[1] != "editor-outliner-item" ||
                !parts[2].Equals(item.Type, StringComparison.OrdinalIgnoreCase) ||
                !int.TryParse(parts[3], out var index) || index != item.Index) continue;
            return new OutlinerItemMetadata(parts[4] == "-" ? null : parts[4], DecodeOutlinerLabel(parts[5]));
        }
        return null;
    }

    private void SetOutlinerItemMetadata(LevelObject item, string? categoryId, string? name)
    {
        for (var index = lines.Count - 1; index >= 0; --index)
        {
            var parts = lines[index].OriginalText.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 7 && parts[0] == "#" && parts[1] == "editor-outliner-item" &&
                parts[2].Equals(item.Type, StringComparison.OrdinalIgnoreCase) &&
                int.TryParse(parts[3], out var itemIndex) && itemIndex == item.Index)
                lines.RemoveAt(index);
        }
        if (categoryId == null && name == null) return;
        var encodedName = name == null ? "-" : EncodeOutlinerLabel(name);
        lines.Add(new LineRecord($"{OutlinerItemPrefix}{item.Type} {item.Index} {categoryId ?? "-"} {encodedName} -", null));
    }

    private void ReplaceOutlinerCategory(OutlinerCategory replacement)
    {
        for (var index = 0; index < lines.Count; ++index)
        {
            if (ParseOutlinerCategory(lines[index].OriginalText)?.Id != replacement.Id) continue;
            lines[index] = new LineRecord(SerializeOutlinerCategory(replacement), null);
            return;
        }
    }

    private static OutlinerCategory? ParseOutlinerCategory(string text)
    {
        var parts = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length != 5 || parts[0] != "#" || parts[1] != "editor-outliner-category") return null;
        var name = DecodeOutlinerLabel(parts[4]);
        return name == null ? null : new OutlinerCategory(parts[2], parts[3] == "-" ? null : parts[3], name);
    }

    private static string SerializeOutlinerCategory(OutlinerCategory category) =>
        $"{OutlinerCategoryPrefix}{category.Id} {category.ParentId ?? "-"} {EncodeOutlinerLabel(category.Name)}";

    private static string ValidateOutlinerLabel(string value, string kind)
    {
        value = value.Trim();
        if (value.Length is < 1 or > 80 || value.Contains('\n') || value.Contains('\r'))
            throw new ArgumentException($"Enter a {kind} between 1 and 80 characters.");
        return value;
    }

    private static string EncodeOutlinerLabel(string value) => Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(value));

    private static string? DecodeOutlinerLabel(string value)
    {
        if (value == "-") return null;
        try { return System.Text.Encoding.UTF8.GetString(Convert.FromBase64String(value)); }
        catch (FormatException) { return null; }
    }

    internal IReadOnlyDictionary<int, string> SpriteSheets => Objects
        .Where(item => item.Type == "spriteSheet")
        .Select(item => item.Arguments.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries))
        .Where(parts => parts.Length == 2 && int.TryParse(parts[0], out var index) && index is >= 3 and <= 255)
        .GroupBy(parts => int.Parse(parts[0]))
        .ToDictionary(group => group.Key, group => group.Last()[1]);

    internal int AddSpriteSheet(string path)
    {
        path = Path.GetFullPath(path).Replace('\\', '/');
        var sheets = SpriteSheets;
        foreach (var entry in sheets)
            if (entry.Value.Equals(path, StringComparison.OrdinalIgnoreCase)) return entry.Key;
        var index = Enumerable.Range(3, 253).FirstOrDefault(candidate => !sheets.ContainsKey(candidate));
        if (index == 0) throw new InvalidOperationException("All sprite sheet slots are in use.");
        var arguments = $"{index} {path}";
        lines.Add(new LineRecord($"spriteSheet {arguments}", new LevelObject("spriteSheet", sheets.Count + 1, arguments)));
        return index;
    }

    internal int GetAnchorPoint(LevelObject item)
    {
        foreach (var line in lines)
        {
            var parts = line.OriginalText.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 5 && parts[0] == "#" && parts[1] == "editor-anchor" &&
                parts[2].Equals(item.Type, StringComparison.OrdinalIgnoreCase) &&
                int.TryParse(parts[3], out var index) && index == item.Index &&
                int.TryParse(parts[4], out var anchor) && anchor is >= 0 and <= 8)
                return anchor;
        }
        return 4;
    }

    internal void SetAnchorPoint(LevelObject item, int anchor)
    {
        anchor = Math.Clamp(anchor, 0, 8);
        var lineText = $"# editor-anchor {item.Type} {item.Index} {anchor}";
        for (var lineIndex = 0; lineIndex < lines.Count; ++lineIndex)
        {
            var parts = lines[lineIndex].OriginalText.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 5 || parts[0] != "#" || parts[1] != "editor-anchor" ||
                !parts[2].Equals(item.Type, StringComparison.OrdinalIgnoreCase) ||
                !int.TryParse(parts[3], out var index) || index != item.Index) continue;
            lines[lineIndex] = new LineRecord(lineText, null);
            return;
        }
        lines.Add(new LineRecord(lineText, null));
    }

    internal void Load(string path)
    {
        FilePath = path;
        RestoreSnapshot(string.Join('\n', File.ReadAllLines(path)));
    }

    internal string CaptureSnapshot()
    {
        ReindexObjects();
        var data = lines.Select(line => line.Object == null ? line.OriginalText
            : string.IsNullOrWhiteSpace(line.Object.Arguments) ? line.Object.Type : $"{line.Object.Type} {line.Object.Arguments}");
        return string.Join('\n', data.Concat(Objects.Where(item => item.Scripts.Length > 0)
            .Select(item => $"objectScripts {item.Type} {item.Index} {System.Text.Json.JsonSerializer.Serialize(item.Scripts)}"))
            .Concat(Objects.Where(item => !string.IsNullOrEmpty(item.Sound))
            .Select(item => $"objectSound {item.Type} {item.Index} {QuoteAudio(item.Sound)}")));
    }

    internal void RestoreSnapshot(string snapshot)
    {
        lines.Clear();
        var attachments = new List<string>();
        var sounds = new List<string>();
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
            if (type == "objectScripts") { attachments.Add(arguments); continue; }
            if (type == "objectSound") { sounds.Add(arguments); continue; }
            if (type.Equals("exit", StringComparison.OrdinalIgnoreCase))
                arguments = NormalizeExitArguments(arguments);
            counts.TryGetValue(type, out var count);
            counts[type] = ++count;
            lines.Add(new LineRecord(text, new LevelObject(type, count, arguments)));
        }
        EnsureCollisionLayer();
        foreach (var sound in sounds)
        {
            var parts = sound.Split(' ', 3, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 3 || !int.TryParse(parts[1], out var index)) continue;
            var target = Objects.FirstOrDefault(item => item.Type == parts[0] && item.Index == index);
            try { if (target != null) target.Sound = System.Text.Json.JsonSerializer.Deserialize<string>(parts[2]) ?? ""; }
            catch (System.Text.Json.JsonException) { lines.Add(new LineRecord("objectSound " + sound, null)); }
        }
        foreach (var attachment in attachments)
        {
            var parts = attachment.Split(' ', 3, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 3 || !int.TryParse(parts[1], out var index)) continue;
            var target = Objects.FirstOrDefault(item => item.Type == parts[0] && item.Index == index);
            if (target == null) continue;
            try { target.Scripts = System.Text.Json.JsonSerializer.Deserialize<string[]>(parts[2]) ?? []; }
            catch (System.Text.Json.JsonException) { lines.Add(new LineRecord("objectScripts " + attachment, null)); }
        }
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

    internal string TileLayerLabel(string key)
    {
        var record = Objects.LastOrDefault(item => item.Type == "tileLayerName" &&
            item.Arguments.Split(' ', 2)[0].Equals(key, StringComparison.OrdinalIgnoreCase));
        if (record != null) return record.Arguments.Split(' ', 2)[1];
        return int.TryParse(key.AsSpan(4), out var slot) ? $"User Layer {slot}" : key;
    }

    internal void RenameTileLayer(string key, string label)
    {
        if (!CustomTileLayers.Contains(key, StringComparer.OrdinalIgnoreCase))
            throw new ArgumentException("Only user-created layers can be renamed.");
        label = label.Trim();
        if (label.Length == 0 || label.Length > 80 || label.Contains('\n') || label.Contains('\r'))
            throw new ArgumentException("Enter a layer name between 1 and 80 characters.");
        if (CustomTileLayers.Any(other => other != key && TileLayerLabel(other).Equals(label, StringComparison.OrdinalIgnoreCase)))
            throw new ArgumentException("Another layer already has that name.");
        var existing = Objects.FirstOrDefault(item => item.Type == "tileLayerName" && item.Arguments.Split(' ', 2)[0] == key);
        if (existing != null) existing.Arguments = $"{key} {label}";
        else lines.Add(new LineRecord($"tileLayerName {key} {label}", new LevelObject("tileLayerName", 1, $"{key} {label}")));
    }

    internal bool IsLayerAboveDefaults(string key) => Objects.Any(item => item.Type == "tileLayerAbove" && item.Arguments == key);

    internal bool CanMoveTileLayer(string key, int direction)
    {
        var above = IsLayerAboveDefaults(key);
        var group = CustomTileLayers.Where(other => IsLayerAboveDefaults(other) == above).ToList();
        var index = group.IndexOf(key);
        return index >= 0 && (direction < 0 ? index > 0 || !above : direction > 0 && (index < group.Count - 1 || above));
    }

    internal bool MoveTileLayer(string key, int direction)
    {
        if (!CanMoveTileLayer(key, direction)) return false;
        var above = CustomTileLayers.Where(IsLayerAboveDefaults).ToList();
        var below = CustomTileLayers.Where(other => !IsLayerAboveDefaults(other)).ToList();
        var group = IsLayerAboveDefaults(key) ? above : below;
        var index = group.IndexOf(key);
        var target = index + Math.Sign(direction);
        if (target >= 0 && target < group.Count) (group[index], group[target]) = (group[target], group[index]);
        else if (direction < 0)
        {
            below.Remove(key);
            above.Add(key);
            lines.Add(new LineRecord($"tileLayerAbove {key}", new LevelObject("tileLayerAbove", 1, key)));
        }
        else
        {
            above.Remove(key);
            below.Insert(0, key);
            lines.RemoveAll(line => line.Object?.Type == "tileLayerAbove" && line.Object.Arguments == key);
        }
        var records = lines.Where(line => line.Object?.Type == "tileLayer" && CustomTileLayers.Contains(line.Object.Arguments.Trim()))
            .ToDictionary(line => line.Object!.Arguments.Trim());
        var ordered = above.Concat(below).Select(name => records[name]).ToArray();
        var next = 0;
        for (var i = 0; i < lines.Count; ++i)
            if (lines[i].Object?.Type == "tileLayer" && records.ContainsKey(lines[i].Object!.Arguments.Trim())) lines[i] = ordered[next++];
        return true;
    }

    internal void RemoveTileLayer(string name)
    {
        if (name.Equals("collision", StringComparison.OrdinalIgnoreCase)) return;
        lines.RemoveAll(line => line.Object?.Type == "tileLayerAbove" && line.Object.Arguments == name);
        lines.RemoveAll(line => line.Object?.Type == "tileLayerName" && line.Object.Arguments.Split(' ', 2)[0] == name);
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
        // Preserve authored door dimensions so loading and saving does not resize a fitted exit.
        if (values.Length >= 4 &&
            float.TryParse(values[2], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var width) && width > 0 &&
            float.TryParse(values[3], System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var height) && height > 0)
            return FormattableString.Invariant($"{x:0.###} {y:0.###} {width:0.###} {height:0.###}");
        return FormattableString.Invariant($"{x:0.###} {y:0.###} {StandardExitDoorWidth:0.###} {StandardExitDoorHeight:0.###}");
    }

    private void RebaseAudioPaths(string oldRoot, string? newRoot)
    {
        string Rebase(string value) => string.IsNullOrEmpty(value) ? "" :
            (newRoot == null ? Path.GetFullPath(value, oldRoot) :
                Path.GetRelativePath(newRoot, Path.GetFullPath(value, oldRoot))).Replace('\\', '/');
        foreach (var item in Objects) {
            item.Sound = Rebase(item.Sound);
            if (item.Type == "music") {
                try { item.Arguments = QuoteAudio(Rebase(
                    System.Text.Json.JsonSerializer.Deserialize<string>(item.Arguments) ?? "")); }
                catch (System.Text.Json.JsonException) { }
            }
        }
    }

    internal void Save(string? path = null)
    {
        if (path != null && FilePath != null && Path.GetFullPath(path) != Path.GetFullPath(FilePath))
        {
            var previousRoot = Path.GetDirectoryName(Path.GetFullPath(FilePath))!;
            var newRoot = Path.GetDirectoryName(Path.GetFullPath(path))!;
            RebaseAudioPaths(previousRoot, newRoot);
            foreach (var item in Objects)
                item.Scripts = item.Scripts.Select(script => Path.GetRelativePath(newRoot,
                    Path.GetFullPath(script, previousRoot)).Replace('\\', '/')).ToArray();
        }
        FilePath = path ?? FilePath ?? throw new InvalidOperationException("The level has no file path.");
        WriteSnapshot(FilePath);
    }

    internal void WriteSnapshot(string path, bool resolveScriptPaths = false)
    {
        if (!resolveScriptPaths) { File.WriteAllText(path, CaptureSnapshot()); return; }
        // Playtests live in a temporary folder. Resolve against the original
        // level without mutating saved paths or adding a history entry.
        var original = Objects.Select(item => (item, scripts: item.Scripts)).ToArray();
        var audio = Objects.Select(item => (item, item.Sound, item.Arguments)).ToArray();
        var root = FilePath == null ? Environment.CurrentDirectory : Path.GetDirectoryName(Path.GetFullPath(FilePath))!;
        try
        {
            RebaseAudioPaths(root, null);
            foreach (var (item, scripts) in original)
                item.Scripts = scripts.Select(script => Path.GetFullPath(script, root).Replace('\\', '/')).ToArray();
            File.WriteAllText(path, CaptureSnapshot());
        }
        finally {
            foreach (var (item, scripts) in original) item.Scripts = scripts;
            foreach (var (item, sound, arguments) in audio) { item.Sound = sound; item.Arguments = arguments; }
        }
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
            "steeringWheel", "screw", "fan", "pinwheel", "wedge", "ramp", "seeSaw", "trapDoor", "chain",
            "physicsRope", "button", "portalPair", "directionalSpikeHazard", "arrowTrap",
            "breakableTile", "enemy", "labelSized", "valve", "waterPit", "spikeHazard", "clockFace", "toxinLeak",
            "checkpoint", "collectible",
            "ball", "barrel", "movingPlatform", "elevator", "pendulumBob", "oneWayPlatform",
            "ceilingHook", "guideRail", "spring", "compressionSpring", "extensionSpring", "torsionSpring",
            "garterSpring", "voluteSpring", "spiralSpring", "constantForceSpring", "constantTorqueSpring",
            "leafSpring", "beamSpring", "discSpring", "waveSpring", "waveWasher", "torsionBar", "ringSpring",
            "elastomerSpring", "pneumaticSpring", "gasSpring", "hydropneumaticSpring",
            "magneticSpring", "compositeSpring", "rod", "fixedJoint", "crank", "ratchet", "clutch", "brake",
            "buttonPlatformLoop", "platformLoopButton", "buttonTrapDoor", "buttonLadder", "buttonExit", "buttonFan", "buttonPlatform", "buttonSpikeHazard", "valveFluidFill",
            "cam", "conveyorBelt", "turntable", "battery", "electricMotor", "generator", "limitSwitch", "relay", "fuse", "magnet", "antiGravityPad", "tripodDerrick", "chainHoist", "blockAndTackle", "comeAlong", "manualBenchGrinder", "loadCart", "revolvingStoneGate", "rockHammer", "fieldCoil", "spokePress", "polarisTelescope", "leedskalninSundial", "perpetualMotionHolder",
            "bellows", "trampoline", "boxingGlove", "vacuum", "magnifyingGlass", "lightSource", "ignitableFuse", "programmableBall", "transRotoConverter", "rotoTransConverter", "acceleratorTube", "pinballBumper", "pinballFlipper", "laserEmitter", "laserMixer", "laserDetector", "hotAirBalloon", "blimp", "leakyBucket", "destructibleWood", "destructibleBrick", "destructibleGlass", "glassPane", "glassBlock", "iceBlock", "iceSheet", "heater", "campfire", "torch", "waterBucket",
            "piston", "hydraulicCylinder", "rocketThruster", "crushingBlock", "swingingHammer", "sawBlade", "spinnerTrap", "steamVent", "electricalArc", "oil", "mud", "speedSensor", "beamSensor", "key", "gasMask", "scubaGear", "breakableCrate", "explosiveBarrel", "tnt", "teleforce", "teslaCoil"
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
            // When insertion/deletion changes the count, match surviving records
            // before updating slots. Otherwise scripts could jump to a neighbor.
            if (existing.Count != replacements.Count)
            {
                var available = existing.Select(entry => entry.line.Object!).ToList();
                var scripts = replacements.Select(arguments => {
                    var match = available.FirstOrDefault(item => item.Arguments == arguments);
                    if (match == null) return (Scripts: Array.Empty<string>(), Sound: "");
                    available.Remove(match);
                    return (Scripts: match.Scripts.ToArray(), Sound: match.Sound);
                }).ToArray();
                for (var index = 0; index < sharedCount; ++index)
                {
                    existing[index].line.Object!.Scripts = scripts[index].Scripts;
                    existing[index].line.Object!.Sound = scripts[index].Sound;
                }
                // Newly inserted instances receive attachments only when matched
                // to a surviving record, never from a deleted slot.
                if (replacements.Count > existing.Count && type is "stoneBlock" or "boulder" or "physicsWheel" or "gear" or "flywheel" or "screw")
                    lines.Add(new LineRecord("layer middleground", null));
                for (var index = existing.Count; index < replacements.Count; ++index)
                    lines.Add(new LineRecord(string.Empty, new LevelObject(type, index + 1, replacements[index]) { Scripts = scripts[index].Scripts, Sound = scripts[index].Sound }));
            }
            for (var index = 0; index < sharedCount; ++index)
                existing[index].line.Object!.Arguments = replacements[index];
            for (var index = existing.Count - 1; index >= replacements.Count; --index)
                lines.RemoveAt(existing[index].index);
            if (replacements.Count <= existing.Count) continue;
            // New records were added above together with their attachment data.
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

    internal IReadOnlyList<LevelObject> ConnectionTargets(LevelObject source) => Objects.Where(item =>
        source.Type == "valve" ? item.Type is "water" or "sand" or "gel" or "gas" :
        source.Type == "button" && item.Type is "trapDoor" or "fan" or "ladder" or "platform" or "exit" or "directionalSpikeHazard" or "spikeHazard" or "buttonPlatformLoop")
        .ToList();

    internal void ConnectObjects(LevelObject source, LevelObject target)
    {
        if (!Objects.Contains(source) || !ConnectionTargets(source).Contains(target))
            throw new InvalidOperationException("Choose a button or valve and a compatible target.");
        var args = target.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        var button = RuntimeButtonIndex(source);
        var type = "";
        var arguments = "";
        var consumesTarget = false;
        switch (target.Type)
        {
            case "buttonPlatformLoop":
                if (args.Length < 9) throw new InvalidOperationException("Loop settings are incomplete.");
                args[0] = button.ToString(System.Globalization.CultureInfo.InvariantCulture);
                target.Arguments = string.Join(' ', args);
                return;
            case "trapDoor": type = "buttonTrapDoor"; arguments = $"{button} {target.Index - 1} 75 120"; break;
            case "fan": type = "buttonFan"; arguments = $"{button} {target.Index - 1} 1"; break;
            case "exit": type = "buttonExit"; arguments = button.ToString(); break;
            case "ladder":
            case "platform":
                if (args.Length < 4) throw new InvalidOperationException("Target bounds are incomplete.");
                type = target.Type == "ladder" ? "buttonLadder" : "buttonPlatform";
                arguments = $"{button} {string.Join(' ', args.Take(4))}";
                consumesTarget = true;
                break;
            case "spikeHazard":
            case "directionalSpikeHazard":
                var offset = target.Type == "directionalSpikeHazard" ? 1 : 0;
                if (args.Length < offset + 4) throw new InvalidOperationException("Target bounds are incomplete.");
                type = "buttonSpikeHazard";
                arguments = $"{button} {(offset == 1 ? args[0] : "up")} {string.Join(' ', args.Skip(offset).Take(4))}";
                consumesTarget = true;
                break;
            default:
                type = "valveFluidFill";
                var fluidIndex = Objects.Where(item => item.Type is "water" or "sand" or "gel" or "gas").ToList().IndexOf(target);
                arguments = $"{fluidIndex} 1 86";
                break;
        }
        // Exit and valve fill are singleton game records. Re-linking replaces them.
        if (type is "buttonExit" or "valveFluidFill") lines.RemoveAll(line => line.Object?.Type == type);
        if (!Objects.Any(item => item.Type == type && item.Arguments == arguments))
            lines.Add(new LineRecord(string.Empty, new LevelObject(type, Objects.Count(item => item.Type == type) + 1, arguments)));
        if (consumesTarget) lines.RemoveAll(line => ReferenceEquals(line.Object, target));
        ReindexObjects();
    }

    internal void RemoveConnection(LevelObject link)
    {
        if (link.Type == "platformLoopButton") NormalizeAndPositionRiders();
        if (link.Type == "buttonPlatformLoop") {
            var values = link.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (values.Length > 0) { values[0] = "-1"; link.Arguments = string.Join(' ', values); }
            return;
        }
        if (link.Type == "platformLoopButton") {
            lines.RemoveAll(line => ReferenceEquals(line.Object, link));
            ReindexObjects();
            return;
        }
        if (link.Type is not ("buttonTrapDoor" or "buttonFan" or "buttonExit" or "buttonLadder" or
            "buttonPlatform" or "buttonSpikeHazard" or "valveFluidFill")) return;
        var args = link.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        var restoredType = link.Type == "buttonLadder" ? "ladder" : link.Type == "buttonPlatform" ? "platform" :
            link.Type == "buttonSpikeHazard" ? "directionalSpikeHazard" : null;
        if (restoredType != null) {
            var arguments = string.Join(' ', args.Skip(1));
            lines.Add(new LineRecord(string.Empty, new LevelObject(restoredType, 1, arguments)));
        }
        lines.RemoveAll(line => ReferenceEquals(line.Object, link));
        ReindexObjects();
    }

    private void ReindexObjects()
    {
        var counts = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var item in Objects) { counts.TryGetValue(item.Type, out var count); item.Index = counts[item.Type] = count + 1; }
    }

    internal LevelObject AddPlatformLoop(float x, float y, float radiusX, float radiusY,
        float width, float height, float speed, int count)
    {
        if (!new[] { x, y, radiusX, radiusY, width, height, speed }.All(float.IsFinite) ||
            radiusX <= 0 || radiusY <= 0 || width <= 0 || height <= 0 || count is < 1 or > 16)
            throw new InvalidOperationException("Use positive loop and platform sizes, and 1–16 platforms.");
        var item = new LevelObject("buttonPlatformLoop", Objects.Count(item => item.Type == "buttonPlatformLoop") + 1,
            FormattableString.Invariant($"-1 {x} {y} {radiusX} {radiusY} {width} {height} {speed} {count}"));
        lines.Add(new LineRecord(string.Empty, item));
        return item;
    }

    internal int RuntimeButtonIndex(LevelObject button)
    {
        var index = 0;
        foreach (var item in Objects) {
            if (ReferenceEquals(item, button)) return index;
            if (item.Type == "button" || item.Type == "platformLoopButton" &&
                item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).Length < 5) ++index;
        }
        throw new InvalidOperationException("The button is no longer in this level.");
    }

    internal LevelObject AttachLoopButton(LevelObject loop, int platform, LevelObject? button = null)
    {
        var values = loop.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (!Objects.Contains(loop) || loop.Type != "buttonPlatformLoop" || values.Length < 9 ||
            !int.TryParse(values[8], out var count) || platform < 0 || platform >= count)
            throw new InvalidOperationException("Choose an existing platform in this loop.");
        if (button != null && (!Objects.Contains(button) || button.Type != "button"))
            throw new InvalidOperationException("Choose an existing button.");
        button ??= AddButton();
        var index = RuntimeButtonIndex(button);
        var size = button.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (size.Length < 4) throw new InvalidOperationException("Button bounds are incomplete.");
        lines.RemoveAll(line => {
            if (line.Object?.Type != "platformLoopButton") return false;
            var values = line.Object.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            return values.Length >= 5 && values[4] == index.ToString();
        });
        var link = new LevelObject("platformLoopButton", 1,
            $"{loop.Index - 1} {platform} {size[2]} {size[3]} {index}");
        lines.Add(new LineRecord(string.Empty, link));
        NormalizeAndPositionRiders();
        ReindexObjects();
        return link;
    }

    private LevelObject AddButton()
    {
        var button = new LevelObject("button", Objects.Count(item => item.Type == "button") + 1, "0 0 32 16");
        lines.Add(new LineRecord(string.Empty, button));
        return button;
    }

    internal void DeletePlatformLoop(LevelObject loop)
    {
        if (loop.Type != "buttonPlatformLoop" || !Objects.Contains(loop)) return;
        NormalizeAndPositionRiders();
        var index = loop.Index - 1;
        foreach (var rider in Objects.Where(item => item.Type == "platformLoopButton").ToList()) {
            var args = rider.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (args.Length == 0 || !int.TryParse(args[0], out var loopIndex)) continue;
            if (loopIndex == index) lines.RemoveAll(line => ReferenceEquals(line.Object, rider));
            else if (loopIndex > index) { args[0] = (loopIndex - 1).ToString(); rider.Arguments = string.Join(' ', args); }
        }
        lines.RemoveAll(line => ReferenceEquals(line.Object, loop));
        ReindexObjects();
    }

    private void NormalizeAndPositionRiders()
    {
        var runtimeIndex = 0;
        for (var i = 0; i < lines.Count; ++i) {
            var item = lines[i].Object;
            if (item?.Type == "button") { ++runtimeIndex; continue; }
            if (item?.Type != "platformLoopButton") continue;
            var values = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (values.Length != 4) continue;
            // Keep legacy runtime order while converting an implicit rider into
            // an ordinary button with an explicit attachment reference.
            var button = new LevelObject("button", 1, $"0 0 {values[2]} {values[3]}");
            lines.Insert(i, new LineRecord(string.Empty, button));
            item.Arguments += $" {runtimeIndex++}";
            ++i;
        }
        ReindexObjects();
        var buttons = Objects.Where(item => item.Type == "button").ToList();
        var loops = Objects.Where(item => item.Type == "buttonPlatformLoop").ToList();
        foreach (var rider in Objects.Where(item => item.Type == "platformLoopButton")) {
            var args = rider.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (args.Length < 5 || !int.TryParse(args[0], out var loopIndex) || !int.TryParse(args[1], out var platform) ||
                !int.TryParse(args[4], out var buttonIndex) || loopIndex < 0 || loopIndex >= loops.Count ||
                buttonIndex < 0 || buttonIndex >= buttons.Count) continue;
            var loop = loops[loopIndex].Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (loop.Length < 9) continue;
            var numbers = loop.Select(value => float.TryParse(value, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var number) ? number : 0).ToArray();
            var cap = Math.Min(numbers[3], numbers[4]);
            var half = Math.Max(0, numbers[4] - cap);
            var straight = half * 2; var arc = MathF.PI * cap; var perimeter = straight * 2 + arc * 2;
            if (cap <= 0 || numbers[8] <= 0 || platform < 0 || platform >= numbers[8]) continue;
            var distance = (platform / numbers[8] * perimeter + half) % perimeter;
            float x, y;
            if (distance < straight) { x = cap; y = -half + distance; }
            else if ((distance -= straight) < arc) { x = MathF.Cos(distance / cap) * cap; y = half + MathF.Sin(distance / cap) * cap; }
            else if ((distance -= arc) < straight) { x = -cap; y = half - distance; }
            else { var angle = MathF.PI + (distance - straight) / cap; x = MathF.Cos(angle) * cap; y = -half + MathF.Sin(angle) * cap; }
            if (!float.TryParse(args[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var width) ||
                !float.TryParse(args[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var height)) continue;
            buttons[buttonIndex].Arguments = FormattableString.Invariant($"{numbers[1] + x - width / 2} {numbers[2] + y - numbers[6] / 2 - height + 4} {width} {height}");
        }
    }
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

    [Category("Scripts"), Description("Attached Lua and C++ paths. Lua runs during playtesting; C++ requires compilation.")]
    public string[] Scripts { get; set; } = [];
    public string Sound { get; set; } = "";

    [Browsable(false)]
    public string DisplayName => $"{Type} {Index}";
}

internal sealed record OutlinerCategory(string Id, string? ParentId, string Name);

internal sealed record OutlinerItemMetadata(string? CategoryId, string? Name);
