// CsvLoader.cpp
//
// WinDirStat - Directory Statistics
// Copyright (C) 2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//

#include "stdafx.h"
#include "langs.h"
#include "Item.h"
#include "Localization.h"
#include "CsvLoader.h"
#include "GlobalHelpers.h"

#include <fstream>
#include <string>
#include <stack>
#include <sstream>
#include <map>
#include <format>
#include <chrono>

enum
{
    FIELD_NAME,
    FIELD_FILES,
    FIELD_SUBDIRS,
    FIELD_SIZE,
    FIELD_ATTRIBUTES,
    FIELD_LASTCHANGE,
    FIELD_ATTRIBUTES_WDS,
    FIELD_OWNER,
    FIELD_COUNT
};

CHAR order_map[FIELD_COUNT];
static void ParseHeaderLine(std::vector<std::wstring> header)
{
    std::fill_n(order_map, FIELD_COUNT, (CHAR) -1);
    for (bool neutral: { true, false })
    {
        CStringW(*Lookup)(const UINT) = (neutral) ? Localization::LookupNeutral : static_cast<CStringW(*)(const UINT)>(&Localization::Lookup);
        std::map<std::wstring, DWORD> res_map =
        {
            { Lookup(IDS_TREECOL_NAME).GetString(), FIELD_NAME},
            { Lookup(IDS_TREECOL_FILES).GetString(), FIELD_FILES },
            { Lookup(IDS_TREECOL_SUBDIRS).GetString(), FIELD_SUBDIRS },
            { Lookup(IDS_TREECOL_SIZE).GetString(), FIELD_SIZE },
            { Lookup(IDS_TREECOL_ATTRIBUTES).GetString(), FIELD_ATTRIBUTES },
            { Lookup(IDS_TREECOL_LASTCHANGE).GetString(), FIELD_LASTCHANGE },
            { (Lookup(IDS_APP_TITLE) + L" " + Lookup(IDS_TREECOL_ATTRIBUTES)).GetString(), FIELD_ATTRIBUTES_WDS },
            { Lookup(IDS_TREECOL_OWNER).GetString(), FIELD_OWNER }
        };

        for (std::vector<std::wstring>::size_type c = 0; c < header.size(); c++)
        {
            if (res_map.contains(header.at(c))) order_map[res_map[header.at(c)]] = static_cast<BYTE>(c);
        }
    }
}

static std::chrono::file_clock::time_point ToTimePoint(const FILETIME& ft)
{
    std::chrono::file_clock::duration d{ (static_cast<int64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime };
    return std::chrono::file_clock::time_point { d };
}

static FILETIME FromTimeString(std::wstring s)
{
    // Parse date string
    std::wistringstream in{ s };
    std::chrono::file_clock::time_point tp;
    in >> std::chrono::parse(L"%Y-%m-%dT%H:%M:%S%Z", tp);

    // Adjust time divisor to 100ns 
    auto tmp = std::chrono::duration_cast<std::chrono::duration<int64_t,
        std::ratio_multiply<std::hecto, std::nano>>>(tp.time_since_epoch()).count();

    // Load into file time structure
    FILETIME ft;
    ft.dwLowDateTime = static_cast<ULONG>(tmp);
    ft.dwHighDateTime = tmp >> 32;
    return ft;
}

static std::string QuoteAndConvert(const CStringW& inc)
{
    const int sz = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, inc.GetString(), -1, nullptr, 0, NULL, NULL);
    std::string out = "\"";
    out.resize(sz + 1);
    WideCharToMultiByte(CP_UTF8, 0, inc.GetString(), -1, &out[1], sz, NULL, NULL);
    out[sz] = '"';
    return out;
}

