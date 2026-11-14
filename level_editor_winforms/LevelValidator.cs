namespace PowerPulleyPanic.LevelEditor;

internal sealed record LevelValidationIssue(string Severity, string Message, LevelObject? Target = null);

internal static class LevelValidator
{
    internal static IReadOnlyList<LevelValidationIssue> Validate(LevelDocument document)
    {
        var issues = new List<LevelValidationIssue>();
        var objects = document.Objects;
        var root = document.FilePath is { Length: > 0 } path
            ? Path.GetDirectoryName(Path.GetFullPath(path))! : Environment.CurrentDirectory;
        var starts = objects.Where(item => item.Type.Equals("playerStart", StringComparison.OrdinalIgnoreCase)).ToArray();
        if (starts.Length == 0) issues.Add(new("Error", "The level has no Player Start."));
        else if (starts.Length > 1) issues.Add(new("Warning", $"The level has {starts.Length} Player Starts; gameplay uses one start."));
        if (!objects.Any(item => item.Type.Equals("exit", StringComparison.OrdinalIgnoreCase)))
            issues.Add(new("Warning", "The level has no Exit Trigger."));

        var bounds = objects.FirstOrDefault(item => item.Type.Equals("bounds", StringComparison.OrdinalIgnoreCase));
        if (bounds == null || !HasPositiveBounds(bounds.Arguments))
            issues.Add(new("Error", "Level bounds are missing or invalid.", bounds));

        foreach (var sheet in objects.Where(item => item.Type.Equals("spriteSheet", StringComparison.OrdinalIgnoreCase)))
        {
            var parts = sheet.Arguments.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 2) { issues.Add(new("Error", "A sprite sheet record has no file path.", sheet)); continue; }
            CheckPath(parts[1], "Sprite sheet", sheet);
        }
        foreach (var item in objects)
        {
            foreach (var script in item.Scripts) CheckPath(script, "Attached script", item);
            if (!string.IsNullOrWhiteSpace(item.Sound)) CheckPath(item.Sound, "Attached sound", item);
        }
        foreach (var music in objects.Where(item => item.Type.Equals("music", StringComparison.OrdinalIgnoreCase)))
        {
            try
            {
                var musicPath = System.Text.Json.JsonSerializer.Deserialize<string>(music.Arguments);
                if (!string.IsNullOrWhiteSpace(musicPath)) CheckPath(musicPath, "Level music", music);
            }
            catch (System.Text.Json.JsonException) { issues.Add(new("Error", "Level music path is malformed.", music)); }
        }

        if (issues.Count == 0) issues.Add(new("OK", "No issues found."));
        return issues;

        void CheckPath(string value, string kind, LevelObject owner)
        {
            try
            {
                var resolved = Path.GetFullPath(value, root);
                if (!File.Exists(resolved)) issues.Add(new("Error", $"{kind} file is missing: {value}", owner));
            }
            catch (Exception error) when (error is ArgumentException or IOException or NotSupportedException)
            { issues.Add(new("Error", $"{kind} path is invalid: {value}", owner)); }
        }
    }

    private static bool HasPositiveBounds(string arguments)
    {
        var values = arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        return values.Length >= 4 &&
            float.TryParse(values[0], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out _) &&
            float.TryParse(values[1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out _) &&
            float.TryParse(values[2], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var width) && width > 0 &&
            float.TryParse(values[3], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var height) && height > 0;
    }
}
