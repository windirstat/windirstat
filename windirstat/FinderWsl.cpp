// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FinderWsl.h"
#include "FinderWSLProtocol.h"

// --- Distribution and path helpers ---

static constexpr std::wstring_view WslPrefixes[] =
    { L"\\\\wsl.localhost\\", L"\\\\wsl$\\", L"\\\\?\\UNC\\wsl.localhost\\", L"\\\\?\\UNC\\wsl$\\" };
static constexpr std::wstring_view LongPathPrefix = L"\\\\?\\";

static std::wstring_view StripLongPrefix(std::wstring_view path) noexcept
{
    if (path.starts_with(LongPathPrefix) && !path.starts_with(L"\\\\?\\UNC\\"))
        path.remove_prefix(LongPathPrefix.size());
    return path;
}

static bool EqualsNoCase(const std::wstring_view a, const std::wstring_view b) noexcept
{
    return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
        b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

// Returns the remainder of the path after the WSL share prefix, or nothing if it has none.
static std::optional<std::wstring_view> AfterPrefix(const std::wstring_view path) noexcept
{
    for (const auto prefix : WslPrefixes)
    {
        if (path.size() > prefix.size() && EqualsNoCase(path.substr(0, prefix.size()), prefix))
            return path.substr(prefix.size());
    }
    return std::nullopt;
}

// The Plan 9 server shows characters that Windows forbids in names in a private-use range.
static bool IsRemappedCharacter(const wchar_t c) noexcept
{
    return c > 0 && c < 0x20 || c == L'"' || c == L'*' || c == L':' || c == L'<' || c == L'>' ||
        c == L'?' || c == L'\\' || c == L'|';
}

// Decodes a Linux name as \\wsl.localhost shows it: Windows decodes the share's UTF-8 names with this same
// conversion, which decides how many U+FFFD replace an invalid sequence.
static void AppendLinuxName(std::vector<wchar_t>& out, const std::string_view name)
{
    // UTF-16 never needs more code units than UTF-8 needs bytes
    const size_t offset = out.size();
    out.resize(offset + name.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()),
        out.data() + offset, static_cast<int>(name.size()));
    out.resize(offset + static_cast<size_t>(std::max(length, 0)));
    for (wchar_t& c : std::span(out).subspan(offset))
        if (IsRemappedCharacter(c)) c = static_cast<wchar_t>(c + 0xF000);
}

