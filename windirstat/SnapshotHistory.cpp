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

    using SnapshotMap = std::unordered_map<std::wstring, SnapshotEntry>;
    using ResultsFieldOrder = std::array<UCHAR, RESULT_FIELD_COUNT>;

    struct CurrentSnapshot
    {
        SnapshotMap entries;
        std::unordered_map<std::wstring, CItem*> items;
    };

    struct ResultsCsvLoadResult
    {
        ResultsCsvCompareStatus status = ResultsCsvCompareStatus::UnsupportedFormat;
        std::wstring previousSnapshotLabel;
        SnapshotMap snapshot;
    };

    std::wstring CanonicalizeKey(std::wstring key)
    {
        std::ranges::transform(key, key.begin(), [](const wchar_t ch)
        {
            return static_cast<wchar_t>(towupper(ch));
        });
        return key;
    }

    bool ShouldPersistItem(const CItem* item)
    {
        return item != nullptr && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX, IT_HLINKS_FILE);
    }

    bool ShouldDisplayGrowthItem(const CItem* item)
    {
        return item != nullptr && !item->IsTypeOrFlag(ITF_ROOTITEM) && item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY, IT_FILE);
    }

    std::wstring GetSnapshotKey(const std::wstring& rootSpec, const CItem* item)
    {
        return item->IsTypeOrFlag(ITF_ROOTITEM) ? rootSpec : item->GetPath();
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

    std::vector<std::wstring_view> ParseCsvLine(std::wstring& line)
    {
        std::vector<std::wstring_view> fields;
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(L',', pos);
            size_t end = comma == std::wstring::npos ? line.length() : comma;

            bool quoted = line.at(pos) == L'"';
            if (quoted)
            {
                pos++;
                end = line.find(L'"', pos);
                if (end == std::wstring::npos) return {};
            }

            const wchar_t restore = end < line.size() ? line[end] : wds::chrNull;
            if (end < line.size()) line[end] = wds::chrNull;
            fields.emplace_back(line.data() + pos, end - pos);
            if (end < line.size()) line[end] = restore;
            pos = end + (quoted ? 1 : 0);
        }

        return fields;
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

        std::vector<CItem*> queue{ rootItem };
        while (!queue.empty())
        {
            CItem* item = queue.back();
            queue.pop_back();

            if (!ShouldPersistItem(item)) continue;

            SnapshotEntry entry;
            entry.path = GetSnapshotKey(rootSpec, item);
            entry.type = item->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK & ~ITF_EXTDATA;
            entry.sizePhysical = item->GetSizePhysicalRaw();
            entry.files = item->GetFilesCount();
            entry.folders = item->GetFoldersCount();
            entry.lastChange = item->GetLastChange();

            const std::wstring canonicalPath = CanonicalizeKey(entry.path);
            snapshot.entries[canonicalPath] = entry;
            snapshot.items[canonicalPath] = item;

            if (item->IsLeaf()) continue;
            for (const auto& child : item->GetChildren()) queue.push_back(child);
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
        while (std::getline(input, lineBuffer))
        {
            if (lineBuffer.empty()) continue;

            line = Localization::ConvertToWideString(lineBuffer);
            if (line.empty()) continue;

            auto fields = ParseCsvLine(line);
            if (fields.size() != 6) return std::nullopt;

            SnapshotEntry entry;
            entry.path = fields[0];
            entry.type = static_cast<ITEMTYPE>(wcstoul(fields[1].data(), nullptr, 16));
            entry.sizePhysical = wcstoull(fields[2].data(), nullptr, 10);
            entry.files = wcstoul(fields[3].data(), nullptr, 10);
            entry.folders = wcstoul(fields[4].data(), nullptr, 10);
            entry.lastChange = ParseTimestamp(fields[5]);

            snapshot[CanonicalizeKey(entry.path)] = entry;
        }

        return snapshot;
    }

    bool IsPositiveGrowth(const SnapshotGrowthEntry& entry)
    {
        return entry.deltaSizePhysical > 0 || entry.deltaFiles > 0 || entry.deltaFolders > 0;
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

    ResultsCsvLoadResult LoadResultsCsvSnapshot(const std::wstring& resultsPath)
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

        std::wstring line = Localization::ConvertToWideString(lineBuffer);
        auto header = ParseCsvLine(line);
        ResultsFieldOrder order{};
        if (!TryParseResultsHeader(header, order))
        {
            result.status = ResultsCsvCompareStatus::UnsupportedFormat;
            return result;
        }

        const auto maxField = static_cast<size_t>(*std::ranges::max_element(order));

        while (std::getline(input, lineBuffer))
        {
            if (lineBuffer.empty()) continue;

            line = Localization::ConvertToWideString(lineBuffer);
            if (line.empty()) continue;

            auto fields = ParseCsvLine(line);
            if (fields.empty())
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
            entry.path = fields[order[RESULT_FIELD_NAME]];
            entry.type = static_cast<ITEMTYPE>(wcstoul(fields[order[RESULT_FIELD_ATTRIBUTES_WDS]].data(), nullptr, 16));
            entry.sizePhysical = wcstoull(fields[order[RESULT_FIELD_SIZE_PHYSICAL]].data(), nullptr, 10);
            entry.files = wcstoul(fields[order[RESULT_FIELD_FILES]].data(), nullptr, 10);
            entry.folders = wcstoul(fields[order[RESULT_FIELD_FOLDERS]].data(), nullptr, 10);

            result.snapshot[CanonicalizeKey(entry.path)] = std::move(entry);
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

        std::ranges::sort(result.entries, [](const auto& lhs, const auto& rhs)
        {
            if (lhs.deltaSizePhysical != rhs.deltaSizePhysical) return lhs.deltaSizePhysical > rhs.deltaSizePhysical;
            if (lhs.deltaFiles != rhs.deltaFiles) return lhs.deltaFiles > rhs.deltaFiles;
            if (lhs.deltaFolders != rhs.deltaFolders) return lhs.deltaFolders > rhs.deltaFolders;
            return _wcsicmp(lhs.path.c_str(), rhs.path.c_str()) < 0;
        });

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

    const ResultsCsvLoadResult loadResult = LoadResultsCsvSnapshot(previousResultsPath);
    compareResult.status = loadResult.status;
    if (loadResult.status != ResultsCsvCompareStatus::Success) return compareResult;

    const CurrentSnapshot currentSnapshot = CollectCurrentSnapshot(currentRootSpec, currentRootItem);
    compareResult.result = BuildGrowthResult(currentSnapshot, loadResult.snapshot, loadResult.previousSnapshotLabel);
    return compareResult;
}

SnapshotGrowthResult CompareSnapshotTrees(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotLabel, CItem* previousRootItem)
{
    SnapshotGrowthResult result;
    if (currentRootItem == nullptr || previousRootItem == nullptr) return result;

    const CurrentSnapshot currentSnapshot = CollectCurrentSnapshot(currentRootSpec, currentRootItem);
    const CurrentSnapshot previousSnapshot = CollectCurrentSnapshot(previousRootItem->GetPath(), previousRootItem);
    return BuildGrowthResult(currentSnapshot, previousSnapshot.entries, previousSnapshotLabel);
}

SnapshotGrowthResult CompareSnapshotFileToCurrent(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotPath)
{
    SnapshotGrowthResult result;
    if (currentRootItem == nullptr || previousSnapshotPath.empty()) return result;

    const auto previousSnapshot = LoadSnapshotFile(previousSnapshotPath);
    if (!previousSnapshot.has_value()) return result;

    const CurrentSnapshot currentSnapshot = CollectCurrentSnapshot(currentRootSpec, currentRootItem);
    return BuildGrowthResult(currentSnapshot, *previousSnapshot, BuildSnapshotLabel(previousSnapshotPath));
}
