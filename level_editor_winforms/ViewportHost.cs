using System.Diagnostics;
using System.Runtime.InteropServices;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class ViewportHost : Panel
{
    private Process? process;
    private IntPtr viewportWindow;
    internal event Action<string>? TilePainted;
    internal event Action<string>? SelectionChangedInViewport;

    internal ViewportHost()
    {
        BackColor = Color.FromArgb(15, 18, 23);
        Dock = DockStyle.Fill;
        Resize += (_, _) => ResizeViewport();
    }

    internal void OpenLevel(string levelPath)
    {
        StartViewport($"--viewport-only \"{levelPath}\"");
    }

    internal void OpenBlank()
    {
        StartViewport("--viewport-only");
    }

    internal void ReloadDocument(string path)
    {
        if (viewportWindow == IntPtr.Zero) { OpenLevel(path); return; }
        var bytes = System.Text.Encoding.UTF8.GetBytes(path + '\0');
        var buffer = Marshal.AllocHGlobal(bytes.Length);
        try
        {
            Marshal.Copy(bytes, 0, buffer, bytes.Length);
            var data = new NativeMethods.CopyData { Identifier = (IntPtr)1, ByteCount = bytes.Length, Data = buffer };
            NativeMethods.SendCopyData(viewportWindow, NativeMethods.WmCopyData, Handle, ref data);
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private void StartViewport(string arguments)
    {
        StopViewport();
        var executable = Path.Combine(AppContext.BaseDirectory, "LevelEditorViewport.exe");
        if (!File.Exists(executable)) return;

        process = Process.Start(new ProcessStartInfo(executable, arguments)
        {
            WorkingDirectory = AppContext.BaseDirectory,
            UseShellExecute = false
        });
        if (process == null) return;

        for (var attempt = 0; attempt < 80 && !process.HasExited; ++attempt)
        {
            process.Refresh();
            viewportWindow = process.MainWindowHandle;
            if (viewportWindow != IntPtr.Zero) break;
            Thread.Sleep(25);
        }
        if (viewportWindow == IntPtr.Zero) return;

        NativeMethods.SetParent(viewportWindow, Handle);
        NativeMethods.SetWindowLong(viewportWindow, NativeMethods.GwlStyle,
            NativeMethods.WsChild | NativeMethods.WsVisible);
        ResizeViewport();
        SendCommand(33, 0);
    }

    internal void SendKey(Keys key)
    {
        if (viewportWindow == IntPtr.Zero) return;
        NativeMethods.SetFocus(viewportWindow);
        NativeMethods.PostMessage(viewportWindow, NativeMethods.WmKeyDown, (IntPtr)key, IntPtr.Zero);
        NativeMethods.PostMessage(viewportWindow, NativeMethods.WmKeyUp, (IntPtr)key, IntPtr.Zero);
    }

    internal void SendCommand(int command, int value)
    {
        if (viewportWindow != IntPtr.Zero)
            NativeMethods.PostMessage(viewportWindow, NativeMethods.ViewportHostCommand,
                (IntPtr)command, (IntPtr)value);
    }

    internal void StopViewport()
    {
        viewportWindow = IntPtr.Zero;
        if (process is { HasExited: false }) process.Kill(true);
        process?.Dispose();
        process = null;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) StopViewport();
        base.Dispose(disposing);
    }

    protected override void WndProc(ref Message message)
    {
        if ((uint)message.Msg == NativeMethods.WmCopyData)
        {
            var data = Marshal.PtrToStructure<CopyData>(message.LParam);
            var text = Marshal.PtrToStringUTF8(data.Data, Math.Max(0, data.ByteCount - 1));
            if (!string.IsNullOrWhiteSpace(text))
            {
                if (text.StartsWith("select\t", StringComparison.Ordinal)) SelectionChangedInViewport?.Invoke(text);
                else TilePainted?.Invoke(text);
            }
            message.Result = (IntPtr)1;
            return;
        }
        base.WndProc(ref message);
    }

    private void ResizeViewport()
    {
        if (viewportWindow != IntPtr.Zero)
            NativeMethods.MoveWindow(viewportWindow, 0, 0, ClientSize.Width, ClientSize.Height, true);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct CopyData
    {
        internal IntPtr Identifier;
        internal int ByteCount;
        internal IntPtr Data;
    }
}
