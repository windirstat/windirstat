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

bool RecoveryFails(const auto& action, const RecoveryShared::Failure expected = {})
{
    try { action(); }
    catch (const RecoveryShared::Failure& failure)
    {
        if ((expected.error && failure.error != expected.error) ||
            (!expected.message.empty() && failure.message != expected.message)) throw;
        return true;
    }
    return false;
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

TestHandle OpenFixture(const std::wstring& path, const std::string_view operation, const DWORD access,
    const DWORD sharing = 0, const DWORD creation = OPEN_EXISTING, const DWORD flags = 0)
{
    TestHandle file(CloseHandle, CreateFileW(path.c_str(), access, sharing, nullptr, creation, flags, nullptr));
    WinCheck(file.IsValid(), operation);
    return file;
}

std::array<FILETIME, 2> ReadFileTimes(const HANDLE file)
{
    std::array<FILETIME, 2> times{};
    WinCheck(GetFileTime(file, &times[0], nullptr, &times[1]), "read fixture creation and modification times");
    return times;
}

void WriteBytes(const std::wstring& path, const std::span<const BYTE> bytes)
{
    const auto file = OpenFixture(path, "CreateFile", GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL);
    DWORD written = 0;
    WinCheck(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr), "WriteFile");
    Require(written == bytes.size(), "complete fixture write");
    const FILETIME time{ 0xD09AC000, 0x01DA3C45 };
    WinCheck(SetFileTime(file, &time, nullptr, &time), "SetFileTime");
    WinCheck(FlushFileBuffers(file), "FlushFileBuffers");
}

std::vector<BYTE> ReadBytes(const std::wstring& path)
{
    const auto file = OpenFixture(path, "open recovered file", GENERIC_READ, FILE_SHARE_READ);
    LARGE_INTEGER size{};
    WinCheck(GetFileSizeEx(file, &size), "GetFileSizeEx");
    Require(size.QuadPart >= 0 && size.QuadPart < 1024 * 1024 * 1024, "bounded recovered length");
    std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    WinCheck(ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr), "ReadFile");
    Require(read == bytes.size(), "complete verification read");
    return bytes;
}

