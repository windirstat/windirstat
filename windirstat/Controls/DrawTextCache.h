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

#pragma once

#include <afxwin.h>
#include <string>
#include <unordered_map>
#include <list>
#include <memory>

class DrawTextCache
{
public:
    static constexpr size_t MAX_CACHE_SIZE = 1000;

    // Singleton access
    static DrawTextCache& GetInstance()
    {
        static DrawTextCache instance;
        return instance;
    }

    // Main drawing function - replacement for DrawText
    int DrawTextCached(CDC* pDC, const std::wstring& text, CRect& rect, UINT format);

private:
    DrawTextCache() = default;
    ~DrawTextCache() = default;

    // Cache key structure
    struct CacheKey
    {
        std::wstring text;
        COLORREF textColor;
        UINT format;
        int width;
        int height;

        bool operator==(const CacheKey& other) const = default;
    };

    // Hash function for CacheKey
    struct CacheKeyHash
    {
        size_t operator()(const CacheKey& key) const
        {
            size_t hash = std::hash<std::wstring>{}(key.text);
            hash ^= std::hash<COLORREF>{}(key.textColor) << 1;
            hash ^= std::hash<UINT>{}(key.format) << 2;
            hash ^= std::hash<int>{}(key.width) << 3;
            hash ^= std::hash<int>{}(key.height) << 4;
            return hash;
        }
    };

    // Cached bitmap entry - stored via unique_ptr to avoid copy issues with CBitmap
    struct CacheEntry
    {
        CBitmap bitmap;
        CBitmap mask;
        int textHeight = 0;
        CSize bitmapSize;

        CacheEntry() = default;
        ~CacheEntry() = default;

        // Non-copyable
        CacheEntry(const CacheEntry&) = delete;
        CacheEntry& operator=(const CacheEntry&) = delete;

        // Movable
        CacheEntry(CacheEntry&&) = default;
        CacheEntry& operator=(CacheEntry&&) = default;
    };

    // LRU list type - stores keys in order of use (most recent at front)
    using LRUList = std::list<CacheKey>;

    // Cache map type - uses unique_ptr to store entries
    using CacheMap = std::unordered_map<CacheKey,
        std::pair<std::unique_ptr<CacheEntry>, LRUList::iterator>,
        CacheKeyHash>;

    // Create cache key from current DC state
    [[nodiscard]] CacheKey CreateCacheKey(CDC* pDC, const std::wstring& text,
        const CRect& rect, UINT format) const;

    // Create cached bitmap for the text
    std::unique_ptr<CacheEntry> CreateCachedBitmap(CDC* pDC, const std::wstring& text,
        const CRect& rect, UINT format);

    // Evict oldest entry if cache is full
    void EvictIfNeeded();

    // Move key to front of LRU list
    void TouchEntry(CacheMap::iterator it);

    // Paint cached entry to DC
    void PaintCachedEntry(CDC* pDC, const CRect& rect, CacheEntry& entry);

    CacheMap m_cache;
    LRUList m_lruList;
};
