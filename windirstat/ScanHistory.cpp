// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ScanHistory.h"

#include <charconv>

namespace
{
    constexpr std::size_t MAX_POINTS = 365;           // per folder, oldest are dropped
    constexpr std::size_t MAX_FOLDERS = 2000;         // folders stored per snapshot
    constexpr ULONGLONG MIN_SHARE_DIVISOR = 1000;     // folders below 0.1 % of the root are not stored
    constexpr std::int64_t MIN_POINT_GAP = 60;        // seconds; closer scans replace the last point

    struct Folder
    {
        std::wstring path;                  // original spelling; the map key is lower-cased
        std::vector<HistoryPoint> points;
    };
    using FolderMap = std::unordered_map<std::wstring, Folder>;

    // Cache of the origin currently displayed (all access is from the UI thread).
    std::wstring g_origin;
    FolderMap g_folders;
    bool g_loaded = false;

    std::string ToUtf8(const std::wstring_view text)
    {
        if (text.empty()) return {};
        std::string out(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
            out.data(), static_cast<int>(out.size()), nullptr, nullptr);
        return out;
    }

    std::wstring FromUtf8(const std::string_view text)
    {
        if (text.empty()) return {};
        std::wstring out(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0)), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
            out.data(), static_cast<int>(out.size()));
        return out;
    }

    std::string JsonString(const std::wstring& text)
    {
        std::string out = "\"";
        for (const char c : ToUtf8(text))
        {
            if (c == '"' || c == '\\') { out += '\\'; out += c; }
            else if (static_cast<unsigned char>(c) < 0x20) out += std::format("\\u{:04x}", static_cast<int>(c));
            else out += c;
        }
        return out + '"';
    }

    // ---- Minimal JSON reader: just enough for the history file ----
    struct Json
    {
        enum class Type { Null, Number, String, Array, Object } type = Type::Null;
        double number = 0;
        std::string string;
        std::vector<Json> items;                           // array elements or object values
        std::vector<std::string> keys;                     // object keys (parallel to items)

        const Json* Get(const std::string_view key) const
        {
            for (std::size_t i = 0; i < keys.size(); ++i) if (keys[i] == key) return &items[i];
            return nullptr;
        }
    };

    class JsonReader
    {
    public:
        explicit JsonReader(const std::string_view text) : m_text(text) {}

        bool Parse(Json& out)
        {
            return ParseValue(out, 0) && (SkipSpace(), m_pos == m_text.size());
        }

    private:
        void SkipSpace()
        {
            while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
        }

        bool Consume(const char c)
        {
            SkipSpace();
            if (m_pos >= m_text.size() || m_text[m_pos] != c) return false;
            ++m_pos;
            return true;
        }

        bool ParseString(std::string& out)
        {
            if (!Consume('"')) return false;
            out.clear();
            while (m_pos < m_text.size())
            {
                const char c = m_text[m_pos++];
                if (c == '"') return true;
                if (c != '\\') { out += c; continue; }
                if (m_pos >= m_text.size()) return false;
                const char e = m_text[m_pos++];
                switch (e)
                {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':
                {
                    if (m_pos + 4 > m_text.size()) return false;
                    unsigned code = 0;
                    const auto [ptr, ec] = std::from_chars(m_text.data() + m_pos, m_text.data() + m_pos + 4, code, 16);
                    if (ec != std::errc{} || ptr != m_text.data() + m_pos + 4) return false;
                    m_pos += 4;
                    // History files only escape control characters, so one UTF-16 unit is enough.
                    out += ToUtf8(std::wstring(1, static_cast<wchar_t>(code)));
                    break;
                }
                default: out += e; break;
                }
            }
            return false;
        }

        bool ParseValue(Json& out, const int depth)
        {
            if (depth > 16) return false;
            SkipSpace();
            if (m_pos >= m_text.size()) return false;
            const char c = m_text[m_pos];
            if (c == '"')
            {
                out.type = Json::Type::String;
                return ParseString(out.string);
            }
            if (c == '[' || c == '{')
            {
                const bool object = c == '{';
                out.type = object ? Json::Type::Object : Json::Type::Array;
                ++m_pos;
                if (Consume(object ? '}' : ']')) return true;
                do
                {
                    if (object)
                    {
                        std::string key;
                        if (!ParseString(key) || !Consume(':')) return false;
                        out.keys.push_back(std::move(key));
                    }
                    Json child;
                    if (!ParseValue(child, depth + 1)) return false;
                    out.items.push_back(std::move(child));
                } while (Consume(','));
                return Consume(object ? '}' : ']');
            }

            // Number (the only other value type the file contains)
            const char* begin = m_text.data() + m_pos;
            const char* end = m_text.data() + m_text.size();
            const auto [ptr, ec] = std::from_chars(begin, end, out.number);
            if (ec != std::errc{}) return false;
            out.type = Json::Type::Number;
            m_pos += static_cast<std::size_t>(ptr - begin);
            return true;
        }

        std::string_view m_text;
        std::size_t m_pos = 0;
    };

    std::uint64_t Fnv1a(const std::wstring& text)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (const wchar_t c : text) hash = (hash ^ static_cast<std::uint64_t>(c)) * 1099511628211ULL;
        return hash;
    }

    std::wstring GetHistoryFolder()
    {
        if (!COptions::HistoryFolder.Obj().empty()) return COptions::HistoryFolder.Obj();
        if (CDirStatApp::InPortableMode()) return GetAppFolder() + L"\\History";

        PWSTR appData = nullptr;
        std::wstring folder;
        if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &appData) == S_OK && appData != nullptr)
            folder = std::wstring(appData) + L"\\WinDirStat\\History";
        CoTaskMemFree(appData);
        return folder;
    }

    std::int64_t NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void Load(const std::wstring& origin)
    {
        g_origin = origin;
        g_folders.clear();
        g_loaded = true;

        const std::wstring file = CScanHistory::GetFilePath(origin);
        if (file.empty()) return;

        std::ifstream stream(file, std::ios::binary);
        if (!stream) return;
        const std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        Json root;
        if (!JsonReader(content).Parse(root) || root.type != Json::Type::Object) return;
        const Json* folders = root.Get("folders");
        if (folders == nullptr || folders->type != Json::Type::Array) return;

        for (const Json& folder : folders->items)
        {
            const Json* path = folder.Get("path");
            const Json* points = folder.Get("points");
            if (path == nullptr || points == nullptr || path->type != Json::Type::String
                || points->type != Json::Type::Array) continue;

            std::vector<HistoryPoint> list;
            for (const Json& point : points->items)
            {
                // [time, size, allocated, files]
                if (point.type != Json::Type::Array || point.items.size() != 4) continue;
                list.push_back({ static_cast<std::int64_t>(point.items[0].number),
                    static_cast<ULONGLONG>(point.items[1].number),
                    static_cast<ULONGLONG>(point.items[2].number),
                    static_cast<ULONGLONG>(point.items[3].number) });
            }
            std::ranges::sort(list, {}, &HistoryPoint::time);
            if (list.empty()) continue;
            std::wstring name = FromUtf8(path->string);
            g_folders[MakeLower(name)] = { name, std::move(list) };
        }
    }

    void Save(const std::wstring& origin)
    {
        const std::wstring file = CScanHistory::GetFilePath(origin);
        if (file.empty()) return;

        std::string json = "{\n  \"version\": 1,\n  \"origin\": " + JsonString(origin) + ",\n  \"folders\": [";
        bool firstFolder = true;
        for (const auto& folder : g_folders | std::views::values)
        {
            const auto& points = folder.points;
            json += firstFolder ? "\n    " : ",\n    ";
            firstFolder = false;
            json += "{ \"path\": " + JsonString(folder.path) + ", \"points\": [";
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const HistoryPoint& p = points[i];
                json += std::format("{}[{},{},{},{}]", i == 0 ? "" : ",", p.time, p.size, p.allocated, p.files);
            }
            json += "] }";
        }
        json += "\n  ]\n}\n";

        // Write to a temporary file first so a crash cannot corrupt the existing history.
        const std::filesystem::path target(file);
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
        const std::filesystem::path temp = target.wstring() + L".tmp";
        {
            std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
            if (!stream) return;
            stream.write(json.data(), static_cast<std::streamsize>(json.size()));
            if (!stream) return;
        }
        std::filesystem::rename(temp, target, ec);
        if (ec) std::filesystem::remove(temp, ec);
    }
}