// Converts the part of a \\wsl.localhost path after the distribution name to a Linux path.
static std::string ToLinuxPath(const std::wstring_view relative)
{
    std::wstring mapped(relative);
    for (auto& c : mapped)
    {
        if (c == L'\\') c = L'/';
        else if (c >= 0xF000 && c < 0xF080 && IsRemappedCharacter(static_cast<wchar_t>(c - 0xF000)))
            c = static_cast<wchar_t>(c - 0xF000);
    }
    while (mapped.size() > 1 && mapped.ends_with(L'/')) mapped.pop_back();
    if (!mapped.starts_with(L'/')) mapped.insert(0, 1, L'/');

    const int size = WideCharToMultiByte(CP_UTF8, 0, mapped.data(), static_cast<int>(mapped.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(std::max(size, 0)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, mapped.data(), static_cast<int>(mapped.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring GetWslExecutable()
{
    // A 32-bit build must reach the native System32 through Sysnative.
    std::wstring windows(MAX_PATH, L'\0');
    windows.resize(GetSystemWindowsDirectory(windows.data(), MAX_PATH));
    BOOL wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow64);
    return windows + (wow64 ? L"\\Sysnative\\wsl.exe" : L"\\System32\\wsl.exe");
}

// Quotes one argument so that CommandLineToArgvW-compatible parsers read it back unchanged.
static std::wstring QuoteArgument(const std::wstring_view argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
        return std::wstring(argument);

    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const wchar_t c : argument)
    {
        if (c == L'\\') { ++backslashes; continue; }
        quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        quoted += c;
    }
    quoted.append(backslashes * 2, L'\\');
    return quoted + L'"';
}

static std::wstring Utf8ToWide(const std::string_view text)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(std::max(size, 0)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// wsl.exe reports its own messages in UTF-16 and relays Linux output unchanged.
static std::wstring DecodeConsoleText(const std::string_view text)
{
    std::wstring result;
    if (text.size() >= 2 && text.size() % 2 == 0 && text[1] == '\0')
        result.assign(reinterpret_cast<const wchar_t*>(text.data()), text.size() / 2);
    else result = Utf8ToWide(text);
    std::erase(result, L'\0');
    std::erase(result, L'\r');
    while (!result.empty() && iswspace(result.back())) result.pop_back();
    return result;
}

// A child process whose standard streams are pipes owned by this object.
class WslProcess final
{
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_process{ CloseHandle, nullptr };
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_input{ CloseHandle, nullptr };
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_output{ CloseHandle, nullptr };
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_outputReady{ CloseHandle, nullptr };
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_errors{ CloseHandle, nullptr };
    std::string m_errorText;

public:
    WslProcess() = default;
    WslProcess(const WslProcess&) = delete;
    WslProcess& operator=(const WslProcess&) = delete;

    ~WslProcess()
    {
        // Closing the pipes lets the helper stop on a broken pipe and remove its copy.
        m_input = nullptr;
        m_output = nullptr;
        m_errors = nullptr;
        if (m_process != nullptr && WaitForSingleObject(m_process, 3000) == WAIT_TIMEOUT)
            TerminateProcess(m_process, 1);
    }

    bool Start(const std::wstring& arguments, const DWORD inputSize)
    {
        // Standard output is overlapped, so a read can wait for data and still notice pause and cancellation
        static std::atomic<ULONG> pipes;
        const std::wstring outputName = std::format(L"\\\\.\\pipe\\WinDirStat-WSL-{}-{}",
            GetCurrentProcessId(), pipes.fetch_add(1));
        m_output = CreateNamedPipe(outputName.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED |
            FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, 1024 * 1024, 0, nullptr);
        m_outputReady = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!m_output.IsValid() || m_outputReady == nullptr) return false;
        SECURITY_ATTRIBUTES inherit{ sizeof(inherit), nullptr, TRUE };
        SmartPointer<HANDLE, decltype(&CloseHandle)> outputGuard(CloseHandle, CreateFile(outputName.c_str(),
            GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, &inherit, OPEN_EXISTING, 0, nullptr));
        HANDLE childInput = nullptr, childErrors = nullptr, handle = nullptr;
        if (!outputGuard.IsValid() || !CreatePipe(&childInput, &handle, &inherit, std::max<DWORD>(inputSize, 4096)))
            return false;
        SmartPointer<HANDLE, decltype(&CloseHandle)> inputGuard(CloseHandle, childInput);
        m_input = handle;
        if (!CreatePipe(&handle, &childErrors, &inherit, 64 * 1024)) return false;
        SmartPointer<HANDLE, decltype(&CloseHandle)> errorsGuard(CloseHandle, childErrors);
        m_errors = handle;
        for (HANDLE own : { static_cast<HANDLE>(m_input), static_cast<HANDLE>(m_errors) })
            SetHandleInformation(own, HANDLE_FLAG_INHERIT, 0);

        // Limit inheritance to this child's pipes; other scans may be starting processes concurrently.
        std::array inherited{ childInput, static_cast<HANDLE>(outputGuard), childErrors };
        SIZE_T attributeSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
        std::vector<BYTE> attributeBuffer(attributeSize);
        const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize)) return false;
        const SmartPointer attributeGuard(DeleteProcThreadAttributeList, attributes);
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inherited.data(), sizeof(inherited), nullptr, nullptr)) return false;

        STARTUPINFOEX startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = childInput;
        startup.StartupInfo.hStdOutput = outputGuard;
        startup.StartupInfo.hStdError = childErrors;
        startup.lpAttributeList = attributes;

        const std::wstring executable = GetWslExecutable();
        std::wstring commandLine = QuoteArgument(executable) + L' ' + arguments;
        PROCESS_INFORMATION info{};
        if (!CreateProcess(executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &info))
            return false;
        CloseHandle(info.hThread);
        m_process = info.hProcess;
        return true;
    }

    bool Write(const std::span<const BYTE> data)
    {
        for (size_t offset = 0; offset < data.size();)
        {
            DWORD written = 0;
            const auto chunk = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 1024 * 1024));
            if (!WriteFile(m_input, data.data() + offset, chunk, &written, nullptr) || written == 0) return false;
            offset += written;
        }
        m_input = nullptr;
        return true;
    }

    // Reads standard output; returns 0 at its end, -1 if the read failed, and -2 if pause or cancellation
    // interrupted it.
    int Read(BYTE* buffer, const DWORD size, BlockingQueue<CItem*>* queue = nullptr)
    {
        // Wait in short slices so a silent helper or a full error pipe cannot prevent pause and cancellation;
        // an exited process leaves no output beyond what the pipe holds, which completes a read at once
        OVERLAPPED overlapped{ .hEvent = m_outputReady };
        if (!ReadFile(m_output, buffer, size, nullptr, &overlapped) && GetLastError() != ERROR_IO_PENDING)
            return GetLastError() == ERROR_BROKEN_PIPE ? 0 : -1;
        bool exited = false;
        for (const std::array events{ static_cast<HANDLE>(m_outputReady), static_cast<HANDLE>(m_process) };;)
        {
            const DWORD wait = WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, 50);
            ReadErrors();
            if (wait == WAIT_OBJECT_0) break;
            exited = wait != WAIT_TIMEOUT;
            if (exited || queue != nullptr && queue->IsPauseOrCancelRequested())
            {
                CancelIoEx(m_output, &overlapped);
                break;
            }
        }
        DWORD read = 0;
        if (GetOverlappedResult(m_output, &overlapped, &read, TRUE)) return static_cast<int>(read);
        const DWORD error = GetLastError();
        return error == ERROR_BROKEN_PIPE || exited ? 0 : error == ERROR_OPERATION_ABORTED ? -2 : -1;
    }

    // Drains the error pipe and returns the first 64 KiB that it carried.
    const std::string& ReadErrors()
    {
        std::array<char, 4096> buffer{};
        DWORD available = 0, read = 0;
        if (PeekNamedPipe(m_errors, nullptr, 0, nullptr, &available, nullptr))
            while (available != 0 && ReadFile(m_errors, buffer.data(),
                std::min(available, static_cast<DWORD>(buffer.size())), &read, nullptr) && read != 0)
            {
                m_errorText.append(buffer.data(), std::min<size_t>(read, 64 * 1024 - m_errorText.size()));
                available -= read;
            }
        return m_errorText;
    }

    DWORD Wait(const DWORD timeout)
    {
        DWORD code = STILL_ACTIVE;
        if (WaitForSingleObject(m_process, timeout) == WAIT_OBJECT_0) GetExitCodeProcess(m_process, &code);
        return code;
    }
};

