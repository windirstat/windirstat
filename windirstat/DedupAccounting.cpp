// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DedupAccounting.h"

CDedupAccounting& CDedupAccounting::Get()
{
    // Never destroyed: items unregister during static destruction, after a local static would be gone
    static auto* instance = new CDedupAccounting;
    return *instance;
}

std::wstring CDedupAccounting::GetVolumePath(const CItem* volumeRoot)
{
    // Folder roots and mount points are resolved so "D:\Mount" maps to its own volume
    std::wstring path = volumeRoot->GetPathLong();
    if (volumeRoot->IsTypeOrFlag(IT_DRIVE)) return path;
    if (path.empty()) return {};

    std::wstring volume(path.size() + 2, L'\0');
    if (!GetVolumePathNameW(path.c_str(), volume.data(), static_cast<DWORD>(volume.size()))) return {};
    volume.resize(wcslen(volume.c_str()));
    return volume;
}

CItem* CDedupAccounting::AddPseudoItem(CItem* parent, const DEDUPITEMKIND kind, const ULONGLONG bytes)
{
    static constexpr std::array names{ IDS_DEDUP_ITEM_SHARED, IDS_DEDUP_ITEM_EXTERNAL, IDS_DEDUP_ITEM_GARBAGE,
        IDS_DEDUP_ITEM_OVERHEAD, IDS_DEDUP_ITEM_GARBAGE_FULL };

    // Like free space items: undone parent, finished child, then the insertion that propagates the size
    parent->UpwardSetUndone();
    CItem* item = CItem::Create(IT_DEDUP, Localization::Lookup(names[kind]));
    item->SetIndex(kind);
    item->SetSizePhysical(bytes);
    item->SetDone();
    parent->AddChild(item);
    return item;
}

void CDedupAccounting::AddFailure(const std::wstring& volume, const std::wstring& reason)
{
    std::unique_lock lock(m_lock);
    m_failures.push_back(std::format(L"{}: {}", volume, reason));
}

std::vector<std::wstring> CDedupAccounting::TakeFailures()
{
    std::unique_lock lock(m_lock);
    return std::exchange(m_failures, {});
}

void CDedupAccounting::RunPostScan(const std::vector<CItem*>& scanRoots, const CDedupStore::CancelCheck& cancelled)
{
    // Collect candidates per volume; files below a mount belong to that volume's own chunk store
    if (cancelled && cancelled()) return;
    std::unordered_map<CItem*, std::vector<CItem*>> perVolume;
    std::unordered_set<const CItem*> visitedRoots;
    std::unordered_set<const CItem*> collected; // a mounted volume is also reached through its outer volume
    {
        std::shared_lock lock(m_lock);
        for (CItem* root : scanRoots)
        {
            if (root->IsTypeOrFlag(ITF_MTP, ITF_REMOTE)) continue;
            CItem* volumeRoot = root->GetVolumeRoot();
            if (!visitedRoots.insert(volumeRoot).second) continue;
            if (m_indexes.contains(volumeRoot) || m_loadedVolumes.contains(volumeRoot)) continue;

            // The MFT engine cannot see the tag of a non-resident reparse attribute, so reparse points
            // with an unknown tag are verified too; partially optimized files are not sparse
            for (CItem* file : CItem::GetItemsRecursive({ volumeRoot }, [](const CItem* item)
                {
                    if (!item->IsTypeOrFlag(IT_FILE) || item->IsTypeOrFlag(ITF_MTP, ITF_REMOTE)) return false;
                    if (item->IsTypeOrFlag(ITRP_DEDUP)) return true;
                    const DWORD attributes = item->GetAttributes();
                    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                        (item->GetRawType() & ITRP_MASK) == ITRP_NONE;
                }))
            {
                CItem* fileVolume = file->GetVolumeRoot();
                if (m_indexes.contains(fileVolume) || m_loadedVolumes.contains(fileVolume) || !collected.insert(file).second) continue;
                perVolume[fileVolume].push_back(file);
            }
        }
    }
    if (perVolume.empty()) return;

    std::vector<std::pair<CItem*, std::vector<CItem*>*>> volumes;
    for (auto& [volumeRoot, files] : perVolume) volumes.emplace_back(volumeRoot, &files);
    std::for_each(std::execution::par, volumes.begin(), volumes.end(), [&](auto& volume)
    {
        // An exception would terminate the parallel loop; very large stores can exhaust memory
        try
        {
            ProcessVolume(volume.first, *volume.second, cancelled);
        }
        catch (const std::bad_alloc&)
        {
            AddFailure(volume.first->GetPath(), Localization::Lookup(IDS_DEDUP_OUT_OF_MEMORY));
        }
        catch (const std::length_error&)
        {
            AddFailure(volume.first->GetPath(), Localization::Lookup(IDS_DEDUP_OUT_OF_MEMORY));
        }
    });
}