std::wstring CScanHistory::GetFilePath(const std::wstring& origin)
{
    const std::wstring folder = GetHistoryFolder();
    if (folder.empty() || origin.empty()) return {};

    std::wstring name;
    for (const wchar_t c : origin)
    {
        if (name.size() >= 60) break;
        name += (std::iswalnum(c) && c < 128) || c == L'-' || c == L'.' ? c : L'_';
    }
    return std::format(L"{}\\{}_{:016x}.json", folder, name, Fnv1a(MakeLower(origin)));
}

const std::vector<HistoryPoint>* CScanHistory::Find(const std::wstring& origin, const std::wstring& path)
{
    if (!g_loaded || g_origin != origin) Load(origin);
    const auto found = g_folders.find(MakeLower(path));
    return found == g_folders.end() ? nullptr : &found->second.points;
}

void CScanHistory::RecordScan(const CItem* root, const std::wstring& origin)
{
    if (root == nullptr || origin.empty()) return;
    if (!g_loaded || g_origin != origin) Load(origin);

    // Collect the root and every folder big enough to be worth a history.
    struct Entry { const CItem* item; ULONGLONG size; };
    std::vector<Entry> entries;
    const ULONGLONG minSize = root->GetSizeLogical() / MIN_SHARE_DIVISOR;
    std::vector<const CItem*> stack{ root };
    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();
        if (item->IsTypeOrFlag(IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX)) continue;
        if (item == root || item->GetSizeLogical() >= minSize)
            entries.push_back({ item, item->GetSizeLogical() });
        else continue;   // a small folder cannot contain a big one
        for (const CItem* child : item->GetChildren())
            if (!child->IsLeaf()) stack.push_back(child);
    }
    std::ranges::sort(entries, std::greater<>{}, &Entry::size);
    if (entries.size() > MAX_FOLDERS) entries.resize(MAX_FOLDERS);

    const std::int64_t now = NowSeconds();
    for (const auto& [item, size] : entries)
    {
        const std::wstring path = item->GetPath();
        const std::wstring key = MakeLower(path);
        const HistoryPoint point{ now, size, item->GetSizePhysical(), item->GetFilesCount() };
        auto& folder = g_folders[key];
        folder.path = path;
        auto& points = folder.points;
        if (!points.empty() && now - points.back().time < MIN_POINT_GAP) points.back() = point;
        else points.push_back(point);
        if (points.size() > MAX_POINTS)
            points.erase(points.begin(), points.begin() + static_cast<std::ptrdiff_t>(points.size() - MAX_POINTS));
    }

    Save(origin);
}

