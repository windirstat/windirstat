// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "SnapshotHistory.h"

namespace
{
    constexpr std::string_view SnapshotSignature = "# WinDirStatSnapshotV1";
    constexpr std::string_view SnapshotHeader = "path,type,size_physical,files,folders,last_change";
    constexpr size_t MaxSnapshotsPerRoot = 20;

    enum : std::uint8_t
    {
        RESULT_FIELD_NAME,
        RESULT_FIELD_FILES,
        RESULT_FIELD_FOLDERS,
        RESULT_FIELD_SIZE_PHYSICAL,
        RESULT_FIELD_ATTRIBUTES_WDS,
        RESULT_FIELD_COUNT,
    };

    struct SnapshotEntry
    {
        std::wstring path;
        ITEMTYPE type = IT_NONE;
        ULONGLONG sizePhysical = 0;
        ULONG files = 0;
        ULONG folders = 0;
        FILETIME lastChange{};
    };

    struct SnapshotPathHash
    {
        size_t operator()(const std::wstring& value) const noexcept
        {
            constexpr std::uint64_t basis = 14695981039346656037ull;
            constexpr std::uint64_t prime = 1099511628211ull;

            std::uint64_t hash = basis;
            for (const wchar_t ch : value)
            {
                hash ^= static_cast<std::uint64_t>(towupper(ch));
                hash *= prime;
            }

            return static_cast<size_t>(hash);
        }
    };

    struct SnapshotPathEqual
    {
        bool operator()(const std::wstring& lhs, const std::wstring& rhs) const noexcept
        {
            return _wcsicmp(lhs.c_str(), rhs.c_str()) == 0;
        }
    };

    using SnapshotMap = std::unordered_map<std::wstring, SnapshotEntry, SnapshotPathHash, SnapshotPathEqual>;
    using ResultsFieldOrder = std::array<UCHAR, RESULT_FIELD_COUNT>;
    using SnapshotItemMap = std::unordered_map<std::wstring, CItem*, SnapshotPathHash, SnapshotPathEqual>;

    struct CurrentSnapshot
    {
        SnapshotMap entries;
        SnapshotItemMap items;
    };

    struct PendingSnapshotItem
    {
        CItem* item = nullptr;
        std::wstring path;
    };

    struct ResultsCsvLoadResult
    {
        ResultsCsvCompareStatus status = ResultsCsvCompareStatus::UnsupportedFormat;
        std::wstring previousSnapshotLabel;
        SnapshotMap snapshot;
    };

    std::wstring BuildChildSnapshotPath(const std::wstring& parentPath, const CItem* child)
    {
        if (child == nullptr) return {};

        if (child->IsTypeOrFlag(IT_DRIVE))
        {
            std::wstring path(child->GetNameView());
            if (!path.empty() && path.back() != wds::chrBackslash) path.push_back(wds::chrBackslash);
            return path;
        }

        std::wstring path = parentPath;
        if (!path.empty() && path.back() != wds::chrBackslash) path.push_back(wds::chrBackslash);
        path.append(child->GetNameView());
        return path;
    }