void CDedupAccounting::ProcessVolume(CItem* volumeRoot, std::vector<CItem*>& files, const CDedupStore::CancelCheck& cancelled)
{
    const std::wstring volumePath = GetVolumePath(volumeRoot);
    if (volumePath.empty()) return;

    // Resolve each candidate's stream map digests from its reparse data
    struct Entry
    {
        CItem* item = nullptr;
        std::vector<CDedupStore::Hash16> maps;
        ULONGLONG onDisk = 0;
        ULONGLONG exclusive = 0;
        double effective = 0;
    };
    std::vector<Entry> entries(files.size());
    CDedupStore::CancelLatch latch(cancelled);
    std::for_each(std::execution::par, entries.begin(), entries.end(), [&](Entry& entry)
    {
        entry.item = files[static_cast<std::size_t>(&entry - entries.data())];
        if (latch()) return;
        if (auto info = CDedupStore::ReadReparseData(entry.item->GetPathLong()); info.has_value())
        {
            entry.maps = std::move(info->maps);

            // Each item is handled by exactly one thread, so the flag update is not shared
            entry.item->SetReparseTag(IO_REPARSE_TAG_DEDUP);
        }
    });
    std::erase_if(entries, [](const Entry& entry) { return entry.maps.empty(); });
    if (entries.empty() || latch() || (cancelled && cancelled())) return;

    // Undone volumes are re-sorted and marked done again by the scan finalization
    volumeRoot->UpwardSetUndone();
    {
        std::unique_lock lock(m_lock);
        m_dedupVolumes.insert(volumePath);
    }

    const std::wstring storePath = CDedupStore::FindStore(volumePath);
    if (storePath.empty())
    {
        AddFailure(volumePath, Localization::Lookup(IDS_DEDUP_NO_ACCESS));
        return;
    }

    // Live maps come from existing files; the store only learns of deletions at garbage collection
    const auto liveMaps = CDedupStore::EnumerateVolumeMaps(volumePath, cancelled);
    if (cancelled && cancelled()) return;
    CDedupStore store;
    if (!store.Load(storePath, liveMaps.has_value() ? &*liveMaps : nullptr, cancelled))
    {
        if (!(cancelled && cancelled())) AddFailure(volumePath, store.GetError());
        return;
    }

    // A scanned chunk store is replaced by the attributed data, so its garbage and metadata
    // are shown too; nothing has changed yet if that split fails
    const auto metric = static_cast<DEDUPMETRIC>(COptions::DedupPhysicalMetric.Obj());
    std::wstring storeDirectory = storePath;
    if (storeDirectory.starts_with(L"\\\\?\\")) storeDirectory.erase(0, 4);
    if (const auto separator = storeDirectory.rfind(L'\\'); separator != std::wstring::npos) storeDirectory.resize(separator);
    CItem* const storeItem = volumeRoot->FindItemByPath(storeDirectory);
    CItem* const chunkStore = metric != DEDUP_ALLOCATION ? storeItem : nullptr; // neutralized below
    if (chunkStore != nullptr && !store.ComputeSpaceUsage(cancelled))
    {
        if (!(cancelled && cancelled())) AddFailure(volumePath, store.GetError());
        return;
    }

    // Each stream map is resolved once and remembers every entry (hardlinks too) using it
    std::vector<std::pair<CDedupStore::Hash16, std::uint32_t>> mapEntries;
    for (const auto [index, entry] : std::views::enumerate(entries))
    {
        for (const auto& hash : entry.maps) mapEntries.emplace_back(hash, static_cast<std::uint32_t>(index));
    }
    std::ranges::sort(mapEntries);
    mapEntries.erase(std::ranges::unique(mapEntries).begin(), mapEntries.end());
    std::vector<CDedupStore::Hash16> requests;
    std::vector<std::uint32_t> mapUserOffsets;
    std::vector<std::uint32_t> mapUsers;
    for (const auto& [hash, entry] : mapEntries)
    {
        if (requests.empty() || requests.back() != hash)
        {
            requests.push_back(hash);
            mapUserOffsets.push_back(static_cast<std::uint32_t>(mapUsers.size()));
        }
        mapUsers.push_back(entry);
    }
    mapUserOffsets.push_back(static_cast<std::uint32_t>(mapUsers.size()));
    mapEntries = {};

    // Chunks of one map of one file are exclusive; the others are recorded as (chunk, map)
    // visits. Hardlinks count as distinct files, since deleting one link frees nothing.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> visits;
    const bool resolved = store.Resolve(requests, [&](const std::size_t request, const std::uint32_t chunkIndex,
        const CDedupStore::ChunkInfo& chunk)
    {
        const std::uint32_t first = mapUserOffsets[request];
        const std::uint32_t last = mapUserOffsets[request + 1];
        for (std::uint32_t i = first; i < last; i++)
            entries[mapUsers[i]].effective += static_cast<double>(chunk.diskSize) / std::max(chunk.refs, 1u);
        if (chunk.refs <= 1 && last - first == 1)
        {
            entries[mapUsers[first]].onDisk += chunk.diskSize;
            entries[mapUsers[first]].exclusive += chunk.diskSize;
        }
        else visits.emplace_back(chunkIndex, static_cast<std::uint32_t>(request));
    }, cancelled);
    if (!resolved || (cancelled && cancelled()))
    {
        if (!(cancelled && cancelled())) AddFailure(volumePath, store.GetError());
        return;
    }

    // Per chunk: its distinct users, whether unscanned files use it too, and their common folder
    std::ranges::sort(visits);
    VolumeIndex index;
    index.files.resize(entries.size());
    std::unordered_map<CItem*, ULONGLONG> owned;    // folder -> chunk bytes freed only with it
    std::unordered_map<CItem*, FolderInfo> folders; // shared bytes per folder
    std::unordered_map<CItem*, std::uint32_t> stamps;
    std::vector<CItem*> chunkLca;
    ULONGLONG externalBytes = 0;
    std::vector<std::uint32_t> chunkUsers;
    index.chunkOffsets.push_back(0);
    for (std::size_t begin = 0; begin < visits.size();)
    {
        const std::uint32_t chunkIndex = visits[begin].first;
        std::size_t end = begin;
        chunkUsers.clear();
        for (; end < visits.size() && visits[end].first == chunkIndex; end++)
        {
            const std::uint32_t map = visits[end].second;
            chunkUsers.insert(chunkUsers.end(), mapUsers.begin() + mapUserOffsets[map], mapUsers.begin() + mapUserOffsets[map + 1]);
        }
        std::ranges::sort(chunkUsers);
        chunkUsers.erase(std::ranges::unique(chunkUsers).begin(), chunkUsers.end());
        const std::size_t mapVisits = end - begin; // distinct maps, as each is resolved once
        begin = end;

        const CDedupStore::ChunkInfo& chunk = store.GetChunk(chunkIndex);
        const bool external = mapVisits < chunk.refs;
        for (const std::uint32_t user : chunkUsers) entries[user].onDisk += chunk.diskSize;

        // Several stream maps of one file (one per GB) still make it exclusive
        if (!external && chunkUsers.size() == 1)
        {
            entries[chunkUsers.front()].exclusive += chunk.diskSize;
            continue;
        }

        // The users are distinct files, so their common folder is above the first one
        CItem* lca = nullptr;
        if (!external)
        {
            lca = entries[chunkUsers.front()].item->GetParent();
            for (const std::uint32_t user : chunkUsers)
                while (!lca->IsAncestorOf(entries[user].item)) lca = lca->GetParent();
            owned[lca] += chunk.diskSize;
        }
        else externalBytes += chunk.diskSize;

        // Folders between a user and the common folder (or the volume root) hold data used elsewhere too
        const auto stamp = static_cast<std::uint32_t>(chunkLca.size() + 1);
        for (const std::uint32_t user : chunkUsers)
        {
            for (CItem* parent = entries[user].item->GetParent(); parent != nullptr && parent != lca; parent = parent->GetParent())
            {
                auto& mark = stamps[parent];
                if (mark == stamp) break;
                mark = stamp;
                folders[parent].shared += chunk.diskSize;
                if (parent == volumeRoot) break;
            }
        }

        index.chunkFiles.insert(index.chunkFiles.end(), chunkUsers.begin(), chunkUsers.end());
        index.chunkOffsets.push_back(static_cast<std::uint32_t>(index.chunkFiles.size()));
        index.chunkBytes.push_back(chunk.diskSize);
        index.chunkExternal.push_back(external ? 1 : 0);
        chunkLca.push_back(lca);
    }
    visits = {};
    stamps = {};

    // Invert into file -> shared chunks
    index.fileOffsets.assign(entries.size() + 1, 0);
    for (const std::uint32_t user : index.chunkFiles) index.fileOffsets[user + 1]++;
    for (std::size_t i = 1; i < index.fileOffsets.size(); i++) index.fileOffsets[i] += index.fileOffsets[i - 1];
    index.fileChunks.resize(index.chunkFiles.size());
    {
        std::vector<std::uint32_t> cursor(index.fileOffsets.begin(), index.fileOffsets.end() - 1);
        for (std::uint32_t chunk = 0; chunk + 1 < index.chunkOffsets.size(); chunk++)
        {
            for (std::uint32_t i = index.chunkOffsets[chunk]; i < index.chunkOffsets[chunk + 1]; i++)
            {
                index.fileChunks[cursor[index.chunkFiles[i]]++] = chunk;
            }
        }
    }

    // Per-file values and the size each one contributes under the metric
    std::vector<FileInfo> infos(entries.size());
    for (const auto [slot, entry] : std::views::enumerate(entries))
    {
        FileInfo& info = infos[static_cast<std::size_t>(slot)];
        info.onDisk = entry.onDisk;
        info.exclusive = entry.exclusive;

        // The metric is added to what the file system still allocates (e.g. ranges recalled after optimization)
        const ULONGLONG raw = entry.item->GetSizePhysicalRaw();
        const ULONGLONG logical = entry.item->GetSizeLogical();
        switch (metric)
        {
        case DEDUP_SHARED_ONCE:
        case DEDUP_EXCLUSIVE: info.applied = info.exclusive; break;
        case DEDUP_EFFECTIVE: info.applied = static_cast<ULONGLONG>(std::llround(entry.effective)); break;
        case DEDUP_LOGICAL: info.applied = logical > raw ? logical - raw : 0; break;
        default: info.applied = 0; break;
        }

        index.files[static_cast<std::size_t>(slot)] = entry.item;
        index.fileOnDisk.push_back(info.onDisk);
        index.fileExclusive.push_back(info.exclusive);

        // Every folder holding an optimized file gets a Shared value, if only 0
        for (CItem* parent = entry.item->GetParent(); parent != nullptr; parent = parent->GetParent())
        {
            folders.try_emplace(parent);
            if (parent == volumeRoot) break;
        }
    }
    // ComputeTotals adds the chunks shared only below a folder to that folder
    for (const auto& [node, bytes] : owned) folders[node].owned += bytes;

    // Finalization re-sorts only the folders that are marked undone
    std::unordered_set<CItem*> touchedParents;
    for (const auto [slot, entry] : std::views::enumerate(entries))
    {
        entry.item->UpwardAddSizePhysical(infos[static_cast<std::size_t>(slot)].applied);
        touchedParents.insert(entry.item->GetParent());
    }

    // The chunk store holds the bytes now attributed to files and pseudo items
    std::vector<std::pair<CItem*, ULONGLONG>> neutralized;
    if (chunkStore != nullptr)
    {
        for (CItem* file : CItem::GetItemsRecursive({ chunkStore }, [](const CItem* item)
            { return item->IsTypeOrFlag(IT_FILE) && item->GetSizePhysicalRaw() > 0; }))
        {
            const ULONGLONG bytes = file->GetSizePhysicalRaw();
            file->UpwardSubtractSizePhysical(bytes);
            neutralized.emplace_back(file, bytes);
            touchedParents.insert(file->GetParent());
        }
    }

    // Pseudo items are added without m_lock: insertion may wait for the UI thread, which reads the maps
    std::vector<std::pair<CItem*, ULONGLONG>> pseudo;
    std::unordered_map<CItem*, CItem*> sharedItems; // common folder -> its pseudo item
    if (metric == DEDUP_SHARED_ONCE)
    {
        for (const auto& [node, bytes] : owned)
        {
            if (bytes == 0) continue;
            CItem* item = AddPseudoItem(node, DEDUPITEM_SHARED, bytes);
            sharedItems.emplace(node, item);
            pseudo.emplace_back(item, bytes);
        }
        if (externalBytes != 0) pseudo.emplace_back(AddPseudoItem(volumeRoot, DEDUPITEM_EXTERNAL, externalBytes), externalBytes);
    }
    if (chunkStore != nullptr)
    {
        // Garbage a regular garbage collection leaves alone needs a full one
        const auto& space = store.GetSpaceUsage();
        const ULONGLONG fullOnly = space.garbageBytes - space.regularGarbageBytes;
        if (space.regularGarbageBytes != 0)
            pseudo.emplace_back(AddPseudoItem(volumeRoot, DEDUPITEM_GARBAGE, space.regularGarbageBytes), space.regularGarbageBytes);
        if (fullOnly != 0) pseudo.emplace_back(AddPseudoItem(volumeRoot, DEDUPITEM_GARBAGE_FULL, fullOnly), fullOnly);
        if (space.overheadBytes != 0)
            pseudo.emplace_back(AddPseudoItem(volumeRoot, DEDUPITEM_OVERHEAD, space.overheadBytes), space.overheadBytes);
    }
    index.chunkOwner.resize(chunkLca.size(), nullptr);
    for (std::size_t chunk = 0; chunk < chunkLca.size(); chunk++)
    {
        if (const auto it = sharedItems.find(chunkLca[chunk]); it != sharedItems.end()) index.chunkOwner[chunk] = it->second;
    }

    {
        std::unique_lock lock(m_lock);
        for (const auto [slot, entry] : std::views::enumerate(entries))
        {
            m_files[entry.item] = infos[static_cast<std::size_t>(slot)];
            m_slots[entry.item] = { volumeRoot, static_cast<std::uint32_t>(slot) };
        }
        for (auto& [folder, info] : folders) m_folders[folder] = info;
        for (const auto& [item, bytes] : neutralized) m_neutralized[item] = bytes;
        for (const auto& [item, bytes] : pseudo) m_pseudo[item] = bytes;
        if (storeItem != nullptr) m_chunkStores.insert(storeItem);
        m_indexes[volumeRoot] = std::move(index);
    }

    for (CItem* parent : touchedParents)
    {
        if (parent != nullptr) parent->UpwardSetUndone();
    }

    VTRACE(L"Dedup accounting for {}: {} files, {} maps, {} shared chunks, {} external bytes",
        volumePath, entries.size(), requests.size(), chunkLca.size(), externalBytes);
}

