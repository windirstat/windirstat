using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public sealed class TestFrame
{
    public int Width;
    public int Height;
    public byte[] Pixels;
}

public sealed class TestWindow
{
    public long Handle;
    public int Id;
    public string Class;
    public string Text;
    public bool Visible;
    public bool Enabled;
    public int[] Rectangle;
}

public static class TestDesktop
{
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT
    {
        public int Left, Top, Right, Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT
    {
        public int X, Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct GUITHREADINFO
    {
        public uint Size, Flags;
        public IntPtr Active, Focus, Capture, MenuOwner, MoveSize, Caret;
        public RECT CaretRect;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BITMAPINFO
    {
        public uint Size;
        public int Width, Height;
        public ushort Planes, BitCount;
        public uint Compression, SizeImage;
        public int XPelsPerMeter, YPelsPerMeter;
        public uint ClrUsed, ClrImportant;
    }

    private delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr root,
        EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr window,
        StringBuilder text, int size);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetWindowText(IntPtr window,
        StringBuilder text, int size);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsChild(IntPtr root, IntPtr child);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern IntPtr GetAncestor(IntPtr window, uint flags);
    [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")] private static extern bool AttachThreadInput(uint thread, uint other, bool attach);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] private static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr SetCapture(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ReleaseCapture();
    [DllImport("user32.dll")] private static extern uint GetGuiResources(IntPtr process, uint flags);
    [DllImport("user32.dll")] private static extern bool GetGUIThreadInfo(uint thread, ref GUITHREADINFO info);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool GetClientRect(IntPtr window,
        out RECT rect);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] private static extern bool ClientToScreen(IntPtr window, ref POINT point);
    [DllImport("user32.dll")] private static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
    [DllImport("user32.dll")] private static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", SetLastError = true)] private static extern IntPtr SendMessageTimeout(IntPtr window,
        uint message, IntPtr wp, IntPtr lp, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr SendMessageTimeout(IntPtr window,
        uint message, IntPtr wp, string lp, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr SendMessageTimeout(IntPtr window,
        uint message, IntPtr wp, StringBuilder lp, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr window, IntPtr after,
        int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] private static extern bool RedrawWindow(IntPtr window,
        IntPtr rect, IntPtr region, uint flags);
    [DllImport("user32.dll", EntryPoint = "RedrawWindow")] private static extern bool RedrawRect(IntPtr window,
        ref RECT rect, IntPtr region, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr CreateWindowEx(uint exStyle,
        string className, string text, uint style, int x, int y, int width, int height,
        IntPtr parent, IntPtr menu, IntPtr instance, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern bool UpdateWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr window);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr window, IntPtr dc);
    [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr menu, int position);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr menu, int position);
    [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr menu, uint id, uint flags);
    [DllImport("gdi32.dll")] private static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] private static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int width, int height);
    [DllImport("gdi32.dll")] private static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] private static extern bool BitBlt(IntPtr dest, int x, int y, int width, int height,
        IntPtr source, int sx, int sy, uint operation);
    [DllImport("gdi32.dll")] private static extern int GetDIBits(IntPtr dc, IntPtr bitmap, uint first,
        uint lines, byte[] pixels, ref BITMAPINFO info, uint usage);
    [DllImport("gdi32.dll")] private static extern bool DeleteObject(IntPtr obj);
    [DllImport("gdi32.dll")] private static extern bool DeleteDC(IntPtr dc);
    [DllImport("dwmapi.dll")] private static extern int DwmFlush();
    [DllImport("user32.dll")] private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    // Preserve the caller's input and DPI context around desktop scenarios.
    public static IntPtr DpiContext(IntPtr context) => SetThreadDpiAwarenessContext(context);
    public static uint GdiObjects(IntPtr process) => GetGuiResources(process, 0);

    public static int[] CursorPosition()
    {
        return GetCursorPos(out POINT point) ? new[] { point.X, point.Y } :
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static void MoveCursor(int x, int y) => SetCursorPos(x, y);

    public static IntPtr TransferCapture(IntPtr currentOwner, IntPtr window)
    {
        // Share input queues only while another owned window takes native capture.
        uint current = GetCurrentThreadId();
        uint target = GetWindowThreadProcessId(currentOwner, out _);
        bool attached = target != current && AttachThreadInput(current, target, true);
        if (target != current && !attached)
            throw new Win32Exception(Marshal.GetLastWin32Error());
        try { return SetCapture(window); }
        finally { if (attached) AttachThreadInput(current, target, false); }
    }

    public static void ParkCursor(IntPtr window)
    {
        int[] rect = Rectangle(window);
        SetCursorPos(rect[0] + 20, rect[1] + 10);
    }

    public static bool HasInputDesktop()
    {
        IntPtr desktop = OpenInputDesktop(0, false, 1);
        if (desktop == IntPtr.Zero) return false;
        CloseDesktop(desktop);
        return true;
    }

    public static IntPtr Message(IntPtr window, uint message, long wp = 0, long lp = 0)
    {
        // Bound cross-process calls so a hung receiver cannot strand the harness.
        if (!IsWindow(window)) throw new InvalidOperationException("Window no longer exists.");
        if (SendMessageTimeout(window, message, (IntPtr)wp, (IntPtr)lp, 0x22, 3000, out UIntPtr result) == IntPtr.Zero)
            throw new TimeoutException("Window did not respond to message 0x" + message.ToString("X"));
        return (IntPtr)unchecked((long)result.ToUInt64());
    }

    public static void SetText(IntPtr window, string text)
    {
        if (SendMessageTimeout(window, 0x000C, IntPtr.Zero, text, 0x22, 3000, out _) == IntPtr.Zero)
            throw new TimeoutException("Window did not accept text.");
    }

    public static string Text(IntPtr window)
    {
        var text = new StringBuilder(8192);
        GetWindowText(window, text, text.Capacity);
        return text.ToString();
    }

    public static string ControlText(IntPtr window)
    {
        var text = new StringBuilder(8192);
        if (SendMessageTimeout(window, 0x000D, (IntPtr)text.Capacity, text, 0x22, 3000, out _) == IntPtr.Zero)
            throw new TimeoutException("Control did not return its text.");
        return text.ToString();
    }

    public static string Class(IntPtr window)
    {
        var text = new StringBuilder(256);
        GetClassName(window, text, text.Capacity);
        return text.ToString();
    }

    public static IntPtr[] Windows(int processId)
    {
        var windows = new List<IntPtr>();
        EnumWindows((window, _) =>
        {
            GetWindowThreadProcessId(window, out uint pid);
            if (pid == processId) windows.Add(window);
            return true;
        }, IntPtr.Zero);
        return windows.ToArray();
    }

    public static IntPtr[] Children(IntPtr root)
    {
        var windows = new List<IntPtr>();
        EnumChildWindows(root, (window, _) => { windows.Add(window); return true; }, IntPtr.Zero);
        return windows.ToArray();
    }

    public static TestWindow[] Describe(IntPtr root)
    {
        var windows = new List<IntPtr>(Children(root));
        windows.Insert(0, root);
        return windows.ConvertAll(window => new TestWindow
        {
            Handle = window.ToInt64(), Id = GetDlgCtrlID(window), Class = Class(window), Text = Text(window),
            Visible = IsWindowVisible(window), Enabled = IsWindowEnabled(window), Rectangle = Rectangle(window)
        }).ToArray();
    }

    public static int[] Rectangle(IntPtr window)
    {
        return GetWindowRect(window, out RECT rect) ? new[] { rect.Left, rect.Top, rect.Right, rect.Bottom } :
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static int[] ClientSize(IntPtr window)
    {
        return GetClientRect(window, out RECT rect) ? new[] { rect.Right, rect.Bottom } :
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static IntPtr Focus(IntPtr root)
    {
        uint thread = GetWindowThreadProcessId(root, out _);
        var info = new GUITHREADINFO { Size = (uint)Marshal.SizeOf<GUITHREADINFO>() };
        return GetGUIThreadInfo(thread, ref info) ? info.Focus : IntPtr.Zero;
    }

    public static IntPtr CaptureOwner(IntPtr root)
    {
        uint thread = GetWindowThreadProcessId(root, out _);
        var info = new GUITHREADINFO { Size = (uint)Marshal.SizeOf<GUITHREADINFO>() };
        return GetGUIThreadInfo(thread, ref info) ? info.Capture : IntPtr.Zero;
    }

    public static void Position(IntPtr window, int x, int y, int width, int height)
    {
        if (!SetWindowPos(window, IntPtr.Zero, x, y, width, height, 0x0004))
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static void KeepVisible(IntPtr window)
    {
        // Keep the owned test window above unrelated apps while reading actual desktop pixels.
        if (!SetWindowPos(window, (IntPtr)(-1), 0, 0, 0, 0, 0x0013))
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static void Activate(IntPtr window)
    {
        // Join the current foreground input queue only for the activation request.
        uint current = GetCurrentThreadId();
        uint foreground = GetWindowThreadProcessId(GetForegroundWindow(), out _);
        bool attached = foreground != 0 && foreground != current && AttachThreadInput(current, foreground, true);
        try { SetForegroundWindow(window); }
        finally { if (attached) AttachThreadInput(current, foreground, false); }
    }

    public static void Expose(IntPtr window)
    {
        IntPtr root = GetAncestor(window, 2);
        int[] rect = Rectangle(window);
        IntPtr covering = WindowFromPoint(new POINT { X = (rect[0] + rect[2]) / 2, Y = (rect[1] + rect[3]) / 2 });
        if (covering == root || IsChild(root, covering)) return;
        KeepVisible(root);
    }

    public static void Repaint(IntPtr window, bool partial = false)
    {
        bool success;
        if (partial)
        {
            int[] size = ClientSize(window);
            var rect = new RECT { Left = size[0] / 4, Top = size[1] / 4,
                Right = size[0] * 3 / 4, Bottom = size[1] * 3 / 4 };
            success = RedrawRect(window, ref rect, IntPtr.Zero, 0x0105);
        }
        else success = RedrawWindow(window, IntPtr.Zero, IntPtr.Zero, 0x0185);
        if (!success) throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static IntPtr Cover(IntPtr window)
    {
        int[] rect = Rectangle(window);
        IntPtr cover = CreateWindowEx(0x08000088, "Static", "WinDirStat repaint test", 0x90000000,
            rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        if (cover == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        UpdateWindow(cover);
        DwmFlush();
        return cover;
    }

    public static TestFrame Capture(IntPtr window)
    {
        // Read displayed pixels without sending WM_PRINT or forcing the target to repaint.
        IntPtr previousDpi = SetThreadDpiAwarenessContext((IntPtr)(-4));
        IntPtr screen = IntPtr.Zero, memory = IntPtr.Zero, bitmap = IntPtr.Zero, previous = IntPtr.Zero;
        try
        {
            if (!IsWindowVisible(window)) throw new InvalidOperationException("Cannot capture a hidden window.");
            GetClientRect(window, out RECT rect);
            var point = new POINT();
            ClientToScreen(window, ref point);
            int left = GetSystemMetrics(76), top = GetSystemMetrics(77);
            if (rect.Right < 1 || rect.Bottom < 1 || point.X < left || point.Y < top ||
                point.X + rect.Right > left + GetSystemMetrics(78) ||
                point.Y + rect.Bottom > top + GetSystemMetrics(79))
                throw new InvalidOperationException("Capture target must fit entirely on the desktop.");
            DwmFlush();
            screen = GetDC(IntPtr.Zero);
            memory = CreateCompatibleDC(screen);
            bitmap = CreateCompatibleBitmap(screen, rect.Right, rect.Bottom);
            if (screen == IntPtr.Zero || memory == IntPtr.Zero || bitmap == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            previous = SelectObject(memory, bitmap);
            if (!BitBlt(memory, 0, 0, rect.Right, rect.Bottom, screen, point.X, point.Y, 0x40CC0020))
                throw new Win32Exception(Marshal.GetLastWin32Error());
            SelectObject(memory, previous);
            previous = IntPtr.Zero;
            var info = new BITMAPINFO { Size = 40, Width = rect.Right, Height = -rect.Bottom, Planes = 1, BitCount = 32 };
            var pixels = new byte[checked(rect.Right * rect.Bottom * 4)];
            if (GetDIBits(screen, bitmap, 0, (uint)rect.Bottom, pixels, ref info, 0) != rect.Bottom)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            return new TestFrame { Width = rect.Right, Height = rect.Bottom, Pixels = pixels };
        }
        finally
        {
            // Release every GDI object even when capture fails halfway through.
            if (previous != IntPtr.Zero) SelectObject(memory, previous);
            if (bitmap != IntPtr.Zero) DeleteObject(bitmap);
            if (memory != IntPtr.Zero) DeleteDC(memory);
            if (screen != IntPtr.Zero) ReleaseDC(IntPtr.Zero, screen);
            if (previousDpi != IntPtr.Zero) SetThreadDpiAwarenessContext(previousDpi);
        }
    }

    public static double Difference(TestFrame expected, TestFrame actual, int tolerance = 8)
    {
        // Ignore alpha and small color-rounding changes; count changed pixels, not channels.
        if (expected.Width != actual.Width || expected.Height != actual.Height) return 1;
        int changed = 0;
        for (int offset = 0; offset < expected.Pixels.Length; offset += 4)
        {
            if (Math.Abs(expected.Pixels[offset] - actual.Pixels[offset]) > tolerance ||
                Math.Abs(expected.Pixels[offset + 1] - actual.Pixels[offset + 1]) > tolerance ||
                Math.Abs(expected.Pixels[offset + 2] - actual.Pixels[offset + 2]) > tolerance) ++changed;
        }
        return (double)changed / (expected.Width * expected.Height);
    }

    public static int ColorCount(TestFrame frame)
    {
        var colors = new HashSet<int>();
        for (int offset = 0; offset < frame.Pixels.Length; offset += 4)
            colors.Add(frame.Pixels[offset] | (frame.Pixels[offset + 1] << 8) | (frame.Pixels[offset + 2] << 16));
        return colors.Count;
    }
}
