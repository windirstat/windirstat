// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DedupAccounting.h"

CDedupAccounting& CDedupAccounting::Get()
{
    return CWinDirStatModel::Get()->GetDedupAccounting();
}

std::wstring CDedupAccounting::GetVolumePath(const CItem* item)
{
    if (!item->SupportsFilesystemApis()) return {};
    const SmartPointer handle(CloseHandle, CreateFileW(item->GetPathLong().c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle == INVALID_HANDLE_VALUE) return {};
    std::wstring path(MAX_PATH, L'\0');
    DWORD size = GetFinalPathNameByHandleW(handle, path.data(), static_cast<DWORD>(path.size()), VOLUME_NAME_GUID);
    if (size >= path.size())
    {
        path.resize(static_cast<std::size_t>(size) + 1);
        size = GetFinalPathNameByHandleW(handle, path.data(), static_cast<DWORD>(path.size()), VOLUME_NAME_GUID);
    }
    if (size == 0 || size >= path.size()) return {};
    path.resize(size);
    const auto end = path.find(L"}\\");
    if (!path.starts_with(L"\\\\?\\Volume{") || end == std::wstring::npos) return {};
    path.resize(end + 2);
    return path;
}

static bool IsVolumeBoundary(const CItem* item)
{
    return !item->IsTypeOrFlag(IT_MYCOMPUTER) &&
        (item->IsScanRoot() || item->IsTypeOrFlag(IT_DRIVE, ITRP_MOUNT, ITRP_JUNCTION, ITRP_SYMLINK));
}

static CItem* CommonParent(CItem* parent, const CItem* item)
{
    while (parent != nullptr && !parent->IsAncestorOf(item)) parent = parent->GetParent();
    return parent;
}

// Enumeration roots and mount aliases share one file-identity namespace.
static std::vector<CItem*> CollectFiles(const std::vector<CItem*>& roots)
{
    std::vector<CItem*> files;
    for (CItem* root : roots)
    {
        for (std::vector<CItem*> pending{ root }; !pending.empty();)
        {
            CItem* item = pending.back(); pending.pop_back();
            if (item->IsTypeOrFlag(IT_FILE)) files.push_back(item);
            else if (!item->IsLeaf() && !item->IsTypeOrFlag(IT_HLINKS) &&
                (item == root || !IsVolumeBoundary(item))) pending.append_range(item->GetChildren());
        }
    }
    return files;
}

// Store folders in the tree, found by file identity so drive letters, selected folders and mount paths agree.
static std::unordered_set<const CItem*> FindStores(const std::vector<CItem*>& files, const ULONGLONG storeId)
{
    std::unordered_set<const CItem*> stores;
    if (storeId != 0) for (const CItem* file : files)
    {
        for (const CItem* parent = file->GetParent(); parent != nullptr; parent = parent->GetParent())
        {
            if (parent->GetIndex() == storeId) { stores.insert(parent); break; }
            if (IsVolumeBoundary(parent)) break;
        }
    }
    return stores;
}

// Remaining names of a file by identity: 0 once it is gone, nullopt when that cannot be told.
static std::optional<DWORD> CountLinks(const HANDLE volume, const ULONGLONG id)
{
    FILE_ID_DESCRIPTOR descriptor{ .dwSize = sizeof(FILE_ID_DESCRIPTOR), .Type = FileIdType };
    descriptor.FileId.QuadPart = static_cast<LONGLONG>(id);
    const SmartPointer handle(CloseHandle, OpenFileById(volume, &descriptor, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT));
    if (handle == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_INVALID_PARAMETER || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return 0;
        return std::nullopt;
    }
    FILE_STANDARD_INFO standard{};
    if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard))) return std::nullopt;
    return standard.DeletePending && standard.NumberOfLinks != 0 ? standard.NumberOfLinks - 1 : standard.NumberOfLinks;
}