void CDedupAccounting::Revert(const CItem* volumeRoot)
{
    // Collect under the lock, then change sizes and remove pseudo items without
    // it: removal deletes items on the UI thread, whose destructor unregisters
    const auto inScope = [volumeRoot](const CItem* item) { return volumeRoot->IsAncestorOf(item); };
    std::vector<std::pair<CItem*, ULONGLONG>> applied;
    std::vector<std::pair<CItem*, ULONGLONG>> neutralized;
    std::vector<CItem*> pseudo;
    {
        std::unique_lock lock(m_lock);
        std::erase_if(m_files, [&](const auto& entry)
        {
            if (!inScope(entry.first)) return false;
            if (entry.second.applied != 0) applied.emplace_back(const_cast<CItem*>(entry.first), entry.second.applied);
            m_slots.erase(entry.first);
            return true;
        });
        std::erase_if(m_neutralized, [&](const auto& entry)
        {
            if (!inScope(entry.first)) return false;
            neutralized.emplace_back(const_cast<CItem*>(entry.first), entry.second);
            return true;
        });
        std::erase_if(m_pseudo, [&](const auto& entry)
        {
            if (!inScope(entry.first)) return false;
            pseudo.push_back(const_cast<CItem*>(entry.first));
            return true;
        });
        std::erase_if(m_folders, [&](const auto& entry) { return inScope(entry.first); });
        std::erase_if(m_indexes, [&](const auto& entry) { return inScope(entry.first); });
        std::erase_if(m_loadedVolumes, inScope);
        std::erase_if(m_chunkStores, inScope);

        // Values a loaded report stored for the enclosing folders include the volume
        for (const CItem* parent = volumeRoot->GetParent(); parent != nullptr; parent = parent->GetParent())
        {
            if (const auto folder = m_folders.find(parent); folder != m_folders.end() && folder->second.loaded)
                m_folders.erase(folder);
        }

        // Sizes change below, so the totals are recomputed after the next scan
        m_folderTotals.clear();
    }

    // Parents are re-sorted at finalization only if they are marked undone
    std::unordered_set<CItem*> touchedParents;
    for (const auto& [item, bytes] : applied)
    {
        item->UpwardSubtractSizePhysical(bytes);
        touchedParents.insert(item->GetParent());
    }
    for (const auto& [item, bytes] : neutralized)
    {
        item->UpwardAddSizePhysical(bytes);
        touchedParents.insert(item->GetParent());
    }
    for (CItem* item : pseudo)
    {
        CItem* parent = item->GetParent();
        parent->UpwardSubtractSizePhysical(item->GetSizePhysical());
        parent->UpwardSetUndone();
        parent->RemoveChild(item);
    }
    for (CItem* parent : touchedParents)
    {
        if (parent != nullptr) parent->UpwardSetUndone();
    }
}

