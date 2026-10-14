using System.Diagnostics;

namespace PowerPulleyPanic.LevelEditor;

internal sealed record ScriptBuildResult(bool Success, string Diagnostics, int Built);

internal static class ScriptBuilder
{
    internal static bool IsCpp(string? path) => !string.IsNullOrWhiteSpace(path) && Path.GetExtension(path).ToLowerInvariant() is ".cpp" or ".cc" or ".cxx";

    internal static string? FindCompiler()
    {
        var configured = Environment.GetEnvironmentVariable("PPP_CPP_COMPILER");
        if (!string.IsNullOrWhiteSpace(configured)) return configured;
        var saved = Path.Combine(AppContext.BaseDirectory, "script_sdk", "compiler.txt");
        if (File.Exists(saved) && File.Exists(File.ReadAllText(saved).Trim())) return File.ReadAllText(saved).Trim();
        foreach (var folder in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator))
        {
            var candidate = Path.Combine(folder.Trim('"'), "g++.exe");
            if (File.Exists(candidate)) return candidate;
        }
        const string bundled = @"C:\raylib\w64devkit\bin\g++.exe";
        return File.Exists(bundled) ? bundled : null;
    }

    internal static async Task<ScriptBuildResult> BuildAsync(IEnumerable<string> paths, string compiler, string sdk,
        CancellationToken cancellationToken = default)
    {
        var sources = paths.Where(IsCpp).Select(Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (sources.Length == 0) return new(true, "No C++ attachments to build. Lua scripts run directly.", 0);
        if (!File.Exists(Path.Combine(sdk, "CppScript.h"))) return new(false, "Script SDK is missing: " + sdk, 0);
        var diagnostics = new System.Text.StringBuilder();
        var built = 0;
        foreach (var source in sources)
        {
            cancellationToken.ThrowIfCancellationRequested();
            diagnostics.AppendLine("Building " + source);
            if (!File.Exists(source)) { diagnostics.AppendLine("Source file does not exist."); return new(false, diagnostics.ToString(), built); }
            var output = source + ".dll";
            var temporary = source + "." + Guid.NewGuid().ToString("N") + ".dll";
            try
            {
                var start = new ProcessStartInfo(compiler) { UseShellExecute = false, CreateNoWindow = true,
                    WorkingDirectory = Path.GetDirectoryName(source)!, RedirectStandardError = true, RedirectStandardOutput = true };
                // w64devkit's compiler locates its assembler/linker through PATH.
                start.Environment["PATH"] = Path.GetDirectoryName(Path.GetFullPath(compiler)) + Path.PathSeparator + Environment.GetEnvironmentVariable("PATH");
                foreach (var arg in new[] { "-std=c++17", "-shared", "-O2", "-Wall", "-Wextra", "-static-libgcc", "-static-libstdc++", "-Wl,--require-defined,ppp_get_script",
                    "-I", Path.GetFullPath(sdk), source, "-o", temporary }) start.ArgumentList.Add(arg);
                using var process = Process.Start(start) ?? throw new IOException("The compiler could not be started.");
                var stdout = process.StandardOutput.ReadToEndAsync(cancellationToken);
                var stderr = process.StandardError.ReadToEndAsync(cancellationToken);
                try { await process.WaitForExitAsync(cancellationToken); }
                catch (OperationCanceledException) { if (!process.HasExited) process.Kill(true); await process.WaitForExitAsync(); throw; }
                diagnostics.Append(await stdout); diagnostics.Append(await stderr);
                if (process.ExitCode != 0 || !File.Exists(temporary)) {
                    diagnostics.AppendLine("Build failed. Playtesting was not started.");
                    return new(false, diagnostics.ToString(), built);
                }
                // Publish only a successful result. A compiler failure preserves
                // the previous DLL; Play never launches after a failed build.
                File.Move(temporary, output, overwrite: true);
                ++built; diagnostics.AppendLine("Built " + output);
            }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException or System.ComponentModel.Win32Exception)
            {
                diagnostics.AppendLine(error.Message); return new(false, diagnostics.ToString(), built);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        return new(true, diagnostics.ToString(), built);
    }
}