bool CScanHistory::RemovePoint(const std::wstring& origin, const std::wstring& path, const std::int64_t time)
{
    if (!g_loaded || g_origin != origin) Load(origin);
    const auto found = g_folders.find(MakeLower(path));
    if (found == g_folders.end()) return false;

    auto& points = found->second.points;
    const auto removed = std::erase_if(points, [time](const HistoryPoint& p) { return p.time == time; });
    if (removed == 0) return false;
    if (points.empty()) g_folders.erase(found);
    Save(origin);
    return true;
}

void CScanHistory::Purge(const std::wstring& origin)
{
    g_folders.clear();
    g_origin = origin;
    g_loaded = true;
    const std::wstring file = GetFilePath(origin);
    if (file.empty()) return;
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(file), ec);
}

void CScanHistory::SetStorageFolder(const std::wstring& folder)
{
    COptions::HistoryFolder = folder;
    g_loaded = false;   // the next access reads the origin from the new location
}

std::vector<std::int64_t> CScanHistory::GetScanTimes(const std::wstring& origin)
{
    if (!g_loaded || g_origin != origin) Load(origin);

    std::vector<std::int64_t> times;
    for (const auto& folder : g_folders | std::views::values)
        for (const HistoryPoint& p : folder.points) times.push_back(p.time);
    std::ranges::sort(times);
    times.erase(std::ranges::unique(times).begin(), times.end());
    return times;
}

std::vector<CScanHistory::FolderChange> CScanHistory::GetChanges(const std::wstring& origin, const std::int64_t baseline)
{
    if (!g_loaded || g_origin != origin) Load(origin);

    // Only folders stored by the newest scan can be compared with an earlier scan.
    std::int64_t newest = 0;
    for (const auto& folder : g_folders | std::views::values) newest = std::max(newest, folder.points.back().time);

    std::vector<FolderChange> changes;
    for (const auto& folder : g_folders | std::views::values)
    {
        const auto& points = folder.points;
        if (points.size() < 2 || points.back().time != newest) continue;
        if (baseline == 0)
        {
            changes.push_back({ folder.path, points[points.size() - 2], points.back() });
            continue;
        }

        // A folder too small in the chosen scan has no point there and is left out.
        const auto previous = std::ranges::find(points, baseline, &HistoryPoint::time);
        if (previous == points.end() || previous->time == newest) continue;
        changes.push_back({ folder.path, *previous, points.back() });
    }
    return changes;
}
