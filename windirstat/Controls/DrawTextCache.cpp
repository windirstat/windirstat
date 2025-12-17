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

int DrawTextCache::DrawTextCached(CDC* pDC, const std::wstring& text, CRect& rect, UINT format)
{
    ASSERT((format & DT_SINGLELINE) != 0);
    ASSERT((format & DT_VCENTER) != 0);

    if (!pDC || text.empty())
    {
        return 0;
    }

    // Look up in cache
    CacheKey key = CreateCacheKey(pDC, text, rect, format);
    if (auto it = m_Cache.find(key); it != m_Cache.end())
    {
        // Cache hit - use cached entry
        TouchEntry(it);

        // Handle rectangle calculation or normal drawing
        if (format & DT_CALCRECT) rect = it->second.first->calculatedRect;
        else PaintCachedEntry(pDC, rect, *it->second.first);

        return it->second.first->textHeight;
    }

    // Cache miss - create new cached entry
    EvictIfNeeded();

    // Handle rectangle calculation or normal drawing
    auto entry = CreateCachedBitmap(pDC, text, rect, format);
    const int textHeight = entry->textHeight;
    if (format & DT_CALCRECT) rect = entry->calculatedRect;
    else PaintCachedEntry(pDC, rect, *entry);

    // Add to LRU list and cache
    m_LeastRecentList.push_front(key);
    m_Cache.emplace(std::move(key),
        std::make_pair(std::move(entry), m_LeastRecentList.begin()));

    return textHeight;
}

DrawTextCache::CacheKey DrawTextCache::CreateCacheKey(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format) const
{
    return CacheKey{
        .text = text, .textColor = pDC->GetTextColor(),
        .backgroundColor = pDC->GetBkColor(), .format = format,
        .width = rect.Width(), .height = rect.Height() };
}

std::unique_ptr<DrawTextCache::CacheEntry> DrawTextCache::CreateCachedBitmap(
    CDC* pDC, const std::wstring& text, const CRect& rect, UINT format)
{
    // Create temporary DC to calculate text bounds
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);

    // Select the same font to get accurate measurements
    SmartPointer<CFont*> pOldFont([&](CFont* p) { memDC.SelectObject(p); }, nullptr);
    if (CFont* pFont = pDC->GetCurrentFont(); pFont)
    {
        pOldFont = memDC.SelectObject(pFont);
    }

    // Calculate actual text dimensions
    CRect calcRect(0, 0, rect.Width(), rect.Height());
    memDC.DrawTextW(text.c_str(), static_cast<int>(text.length()),
        &calcRect, format | DT_CALCRECT);

    // For single-line text, use font metrics for accurate height
    int textHeight = calcRect.Height();
    if (TEXTMETRIC tm; memDC.GetTextMetrics(&tm))
    {
        textHeight = tm.tmHeight - 1;
    }

    // Store the actual drawn rectangle (relative to input rect)
    // The vertical offset positions the text correctly within the target rect
    const int vertOffset = (rect.Height() - textHeight) / 2;
    auto entry = std::make_unique<CacheEntry>();
    entry->drawnRect = CRect(calcRect.left, vertOffset, calcRect.right, vertOffset + textHeight);
    entry->textHeight = textHeight;
    entry->bitmapSize = CSize(calcRect.Width(), textHeight);

    // Store the calculated rectangle for DT_CALCRECT requests
    // Preserve the original position (left, top) from input rect, only update size
    entry->calculatedRect = CRect(rect.left, rect.top,
        rect.left + calcRect.Width(), rect.top + calcRect.Height());

    // If this is just a DT_CALCRECT request, we don't need to create the bitmap
    if (format & DT_CALCRECT)
    {
        return entry;
    }

    // Create compatible bitmap for the text
    entry->bitmap.CreateCompatibleBitmap(pDC, calcRect.Width(), textHeight);
    SmartPointer<CBitmap*> pOldBitmap([&](CBitmap* p) { memDC.SelectObject(p); },
        memDC.SelectObject(&entry->bitmap));

    // Fill with background color and draw text with actual text color
    memDC.SetBkColor(pDC->GetBkColor());
    memDC.SetTextColor(pDC->GetTextColor());

    // Fill the entire bitmap with background color first
    CRect drawRect(0, 0, calcRect.Width(), textHeight);
    memDC.FillSolidRect(&drawRect, pDC->GetBkColor());

    // Draw the text - remove vertical alignment flags since we're drawing into
    // a bitmap sized exactly for the text
    memDC.DrawTextW(text.c_str(), static_cast<int>(text.length()),
        &drawRect, format & ~DT_VCENTER);

    return entry;
}

void DrawTextCache::EvictIfNeeded()
{
    while (m_Cache.size() >= MAX_CACHE_SIZE && !m_LeastRecentList.empty())
    {
        // Remove least recently used (back of list)
        const CacheKey& keyToRemove = m_LeastRecentList.back();
        m_Cache.erase(keyToRemove);
        m_LeastRecentList.pop_back();
    }
}

void DrawTextCache::TouchEntry(CacheMap::iterator it)
{
    // Move to front of LRU list
    auto& lruIt = it->second.second;
    if (lruIt != m_LeastRecentList.begin())
    {
        m_LeastRecentList.splice(m_LeastRecentList.begin(), m_LeastRecentList, lruIt);
    }
}

void DrawTextCache::PaintCachedEntry(CDC* pDC, const CRect& rect, CacheEntry& entry)
{
    // Create memory DC
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    SmartPointer<CBitmap*> pOldBitmap([&](CBitmap* p) { memDC.SelectObject(p); },
        memDC.SelectObject(&entry.bitmap));

    // BitBlt at the offset where the text was actually drawn
    // entry.drawnRect contains the position relative to the input rect's origin
    pDC->BitBlt(rect.left + entry.drawnRect.left, rect.top + entry.drawnRect.top,
        entry.bitmapSize.cx, entry.bitmapSize.cy, &memDC, 0, 0, SRCCOPY);
}
