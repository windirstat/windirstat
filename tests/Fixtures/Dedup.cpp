// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DedupStore.h"

static int checks = 0;
static int failures = 0;

static void Require(const bool condition, const std::string_view message)
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}

std::wstring TranslateError(const HRESULT error)
{
    return std::format(L"Windows error {}", error);
}

template <typename T>
static void WriteAt(std::vector<std::byte>& bytes, const std::size_t offset, const T value)
{
    Require(offset + sizeof(T) <= bytes.size(), "fixture field fits in the record");
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

static CDedupStore::Hash16 Hash(const std::uint8_t id)
{
    CDedupStore::Hash16 result{};
    result[0] = id;
    return result;
}

static void WriteBytes(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    Require(file.good(), "fixture write completes");
}

struct StoreFixture
{
    std::filesystem::path root;
    std::filesystem::path stream;
    std::filesystem::path index;
    std::vector<std::byte> records;
    std::vector<std::uint32_t> offsets;
    CDedupStore::MapReferences live;

    StoreFixture()
    {
        static int sequence = 0;
        root = std::filesystem::temp_directory_path() /
            std::format(L"WinDirStat-DedupTests-{}-{}", GetCurrentProcessId(), ++sequence);
        std::filesystem::create_directories(root / L"Stream");
        std::filesystem::create_directory(root / L"Data");
        stream = root / L"Stream" / L"00000001.00010000.ccc";
        index = root / L"Stream" / L"00000001.00010000.01.cd";
    }

    ~StoreFixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    void AddMap(const std::uint8_t id, const std::initializer_list<std::uint64_t> chunks)
    {
        const std::size_t start = records.size();
        offsets.push_back(static_cast<std::uint32_t>(start));
        records.resize(start + 0x70 + chunks.size() * 64);
        std::memcpy(records.data() + start, "Ckhr\x01\x03\x03\x01", 8);
        WriteAt(records, start + 0x0c, static_cast<std::uint32_t>(8 + chunks.size() * 64));
        const auto hash = Hash(id);
        std::memcpy(records.data() + start + 0x38, hash.data(), hash.size());
        std::memcpy(records.data() + start + 0x68, "Smap\x01\x04\x04\x01", 8);
        std::size_t entry = start + 0x70;
        for (const auto chunk : chunks)
        {
            WriteAt(records, entry + 24, chunk);
            WriteAt(records, entry + 56, 100ull * chunk);
            entry += 64;
        }
        live[hash] = 1;
    }

    void Save()
    {
        WriteBytes(stream, records);
        std::vector<std::byte> bytes(0x1c + 4 * offsets.size());
        std::memcpy(bytes.data(), "Cdir", 4);
        WriteAt(bytes, 0x10, static_cast<std::uint32_t>(offsets.size()));
        for (std::size_t i = 0; i < offsets.size(); i++) WriteAt(bytes, 0x1c + 4 * i, offsets[i]);
        WriteBytes(index, bytes);
    }
};

static void Test(const std::string_view name, const std::function<void()>& body)
{
    try
    {
        body();
        std::cout << "PASS " << name << std::endl;
    }
    catch (const std::exception& error)
    {
        ++failures;
        std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    }
}

int main()
{
    Test("shared chunks and repeated segments", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1, 2, 2 });
        fixture.AddMap(2, { 2 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load a complete store");
        const std::array requests{ Hash(1), Hash(2) };
        std::vector<std::tuple<std::size_t, DWORD, DWORD>> visits;
        Require(store.Resolve(requests, [&](const auto request, auto, const auto& chunk)
        {
            visits.emplace_back(request, chunk.diskSize, chunk.refs);
        }), "resolve each requested map");
        std::ranges::sort(visits);
        Require(visits == decltype(visits){ { 0, 100, 1 }, { 0, 200, 2 }, { 1, 200, 2 } },
            "repeated chunks count once per map and shared chunks retain both users");
    });

    Test("missing requested map", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load valid metadata");
        Require(!store.Resolve(std::array<CDedupStore::Hash16, 1>{ Hash(2) }, [](auto, auto, const auto&) {}),
            "unknown maps must not silently produce zero-size files");
        Require(!store.GetError().empty(), "failed resolution identifies the error");
    });

    Test("chunk changed after loading", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load original map");
        WriteAt(fixture.records, 0x70 + 24, 2ull);
        WriteBytes(fixture.stream, fixture.records);
        Require(!store.Resolve(std::array<CDedupStore::Hash16, 1>{ Hash(1) }, [](auto, auto, const auto&) {}),
            "a changed map must not silently lose an unknown chunk");
    });

    Test("unreadable container", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load before the container is locked");
        const SmartPointer locked(CloseHandle, CreateFileW(fixture.stream.c_str(), GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        Require(locked.IsValid(), "hold an exclusive file handle");
        Require(!store.Resolve(std::array<CDedupStore::Hash16, 1>{ Hash(1) }, [](auto, auto, const auto&) {}),
            "sharing violations fail resolution");
    });

    Test("missing live metadata", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.live[Hash(2)] = 1;
        fixture.Save();
        CDedupStore store;
        Require(!store.Load(fixture.root, fixture.live), "a live file with no stream map invalidates the snapshot");
    });

    Test("repeat load replaces the snapshot", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load initial snapshot");
        fixture.records.clear();
        fixture.offsets.clear();
        fixture.live.clear();
        fixture.AddMap(2, { 2 });
        fixture.Save();
        Require(store.Load(fixture.root, fixture.live), "reload another snapshot");
        ULONGLONG bytes = 0;
        Require(store.Resolve(std::array<CDedupStore::Hash16, 1>{ Hash(2) }, [&](auto, auto, const auto& chunk)
            { bytes += chunk.diskSize; }), "resolve the replacement");
        Require(bytes == 200, "replacement values contain no stale references");
        Require(!store.Resolve(std::array<CDedupStore::Hash16, 1>{ Hash(1) }, [](auto, auto, const auto&) {}), "old map is gone");
    });

    Test("incomplete data directory", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load valid stream metadata");
        std::filesystem::remove(fixture.root / L"Data");
        Require(!store.ComputeSpaceUsage(), "a missing data directory cannot become metadata overhead");
    });

    Test("malformed data index and header", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        const auto data = fixture.root / L"Data" / L"00000001.00010000.ccc";
        const auto index = fixture.root / L"Data" / L"00000001.00010000.01.cd";
        WriteBytes(data, std::vector<std::byte>(8192));
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load the stream map");
        Require(!store.ComputeSpaceUsage(), "a missing data index is not ignored");
        std::vector<std::byte> bytes(0x20);
        std::memcpy(bytes.data(), "Cdir", 4);
        WriteAt(bytes, 0x10, 1u);
        WriteAt(bytes, 0x1c, 0u);
        WriteBytes(index, bytes);
        Require(!store.ComputeSpaceUsage(), "unreadable chunk headers are not counted as metadata");
    });

    Test("allocated store bytes and cancellation", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        WriteBytes(fixture.root / L"metadata", std::vector<std::byte>(5000));
        ULONGLONG allocated = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fixture.root))
        {
            if (entry.is_directory()) continue;
            const SmartPointer file(CloseHandle, CreateFileW(entry.path().c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
            FILE_STANDARD_INFO info{};
            Require(GetFileInformationByHandleEx(file, FileStandardInfo, &info, sizeof(info)), "read actual allocation");
            allocated += info.AllocationSize.QuadPart;
        }
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "load valid stream metadata");
        Require(store.ComputeSpaceUsage(), "account for a complete directory");
        const auto& space = store.GetSpaceUsage();
        Require(space.storeBytes == allocated, "cluster slack is included in store allocation");
        Require(space.liveChunkBytes + space.garbageBytes + space.overheadBytes == allocated,
            "store categories conserve allocated bytes");
        Require(!store.ComputeSpaceUsage([] { return true; }), "cancellation stops the directory walk");
    });

    Test("bounded reads from a large container", []
    {
        StoreFixture fixture;
        fixture.AddMap(1, { 1 });
        fixture.Save();
        {
            const SmartPointer file(CloseHandle, CreateFileW(fixture.stream.c_str(), GENERIC_WRITE, 0,
                nullptr, OPEN_EXISTING, 0, nullptr));
            DWORD returned = 0;
            Require(DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr),
                "make oversized fixture sparse");
            const LARGE_INTEGER size{ .QuadPart = 4ll * 1024 * 1024 * 1024 };
            Require(SetFilePointerEx(file, size, nullptr, FILE_BEGIN) && SetEndOfFile(file), "set 4 GiB length");
        }
        CDedupStore store;
        Require(store.Load(fixture.root, fixture.live), "read mapped records without allocating the entire container");
    });

    std::cout << checks << " checks, " << failures << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