bool CDedupAccounting::HasLoadedData(const CItem* volumeRoot) const
{
    std::shared_lock lock(m_lock);
    return m_loadedVolumes.contains(volumeRoot);
}

std::vector<CItem*> CDedupAccounting::GetLoadedVolumesBelow(const CItem* volumeRoot) const
{
    std::shared_lock lock(m_lock);
    std::vector<CItem*> volumes;
    for (const CItem* volume : m_loadedVolumes)
    {
        if (volume != volumeRoot && volumeRoot->IsAncestorOf(volume)) volumes.push_back(const_cast<CItem*>(volume));
    }
    return volumes;
}

void CDedupAccounting::Clear()
{
    std::unique_lock lock(m_lock);
    m_files.clear();
    m_folders.clear();
    m_pseudo.clear();
    m_neutralized.clear();
    m_indexes.clear();
    m_slots.clear();
    m_failures.clear();
    m_loadedVolumes.clear();
    m_folderTotals.clear();
    m_chunkStores.clear();
    m_dedupVolumes.clear();
    m_storeProbes.clear();
}

void CDedupAccounting::Unregister(const CItem* item)
{
    std::unique_lock lock(m_lock);
    m_files.erase(item);
    m_pseudo.erase(item);

    // Keep the index free of dangling items; lookups skip empty slots
    if (const auto slot = m_slots.find(item); slot != m_slots.end())
    {
        if (const auto index = m_indexes.find(slot->second.first); index != m_indexes.end())
            index->second.files[slot->second.second] = nullptr;
        m_slots.erase(slot);
    }
}

