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
#include "ItemDupe.h"
#include "CsvLoader.h"

enum : std::uint8_t
{
    FIELD_NAME,
    FIELD_FILES,
    FIELD_FOLDERS,
    FIELD_SIZE_LOGICAL,
    FIELD_SIZE_PHYSICAL,
    FIELD_ATTRIBUTES,
    FIELD_LAST_CHANGE,
    FIELD_ATTRIBUTES_WDS,
    FIELD_INDEX,
    FIELD_OWNER,
    FIELD_COUNT
};

static std::array<UCHAR, FIELD_COUNT> orderMap{};
static void ParseHeaderLine(const std::vector<std::wstring>& header)
{
    orderMap.fill(static_cast<size_t>(UCHAR_MAX));
    std::unordered_map<std::wstring, DWORD> resMap =
    {
        { Localization::Lookup(IDS_COL_NAME), FIELD_NAME},
        { Localization::Lookup(IDS_COL_FILES), FIELD_FILES },
        { Localization::Lookup(IDS_COL_FOLDERS), FIELD_FOLDERS },
        { Localization::Lookup(IDS_COL_SIZE_LOGICAL), FIELD_SIZE_LOGICAL },
        { Localization::Lookup(IDS_COL_SIZE_PHYSICAL), FIELD_SIZE_PHYSICAL },
        { Localization::Lookup(IDS_COL_ATTRIBUTES), FIELD_ATTRIBUTES },
        { Localization::Lookup(IDS_COL_LAST_CHANGE), FIELD_LAST_CHANGE },
        { (Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES)), FIELD_ATTRIBUTES_WDS },
        { Localization::Lookup(IDS_COL_INDEX), FIELD_INDEX },
        { Localization::Lookup(IDS_COL_OWNER), FIELD_OWNER }
    };

    for (const auto c : std::views::iota(0u, header.size()))
    {
        if (const auto it = resMap.find(header[c]); it != resMap.end())
            orderMap[it->second] = static_cast<BYTE>(c);
    }
}

static std::string ToTimePoint(const FILETIME& fileTime)
{
    SYSTEMTIME sysTime;
    if (!FileTimeToSystemTime(&fileTime, &sysTime)) return {};
    return std::format("{}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
}

static FILETIME FromTimeString(const std::wstring& time)
{
    // Expected format: YYYY-MM-DDTHH:MM:SSZ
    SYSTEMTIME utc = {};
    if (time.size() < std::size("YYYY-MM-DDTHH:MM:SS") || time[10] != 'T' ||
        swscanf_s(time.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
        &utc.wYear, &utc.wMonth, &utc.wDay,
        &utc.wHour, &utc.wMinute, &utc.wSecond) != 6) return {};

    FILETIME ft = {};
    SystemTimeToFileTime(&utc, &ft);
    return ft;
}

static std::string QuoteAndConvert(const std::wstring& inc)
{
    thread_local std::vector<CHAR> result(512, '"');
    int charsWritten = static_cast<int>(result.size());
    while ((charsWritten = WideCharToMultiByte(CP_UTF8, 0, inc.data(), static_cast<int>(inc.size()),
        result.data() + 1, static_cast<int>(result.size() - 2), nullptr, nullptr)) == 0
        && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        result.resize(result.size() * 2);
    }

    if (charsWritten == 0) return {};
    result[1 + charsWritten] = '"';
    return { result.begin(), result.begin() + 1 + charsWritten + 1 };
}

CItem* LoadResults(const std::wstring & path)
{
    std::ifstream reader(path);
    if (!reader.is_open()) return nullptr;

    CItem* newroot = nullptr;
    std::string linebuf;
    std::wstring line;
    std::unordered_map<const std::wstring, CItem*, std::hash<std::wstring>> parentMap;

    bool headerProcessed = false;
    while (std::getline(reader, linebuf))
    {
        if (linebuf.empty()) continue;
        std::vector<std::wstring> fields;

        // Convert to wide string
        line.resize(linebuf.size() + 1);
        const int size = MultiByteToWideChar(CP_UTF8, 0, linebuf.c_str(), -1,
            line.data(), static_cast<int>(line.size()));
        if (size == 0) continue;
        line.resize(size - 1);

        // Parse all fields
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(L',', pos);
            size_t end = comma == std::wstring::npos ? line.length() : comma;

            // Adjust for quoted lines
            bool quoted = line.at(pos) == '"';
            if (quoted)
            {
                pos = pos + 1;
                end = line.find('"', pos);
                if (end == std::wstring::npos) return nullptr;
            }

            // Extra value(s)
            fields.emplace_back(line, pos, end - pos);
            pos = end + (quoted ? 1 : 0);
        }

        // Process the header if not done already
        if (!headerProcessed)
        {
            ParseHeaderLine(fields);
            headerProcessed = true;

            // Validate all necessary fields are present
            for (const auto i : std::views::iota(0u, orderMap.size()))
            {
                if (i != FIELD_OWNER && orderMap[i] == UCHAR_MAX) return nullptr;
            }
            continue;
        }

        // Decode item type
        const ITEMTYPE type = static_cast<ITEMTYPE>(wcstoull(fields[orderMap[FIELD_ATTRIBUTES_WDS]].c_str(), nullptr, 16));

        // Determine how to store the path if it was the root or not
        const auto itType = IT_MASK & type;
        const bool isRoot = (type & ITF_ROOTITEM);
        const bool isInRoot = itType == IT_DRIVE;
        const bool useFullPath = isRoot || isInRoot;
        LPWSTR lookupPath = fields[orderMap[FIELD_NAME]].data();
        LPWSTR displayName = useFullPath ? lookupPath : wcsrchr(lookupPath, L'\\');
        if (!useFullPath && displayName != nullptr)
        {
            displayName[0] = wds::chrNull;
            displayName = &displayName[1];
        }

        // Parse attributes and set directory flag if needed
        DWORD attributes = ParseAttributes(fields[orderMap[FIELD_ATTRIBUTES]]);
        if (type & IT_DIRECTORY) attributes |= FILE_ATTRIBUTE_DIRECTORY;

        // Create the tree item
        CItem* newitem = new CItem(
            type,
            displayName,
            FromTimeString(fields[orderMap[FIELD_LAST_CHANGE]]),
            wcstoull(fields[orderMap[FIELD_SIZE_PHYSICAL]].c_str(), nullptr, 10),
            wcstoull(fields[orderMap[FIELD_SIZE_LOGICAL]].c_str(), nullptr, 10),
            wcstoull(fields[orderMap[FIELD_INDEX]].c_str(), nullptr, 16),
            attributes,
            wcstoul(fields[orderMap[FIELD_FILES]].c_str(), nullptr, 10),
            wcstoul(fields[orderMap[FIELD_FOLDERS]].c_str(), nullptr, 10));

        if (isRoot)
        {
            newroot = newitem;
        }
        else if (isInRoot)
        {
            newroot->AddChild(newitem, true);
        }
        else if (auto parent = parentMap.find(lookupPath); parent != parentMap.end())
        {
            parent->second->AddChild(newitem, true);
        }
        else ASSERT(FALSE);

        if (!newitem->TmiIsLeaf() && newitem->GetItemsCount() > 0)
        {
            // Restore full path for parent assignment
            if (lookupPath != displayName) lookupPath[wcslen(lookupPath)] = wds::chrBackslash;

            const std::wstring & mapPath = fields[orderMap[FIELD_NAME]];
            parentMap[mapPath] = newitem;

            // Special case: also add mapping for drive without backslash
            if (newitem->IsTypeOrFlag(IT_DRIVE)) parentMap[mapPath.substr(0, 2)] = newitem;
        }
    }

    // Sort all parent items
    for (const auto& val : parentMap | std::views::values)
    {
        val->SortItemsBySizePhysical();
    }

    return newroot;
}

