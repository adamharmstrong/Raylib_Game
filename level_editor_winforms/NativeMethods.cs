using System.Runtime.InteropServices;

namespace PowerPulleyPanic.LevelEditor;

internal static class NativeMethods
{
    internal const int GwlStyle = -16;
    internal const int WsChild = 0x40000000;
    internal const int WsVisible = 0x10000000;
    internal const uint WmKeyDown = 0x0100;
    internal const uint WmKeyUp = 0x0101;
    internal const uint WmCopyData = 0x004A;
    internal const uint ViewportHostCommand = 0x8000 + 120;

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SetParent(IntPtr child, IntPtr parent);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern int SetWindowLong(IntPtr window, int index, int value);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern bool MoveWindow(IntPtr window, int x, int y, int width, int height, bool repaint);

    [DllImport("user32.dll")]
    internal static extern IntPtr SetFocus(IntPtr window);

    [DllImport("user32.dll")]
    internal static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW")]
    internal static extern IntPtr SendCopyData(IntPtr window, uint message, IntPtr wParam, ref CopyData data);

    [StructLayout(LayoutKind.Sequential)]
    internal struct CopyData
    {
        internal IntPtr Identifier;
        internal int ByteCount;
        internal IntPtr Data;
    }

    [DllImport("dwmapi.dll")]
    internal static extern int DwmSetWindowAttribute(IntPtr window, int attribute, ref int value, int size);

    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
    internal static extern int SetWindowTheme(IntPtr window, string? subAppName, string? subIdList);
}