void CDedupAccounting::SetLoaded(const CItem* item, const ULONGLONG freed, const ULONGLONG shared)
{
    std::unique_lock lock(m_lock);
    m_staged.emplace_back(item, freed, shared);
}

void CDedupAccounting::CommitLoaded()
{
    std::unique_lock lock(m_lock);
    for (const auto& [item, freed, shared] : m_staged)
    {
        // Only optimized files (the only files staged) and pseudo items carry deduplicated bytes
        if (item->IsTypeOrFlag(IT_FILE, IT_DEDUP) && m_loadedVolumes.insert(item->GetVolumeRoot()).second)
        {
            if (const auto volumePath = GetVolumePath(item->GetVolumeRoot()); !volumePath.empty()) m_dedupVolumes.insert(volumePath);
        }
        if (item->IsTypeOrFlag(IT_FILE))
        {
            FileInfo info;
            info.exclusive = freed;
            info.onDisk = freed + shared;
            m_files[item] = info;
        }
        else if (item->IsTypeOrFlag(IT_DEDUP))
        {
            m_pseudo[item] = item->GetSizePhysicalRaw();
        }
        else if (item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE, IT_MYCOMPUTER))
        {
            FolderInfo info;
            info.freed = freed;
            info.loaded = true;
            info.shared = shared;
            m_folders[item] = info;
        }
    }
    m_staged.clear();
}

