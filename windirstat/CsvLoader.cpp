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

static bool IsJsonPath(const std::wstring& path)
{
    return path.size() >= 5 && _wcsicmp(path.c_str() + path.size() - 5, L".json") == 0;
}

// Wide string → UTF-8; Localization has no reverse equivalent so we keep this here
static std::string WideToUtf8(std::wstring_view wv)
{
    if (wv.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wv.data(), static_cast<int>(wv.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wv.data(), static_cast<int>(wv.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

// JSON-escape a UTF-8 string and wrap it in double quotes
static std::string JsonQuote(std::string_view utf8)
{
    std::string out;
    out.reserve(utf8.size() + 2);
    out.push_back('"');
    for (const unsigned char c : utf8)
    {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out.push_back(static_cast<char>(c));
    }
    out.push_back('"');
    return out;
}

static std::string JsonQuoteW(std::wstring_view wv)
{
    return JsonQuote(WideToUtf8(wv));
}

// Unescape a JSON string value
static std::string JsonUnescape(std::string_view sv)
{
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i)
    {
        if (sv[i] == '"') break;   // closing quote
        if (sv[i] == '\\' && i + 1 < sv.size())
        {
            ++i;
            out.push_back(sv[i]);
        }
        else out.push_back(sv[i]);
    }
    return out;
}

// Parse one JSON object {…} from the stream into a key→wstring map; returns false at ']'/EOF
static bool JsonReadObject(std::istream& in, std::unordered_map<std::wstring, std::wstring>& obj)
{
    obj.clear();
    std::string linebuf;
    while (std::getline(in, linebuf))
    {
        // Trim leading whitespace
        const char* p = linebuf.c_str();
        while (*p == ' ' || *p == '\t') ++p;

        if (*p == '}') return true;   // end of this object
        if (*p == ']') return false;  // end of array
        if (*p != '"') continue;      // skip blank lines, '[', etc.

        // Parse key then locate ':'
        const char* keyStart = p + 1;
        const char* colon = strchr(keyStart, ':');
        if (!colon) continue;
        const std::wstring key = Localization::ConvertToWideString(JsonUnescape(
            { keyStart, static_cast<size_t>(colon - keyStart) }));

        // Parse value (string or bare numeric)
        const char* valp = colon + 1;
        while (*valp == ' ' || *valp == '\t') ++valp;

        std::wstring value;
        if (*valp == '"')
        {
            // Quoted string — unescape inner content
            ++valp;
            value = Localization::ConvertToWideString(JsonUnescape(
                { valp, linebuf.size() - static_cast<size_t>(valp - linebuf.c_str()) }));
        }
        else
        {
            // Bare numeric — strip trailing comma if present
            std::string_view raw(valp);
            if (!raw.empty() && raw.back() == ',') raw.remove_suffix(1);
            value = Localization::ConvertToWideString(raw);
        }
        obj.emplace(std::move(key), std::move(value));
    }
    return false;
}

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
static void ParseHeaderLine(const std::vector<std::wstring_view>& header)
{
    orderMap.fill(UCHAR_MAX);
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
        if (const auto it = resMap.find(header[c].data()); it != resMap.end())
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

static FILETIME FromTimeString(std::wstring_view time)
{
    // Expected format: YYYY-MM-DDTHH:MM:SSZ
    SYSTEMTIME utc = {};
    if (time.size() < std::size("YYYY-MM-DDTHH:MM:SS") || time[10] != 'T' ||
        swscanf_s(time.data(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu",
        &utc.wYear, &utc.wMonth, &utc.wDay,
        &utc.wHour, &utc.wMinute, &utc.wSecond) != 6) return {};

    FILETIME ft = {};
    SystemTimeToFileTime(&utc, &ft);
    return ft;
}

// CSV quoting: wrap UTF-8 bytes in double quotes (CSV values contain no embedded quotes)
static std::string QuoteAndConvert(std::wstring_view inc)
{
    return '"' + WideToUtf8(inc) + '"';
}

// ── shared item-construction helper ─────────────────────────────────────────

// Build a CItem from decoded field values, attach to tree, and register in parentMap
static CItem* BuildAndAttachItem(std::wstring& namePath, std::wstring_view wdsAttr,
    std::wstring_view lastChange, std::wstring_view sizePhysical, std::wstring_view sizeLogical,
    std::wstring_view index, std::wstring_view attributes, std::wstring_view files,
    std::wstring_view folders, CItem*& newroot, std::unordered_map<std::wstring, CItem*>& parentMap)
{
    const auto type = static_cast<ITEMTYPE>(wcstoull(wdsAttr.data(), nullptr, 16));

    const auto itType = IT_MASK & type;
    const bool isRoot = (type & ITF_ROOTITEM) != 0;
    const bool isInRoot  = itType == IT_DRIVE;
    const bool useFullPath = isRoot || isInRoot;

    LPWSTR lookupPath = namePath.data();
    LPWSTR displayName = useFullPath ? lookupPath : wcsrchr(lookupPath, L'\\');
    if (!useFullPath && displayName != nullptr)
    {
        displayName[0] = wds::chrNull;
        displayName = &displayName[1];
    }

    DWORD attrs = ParseAttributes(attributes);
    if (type & IT_DIRECTORY) attrs |= FILE_ATTRIBUTE_DIRECTORY;

    CItem* newitem = new CItem(type, displayName, FromTimeString(lastChange),
        wcstoull(sizePhysical.data(), nullptr, 10), wcstoull(sizeLogical.data(),  nullptr, 10),
        wcstoull(index.data(), nullptr, 16), attrs, wcstoul(files.data(),   nullptr, 10),
        wcstoul(folders.data(), nullptr, 10));

    if (isRoot)
    {
        newroot = newitem;
    }
    else if (isInRoot)
    {
        if (!newroot) { delete newitem; return nullptr; }
        newroot->AddChild(newitem, true);
    }
    else if (auto parent = parentMap.find(lookupPath); parent != parentMap.end())
    {
        parent->second->AddChild(newitem, true);
    }
    else { ASSERT(FALSE); }

    if (!newitem->TmiIsLeaf() && newitem->GetItemsCount() > 0)
    {
        // Restore the backslash separator we may have zeroed out
        if (lookupPath != displayName) lookupPath[wcslen(lookupPath)] = wds::chrBackslash;

        parentMap[namePath] = newitem;

        // Also map drive letter without trailing backslash
        if (newitem->IsTypeOrFlag(IT_DRIVE)) parentMap[namePath.substr(0, 2)] = newitem;
    }

    return newitem;
}

static CItem* LoadResultsCsv(std::ifstream& reader)
{
    CItem* newroot = nullptr;
    std::string linebuf;
    std::wstring line;
    std::unordered_map<std::wstring, CItem*> parentMap;

    bool headerProcessed = false;
    for (std::vector<std::wstring_view> fields; std::getline(reader, linebuf); fields.clear())
    {
        if (linebuf.empty()) continue;

        // Convert to wide string
        line.resize(linebuf.size() + 1);
        const int size = MultiByteToWideChar(CP_UTF8, 0, linebuf.c_str(), -1,
            line.data(), static_cast<int>(line.size()));
        if (size == 0) continue;
        line.resize(size - static_cast<size_t>(1));

        // Parse all fields
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(L',', pos);
            size_t end = comma == std::wstring::npos ? line.length() : comma;

            // Adjust for quoted fields
            bool quoted = line.at(pos) == L'"';
            if (quoted)
            {
                pos = pos + 1;
                end = line.find(L'"', pos);
                if (end == std::wstring::npos) return nullptr;
            }

            line[end] = L'\0';
            fields.emplace_back(line.data() + pos, end - pos);
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

        std::wstring namePath(fields[orderMap[FIELD_NAME]]);
        BuildAndAttachItem(namePath, fields[orderMap[FIELD_ATTRIBUTES_WDS]],
            fields[orderMap[FIELD_LAST_CHANGE]], fields[orderMap[FIELD_SIZE_PHYSICAL]],
            fields[orderMap[FIELD_SIZE_LOGICAL]], fields[orderMap[FIELD_INDEX]],
            fields[orderMap[FIELD_ATTRIBUTES]], fields[orderMap[FIELD_FILES]],
            fields[orderMap[FIELD_FOLDERS]], newroot, parentMap);
    }

    for (const auto& val : parentMap | std::views::values)
        val->SortItemsBySizePhysical();

    return newroot;
}

// ── JSON load ─────────────────────────────────────────────────────────────────

static CItem* LoadResultsJson(std::ifstream& reader)
{
    // Build the same column-name → field-index mapping reused from CSV
    const std::unordered_map<std::wstring, DWORD> keyToField =
    {
        { Localization::Lookup(IDS_COL_NAME),          FIELD_NAME          },
        { Localization::Lookup(IDS_COL_FILES),         FIELD_FILES         },
        { Localization::Lookup(IDS_COL_FOLDERS),       FIELD_FOLDERS       },
        { Localization::Lookup(IDS_COL_SIZE_LOGICAL),  FIELD_SIZE_LOGICAL  },
        { Localization::Lookup(IDS_COL_SIZE_PHYSICAL), FIELD_SIZE_PHYSICAL },
        { Localization::Lookup(IDS_COL_ATTRIBUTES),    FIELD_ATTRIBUTES    },
        { Localization::Lookup(IDS_COL_LAST_CHANGE),   FIELD_LAST_CHANGE   },
        { Localization::LookupNeutral(AFX_IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES), FIELD_ATTRIBUTES_WDS },
        { Localization::Lookup(IDS_COL_INDEX),         FIELD_INDEX         },
        { Localization::Lookup(IDS_COL_OWNER),         FIELD_OWNER         }
    };

    CItem* newroot = nullptr;
    std::unordered_map<std::wstring, CItem*> parentMap;
    std::unordered_map<std::wstring, std::wstring> obj;

    // Skip lines until '[' (start of JSON array)
    std::string linebuf;
    while (std::getline(reader, linebuf))
    {
        const char* p = linebuf.c_str();
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '[') break;
    }

    // Read each object in the array
    while (JsonReadObject(reader, obj))
    {
        std::wstring fieldValues[FIELD_COUNT];
        for (auto& [key, val] : obj)
        {
            if (const auto it = keyToField.find(key); it != keyToField.end())
                fieldValues[it->second] = std::move(val);
        }

        // Validate minimum required fields
        if (fieldValues[FIELD_NAME].empty()) continue;

        BuildAndAttachItem(fieldValues[FIELD_NAME], fieldValues[FIELD_ATTRIBUTES_WDS],
            fieldValues[FIELD_LAST_CHANGE], fieldValues[FIELD_SIZE_PHYSICAL],
            fieldValues[FIELD_SIZE_LOGICAL], fieldValues[FIELD_INDEX],
            fieldValues[FIELD_ATTRIBUTES], fieldValues[FIELD_FILES],
            fieldValues[FIELD_FOLDERS], newroot, parentMap);
    }

    for (const auto& val : parentMap | std::views::values)
        val->SortItemsBySizePhysical();

    return newroot;
}

CItem* LoadResults(const std::wstring& path)
{
    std::ifstream reader(path);
    if (!reader.is_open()) return nullptr;
    return IsJsonPath(path) ? LoadResultsJson(reader) : LoadResultsCsv(reader);
}

// Walk the item tree breadth-first and return a flat ordered list
static std::vector<const CItem*> CollectItems(CItem* rootItem)
{
    std::vector<const CItem*> items;
    items.reserve(static_cast<size_t>(rootItem->GetItemsCount()));

    std::vector<CItem*> queue({ rootItem });
    while (!queue.empty())
    {
        const CItem* qitem = queue.back();
        queue.pop_back();

        if (qitem->IsTypeOrFlag(IT_HLINKS)) continue;

        items.push_back(qitem);
        if (qitem->IsLeaf()) continue;

        std::vector<CItem*> children = qitem->GetChildren();
        std::ranges::sort(children, [](auto a, auto b)
        {
            return _wcsicmp(a->GetNameView().data(), b->GetNameView().data()) > 0;
        });

        queue.insert(queue.end(), children.begin(), children.end());
    }
    return items;
}

// Compute adjusted physical sizes (undoes hardlink accounting)
static std::unordered_map<const CItem*, LONGLONG>
    ComputeAdjustedSizes(const std::vector<const CItem*>& items)
{
    std::unordered_map<const CItem*, LONGLONG> adjustedSizes;
    for (const auto* item : items)
    {
        if (item->IsTypeOrFlag(IT_DRIVE))
            if (const auto hlinks = item->FindHardlinksItem(); hlinks != nullptr)
            {
                adjustedSizes[item] -= hlinks->GetSizePhysicalRaw();
                if (const auto root = item->GetParent(); root != nullptr)
                    adjustedSizes[root] -= hlinks->GetSizePhysicalRaw();
            }

        if (!item->IsTypeOrFlag(ITF_HARDLINK)) continue;
        for (const CItem* p = item->GetParent(); p != nullptr; p = p->GetParent())
        {
            adjustedSizes[p] += item->GetSizePhysicalRaw();
            if (!p->IsTypeOrFlag(IT_DRIVE)) continue;
        }
    }
    return adjustedSizes;
}

static bool SaveResultsCsv(std::ofstream& outf, const std::vector<const CItem*>& items,
    const std::vector<std::wstring>& cols, const std::unordered_map<const CItem*, LONGLONG>& adjustedSizes)
{
    // Header
    for (size_t i = 0; i < cols.size(); ++i)
        outf << QuoteAndConvert(cols[i]) << (i + 1 < cols.size() ? "," : "");

    // Rows
    for (const auto* item : items)
    {
        const bool nonPathItem = item->IsTypeOrFlag(IT_MYCOMPUTER);
        const ITEMTYPE itemType = item->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK & ~ITF_EXTDATA;
        const auto adjIt = adjustedSizes.find(item);
        const auto adjustedSize = adjIt != adjustedSizes.end() ? adjIt->second : 0;
        outf << std::format("\r\n{},{},{},{},{},{},{},0x{:08X},0x{:016X}",
            QuoteAndConvert(nonPathItem ? item->GetName() : item->GetPath()),
            item->GetFilesCount(),
            item->GetFoldersCount(),
            item->GetSizeLogical(),
            item->GetSizePhysicalRaw() + adjustedSize,
            QuoteAndConvert(FormatAttributes(item->GetAttributes())),
            ToTimePoint(item->GetLastChange()),
            static_cast<std::uint32_t>(itemType),
            item->GetIndex());
        if (COptions::ShowColumnOwner) outf << "," << QuoteAndConvert(item->GetOwner(true));
    }
    outf.flush();
    return outf.good();
}

// ── JSON save ─────────────────────────────────────────────────────────────────

static bool SaveResultsJson(std::ofstream& outf,
    const std::vector<const CItem*>& items,
    const std::vector<std::wstring>& cols,
    const std::unordered_map<const CItem*, LONGLONG>& adjustedSizes)
{
    // Pre-quote all column key strings once (cols are in FIELD_* index order)
    std::array<std::string, FIELD_COUNT> jk;
    for (size_t i = 0; i < cols.size(); ++i) jk[i] = JsonQuoteW(cols[i]);
    const std::string jkOwner = COptions::ShowColumnOwner ? JsonQuoteW(cols[FIELD_OWNER]) : std::string{};

    outf << "[\r\n";
    bool firstItem = true;
    for (const auto* item : items)
    {
        if (!firstItem) outf << ",\r\n";
        firstItem = false;

        const bool nonPathItem = item->IsTypeOrFlag(IT_MYCOMPUTER);
        const ITEMTYPE itemType = item->GetRawType() & ~ITF_HARDLINK & ~ITHASH_MASK & ~ITF_EXTDATA;
        const auto adjIt = adjustedSizes.find(item);
        const auto adjustedSize = adjIt != adjustedSizes.end() ? adjIt->second : 0;

        // Write one JSON object per item
        outf << "{\r\n";
        outf << "  " << jk[FIELD_NAME]           << ": " << JsonQuoteW(nonPathItem ? item->GetName() : item->GetPath()) << ",\r\n";
        outf << "  " << jk[FIELD_FILES]          << ": " << item->GetFilesCount()                                       << ",\r\n";
        outf << "  " << jk[FIELD_FOLDERS]        << ": " << item->GetFoldersCount()                                     << ",\r\n";
        outf << "  " << jk[FIELD_SIZE_LOGICAL]   << ": " << item->GetSizeLogical()                                      << ",\r\n";
        outf << "  " << jk[FIELD_SIZE_PHYSICAL]  << ": " << (item->GetSizePhysicalRaw() + adjustedSize)                << ",\r\n";
        outf << "  " << jk[FIELD_ATTRIBUTES]     << ": " << JsonQuoteW(FormatAttributes(item->GetAttributes()))        << ",\r\n";
        outf << "  " << jk[FIELD_LAST_CHANGE]    << ": " << JsonQuote(ToTimePoint(item->GetLastChange()))              << ",\r\n";
        outf << std::format("  {}: \"0x{:08X}\",\r\n", jk[FIELD_ATTRIBUTES_WDS], static_cast<std::uint32_t>(itemType));
        outf << std::format("  {}: \"0x{:016X}\"",     jk[FIELD_INDEX],          item->GetIndex());
        if (COptions::ShowColumnOwner)
            outf << ",\r\n  " << jkOwner << ": " << JsonQuoteW(item->GetOwner(true));
        outf << "\r\n}";
    }
    outf << "\r\n]\r\n";
    outf.flush();
    return outf.good();
}

bool SaveResults(const std::wstring& path, CItem* rootItem)
{
    const std::vector<const CItem*> items       = CollectItems(rootItem);
    const auto                      adjustedSizes = ComputeAdjustedSizes(items);

    std::vector<std::wstring> cols =
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

    std::ofstream outf(path, std::ios::binary);
    if (!outf.is_open()) return false;

    return IsJsonPath(path)
        ? SaveResultsJson(outf, items, cols, adjustedSizes)
        : SaveResultsCsv (outf, items, cols, adjustedSizes);
}

static std::vector<std::tuple<std::wstring, const CItem*>>
    CollectAndSortDupes(const CItemDupe* rootDupe)
{
    std::vector<std::tuple<std::wstring, const CItem*>> dupeItems;
    for (const auto& dupeGroup : rootDupe->GetChildren())
        for (const auto& dupeFile : dupeGroup->GetChildren())
            if (const auto* dupeItem = dupeFile->GetLinkedItem(); dupeItem != nullptr)
                dupeItems.emplace_back(dupeGroup->GetHash(), dupeItem);

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
    return dupeItems;
}

static bool SaveDuplicatesCsv(std::ofstream& outf, const std::vector<std::wstring>& cols,
    const std::vector<std::tuple<std::wstring, const CItem*>>& dupeItems)
{
    for (size_t i = 0; i < cols.size(); ++i)
        outf << QuoteAndConvert(cols[i]) << (i + 1 < cols.size() ? "," : "");
    outf << "\r\n";

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
    return outf.good();
}

// ── JSON dupe save ─────────────────────────────────────────────────────────────

static bool SaveDuplicatesJson(std::ofstream& outf,
    const std::vector<std::wstring>& cols,
    const std::vector<std::tuple<std::wstring, const CItem*>>& dupeItems)
{
    // cols order: HASH, NAME, SIZE_LOGICAL, SIZE_PHYSICAL, LAST_CHANGE, ATTRIBUTES
    const auto jHash      = JsonQuoteW(cols[0]);
    const auto jName      = JsonQuoteW(cols[1]);
    const auto jSizeLog   = JsonQuoteW(cols[2]);
    const auto jSizePhys  = JsonQuoteW(cols[3]);
    const auto jLastChg   = JsonQuoteW(cols[4]);
    const auto jAttr      = JsonQuoteW(cols[5]);

    outf << "[\r\n";
    bool first = true;
    for (const auto& [hash, item] : dupeItems)
    {
        if (!first) outf << ",\r\n";
        first = false;
        outf << "{\r\n";
        outf << "  " << jHash     << ": " << JsonQuoteW(hash) << ",\r\n";
        outf << "  " << jName     << ": " << JsonQuoteW(item->GetPath()) << ",\r\n";
        outf << "  " << jSizeLog  << ": " << item->GetSizeLogical() << ",\r\n";
        outf << "  " << jSizePhys << ": " << item->GetSizePhysicalRaw() << ",\r\n";
        outf << "  " << jLastChg  << ": " << JsonQuote(ToTimePoint(item->GetLastChange())) << ",\r\n";
        outf << "  " << jAttr     << ": " << JsonQuoteW(FormatAttributes(item->GetAttributes())) << "\r\n";
        outf << "}";
    }
    outf << "\r\n]\r\n";
    outf.flush();
    return outf.good();
}

// ── public dispatcher ─────────────────────────────────────────────────────────

bool SaveDuplicates(const std::wstring& path, const CItemDupe* rootDupe)
{
    std::ofstream outf(path, std::ios::binary);
    if (!outf.is_open()) return false;

    const std::vector<std::wstring> cols =
    {
        Localization::Lookup(IDS_COL_HASH),
        Localization::Lookup(IDS_COL_NAME),
        Localization::Lookup(IDS_COL_SIZE_LOGICAL),
        Localization::Lookup(IDS_COL_SIZE_PHYSICAL),
        Localization::Lookup(IDS_COL_LAST_CHANGE),
        Localization::Lookup(IDS_COL_ATTRIBUTES)
    };

    const auto dupeItems = CollectAndSortDupes(rootDupe);

    return IsJsonPath(path)
        ? SaveDuplicatesJson(outf, cols, dupeItems)
        : SaveDuplicatesCsv (outf, cols, dupeItems);
}