    bool ShouldPersistItem(const CItem* item)
    {
        return item != nullptr && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX, IT_HLINKS_FILE);
    }

    bool ShouldDisplayGrowthItem(const CItem* item)
    {
        return item != nullptr && !item->IsTypeOrFlag(ITF_ROOTITEM) && item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY, IT_FILE);
    }

    bool ConvertUtf8ToWide(const std::string_view input, std::wstring& output)
    {
        output.clear();
        if (input.empty()) return true;

        const int required = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
        if (required <= 0) return false;

        output.resize(required);
        if (MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), required) <= 0)
        {
            output.clear();
            return false;
        }

        return true;
    }

    std::string ToUtf8(const std::wstring_view input)
    {
        if (input.empty()) return {};

        const int required = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0) return {};

        std::string output(required, '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), required, nullptr, nullptr) <= 0)
        {
            return {};
        }

        return output;
    }

    std::wstring QuoteCsv(const std::wstring_view input)
    {
        return L'"' + std::wstring(input) + L'"';
    }

    std::wstring FormatTimestamp(const FILETIME& fileTime)
    {
        SYSTEMTIME sysTime{};
        if (!FileTimeToSystemTime(&fileTime, &sysTime)) return {};

        return std::format(L"{}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
            sysTime.wYear, sysTime.wMonth, sysTime.wDay,
            sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
    }

    FILETIME ParseTimestamp(const std::wstring_view& input)
    {
        SYSTEMTIME sysTime{};
        if (input.size() < std::size("YYYY-MM-DDTHH:MM:SS") ||
            swscanf_s(input.data(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
                &sysTime.wYear, &sysTime.wMonth, &sysTime.wDay,
                &sysTime.wHour, &sysTime.wMinute, &sysTime.wSecond) != 6)
        {
            return {};
        }

        FILETIME fileTime{};
        (void)SystemTimeToFileTime(&sysTime, &fileTime);
        return fileTime;
    }

    bool ParseCsvLine(std::wstring& line, std::vector<std::wstring_view>& fields)
    {
        fields.clear();
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(L',', pos);
            size_t end = comma == std::wstring::npos ? line.length() : comma;

            bool quoted = line.at(pos) == L'"';
            if (quoted)
            {
                pos++;
                end = line.find(L'"', pos);
                if (end == std::wstring::npos)
                {
                    fields.clear();
                    return false;
                }
            }

            const wchar_t restore = end < line.size() ? line[end] : wds::chrNull;
            if (end < line.size()) line[end] = wds::chrNull;
            fields.emplace_back(line.data() + pos, end - pos);
            if (end < line.size()) line[end] = restore;
            pos = end + (quoted ? 1 : 0);
        }

        return true;
    }

    bool ParseCsvLine(std::string& line, std::vector<std::string_view>& fields)
    {
        fields.clear();
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(',', pos);
            size_t end = comma == std::string::npos ? line.length() : comma;

            bool quoted = line.at(pos) == '"';
            if (quoted)
            {
                pos++;
                end = line.find('"', pos);
                if (end == std::string::npos)
                {
                    fields.clear();
                    return false;
                }
            }

            const char restore = end < line.size() ? line[end] : '\0';
            if (end < line.size()) line[end] = '\0';
            fields.emplace_back(line.data() + pos, end - pos);
            if (end < line.size()) line[end] = restore;
            pos = end + (quoted ? 1 : 0);
        }

        return true;
    }

    std::wstring HashRootSpec(const std::wstring& rootSpec)
    {
        constexpr std::uint64_t basis = 14695981039346656037ull;
        constexpr std::uint64_t prime = 1099511628211ull;

        std::uint64_t hash = basis;
        for (const wchar_t ch : rootSpec)
        {
            hash ^= static_cast<std::uint64_t>(towupper(ch));
            hash *= prime;
        }

        return std::format(L"{:016X}", hash);
    }

    std::wstring GetLegacyHistoryRoot()
    {
        SmartPointer<PWSTR> localAppData(CoTaskMemFree);
        if (SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData) == S_OK && localAppData != nullptr)
        {
            return std::wstring(localAppData) + L"\\WinDirStat\\History";
        }

        return GetAppFolder() + L"\\history";
    }

    std::wstring GetHistoryRoot()
    {
        return GetAppFolder() + L"\\history";
    }

    void MigrateLegacyHistoryIfNeeded()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            std::error_code ec;
            const std::filesystem::path historyRoot = GetHistoryRoot();
            const std::filesystem::path legacyRoot = GetLegacyHistoryRoot();

            if (historyRoot == legacyRoot || std::filesystem::exists(historyRoot, ec) || !std::filesystem::exists(legacyRoot, ec)) return;

            std::filesystem::create_directories(historyRoot, ec);
            if (ec) return;

            std::filesystem::copy(legacyRoot, historyRoot,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing, ec);
        });
    }

    std::filesystem::path GetBucketPath(const std::wstring& rootSpec)
    {
        MigrateLegacyHistoryIfNeeded();
        return std::filesystem::path(GetHistoryRoot()) / HashRootSpec(rootSpec);
    }

    std::wstring BuildSnapshotLabel(const std::filesystem::path& snapshotPath)
    {
        const std::wstring stem = snapshotPath.stem().wstring();
        if (stem.size() < 16) return snapshotPath.filename().wstring();

        return std::format(L"{}-{}-{} {}:{}:{} UTC",
            stem.substr(0, 4), stem.substr(4, 2), stem.substr(6, 2),
            stem.substr(9, 2), stem.substr(11, 2), stem.substr(13, 2));
    }

    std::optional<std::filesystem::path> FindLatestSnapshot(const std::wstring& rootSpec)
    {
        std::error_code ec;
        const auto bucketPath = GetBucketPath(rootSpec);
        if (!std::filesystem::exists(bucketPath, ec)) return std::nullopt;

        std::optional<std::filesystem::path> latest;
        for (const auto& entry : std::filesystem::directory_iterator(bucketPath, ec))
        {
            if (ec || !entry.is_regular_file() || entry.path().extension() != L".csv") continue;
            if (!latest || entry.path().filename().wstring() > latest->filename().wstring()) latest = entry.path();
        }

        return latest;
    }

    void PruneSnapshots(const std::wstring& rootSpec)
    {
        std::error_code ec;
        const auto bucketPath = GetBucketPath(rootSpec);
        if (!std::filesystem::exists(bucketPath, ec)) return;

        std::vector<std::filesystem::path> snapshots;
        for (const auto& entry : std::filesystem::directory_iterator(bucketPath, ec))
        {
            if (ec || !entry.is_regular_file() || entry.path().extension() != L".csv") continue;
            snapshots.push_back(entry.path());
        }

        if (snapshots.size() <= MaxSnapshotsPerRoot) return;

        std::ranges::sort(snapshots, {}, [](const auto& path) { return path.filename().wstring(); });
        for (const auto& path : snapshots | std::views::take(snapshots.size() - MaxSnapshotsPerRoot))
        {
            std::filesystem::remove(path, ec);
        }
    }

    CurrentSnapshot CollectCurrentSnapshot(const std::wstring& rootSpec, CItem* rootItem)
    {
        CurrentSnapshot snapshot;
        if (rootItem == nullptr) return snapshot;

        const auto estimatedItems = static_cast<size_t>(rootItem->GetItemsCount()) + 1;
        snapshot.entries.reserve(estimatedItems);
        snapshot.items.reserve(estimatedItems);

        std::vector<PendingSnapshotItem> queue;
        queue.reserve(estimatedItems);
        queue.push_back({ rootItem, rootSpec });

        while (!queue.empty())
        {
            PendingSnapshotItem pending = std::move(queue.back());
            queue.pop_back();

            CItem* item = pending.item;

            if (!ShouldPersistItem(item)) continue;

            SnapshotEntry entry;
            entry.path = std::move(pending.path);
            entry.type = item->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK & ~ITF_EXTDATA;
            entry.sizePhysical = item->GetSizePhysicalRaw();
            entry.files = item->GetFilesCount();
            entry.folders = item->GetFoldersCount();
            entry.lastChange = item->GetLastChange();

            snapshot.items[entry.path] = item;
            snapshot.entries[entry.path] = entry;

            if (item->IsLeaf()) continue;

            const auto& children = item->GetChildren();
            queue.reserve(queue.size() + children.size());
            for (const auto& child : children)
            {
                queue.push_back({ child, BuildChildSnapshotPath(entry.path, child) });
            }
        }

        return snapshot;
    }

    bool SaveSnapshotFile(const std::wstring& rootSpec, const CurrentSnapshot& snapshot)
    {
        std::error_code ec;
        const auto bucketPath = GetBucketPath(rootSpec);
        std::filesystem::create_directories(bucketPath, ec);
        if (ec) return false;

        SYSTEMTIME now{};
        GetSystemTime(&now);
        const auto snapshotPath = bucketPath / std::format(L"{:04}{:02}{:02}T{:02}{:02}{:02}Z.csv",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

        std::ofstream output(snapshotPath, std::ios::binary);
        if (!output.is_open()) return false;

        output << SnapshotSignature << "\r\n" << SnapshotHeader;

        std::vector<const SnapshotEntry*> entries;
        entries.reserve(snapshot.entries.size());
        for (const auto& entry : snapshot.entries | std::views::values) entries.push_back(&entry);
        std::ranges::sort(entries, {}, [](const SnapshotEntry* entry) { return entry->path; });

        for (const auto* entry : entries)
        {
            output << "\r\n" << ToUtf8(QuoteCsv(entry->path)) << ','
                << std::format("0x{:08X}", static_cast<std::uint32_t>(entry->type)) << ','
                << entry->sizePhysical << ','
                << entry->files << ','
                << entry->folders << ','
                << ToUtf8(QuoteCsv(FormatTimestamp(entry->lastChange)));
        }

        if (!output.good()) return false;
        PruneSnapshots(rootSpec);
        return true;
    }

    std::optional<SnapshotMap> LoadSnapshotFile(const std::filesystem::path& snapshotPath)
    {
        std::ifstream input(snapshotPath, std::ios::binary);
        if (!input.is_open()) return std::nullopt;

        std::string lineBuffer;
        if (!std::getline(input, lineBuffer) || lineBuffer != SnapshotSignature) return std::nullopt;
        if (!std::getline(input, lineBuffer) || lineBuffer != SnapshotHeader) return std::nullopt;

        SnapshotMap snapshot;
        std::wstring line;
        std::vector<std::wstring_view> fields;
        fields.reserve(6);
        while (std::getline(input, lineBuffer))
        {
            if (lineBuffer.empty()) continue;

            if (!ConvertUtf8ToWide(lineBuffer, line)) return std::nullopt;
            if (line.empty()) continue;

            if (!ParseCsvLine(line, fields) || fields.size() != 6) return std::nullopt;

            SnapshotEntry entry;
            entry.path = fields[0];
            entry.type = static_cast<ITEMTYPE>(wcstoul(fields[1].data(), nullptr, 16));
            entry.sizePhysical = wcstoull(fields[2].data(), nullptr, 10);
            entry.files = wcstoul(fields[3].data(), nullptr, 10);
            entry.folders = wcstoul(fields[4].data(), nullptr, 10);
            entry.lastChange = ParseTimestamp(fields[5]);

            snapshot[entry.path] = entry;
        }

        return snapshot;
    }

    bool IsPositiveGrowth(const SnapshotGrowthEntry& entry)
    {
        return entry.deltaSizePhysical > 0 || entry.deltaFiles > 0 || entry.deltaFolders > 0;
    }

    void SortGrowthEntries(std::vector<SnapshotGrowthEntry>& entries)
    {
        std::ranges::sort(entries, [](const auto& lhs, const auto& rhs)
        {
            if (lhs.deltaSizePhysical != rhs.deltaSizePhysical) return lhs.deltaSizePhysical > rhs.deltaSizePhysical;
            if (lhs.deltaFiles != rhs.deltaFiles) return lhs.deltaFiles > rhs.deltaFiles;
            if (lhs.deltaFolders != rhs.deltaFolders) return lhs.deltaFolders > rhs.deltaFolders;
            return _wcsicmp(lhs.path.c_str(), rhs.path.c_str()) < 0;
        });
    }

    void AppendGrowthEntryForCurrentItem(std::vector<SnapshotGrowthEntry>& entries, const SnapshotMap& previousSnapshot,
        CItem* currentItem, const std::wstring& currentPath)
    {
        if (!ShouldDisplayGrowthItem(currentItem)) return;

        const auto previousIt = previousSnapshot.find(currentPath);
        const SnapshotEntry* previousEntry = previousIt != previousSnapshot.end() ? &previousIt->second : nullptr;

        SnapshotGrowthEntry growth;
        growth.path = currentPath;
        growth.type = currentItem->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK & ~ITF_EXTDATA;
        growth.currentItem = currentItem;
        growth.currentSizePhysical = currentItem->GetSizePhysicalRaw();
        growth.previousSizePhysical = previousEntry != nullptr ? previousEntry->sizePhysical : 0;
        growth.deltaSizePhysical = static_cast<LONGLONG>(growth.currentSizePhysical) - static_cast<LONGLONG>(growth.previousSizePhysical);
        growth.deltaFiles = static_cast<LONGLONG>(currentItem->GetFilesCount()) - static_cast<LONGLONG>(previousEntry != nullptr ? previousEntry->files : 0);
        growth.deltaFolders = static_cast<LONGLONG>(currentItem->GetFoldersCount()) - static_cast<LONGLONG>(previousEntry != nullptr ? previousEntry->folders : 0);

        if (IsPositiveGrowth(growth)) entries.push_back(std::move(growth));
    }

    SnapshotGrowthResult CompareSnapshotToCurrentTree(const std::wstring& currentRootSpec, CItem* currentRootItem,
        const SnapshotMap& previousSnapshot, std::wstring previousSnapshotLabel)
    {
        SnapshotGrowthResult result;
        if (currentRootItem == nullptr) return result;

        result.previousSnapshotLabel = std::move(previousSnapshotLabel);

        const auto estimatedItems = static_cast<size_t>(currentRootItem->GetItemsCount()) + 1;
        std::vector<PendingSnapshotItem> queue;
        queue.reserve(estimatedItems);
        queue.push_back({ currentRootItem, currentRootSpec });

        while (!queue.empty())
        {
            PendingSnapshotItem pending = std::move(queue.back());
            queue.pop_back();

            CItem* item = pending.item;
            if (!ShouldPersistItem(item)) continue;

            AppendGrowthEntryForCurrentItem(result.entries, previousSnapshot, item, pending.path);

            if (item->IsLeaf()) continue;

            const auto& children = item->GetChildren();
            queue.reserve(queue.size() + children.size());
            for (const auto& child : children)
            {
                queue.push_back({ child, BuildChildSnapshotPath(pending.path, child) });
            }
        }

        SortGrowthEntries(result.entries);
        return result;
    }

    bool TryParseResultsHeader(const std::vector<std::wstring_view>& header, ResultsFieldOrder& order)
    {
        order.fill(static_cast<UCHAR>(UCHAR_MAX));

        const auto assignField = [&](const std::wstring_view columnName, const std::uint8_t field)
        {
            for (const auto i : std::views::iota(0u, header.size()))
            {
                if (header[i] == columnName)
                {
                    order[field] = static_cast<UCHAR>(i);
                    return;
                }
            }
        };

        assignField(Localization::Lookup(IDS_COL_NAME), RESULT_FIELD_NAME);
        assignField(Localization::Lookup(IDS_COL_FILES), RESULT_FIELD_FILES);
        assignField(Localization::Lookup(IDS_COL_FOLDERS), RESULT_FIELD_FOLDERS);
        assignField(Localization::Lookup(IDS_COL_SIZE_PHYSICAL), RESULT_FIELD_SIZE_PHYSICAL);
        assignField(Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES), RESULT_FIELD_ATTRIBUTES_WDS);

        return std::ranges::all_of(order, [](const UCHAR index) { return index != UCHAR_MAX; });
    }

    ResultsCsvLoadResult LoadResultsCsvSnapshot(const std::wstring& resultsPath, const size_t reserveHint)
    {
        ResultsCsvLoadResult result;

        std::ifstream input(resultsPath, std::ios::binary);
        if (!input.is_open())
        {
            result.status = ResultsCsvCompareStatus::InvalidResultsFile;
            return result;
        }

        std::string lineBuffer;
        if (!std::getline(input, lineBuffer))
        {
            result.status = ResultsCsvCompareStatus::InvalidResultsFile;
            return result;
        }

        if (lineBuffer == SnapshotSignature)
        {
            result.status = ResultsCsvCompareStatus::UnsupportedFormat;
            return result;
        }

        std::wstring line;
        std::vector<std::wstring_view> header;
        header.reserve(RESULT_FIELD_COUNT + 2);
        if (!ConvertUtf8ToWide(lineBuffer, line) || !ParseCsvLine(line, header))
        {
            result.status = ResultsCsvCompareStatus::InvalidResultsFile;
            return result;
        }

        ResultsFieldOrder order{};
        if (!TryParseResultsHeader(header, order))
        {
            result.status = ResultsCsvCompareStatus::UnsupportedFormat;
            return result;
        }

        const auto maxField = static_cast<size_t>(*std::ranges::max_element(order));
        result.snapshot.reserve(reserveHint);
        std::vector<std::string_view> fields;
        fields.reserve(maxField + 1);
        std::wstring pathBuffer;

        while (std::getline(input, lineBuffer))
        {
            if (lineBuffer.empty()) continue;

            if (!ParseCsvLine(lineBuffer, fields))
            {
                result.status = ResultsCsvCompareStatus::InvalidResultsFile;
                return result;
            }

            if (fields.size() <= maxField)
            {
                result.status = ResultsCsvCompareStatus::InvalidResultsFile;
                return result;
            }

            SnapshotEntry entry;
            if (!ConvertUtf8ToWide(fields[order[RESULT_FIELD_NAME]], pathBuffer))
            {
                result.status = ResultsCsvCompareStatus::InvalidResultsFile;
                return result;
            }

            entry.path = pathBuffer;
            entry.type = static_cast<ITEMTYPE>(strtoul(fields[order[RESULT_FIELD_ATTRIBUTES_WDS]].data(), nullptr, 16));
            entry.sizePhysical = strtoull(fields[order[RESULT_FIELD_SIZE_PHYSICAL]].data(), nullptr, 10);
            entry.files = strtoul(fields[order[RESULT_FIELD_FILES]].data(), nullptr, 10);
            entry.folders = strtoul(fields[order[RESULT_FIELD_FOLDERS]].data(), nullptr, 10);

            result.snapshot[entry.path] = std::move(entry);
        }

        result.status = ResultsCsvCompareStatus::Success;
        result.previousSnapshotLabel = std::filesystem::path(resultsPath).filename().wstring();
        return result;
    }

    SnapshotGrowthResult BuildGrowthResult(const CurrentSnapshot& currentSnapshot, const SnapshotMap& previousSnapshot,
        std::wstring previousSnapshotLabel)
    {
        SnapshotGrowthResult result;
        result.previousSnapshotLabel = std::move(previousSnapshotLabel);
        result.entries.reserve(currentSnapshot.entries.size());

        for (const auto& [canonicalKey, currentEntry] : currentSnapshot.entries)
        {
            const auto currentItemIt = currentSnapshot.items.find(canonicalKey);
            CItem* currentItem = currentItemIt != currentSnapshot.items.end() ? currentItemIt->second : nullptr;
            if (!ShouldDisplayGrowthItem(currentItem)) continue;

            const auto previousIt = previousSnapshot.find(canonicalKey);
            const SnapshotEntry* previousEntry = previousIt != previousSnapshot.end() ? &previousIt->second : nullptr;

            SnapshotGrowthEntry growth;
            growth.path = currentEntry.path;
            growth.type = currentEntry.type;
            growth.currentItem = currentItem;
            growth.currentSizePhysical = currentEntry.sizePhysical;
            growth.previousSizePhysical = previousEntry != nullptr ? previousEntry->sizePhysical : 0;
            growth.deltaSizePhysical = static_cast<LONGLONG>(growth.currentSizePhysical) - static_cast<LONGLONG>(growth.previousSizePhysical);
            growth.deltaFiles = static_cast<LONGLONG>(currentEntry.files) - static_cast<LONGLONG>(previousEntry != nullptr ? previousEntry->files : 0);
            growth.deltaFolders = static_cast<LONGLONG>(currentEntry.folders) - static_cast<LONGLONG>(previousEntry != nullptr ? previousEntry->folders : 0);

            if (IsPositiveGrowth(growth)) result.entries.push_back(growth);
        }

        SortGrowthEntries(result.entries);

        return result;
    }
}