void CDedupAccounting::DiscardLoaded()
{
    std::unique_lock lock(m_lock);
    m_staged.clear();
}

ULONGLONG CDedupAccounting::GetFileFreedLocked(const CItem* file, const bool checkChunkStore) const
{
    // Another link keeps the data of a hardlinked file
    if (file->IsTypeOrFlag(ITF_HARDLINK)) return 0;

    // Chunk store files hold the data already counted for the optimized files
    if (checkChunkStore && !m_chunkStores.empty())
    {
        for (const CItem* parent = file->GetParent(); parent != nullptr; parent = parent->GetParent())
            if (m_chunkStores.contains(parent)) return 0;
    }

    const ULONGLONG size = file->GetSizePhysicalRaw();
    const auto info = m_files.find(file);
    if (info == m_files.end()) return size;

    // A loaded report stores the complete value; otherwise the clusters the
    // file system still allocates add to the exclusive chunks
    if (!m_loadedVolumes.empty() && m_loadedVolumes.contains(file->GetVolumeRoot())) return info->second.exclusive;
    return size - std::min(size, info->second.applied) + info->second.exclusive;
}

std::optional<ULONGLONG> CDedupAccounting::GetFreed(const CItem* item) const
{
    std::shared_lock lock(m_lock);
    if (item->IsTypeOrFlag(IT_FILE))
    {
        if (m_folderTotals.empty() && !m_files.contains(item)) return std::nullopt;
        return GetFileFreedLocked(item);
    }
    if (item->IsTypeOrFlag(IT_DEDUP))
    {
        // Garbage is freed by the next garbage collection
        const auto pseudo = m_pseudo.find(item);
        if (pseudo == m_pseudo.end() || (item->GetIndex() != DEDUPITEM_GARBAGE && item->GetIndex() != DEDUPITEM_GARBAGE_FULL))
            return std::nullopt;
        return pseudo->second;
    }
    if (const auto folder = m_folders.find(item); folder != m_folders.end() && folder->second.loaded) return folder->second.freed;
    if (const auto total = m_folderTotals.find(item); total != m_folderTotals.end()) return total->second;
    return std::nullopt;
}