bool SaveResults(const std::wstring& path, CItem* rootItem)
{
    // Vector to store all entries
    std::vector<const CItem*> items;
    items.reserve(static_cast<size_t>(rootItem->GetItemsCount()));

    // Output all items to file
    std::stack<CItem*> queue({ rootItem });
    while (!queue.empty())
    {
        // Grab item from queue
        const CItem* qitem = queue.top();
        queue.pop();

        // Skip hardlink container items - we output files as if hardlink processing wasn't done
        if (qitem->IsTypeOrFlag(IT_HLINKS)) continue;

        items.push_back(qitem);
        if (qitem->IsLeaf()) continue;

        // Sort child items alphabetically
        std::vector<CItem*> children = qitem->GetChildren();
        std::ranges::sort(children, [](auto a, auto b)
        {
            return _wcsicmp(a->GetNameView().data(), b->GetNameView().data()) > 0;
        });

        // Descend into childitems
        for (const auto& child : children) queue.push(child);
    }

    // Output header line to file
    std::ofstream outf(path, std::ios::binary);

    // Determine columns
    std::vector cols =
    {
        Localization::Lookup(IDS_COL_NAME),
        Localization::Lookup(IDS_COL_FILES),
        Localization::Lookup(IDS_COL_FOLDERS),
        Localization::Lookup(IDS_COL_SIZE_LOGICAL),
        Localization::Lookup(IDS_COL_SIZE_PHYSICAL),
        Localization::Lookup(IDS_COL_ATTRIBUTES),
        Localization::Lookup(IDS_COL_LAST_CHANGE),
        Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES),
        Localization::Lookup(IDS_COL_INDEX)
    };
    if (COptions::ShowColumnOwner) cols.push_back(Localization::Lookup(IDS_COL_OWNER));

    // Output columns to file
    for (const auto i : std::views::iota(0u, cols.size()))
    {
        outf << QuoteAndConvert(cols[i]) << (i + 1 < cols.size() ? "," : "");
    }

    // Calculate accumulated physical sizes for parent items (to undo hardlink adjustments)
    std::unordered_map<const CItem*, LONGLONG> adjustedSizes;
    std::set<std::pair<const CItem*,ULONGLONG>> seenIndex;
    std::unordered_map<CItem*, LONGLONG> unknownSize;
    for (const auto* item : items)
    {
        // Subtract hardlinks size from drives and root node
        if (item->IsTypeOrFlag(IT_DRIVE)) if (const auto hlinks = item->FindHardlinksItem(); hlinks != nullptr)
        {
            adjustedSizes[item] -= hlinks->GetSizePhysicalRaw();
            if (const auto root = item->GetParent(); root != nullptr)
            {
                adjustedSizes[root] -= hlinks->GetSizePhysicalRaw();
            }
        }

        // Add size to all ancestors
        if (!item->IsTypeOrFlag(ITF_HARDLINK)) continue;
        for (const CItem* p = item->GetParent(); p != nullptr; p = p->GetParent())
        {
            adjustedSizes[p] += item->GetSizePhysicalRaw();

            if (!p->IsTypeOrFlag(IT_DRIVE)) continue;
            const bool alreadySeen = seenIndex.contains({ p, item->GetIndex() });
            if (!alreadySeen) seenIndex.emplace(p, item->GetIndex());

            // Unknown size should be updated to account for all but one
            if (const auto unknown = p->FindUnknownItem(); !alreadySeen && unknown != nullptr)
            { 
                unknownSize[unknown] -= item->GetSizePhysicalRaw();
            }
        }
    }

    // Output all items to file
    outf << "\r\n";
    for (const auto* item : items)
    {
        // Output primary columns
        const bool nonPathItem = item->IsTypeOrFlag(IT_MYCOMPUTER);
        const ITEMTYPE itemType = item->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK;
        const auto adjustedSize = adjustedSizes.contains(item) ? adjustedSizes[item] : 0;
        outf << std::format("{},{},{},{},{},{},{},0x{:08X},0x{:016X}",
            QuoteAndConvert(nonPathItem ? item->GetName() : item->GetPath()),
            item->GetFilesCount(),
            item->GetFoldersCount(),
            item->GetSizeLogical(),
            item->GetSizePhysicalRaw() + adjustedSize,
            QuoteAndConvert(FormatAttributes(item->GetAttributes())),
            ToTimePoint(item->GetLastChange()),
            static_cast<std::uint32_t>(itemType),
            item->GetIndex());

        // Output additional columns
        if (COptions::ShowColumnOwner) outf << "," << QuoteAndConvert(item->GetOwner(true));

        // Finalize lines
        outf << "\r\n";
    }

    return true;
}