CItem* LoadResults(std::wstring path)
{
    std::ifstream reader(path);
    if (!reader.is_open()) return nullptr;

    CItem* newroot = nullptr;
    std::string linebuf;
    std::wstring line;
    std::unordered_map<const std::wstring, CItem*, std::hash<std::wstring>> parent_map;

    bool header_processed = false;
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
        if (!header_processed)
        {
            ParseHeaderLine(fields);
            header_processed = true;

            // Validate all necessary fields are present
            for (int i = 0; i < _countof(order_map); i++)
            {
                if (i != FIELD_OWNER && order_map[i] == -1) return NULL;
            }
            continue;
        }

        // Decode item type
        const ITEMTYPE type = static_cast<ITEMTYPE>(wcstoul(fields[order_map[FIELD_ATTRIBUTES_WDS]].c_str(), nullptr, 16));

        // Determine how to store the path if it was the root or not
        const bool is_root = (type & ITF_ROOTITEM);
        const bool is_in_root = (type & IT_DRIVE) || (type & IT_UNKNOWN) || (type & IT_FREESPACE);
        const bool use_full_path = is_root || is_in_root;
        const std::wstring map_path = fields[order_map[FIELD_NAME]];
        LPWSTR lookup_path = fields[order_map[FIELD_NAME]].data();
        LPWSTR display_name = use_full_path ? lookup_path : wcsrchr(lookup_path, L'\\');
        if (!use_full_path && display_name != nullptr)
        {
            display_name[0] = L'\0';
            display_name = &display_name[1];
        }

        // Create the tree item
        CItem* newitem = new CItem(
            type,
            display_name,
            FromTimeString(fields[order_map[FIELD_LASTCHANGE]]),
            _wcstoui64(fields[order_map[FIELD_SIZE]].c_str(), nullptr, 10),
            wcstoul(fields[order_map[FIELD_ATTRIBUTES]].c_str(), nullptr, 16),
            wcstoul(fields[order_map[FIELD_FILES]].c_str(), nullptr, 10),
            wcstoul(fields[order_map[FIELD_SUBDIRS]].c_str(), nullptr, 10));
        if (is_root) newroot = newitem;

        auto parent = parent_map.find(lookup_path);
        if (parent != parent_map.end())
        {
            parent->second->AddChild(newitem, true);
        }
        else if (is_in_root)
        {
            newroot->AddChild(newitem, true);
        }
        else ASSERT(FALSE);

        if (!newitem->TmiIsLeaf())
        {
            parent_map[map_path] = newitem;

            // Special case: also add mapping for drive without backslash
            if (newitem->IsType(IT_DRIVE)) parent_map[map_path.substr(0,2)] = newitem;
        }
    }

    // Sort all parent items
    for (auto const& item : parent_map)
    {
        item.second->UpwardSetUndone();
        item.second->SetDone();
    }

    return newroot;
}

bool SaveResults(std::wstring path, CItem * item)
{
    // Output header line to file
    std::ofstream outf;
    outf.open(path, std::ios::binary);

    // Determine columns
    std::vector<CStringW> cols =
    {
        Localization::Lookup(IDS_TREECOL_NAME),
        Localization::Lookup(IDS_TREECOL_FILES),
        Localization::Lookup(IDS_TREECOL_SUBDIRS),
        Localization::Lookup(IDS_TREECOL_SIZE),
        Localization::Lookup(IDS_TREECOL_ATTRIBUTES),
        Localization::Lookup(IDS_TREECOL_LASTCHANGE),
        Localization::Lookup(IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_TREECOL_ATTRIBUTES)
    };
    if (COptions::ShowColumnOwner)
    {
        cols.push_back(Localization::Lookup(IDS_TREECOL_OWNER));
    }

    // Output columns to file
    for (unsigned int i = 0; i < cols.size(); i++)
    {
        outf << QuoteAndConvert(cols[i]) << ((i < cols.size() - 1) ? "," : "");
    }

    // Output all items to file
    outf << "\r\n";
    std::stack<CItem*> queue;
    queue.push(item);
    while (!queue.empty())
    {
        // Grab item from queue
        const CItem* qitem = queue.top();
        queue.pop();

        // Output primary columns
        const bool non_path_item = qitem->IsType(ITF_ROOTITEM | IT_UNKNOWN | IT_FREESPACE);
        outf << std::format("{},{},{},{},0x{:08X},{:%FT%TZ},0x{:04X}",
            QuoteAndConvert(non_path_item ? qitem->GetName() : qitem->GetPath()),
            qitem->GetFilesCount(),
            qitem->GetSubdirsCount(),
            qitem->GetSize(),
            qitem->GetAttributes(),
            ToTimePoint(qitem->GetLastChange()),
            static_cast<unsigned short>(qitem->GetRawType()));

        // Output additional columns
        if (COptions::ShowColumnOwner)
        {
            outf << "," << QuoteAndConvert(qitem->GetOwner(true));
        }

        // Finalize lines
        outf << "\r\n";

        // Descend into childitems
        if (qitem->IsType(IT_FILE)) continue;
        for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    outf.close();
    return true;
}