void CDedupAccounting::ComputeTotals(const CItem* root)
{
    if (root == nullptr) return;

    // Post-order walk: a folder frees its children plus the chunks shared only below it;
    // pseudo items, free and unknown space and the hardlink index free nothing
    std::unordered_map<const CItem*, ULONGLONG> totals;
    {
        std::shared_lock lock(m_lock);
        struct Frame
        {
            const CItem* folder;
            std::size_t next;
            ULONGLONG sum;
            bool inChunkStore;
        };
        const auto ownFreed = [this](const CItem* folder)
        {
            const auto info = m_folders.find(folder);
            return info == m_folders.end() ? 0 : info->second.owned;
        };
        std::vector<Frame> stack{ { root, 0, ownFreed(root), m_chunkStores.contains(root) } };
        while (!stack.empty())
        {
            Frame& frame = stack.back();
            const auto& children = frame.folder->GetChildren();
            if (frame.next == children.size())
            {
                const Frame done = frame;
                stack.pop_back();
                totals.emplace(done.folder, done.sum);
                if (!stack.empty()) stack.back().sum += done.sum;
                continue;
            }

            const CItem* child = children[frame.next++];
            if (child->IsTypeOrFlag(IT_FILE)) frame.sum += frame.inChunkStore ? 0 : GetFileFreedLocked(child, false);
            else if (child->IsLeaf() || child->IsTypeOrFlag(IT_HLINKS)) continue;
            else if (const auto loaded = m_folders.find(child); loaded != m_folders.end() && loaded->second.loaded)
                frame.sum += loaded->second.freed;
            else stack.push_back({ child, 0, ownFreed(child), frame.inChunkStore || m_chunkStores.contains(child) });
        }
    }

    std::unique_lock lock(m_lock);
    m_folderTotals = std::move(totals);
}

std::optional<ULONGLONG> CDedupAccounting::GetShared(const CItem* item) const
{
    std::shared_lock lock(m_lock);
    if (const auto file = m_files.find(item); file != m_files.end())
        return file->second.onDisk > file->second.exclusive ? file->second.onDisk - file->second.exclusive : 0;
    if (const auto folder = m_folders.find(item); folder != m_folders.end()) return folder->second.shared;
    return std::nullopt;
}

bool CDedupAccounting::HasData() const
{
    std::shared_lock lock(m_lock);
    return !m_files.empty() || !m_folders.empty() || !m_pseudo.empty();
}

bool CDedupAccounting::HasSharingIndex(const CItem* item) const
{
    std::shared_lock lock(m_lock);
    // Pseudo items restored from a saved report have no index behind them
    if (item->IsTypeOrFlag(IT_DEDUP))
        return item->GetIndex() == DEDUPITEM_SHARED && m_pseudo.contains(item) && m_indexes.contains(item->GetVolumeRoot());
    if (item->IsTypeOrFlag(IT_FILE)) return m_slots.contains(item);
    return m_indexes.contains(item->GetVolumeRoot());
}

bool CDedupAccounting::IsDedupVolume(const CItem* item)
{
    const std::wstring volumePath = GetVolumePath(item->GetVolumeRoot());
    if (volumePath.empty()) return false;
    {
        std::shared_lock lock(m_lock);
        if (m_dedupVolumes.contains(volumePath)) return true;
        if (const auto probe = m_storeProbes.find(volumePath); probe != m_storeProbes.end()) return probe->second;
    }

    // Finding the chunk store needs administrator rights; optimized files still block on their own
    const bool hasStore = !CDedupStore::FindStore(volumePath).empty();
    std::unique_lock lock(m_lock);
    m_storeProbes[volumePath] = hasStore;
    return hasStore;
}

bool CDedupAccounting::BlocksCompression(const CItem* item)
{
    if (item->IsTypeOrFlag(IT_FILE))
    {
        if (item->IsTypeOrFlag(ITRP_DEDUP)) return true;
        const std::wstring path = item->GetPath();
        return FindStringOrdinal(FIND_FROMSTART, path.c_str(), static_cast<int>(path.size()),
            L"\\System Volume Information\\", -1, TRUE) >= 0 && IsDedupVolume(item);
    }
    return item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE) && IsDedupVolume(item);
}