// Picks the embedded helper matching the processor that WSL 2 virtualizes.
static std::span<const BYTE> GetHelperBinary()
{
    USHORT processMachine = 0, nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    if (const auto isWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
        GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process2"));
        isWow64Process2 == nullptr || !isWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
        return {};

    const UINT id = nativeMachine == IMAGE_FILE_MACHINE_ARM64 ? IDR_WSLSCAN_ARM64 :
        nativeMachine == IMAGE_FILE_MACHINE_AMD64 ? IDR_WSLSCAN_X64 : 0;
    if (id == 0) return {};
    const HRSRC resource = FindResource(nullptr, MAKEINTRESOURCE(id), RT_RCDATA);
    const auto data = resource != nullptr ?
        static_cast<const BYTE*>(LockResource(LoadResource(nullptr, resource))) : nullptr;
    return data != nullptr ? std::span(data, SizeofResource(nullptr, resource)) : std::span<const BYTE>{};
}

template <typename T> static T ReadValue(const BYTE* data) noexcept
{
    T value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

static FILETIME ToFileTime(const LONGLONG seconds, const ULONG nanoseconds) noexcept
{
    constexpr LONGLONG epochDifference = 11644473600ll;
    if (seconds < -epochDifference) return {};
    const auto ticks = static_cast<ULONGLONG>(seconds + epochDifference) * 10000000ull + nanoseconds / 100;
    return std::bit_cast<FILETIME>(ticks);
}

static std::wstring LinuxErrorText(const int error)
{
    // Linux and the Windows C runtime share error numbers only up to ERANGE (34)
    std::array<wchar_t, 128> text{};
    if (error > 0 && error < 35 && _wcserror_s(text.data(), text.size(), error) == 0) return text.data();
    return std::format(L"Linux error {}", error);
}

bool FinderWsl::IsPath(const std::wstring_view path) noexcept
{
    const auto rest = AfterPrefix(path);
    return rest.has_value() && !rest->empty() && rest->front() != L'\\';
}

std::wstring FinderWsl::GetDistroName(const std::wstring_view path)
{
    const auto rest = AfterPrefix(path);
    if (!rest) return {};
    return std::wstring(rest->substr(0, rest->find(L'\\')));
}

std::wstring FinderWsl::GetPathKey(const std::wstring_view path)
{
    const auto rest = AfterPrefix(path);
    if (!rest || rest->empty() || rest->front() == L'\\') return {};

    // Share aliases and distribution names ignore case; names inside Linux keep their exact case.
    const auto separator = rest->find(L'\\');
    std::wstring key = GetRootPath(MakeLower(std::wstring(rest->substr(0, separator))));
    if (separator != rest->npos) key.append(rest->substr(separator));
    return key;
}

std::wstring FinderWsl::ExpandShortNames(std::wstring path)
{
    // A registration and a scan can name one disk through different 8.3 aliases, which always contain a tilde
    if (!path.contains(L'~')) return path;
    const DWORD size = GetLongPathName(path.c_str(), nullptr, 0);
    std::wstring expanded(size, L'\0');
    const DWORD length = size == 0 ? 0 : GetLongPathName(path.c_str(), expanded.data(), size);
    if (length == 0 || length >= size) return path;
    expanded.resize(length);
    return expanded;
}

std::vector<WslDistro> FinderWsl::GetDistros()
{
    std::vector<WslDistro> distros;
    HKEY lxss = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss",
        0, KEY_READ, &lxss) != ERROR_SUCCESS) return distros;
    const SmartPointer lxssGuard(RegCloseKey, lxss);

    const auto readString = [](const HKEY key, const wchar_t* name)
    {
        std::array<wchar_t, 1024> buffer{};
        DWORD size = sizeof(buffer) - sizeof(wchar_t);
        if (RegGetValue(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr,
            buffer.data(), &size) != ERROR_SUCCESS) return std::wstring();
        return std::wstring(buffer.data());
    };

    std::array<wchar_t, 256> subkey{};
    for (DWORD i = 0;; ++i)
    {
        DWORD length = static_cast<DWORD>(subkey.size());
        if (RegEnumKeyEx(lxss, i, subkey.data(), &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY distroKey = nullptr;
        if (RegOpenKeyEx(lxss, subkey.data(), 0, KEY_READ, &distroKey) != ERROR_SUCCESS) continue;
        const SmartPointer distroGuard(RegCloseKey, distroKey);

        // WSL 1 keeps its files on NTFS, where the regular scan already sees them.
        DWORD version = 0, size = sizeof(version);
        if (RegGetValue(distroKey, nullptr, L"Version", RRF_RT_REG_DWORD, nullptr, &version, &size) != ERROR_SUCCESS ||
            version < 2) continue;

        WslDistro distro{ readString(distroKey, L"DistributionName"), {} };
        std::wstring basePath(StripLongPrefix(readString(distroKey, L"BasePath")));
        std::wstring vhdName = readString(distroKey, L"VhdFileName");
        if (distro.name.empty() || basePath.empty()) continue;
        if (vhdName.empty()) vhdName = L"ext4.vhdx";
        while (basePath.ends_with(L'\\')) basePath.pop_back();
        distro.vhdPath = ExpandShortNames(basePath + L'\\' + vhdName);
        distros.emplace_back(std::move(distro));
    }
    return distros;
}

std::optional<WslDistro> FinderWsl::FindDistroByVhd(const std::wstring_view path)
{
    const auto target = ExpandShortNames(std::wstring(StripLongPrefix(path)));
    for (auto& distro : GetDistros())
        if (EqualsNoCase(distro.vhdPath, target)) return std::move(distro);
    return std::nullopt;
}

std::optional<WslDistro> FinderWsl::FindDistroByName(const std::wstring_view name)
{
    for (auto& distro : GetDistros())
        if (EqualsNoCase(distro.name, name)) return std::move(distro);
    return std::nullopt;
}

std::vector<std::wstring> FinderWsl::GetRunningDistros()
{
    // Asking wsl.exe without a registered distribution could offer to install WSL.
    if (GetDistros().empty()) return {};

    WslProcess process;
    if (!process.Start(L"--list --running --quiet", 0)) return {};
    process.Write({});

    // The names fit in the output pipe, so waiting first bounds a stuck wsl.exe that would block a read
    if (process.Wait(10000) != 0) return {};
    std::string output;
    std::array<BYTE, 4096> buffer{};
    for (int read; (read = process.Read(buffer.data(), static_cast<DWORD>(buffer.size()))) > 0;)
        output.append(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(read));

    std::vector<std::wstring> names;
    for (const auto line : std::views::split(DecodeConsoleText(output), L'\n'))
    {
        std::wstring name(line.begin(), line.end());
        while (!name.empty() && iswspace(name.back())) name.pop_back();
        while (!name.empty() && iswspace(name.front())) name.erase(0, 1);
        if (!name.empty()) names.emplace_back(std::move(name));
    }
    return names;
}

bool FinderWsl::IsDistroRunning(const std::wstring_view name)
{
    return std::ranges::any_of(GetRunningDistros(), [&](const auto& running) { return EqualsNoCase(running, name); });
}

// --- Tree loading ---

std::unique_ptr<FinderWslContext::Tree> FinderWslContext::Load(const std::wstring& path,
    BlockingQueue<CItem*>* queue, std::wstring& error)
{
    const auto rest = AfterPrefix(path);
    const auto helper = GetHelperBinary();
    if (!rest || helper.empty())
    {
        error = TranslateError(ERROR_NOT_SUPPORTED);
        return nullptr;
    }
    const std::wstring distro = FinderWsl::GetDistroName(path);
    const auto separator = rest->find(L'\\');
    const std::string linuxPath = ToLinuxPath(separator == std::wstring_view::npos ? L"" : rest->substr(separator));

    // Copy the helper from standard input into the first temporary directory that permits execution;
    // exit code 126 from the shell means a directory refused to run it, so the next one is tried.
    constexpr std::wstring_view script =
        L"s=; for d in \"${TMPDIR:-/tmp}\" /dev/shm /run /root; do "
        L"f=$(mktemp \"$d/wds-wsl-scan.XXXXXX\" 2>/dev/null) || continue; "
        L"if [ -z \"$s\" ]; then cat >\"$f\" || { rm -f \"$f\"; exit 125; }; s=$f; "
        L"else cp \"$s\" \"$f\" || { rm -f \"$f\"; continue; }; fi; "
        L"chmod 700 \"$f\"; \"$f\" --self-delete \"$1\"; r=$?; "
        L"if [ $r -ne 126 ]; then rm -f \"$s\" \"$f\"; exit $r; fi; "
        L"if [ \"$f\" != \"$s\" ]; then rm -f \"$f\"; fi; done; rm -f \"$s\"; exit 126";
    const std::wstring arguments = std::format(L"--distribution {} --user root --exec /bin/sh -c {} wds-wsl-scan {}",
        QuoteArgument(distro), QuoteArgument(script), QuoteArgument(Utf8ToWide(linuxPath)));

    WslProcess process;
    if (!process.Start(arguments, static_cast<DWORD>(helper.size()) + 4096) || !process.Write(helper))
    {
        error = TranslateError();
        return nullptr;
    }

    auto tree = std::make_unique<Tree>();
    std::unordered_set<ULONGLONG> directories;
    std::vector<BYTE> pending;
    std::vector<BYTE> buffer(1024 * 1024);
    bool headerSeen = false, ended = false, rootSeen = false;
    std::string preamble;

    for (;;)
    {
        const int read = process.Read(buffer.data(), static_cast<DWORD>(buffer.size()), queue);
        if (read == -1)
        {
            error = TranslateError();
            return nullptr;
        }
        if (read < 0)
        {
            // A stopping scan cancels reads before it asks workers to wait; stopping throws from here.
            queue->WaitIfSuspended();
            continue;
        }
        if (read == 0) break;
        if (queue->IsPauseOrCancelRequested()) queue->WaitIfSuspended();
        pending.insert(pending.end(), buffer.data(), buffer.data() + read);

        size_t offset = 0;
        if (!headerSeen)
        {
            if (pending.size() < sizeof(WslStreamHeader)) continue;
            const auto header = ReadValue<WslStreamHeader>(pending.data());
            if (std::memcmp(header.magic, WslStreamHeaderValue.magic, sizeof(header.magic)) != 0)
            {
                // Not the helper speaking: keep the text for the error message.
                preamble.assign(pending.begin(), pending.begin() + std::min<size_t>(pending.size(), 4096));
                break;
            }
            if (header.version != WslStreamHeaderValue.version)
            {
                error = TranslateError(ERROR_INVALID_DATA);
                return nullptr;
            }
            headerSeen = true;
            offset = sizeof(header);
        }

        while (offset < pending.size())
        {
            if (ended)
            {
                error = TranslateError(ERROR_INVALID_DATA);
                return nullptr;
            }
            const BYTE* record = pending.data() + offset;
            const size_t available = pending.size() - offset;
            if (record[0] == WslKindEntry)
            {
                if (available < sizeof(WslEntryRecord)) break;
                const auto wire = ReadValue<WslEntryRecord>(record);
                const auto nameLength = wire.nameLength;
                if (available < sizeof(wire) + nameLength) break;

                // Directory identities must be unique: renames during traversal can otherwise create a cyclic tree.
                const std::string_view name(reinterpret_cast<const char*>(record + sizeof(wire)), nameLength);
                if (wire.type > WslTypeOther || (wire.flags & ~WslFlagMount) != 0 ||
                    wire.inode == 0 || wire.inode == FinderWsl::MountIndex || wire.nanoseconds >= 1000000000 ||
                    (!rootSeen ?
                        wire.type != WslTypeDirectory || wire.flags != 0 || wire.parent != 0 || nameLength != 0 :
                        wire.parent == 0 || nameLength == 0 || nameLength > 255 || name == "." || name == ".." ||
                        name.contains('/') || name.contains('\0')) ||
                    (wire.type == WslTypeDirectory && wire.flags == 0 && !directories.insert(wire.inode).second))
                {
                    error = TranslateError(ERROR_INVALID_DATA);
                    return nullptr;
                }

                Record entry;
                entry.type = wire.type;
                entry.flags = wire.flags;
                entry.inode = wire.inode;
                entry.size = wire.size;
                entry.allocated = wire.allocated;
                entry.modified = ToFileTime(wire.seconds, wire.nanoseconds);
                offset += sizeof(wire) + nameLength;

                if (!rootSeen)
                {
                    // The first entry is the root itself.
                    rootSeen = true;
                    tree->rootInode = entry.inode;
                    tree->children.try_emplace(entry.inode);
                    continue;
                }

                entry.nameOffset = tree->names.size();
                AppendLinuxName(tree->names, name);
                entry.nameLength = static_cast<USHORT>(tree->names.size() - entry.nameOffset);
                if (entry.type == WslTypeDirectory && (entry.flags & WslFlagMount) == 0)
                    tree->children.try_emplace(entry.inode);
                tree->children[wire.parent].push_back(static_cast<ULONG>(tree->records.size()));
                tree->records.push_back(entry);
            }
            else if (record[0] == WslKindFailure || record[0] == WslKindEnd)
            {
                if (available < sizeof(WslShortRecord)) break;
                const auto wire = ReadValue<WslShortRecord>(record);
                if (wire.kind == WslKindFailure)
                    tree->failures.try_emplace(wire.value, LinuxErrorText(static_cast<int>(wire.error)));
                else
                {
                    if (wire.error != 0 || wire.value != tree->records.size() + 1)
                    {
                        error = TranslateError(ERROR_INVALID_DATA);
                        return nullptr;
                    }
                    ended = true;
                }
                offset += sizeof(wire);
            }
            else
            {
                error = TranslateError(ERROR_INVALID_DATA);
                return nullptr;
            }
        }
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    const std::wstring errors = DecodeConsoleText(process.ReadErrors());
    const DWORD exitCode = process.Wait(5000);
    if (!rootSeen)
    {
        if (!preamble.empty()) error = DecodeConsoleText(preamble);
        else if (!errors.empty()) error = errors;
        else error = std::format(L"wsl.exe exit code {}", exitCode);
        return nullptr;
    }
    if (!ended || exitCode != 0)
        tree->result = { ScanOutcome::Partial, errors.empty() ? TranslateError(ERROR_INVALID_DATA) : errors };
    return tree;
}

const FinderWslContext::Tree* FinderWslContext::Resolve(CItem* item, BlockingQueue<CItem*>* queue)
{
    const ULONGLONG index = item->GetIndex();
    {
        // A loaded tree covers this folder if it is the tree's root or one of its folders.
        std::shared_lock lock(m_lock);
        for (const CItem* ancestor = item;; ancestor = ancestor->GetParent())
        {
            if (const auto found = m_trees.find(ancestor); found != m_trees.end())
            {
                // A tree whose helper failed keeps all of its folders on the share
                if (found->second == nullptr) return nullptr;
                if (ancestor == item || index == FinderWsl::MountIndex || found->second->children.contains(index))
                    return found->second.get();
                break;
            }
            if (!ancestor->IsTypeOrFlag(ITF_WSL) || ancestor->GetParent() == nullptr) break;
        }
    }
    // A mount point stays unentered when it is refreshed on its own, as it was when it was first listed
    static const Tree unentered;
    if (index == FinderWsl::MountIndex && !item->IsWslRoot()) return &unentered;

    // A failure is recorded for this tree alone, so other distributions in the scan still use the helper
    std::wstring error;
    auto tree = Load(item->GetPath(), queue, error);
    if (tree != nullptr) item->SetIndex(tree->rootInode);
    else if (!item->IsScanRoot())
    {
        // A share cannot preserve native hardlink identities during partial refreshes or the mount boundaries
        // of a distribution shown in place of its virtual disk, so these failures stay incomplete.
        tree = std::make_unique<Tree>();
        tree->rootInode = index;
        tree->result = { ScanOutcome::EnumerationError, error };
    }
    else VTRACE(L"WSL helper unavailable for {}: {}", item->GetPath(), error);
    std::unique_lock lock(m_lock);
    return m_trees.insert_or_assign(item, std::move(tree)).first->second.get();
}

FinderBasicContext* FinderWslContext::GetShareContext(const CItem* root)
{
    std::unique_lock lock(m_lock);
    return &m_shareContexts.try_emplace(root, root->GetPath()).first->second;
}

// --- Enumeration ---

bool FinderWsl::Resolve(CItem* item, BlockingQueue<CItem*>* queue)
{
    m_tree = m_master->Resolve(item, queue);
    return m_tree != nullptr;
}

bool FinderWsl::FindFile(const CItem* item)
{
    m_base = item->GetPath();
    m_result = { ScanOutcome::Complete, {} };
    m_position = m_end = nullptr;

    const ULONGLONG key = item->GetIndex();
    if (key == MountIndex) return false;
    if (key == m_tree->rootInode && m_tree->result.outcome != ScanOutcome::Complete) m_result = m_tree->result;
    if (const auto failure = m_tree->failures.find(key); failure != m_tree->failures.end())
        m_result = { ScanOutcome::EnumerationError, m_base + L": " + failure->second };

    const auto children = m_tree->children.find(key);
    if (children == m_tree->children.end() || children->second.empty()) return false;
    m_position = children->second.data();
    m_end = m_position + children->second.size();
    return FindNext();
}

bool FinderWsl::FindNext()
{
    if (m_position == m_end) return false;
    m_record = &m_tree->records[*m_position++];
    return true;
}

DWORD FinderWsl::GetAttributes() const
{
    return m_record->type == WslTypeDirectory ? FILE_ATTRIBUTE_DIRECTORY :
        m_record->type == WslTypeSymlink ? FILE_ATTRIBUTE_REPARSE_POINT : FILE_ATTRIBUTE_NORMAL;
}

DWORD FinderWsl::GetReparseTag() const
{
    return m_record->type == WslTypeSymlink ? IO_REPARSE_TAG_SYMLINK : 0;
}

ULONGLONG FinderWsl::GetIndex() const
{
    if ((m_record->flags & WslFlagMount) != 0)
        return m_record->type == WslTypeDirectory ? MountIndex : 0;
    return m_record->inode;
}

std::wstring FinderWsl::GetFilePath() const
{
    std::wstring path = m_base;
    if (!path.ends_with(L'\\')) path += L'\\';
    return path.append(GetFileName());
}