TestHandle OpenVolume(const std::wstring& source)
{
    // Open the volume itself by removing the mount point's trailing separator.
    wchar_t volume[MAX_PATH]{};
    WinCheck(GetVolumeNameForVolumeMountPointW(source.c_str(), volume, MAX_PATH), "volume name");
    std::wstring name(volume);
    name.pop_back();
    return OpenFixture(name, "open volume", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
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
            Require(RecoveryFails([&] { reader->Recover(*candidate, argv[3], progress); },
                { .error = ERROR_DISK_FULL }), "insufficient space is reported as a disk-full failure");
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
        add("cluster-plus-one", base, L"cluster plus one.bin", cluster + 1);
        add("multi-cluster", base, L"multiple clusters.bin", cluster * 17 + 137);
        const auto copySize = std::min<size_t>(3 * 1024 * 1024 + 13, size_t(freeClusters) * cluster / 8);
        add("copy-boundary", base, L"copy buffer boundary.bin", copySize);
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

        // Keep the reuse source live while the rest of the scan fixtures are exercised.
        const auto reuseDirectory = base + L"\\cluster reuse";
        MakeDirectory(reuseDirectory);
        const std::wstring reuseName = L"deleted cluster reuse source.bin";
        const auto reuseSource = reuseDirectory + L"\\" + reuseName;
        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);
        const size_t moveBytes = std::max<size_t>(cluster, systemInfo.dwPageSize);
        const DWORD moveClusters = static_cast<DWORD>(moveBytes / cluster);
        const auto reuseBytes = Pattern(2 * moveBytes, 0x71AABC23);
        WriteBytes(reuseSource, reuseBytes);

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

        // Separate the replacement and its parent from the source's NTFS record so moves cannot rewrite its MFT page.
        const auto replacementDirectory = reuseDirectory + L"\\replacement";
        MakeDirectory(replacementDirectory);
        const auto replacement = replacementDirectory + L"\\allocated replacement.bin";
        WriteBytes(replacement, Pattern(moveBytes, 0x198735));

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
            const auto file = OpenFixture(fixture.path, "open tail fixture", GENERIC_WRITE);
            LARGE_INTEGER end{ .QuadPart = static_cast<LONGLONG>(cluster * 7 + 19) };
            WinCheck(SetFilePointerEx(file, end, nullptr, FILE_BEGIN) && SetEndOfFile(file), "extend logical EOF");
            fixture.bytes.resize(static_cast<size_t>(end.QuadPart), 0);
            WinCheck(FlushFileBuffers(file), "flush extended file");

            // Sparse holes preserve logical zeros without borrowing data from adjacent physical allocations.
            const auto hole = std::max<size_t>(cluster, 64 * 1024);
            add("sparse", base, L"sparse recovery.bin", hole * 3);
            auto& sparse = fixtures.back();
            const auto sparseFile = OpenFixture(sparse.path, "open sparse fixture", GENERIC_READ | GENERIC_WRITE);
            DWORD returned = 0;
            FILE_ZERO_DATA_INFORMATION zero{ .FileOffset = { .QuadPart = static_cast<LONGLONG>(hole) },
                .BeyondFinalZero = { .QuadPart = static_cast<LONGLONG>(hole * 2) } };
            WinCheck(DeviceIoControl(sparseFile, FSCTL_SET_SPARSE, nullptr, 0,
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
                const auto compressedFile = OpenFixture(compressed.path, "open compressed fixture",
                    GENERIC_READ | GENERIC_WRITE);
                WORD format = COMPRESSION_FORMAT_DEFAULT;
                DWORD written = 0;
                WinCheck(DeviceIoControl(compressedFile, FSCTL_SET_COMPRESSION,
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
            const auto file = OpenFixture(fragmented, "open fragmentation fixture", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE);
            const auto decoy = OpenFixture(decoyPath, "open fragmentation decoy", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE);
            const auto volume = OpenVolume(source);
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
            fixture.times = ReadFileTimes(OpenFixture(fixture.path, "read original timestamps",
                FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT));
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
        // Flush filesystem metadata before the readers inspect the raw volume.
        WinCheck(FlushFileBuffers(OpenVolume(source)), "flush volume");

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
                    // FAT can only infer a freed chain, so the fragmented file is offered with low confidence.
                    if (fat && fixture.label == "fragmented") Require(record != nullptr &&
                        record->confidence == RecoveryShared::Confidence::Low,
                        "an inferred FAT extent is offered with low confidence");
                    else Require(record == nullptr, "ambiguous allocation must not be offered as recoverable");
                    return;
                }
                Require(record != nullptr, "deleted fixture is discoverable with its original name");
                Require(!fat || (fixture.bytes.size() > cluster) ==
                    (record->confidence == RecoveryShared::Confidence::Low),
                    "only FAT files beyond their first cluster have low confidence");
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
                const auto times = ReadFileTimes(OpenFixture(recovered, "recovered file times",
                    FILE_READ_ATTRIBUTES, FILE_SHARE_READ));
                Require(CompareFileTime(&times[0], &fixture.times[0]) == 0, "original creation timestamp is preserved");
                Require(CompareFileTime(&times[1], &fixture.times[1]) == 0,
                    "original modification timestamp is preserved");
            });
        }

        Test("canceled-scan-retains-candidates", [&]
        {
            RecoveryShared::Progress stopped;
            RecoveryShared::ScanResult partial;
            const bool canceled = RecoveryFails([&]
            {
                reader->Scan(stopped, partial, [&](const RecoveryShared::Record& record)
                {
                    if (record.name == fixtures[0].name) stopped.cancel = true;
                });
            }, { .error = ERROR_CANCELLED });
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
                Require(RecoveryFails([&] { reader->Recover(*candidate, file, progress); }),
                    "a file cannot be used as a recovery directory");
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
                Require(RecoveryFails([&] { reader->Recover(*candidate, folder, progress); }),
                    "existing output must not be overwritten");
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
                Require(RecoveryFails([&] { reader->Recover(*candidate, folder, stopped); },
                    { .error = ERROR_CANCELLED }), "paused recovery cancels promptly");
                Require(std::filesystem::is_empty(folder), "canceled recovery leaves no output");
            });
            Test("recreated-source-name", [&]
            {
                WriteBytes(fixtures[1].path, Pattern(777, 345));
                bool retained = false;
                if (ntfs)
                {
                    // A recreated name can use a different MFT record while the deleted version remains intact.
                    const auto file = OpenFixture(fixtures[1].path, "open replacement fixture",
                        FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE);
                    BY_HANDLE_FILE_INFORMATION information{};
                    WinCheck(GetFileInformationByHandle(file, &information), "replacement identity");
                    retained = ((ULONGLONG(information.nFileIndexHigh) << 32 | information.nFileIndexLow) &
                        0x0000FFFFFFFFFFFFull) != candidate->number;
                }
                WinCheck(FlushFileBuffers(OpenVolume(source)), "flush volume");
                const auto folder = outputRoot + L"\\stale";
                MakeDirectory(folder);
                const bool rejected = RecoveryFails([&]
                {
                    const auto recovered = reader->Recover(*candidate, folder, progress);
                    Require(retained && ReadBytes(recovered) == fixtures[1].bytes,
                        "an intact earlier version recovers its original data after the name is reused");
                });
                Require(retained || rejected, "reallocated file records invalidate the saved candidate");
                if (rejected) Require(std::filesystem::is_empty(folder), "stale candidate leaves no output");
            });
        }

        Test("reallocated-source-cluster-rejected", [&]
        {
            auto file = OpenFixture(replacement, "open reallocation fixture", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE);
            const auto volume = OpenVolume(source);

            WinCheck(DeleteFileW(reuseSource.c_str()) && FlushFileBuffers(volume), "delete and flush reuse source");
            const auto reuseReader = RecoveryShared::Open(source, &progress);
            RecoveryShared::ScanResult saved;
            reuseReader->Scan(progress, saved);
            const auto record = std::ranges::find(saved.records, reuseName, &RecoveryShared::Record::name);
            Require(record != saved.records.end() && !record->data.runs.empty(),
                "the deleted reuse source has a saved physical extent");

            // Move a whole, aligned page on volumes with clusters smaller than a memory page.
            const auto run = std::ranges::max(record->data.runs, {}, &RecoveryShared::Run::count);
            const auto target = (run.lcn + moveClusters - 1) / moveClusters * moveClusters;
            Require(run.lcn >= 0 && static_cast<ULONGLONG>(target - run.lcn) + moveClusters <= run.count,
                "the deleted extent contains the entire replacement allocation");

            // Recover first to prove the saved metadata and bytes are intact immediately before reuse.
            const auto folder = outputRoot + L"\\reallocated source";
            MakeDirectory(folder);
            const auto original = reuseReader->Recover(*record, folder, progress);
            Require(ReadBytes(original) == reuseBytes, "the candidate recovers before its cluster is reused");
            WinCheck(DeleteFileW(original.c_str()), "remove control recovery");

            MOVE_FILE_DATA move{ .FileHandle = file, .StartingVcn = {},
                .StartingLcn = { .QuadPart = target }, .ClusterCount = moveClusters };
            DWORD returned = 0, extentCount = 0;
            WinCheck(DeviceIoControl(volume, FSCTL_MOVE_FILE, &move, sizeof(move), nullptr, 0, &returned, nullptr),
                "reuse deleted fixture allocation");
            WinCheck(FlushFileBuffers(volume), "flush reused source allocation");
            Require(FirstCluster(file, extentCount) == target, "the live replacement owns the saved source cluster");
            Require(RecoveryFails([&] { reuseReader->Recover(*record, folder, progress); },
                { .message = L"IDS_RECOVERY_CHANGED" }), "a newly allocated source cluster cannot be copied");
            Require(std::filesystem::is_empty(folder), "reused source allocation leaves no output");

            // Releasing the cluster must make the same saved record recoverable again, even if its bytes changed.
            file.Release();
            WinCheck(DeleteFileW(replacement.c_str()) && FlushFileBuffers(volume), "release replacement allocation");
            const auto released = reuseReader->Recover(*record, folder, progress);
            Require(std::filesystem::file_size(released) == reuseBytes.size(),
                "the saved record remains valid after the replacement releases its cluster");
            WinCheck(DeleteFileW(released.c_str()), "remove released control recovery");
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
