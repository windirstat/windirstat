// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "RecoveryShared.h"

using TestHandle = SmartPointer<HANDLE, decltype(&CloseHandle)>;
int failures = 0;
int checks = 0;

void Require(const bool condition, const std::string_view message)
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

void WinCheck(const bool success, const std::string_view operation)
{
    if (!success) throw std::runtime_error(std::format("{}: Windows error {}", operation, GetLastError()));
}

void Test(const std::string_view name, const std::function<void()>& body)
{
    try
    {
        body();
        std::cout << "PASS " << name << std::endl;
    }
    catch (const RecoveryShared::Failure& failure)
    {
        ++failures;
        std::cout << "FAIL " << name << ": recovery error " << failure.error;
        std::wcout << L" " << failure.message << std::endl;
    }
    catch (const std::exception& failure)
    {
        ++failures;
        std::cout << "FAIL " << name << ": " << failure.what() << std::endl;
    }
}

void MakeDirectory(const std::wstring& path)
{
    WinCheck(CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS, "CreateDirectory");
}

std::vector<BYTE> Pattern(const size_t size, DWORD seed)
{
    // Distinct nonzero payloads expose misplaced, overwritten and zero-filled reads.
    std::vector<BYTE> bytes(size);
    for (auto& byte : bytes)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        byte = static_cast<BYTE>(seed);
    }
    return bytes;
}

void WriteBytes(const std::wstring& path, const std::span<const BYTE> bytes)
{
    const TestHandle file(CloseHandle, CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    WinCheck(file.IsValid(), "CreateFile");
    DWORD written = 0;
    WinCheck(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr), "WriteFile");
    Require(written == bytes.size(), "complete fixture write");
    const FILETIME time{ 0xD09AC000, 0x01DA3C45 };
    WinCheck(SetFileTime(file, &time, nullptr, &time), "SetFileTime");
    WinCheck(FlushFileBuffers(file), "FlushFileBuffers");
}

std::vector<BYTE> ReadBytes(const std::wstring& path)
{
    const TestHandle file(CloseHandle, CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr));
    WinCheck(file.IsValid(), "open recovered file");
    LARGE_INTEGER size{};
    WinCheck(GetFileSizeEx(file, &size), "GetFileSizeEx");
    Require(size.QuadPart >= 0 && size.QuadPart < 1024 * 1024 * 1024, "bounded recovered length");
    std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    WinCheck(ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr), "ReadFile");
    Require(read == bytes.size(), "complete verification read");
    return bytes;
}

