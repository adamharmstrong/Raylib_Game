using PowerPulleyPanic.LevelEditor;
using System.Security.Cryptography;

static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
var root = Path.GetFullPath(args[0]);
var compiler = args.Length > 1 ? args[1] : ScriptBuilder.FindCompiler()!;
var sdk = Path.Combine(root, "script_sdk");
var folder = Path.Combine(root, "tmp", "script builder " + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(folder);
var source = Path.Combine(folder, "bounce behavior.cpp");
try {
    File.Copy(Path.Combine(root, "game_data", "scripts", "bounce.cpp"), source);
    var success = await ScriptBuilder.BuildAsync(new[] { source, source, "ignored.lua" }, compiler, sdk);
    Check(success.Success && success.Built == 1 && File.Exists(source + ".dll"), "Build must compile actual C++ source, handle spaces and deduplicate attachments.\n" + success.Diagnostics);
    var previous = SHA256.HashData(File.ReadAllBytes(source + ".dll"));
    File.WriteAllText(source, "#include \"CppScript.h\"\nthis is invalid C++\n");
    var failed = await ScriptBuilder.BuildAsync(new[] { source }, compiler, sdk);
    Check(!failed.Success && failed.Diagnostics.Contains("error") && previous.SequenceEqual(SHA256.HashData(File.ReadAllBytes(source + ".dll"))),
        "Compile failure must report diagnostics and preserve the last working DLL.");
    File.WriteAllText(source, "int unrelated_function() { return 1; }");
    var missingEntry = await ScriptBuilder.BuildAsync(new[] { source }, compiler, sdk);
    Check(!missingEntry.Success && missingEntry.Diagnostics.Contains("ppp_get_script"), "Missing ABI entry point must fail the build.");
    var missingSource = await ScriptBuilder.BuildAsync(new[] { source + "missing.cpp" }, compiler, sdk);
    Check(!missingSource.Success && missingSource.Diagnostics.Contains("does not exist"), "Missing files must have useful diagnostics.");
    using var cancellation = new CancellationTokenSource(); cancellation.Cancel();
    var cancelled = false;
    try { await ScriptBuilder.BuildAsync(new[] { source }, compiler, sdk, cancellation.Token); }
    catch (OperationCanceledException) { cancelled = true; }
    Check(cancelled, "Cancelled builds must stop before invoking the compiler.");
    File.Copy(Path.Combine(root, "game_data", "scripts", "bounce.cpp"), source, overwrite: true);
    using var runningCancellation = new CancellationTokenSource(); runningCancellation.CancelAfter(20);
    cancelled = false;
    try { await ScriptBuilder.BuildAsync(new[] { source }, compiler, sdk, runningCancellation.Token); }
    catch (OperationCanceledException) { cancelled = true; }
    Check(cancelled && previous.SequenceEqual(SHA256.HashData(File.ReadAllBytes(source + ".dll"))),
        "Cancelling an active compiler must terminate it and preserve the published DLL.");
    Check(!Directory.EnumerateFiles(folder).Any(path => path != source && path != source + ".dll"), "Temporary DLLs must be cleaned up.");
    Console.WriteLine("Script builder checks passed: actual compilation, spaces, deduplication, diagnostics, atomic publication, required export, cancellation.");
}
finally {
    if (File.Exists(source)) File.Delete(source);
    if (File.Exists(source + ".dll")) File.Delete(source + ".dll");
    Directory.Delete(folder);
}
