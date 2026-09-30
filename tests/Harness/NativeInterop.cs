using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class WdsNativeFs
{
    // Normalize paths at the Win32 boundary, including long paths and UNC spellings.
    private static string NativePath(string path)
    {
        if (path.StartsWith(@"\\?\")) return path;
        return path.StartsWith(@"\\") ? @"\\?\UNC\" + path.Substring(2) : @"\\?\" + System.IO.Path.GetFullPath(path);
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct BY_HANDLE_FILE_INFORMATION
    {
        public uint FileAttributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastAccessTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWriteTime;
        public uint VolumeSerialNumber;
        public uint FileSizeHigh;
        public uint FileSizeLow;
        public uint NumberOfLinks;
        public uint FileIndexHigh;
        public uint FileIndexLow;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WOF_FILE_COMPRESSION_INFO_V1
    {
        public uint Algorithm;
        public ulong Flags;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess, uint dwShareMode,
        IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetFileInformationByHandle(IntPtr hFile, out BY_HANDLE_FILE_INFORMATION lpFileInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr hObject);

    [DllImport("WofUtil.dll", CharSet = CharSet.Unicode)]
    static extern int WofIsExternalFile(string FilePath, out int IsExternalFile, out uint Provider,
        IntPtr ExternalFileInfo, ref uint BufferLength);

    const uint FILE_SHARE_READ = 0x1, FILE_SHARE_WRITE = 0x2, FILE_SHARE_DELETE = 0x4;
    const uint OPEN_EXISTING = 3;
    const uint FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
    const uint WOF_PROVIDER_FILE = 2;

    // Returns "volSerial:fileIndex" identity plus the hardlink count, or null on failure.
    public static string GetFileIdentity(string path, out uint links)
    {
        links = 0;
        IntPtr h = CreateFileW(NativePath(path), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            IntPtr.Zero, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, IntPtr.Zero);
        if (h == new IntPtr(-1)) return null;
        try
        {
            BY_HANDLE_FILE_INFORMATION info;
            if (!GetFileInformationByHandle(h, out info)) return null;
            links = info.NumberOfLinks;
            ulong index = ((ulong)info.FileIndexHigh << 32) | info.FileIndexLow;
            return info.VolumeSerialNumber.ToString("X8") + ":" + index.ToString("X16");
        }
        finally { CloseHandle(h); }
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool GetVolumeInformationW(string rootPath, IntPtr volName, int volNameSize,
        out uint serial, out uint maxComponentLen, out uint fileSystemFlags, IntPtr fsName, int fsNameSize);

    // FileSystemFlags from GetVolumeInformation (0 on failure). Bit 0x10 =
    // FILE_FILE_COMPRESSION, which is exactly what WinDirStat checks to enable
    // standard (LZNT1) compression in CompressFileAllowed().
    public static uint GetVolumeFlags(string rootPath)
    {
        uint serial, maxComp, flags;
        if (!GetVolumeInformationW(rootPath, IntPtr.Zero, 0, out serial,
            out maxComp, out flags, IntPtr.Zero, 0)) return 0;
        return flags;
    }

    // Returns -1 if not WOF, otherwise the WOF algorithm id (XPRESS4K=0, LZX=1, XPRESS8K=2, XPRESS16K=3).
    public static int GetWofAlgorithm(string path)
    {
        uint len = (uint)Marshal.SizeOf(typeof(WOF_FILE_COMPRESSION_INFO_V1));
        IntPtr buf = Marshal.AllocHGlobal((int)len);
        try
        {
            int isExternal; uint provider;
            int hr = WofIsExternalFile(NativePath(path), out isExternal, out provider, buf, ref len);
            if (hr != 0 || isExternal == 0 || provider != WOF_PROVIDER_FILE) return -1;
            var fci = (WOF_FILE_COMPRESSION_INFO_V1)Marshal.PtrToStructure(buf, typeof(WOF_FILE_COMPRESSION_INFO_V1));
            return (int)fci.Algorithm;
        }
        catch { return -1; }
        finally { Marshal.FreeHGlobal(buf); }
    }

    [StructLayout(LayoutKind.Sequential)]
    struct FILE_STANDARD_INFO { public long AllocationSize; public long EndOfFile; public uint NumberOfLinks; public byte DeletePending; public byte Directory; }

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetFileInformationByHandleEx(IntPtr hFile, int infoClass,
        out FILE_STANDARD_INFO info, uint size);

    // True on-disk AllocationSize (cluster-rounded; the resident size for tiny
    // files) — exactly what WinDirStat reports as Physical Size.  Unlike
    // GetCompressedFileSize, which returns the LOGICAL size for ordinary files.
    public static long GetAllocationSize(string path)
    {
        IntPtr h = CreateFileW(NativePath(path), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            IntPtr.Zero, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, IntPtr.Zero);
        if (h == new IntPtr(-1)) return -1;
        try
        {
            FILE_STANDARD_INFO info;
            if (!GetFileInformationByHandleEx(h, 1 /* FileStandardInfo */,
                out info, (uint)Marshal.SizeOf(typeof(FILE_STANDARD_INFO)))) return -1;
            return info.AllocationSize;
        }
        finally { CloseHandle(h); }
    }
}

public static class NativeListViewHelper
{
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    private const uint LVM_FIRST = 0x1000;
    private const uint LVM_GETITEMCOUNT = LVM_FIRST + 4;
    private const uint LVM_GETITEMW = LVM_FIRST + 75;
    private const uint LVM_GETITEMSTATE = LVM_FIRST + 44;
    private const uint LVM_GETSELECTEDCOUNT = LVM_FIRST + 50;
    private const uint LVM_SETITEMSTATE = LVM_FIRST + 43;
    private const uint LVM_ENSUREVISIBLE = LVM_FIRST + 19;
    private const uint LVM_GETHEADER = LVM_FIRST + 31;
    private const uint WM_KEYDOWN = 0x0100;
    private const uint WM_KEYUP = 0x0101;
    private const uint WM_CONTEXTMENU = 0x007B;
    private const uint VK_RIGHT = 0x27;
    private const uint LVIF_TEXT = 0x0001;
    private const uint LVIS_FOCUSED = 0x0001;
    private const uint LVIS_SELECTED = 0x0002;

    private const uint PROCESS_VM_OPERATION = 0x0008;
    private const uint PROCESS_VM_READ = 0x0010;
    private const uint PROCESS_VM_WRITE = 0x0020;
    private const uint PROCESS_QUERY_INFORMATION = 0x0400;
    private const uint MEM_COMMIT = 0x1000;
    private const uint MEM_RESERVE = 0x2000;
    private const uint MEM_RELEASE = 0x8000;
    private const uint PAGE_READWRITE = 0x0004;
    private const uint SMTO_BLOCK = 0x0001;
    private const uint SMTO_ABORTIFHUNG = 0x0002;
    private const uint SMTO_ERRORONEXIT = 0x0020;
    private const uint MESSAGE_TIMEOUT_MS = 5000;

    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct LVITEM
    {
        public uint mask;
        public int iItem;
        public int iSubItem;
        public uint state;
        public uint stateMask;
        public IntPtr pszText;
        public int cchTextMax;
        public int iImage;
        public IntPtr lParam;
        public int iIndent;
        public int iGroupId;
        public uint cColumns;
        public IntPtr puColumns;
        public IntPtr piColFmt;
        public int iGroup;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct TCITEM
    {
        public uint mask, state, stateMask;
        public IntPtr pszText;
        public int cchTextMax, image;
        public IntPtr parameter;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct HDITEM
    {
        public uint mask;
        public int cxy;
        public IntPtr text;
        public IntPtr bitmap;
        public int textCapacity;
        public int format;
        public IntPtr parameter;
        public int image;
        public int order;
        public uint type;
        public IntPtr filter;
        public uint state;
    }

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam,
                                                     uint flags, uint timeoutMilliseconds, out UIntPtr result);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool AttachThreadInput(uint attachThread, uint attachToThread, bool attach);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern IntPtr GetFocus();

    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(uint access, bool inheritHandle, uint processId);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size,
        uint allocationType, uint protect);

    [DllImport("kernel32.dll")]
    private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteProcessMemory(IntPtr process, IntPtr address, IntPtr buffer,
        UIntPtr size, out UIntPtr written);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer,
        UIntPtr size, out UIntPtr read);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process(IntPtr process, out bool wow64);

    public static IntPtr[] GetVisibleListViewsIncludingEmpty(IntPtr root)
    {
        var result = new List<IntPtr>();
        EnumChildWindows(root, delegate (IntPtr hwnd, IntPtr unused)
        {
            var className = new StringBuilder(64);
            GetClassName(hwnd, className, className.Capacity);
            if (className.ToString() == "SysListView32" && IsWindowVisible(hwnd))
                result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }

    public static IntPtr[] GetVisibleListViews(IntPtr root)
    {
        var result = new List<IntPtr>(GetVisibleListViewsIncludingEmpty(root));
        // Keep bounded message calls on the managed side of the callback so a
        // timeout becomes a normal exception instead of crossing a native frame.
        result.RemoveAll(delegate (IntPtr hwnd) { return GetItemCount(hwnd) <= 0; });
        return result.ToArray();
    }

    public static int GetItemCount(IntPtr listView)
    {
        return SendBounded(listView, LVM_GETITEMCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }

    public static int GetSelectedCount(IntPtr listView)
    {
        return SendBounded(listView, LVM_GETSELECTEDCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }

    public static bool SetHeaderWidth(IntPtr listView, int column, int width)
    {
        const uint HDM_SETITEMW = 0x120C;
        IntPtr header = SendBounded(listView, LVM_GETHEADER, IntPtr.Zero, IntPtr.Zero);
        uint processId;
        GetWindowThreadProcessId(listView, out processId);
        IntPtr process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                                     false, processId);
        if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        IntPtr remote = IntPtr.Zero;
        IntPtr local = IntPtr.Zero;
        bool releaseRemote = true;
        try
        {
            EnsureSameBitness(process);
            int size = Marshal.SizeOf(typeof(HDITEM));
            remote = AllocateRemote(process, size);
            local = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(new HDITEM { mask = 1, cxy = width }, local, false);
            WriteRemote(process, remote, local, size);
            return SendBounded(header, HDM_SETITEMW, (IntPtr)column, remote) != IntPtr.Zero;
        }
        catch (TimeoutException)
        {
            releaseRemote = false;
            throw;
        }
        finally
        {
            if (local != IntPtr.Zero) Marshal.FreeHGlobal(local);
            if (releaseRemote && remote != IntPtr.Zero) VirtualFreeEx(process, remote, UIntPtr.Zero, MEM_RELEASE);
            CloseHandle(process);
        }
    }

    public static string[] GetItemTexts(IntPtr listView, int column = 0, int maximum = 1000000)
        => GetControlTexts(listView, column, maximum, false);

    public static string[] GetTabTexts(IntPtr tab) => GetControlTexts(tab, 0, 64, true);

    public static int[] GetTabRectangle(IntPtr tab, int index)
    {
        GetWindowThreadProcessId(tab, out uint processId);
        IntPtr process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, processId);
        if (process == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        IntPtr remote = IntPtr.Zero;
        bool releaseRemote = true;
        try
        {
            remote = AllocateRemote(process, 16);
            if (SendBounded(tab, 0x130A, (IntPtr)index, remote) == IntPtr.Zero)
                throw new InvalidOperationException("Could not read the requested tab rectangle.");
            var bytes = new byte[16];
            if (!ReadProcessMemory(process, remote, bytes, (UIntPtr)16, out UIntPtr read) || read.ToUInt64() != 16)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            return new[] { BitConverter.ToInt32(bytes, 0), BitConverter.ToInt32(bytes, 4),
                BitConverter.ToInt32(bytes, 8), BitConverter.ToInt32(bytes, 12) };
        }
        catch (TimeoutException) { releaseRemote = false; throw; }
        finally
        {
            if (releaseRemote && remote != IntPtr.Zero) VirtualFreeEx(process, remote, UIntPtr.Zero, MEM_RELEASE);
            CloseHandle(process);
        }
    }

    private static string[] GetControlTexts(IntPtr listView, int column, int maximum, bool tabs)
    {
        int count = tabs ? SendBounded(listView, 0x1304, IntPtr.Zero, IntPtr.Zero).ToInt32() : GetItemCount(listView);
        if (count < 0 || count > 1000000)
            throw new InvalidOperationException("Invalid control item count: " + count);
        count = Math.Min(count, maximum);

        uint processId;
        GetWindowThreadProcessId(listView, out processId);
        IntPtr process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ |
            PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, processId);
        if (process == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not open the list-view process.");

        IntPtr remoteItem = IntPtr.Zero;
        IntPtr remoteText = IntPtr.Zero;
        IntPtr localItem = IntPtr.Zero;
        bool releaseRemote = true;
        try
        {
            EnsureSameBitness(process);
            Type itemType = tabs ? typeof(TCITEM) : typeof(LVITEM);
            int structSize = Marshal.SizeOf(itemType);
            const int textBytes = 8192;
            remoteItem = AllocateRemote(process, structSize);
            remoteText = AllocateRemote(process, textBytes);
            localItem = Marshal.AllocHGlobal(structSize);
            var result = new string[count];

            for (int index = 0; index < count; ++index)
            {
                object item = tabs ? (object)new TCITEM { mask = 1, pszText = remoteText, cchTextMax = textBytes / 2 } : new LVITEM
                {
                    mask = LVIF_TEXT,
                    iItem = index,
                    iSubItem = column,
                    pszText = remoteText,
                    cchTextMax = textBytes / 2
                };
                Marshal.StructureToPtr(item, localItem, false);
                WriteRemote(process, remoteItem, localItem, structSize);

                if (SendBounded(listView, tabs ? 0x133C : LVM_GETITEMW, tabs ? (IntPtr)index : IntPtr.Zero,
                    remoteItem) == IntPtr.Zero)
                    throw new InvalidOperationException("Could not read control item " + index + ".");

                // An owner-data provider may replace pszText while servicing
                // the text request. Read the returned item and follow its pointer
                // rather than assuming our exchange buffer was retained.
                var returnedBytes = new byte[structSize];
                UIntPtr bytesRead;
                if (!ReadProcessMemory(process, remoteItem, returnedBytes, (UIntPtr)structSize, out bytesRead) ||
                    bytesRead.ToUInt64() != (ulong)structSize)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read the returned LVITEM.");
                Marshal.Copy(returnedBytes, 0, localItem, structSize);
                object returned = Marshal.PtrToStructure(localItem, itemType);
                IntPtr text = tabs ? ((TCITEM)returned).pszText : ((LVITEM)returned).pszText;
                result[index] = ReadRemoteUnicodeString(process, text, textBytes / 2);
            }
            return result;
        }
        catch (TimeoutException)
        {
            // A timed-out receiver may still own the LVITEM pointers. Leave the
            // tiny allocations in the target process; Windows reclaims them
            // when the harness terminates that hung process.
            releaseRemote = false;
            throw;
        }
        finally
        {
            if (localItem != IntPtr.Zero) Marshal.FreeHGlobal(localItem);
            if (releaseRemote && remoteItem != IntPtr.Zero)
                VirtualFreeEx(process, remoteItem, UIntPtr.Zero, MEM_RELEASE);
            if (releaseRemote && remoteText != IntPtr.Zero)
                VirtualFreeEx(process, remoteText, UIntPtr.Zero, MEM_RELEASE);
            CloseHandle(process);
        }
    }

    public static bool SelectSingleItem(IntPtr listView, int index)
    {
        return SelectItems(listView, new int[] { index });
    }

    public static bool SelectItems(IntPtr listView, int[] indices)
    {
        int count = GetItemCount(listView);
        if (indices == null) return false;
        var unique = new HashSet<int>(indices);
        if (unique.Count != indices.Length) return false;
        foreach (int index in indices)
            if (index < 0 || index >= count) return false;

        uint processId;
        GetWindowThreadProcessId(listView, out processId);
        IntPtr process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                                     false, processId);
        if (process == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not open the list-view process.");

        IntPtr remoteItem = IntPtr.Zero;
        IntPtr localItem = IntPtr.Zero;
        bool releaseRemote = true;
        try
        {
            EnsureSameBitness(process);
            int structSize = Marshal.SizeOf(typeof(LVITEM));
            remoteItem = AllocateRemote(process, structSize);
            localItem = Marshal.AllocHGlobal(structSize);

            var clear = new LVITEM { state = 0, stateMask = LVIS_SELECTED | LVIS_FOCUSED };
            Marshal.StructureToPtr(clear, localItem, false);
            WriteRemote(process, remoteItem, localItem, structSize);
            if (SendBounded(listView, LVM_SETITEMSTATE, new IntPtr(-1), remoteItem) == IntPtr.Zero)
                return false;

            for (int position = 0; position < indices.Length; ++position)
            {
                int index = indices[position];
                uint desiredState = LVIS_SELECTED | (position == 0 ? LVIS_FOCUSED : 0);
                var select = new LVITEM
                {
                    iItem = index,
                    state = desiredState,
                    stateMask = LVIS_SELECTED | LVIS_FOCUSED
                };
                Marshal.StructureToPtr(select, localItem, false);
                WriteRemote(process, remoteItem, localItem, structSize);
                if (SendBounded(listView, LVM_SETITEMSTATE, (IntPtr)index, remoteItem) == IntPtr.Zero)
                    return false;
            }
            if (indices.Length != 0)
                SendBounded(listView, LVM_ENSUREVISIBLE, (IntPtr)indices[0], IntPtr.Zero);

            int selectedCount = SendBounded(listView, LVM_GETSELECTEDCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
            if (selectedCount != indices.Length) return false;
            for (int position = 0; position < indices.Length; ++position)
            {
                uint requiredState = LVIS_SELECTED | (position == 0 ? LVIS_FOCUSED : 0);
                uint state = unchecked((uint)SendBounded(listView, LVM_GETITEMSTATE, (IntPtr)indices[position],
                                                          (IntPtr)(LVIS_SELECTED | LVIS_FOCUSED)).ToInt64());
                if ((state & requiredState) != requiredState) return false;
            }
            return true;
        }
        catch (TimeoutException)
        {
            releaseRemote = false;
            throw;
        }
        finally
        {
            if (localItem != IntPtr.Zero) Marshal.FreeHGlobal(localItem);
            if (releaseRemote && remoteItem != IntPtr.Zero)
                VirtualFreeEx(process, remoteItem, UIntPtr.Zero, MEM_RELEASE);
            CloseHandle(process);
        }
    }

    public static IntPtr FocusWindow(IntPtr window)
    {
        uint processId;
        uint targetThread = GetWindowThreadProcessId(window, out processId);
        uint currentThread = GetCurrentThreadId();
        bool attached = false;
        try
        {
            if (targetThread != currentThread)
            {
                attached = AttachThreadInput(currentThread, targetThread, true);
                if (!attached) return IntPtr.Zero;
            }
            SetFocus(window);
            return GetFocus();
        }
        finally
        {
            if (attached) AttachThreadInput(currentThread, targetThread, false);
        }
    }

    public static bool FocusListView(IntPtr listView)
    {
        return FocusWindow(listView) == listView;
    }

    private static bool PostKey(IntPtr window, uint virtualKey)
    {
        return PostMessage(window, WM_KEYDOWN, (IntPtr)virtualKey, IntPtr.Zero) &&
               PostMessage(window, WM_KEYUP, (IntPtr)virtualKey, IntPtr.Zero);
    }

    public static bool PostRight(IntPtr window) { return PostKey(window, VK_RIGHT); }
    public static bool PostContextMenu(IntPtr window) =>
        PostMessage(window, WM_CONTEXTMENU, window, new IntPtr(-1));

    private static void EnsureSameBitness(IntPtr targetProcess)
    {
        if (!Environment.Is64BitOperatingSystem) return;
        bool targetWow64;
        if (!IsWow64Process(targetProcess, out targetWow64))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not determine target process bitness.");
        bool currentWow64 = !Environment.Is64BitProcess;
        if (targetWow64 != currentWow64)
            throw new InvalidOperationException(
                "Native list-view access requires PowerShell and WinDirStat to have equal bitness.");
    }

    private static IntPtr AllocateRemote(IntPtr process, int bytes)
    {
        IntPtr result = VirtualAllocEx(process, IntPtr.Zero, (UIntPtr)bytes,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (result == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not allocate list-view exchange memory.");
        return result;
    }

    private static void WriteRemote(IntPtr process, IntPtr remote, IntPtr local, int bytes)
    {
        UIntPtr written;
        if (!WriteProcessMemory(process, remote, local, (UIntPtr)bytes, out written) ||
            written.ToUInt64() != (ulong)bytes)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not write list-view exchange memory.");
    }

    private static IntPtr SendBounded(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam)
    {
        UIntPtr result;
        // SendMessageTimeout may return zero on timeout without setting an
        // error. Clear the P/Invoke slot so a stale error cannot make us free
        // remote buffers that the receiver might still reference.
        Marshal.SetLastPInvokeError(0);
        IntPtr succeeded = SendMessageTimeout(hwnd, message, wParam, lParam,
                                               SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT,
                                               MESSAGE_TIMEOUT_MS, out result);
        if (succeeded == IntPtr.Zero)
        {
            int error = Marshal.GetLastWin32Error();
            if (error == 0 || error == 1460) // ERROR_TIMEOUT is not guaranteed to be set.
                throw new TimeoutException("List-view message 0x" + message.ToString("X") +
                                           " timed out after " + MESSAGE_TIMEOUT_MS + " ms.");
            throw new Win32Exception(error, "Could not send a bounded list-view message.");
        }
        return new IntPtr(unchecked((long)result.ToUInt64()));
    }

    private static string ReadRemoteUnicodeString(IntPtr process, IntPtr address, int maxCharacters)
    {
        if (address == IntPtr.Zero) return String.Empty;
        var result = new StringBuilder();
        var bytes = new byte[2];
        for (int index = 0; index < maxCharacters; ++index)
        {
            UIntPtr bytesRead;
            IntPtr current = new IntPtr(address.ToInt64() + index * 2L);
            if (!ReadProcessMemory(process, current, bytes, (UIntPtr)2, out bytesRead) ||
                bytesRead.ToUInt64() != 2)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read list-view text.");
            char character = (char)(bytes[0] | (bytes[1] << 8));
            if (character == '\0') break;
            result.Append(character);
        }
        return result.ToString();
    }
}
