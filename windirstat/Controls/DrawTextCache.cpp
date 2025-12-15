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
#include "DrawTextCache.h"
#include "SelectObject.h"

int DrawTextCache::DrawTextCached(CDC* pDC, const std::wstring& text,
    CRect& rect, UINT format)
{
    if (!pDC || text.empty())
    {
        return 0;
    }

    // Handle DT_CALCRECT specially - no caching needed
    if (format & DT_CALCRECT)
    {
        return pDC->DrawTextW(text.c_str(), static_cast<int>(text.length()),
            &rect, format);
    }

    // Create cache key based on current state
    CacheKey key = CreateCacheKey(pDC, text, rect, format);

    // Look up in cache
    auto it = m_cache.find(key);
    if (it != m_cache.end())
    {
        // Cache hit - use cached bitmap
        TouchEntry(it);
        PaintCachedEntry(pDC, rect, *it->second.first);
        return it->second.first->textHeight;
    }

    // Cache miss - create new cached bitmap
    EvictIfNeeded();

    auto entry = CreateCachedBitmap(pDC, text, rect, format);
    int textHeight = entry->textHeight;

    PaintCachedEntry(pDC, rect, *entry);

    // Add to LRU list and cache
    m_lruList.push_front(key);
    m_cache.emplace(std::move(key),
        std::make_pair(std::move(entry), m_lruList.begin()));

    return textHeight;
}

DrawTextCache::CacheKey DrawTextCache::CreateCacheKey(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format) const
{
    return CacheKey{
        .text = text,
        .textColor = pDC->GetTextColor(),
        .format = format,
        .width = rect.Width(),
        .height = rect.Height()
    };
}

std::unique_ptr<DrawTextCache::CacheEntry> DrawTextCache::CreateCachedBitmap(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format)
{
    auto entry = std::make_unique<CacheEntry>();
    entry->bitmapSize = CSize(rect.Width(), rect.Height());

    const int width = rect.Width();
    const int height = rect.Height();
    const COLORREF textColor = pDC->GetTextColor();

    // Create memory DC for the color bitmap
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);

    // Create compatible bitmap for the text
    entry->bitmap.CreateCompatibleBitmap(pDC, width, height);
    CBitmap* pOldBitmap = memDC.SelectObject(&entry->bitmap);

    // Fill with black background, draw text in white for mask creation
    memDC.FillSolidRect(0, 0, width, height, RGB(0, 0, 0));
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(255, 255, 255));

    // Select the same font from source DC
    CFont* pFont = pDC->GetCurrentFont();
    CFont* pOldFont = nullptr;
    if (pFont)
    {
        pOldFont = memDC.SelectObject(pFont);
    }

    // Draw the text to create the mask source
    CRect drawRect(0, 0, width, height);
    entry->textHeight = memDC.DrawTextW(text.c_str(),
        static_cast<int>(text.length()),
        &drawRect, format);

    // Create monochrome mask bitmap
    CDC maskDC;
    maskDC.CreateCompatibleDC(pDC);
    entry->mask.CreateBitmap(width, height, 1, 1, nullptr);
    CBitmap* pOldMask = maskDC.SelectObject(&entry->mask);

    // Set the background color to black so white text becomes 0 (transparent)
    // and black background becomes 1 (opaque/mask)
    memDC.SetBkColor(RGB(0, 0, 0));
    maskDC.BitBlt(0, 0, width, height, &memDC, 0, 0, SRCCOPY);

    // Now redraw the bitmap with the actual text color
    memDC.FillSolidRect(0, 0, width, height, RGB(0, 0, 0));
    memDC.SetTextColor(textColor);
    memDC.DrawTextW(text.c_str(), static_cast<int>(text.length()),
        &drawRect, format);

    // Cleanup
    if (pOldFont)
    {
        memDC.SelectObject(pOldFont);
    }
    memDC.SelectObject(pOldBitmap);
    maskDC.SelectObject(pOldMask);

    return entry;
}

void DrawTextCache::EvictIfNeeded()
{
    while (m_cache.size() >= MAX_CACHE_SIZE && !m_lruList.empty())
    {
        // Remove least recently used (back of list)
        const CacheKey& keyToRemove = m_lruList.back();
        m_cache.erase(keyToRemove);
        m_lruList.pop_back();
    }
}

void DrawTextCache::TouchEntry(CacheMap::iterator it)
{
    // Move to front of LRU list
    auto& lruIt = it->second.second;
    if (lruIt != m_lruList.begin())
    {
        m_lruList.splice(m_lruList.begin(), m_lruList, lruIt);
    }
}

void DrawTextCache::PaintCachedEntry(CDC* pDC, const CRect& rect, CacheEntry& entry)
{
    const int width = entry.bitmapSize.cx;
    const int height = entry.bitmapSize.cy;

    // Create memory DCs
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CBitmap* pOldBitmap = memDC.SelectObject(&entry.bitmap);

    CDC maskDC;
    maskDC.CreateCompatibleDC(pDC);
    CBitmap* pOldMask = maskDC.SelectObject(&entry.mask);

    // Use mask to paint transparently
    pDC->BitBlt(rect.left, rect.top, width, height, &maskDC, 0, 0, SRCAND);
    pDC->BitBlt(rect.left, rect.top, width, height, &memDC, 0, 0, SRCPAINT);

    // Cleanup
    memDC.SelectObject(pOldBitmap);
    maskDC.SelectObject(pOldMask);
}