bool SaveDuplicates(const std::wstring& path, const CItemDupe* rootDupe)
{
    // Open output file
    std::ofstream outf(path, std::ios::binary);
    if (!outf.is_open()) return false;

    // Define and output column headers
    std::vector cols =
    {
        Localization::Lookup(IDS_COL_HASH),
        Localization::Lookup(IDS_COL_NAME),
        Localization::Lookup(IDS_COL_SIZE_LOGICAL),
        Localization::Lookup(IDS_COL_SIZE_PHYSICAL),
        Localization::Lookup(IDS_COL_LAST_CHANGE),
        Localization::Lookup(IDS_COL_ATTRIBUTES)
    };

    for (const auto i : std::views::iota(0u, cols.size()))
    {
        outf << QuoteAndConvert(cols[i]) << (i + 1 < cols.size() ? "," : "");
    }
    outf << "\r\n";

    // Collect all duplicate items with their hash and linked item
    std::vector<std::tuple<std::wstring, const CItem*>> dupeItems;
    for (const auto& dupeGroup : rootDupe->GetChildren())
    {
        for (const auto& dupeFile : dupeGroup->GetChildren())
        {
            if (const auto* dupeItem = dupeFile->GetLinkedItem(); dupeItem != nullptr)
            {
                dupeItems.emplace_back(dupeGroup->GetHash(), dupeItem);
            }
        }
    }

    // Sort by logical size (descending) then by hash then by path
    std::ranges::sort(dupeItems, [](const auto& a, const auto& b)
    {
        const auto& [hashA, itemA] = a;
        const auto& [hashB, itemB] = b;

        const auto sizeA = itemA->GetSizeLogical();
        const auto sizeB = itemB->GetSizeLogical();
        if (sizeA != sizeB) return sizeA > sizeB;

        if (const int hashCmp = hashA.compare(hashB); hashCmp != 0) return hashCmp < 0;

        return _wcsicmp(itemA->GetPath().c_str(), itemB->GetPath().c_str()) < 0;
    });

    // Output all items to file
    for (const auto& [hash, linkedItem] : dupeItems)
    {
        outf << std::format("{},{},{},{},{},{}\r\n",
            QuoteAndConvert(hash),
            QuoteAndConvert(linkedItem->GetPath()),
            linkedItem->GetSizeLogical(),
            linkedItem->GetSizePhysicalRaw(),
            ToTimePoint(linkedItem->GetLastChange()),
            QuoteAndConvert(FormatAttributes(linkedItem->GetAttributes())));
    }

    return true;
}
