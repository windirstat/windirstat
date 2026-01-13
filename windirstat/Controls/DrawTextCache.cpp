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

void DrawTextCache::DrawTextCached(CDC* pDC, const std::wstring& text, CRect& rect, const bool leftAlign, const bool calcRect)
{
    if (!pDC || text.empty()) return;

    const UINT format = DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX |
        (leftAlign ? DT_LEFT : DT_RIGHT) | (calcRect ? DT_CALCRECT : 0);

    // If caching is disabled, use normal DrawText API
    if (!COptions::UseDrawTextCache)
    {
        pDC->DrawText(text.c_str(), static_cast<int>(text.length()), &rect, format);
        return;
    }

    // Look up in cache
    CacheKey key = CreateCacheKey(pDC, text, rect, format);
    if (auto it = m_cache.find(key); it != m_cache.end())
    {
        // Cache hit - use cached entry
        TouchEntry(it);

        // Handle rectangle calculation or normal drawing
        auto& entry = *it->second.first;
        if (format & DT_CALCRECT) rect = entry.calcRect;
        else PaintCachedEntry(pDC, rect, entry);
        return;
    }

    // Cache miss - create new cached entry
    while (m_cache.size() >= MAX_CACHE_SIZE && !m_leastRecentList.empty())
    {
        // Remove least recently used (back of list)
        const CacheKey& keyToRemove = m_leastRecentList.back();
        m_cache.erase(keyToRemove);
        m_leastRecentList.pop_back();
    }
    
    // Handle rectangle calculation or normal drawing
    auto entry = CreateCachedBitmap(pDC, text, rect, format);
    if (format & DT_CALCRECT) rect = entry->calcRect;
    else PaintCachedEntry(pDC, rect, *entry);

    // Add to LRU list and cache
    m_leastRecentList.push_front(key);
    m_cache.emplace(std::move(key),
        std::make_pair(std::move(entry), m_leastRecentList.begin()));
}

DrawTextCache::CacheKey DrawTextCache::CreateCacheKey(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format) const
{
    return CacheKey{
        .text = text, .textColor = pDC->GetTextColor(),
        .backgroundColor = pDC->GetBkColor(), .format = format,
        .width = static_cast<USHORT>(rect.Width()), .height = static_cast<USHORT>(rect.Height()),
        .dpi = static_cast<USHORT>(::GetDeviceCaps(pDC->m_hDC, LOGPIXELSX))};
}

std::unique_ptr<DrawTextCache::CacheEntry> DrawTextCache::CreateCachedBitmap(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format)
{
    // Create temporary DC to calculate text bounds
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);

    // Select the same font to get accurate measurements
    CSelectObject sofont(&memDC, pDC->GetCurrentFont());

    // Calculate actual text dimensions
    CRect calcRect(0, 0, rect.Width(), rect.Height());
    memDC.DrawText(text.c_str(), static_cast<int>(text.length()),
        &calcRect, format | DT_CALCRECT);

    // Get font metrics for accurate text height
    TEXTMETRIC tm;
    const int textHeight = memDC.GetTextMetrics(&tm) ? tm.tmHeight : calcRect.Height();

    auto entry = std::make_unique<CacheEntry>();
    entry->bmpSize = CSize(calcRect.Width(), textHeight);
    entry->format = format;

    // Store the calculated rectangle for DT_CALCRECT requests
    // Preserve the original position (left, top) from input rect, only update size
    entry->calcRect = CRect(rect.left, rect.top,
        rect.left + calcRect.Width(), rect.top + calcRect.Height());

    // If this is just a DT_CALCRECT request, we don't need to create the bitmap
    if (format & DT_CALCRECT)
    {
        return entry;
    }

    // Create compatible bitmap sized exactly for the text
    entry->bmp.CreateCompatibleBitmap(pDC, calcRect.Width(), textHeight);
    CSelectObject sobmp(&memDC, &entry->bmp);

    // Fill with background color and draw text with actual text color
    memDC.SetBkColor(pDC->GetBkColor());
    memDC.SetTextColor(pDC->GetTextColor());

    // Fill the bitmap with background color
    CRect drawRect(0, 0, calcRect.Width(), textHeight);
    memDC.FillSolidRect(&drawRect, pDC->GetBkColor());

    // Draw the text without vertical centering (bitmap is exact text height)
    memDC.DrawText(text.c_str(), static_cast<int>(text.length()),
        &drawRect, format & ~DT_VCENTER);

    return entry;
}

void DrawTextCache::ClearCache()
{
    m_cache.clear();
    m_leastRecentList.clear();
}

void DrawTextCache::TouchEntry(CacheMap::iterator it)
{
    // Move to front of LRU list
    m_leastRecentList.splice(m_leastRecentList.begin(),
        m_leastRecentList, it->second.second);
}

void DrawTextCache::PaintCachedEntry(CDC* pDC, const CRect& rect, CacheEntry& entry)
{
    // Create memory DC
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CSelectObject sobmp(&memDC, &entry.bmp);

    // Calculate horizontal position based on alignment
    int xPos = rect.left;
    if (entry.format & DT_RIGHT)
    {
        xPos = rect.right - entry.bmpSize.cx;
    }

    // Calculate vertical position for centering
    const int yPos = rect.top + (rect.Height() - entry.bmpSize.cy) / 2;

    // BitBlt at the calculated position (skip 1 pixel top border)
    pDC->BitBlt(xPos, yPos, entry.bmpSize.cx, entry.bmpSize.cy, &memDC, 0, 1, SRCCOPY);
}
