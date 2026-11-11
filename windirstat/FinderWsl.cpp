// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FinderWsl.h"

// --- Distribution and path helpers ---

namespace
{
    constexpr std::wstring_view WslPrefixes[] = { L"\\\\wsl.localhost\\", L"\\\\wsl$\\" };
    constexpr std::wstring_view LongPathPrefix = L"\\\\?\\";

    std::wstring_view StripLongPrefix(std::wstring_view path) noexcept
    {
        if (path.starts_with(LongPathPrefix) && !path.starts_with(L"\\\\?\\UNC\\")) path.remove_prefix(LongPathPrefix.size());
        return path;
    }

    bool EqualsNoCase(const std::wstring_view a, const std::wstring_view b) noexcept
    {
        return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
            b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
    }

    // Returns the remainder of the path after the WSL share prefix, or nothing if it has none.
    std::optional<std::wstring_view> AfterPrefix(const std::wstring_view path) noexcept
    {
        for (const auto prefix : WslPrefixes)
        {
            if (path.size() > prefix.size() && EqualsNoCase(path.substr(0, prefix.size()), prefix))
                return path.substr(prefix.size());
        }
        return std::nullopt;
    }

    // The Plan 9 server shows characters that Windows forbids in names in a private-use range.
    bool IsRemappedCharacter(const char32_t c) noexcept
    {
        return c > 0 && c < 0x20 || c == L'"' || c == L'*' || c == L':' || c == L'<' || c == L'>' ||
            c == L'?' || c == L'\\' || c == L'|';
    }

    // Decodes a Linux name the way \\wsl.localhost shows it: invalid bytes become U+FFFD each.
    void AppendLinuxName(std::vector<wchar_t>& out, const std::string_view name)
    {
        for (size_t i = 0; i < name.size();)
        {
            const auto byte = static_cast<unsigned char>(name[i]);
            char32_t c = 0xFFFD;
            size_t length = 1;
            if (byte < 0x80) c = byte;
            else
            {
                const size_t need = byte >= 0xF0 && byte <= 0xF4 ? 4 : byte >= 0xE0 ? 3 : byte >= 0xC2 && byte < 0xE0 ? 2 : 0;
                if (need != 0 && i + need <= name.size())
                {
                    char32_t value = byte & (0x7F >> need);
                    bool valid = true;
                    for (size_t k = 1; k < need && valid; ++k)
                    {
                        const auto next = static_cast<unsigned char>(name[i + k]);
                        valid = (next & 0xC0) == 0x80;
                        value = value << 6 | (next & 0x3F);
                    }
                    const char32_t minimum = need == 2 ? 0x80 : need == 3 ? 0x800 : 0x10000;
                    if (valid && value >= minimum && value <= 0x10FFFF && (value < 0xD800 || value > 0xDFFF))
                    {
                        c = value;
                        length = need;
                    }
                }
            }
            i += length;

            if (IsRemappedCharacter(c)) c += 0xF000;
            if (c >= 0x10000)
            {
                c -= 0x10000;
                out.push_back(static_cast<wchar_t>(0xD800 + (c >> 10)));
                out.push_back(static_cast<wchar_t>(0xDC00 + (c & 0x3FF)));
            }
            else out.push_back(static_cast<wchar_t>(c));
        }
    }