std::expected<std::shared_ptr<const CDedupAccounting::Volume>, std::wstring> CDedupAccounting::BuildVolume(
    const std::wstring& path, const std::vector<CItem*>& roots, const CDedupStore::CancelCheck& cancelled)
{
    CDedupStore::CancelLatch latch(cancelled);
    const auto error = [](const DWORD code) { return std::unexpected(TranslateError(code)); };
    std::vector<CItem*> files = CollectFiles(roots);
    const auto candidate = [](const CItem* item)
    {
        const DWORD attributes = item->GetAttributes();
        return item->IsTypeOrFlag(ITRP_DEDUP) ||
            attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
            (item->GetRawType() & ITRP_MASK) == ITRP_NONE;
    };
    const auto metric = static_cast<DEDUPMETRIC>(COptions::DedupPhysicalMetric.Obj());
    const bool wholeVolume = std::ranges::any_of(roots,
        [](const CItem* root) { return root->IsTypeOrFlag(IT_DRIVE, ITRP_MOUNT); });
    if (files.size() >= None) return error(ERROR_NOT_ENOUGH_MEMORY);
    if ((!wholeVolume || metric == DEDUP_ALLOCATION) && !std::ranges::any_of(files, candidate))
        return std::shared_ptr<const Volume>{};
    const std::wstring storePath = CDedupStore::FindStore(path);
    if (storePath.empty())
    {
        if (std::ranges::any_of(files, [](const CItem* item) { return item->IsTypeOrFlag(ITRP_DEDUP); }))
            return error(ERROR_ACCESS_DENIED);
        return std::shared_ptr<const Volume>{};
    }
    const auto candidates = std::ranges::partition(files, [&](const CItem* item)
    { return candidate(item) || !item->IsTypeOrFlag(ITF_SINGLE_LINK) && item->GetSizePhysicalRaw() != 0; });
    const auto count = static_cast<std::size_t>(candidates.begin() - files.begin());
    std::ranges::sort(files.begin(), candidates.begin(), {}, &CItem::GetIndex);

    struct File
    {
        std::size_t first, last;
        DWORD links = 0;
        std::vector<CDedupStore::Hash16> maps;
        ULONGLONG exclusive = 0;
        double effective = 0;
    };
    std::vector<File> records;
    for (std::size_t first = 0; first < count;)
    {
        std::size_t last = first + 1;
        if (files[first]->GetIndex() != 0)
            while (last < count && files[last]->GetIndex() == files[first]->GetIndex()) last++;
        records.push_back({ first, last });
        first = last;
    }

    // Reuse the authoritative volume enumeration for selected files, including their hardlink counts.
    const auto liveMaps = CDedupStore::EnumerateVolumeMaps(path, cancelled,
        [&](const ULONGLONG id, const DWORD links, std::vector<CDedupStore::Hash16>&& maps)
    {
        const auto file = std::ranges::lower_bound(records, id, {},
            [&](const File& record) { return files[record.first]->GetIndex(); });
        if (file == records.end() || files[file->first]->GetIndex() != id) return;
        file->links = links;
        file->maps = std::move(maps);
    });
    if (!liveMaps) return error(ERROR_READ_FAULT);

    // Only ordinary hardlinks and files without an identity need another metadata read.
    std::exception_ptr failure;
    std::mutex failureLock;
    std::for_each(std::execution::par, records.begin(), records.end(), [&](File& file)
    {
        try
        {
            if (latch() || !file.maps.empty()) return;
            CItem* item = files[file.first];
            if (candidate(item) && item->GetIndex() == 0)
            {
                if (auto info = CDedupStore::ReadReparseData(item->GetPathLong())) file.maps = std::move(info->maps);
            }
            if (file.maps.empty() && file.last - file.first == 1 && item->IsTypeOrFlag(ITF_SINGLE_LINK))
                file.links = 1;
            else if (item->GetSizePhysicalRaw() != 0 || !file.maps.empty() || file.last - file.first > 1)
            {
                const SmartPointer handle(CloseHandle, CreateFileW(item->GetPathLong().c_str(), FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                BY_HANDLE_FILE_INFORMATION info{};
                if (handle != INVALID_HANDLE_VALUE && GetFileInformationByHandle(handle, &info) &&
                    (item->GetIndex() == 0 || item->GetIndex() ==
                        (static_cast<ULONGLONG>(info.nFileIndexHigh) << 32 | info.nFileIndexLow)))
                    file.links = info.nNumberOfLinks;
            }
            else file.links = 1;
            std::ranges::sort(file.maps);
            file.maps.erase(std::ranges::unique(file.maps).begin(), file.maps.end());
        }
        catch (...)
        {
            std::lock_guard lock(failureLock);
            failure = std::current_exception();
        }
    });
    if (failure) std::rethrow_exception(failure);
    if (cancelled && cancelled()) return error(ERROR_CANCELLED);

    for (const File& file : records)
    {
        if (files[file.first]->IsTypeOrFlag(ITRP_DEDUP) && file.maps.empty()) return error(ERROR_READ_FAULT);
        if (!file.maps.empty() && file.links == 0) return error(ERROR_READ_FAULT);
        if (file.links != 0 && file.links < file.last - file.first) return error(ERROR_INVALID_DATA);
    }

    // Ordinary hardlinks only place their allocation; optimized files follow the store.
    std::vector<File> linked;
    for (File& file : records)
        if (file.maps.empty() && (file.links != 1 || file.last - file.first != 1)) linked.push_back(std::move(file));
    std::erase_if(records, [](const File& file) { return file.maps.empty(); });
    if (records.empty() && (!wholeVolume || metric == DEDUP_ALLOCATION)) return std::shared_ptr<const Volume>{};

    CDedupStore store;
    if (!store.Load(storePath, *liveMaps, cancelled)) return std::unexpected(store.GetError());

    // Find the store by file identity so drive letters, selected folders and mount paths agree.
    ULONGLONG storeId = 0;
    const SmartPointer storeHandle(CloseHandle, CreateFileW(storePath.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    BY_HANDLE_FILE_INFORMATION storeInfo{};
    if (storeHandle != INVALID_HANDLE_VALUE && GetFileInformationByHandle(storeHandle, &storeInfo))
        storeId = static_cast<ULONGLONG>(storeInfo.nFileIndexHigh) << 32 | storeInfo.nFileIndexLow;
    const auto stores = FindStores(files, storeId);
    const bool replaceStore = !stores.empty() && metric != DEDUP_ALLOCATION;
    const bool withSpaceItems = metric != DEDUP_ALLOCATION && (wholeVolume || replaceStore);
    if (withSpaceItems && !store.ComputeSpaceUsage(cancelled)) return std::unexpected(store.GetError());
    if (replaceStore) for (const CItem* storeItem : stores)
        if (storeItem->GetSizePhysicalRaw() != store.GetSpaceUsage().storeBytes) return error(ERROR_INVALID_DATA);

    // Resolve each map once; map users are physical files, with directory-entry counts kept separately.
    std::vector<std::pair<CDedupStore::Hash16, std::uint32_t>> mapEntries;
    for (const auto [file, record] : std::views::enumerate(records))
        for (const auto& map : record.maps) mapEntries.emplace_back(map, static_cast<std::uint32_t>(file));
    std::ranges::sort(mapEntries);
    std::vector<CDedupStore::Hash16> requests;
    std::vector<std::uint32_t> mapOffsets, mapUsers;
    for (const auto& [hash, file] : mapEntries)
    {
        if (requests.empty() || requests.back() != hash)
        {
            requests.push_back(hash);
            mapOffsets.push_back(static_cast<std::uint32_t>(mapUsers.size()));
        }
        mapUsers.push_back(file);
    }
    mapOffsets.push_back(static_cast<std::uint32_t>(mapUsers.size()));
    decltype(mapEntries){}.swap(mapEntries);
    std::vector<bool> mapExternal(requests.size());
    for (std::size_t map = 0; map < requests.size(); map++)
    {
        const auto live = liveMaps->find(requests[map]);
        ULONGLONG selected = 0;
        for (auto i = mapOffsets[map]; i < mapOffsets[map + 1]; i++)
            selected += records[mapUsers[i]].last - records[mapUsers[i]].first;
        if (live == liveMaps->end() || live->second < selected) return error(ERROR_INVALID_DATA);
        mapExternal[map] = live->second > selected;
    }
    for (auto& record : records) decltype(record.maps){}.swap(record.maps);

    std::vector<std::pair<std::uint32_t, std::uint32_t>> visits;
    if (!store.Resolve(requests, [&](const std::size_t map, const std::uint32_t chunk,
        const CDedupStore::ChunkInfo& info)
    {
        const double share = static_cast<double>(info.diskSize) / info.refs / liveMaps->at(requests[map]);
        for (auto i = mapOffsets[map]; i < mapOffsets[map + 1]; i++)
            records[mapUsers[i]].effective += share;
        const auto first = mapOffsets[map], last = mapOffsets[map + 1];
        File& file = records[mapUsers[first]];
        if (info.refs == 1 && last - first == 1 && file.links == 1 && !mapExternal[map]) file.exclusive += info.diskSize;
        else visits.emplace_back(chunk, static_cast<std::uint32_t>(map));
    }, cancelled)) return std::unexpected(store.GetError());

    // Chunks are counted once across every root; their ownership is placed by the projection.
    Model model;
    std::ranges::sort(visits);
    std::vector<std::uint32_t> users;
    for (std::size_t begin = 0; begin < visits.size();)
    {
        if (latch()) return error(ERROR_CANCELLED);
        const auto chunk = visits[begin].first;
        std::size_t end = begin;
        users.clear();
        bool external = false;
        for (; end < visits.size() && visits[end].first == chunk; end++)
        {
            const auto map = visits[end].second;
            external = external || mapExternal[map];
            users.insert(users.end(), mapUsers.begin() + mapOffsets[map], mapUsers.begin() + mapOffsets[map + 1]);
        }
        std::ranges::sort(users);
        users.erase(std::ranges::unique(users).begin(), users.end());
        const auto& info = store.GetChunk(chunk);
        model.chunkFiles.insert(model.chunkFiles.end(), users.begin(), users.end());
        model.chunkOffsets.push_back(static_cast<std::uint32_t>(model.chunkFiles.size()));
        model.chunkBytes.push_back(info.diskSize);
        model.chunkExternal.push_back(external || end - begin < info.refs);
        begin = end;
    }
    decltype(visits){}.swap(visits);

    std::vector<CItem*> entries;
    std::vector<std::uint32_t> entryOffsets;
    const auto add = [&](const File& file)
    {
        model.ids.push_back(files[file.first]->GetIndex());
        model.links.push_back(file.links);
        entryOffsets.push_back(static_cast<std::uint32_t>(entries.size()));
        entries.insert(entries.end(), files.begin() + static_cast<std::ptrdiff_t>(file.first),
            files.begin() + static_cast<std::ptrdiff_t>(file.last));
    };
    for (const File& file : records)
    {
        add(file);
        model.exclusive.push_back(file.exclusive);
        model.effective.push_back(file.effective);
    }
    for (const File& file : linked) add(file);
    entryOffsets.push_back(static_cast<std::uint32_t>(entries.size()));
    if (withSpaceItems) model.space = store.GetSpaceUsage();
    model.storeId = storeId;
    model.metric = metric;
    model.wholeVolume = wholeVolume;
    model.withSpaceItems = withSpaceItems;
    model.replaceStore = replaceStore;
    return Project(path, roots, files, entries, entryOffsets, std::move(model), cancelled);
}

std::expected<std::shared_ptr<const CDedupAccounting::Volume>, std::wstring> CDedupAccounting::Project(
    const std::wstring& path, const std::vector<CItem*>& roots, const std::vector<CItem*>& files,
    const std::vector<CItem*>& entries, const std::vector<std::uint32_t>& entryOffsets, Model&& model,
    const CDedupStore::CancelCheck& cancelled)
{
    auto volume = std::make_shared<Volume>();
    volume->path = path;
    volume->roots = roots;
    CDedupStore::CancelLatch latch(cancelled);
    const auto error = [](const DWORD code) { return std::unexpected(TranslateError(code)); };
    const auto metric = model.metric;
    const auto optimized = model.exclusive.size();
    const auto names = [&](const std::size_t file)
    {
        return std::span(entries).subspan(entryOffsets[file], entryOffsets[file + 1] - entryOffsets[file]);
    };

    // Direct file contributions and ownership below a folder are kept compact between refreshes.
    std::unordered_map<const CItem*, Values> values;
    const auto own = [&](const CItem* item, const ULONGLONG bytes)
    {
        auto& value = values[item].freed;
        value = (value == Unknown ? 0 : value) + bytes;
    };
    std::unordered_map<CItem*, std::uint32_t> sharedItems;
    const auto pseudo = [&](CItem* parent, const DEDUPITEMKIND kind, const ULONGLONG bytes)
    {
        if (bytes == 0) return None;
        if (kind == DEDUPITEM_SHARED)
        {
            const auto [entry, inserted] = sharedItems.try_emplace(parent,
                static_cast<std::uint32_t>(volume->pseudo.size()));
            if (!inserted) { volume->pseudo[entry->second].bytes += bytes; return entry->second; }
        }
        volume->pseudo.push_back({ parent, bytes, kind });
        return static_cast<std::uint32_t>(volume->pseudo.size() - 1);
    };
    CItem* displayRoot = roots.front();
    for (CItem* root : roots) displayRoot = CommonParent(displayRoot, root);

    // A hardlink allocation belongs to the smallest folder containing every surviving directory entry.
    for (std::size_t file = 0; file < model.ids.size(); file++)
    {
        const auto links = model.links[file];
        if (links == 1 && names(file).size() == 1) continue;
        CItem* owner = names(file).front()->GetParent();
        for (CItem* name : names(file)) owner = CommonParent(owner, name);
        for (CItem* name : names(file)) values[name].freed = links == 0 ? Unknown : 0;
        if (links != 0 && links == names(file).size() && owner != nullptr)
            own(owner, names(file).front()->GetSizePhysicalRaw());
    }

    // Shared ownership also includes all hardlink names; a chunk left with one single-link user is its own.
    std::vector<ULONGLONG> onDisk = model.exclusive;
    auto& exclusive = model.exclusive;
    std::unordered_map<const CItem*, std::uint32_t> stamps;
    ULONGLONG externalBytes = 0, attributed = 0;
    volume->chunkOffsets.push_back(0);
    for (std::size_t chunk = 0; chunk < model.chunkBytes.size(); chunk++)
    {
        if (latch()) return error(ERROR_CANCELLED);
        const auto users = std::span(model.chunkFiles).subspan(model.chunkOffsets[chunk],
            model.chunkOffsets[chunk + 1] - model.chunkOffsets[chunk]);
        const auto bytes = model.chunkBytes[chunk];
        const bool external = model.chunkExternal[chunk];
        for (const auto user : users) onDisk[user] += bytes;
        if (!external && users.size() == 1 && model.links[users.front()] == 1)
        {
            exclusive[users.front()] += bytes;
            continue;
        }
        attributed += bytes;
        CItem* owner = external ? nullptr : names(users.front()).front()->GetParent();
        if (owner != nullptr) for (const auto user : users)
            for (CItem* name : names(user)) owner = CommonParent(owner, name);
        std::uint32_t pseudoIndex = None;
        if (owner != nullptr)
        {
            own(owner, bytes);
            if (metric == DEDUP_SHARED_ONCE) pseudoIndex = pseudo(owner, DEDUPITEM_SHARED, bytes);
        }
        else externalBytes += bytes;
        const auto stamp = static_cast<std::uint32_t>(volume->chunkBytes.size() + 1);
        for (const auto user : users) for (CItem* name : names(user))
        {
            for (CItem* parent = name->GetParent(); parent != nullptr && parent != owner;
                parent = parent->GetParent())
            {
                if (stamps[parent] == stamp) break;
                stamps[parent] = stamp;
                auto& shared = values[parent].shared;
                shared = (shared == Unknown ? 0 : shared) + bytes;
            }
        }
        volume->chunkFiles.insert(volume->chunkFiles.end(), users.begin(), users.end());
        volume->chunkOffsets.push_back(static_cast<std::uint32_t>(volume->chunkFiles.size()));
        volume->chunkBytes.push_back(bytes);
        volume->chunkOwner.push_back(pseudoIndex);
        volume->chunkExternal.push_back(external);
    }
    decltype(stamps){}.swap(stamps);

    // The physical allocations remain in CItem; the projection contains only differences and metrics.
    volume->entries.reserve(entryOffsets[optimized]);
    volume->entryOffsets.reserve(optimized + 1);
    for (std::size_t file = 0; file < optimized; file++)
    {
        volume->entryOffsets.push_back(static_cast<std::uint32_t>(volume->entries.size()));
        for (CItem* item : names(file))
        {
            const ULONGLONG raw = item->GetSizePhysicalRaw();
            Values& value = values[item];
            value.file = static_cast<std::uint32_t>(file);
            value.freed = model.links[file] == 1 ? raw + exclusive[file] : 0;
            value.shared = onDisk[file] - exclusive[file];
            switch (metric)
            {
            case DEDUP_SHARED_ONCE:
            case DEDUP_EXCLUSIVE: value.delta = exclusive[file]; break;
            case DEDUP_EFFECTIVE: value.delta = static_cast<LONGLONG>(std::llround(model.effective[file])); break;
            case DEDUP_LOGICAL: value.delta = std::max(raw, item->GetSizeLogical()) - raw; break;
            default: break;
            }
            volume->entries.push_back(item);
        }
    }
    volume->entryOffsets.push_back(static_cast<std::uint32_t>(volume->entries.size()));
    for (const auto bytes : exclusive) attributed += bytes;

    const auto stores = FindStores(files, model.storeId);
    for (const CItem* file : files)
    {
        for (const CItem* parent = file->GetParent(); parent != nullptr; parent = parent->GetParent())
        {
            if (stores.contains(parent))
            {
                auto& value = values[file];
                value.freed = 0;
                if (model.replaceStore) value.delta = -static_cast<LONGLONG>(file->GetSizePhysicalRaw());
                break;
            }
            if (IsVolumeBoundary(parent)) break;
        }
    }
    if (model.withSpaceItems)
    {
        const auto& space = model.space;
        if (attributed > space.liveChunkBytes) return error(ERROR_INVALID_DATA);
        externalBytes += space.liveChunkBytes - attributed;
        pseudo(displayRoot, DEDUPITEM_GARBAGE, space.regularGarbageBytes);
        pseudo(displayRoot, DEDUPITEM_GARBAGE_FULL, space.garbageBytes - space.regularGarbageBytes);
        pseudo(displayRoot, DEDUPITEM_OVERHEAD, space.overheadBytes);
    }
    if (metric == DEDUP_SHARED_ONCE) pseudo(displayRoot, DEDUPITEM_EXTERNAL, externalBytes);

    // Invert the shared chunk adjacency once for selection and highlighting queries.
    volume->fileOffsets.assign(optimized + 1, 0);
    for (const auto user : volume->chunkFiles) volume->fileOffsets[user + 1]++;
    for (std::size_t i = 1; i < volume->fileOffsets.size(); i++) volume->fileOffsets[i] += volume->fileOffsets[i - 1];
    volume->fileChunks.resize(volume->chunkFiles.size());
    auto cursor = volume->fileOffsets;
    for (std::uint32_t chunk = 0; chunk < volume->chunkBytes.size(); chunk++)
        for (auto i = volume->chunkOffsets[chunk]; i < volume->chunkOffsets[chunk + 1]; i++)
            volume->fileChunks[cursor[volume->chunkFiles[i]]++] = chunk;
    volume->values.reserve(values.size());
    for (const auto& entry : values) volume->values.push_back(entry);

    // Keep what a later removal needs to be applied without reading the store again.
    volume->fileBytes = std::move(onDisk);
    volume->fileExclusive = std::move(exclusive);
    volume->fileIds = std::move(model.ids);
    volume->fileLinks = std::move(model.links);
    volume->fileEffective = std::move(model.effective);
    volume->fileEntries.reserve(volume->fileIds.size());
    for (std::size_t file = 0; file < volume->fileIds.size(); file++)
        volume->fileEntries.push_back(entryOffsets[file + 1] - entryOffsets[file]);
    volume->space = model.space;
    volume->storeId = model.storeId;
    volume->metric = metric;
    volume->wholeVolume = model.wholeVolume;
    volume->withSpaceItems = model.withSpaceItems;
    volume->replaceStore = model.replaceStore;
    return volume;
}

// Deleting or recycling only removes or renames directory entries, so the previous results are bound to the
// rescanned tree again by file identity. Files that lost names are asked for their remaining links; a file
// without links frees its exclusive chunks and the shared chunks no other file uses, while one still linked
// outside the scan keeps them as data used outside of it. The store itself is not read.
std::optional<std::shared_ptr<const CDedupAccounting::Volume>> CDedupAccounting::UpdateVolume(
    const Volume& previous, const std::vector<CItem*>& roots, const CDedupStore::CancelCheck& cancelled)
{
    // Effective sizes depend on every reference of a chunk, which only the store has.
    const auto metric = static_cast<DEDUPMETRIC>(COptions::DedupPhysicalMetric.Obj());
    if (metric != previous.metric || metric == DEDUP_EFFECTIVE) return std::nullopt;
    const std::size_t count = previous.fileIds.size();
    const std::size_t optimized = previous.fileExclusive.size();
    std::unordered_map<ULONGLONG, std::uint32_t> known;
    for (std::uint32_t file = 0; file < count; file++)
        if (previous.fileIds[file] == 0 || !known.emplace(previous.fileIds[file], file).second) return std::nullopt;

    // An optimized file the previous results do not know as one needs the store, and so does a changed store.
    const auto files = CollectFiles(roots);
    std::vector<std::vector<CItem*>> names(count);
    for (CItem* item : files)
    {
        const auto found = item->GetIndex() != 0 ? known.find(item->GetIndex()) : known.end();
        if (item->IsTypeOrFlag(ITRP_DEDUP) && (found == known.end() || found->second >= optimized)) return std::nullopt;
        if (found != known.end()) names[found->second].push_back(item);
    }
    if (previous.replaceStore)
    {
        const auto stores = FindStores(files, previous.storeId);
        if (stores.empty() || std::ranges::any_of(stores, [&](const CItem* store)
            { return store->GetSizePhysicalRaw() != previous.space.storeBytes; })) return std::nullopt;
    }

    std::vector<DWORD> links = previous.fileLinks;
    const SmartPointer volumeHandle(CloseHandle, CreateFileW(previous.path.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (volumeHandle == INVALID_HANDLE_VALUE) return std::nullopt;
    for (std::size_t file = 0; file < count; file++)
    {
        if (names[file].size() == previous.fileEntries[file]) continue;
        if (cancelled && cancelled()) return std::nullopt;
        const auto remaining = CountLinks(volumeHandle, previous.fileIds[file]);
        if (!remaining || *remaining < names[file].size()) return std::nullopt;
        links[file] = *remaining;
    }

    Model model;
    std::vector<CItem*> entries;
    std::vector<std::uint32_t> entryOffsets, index(count, None);
    std::vector<bool> outside(optimized);
    ULONGLONG released = 0;
    const auto keep = [&](const std::size_t file)
    {
        index[file] = static_cast<std::uint32_t>(model.ids.size());
        model.ids.push_back(previous.fileIds[file]);
        model.links.push_back(links[file]);
        entryOffsets.push_back(static_cast<std::uint32_t>(entries.size()));
        entries.append_range(names[file]);
    };
    for (std::size_t file = 0; file < optimized; file++)
    {
        if (!names[file].empty())
        {
            keep(file);
            model.exclusive.push_back(previous.fileExclusive[file]);
            model.effective.push_back(previous.fileEffective[file]);
        }
        else if (links[file] == 0) released += previous.fileExclusive[file];
        else outside[file] = true;
    }
    for (std::size_t file = optimized; file < count; file++) if (!names[file].empty()) keep(file);
    entryOffsets.push_back(static_cast<std::uint32_t>(entries.size()));

    for (std::size_t chunk = 0; chunk < previous.chunkBytes.size(); chunk++)
    {
        bool external = previous.chunkExternal[chunk];
        const auto before = model.chunkFiles.size();
        for (auto i = previous.chunkOffsets[chunk]; i < previous.chunkOffsets[chunk + 1]; i++)
        {
            const auto user = previous.chunkFiles[i];
            if (index[user] == None) external = external || outside[user];
            else
            {
                model.chunkFiles.push_back(index[user]);
                external = external || links[user] > names[user].size();
            }
        }
        if (model.chunkFiles.size() == before)
        {
            if (!external) released += previous.chunkBytes[chunk];
            continue;
        }
        model.chunkOffsets.push_back(static_cast<std::uint32_t>(model.chunkFiles.size()));
        model.chunkBytes.push_back(previous.chunkBytes[chunk]);
        model.chunkExternal.push_back(external);
    }

    // Released chunks stay in the store until a garbage collection. Whether a regular one reclaims them depends
    // on container occupancy that is not kept, so they count as regular garbage until the next full scan.
    model.space = previous.space;
    if (previous.withSpaceItems)
    {
        if (released > model.space.liveChunkBytes) return std::nullopt;
        model.space.liveChunkBytes -= released;
        model.space.garbageBytes += released;
        model.space.regularGarbageBytes += released;
    }
    model.storeId = previous.storeId;
    model.metric = metric;
    model.wholeVolume = previous.wholeVolume;
    model.withSpaceItems = previous.withSpaceItems;
    model.replaceStore = previous.replaceStore;
    if (model.exclusive.empty() && (!model.wholeVolume || metric == DEDUP_ALLOCATION)) return std::shared_ptr<const Volume>{};
    auto result = Project(previous.path, roots, files, entries, entryOffsets, std::move(model), cancelled);
    if (!result) return std::nullopt;
    return std::move(*result);
}

void CDedupAccounting::RunPostScan(const CDedupStore::CancelCheck& cancelled)
{
    if (m_snapshot || cancelled && cancelled()) return;
    const SmartPointer resetProcessing([](CDedupAccounting* accounting) { accounting->m_processing = false; }, this);
    try
    {
        if (m_imported)
        {
            Publish({});
            return;
        }
        if (!COptions::DedupAccounting) return;

        // Keep cancellation available and show activity while the filesystem scan is being accounted for.
        CItem* root = CWinDirStatModel::Get()->GetRootItem();
        if (root == nullptr) return;
        m_processing = true;
        root->UpwardSetUndone();
        CMainFrame::Get()->InvokeInMessageThread([]
        {
            CMainFrame::Get()->UpdateProgress();
            CDirStatApp::Get()->OnIdle(0);
        });

        // Resolve each boundary once, including selected folders and followed mount paths.
        std::map<std::wstring, std::vector<CItem*>> groups;
        for (std::vector<CItem*> pending{ CWinDirStatModel::Get()->GetRootItem() }; !pending.empty();)
        {
            CItem* item = pending.back(); pending.pop_back();
            if (item == nullptr || item->IsLeaf() || item->IsTypeOrFlag(IT_HLINKS, ITF_MTP, ITF_REMOTE)) continue;
            if (!item->HasChildren() && !item->IsScanRoot()) continue;
            if (IsVolumeBoundary(item))
                if (auto path = GetVolumePath(item); !path.empty()) groups[std::move(path)].push_back(item);
            for (CItem* child : item->GetChildren()) if (!child->IsLeaf()) pending.push_back(child);
        }
        auto volumes = m_volumes;
        std::vector<std::shared_ptr<const Volume>> removed;
        {
            std::unique_lock lock(m_lock);
            removed = std::exchange(m_removed, {});
        }
        for (const auto& [path, roots] : groups)
        {
            if (cancelled && cancelled()) return;
            if (std::ranges::any_of(volumes, [&](const auto& volume) { return volume->path == path; })) continue;

            // A removal updates the previous results; anything that cannot be updated reads the volume again.
            std::optional<std::shared_ptr<const Volume>> updated;
            if (const auto previous = std::ranges::find(removed, path, &Volume::path); previous != removed.end())
                updated = UpdateVolume(**previous, roots, cancelled);
            if (cancelled && cancelled()) return;
            auto result = updated ? std::expected<std::shared_ptr<const Volume>, std::wstring>(std::move(*updated)) :
                BuildVolume(path, roots, cancelled);
            if (cancelled && cancelled()) return;
            if (result && *result) volumes.push_back(std::move(*result));
            else if (!result)
            {
                std::unique_lock lock(m_lock);
                m_failures.push_back(std::format(L"{}: {}", roots.front()->GetPath(), result.error()));
            }
        }
        if (!volumes.empty()) Publish(std::move(volumes));
    }
    catch (const std::exception&)
    {
        std::unique_lock lock(m_lock);
        m_failures.push_back(TranslateError(ERROR_NOT_ENOUGH_MEMORY));
    }
}

void CDedupAccounting::Publish(std::vector<std::shared_ptr<const Volume>> volumes)
{
    auto snapshot = std::make_unique<Snapshot>();
    snapshot->volumes = volumes;
    snapshot->owners.resize(volumes.size());
    auto& values = snapshot->values;
    std::unordered_map<const CItem*, std::uint32_t> boundaries;
    std::vector<std::unique_ptr<CItem>> created;
    std::map<std::pair<CItem*, DEDUPITEMKIND>, CItem*> pseudoItems;
    static constexpr std::array names{ IDS_DEDUP_ITEM_SHARED, IDS_DEDUP_ITEM_EXTERNAL, IDS_DEDUP_ITEM_GARBAGE,
        IDS_DEDUP_ITEM_OVERHEAD, IDS_DEDUP_ITEM_GARBAGE_FULL };

    // Prepare all bindings and rows before exposing any result to the tree.
    for (const auto [index, volume] : std::views::enumerate(volumes))
    {
        for (CItem* root : volume->roots) boundaries[root] = static_cast<std::uint32_t>(index);
        for (const auto& [item, source] : volume->values)
        {
            auto& value = values[item];
            value.delta += source.delta;
            if (source.freed != Unknown) value.freed = (value.freed == Unknown ? 0 : value.freed) + source.freed;
            if (source.shared != Unknown) value.shared = (value.shared == Unknown ? 0 : value.shared) + source.shared;
            if (source.file != None) { value.volume = static_cast<std::uint32_t>(index); value.file = source.file; }
        }
        auto& owners = snapshot->owners[index];
        owners.reserve(volume->pseudo.size());
        for (const auto& pseudo : volume->pseudo)
        {
            auto& item = pseudoItems[{ pseudo.parent, pseudo.kind }];
            if (item == nullptr)
            {
                auto child = std::unique_ptr<CItem>(CItem::Create(IT_DEDUP, Localization::Lookup(names[pseudo.kind])));
                item = child.get();
                item->SetIndex(pseudo.kind);
                item->SetParent(pseudo.parent);
                item->SetDone();
                created.push_back(std::move(child));
                snapshot->pseudo.push_back(item);
            }
            item->SetSizePhysical(item->GetSizePhysicalRaw() + pseudo.bytes);
            owners.push_back(item);
        }
    }
    for (const auto& [item, value] : m_loaded)
    {
        values[item] = value;
        if (item->IsTypeOrFlag(IT_DEDUP)) snapshot->pseudo.push_back(const_cast<CItem*>(item));
    }

    // Display differences follow the existing hardlink grouping, while allocations stay untouched.
    std::unordered_map<const CItem*, CItem*> hardlinks;
    for (CItem* drive : CWinDirStatModel::Get()->GetRootItem()->GetDriveItems())
        if (CItem* links = drive->FindHardlinksItem(); links != nullptr)
            for (CItem* set : links->GetChildren()) for (CItem* group : set->GetChildren())
                for (CItem* entry : group->GetChildren()) hardlinks.emplace(entry->GetLinkedItem(), group);
    std::unordered_map<CItem*, LONGLONG> groupDeltas;
    std::vector<std::pair<const CItem*, LONGLONG>> deltas;
    for (const auto& [item, value] : values) if (item->IsLeaf() && value.delta != 0)
        deltas.emplace_back(item, value.delta);
    const auto propagate = [&](const CItem* parent, const LONGLONG delta)
    {
        for (; parent != nullptr; parent = parent->GetParent()) values[parent].delta += delta;
    };
    for (const auto& [item, delta] : deltas)
    {
        if (const auto group = hardlinks.find(item); group != hardlinks.end()) groupDeltas[group->second] = delta;
        else propagate(item->GetParent(), delta);
    }
    for (const auto& [group, delta] : groupDeltas) propagate(group, delta);
    for (const auto& child : created) propagate(child->GetParent(), static_cast<LONGLONG>(child->GetSizePhysicalRaw()));

    // Each folder frees ordinary allocations plus contributions owned only by that whole subtree.
    if (!m_imported)
    {
        struct Frame
        {
            const CItem* item;
            std::size_t next = 0;
            ULONGLONG total = 0;
            std::uint32_t volume = None;
            bool known = true;
            bool accounted = false;
        };
        const auto frameFor = [&](const CItem* item, std::uint32_t volume)
        {
            if (IsVolumeBoundary(item))
            {
                const auto boundary = boundaries.find(item);
                volume = boundary == boundaries.end() ? None : boundary->second;
            }
            const auto found = values.find(item);
            const ULONGLONG owned = found == values.end() || found->second.freed == Unknown ? 0 : found->second.freed;
            return Frame{ item, 0, owned, volume,
                !item->IsTypeOrFlag(ITF_MTP, ITF_REMOTE) && item->GetScanResult().outcome == ScanOutcome::Complete,
                volume != None };
        };
        std::vector<Frame> stack{ frameFor(CWinDirStatModel::Get()->GetRootItem(), None) };
        while (!stack.empty())
        {
            Frame& frame = stack.back();
            if (frame.next == frame.item->GetChildren().size())
            {
                const Frame done = frame;
                stack.pop_back();
                if (done.accounted || values.contains(done.item))
                {
                    auto& value = values[done.item];
                    value.freed = done.known ? done.total : Unknown;
                    if (done.known && value.shared == Unknown) value.shared = 0;
                    value.volume = done.volume;
                }
                if (!stack.empty())
                {
                    stack.back().total += done.total;
                    stack.back().known &= done.known;
                    stack.back().accounted |= done.accounted;
                }
                continue;
            }
            const CItem* child = frame.item->GetChildren()[frame.next++];
            if (child->IsTypeOrFlag(IT_FILE))
            {
                const auto found = values.find(child);
                const ULONGLONG freed = found != values.end() ? found->second.freed :
                    frame.volume == None ? Unknown : child->GetSizePhysicalRaw();
                if (freed == Unknown) frame.known = false;
                else frame.total += freed;
            }
            else if (!child->IsLeaf() && !child->IsTypeOrFlag(IT_HLINKS))
                stack.push_back(frameFor(child, frame.volume));
        }
    }
    std::unordered_map<CItem*, std::size_t> additions;
    for (const auto& child : created) additions[child->GetParent()]++;
    const auto publish = [&]
    {
        for (const auto& [parent, count] : additions)
        {
            auto& children = parent->GetFolderInfo()->m_children;
            children.reserve(children.size() + count);
        }
        for (const auto& [item, value] : values)
            if (value.delta != 0 && !item->IsLeaf()) const_cast<CItem*>(item)->UpwardSetUndone();
        for (const auto& [parent, count] : additions) parent->UpwardSetUndone();
        for (auto& child : created)
        {
            CItem* item = child.release();
            item->GetParent()->GetFolderInfo()->m_children.push_back(item);
        }
        {
            std::unique_lock lock(m_lock);
            for (const auto& [item, value] : values)
            {
                if (value.delta != 0) const_cast<CItem*>(item)->SetFlag(ITF_ACCOUNTED);
                if (value.file != None) const_cast<CItem*>(item)->SetReparseTag(IO_REPARSE_TAG_DEDUP);
            }
            m_snapshot = std::move(snapshot);
            m_volumes = std::move(volumes);
            decltype(m_loaded){}.swap(m_loaded);
        }
        for (const auto& [key, child] : pseudoItems)
            if (key.first->IsExpanded()) CFileTreeControl::Get()->OnChildAdded(key.first, child);
    };
    CMainFrame::Get()->InvokeInMessageThread(publish);
}

void CDedupAccounting::ClearProjection()
{
    std::unique_ptr<const Snapshot> snapshot;
    {
        std::unique_lock lock(m_lock);
        snapshot = std::move(m_snapshot);
        if (!snapshot) return;
        for (const auto& [item, value] : snapshot->values) const_cast<CItem*>(item)->SetFlag(ITF_ACCOUNTED, true);
    }
    for (const auto& [item, value] : snapshot->values)
        if (value.delta != 0 && !item->IsLeaf()) const_cast<CItem*>(item)->UpwardSetUndone();
    for (CItem* item : snapshot->pseudo)
    {
        CItem* parent = item->GetParent();
        parent->UpwardSetUndone();
        parent->RemoveChild(item);
    }
}

void CDedupAccounting::ExpandRefresh(std::vector<CItem*>& items) const
{
    for (auto*& item : items) if (item->IsTypeOrFlag(IT_DEDUP)) item = item->GetParent();
    if (!items.empty() && m_imported) items = { CWinDirStatModel::Get()->GetRootItem() };
}

void CDedupAccounting::Invalidate(const std::vector<CItem*>& items, const bool removal)
{
    if (items.empty()) return;
    std::unordered_set<std::wstring> paths;
    for (const CItem* item : items)
        if (auto path = GetVolumePath(item); !path.empty()) paths.insert(std::move(path));
    ClearProjection();
    std::unique_lock lock(m_lock);
    m_removed.clear();
    std::erase_if(m_volumes, [&](const auto& volume)
    {
        const auto refreshes = [&](const CItem* root)
        {
            return std::ranges::any_of(items, [&](const CItem* item) { return item->IsAncestorOf(root); });
        };
        const auto contains = [&](const CItem* root)
        {
            return std::ranges::any_of(items, [&](const CItem* item) { return root->IsAncestorOf(item); });
        };
        if (!paths.contains(volume->path) && !std::ranges::any_of(volume->roots,
            [&](const CItem* root) { return refreshes(root) || contains(root); })) return false;

        // A removal below the boundaries is applied to these results after the rescan; their bindings are not used.
        if (removal && !std::ranges::any_of(volume->roots, refreshes)) m_removed.push_back(volume);
        return true;
    });
    m_loaded.clear();
    m_storeProbes.clear();
    m_imported = false;
}

void CDedupAccounting::Clear()
{
    // The model destroys the tree immediately after releasing its accounting bindings.
    std::unique_lock lock(m_lock);
    m_snapshot.reset();
    m_volumes.clear();
    m_removed.clear();
    m_loaded.clear();
    m_storeProbes.clear();
    m_failures.clear();
    m_imported = false;
}

void CDedupAccounting::SetLoaded(const CItem* item, const std::optional<ULONGLONG> physical,
    const std::optional<ULONGLONG> freed, const std::optional<ULONGLONG> shared)
{
    Values value;
    if (item->IsLeaf() && physical) value.delta = static_cast<LONGLONG>(*physical - item->GetSizePhysicalRaw());
    value.freed = freed.value_or(Unknown);
    value.shared = shared.value_or(Unknown);
    if (item->IsTypeOrFlag(IT_FILE) && value.delta == 0 && !shared)
    {
        const auto parent = m_staged.find(item->GetParent());
        const bool known = parent != m_staged.end() && parent->second.freed != Unknown;
        if (known && freed == item->GetSizePhysicalRaw() || !known && !freed) return;
    }
    m_staged[item] = value;
}

void CDedupAccounting::CommitLoaded()
{
    std::unique_lock lock(m_lock);
    m_loaded = std::move(m_staged);
    m_imported = !m_loaded.empty();
}

void CDedupAccounting::DiscardLoaded()
{
    m_staged.clear();
}

ULONGLONG CDedupAccounting::GetPhysical(const CItem* item, const ULONGLONG allocation) const noexcept
{
    std::shared_lock lock(m_lock);
    if (m_snapshot)
        if (const auto value = m_snapshot->values.find(item); value != m_snapshot->values.end())
            return allocation + value->second.delta;
    return allocation;
}

std::optional<ULONGLONG> CDedupAccounting::GetFreed(const CItem* item) const
{
    if (item->IsTypeOrFlag(IT_HLINKS_FILE)) item = item->GetLinkedItem();
    std::shared_lock lock(m_lock);
    if (!m_snapshot) return std::nullopt;
    if (item->IsTypeOrFlag(IT_DEDUP))
    {
        if (item->GetIndex() != DEDUPITEM_GARBAGE && item->GetIndex() != DEDUPITEM_GARBAGE_FULL) return std::nullopt;
        const auto value = m_snapshot->values.find(item);
        return item->GetSizePhysicalRaw() + (value == m_snapshot->values.end() ? 0 : value->second.delta);
    }
    if (const auto found = m_snapshot->values.find(item); found != m_snapshot->values.end())
        return found->second.freed == Unknown ? std::nullopt : std::optional(found->second.freed);
    if (item->IsTypeOrFlag(IT_FILE) && item->GetParent() != nullptr)
        if (const auto parent = m_snapshot->values.find(item->GetParent());
            parent != m_snapshot->values.end() &&
            (parent->second.volume != None || m_imported && parent->second.freed != Unknown))
            return item->GetSizePhysicalRaw();
    return std::nullopt;
}

std::optional<ULONGLONG> CDedupAccounting::GetShared(const CItem* item) const
{
    if (item->IsTypeOrFlag(IT_HLINKS_FILE)) item = item->GetLinkedItem();
    std::shared_lock lock(m_lock);
    if (!m_snapshot) return std::nullopt;
    if (const auto value = m_snapshot->values.find(item); value != m_snapshot->values.end())
        return value->second.shared == Unknown ? std::nullopt : std::optional(value->second.shared);
    return std::nullopt;
}

bool CDedupAccounting::HasData() const
{
    std::shared_lock lock(m_lock);
    return m_snapshot != nullptr;
}

bool CDedupAccounting::HasSharingIndex(const CItem* item) const
{
    std::shared_lock lock(m_lock);
    if (!m_snapshot) return false;
    if (const auto value = m_snapshot->values.find(item); value != m_snapshot->values.end())
        if (value->second.volume != None) return true;
    if (item->IsTypeOrFlag(IT_DEDUP) && item->GetIndex() != DEDUPITEM_SHARED) return false;
    return std::ranges::any_of(m_snapshot->volumes, [&](const auto& volume)
    {
        if (item->IsTypeOrFlag(IT_DEDUP))
            return std::ranges::any_of(volume->pseudo, [&](const Pseudo& pseudo)
            { return pseudo.kind == DEDUPITEM_SHARED && pseudo.parent == item->GetParent(); });
        return !item->IsLeaf() && std::ranges::any_of(volume->roots,
            [&](const CItem* root) { return item->IsAncestorOf(root); });
    });
}

bool CDedupAccounting::IsDedupVolume(const CItem* item)
{
    const CItem* root = item;
    while (root->GetParent() != nullptr && !IsVolumeBoundary(root)) root = root->GetParent();
    {
        std::shared_lock lock(m_lock);
        if (const auto found = m_storeProbes.find(root); found != m_storeProbes.end()) return found->second;
    }
    // Only a volume with Data Deduplication has its folder in System Volume Information. Without administrator
    // rights the folder cannot be opened, but the access error still shows that it exists.
    const auto path = GetVolumePath(root);
    const std::wstring folder = path + std::wstring(wds::strSystemVolumeInformation) + L"\\Dedup";
    const bool hasDedupFolder = !path.empty() &&
        (GetFileAttributesW(folder.c_str()) != INVALID_FILE_ATTRIBUTES || GetLastError() == ERROR_ACCESS_DENIED);
    std::unique_lock lock(m_lock);
    m_storeProbes[root] = hasDedupFolder;
    return hasDedupFolder;
}

std::vector<std::wstring> CDedupAccounting::TakeFailures()
{
    std::unique_lock lock(m_lock);
    return std::exchange(m_failures, {});
}

CDedupAccounting::SharingResult CDedupAccounting::QuerySharing(const std::vector<CItem*>& items) const
{
    SharingResult result;
    std::shared_lock lock(m_lock);
    if (!m_snapshot)
    {
        result.error = Localization::Lookup(IDS_DEDUP_SHARING_NOT_ACCOUNTED);
        return result;
    }
    std::unordered_set<const CItem*> selected, selectedOwners;
    std::vector<std::vector<std::uint32_t>> files(m_snapshot->volumes.size());
    std::unordered_set<CItem*> roots;
    for (CItem* item : items) roots.insert(item->IsTypeOrFlag(IT_HLINKS_FILE) ? item->GetLinkedItem() : item);
    std::vector<CItem*> pending(roots.begin(), roots.end());
    std::erase_if(pending, [&](const CItem* item)
    {
        for (const CItem* parent = item->GetParent(); parent != nullptr; parent = parent->GetParent())
            if (roots.contains(const_cast<CItem*>(parent))) return true;
        return false;
    });
    while (!pending.empty())
    {
        CItem* item = pending.back(); pending.pop_back();
        if (item->IsTypeOrFlag(IT_DEDUP))
        {
            if (item->GetIndex() == DEDUPITEM_SHARED) selectedOwners.insert(item);
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            if (const auto value = m_snapshot->values.find(item);
                value != m_snapshot->values.end() && value->second.file != None && selected.insert(item).second)
                files[value->second.volume].push_back(value->second.file);
        }
        else if (!item->IsLeaf() && !item->IsTypeOrFlag(IT_HLINKS)) pending.append_range(item->GetChildren());
    }

    // The union includes each physical chunk once, even for overlapping folders and pseudo rows.
    std::unordered_map<CItem*, std::pair<ULONGLONG, ULONGLONG>> overlap;
    for (const auto [index, volume] : std::views::enumerate(m_snapshot->volumes))
    {
        auto& requested = files[index];
        std::ranges::sort(requested);
        requested.erase(std::ranges::unique(requested).begin(), requested.end());
        if (requested.empty() && selectedOwners.empty()) continue;
        std::vector<bool> seen(volume->chunkBytes.size());
        std::vector<std::uint32_t> chunks;
        const auto add = [&](const std::uint32_t chunk)
        {
            if (seen[chunk]) return;
            seen[chunk] = true;
            chunks.push_back(chunk);
        };
        for (const auto file : requested)
        {
            result.success = true;
            result.queriedBytes += volume->fileExclusive[file];
            for (auto i = volume->fileOffsets[file]; i < volume->fileOffsets[file + 1]; i++)
                add(volume->fileChunks[i]);
        }
        if (!selectedOwners.empty()) for (std::uint32_t chunk = 0; chunk < volume->chunkOwner.size(); chunk++)
        {
            const auto owner = volume->chunkOwner[chunk];
            if (owner != None && selectedOwners.contains(m_snapshot->owners[index][owner])) add(chunk);
        }
        for (const auto chunk : chunks)
        {
            result.success = true;
            const auto bytes = volume->chunkBytes[chunk];
            result.queriedBytes += bytes;
            if (volume->chunkExternal[chunk]) result.outsideBytes += bytes;
            bool listed = false;
            for (auto i = volume->chunkOffsets[chunk]; i < volume->chunkOffsets[chunk + 1]; i++)
            {
                const auto file = volume->chunkFiles[i];
                for (auto j = volume->entryOffsets[file]; j < volume->entryOffsets[file + 1]; j++)
                {
                    CItem* entry = volume->entries[j];
                    if (selected.contains(entry)) continue;
                    auto& row = overlap[entry];
                    row.first += bytes;
                    row.second = volume->fileBytes[file];
                    listed = true;
                }
            }
            if (listed) result.sharedBytes += bytes;
        }
    }
    for (const auto& [item, bytes] : overlap) result.rows.push_back({ item, bytes.first, bytes.second });
    std::ranges::sort(result.rows, std::greater{}, &SharingRow::sharedBytes);
    if (!result.success) result.error = Localization::Lookup(IDS_DEDUP_SHARING_NOT_ACCOUNTED);
    return result;
}

void CDedupAccounting::CollectRelated(const CItem* item,
    std::unordered_set<const CItem*>& related, const std::size_t limit) const
{
    if (item->IsTypeOrFlag(IT_HLINKS_FILE)) item = item->GetLinkedItem();
    std::shared_lock lock(m_lock);
    if (!m_snapshot) return;
    const auto visit = [&](const std::size_t index, const std::uint32_t chunk)
    {
        const auto& volume = *m_snapshot->volumes[index];
        const auto owner = volume.chunkOwner[chunk];
        if (owner != None && related.size() < limit && m_snapshot->owners[index][owner] != item)
            related.insert(m_snapshot->owners[index][owner]);
        for (auto i = volume.chunkOffsets[chunk]; i < volume.chunkOffsets[chunk + 1]; i++)
        {
            const auto file = volume.chunkFiles[i];
            for (auto j = volume.entryOffsets[file]; j < volume.entryOffsets[file + 1]; j++)
            {
                if (related.size() >= limit) return;
                if (volume.entries[j] != item) related.insert(volume.entries[j]);
            }
        }
    };
    if (item->IsTypeOrFlag(IT_DEDUP) && item->GetIndex() == DEDUPITEM_SHARED)
    {
        for (const auto [index, volume] : std::views::enumerate(m_snapshot->volumes))
            for (std::uint32_t chunk = 0; chunk < volume->chunkOwner.size(); chunk++)
            {
                if (related.size() >= limit) return;
                const auto owner = volume->chunkOwner[chunk];
                if (owner != None && m_snapshot->owners[index][owner] == item) visit(index, chunk);
            }
        return;
    }
    const auto value = m_snapshot->values.find(item);
    if (value == m_snapshot->values.end() || value->second.file == None) return;
    const auto& volume = *m_snapshot->volumes[value->second.volume];
    const auto file = value->second.file;
    for (auto i = volume.fileOffsets[file]; i < volume.fileOffsets[file + 1] && related.size() < limit; i++)
        visit(value->second.volume, volume.fileChunks[i]);
}