void FlushVolume(const std::wstring& source)
{
    // Flush filesystem metadata before the readers inspect the raw volume.
    wchar_t volume[MAX_PATH]{};
    WinCheck(GetVolumeNameForVolumeMountPointW(source.c_str(), volume, MAX_PATH), "volume name");
    std::wstring name(volume);
    name.pop_back();
    const TestHandle handle(CloseHandle, CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
    WinCheck(handle.IsValid(), "open volume to flush");
    WinCheck(FlushFileBuffers(handle), "flush volume");
}

struct Fixture
{
    std::string label;
    std::wstring path;
    std::wstring name;
    std::wstring outputName;
    std::vector<BYTE> bytes;
    bool expected = true;
    std::array<FILETIME, 2> times{};
};

LONGLONG FirstCluster(const HANDLE file, DWORD& extentCount)
{
    // Ask the filesystem for the fixture's actual allocation instead of assuming fragmentation.
    STARTING_VCN_INPUT_BUFFER start{};
    std::vector<BYTE> bytes(64 * 1024);
    DWORD returned = 0;
    WinCheck(DeviceIoControl(file, FSCTL_GET_RETRIEVAL_POINTERS, &start, sizeof(start), bytes.data(),
        static_cast<DWORD>(bytes.size()), &returned, nullptr), "fixture retrieval pointers");
    const auto& mapping = *reinterpret_cast<const RETRIEVAL_POINTERS_BUFFER*>(bytes.data());
    extentCount = mapping.ExtentCount;
    Require(extentCount != 0, "fixture has allocated extents");
    return mapping.Extents[0].Lcn.QuadPart;
}

LONGLONG FreeCluster(const HANDLE volume, const DWORD totalClusters)
{
    // Large-cluster fixtures can occupy most of a small test volume; choose an actually free destination.
    STARTING_LCN_INPUT_BUFFER start{ .StartingLcn = { .QuadPart = totalClusters / 4 * 3 } };
    std::vector<BYTE> bytes(64 * 1024);
    DWORD returned = 0;
    const bool complete = DeviceIoControl(volume, FSCTL_GET_VOLUME_BITMAP, &start, sizeof(start), bytes.data(),
        static_cast<DWORD>(bytes.size()), &returned, nullptr) != 0;
    WinCheck(complete || GetLastError() == ERROR_MORE_DATA, "fixture allocation bitmap");
    const auto& bitmap = *reinterpret_cast<const VOLUME_BITMAP_BUFFER*>(bytes.data());
    const auto origin = bitmap.StartingLcn.QuadPart;
    const auto count = std::min<LONGLONG>(bitmap.BitmapSize.QuadPart,
        (returned - offsetof(VOLUME_BITMAP_BUFFER, Buffer)) * 8);
    for (auto bit = start.StartingLcn.QuadPart - origin; bit < count; ++bit)
        if ((bitmap.Buffer[bit / 8] & (1 << (bit % 8))) == 0) return origin + bit;
    throw std::runtime_error("no free cluster for fragmentation fixture");
}

int wmain(const int argc, wchar_t** argv)
{
    if (argc == 2 && std::wstring_view(argv[1]) == L"--filter-stress")
    {
        Test("deep-path-regex", []
        {
            const auto path = std::wstring(30000, L'd') + L"\\maximum Unicode 界\U0001F680 name.bin";
            for (const auto pattern : { LR"(.*\.bin$)", LR"(d+.*name)", LR"(.*界.*)", LR"(^.*$)" })
                Require(std::regex_search(path, std::wregex(pattern, std::regex::icase | std::regex::optimize)),
                    "deep paths support recovery regex filters");
        });
        return failures == 0 ? 0 : 1;
    }
    if (argc == 4 && std::wstring_view(argv[1]) == L"--disk-full")
    {
        Test("disk-full", [&]
        {
            // Use a new directory on the small destination volume to verify failure leaves no output.
            Require(GetFileAttributesW(argv[3]) == INVALID_FILE_ATTRIBUTES, "disk-full output folder must be new");
            MakeDirectory(argv[3]);
            RecoveryShared::Progress progress;
            auto reader = RecoveryShared::Open(argv[2], &progress);
            RecoveryShared::ScanResult scan;
            reader->Scan(progress, scan);
            ULARGE_INTEGER available{};
            WinCheck(GetDiskFreeSpaceExW(argv[3], &available, nullptr, nullptr), "destination free space");
            const auto candidate = std::ranges::find_if(scan.records, [&](const RecoveryShared::Record& record)
            {
                return record.data.size > available.QuadPart;
            });
            Require(candidate != scan.records.end(), "deleted candidate exceeds the small destination volume");
            bool rejected = false;
            try { reader->Recover(*candidate, argv[3], progress); }
            catch (const RecoveryShared::Failure& failure) { rejected = failure.error == ERROR_DISK_FULL; }
            Require(rejected, "insufficient space is reported as a disk-full failure");
            Require(std::filesystem::is_empty(argv[3]), "disk-full recovery leaves no temporary or final output");
        });
        return failures == 0 ? 0 : 1;
    }
    if (argc != 3)
    {
        std::cout << "Usage: RecoveryTests.exe <disposable-volume-root> <new-output-folder>" << std::endl;
        return 2;
    }
    try
    {
        // Moving NTFS allocations requires the administrator token's volume-management privilege to be enabled.
        TestHandle token(CloseHandle);
        WinCheck(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token), "open token");
        TOKEN_PRIVILEGES privileges{ .PrivilegeCount = 1 };
        WinCheck(LookupPrivilegeValueW(nullptr, SE_MANAGE_VOLUME_NAME, &privileges.Privileges[0].Luid),
            "lookup volume-management privilege");
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        WinCheck(AdjustTokenPrivileges(token, false, &privileges, sizeof(privileges), nullptr, nullptr) &&
            GetLastError() != ERROR_NOT_ALL_ASSIGNED, "enable volume-management privilege");
        const std::wstring source = argv[1];
        const std::wstring outputRoot = argv[2];
        Require(source.ends_with(L"\\"), "source is a volume root or mount point");
        Require(GetFileAttributesW(outputRoot.c_str()) == INVALID_FILE_ATTRIBUTES, "output folder must be new");
        MakeDirectory(outputRoot);
        wchar_t fs[MAX_PATH]{};
        DWORD sectors = 0, sectorSize = 0, freeClusters = 0, totalClusters = 0;
        WinCheck(GetVolumeInformationW(source.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fs, MAX_PATH),
            "GetVolumeInformation");
        WinCheck(GetDiskFreeSpaceW(source.c_str(), &sectors, &sectorSize, &freeClusters, &totalClusters),
            "GetDiskFreeSpace");
        const size_t cluster = size_t(sectors) * sectorSize;
        const bool ntfs = _wcsicmp(fs, L"NTFS") == 0;
        const bool exfat = _wcsicmp(fs, L"exFAT") == 0;
        const bool fat = !ntfs && !exfat;
        std::wcout << L"Filesystem " << fs << L", cluster bytes " << cluster << std::endl;

        // Construct every fixture before deletion so test setup does not reuse saved records or data clusters.
        const auto base = source.starts_with(L"\\\\?\\") ? source + L"RecoveryAudit" :
            L"\\\\?\\" + source + L"RecoveryAudit";
        Require(GetFileAttributesW(base.c_str()) == INVALID_FILE_ATTRIBUTES, "fixture folder must be new");
        MakeDirectory(base);
        std::vector<Fixture> fixtures;
        const auto add = [&](const std::string& label, const std::wstring& directory, const std::wstring& name,
            const size_t size, const bool expected = true, const std::wstring& outputName = L"")
        {
            Fixture fixture{ label, directory + L"\\" + name, name, outputName.empty() ? name : outputName,
                Pattern(size, 0x71AABC23 + static_cast<DWORD>(fixtures.size())), expected };
            WriteBytes(fixture.path, fixture.bytes);
            fixtures.push_back(std::move(fixture));
        };
        add("empty", base, L"empty recovery.bin", 0);
        add("small", base, L"small recovery.bin", 123);
        add("one-cluster", base, L"one cluster recovery.bin", cluster);
        add("cluster-plus-one", base, L"cluster plus one.bin", cluster + 1, !fat);
        add("multi-cluster", base, L"multiple clusters.bin", cluster * 17 + 137, !fat);
        const auto copySize = std::min<size_t>(3 * 1024 * 1024 + 13, size_t(freeClusters) * cluster / 8);
        add("copy-boundary", base, L"copy buffer boundary.bin", copySize, !fat || copySize <= cluster);
        add("unicode", base, L"日本語 العربية Кириллица e\u0301 é \U0001F680 \U0001F9D1\u200D\U0001F4BB.bin", 319);
        std::wstring longest;
        while (longest.size() < 249) longest += L"界é\u0301\U0001F680";
        longest.resize(249);
        if (IS_HIGH_SURROGATE(longest.back())) longest.back() = L'界';
        longest += L"界界.bin";
        Require(longest.size() == 255, "maximum filename has 255 UTF-16 code units");
        add("max-unicode-name", base, longest, 71);
        const auto maxDirectory = base + L"\\" + std::wstring(255, L'界');
        MakeDirectory(maxDirectory);
        add("max-directory-name", maxDirectory, L"maximum directory child.bin", 97);
        add("trailing-dot-space", base, L"trailing filename. ", 57, true, L"trailing filename");
        if (ntfs) add("reserved-device", base, L"NUL.txt", 91, true, L"_NUL.txt");

        // Deep ancestry uses iterative filesystem operations and approaches the extended path limit.
        auto deep = base;
        size_t depth = 0;
        const size_t maximumDepth = freeClusters > 48 ? freeClusters - 48 : 0;
        while (deep.size() < 30000 && depth < maximumDepth)
        {
            deep += std::format(L"\\层{:04}-é\u0301-\U0001F680-deep-directory-component", depth++);
            MakeDirectory(deep);
        }
        add("deep-source", deep, L"very deep recovered file.bin", 119);
        add("deep-max-name", deep, longest, 83);
        std::cout << "Deep fixture levels " << depth << ", path code units " << deep.size() << std::endl;

        // Recycle Bin metadata must restore the original basename and path after both companions are deleted.
        const auto recycle = (source.starts_with(L"\\\\?\\") ? source : L"\\\\?\\" + source) + L"$Recycle.Bin";
        MakeDirectory(recycle);
        const auto sid = recycle + L"\\S-1-5-21-111-222-333-1001";
        MakeDirectory(sid);
        const std::wstring original = L"C:\\Original folder\\回収 \U0001F680 original name.bin";
        std::vector<BYTE> info(28 + (original.size() + 1) * sizeof(wchar_t), 0);
        const ULONGLONG version = 2, originalSize = 187;
        const DWORD nameLength = static_cast<DWORD>(original.size() + 1);
        std::memcpy(info.data(), &version, sizeof(version));
        std::memcpy(info.data() + 8, &originalSize, sizeof(originalSize));
        std::memcpy(info.data() + 24, &nameLength, sizeof(nameLength));
        std::memcpy(info.data() + 28, original.c_str(), nameLength * sizeof(wchar_t));
        WriteBytes(sid + L"\\$IABC123.bin", info);
        add("recycle-original-name", sid, L"$RABC123.bin", static_cast<size_t>(originalSize), true,
            L"回収 \U0001F680 original name.bin");

        if (ntfs)
        {
            add("uninitialized-tail", base, L"uninitialized tail.bin", cluster * 3 + 11);
            auto& fixture = fixtures.back();
            const TestHandle file(CloseHandle, CreateFileW(fixture.path.c_str(), GENERIC_WRITE, 0,
                nullptr, OPEN_EXISTING, 0, nullptr));
            WinCheck(file.IsValid(), "open tail fixture");
            LARGE_INTEGER end{ .QuadPart = static_cast<LONGLONG>(cluster * 7 + 19) };
            WinCheck(SetFilePointerEx(file, end, nullptr, FILE_BEGIN) && SetEndOfFile(file), "extend logical EOF");
            fixture.bytes.resize(static_cast<size_t>(end.QuadPart), 0);
            WinCheck(FlushFileBuffers(file), "flush extended file");

            // Sparse holes preserve logical zeros without borrowing data from adjacent physical allocations.
            const auto hole = std::max<size_t>(cluster, 64 * 1024);
            add("sparse", base, L"sparse recovery.bin", hole * 3);
            auto& sparse = fixtures.back();
            const TestHandle sparseFile(CloseHandle, CreateFileW(sparse.path.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr));
            DWORD returned = 0;
            FILE_ZERO_DATA_INFORMATION zero{ .FileOffset = { .QuadPart = static_cast<LONGLONG>(hole) },
                .BeyondFinalZero = { .QuadPart = static_cast<LONGLONG>(hole * 2) } };
            WinCheck(sparseFile.IsValid() && DeviceIoControl(sparseFile, FSCTL_SET_SPARSE, nullptr, 0,
                nullptr, 0, &returned, nullptr) && DeviceIoControl(sparseFile, FSCTL_SET_ZERO_DATA, &zero,
                    sizeof(zero), nullptr, 0, &returned, nullptr), "create sparse hole");
            std::fill(sparse.bytes.begin() + hole, sparse.bytes.begin() + hole * 2, BYTE{});
            DWORD extents = 0;
            FirstCluster(sparseFile, extents);
            Require(extents >= 3, "sparse fixture has data on both sides of a physical hole");
            WinCheck(FlushFileBuffers(sparseFile), "flush sparse file");

            // Compressed data and reparse objects must not be presented as ordinary recoverable byte streams.
            if (cluster <= 4096)
            {
                add("compressed-excluded", base, L"compressed recovery.bin", 256 * 1024, false);
                auto& compressed = fixtures.back();
                std::fill(compressed.bytes.begin(), compressed.bytes.end(), BYTE{});
                const TestHandle compressedFile(CloseHandle, CreateFileW(compressed.path.c_str(),
                    GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
                WORD format = COMPRESSION_FORMAT_DEFAULT;
                DWORD written = 0;
                WinCheck(compressedFile.IsValid() && DeviceIoControl(compressedFile, FSCTL_SET_COMPRESSION,
                    &format, sizeof(format), nullptr, 0, &returned, nullptr) &&
                    WriteFile(compressedFile, compressed.bytes.data(), static_cast<DWORD>(compressed.bytes.size()),
                        &written, nullptr) && FlushFileBuffers(compressedFile), "create compressed fixture");
                Require(written == compressed.bytes.size(), "complete compressed fixture write");
            }
            const std::wstring linkName = L"reparse recovery.bin";
            const auto link = base + L"\\" + linkName;
            WinCheck(CreateSymbolicLinkW(link.c_str(), fixtures[2].path.c_str(), 0), "create reparse fixture");
            fixtures.push_back({ "reparse-excluded", link, linkName, linkName, {}, false });
        }

        // Force a fragmented allocation and overwrite its vacated cluster with an unrelated, later-deleted file.
        add("fragmented", base, L"fragmented recovery.bin", cluster * 4, ntfs);
        const auto fragmented = fixtures.back().path;
        const auto decoyPath = base + L"\\fragmentation decoy.bin";
        WriteBytes(decoyPath, Pattern(cluster, 0x83DA17));
        {
            const TestHandle file(CloseHandle, CreateFileW(fragmented.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
            const TestHandle decoy(CloseHandle, CreateFileW(decoyPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
            wchar_t name[MAX_PATH]{};
            WinCheck(GetVolumeNameForVolumeMountPointW(source.c_str(), name, MAX_PATH), "fragment volume name");
            std::wstring volumeName(name);
            volumeName.pop_back();
            const TestHandle volume(CloseHandle, CreateFileW(volumeName.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
            WinCheck(file.IsValid() && decoy.IsValid() && volume.IsValid(), "open fragmentation fixtures");
            DWORD count = 0, returned = 0;
            const auto first = FirstCluster(file, count);
            Require(count == 1, "fragmentation fixture initially contiguous");
            MOVE_FILE_DATA move{ .FileHandle = file, .StartingVcn = { .QuadPart = 1 },
                .StartingLcn = { .QuadPart = FreeCluster(volume, totalClusters) }, .ClusterCount = 1 };
            WinCheck(DeviceIoControl(volume, FSCTL_MOVE_FILE, &move, sizeof(move), nullptr, 0, &returned, nullptr),
                "fragment fixture");
            FirstCluster(file, count);
            Require(count > 1, "fixture is physically fragmented");
            move.FileHandle = decoy;
            move.StartingVcn.QuadPart = 0;
            move.StartingLcn.QuadPart = first + 1;
            WinCheck(DeviceIoControl(volume, FSCTL_MOVE_FILE, &move, sizeof(move), nullptr, 0, &returned, nullptr),
                "overwrite vacated fixture cluster");
        }
        WinCheck(DeleteFileW(decoyPath.c_str()), "delete fragmentation decoy");
        for (auto& fixture : fixtures)
        {
            {
                const TestHandle file(CloseHandle, CreateFileW(fixture.path.c_str(), FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                WinCheck(file.IsValid(), "read original timestamps");
                WinCheck(GetFileTime(file, &fixture.times[0], nullptr, &fixture.times[1]), "original file times");
            }
            WinCheck(DeleteFileW(fixture.path.c_str()), "delete fixture");
        }
        WinCheck(DeleteFileW((sid + L"\\$IABC123.bin").c_str()), "delete recycle companion");

        // NTFS retains deleted directory records, including the sequence needed to reconstruct deep ancestry.
        if (ntfs)
        {
            Require(deep.starts_with(base + L"\\"), "deleted directory tree stays within the owned fixture folder");
            for (auto directory = deep; directory.size() > base.size(); directory.resize(directory.rfind(L'\\')))
                WinCheck(RemoveDirectoryW(directory.c_str()), "delete deep fixture directory");
        }
        FlushVolume(source);

        // Exercise the exact production readers, then verify complete file bytes and reconstructed ancestry.
        RecoveryShared::Progress progress;
        auto reader = RecoveryShared::Open(source, &progress);
        RecoveryShared::ScanResult scan;
        Test("scan", [&] { reader->Scan(progress, scan); });
        std::cout << "Candidates " << scan.records.size() << ", invalid records " << scan.invalidRecords << std::endl;
        const auto find = [&](const Fixture& fixture) -> const RecoveryShared::Record*
        {
            for (const auto& record : scan.records)
                if ((record.name == fixture.name || record.name == fixture.outputName) &&
                    record.data.size == fixture.bytes.size()) return &record;
            return nullptr;
        };
        for (const auto& fixture : fixtures)
        {
            Test(fixture.label, [&]
            {
                const auto record = find(fixture);
                if (!fixture.expected)
                {
                    Require(record == nullptr, "ambiguous allocation must not be offered as recoverable");
                    return;
                }
                Require(record != nullptr, "deleted fixture is discoverable with its original name");
                if (fixture.label.starts_with("deep-"))
                {
                    const auto relative = fixture.path.substr(base.size() - std::wstring(L"RecoveryAudit").size());
                    Require(record->path.ends_with(relative), "complete deep source path is reconstructed");
                }
                const auto destination = outputRoot + L"\\" + std::wstring(fixture.label.begin(), fixture.label.end());
                MakeDirectory(destination);
                const auto recovered = reader->Recover(*record, destination, progress);
                Require(std::filesystem::path(recovered).filename() == fixture.outputName, "original output filename");
                Require(ReadBytes(recovered) == fixture.bytes, "recovered data matches the complete original payload");
                const TestHandle file(CloseHandle, CreateFileW(recovered.c_str(), FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
                std::array<FILETIME, 2> times{};
                WinCheck(file.IsValid() && GetFileTime(file, &times[0], nullptr, &times[1]), "recovered file times");
                Require(CompareFileTime(&times[0], &fixture.times[0]) == 0, "original creation timestamp is preserved");
                Require(CompareFileTime(&times[1], &fixture.times[1]) == 0,
                    "original modification timestamp is preserved");
            });
        }

        Test("canceled-scan-retains-candidates", [&]
        {
            RecoveryShared::Progress stopped;
            RecoveryShared::ScanResult partial;
            bool canceled = false;
            try
            {
                reader->Scan(stopped, partial, [&](const RecoveryShared::Record& record)
                {
                    if (record.name == fixtures[0].name) stopped.cancel = true;
                });
            }
            catch (const RecoveryShared::Failure& failure) { canceled = failure.error == ERROR_CANCELLED; }
            Require(canceled && !partial.records.empty(), "canceled scan retains validated discoveries");
            const auto empty = std::ranges::find(partial.records, fixtures[0].name, &RecoveryShared::Record::name);
            Require(empty != partial.records.end(), "published candidate remains available after stopping");
            const auto folder = outputRoot + L"\\stopped scan";
            MakeDirectory(folder);
            stopped.cancel = false;
            Require(ReadBytes(reader->Recover(*empty, folder, stopped)).empty(), "stopped scan candidate can recover");
        });

        // Restore full directory metadata after the deliberately interrupted scan.
        reader->Scan(progress, scan);

        const auto candidate = find(fixtures[1]);
        if (candidate)
        {
            Test("deep-destination", [&]
            {
                auto folder = L"\\\\?\\" + outputRoot + L"\\deep destination";
                MakeDirectory(folder);
                size_t depth = 0;
                while (folder.size() < 30000)
                {
                    folder += std::format(L"\\destination-界-\U0001F680-{:04}-component", depth++);
                    MakeDirectory(folder);
                }
                const auto maximum = find(fixtures[7]);
                Require(maximum != nullptr, "maximum filename candidate for deep output");
                const auto recovered = reader->Recover(*maximum, folder, progress);
                Require(ReadBytes(recovered) == fixtures[7].bytes, "maximum filename recovers into deep destination");
            });
            Test("destination-file-rejected", [&]
            {
                const auto file = outputRoot + L"\\destination file";
                WriteBytes(file, Pattern(1, 123));
                bool rejected = false;
                try { reader->Recover(*candidate, file, progress); }
                catch (const RecoveryShared::Failure&) { rejected = true; }
                Require(rejected, "a file cannot be used as a recovery directory");
            });
            Test("destination-alias-resolves-volume", [&]
            {
                const auto alias = outputRoot + L"\\source alias";
                WinCheck(CreateSymbolicLinkW(alias.c_str(), base.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY),
                    "create destination alias");
                const auto canonical = RecoveryShared::ValidateDestination(alias);
                Require(canonical.starts_with(reader->Name()), "destination alias identifies its actual source volume");
                WinCheck(RemoveDirectoryW(alias.c_str()), "remove destination alias");
            });
            Test("existing-output-preserved", [&]
            {
                const auto folder = outputRoot + L"\\collision";
                MakeDirectory(folder);
                const auto file = folder + L"\\" + candidate->name;
                const auto bytes = Pattern(321, 234);
                WriteBytes(file, bytes);
                bool rejected = false;
                try { reader->Recover(*candidate, folder, progress); }
                catch (const RecoveryShared::Failure&) { rejected = true; }
                Require(rejected, "existing output must not be overwritten");
                Require(ReadBytes(file) == bytes, "existing file bytes are preserved");
                Require(!std::filesystem::exists(folder + L"\\" +
                    std::format(L"{}-{}.partial", candidate->number, candidate->sequence)),
                    "failed copy leaves no partial");
            });
            Test("pause-cancel", [&]
            {
                const auto folder = outputRoot + L"\\cancel";
                MakeDirectory(folder);
                RecoveryShared::Progress stopped;
                stopped.paused = true;
                std::jthread cancel([&]
                {
                    Sleep(75);
                    stopped.cancel = true;
                });
                bool canceled = false;
                try { reader->Recover(*candidate, folder, stopped); }
                catch (const RecoveryShared::Failure& failure) { canceled = failure.error == ERROR_CANCELLED; }
                Require(canceled, "paused recovery cancels promptly");
                Require(std::filesystem::is_empty(folder), "canceled recovery leaves no output");
            });
            Test("recreated-source-name", [&]
            {
                WriteBytes(fixtures[1].path, Pattern(777, 345));
                bool retained = false;
                if (ntfs)
                {
                    // A recreated name can use a different MFT record while the deleted version remains intact.
                    const TestHandle file(CloseHandle, CreateFileW(fixtures[1].path.c_str(), FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
                    BY_HANDLE_FILE_INFORMATION information{};
                    WinCheck(file.IsValid() && GetFileInformationByHandle(file, &information), "replacement identity");
                    retained = ((ULONGLONG(information.nFileIndexHigh) << 32 | information.nFileIndexLow) &
                        0x0000FFFFFFFFFFFFull) != candidate->number;
                }
                FlushVolume(source);
                const auto folder = outputRoot + L"\\stale";
                MakeDirectory(folder);
                bool rejected = false;
                try
                {
                    const auto recovered = reader->Recover(*candidate, folder, progress);
                    Require(retained && ReadBytes(recovered) == fixtures[1].bytes,
                        "an intact earlier version recovers its original data after the name is reused");
                }
                catch (const RecoveryShared::Failure&) { rejected = true; }
                Require(retained || rejected, "reallocated file records invalidate the saved candidate");
                if (rejected) Require(std::filesystem::is_empty(folder), "stale candidate leaves no output");
            });
        }

        Test("reallocated-source-cluster-rejected", [&]
        {
            auto record = find(fixtures[2]);
            if (ntfs && record && record->data.runs.empty()) record = find(fixtures[4]);
            Require(record && record->data.runs.size() == 1, "allocation fixture has a saved physical extent");
            const auto live = base + L"\\allocated replacement.bin";
            WriteBytes(live, Pattern(std::max<size_t>(cluster, 4096), 0x198735));
            const TestHandle file(CloseHandle, CreateFileW(live.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
            wchar_t name[MAX_PATH]{};
            WinCheck(GetVolumeNameForVolumeMountPointW(source.c_str(), name, MAX_PATH), "allocation volume name");
            std::wstring volumeName(name);
            volumeName.pop_back();
            const TestHandle volume(CloseHandle, CreateFileW(volumeName.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
            WinCheck(file.IsValid() && volume.IsValid(), "open reallocation fixture");
            DWORD extentCount = 0, returned = 0;
            if (FirstCluster(file, extentCount) != record->data.runs.front().lcn)
            {
                MOVE_FILE_DATA move{ .FileHandle = file, .StartingVcn = {},
                    .StartingLcn = { .QuadPart = record->data.runs.front().lcn }, .ClusterCount = 1 };
                if (!DeviceIoControl(volume, FSCTL_MOVE_FILE, &move, sizeof(move), nullptr, 0, &returned, nullptr))
                {
                    // Subsector moves can be rejected; reserving free space also reuses the candidate's allocation.
                    const auto error = GetLastError();
                    WinCheck(error == ERROR_INVALID_PARAMETER || (cluster < 4096 && error == ERROR_ACCESS_DENIED),
                        "reuse deleted fixture allocation");
                    ULARGE_INTEGER available{};
                    WinCheck(GetDiskFreeSpaceExW(source.c_str(), &available, nullptr, nullptr),
                        "reallocation free space");
                    FILE_ALLOCATION_INFO allocation{ .AllocationSize =
                        { .QuadPart = static_cast<LONGLONG>(available.QuadPart / cluster * cluster) } };
                    WinCheck(SetFileInformationByHandle(file, FileAllocationInfo, &allocation, sizeof(allocation)),
                        "reserve deleted source allocation");
                    FlushVolume(source);
                }
            }
            const auto folder = outputRoot + L"\\reallocated source";
            MakeDirectory(folder);
            bool rejected = false;
            try { reader->Recover(*record, folder, progress); }
            catch (const RecoveryShared::Failure&) { rejected = true; }
            Require(rejected, "a newly allocated source cluster cannot be copied");
            Require(std::filesystem::is_empty(folder), "reused source allocation leaves no output");
        });
        std::cout << "RESULT " << checks << " checks, " << failures << " failures" << std::endl;
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& failure)
    {
        std::cout << "SETUP FAIL " << failure.what() << std::endl;
        return 2;
    }
    catch (const RecoveryShared::Failure& failure)
    {
        std::cout << "SETUP FAIL recovery error " << failure.error << std::endl;
        return 2;
    }
}