    // Converts the part of a \\wsl.localhost path after the distribution name to a Linux path.
    std::string ToLinuxPath(const std::wstring_view relative)
    {
        std::wstring mapped(relative);
        for (auto& c : mapped)
        {
            if (c == L'\\') c = L'/';
            else if (c >= 0xF000 && c < 0xF080 && IsRemappedCharacter(c - 0xF000)) c = static_cast<wchar_t>(c - 0xF000);
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

    std::wstring GetWslExecutable()
    {
        // A 32-bit build must reach the native System32 through Sysnative.
        std::wstring windows(MAX_PATH, L'\0');
        windows.resize(GetSystemWindowsDirectory(windows.data(), MAX_PATH));
        BOOL wow64 = FALSE;
        IsWow64Process(GetCurrentProcess(), &wow64);
        return windows + (wow64 ? L"\\Sysnative\\wsl.exe" : L"\\System32\\wsl.exe");
    }

    // Quotes one argument so that CommandLineToArgvW-compatible parsers read it back unchanged.
    std::wstring QuoteArgument(const std::wstring_view argument)
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

    std::wstring Utf8ToWide(const std::string_view text)
    {
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(std::max(size, 0)), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    // wsl.exe reports its own messages in UTF-16 and relays Linux output unchanged.
    std::wstring DecodeConsoleText(const std::string_view text)
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
        SmartPointer<HANDLE, decltype(&CloseHandle)> m_errors{ CloseHandle, nullptr };

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
            SECURITY_ATTRIBUTES inherit{ sizeof(inherit), nullptr, TRUE };
            HANDLE childInput = nullptr, childOutput = nullptr, childErrors = nullptr, handle = nullptr;
            if (!CreatePipe(&childInput, &handle, &inherit, std::max<DWORD>(inputSize, 4096))) return false;
            SmartPointer<HANDLE, decltype(&CloseHandle)> inputGuard(CloseHandle, childInput);
            m_input = handle;
            if (!CreatePipe(&handle, &childOutput, &inherit, 1024 * 1024)) return false;
            SmartPointer<HANDLE, decltype(&CloseHandle)> outputGuard(CloseHandle, childOutput);
            m_output = handle;
            if (!CreatePipe(&handle, &childErrors, &inherit, 64 * 1024)) return false;
            SmartPointer<HANDLE, decltype(&CloseHandle)> errorsGuard(CloseHandle, childErrors);
            m_errors = handle;
            for (HANDLE own : { static_cast<HANDLE>(m_input), static_cast<HANDLE>(m_output), static_cast<HANDLE>(m_errors) })
                SetHandleInformation(own, HANDLE_FLAG_INHERIT, 0);

            // Limit inheritance to this child's pipes; other scans may be starting processes concurrently.
            std::array inherited{ childInput, childOutput, childErrors };
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
            startup.StartupInfo.hStdOutput = childOutput;
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

        // Reads standard output; returns 0 at its end, -1 if the read failed, and -2 if it was cancelled.
        int Read(BYTE* buffer, const DWORD size)
        {
            DWORD read = 0;
            if (ReadFile(m_output, buffer, size, &read, nullptr)) return static_cast<int>(read);
            const DWORD error = GetLastError();
            return error == ERROR_BROKEN_PIPE ? 0 : error == ERROR_OPERATION_ABORTED ? -2 : -1;
        }

        std::wstring ReadErrors()
        {
            std::string text;
            std::array<char, 4096> buffer{};
            DWORD read = 0;
            while (text.size() < 64 * 1024 && ReadFile(m_errors, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0)
                text.append(buffer.data(), read);
            return DecodeConsoleText(text);
        }

        DWORD Wait(const DWORD timeout)
        {
            DWORD code = STILL_ACTIVE;
            if (WaitForSingleObject(m_process, timeout) == WAIT_OBJECT_0) GetExitCodeProcess(m_process, &code);
            return code;
        }
    };

    // Picks the embedded helper matching the processor that WSL 2 virtualizes.
    std::span<const BYTE> GetHelperBinary()
    {
        USHORT processMachine = 0, nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
        if (const auto isWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
            GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process2")); isWow64Process2 != nullptr)
            isWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine);
        if (nativeMachine == IMAGE_FILE_MACHINE_UNKNOWN)
        {
            SYSTEM_INFO info{};
            GetNativeSystemInfo(&info);
            nativeMachine = info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? IMAGE_FILE_MACHINE_ARM64 :
                info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? IMAGE_FILE_MACHINE_AMD64 : processMachine;
        }

        const UINT id = nativeMachine == IMAGE_FILE_MACHINE_ARM64 ? IDR_WSLSCAN_ARM64 :
            nativeMachine == IMAGE_FILE_MACHINE_AMD64 ? IDR_WSLSCAN_X64 : 0;
        if (id == 0) return {};
        const HRSRC resource = FindResource(nullptr, MAKEINTRESOURCE(id), RT_RCDATA);
        const auto data = resource != nullptr ?
            static_cast<const BYTE*>(LockResource(LoadResource(nullptr, resource))) : nullptr;
        return data != nullptr ? std::span(data, SizeofResource(nullptr, resource)) : std::span<const BYTE>{};
    }

    // Stream layout written by wslscan/wds-wsl-scan.c
    constexpr BYTE KindEntry = 1, KindFailure = 2, KindEnd = 3;
    constexpr BYTE TypeDirectory = 1;
    constexpr BYTE FlagMount = 1;
    constexpr size_t StreamHeaderSize = 16, EntryHeaderSize = 60, ShortRecordSize = 16;
    constexpr std::array<BYTE, 8> StreamMagic{ 'W', 'D', 'S', 'W', 'S', 'L', 1, 0 };

    template <typename T> T ReadValue(const BYTE* data) noexcept
    {
        T value;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }

    FILETIME ToFileTime(const LONGLONG seconds, const ULONG nanoseconds) noexcept
    {
        constexpr LONGLONG epochDifference = 11644473600ll;
        if (seconds < -epochDifference) return {};
        const auto ticks = static_cast<ULONGLONG>(seconds + epochDifference) * 10000000ull + nanoseconds / 100;
        return std::bit_cast<FILETIME>(ticks);
    }

    std::wstring LinuxErrorText(const int error)
    {
        std::array<wchar_t, 128> text{};
        if (error > 0 && error < 43 && _wcserror_s(text.data(), text.size(), error) == 0) return text.data();
        return std::format(L"Linux error {}", error);
    }
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
        if (RegGetValue(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, buffer.data(), &size) != ERROR_SUCCESS)
            return std::wstring();
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
        distro.vhdPath = basePath + L'\\' + vhdName;
        distros.emplace_back(std::move(distro));
    }
    return distros;
}

std::optional<WslDistro> FinderWsl::FindDistroByVhd(const std::wstring_view path)
{
    const auto target = StripLongPrefix(path);
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
    std::string output;
    std::array<BYTE, 4096> buffer{};
    for (int read; (read = process.Read(buffer.data(), static_cast<DWORD>(buffer.size()))) > 0;)
        output.append(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(read));
    if (process.Wait(10000) != 0) return {};

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
    if (!rest || helper.empty()) return nullptr;
    const std::wstring distro = FinderWsl::GetDistroName(path);
    const auto separator = rest->find(L'\\');
    const std::string linuxPath = ToLinuxPath(separator == std::wstring_view::npos ? L"" : rest->substr(separator));

    // Copy the helper from standard input into the first temporary directory that permits execution;
    // exit code 126 from the shell means a directory refused to run it, so the next one is tried.
    constexpr std::wstring_view script =
        L"s=; for d in \"${TMPDIR:-/tmp}\" /dev/shm /run /root; do "
        L"f=$(mktemp \"$d/wds-wsl-scan.XXXXXX\" 2>/dev/null) || continue; "
        L"if [ -z \"$s\" ]; then cat >\"$f\" || exit 125; s=$f; else cp \"$s\" \"$f\" || continue; fi; "
        L"chmod 700 \"$f\"; \"$f\" --self-delete \"$1\"; r=$?; rm -f \"$f\"; "
        L"if [ $r -ne 126 ]; then rm -f \"$s\"; exit $r; fi; done; rm -f \"$s\"; exit 126";
    const std::wstring arguments = std::format(L"--distribution {} --user root --exec /bin/sh -c {} wds-wsl-scan {}",
        QuoteArgument(distro), QuoteArgument(script), QuoteArgument(Utf8ToWide(linuxPath)));

    WslProcess process;
    if (!process.Start(arguments, static_cast<DWORD>(helper.size()) + 4096) || !process.Write(helper))
    {
        error = TranslateError();
        return nullptr;
    }

    auto tree = std::make_unique<Tree>();
    std::unordered_set<ULONGLONG> linkedFiles;
    std::vector<BYTE> pending;
    std::vector<BYTE> buffer(1024 * 1024);
    bool headerSeen = false, ended = false, rootSeen = false;
    std::string preamble;

    for (;;)
    {
        const int read = process.Read(buffer.data(), static_cast<DWORD>(buffer.size()));
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
            if (pending.size() < StreamHeaderSize) continue;
            if (!std::equal(StreamMagic.begin(), StreamMagic.end(), pending.begin()))
            {
                // Not the helper speaking: keep the text for the error message.
                preamble.assign(pending.begin(), pending.begin() + std::min<size_t>(pending.size(), 4096));
                break;
            }
            headerSeen = true;
            offset = StreamHeaderSize;
        }

        while (offset < pending.size())
        {
            const BYTE* record = pending.data() + offset;
            const size_t available = pending.size() - offset;
            if (record[0] == KindEntry)
            {
                if (available < EntryHeaderSize) break;
                const auto nameLength = ReadValue<USHORT>(record + 4);
                if (available < EntryHeaderSize + nameLength) break;

                Record entry;
                entry.type = record[1];
                entry.flags = record[2];
                const auto links = ReadValue<ULONG>(record + 8);
                entry.inode = ReadValue<ULONGLONG>(record + 12);
                const auto parent = ReadValue<ULONGLONG>(record + 20);
                entry.size = ReadValue<ULONGLONG>(record + 28);
                entry.allocated = ReadValue<ULONGLONG>(record + 36);
                entry.modified = ToFileTime(ReadValue<LONGLONG>(record + 44), ReadValue<ULONG>(record + 52));
                offset += EntryHeaderSize + nameLength;

                if (!rootSeen)
                {
                    // The first entry is the root itself.
                    rootSeen = true;
                    tree->rootInode = entry.inode;
                    tree->children.try_emplace(entry.inode);
                    continue;
                }

                // Count the blocks of a file with several links once, as du does.
                if (entry.type != TypeDirectory && links > 1 && !linkedFiles.insert(entry.inode).second)
                    entry.allocated = 0;

                entry.nameOffset = tree->names.size();
                AppendLinuxName(tree->names, { reinterpret_cast<const char*>(record + EntryHeaderSize), nameLength });
                entry.nameLength = static_cast<USHORT>(tree->names.size() - entry.nameOffset);
                if (entry.type == TypeDirectory && (entry.flags & FlagMount) == 0) tree->children.try_emplace(entry.inode);
                tree->children[parent].push_back(static_cast<ULONG>(tree->records.size()));
                tree->records.push_back(entry);
            }
            else if (record[0] == KindFailure || record[0] == KindEnd)
            {
                if (available < ShortRecordSize) break;
                if (record[0] == KindFailure)
                    tree->failures.try_emplace(ReadValue<ULONGLONG>(record + 8),
                        LinuxErrorText(static_cast<int>(ReadValue<ULONG>(record + 4))));
                else ended = true;
                offset += ShortRecordSize;
            }
            else
            {
                error = TranslateError(ERROR_INVALID_DATA);
                return nullptr;
            }
        }
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    const std::wstring errors = process.ReadErrors();
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
    {
        // A loaded tree covers this folder if it is the tree's root or one of its folders.
        std::shared_lock lock(m_lock);
        for (const CItem* ancestor = item; ancestor != nullptr; ancestor = ancestor->GetParent())
        {
            if (const auto found = m_trees.find(ancestor); found != m_trees.end())
            {
                if (ancestor == item || item->GetIndex() == FinderWsl::MountIndex ||
                    found->second->children.contains(item->GetIndex())) return found->second.get();
                break;
            }
            if (!ancestor->IsTypeOrFlag(ITF_WSL)) break;
        }
    }
    // A mount point stays unentered when it is refreshed on its own, as it was when it was first listed
    static const Tree unentered;
    if (item->GetIndex() == FinderWsl::MountIndex && !item->IsWslRoot()) return &unentered;
    if (m_unavailable) return nullptr;

    std::wstring error;
    auto tree = Load(item->GetPath(), queue, error);
    if (tree == nullptr)
    {
        // Fall back to the Plan 9 share for the rest of this scan.
        VTRACE(L"WSL helper unavailable for {}: {}", item->GetPath(), error);
        m_unavailable = true;
        return nullptr;
    }

    item->SetIndex(tree->rootInode);
    std::unique_lock lock(m_lock);
    return m_trees.insert_or_assign(item, std::move(tree)).first->second.get();
}

// --- Enumeration ---

bool FinderWsl::FindFile(const CItem* item)
{
    std::shared_lock lock(m_master->m_lock);
    for (const CItem* ancestor = item; ancestor != nullptr; ancestor = ancestor->GetParent())
        if (const auto found = m_master->m_trees.find(ancestor); found != m_master->m_trees.end())
            return FindFile(item, found->second.get());
    return false;
}

bool FinderWsl::FindFile(const CItem* item, const FinderWslContext::Tree* tree)
{
    m_tree = tree;
    m_base = item->GetPath();
    m_result = { ScanOutcome::Complete, {} };
    m_position = m_end = nullptr;

    const ULONGLONG key = item->GetIndex();
    if (key == MountIndex) return false;
    if (key == tree->rootInode && !tree->result.error.empty()) m_result = tree->result;
    if (const auto failure = tree->failures.find(key); failure != tree->failures.end())
        m_result = { ScanOutcome::EnumerationError, m_base + L": " + failure->second };

    const auto children = tree->children.find(key);
    if (children == tree->children.end() || children->second.empty()) return false;
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
    return m_record->type == TypeDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

ULONGLONG FinderWsl::GetIndex() const
{
    return m_record->type == TypeDirectory && (m_record->flags & FlagMount) != 0 ? MountIndex : m_record->inode;
}

std::wstring FinderWsl::GetFilePath() const
{
    std::wstring path = m_base;
    if (!path.ends_with(L'\\')) path += L'\\';
    return path.append(GetFileName());
}