SnapshotGrowthResult UpdateSnapshotHistory(const std::wstring& rootSpec, CItem* rootItem)
{
    SnapshotGrowthResult result;
    if (rootSpec.empty() || rootItem == nullptr) return result;

    const CurrentSnapshot currentSnapshot = CollectCurrentSnapshot(rootSpec, rootItem);
    if (currentSnapshot.entries.empty()) return result;

    if (const auto latestSnapshot = FindLatestSnapshot(rootSpec))
    {
        if (const auto previousSnapshot = LoadSnapshotFile(*latestSnapshot))
        {
            result = BuildGrowthResult(currentSnapshot, *previousSnapshot, BuildSnapshotLabel(*latestSnapshot));
        }
    }

    (void)SaveSnapshotFile(rootSpec, currentSnapshot);
    return result;
}

ResultsCsvCompareResult CompareResultsCsvToCurrent(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousResultsPath)
{
    ResultsCsvCompareResult compareResult;
    if (currentRootItem == nullptr || previousResultsPath.empty()) return compareResult;

    const ResultsCsvLoadResult loadResult = LoadResultsCsvSnapshot(previousResultsPath,
        static_cast<size_t>(currentRootItem->GetItemsCount()) + 1);
    compareResult.status = loadResult.status;
    if (loadResult.status != ResultsCsvCompareStatus::Success) return compareResult;

    compareResult.result = CompareSnapshotToCurrentTree(currentRootSpec, currentRootItem,
        loadResult.snapshot, loadResult.previousSnapshotLabel);
    return compareResult;
}

SnapshotGrowthResult CompareSnapshotTrees(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotLabel, CItem* previousRootItem)
{
    SnapshotGrowthResult result;
    if (currentRootItem == nullptr || previousRootItem == nullptr) return result;

    const CurrentSnapshot previousSnapshot = CollectCurrentSnapshot(previousRootItem->GetPath(), previousRootItem);
    return CompareSnapshotToCurrentTree(currentRootSpec, currentRootItem, previousSnapshot.entries, previousSnapshotLabel);
}

SnapshotGrowthResult CompareSnapshotFileToCurrent(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotPath)
{
    SnapshotGrowthResult result;
    if (currentRootItem == nullptr || previousSnapshotPath.empty()) return result;

    const auto previousSnapshot = LoadSnapshotFile(previousSnapshotPath);
    if (!previousSnapshot.has_value()) return result;

    return CompareSnapshotToCurrentTree(currentRootSpec, currentRootItem, *previousSnapshot, BuildSnapshotLabel(previousSnapshotPath));
}