CDedupAccounting::SharingResult CDedupAccounting::QuerySharing(const std::vector<CItem*>& items) const
{
    SharingResult result;
    std::shared_lock lock(m_lock);

    // Queried chunks: those of the optimized files below the selection, or those a shared pseudo item holds
    std::unordered_map<const VolumeIndex*, std::vector<std::uint32_t>> queriedSlots;
    std::unordered_map<const VolumeIndex*, std::vector<std::uint32_t>> ownedChunks;
    for (CItem* item : items)
    {
        if (item->IsTypeOrFlag(IT_DEDUP))
        {
            if (item->GetIndex() != DEDUPITEM_SHARED || !m_pseudo.contains(item)) continue;
            const auto index = m_indexes.find(item->GetVolumeRoot());
            if (index == m_indexes.end()) continue;
            auto& chunks = ownedChunks[&index->second];
            for (std::uint32_t chunk = 0; chunk < index->second.chunkOwner.size(); chunk++)
            {
                if (index->second.chunkOwner[chunk] == item) chunks.push_back(chunk);
            }
            continue;
        }
        for (const CItem* file : CItem::GetItemsRecursive({ item }, [](const CItem* leaf) { return leaf->IsTypeOrFlag(ITRP_DEDUP); }))
        {
            const auto slot = m_slots.find(file);
            if (slot == m_slots.end()) continue;
            if (const auto index = m_indexes.find(slot->second.first); index != m_indexes.end())
                queriedSlots[&index->second].push_back(slot->second.second);
        }
    }
    if (queriedSlots.empty() && ownedChunks.empty())
    {
        result.error = Localization::Lookup(IDS_DEDUP_SHARING_NOT_ACCOUNTED);
        return result;
    }

    std::unordered_map<CItem*, std::pair<ULONGLONG, ULONGLONG>> overlap; // item -> (shared, item bytes)
    const auto visit = [&](const VolumeIndex& index, const std::vector<std::uint32_t>& chunks,
        const std::vector<std::uint8_t>* queried)
    {
        for (const std::uint32_t chunk : chunks)
        {
            const ULONGLONG bytes = index.chunkBytes[chunk];
            result.queriedBytes += queried != nullptr ? 0 : bytes;
            if (index.chunkExternal[chunk] != 0) result.outsideBytes += bytes;
            bool listed = false;
            for (std::uint32_t i = index.chunkOffsets[chunk]; i < index.chunkOffsets[chunk + 1]; i++)
            {
                const std::uint32_t user = index.chunkFiles[i];
                if ((queried != nullptr && (*queried)[user] != 0) || index.files[user] == nullptr) continue;
                auto& row = overlap[index.files[user]];
                row.first += bytes;
                row.second = index.fileOnDisk[user];
                listed = true;
            }
            if (listed) result.sharedBytes += bytes;
        }
    };

    for (const auto& [index, slots] : queriedSlots)
    {
        // Union of the shared chunks of the queried files, plus their exclusive bytes
        std::vector<std::uint8_t> queried(index->files.size(), 0);
        std::vector<std::uint8_t> seen(index->chunkBytes.size(), 0);
        std::vector<std::uint32_t> chunks;
        for (const std::uint32_t slot : slots)
        {
            if (queried[slot] != 0) continue;
            queried[slot] = 1;
            result.queriedBytes += index->fileExclusive[slot];
            for (std::uint32_t i = index->fileOffsets[slot]; i < index->fileOffsets[slot + 1]; i++)
            {
                const std::uint32_t chunk = index->fileChunks[i];
                if (seen[chunk] != 0) continue;
                seen[chunk] = 1;
                chunks.push_back(chunk);
                result.queriedBytes += index->chunkBytes[chunk];
            }
        }
        visit(*index, chunks, &queried);
    }
    for (const auto& [index, chunks] : ownedChunks) visit(*index, chunks, nullptr);

    for (const auto& [item, values] : overlap) result.rows.push_back({ item, values.first, values.second });
    std::ranges::sort(result.rows, std::greater{}, &SharingRow::sharedBytes);
    result.success = true;
    return result;
}

void CDedupAccounting::CollectRelated(const CItem* item, std::unordered_set<const CItem*>& related, const std::size_t limit) const
{
    std::shared_lock lock(m_lock);
    if (item->IsTypeOrFlag(IT_DEDUP))
    {
        // A shared pseudo item relates to the files using the chunks it holds
        if (item->GetIndex() != DEDUPITEM_SHARED || !m_pseudo.contains(item)) return;
        const auto index = m_indexes.find(item->GetVolumeRoot());
        if (index == m_indexes.end()) return;
        const VolumeIndex& volume = index->second;
        for (std::uint32_t chunk = 0; chunk < volume.chunkOwner.size(); chunk++)
        {
            if (volume.chunkOwner[chunk] != item) continue;
            for (std::uint32_t i = volume.chunkOffsets[chunk]; i < volume.chunkOffsets[chunk + 1]; i++)
            {
                if (related.size() >= limit) return;
                if (const CItem* user = volume.files[volume.chunkFiles[i]]; user != nullptr) related.insert(user);
            }
        }
        return;
    }

    // A file relates to the other users of its shared chunks and to the pseudo items holding them
    const auto slot = m_slots.find(item);
    if (slot == m_slots.end()) return;
    const auto index = m_indexes.find(slot->second.first);
    if (index == m_indexes.end()) return;
    const VolumeIndex& volume = index->second;
    const std::uint32_t self = slot->second.second;
    for (std::uint32_t i = volume.fileOffsets[self]; i < volume.fileOffsets[self + 1]; i++)
    {
        const std::uint32_t chunk = volume.fileChunks[i];
        if (const CItem* owner = volume.chunkOwner[chunk]; owner != nullptr && m_pseudo.contains(owner)) related.insert(owner);
        for (std::uint32_t j = volume.chunkOffsets[chunk]; j < volume.chunkOffsets[chunk + 1]; j++)
        {
            // A chunk such as all zeros can be used by a large part of the volume
            if (related.size() >= limit) return;
            const std::uint32_t user = volume.chunkFiles[j];
            if (user != self && volume.files[user] != nullptr) related.insert(volume.files[user]);
        }
    }
}
