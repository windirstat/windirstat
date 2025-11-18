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

#include "stdafx.h"
#include "Item.h"
#include "Localization.h"
#include "CsvLoader.h"
#include "Constants.h"

#include <fstream>
#include <string>
#include <stack>
#include <unordered_map>
#include <format>
#include <array>
#include <execution>
#include <ranges>

enum : std::uint8_t
{
    FIELD_NAME,
    FIELD_FILES,
    FIELDS_FOLDERS,
    FIELD_SIZE_LOGICAL,
    FIELD_SIZE_PHYSICAL,
    FIELD_ATTRIBUTES,
    FIELD_LAST_CHANGE,
    FIELD_ATTRIBUTES_WDS,
    FIELD_OWNER,
    FIELD_COUNT
};

static std::array<CHAR, FIELD_COUNT> orderMap{};
static void ParseHeaderLine(const std::vector<std::wstring>& header)
{
    orderMap.fill(-1);
    std::unordered_map<std::wstring, DWORD> resMap =
    {
        { Localization::Lookup(IDS_COL_NAME), FIELD_NAME},
        { Localization::Lookup(IDS_COL_FILES), FIELD_FILES },
        { Localization::Lookup(IDS_COL_FOLDERS), FIELDS_FOLDERS },
        { Localization::Lookup(IDS_COL_SIZE_LOGICAL), FIELD_SIZE_LOGICAL },
        { Localization::Lookup(IDS_COL_SIZE_PHYSICAL), FIELD_SIZE_PHYSICAL },
        { Localization::Lookup(IDS_COL_ATTRIBUTES), FIELD_ATTRIBUTES },
        { Localization::Lookup(IDS_COL_LAST_CHANGE), FIELD_LAST_CHANGE },
        { (Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES)), FIELD_ATTRIBUTES_WDS },
        { Localization::Lookup(IDS_COL_OWNER), FIELD_OWNER }
    };

    for (std::vector<std::wstring>::size_type c = 0; c < header.size(); c++)
    {
        if (resMap.contains(header.at(c))) orderMap[resMap[header.at(c)]] = static_cast<BYTE>(c);
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
    const int sz = WideCharToMultiByte(CP_UTF8, 0, inc.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out = "\"";
    out.resize(static_cast<size_t>(sz) + 1);
    WideCharToMultiByte(CP_UTF8, 0, inc.data(), -1, &out[1], sz, nullptr, nullptr);
    out[sz] = '"';
    return out;
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
        line.resize(size);

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
            for (auto i = 0; i < static_cast<char>(orderMap.size()); i++)
            {
                if (i != FIELD_OWNER && orderMap[i] == -1) return nullptr;
            }
            continue;
        }

        // Decode item type
        const ITEMTYPE type = static_cast<ITEMTYPE>(wcstoul(fields[orderMap[FIELD_ATTRIBUTES_WDS]].c_str(), nullptr, 16));

        // Determine how to store the path if it was the root or not
        const bool isRoot = (type & ITF_ROOTITEM);
        const bool isInRoot = (type & IT_DRIVE) || (type & IT_UNKNOWN) || (type & IT_FREESPACE);
        const bool useFullPath = isRoot || isInRoot;
        LPWSTR lookupPath = fields[orderMap[FIELD_NAME]].data();
        LPWSTR displayName = useFullPath ? lookupPath : wcsrchr(lookupPath, L'\\');
        if (!useFullPath && displayName != nullptr)
        {
            displayName[0] = wds::chrNull;
            displayName = &displayName[1];
        }

        // Create the tree item
        CItem* newitem = new CItem(
            type,
            displayName,
            FromTimeString(fields[orderMap[FIELD_LAST_CHANGE]]),
            _wcstoui64(fields[orderMap[FIELD_SIZE_PHYSICAL]].c_str(), nullptr, 10),
            _wcstoui64(fields[orderMap[FIELD_SIZE_LOGICAL]].c_str(), nullptr, 10),
            wcstoul(fields[orderMap[FIELD_ATTRIBUTES]].c_str(), nullptr, 16),
            wcstoul(fields[orderMap[FIELD_FILES]].c_str(), nullptr, 10),
            wcstoul(fields[orderMap[FIELDS_FOLDERS]].c_str(), nullptr, 10));

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
            if (newitem->IsType(IT_DRIVE)) parentMap[mapPath.substr(0, 2)] = newitem;
        }
    }

    // Sort all parent items
    for (const auto& val : parentMap | std::views::values)
    {
        val->SortItemsBySizePhysical();
    }

    return newroot;
}

bool SaveResults(const std::wstring& path, CItem * rootItem)
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
        items.push_back(qitem);

        // Descend into childitems
        if (qitem->IsLeaf()) continue;
        for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    // Sort results
    std::sort(std::execution::par_unseq, items.begin(), items.end(),
        [](const CItem* a, const CItem* b) {
            if (a->IsRootItem() != b->IsRootItem()) return a->IsRootItem();
            return a->GetPath() < b->GetPath();
        });

    // Output header line to file
    std::ofstream outf;
    outf.open(path, std::ios::binary);

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
        Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES)
    };
    if (COptions::ShowColumnOwner)
    {
        cols.push_back(Localization::Lookup(IDS_COL_OWNER));
    }

    // Output columns to file
    for (unsigned int i = 0; i < cols.size(); i++)
    {
        outf << QuoteAndConvert(cols[i]) << ((i < cols.size() - 1) ? "," : "");
    }

    // Output all items to file
    outf << "\r\n";
    for (const auto item : items)
    {
        // Output primary columns
        const bool nonPathItem = item->IsType(IT_MYCOMPUTER | IT_UNKNOWN | IT_FREESPACE);
        outf << std::format("{},{},{},{},{},0x{:08X},{},0x{:04X}",
            QuoteAndConvert(nonPathItem ? item->GetName() : item->GetPath()),
            item->GetFilesCount(),
            item->GetFoldersCount(),
            item->GetSizeLogical(),
            item->GetSizePhysical(),
            item->GetAttributes(),
            ToTimePoint(item->GetLastChange()),
            static_cast<unsigned short>(item->GetRawType()));

        // Output additional columns
        if (COptions::ShowColumnOwner)
        {
            outf << "," << QuoteAndConvert(item->GetOwner(true));
        }

        // Finalize lines
        outf << "\r\n";
    }

    outf.close();
    return true;
}
