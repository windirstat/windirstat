// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DrawTextCache.h"

void DrawTextCache::DrawTextCached(CDC* pDC, const std::wstring& text, CRect& rect,
    const bool leftAlign, const bool calcRect, const bool cacheBitmap)
{
    // If no DC or empty text, collapse rect and return
    if (!pDC || text.empty())
    {
        rect.right = rect.left;
        rect.bottom = rect.top;
        return;
    }

    const UINT format = DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX |
        (leftAlign ? DT_LEFT : DT_RIGHT) | (calcRect ? DT_CALCRECT : 0);
    const size_t bitmapBytes = !calcRect && rect.Width() > 0 && rect.Height() > 0
        ? static_cast<size_t>(rect.Width()) * rect.Height() * sizeof(COLORREF) : 0;

    // Selected cells retain their partially highlighted background.
    const int backgroundMode = cacheBitmap ? GetBkMode(pDC->m_hDC) : TRANSPARENT;
    ScopedBkMode background(pDC, backgroundMode);
    if (!COptions::UseDrawTextCache || (!calcRect && (backgroundMode == TRANSPARENT || GetLayout(pDC->m_hDC) != 0
        || bitmapBytes == 0 || bitmapBytes > MAX_BITMAP_BYTES)))
    {
        pDC->DrawText(text, &rect, format);
        return;
    }

    // Look up in cache
    CacheKey key = CreateCacheKey(pDC, text, rect, format);
    if (const auto it = m_cache.find(key); it != m_cache.end())
    {
        // Cache hit - use cached entry
        TouchEntry(it);

        // Handle rectangle calculation or normal drawing
        auto& entry = *it->second.first;
        if (calcRect)
            rect.SetBounds(rect.left, rect.top, rect.left + entry.bmpSize.cx, rect.top + entry.bmpSize.cy);
        else PaintCachedEntry(pDC, rect, entry);
        return;
    }

    // Cache miss - create new cached entry
    std::unique_ptr<CacheEntry> entry;
    if (calcRect)
    {
        pDC->DrawText(text, &rect, format);
        entry = std::make_unique<CacheEntry>();
        entry->bmpSize = rect.Size();
    }
    else
    {
        entry = CreateCachedBitmap(pDC, text, rect, format);
        if (!entry)
        {
            pDC->DrawText(text, &rect, format);
            return;
        }
        PaintCachedEntry(pDC, rect, *entry);
    }

    while (m_cache.size() >= MAX_CACHE_SIZE || m_bitmapBytes + bitmapBytes > MAX_BITMAP_BYTES)
    {
        // Remove least recently used (back of list)
        const auto it = m_cache.find(m_leastRecentList.back());
        m_bitmapBytes -= it->second.first->bitmapBytes;
        m_cache.erase(it);
        m_leastRecentList.pop_back();
    }

    // Add to LRU list and cache
    entry->bitmapBytes = bitmapBytes;
    m_bitmapBytes += bitmapBytes;
    m_leastRecentList.push_front(key);
    m_cache.emplace(std::move(key), std::make_pair(std::move(entry), m_leastRecentList.begin()));
}

DrawTextCache::CacheKey DrawTextCache::CreateCacheKey(const CDC* pDC,
    const std::wstring& text, const CRect& rect, const UINT format) noexcept
{
    return CacheKey{
        .text = text, .textColor = pDC->GetTextColor(),
        .backgroundColor = pDC->GetBkColor(), .format = format,
        .width = rect.Width(), .height = rect.Height(),
        .dpi = static_cast<USHORT>(pDC->GetDeviceCaps(LOGPIXELSX)),
        .font = pDC->GetCurrentFont()};
}

std::unique_ptr<DrawTextCache::CacheEntry> DrawTextCache::CreateCachedBitmap(
    CDC* pDC, const std::wstring& text, const CRect& rect, const UINT format) noexcept
{
    CDC memDC(pDC);
    auto entry = std::make_unique<CacheEntry>();
    entry->bmpSize = rect.Size();
    if (!memDC.m_hDC || !entry->bmp.CreateCompatible(pDC, rect.Width(), rect.Height())) return nullptr;
    GdiObjectSelection sofont(&memDC, pDC->GetCurrentFont());
    GdiObjectSelection sobmp(&memDC, &entry->bmp);

    // Native layout within the entire cell preserves fallback glyphs and overhangs.
    CRect drawRect(0, 0, rect.Width(), rect.Height());
    memDC.SetBkColor(pDC->GetBkColor());
    memDC.SetTextColor(pDC->GetTextColor());
    memDC.FillSolidRect(drawRect, pDC->GetBkColor());
    memDC.DrawText(text, &drawRect, format);
    return entry;
}

void DrawTextCache::ClearCache()
{
    m_cache.clear();
    m_leastRecentList.clear();
    m_bitmapBytes = 0;
}

void DrawTextCache::TouchEntry(const CacheMap::iterator& it)
{
    // Move to front of LRU list
    m_leastRecentList.splice(m_leastRecentList.begin(),
        m_leastRecentList, it->second.second);
}

void DrawTextCache::PaintCachedEntry(CDC* pDC, const CRect& rect, const CacheEntry& entry) noexcept
{
    // Create memory DC
    CDC memDC(pDC);
    GdiObjectSelection sobmp(&memDC, &entry.bmp);
    pDC->BitBlt(rect.left, rect.top, entry.bmpSize.cx, entry.bmpSize.cy, &memDC, 0, 0, SRCCOPY);
}
