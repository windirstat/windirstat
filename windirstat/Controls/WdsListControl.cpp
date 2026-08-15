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
#include "TreeMap.h"
#include "WdsListControl.h"
#include "DrawTextCache.h"

namespace
{
    constexpr UINT TEXT_X_MARGIN = 6u; // Horizontal distance of the text from the edge of the item rectangle
    constexpr UINT LABEL_INFLATE_CX = 3u; // How much the label is enlarged, to get the selection and focus rectangle
    constexpr UINT LABEL_Y_MARGIN = 2u;
    constexpr UINT GENERAL_INDENT = 5u;

    class SelectionPreserver
    {
    public:
        explicit SelectionPreserver(CWdsListControl* list)
            : m_list(list)
        {
            assert(m_list != nullptr);

            if (const int i = m_list->GetNextItem(-1, LVNI_FOCUSED); i != -1)
            {
                m_focusedItem = m_list->GetItem(i);
                m_list->SetItemState(i, 0, LVIS_FOCUSED);
            }

            if (const int i = m_list->GetSelectionMark(); i != -1)
            {
                m_selectionMarkItem = m_list->GetItem(i);
            }

            for (int i = m_list->GetNextItem(-1, LVNI_SELECTED); i != -1; i = m_list->GetNextItem(i, LVNI_SELECTED))
            {
                if (auto* item = m_list->GetItem(i)) m_selectedItems.push_back(item);
            }

            m_list->SetItemState(-1, 0, LVIS_SELECTED);
        }

        ~SelectionPreserver()
        {
            int firstSelected = -1;
            for (const auto* item : m_selectedItems)
            {
                if (const int i = m_list->FindListItem(item); i != -1)
                {
                    m_list->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
                    if (firstSelected == -1) firstSelected = i;
                }
            }

            RestoreSelectionMark(firstSelected);

            if (const int focused = m_list->FindListItem(m_focusedItem); focused != -1 && IsVisible(focused))
            {
                m_list->SetItemState(focused, LVIS_FOCUSED, LVIS_FOCUSED);
            }
        }

    private:
        void RestoreSelectionMark(const int firstSelected) const
        {
            int selectionMark = -1;
            if (m_selectionMarkItem != nullptr)
            {
                selectionMark = m_list->FindListItem(m_selectionMarkItem);
                if (selectionMark == -1) selectionMark = firstSelected;
            }

            m_list->SetSelectionMark(selectionMark);
        }

        bool IsVisible(const int i) const
        {
            const int top = m_list->GetTopIndex();
            return i >= top && i < top + m_list->GetCountPerPage();
        }

        CWdsListControl* m_list;
        std::vector<CWdsListItem*> m_selectedItems;
        CWdsListItem* m_focusedItem = nullptr;
        CWdsListItem* m_selectionMarkItem = nullptr;
    };
}

/////////////////////////////////////////////////////////////////////////////
// CWdsListItem

int CWdsListItem::CompareSort(const CWdsListItem* other, const SSorting& sorting) const
{
    int r = Compare(other, sorting.subitem1);
    if (std::abs(r) < 2 && !sorting.ascending1)
    {
        r = -r;
    }

    if (r == 0 && sorting.subitem1 != sorting.subitem2)
    {
        r = Compare(other, sorting.subitem2);
        if (std::abs(r) < 2 && !sorting.ascending2)
        {
            r = -r;
        }
    }
    return r;
}

// Draws an item label (icon, text) in all parts of the WinDirStat view
// the rest is drawn by DrawItem()
void CWdsListItem::DrawLabel(const CWdsListControl* list, CDC* pdc, CRect& rc, const UINT state, int* width, int* focusLeft, const bool indent)
{
    CRect rcRest = rc;

    // Increase indentation according to tree-level
    if (indent)
    {
        rcRest.left += GENERAL_INDENT;
    }

    // Get default small icon parameters
    static const CSize sizeImage(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (width == nullptr)
    {
        // Draw the color with transparent background
        if (const auto icon = GetIcon(); icon != nullptr)
        {
            const CPoint pt(rcRest.left, rcRest.top + rcRest.Height() / 2 - sizeImage.cy / 2);
            GetIconHandler()->DrawIcon(pdc, icon, pt, sizeImage);
        }
    }

    // Decrease size of the remainder rectangle from left
    rcRest.left += sizeImage.cy;

    GdiObjectSelection sofont(pdc, list->GetFont());

    rcRest.Deflate(TEXT_X_MARGIN, 0);

    CRect rcLabel = rcRest;
    DrawTextCache::Get().DrawTextCached(pdc, GetText(0), rcLabel, true, true);

    rcLabel.Inflate(LABEL_INFLATE_CX, 0);
    rcLabel.top = rcRest.top + LABEL_Y_MARGIN;
    rcLabel.bottom = rcRest.bottom - LABEL_Y_MARGIN;

    COLORREF textColor = GetItemTextColor();
    if (width == nullptr && (state & ODS_SELECTED) != 0)
    {
        // Color for the text in a highlighted item (usually white)
        textColor = list->GetHighlightTextColor();

        CRect selection = rcLabel;
        if (list->IsFullRowSelection())
        {
            selection.right = rc.right;
        }
        // Fill the selection rectangle background (usually dark blue)
        pdc->FillSolidRect(selection, list->GetHighlightColor());
    }

    // Set text color for device context
    ScopedTextColor stc(pdc, textColor);

    if (width == nullptr)
    {
        // Draw the actual text
        DrawTextCache::Get().DrawTextCached(pdc, GetText(0), rcRest);
    }

    rcLabel.Inflate(1, 1);

    *focusLeft = rcLabel.left;

    if ((state & ODS_FOCUS) != 0 && list->HasFocus() && width == nullptr && !list->IsFullRowSelection())
    {
        DarkMode::DrawFocusRect(pdc, rcLabel);
    }

    if (width == nullptr)
    {
        DrawAdditionalState(pdc, rcLabel);
    }

    rcLabel.left = rc.left;
    rc = rcLabel;

    if (width != nullptr)
    {
        *width = rcLabel.Width() + 5; // Don't know, why +5
    }
}

void CWdsListItem::DrawSelection(const CWdsListControl* list, CDC* pdc, CRect rc, const UINT state) const
{
    if (!list->IsFullRowSelection())
    {
        return;
    }
    if ((state & ODS_SELECTED) == 0)
    {
        return;
    }

    rc.Deflate(0, LABEL_Y_MARGIN);
    pdc->FillSolidRect(rc, list->GetHighlightColor());
}

void CWdsListItem::DrawPercentage(CDC* pdc, const CRect rc, const double fraction, const COLORREF color) const
{
    COLORREF dark = RGB(118, 118, 118); // Dark edge
    COLORREF light = RGB(198, 198, 198); // Light edge
    COLORREF bg = RGB(225, 225, 225); // Background
    if (DarkMode::IsDarkModeActive())
    {
        dark = RGB(60, 60, 60); // Dark edge
        light = RGB(50, 50, 50); // Light edge
        bg = RGB(40, 40, 40); // Background
    }

    CRect rcLeft = rc;
    rcLeft.right = static_cast<int>(rcLeft.left + rc.Width() * fraction);

    CRect rcRight = rc;
    rcRight.left = rcLeft.right;

    if (rcLeft.right > rcLeft.left)
    {
        pdc->Draw3dRect(rcLeft, light, dark);
    }
    rcLeft.Deflate(1, 1);
    if (rcLeft.right > rcLeft.left)
    {
        pdc->FillSolidRect(rcLeft, color);
    }

    if (rcRight.right > rcRight.left)
    {
        pdc->Draw3dRect(rcRight, light, light);
    }
    rcRight.Deflate(1, 1);
    if (rcRight.right > rcRight.left)
    {
        pdc->FillSolidRect(rcRight, bg);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CWdsListControl

CWdsListControl::CWdsListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths, std::vector<int>* columnVisibility)
    : m_columnOrder(columnOrder)
    , m_columnWidths(columnWidths)
    , m_columnVisibility(columnVisibility)
{
    assert(m_columnOrder != nullptr);
    assert(m_columnWidths != nullptr);
    assert(m_columnVisibility != nullptr);
    InitializeColors();
}

// This method MUST be called before the Control is shown.
void CWdsListControl::OnColumnsInserted(
    const std::initializer_list<int> requiredColumns,
    const std::initializer_list<int> defaultHiddenColumns)
{
    // Cache the column count
    m_columnCount = Header().GetItemCount();
    m_defaultColumnWidths.resize(m_columnCount);
    for (const int column : std::views::iota(0, m_columnCount))
    {
        m_defaultColumnWidths[column] = GetColumnWidth(column);
    }

    m_requiredColumns.assign(requiredColumns);
    if (m_columnCount > 0)
    {
        const int first = ColumnToSubItem(0);
        if (std::ranges::find(m_requiredColumns, first) == m_requiredColumns.end())
        {
            m_requiredColumns.push_back(first);
        }
    }

    auto& visibility = *m_columnVisibility;
    const size_t previousSize = visibility.size();
    for (const int column : std::views::iota(0, m_columnCount))
    {
        const int subitem = ColumnToSubItem(column);
        if (subitem >= static_cast<int>(visibility.size()))
        {
            visibility.resize(subitem + 1, 1);
        }
        if (subitem >= static_cast<int>(previousSize))
        {
            visibility[subitem] = std::ranges::find(defaultHiddenColumns, subitem) == defaultHiddenColumns.end();
        }
        if (IsColumnRequired(subitem))
        {
            visibility[subitem] = 1;
        }
    }

    // The pacman shall not draw over our header control.
    ModifyStyle(0, WS_CLIPCHILDREN);
    ModifyStyle(0, LVS_OWNERDATA);
    LoadPersistentAttributes();
    for (const int column : std::views::iota(0, m_columnCount))
    {
        ApplyColumnVisibility(column);
    }

    // Calculate row height now that window is created
    CalculateRowHeight();

    // Force the list control to register a new row height
    // This is necessary for controls embedded in a dialog resource.
    SetRowHeight(m_rowHeight);
}

void CWdsListControl::OnFontSizeChanged(const int oldPercent, const int newPercent)
{
    for (int& width : m_defaultColumnWidths) width = MulDiv(width, newPercent, oldPercent);
    for (const int column : std::views::iota(0, m_columnCount))
    {
        const int width = GetColumnWidth(column);
        if (width <= 0) continue;

        const int scaledWidth = MulDiv(width, newPercent, oldPercent);
        SetColumnWidth(column, scaledWidth);
        if (std::cmp_less(column, m_columnWidths->size())) (*m_columnWidths)[column] = scaledWidth;
    }
}

void CWdsListControl::SysColorChanged()
{
    InitializeColors();
    CalculateRowHeight();
}

void CWdsListControl::CalculateRowHeight()
{
    // Create a device context to get font metrics
    if (!IsWindow(m_hWnd)) return;
    CClientDC dc(this);
    GdiObjectSelection sofont(&dc, GetFont());

    if (const auto metrics = dc.TextMetrics())
    {
        // Row height = font height + padding
        // Make sure it's odd number for dotted connector mating
        m_rowHeight = (metrics->tmHeight + (LABEL_Y_MARGIN * 2) + 1) | 1;
    }
}

void CWdsListControl::ShowGrid(const bool show)
{
    m_showGrid = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void CWdsListControl::ShowStripes(const bool show)
{
    m_showStripes = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void CWdsListControl::ShowFullRowSelection(const bool show)
{
    m_showFullRowSelect = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

COLORREF CWdsListControl::GetHighlightColor() const
{
    if (HasFocus())
    {
        return DarkMode::SystemColor(COLOR_HIGHLIGHT);
    }

    return DarkMode::IsDarkModeActive() ? RGB(90, 90, 90) : RGB(190, 190, 190);
}

COLORREF CWdsListControl::GetHighlightTextColor() const
{
    if (HasFocus())
    {
        return DarkMode::SystemColor(COLOR_HIGHLIGHTTEXT);
    }

    return DarkMode::IsDarkModeActive() ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

COLORREF CWdsListControl::GetItemSelectionBackgroundColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection())
    {
        return GetHighlightColor();
    }

    return GetItemBackgroundColor(i);
}

COLORREF CWdsListControl::GetItemSelectionTextColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection())
    {
        return GetHighlightTextColor();
    }

    return DarkMode::SystemColor(COLOR_WINDOWTEXT);
}

int CWdsListControl::GetGeneralLeftIndent() const
{
    return GENERAL_INDENT;
}

CWdsListItem* CWdsListControl::GetItem(const int i) const
{
    if (i < 0 || i >= static_cast<int>(m_items.size()))
    {
        return nullptr;
    }

    return m_items[i];
}

int CWdsListControl::FindListItem(const CWdsListItem* item) const
{
    if (const auto it = m_itemMap.find(const_cast<CWdsListItem*>(item)); it != m_itemMap.end())
    {
        return it->second;
    }
    return -1;
}

void CWdsListControl::InitializeColors()
{
    // I try to find a good contrast to COLOR_WINDOW (usually white or light grey).
    // This is a result of experiments.

    constexpr double diff = 0.07; // Try to alter the brightness by diff.
    constexpr double threshold = 1.04; // If result would be brighter, make color darker.

    m_windowColor = DarkMode::SystemColor(COLOR_WINDOW);

    double b = CColorSpace::GetColorBrightness(m_windowColor);

    if (b + diff > threshold)
    {
        b -= diff;
    }
    else
    {
        b += diff;
        b = std::min<double>(b, 1.0);
    }

    m_stripeColor = DarkMode::IsDarkModeActive() ? DarkMode::SystemColor(COLOR_WINDOWFRAME) :
        CColorSpace::MakeBrightColor(m_windowColor, b);
}

void CWdsListControl::DrawItem(const LPDRAWITEMSTRUCT pdis)
{
    auto* item = GetItem(static_cast<int>(pdis->itemID));
    if (item == nullptr) return;

    auto dc = CDC::Borrow(pdis->hDC);
    auto* pdc = &dc;
    const CRect rcItem(pdis->rcItem);

    CDC dcMem(pdc);
    const CBitmap bm(pdc, rcItem.Width(), rcItem.Height());
    GdiObjectSelection sobm(&dcMem, &bm);

    const COLORREF backColor = GetItemBackgroundColor(static_cast<int>(pdis->itemID));
    dcMem.FillSolidRect(rcItem - rcItem.TopLeft(), backColor);

    // Set defaults for all text drawing
    ScopedBkColor bkColor(&dcMem, backColor);
    GdiObjectSelection sofont(&dcMem, GetFont());

    int focusLeft = 0;
    for (const int i : std::views::iota(0, m_columnCount))
    {
        // The subitem tracks the identifier that maps the column enum
        LVCOLUMN colInfo{ .mask = LVCF_SUBITEM | LVCF_FMT };
        GetColumn(i, &colInfo);
        const int subitem = colInfo.iSubItem;
        if (!IsColumnVisible(subitem)) continue;

        const bool leftAlign = (colInfo.fmt & LVCFMT_RIGHT) == 0;

        const CRect rc = GetWholeSubitemRect(pdis->itemID, i);
        const CRect rcDraw = rc - rcItem.TopLeft();

        if (!item->DrawSubItem(subitem, &dcMem, rcDraw, pdis->itemState, nullptr, &focusLeft))
        {
            item->DrawSelection(this, &dcMem, rcDraw, pdis->itemState);

            CRect rcText = rcDraw;
            rcText.Deflate(TEXT_X_MARGIN, 0);
            const std::wstring s = item->GetText(subitem);

            // Get the correct color in case of compressed or encrypted items
            COLORREF textColor = item->GetItemTextColor();

            // Except if the item is selected - in this case just use standard colors
            COLORREF backColorSub = backColor;
            if (pdis->itemState & ODS_SELECTED && IsFullRowSelection())
            {
                backColorSub = GetItemSelectionBackgroundColor(pdis->itemID);
                textColor = GetItemSelectionTextColor(pdis->itemID);
            }

            // Set the text color
            ScopedTextColor tc(&dcMem, textColor);
            ScopedBkColor backColorObj(&dcMem, backColorSub);

            // Draw the (sub)item text
            DrawTextCache::Get().DrawTextCached(&dcMem, s, rcText, leftAlign);
        }

        if (m_showGrid)
        {
            constexpr COLORREF gridColor = RGB(212, 208, 200);
            constexpr COLORREF gridColorDark = RGB(99, 99, 99);
            CPen pen(PS_SOLID, 1, DarkMode::IsDarkModeActive() ? gridColorDark : gridColor);
            GdiObjectSelection sopen(&dcMem, &pen);

            // Draw top line for first item
            if (pdis->itemID == 0)
            {
                dcMem.MoveTo(rcDraw.left, rcDraw.top);
                dcMem.LineTo(rcDraw.right, rcDraw.top);
            }

            dcMem.MoveTo(rcDraw.right - 1, rcDraw.top);
            dcMem.LineTo(rcDraw.right - 1, rcDraw.bottom);
            dcMem.MoveTo(rcDraw.left, rcDraw.bottom - 1);
            dcMem.LineTo(rcDraw.right, rcDraw.bottom - 1);
        }
    }

    if ((pdis->itemState & ODS_FOCUS) != 0 && HasFocus() && IsFullRowSelection())
    {
        CRect focusRect = rcItem - rcItem.TopLeft();
        focusRect.left = focusLeft - 1;
        DarkMode::DrawFocusRect(&dcMem, focusRect);
    }

    pdc->BitBlt(rcItem.left, rcItem.top,
        rcItem.Width(), rcItem.Height(), &dcMem, 0, 0, SRCCOPY);
}

CRect CWdsListControl::GetWholeSubitemRect(const int item, const int subitem) const
{
    CRect rc;
    if (subitem == 0)
    {
        // Special case column 0:
        // If we did GetSubItemRect(item 0, LVIR_LABEL, rc)
        // and we have an icon list, then we would get the rectangle
        // excluding the icon.
        HDITEM hditem = { .mask = HDI_WIDTH };
        Header().GetItem(0, &hditem);

        [[maybe_unused]] const bool gotItemRect = GetItemRect(item, rc, LVIR_LABEL);
        assert(gotItemRect);
        rc.left = rc.right - hditem.cxy;
    }
    else
    {
        [[maybe_unused]] const bool gotSubItemRect = GetSubItemRect(item, subitem, LVIR_LABEL, rc);
        assert(gotSubItemRect);
    }

    return rc;
}

HFONT CWdsListControl::GetFont() const
{
    if (m_cachedFont == nullptr) m_cachedFont = CWnd::GetFont();
    return m_cachedFont;
}

LRESULT CWdsListControl::OnSetFont(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    m_cachedFont = nullptr;
    const LRESULT result = CallDefaultHandler();
    m_cachedFont = nullptr;
    DrawTextCache::Get().ClearCache();
    CalculateRowHeight();
    SetRowHeight(m_rowHeight);
    Invalidate(false);
    return result;
}

void CWdsListControl::OnSettingChange(const UINT uFlags, const LPCTSTR lpszSection)
{
    m_cachedFont = nullptr;
    CListCtrl::OnSettingChange(uFlags, lpszSection);
    m_cachedFont = nullptr;
    DrawTextCache::Get().ClearCache();
    CalculateRowHeight();
    Invalidate(false);
}

int CWdsListControl::GetSubItemWidth(CWdsListItem* item, const int subitem, CDC* pDC)
{
    if (pDC == nullptr)
    {
        CClientDC dc(this);
        GdiObjectSelection sofont(&dc, GetFont());
        return GetSubItemWidth(item, subitem, &dc);
    }

    const CRect rc(0, 0, 3500, 20);

    int width;
    int dummy = rc.left;
    if (item->DrawSubItem(subitem, pDC, rc, 0, &width, &dummy))
    {
        return width;
    }

    const std::wstring s = item->GetText(subitem);
    if (s.empty())
    {
        return 0;
    }

    return TEXT_X_MARGIN + pDC->GetTextExtent(s.c_str(), static_cast<int>(s.size())).cx;
}

/////////////////////////////////////////////////////////////////////////////
// Sorting functionality (merged from CSortingListControl)

void CWdsListControl::LoadPersistentAttributes()
{
    // Fetch casted column count to avoid signed comparison warnings
    const auto columnCount = static_cast<size_t>(m_columnCount);

    // Load default column order values from resource
    if (m_columnOrder->size() != columnCount)
    {
        m_columnOrder->resize(columnCount);
        GetColumnOrder(*m_columnOrder);
    }

    // Load default column width values from resource
    if (m_columnWidths->size() != columnCount)
    {
        m_columnWidths->resize(columnCount, 0);
        for (const int i : std::views::iota(0, static_cast<int>(m_columnWidths->size())))
        {
            (*m_columnWidths)[i] = GetColumnWidth(i);
        }
    }

    // Set based on persisted values
    SetColumnOrder(*m_columnOrder);
    for (const int i : std::views::iota(0, static_cast<int>(m_columnWidths->size())))
    {
        SetColumnWidth(i, std::min((*m_columnWidths)[i], (*m_columnWidths)[i] * 2));
    }
}

void CWdsListControl::SavePersistentAttributes() const
{
    GetColumnOrder(*m_columnOrder);
    for (const int i : std::views::iota(0, static_cast<int>(m_columnWidths->size())))
    {
        if (IsColumnVisible(ColumnToSubItem(i)))
        {
            (*m_columnWidths)[i] = GetColumnWidth(i);
        }
    }
}

int CWdsListControl::ColumnToSubItem(const int col) const
{
    LVCOLUMN column_info{ .mask = LVCF_SUBITEM };
    GetColumn(col, &column_info);
    return column_info.iSubItem;
}

int CWdsListControl::SubItemToColumn(const int subitem) const
{
    for (const int column : std::views::iota(0, m_columnCount))
    {
        if (ColumnToSubItem(column) == subitem) return column;
    }
    return -1;
}

bool CWdsListControl::IsColumnVisible(const int subitem) const
{
    return COptions::IsColumnVisible(*m_columnVisibility, subitem);
}

bool CWdsListControl::IsColumnRequired(const int subitem) const
{
    return std::ranges::find(m_requiredColumns, subitem) != m_requiredColumns.end();
}

void CWdsListControl::ApplyColumnVisibility(const int column)
{
    const int subitem = ColumnToSubItem(column);
    const bool visible = IsColumnRequired(subitem) || IsColumnVisible(subitem);
    LVCOLUMN columnInfo{ .mask = LVCF_FMT };
    const bool hasColumnInfo = GetColumn(column, &columnInfo);
    if (hasColumnInfo)
    {
        columnInfo.fmt &= ~LVCFMT_FIXED_WIDTH;
        SetColumn(column, &columnInfo);
    }

    const int persistedWidth = column < static_cast<int>(m_columnWidths->size()) ? (*m_columnWidths)[column] : 0;
    const int width = visible ? (persistedWidth > 0 ? persistedWidth : m_defaultColumnWidths[column]) : 0;
    SetColumnWidth(column, width);

    if (!visible && hasColumnInfo)
    {
        columnInfo.fmt |= LVCFMT_FIXED_WIDTH;
        SetColumn(column, &columnInfo);
    }
}

void CWdsListControl::SetColumnVisible(const int subitem, const bool visible)
{
    const int column = SubItemToColumn(subitem);
    if (column < 0 || (!visible && IsColumnRequired(subitem)) || visible == IsColumnVisible(subitem)) return;

    if (!visible && column < static_cast<int>(m_columnWidths->size()))
    {
        (*m_columnWidths)[column] = GetColumnWidth(column);
    }

    COptions::SetColumnVisible(*m_columnVisibility, subitem, visible);
    ApplyColumnVisibility(column);

    bool sortingChanged = false;
    if (!visible && m_sorting.column1 == column)
    {
        const bool ascending = GetAscendingDefault(ColumnToSubItem(0));
        SetSorting(0, ascending, 0, ascending);
        sortingChanged = true;
    }
    else if (!visible && m_sorting.column2 == column)
    {
        SetSorting(m_sorting.column1, m_sorting.ascending1);
        sortingChanged = true;
    }

    if (sortingChanged) SortItems();
    else Invalidate();
}

void CWdsListControl::SetSorting(const int sortColumn1, const bool ascending1, const int sortColumn2, const bool ascending2)
{
    m_sorting = {
        .column1 = sortColumn1,
        .column2 = sortColumn2,
        .subitem1 = ColumnToSubItem(sortColumn1),
        .subitem2 = ColumnToSubItem(sortColumn2),
        .ascending1 = ascending1,
        .ascending2 = ascending2
    };
}

void CWdsListControl::SetSorting(const int sortColumn, const bool ascending)
{
    m_sorting.column2 = m_sorting.column1;
    m_sorting.subitem2 = m_sorting.subitem1;
    m_sorting.ascending2 = m_sorting.ascending1;
    m_sorting.column1 = sortColumn;
    m_sorting.ascending1 = ascending;
    m_sorting.subitem1 = ColumnToSubItem(sortColumn);
}

void CWdsListControl::InsertListItem(const int i, std::span<CWdsListItem* const> items)
{
    if (items.empty()) return;

    assert(i >= 0 && i <= GetItemCount());

    SelectionPreserver preserve(this);

    m_items.insert(m_items.begin() + i, items.begin(), items.end());
    const int itemCount = static_cast<int>(m_items.size());

    for (const int x : std::views::iota(i, itemCount))
    {
        m_itemMap[m_items[x]] = x;
    }

    SetItemCountEx(itemCount, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    RedrawItems(i, itemCount - 1);
}

/*
 * Sorts the list control's items and updates the header to display the correct sorting indicator.
 * This method reorders the list control's items based on the current sorting column and direction.
 * It then updates the header control by using native Windows header flags (HDF_SORTUP and HDF_SORTDOWN)
 * to display a platform-consistent sorting arrow.
 */
void CWdsListControl::SortItems()
{
    SelectionPreserver preserve(this);

    std::ranges::stable_sort(m_items, [this](const CWdsListItem* item1, const CWdsListItem* item2)
    {
        return item1->CompareSort(item2, m_sorting) < 0;
    });

    for (const int i : std::views::iota(0, GetItemCount()))
    {
        m_itemMap[m_items[i]] = i;
    }

    Invalidate();

    CHeaderCtrl& header = Header();
    HDITEM hditem{ .mask = HDI_FORMAT };

    // Remove the sort indicator from the previously sorted column if one exists.
    if (m_indicatedColumn != -1)
    {
        header.GetItem(m_indicatedColumn, &hditem);
        // Use a bitwise operation to clear both the UP and DOWN sort flags.
        hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        header.SetItem(m_indicatedColumn, &hditem);
    }

    // Retrieve the newly sorted column's current format flags.
    header.GetItem(m_sorting.column1, &hditem);
    // Clear any existing sort flags to ensure a clean state before applying the new one.
    hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

    // Apply the correct native sorting indicator based on the sort direction.
    hditem.fmt |= m_sorting.ascending1 ? HDF_SORTUP : HDF_SORTDOWN;

    header.SetItem(m_sorting.column1, &hditem);

    // Store the current sorted column's index to be cleared next time.
    m_indicatedColumn = m_sorting.column1;
}

void CWdsListControl::PostSelectionChanged()
{
    // Only post if there isn't already a pending message
    if (!m_selectionChangePending)
    {
        m_selectionChangePending = true;
        PostMessage(WM_SELECTION_CHANGED, 0, 0);
    }
}

void CWdsListControl::DeselectAll()
{
    SetItemState(-1, 0, LVIS_SELECTED);
}

/////////////////////////////////////////////////////////////////////////////
// Message Map

void CWdsListControl::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    if (point != CPoint(-1, -1))
    {
        const CRect headerRect(Header().Handle());
        if (headerRect.Contains(point))
        {
            ShowColumnContextMenu(point);
            return;
        }
    }

    OnItemContextMenu(point);
}

void CWdsListControl::ShowColumnContextMenu(const CPoint point)
{
    CMenu menu = CMenu::CreatePopup();
    if (!menu) return;

    for (const int column : std::views::iota(0, m_columnCount))
    {
        std::array<wchar_t, 256> text{};
        LVCOLUMN item{
            .mask = LVCF_TEXT,
            .pszText = text.data(),
            .cchTextMax = static_cast<int>(text.size())
        };
        if (!GetColumn(column, &item)) continue;

        const int subitem = ColumnToSubItem(column);
        const bool required = IsColumnRequired(subitem);
        const UINT flags = MF_STRING |
            (required ? MF_GRAYED : MF_ENABLED) |
            (IsColumnVisible(subitem) ? MF_CHECKED : MF_UNCHECKED);
        menu.Append(flags, static_cast<UINT>(column + 1), text.data());
    }

    const UINT command = menu.ShowPopup(
        TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x, point.y, this);
    if (command > 0 && command <= static_cast<UINT>(m_columnCount))
    {
        const int subitem = ColumnToSubItem(command - 1);
        SetColumnVisible(subitem, !IsColumnVisible(subitem));
    }
}

void CWdsListControl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) const
{
    // Check if this is a notification from the header control
    *pResult = CDRF_DODEFAULT;
    if (!DarkMode::IsDarkModeActive())
    {
        return;
    }

    // Handle custom text color for headers in dark mode
    const NMCUSTOMDRAW* pCustomDraw = reinterpret_cast<NMCUSTOMDRAW*>(pNMHDR);
    if (pCustomDraw->dwDrawStage == CDDS_PREPAINT)
    {
        *pResult = CDRF_NOTIFYITEMDRAW;
    }
    else if (pCustomDraw->dwDrawStage == CDDS_ITEMPREPAINT && pNMHDR->hwndFrom == Header().Handle())
    {
        ::SetTextColor(pCustomDraw->hdc, DarkMode::SystemColor(COLOR_BTNTEXT));
    }
}

bool CWdsListControl::OnEraseBkgnd(CDC* pDC) const
{
    // Fetch coordinate of the last item
    CRect lastRect(0, 0, 0, 0);
    if (const int itemCount = GetItemCount(); itemCount > 0)
    {
        GetItemRect(itemCount - 1, &lastRect, LVIR_BOUNDS);
    }

    // Erase unused area to the right of all items
    const CRect rectClient = ClientRect();
    if (lastRect.right < rectClient.right)
    {
        pDC->FillSolidRect(lastRect.right, 0, rectClient.right - lastRect.right,
            lastRect.bottom, DarkMode::SystemColor(COLOR_WINDOW));
    }

    // Erase unused area at the bottom of the last item
    if (lastRect.bottom < rectClient.bottom)
    {
        pDC->FillSolidRect(0, lastRect.bottom, rectClient.right,
            rectClient.bottom - lastRect.bottom, DarkMode::SystemColor(COLOR_WINDOW));
    }

    return true;
}

void CWdsListControl::OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const int column = reinterpret_cast<LPNMHEADER>(pNMHDR)->iItem;
    const int subitem = ColumnToSubItem(column);

    // fetch size of rendered column header text
    // temporarily insert a false column to the finalize column does
    // not autosize to fit the whole control width
    const ScopedRedrawPause lock(this);
    const int falseColumn = InsertColumn(m_columnCount + 1, L"");
    SetColumnWidth(column, LVSCW_AUTOSIZE_USEHEADER);
    int width = GetColumnWidth(column);
    DeleteColumn(falseColumn);

    CClientDC dc(this);
    GdiObjectSelection sofont(&dc, GetFont());

    // fetch size of sub-elements
    for (const int i : std::views::iota(0, GetItemCount()))
    {
        width = std::max(width, GetSubItemWidth(GetItem(i), subitem, &dc));
    }

    // update final column width
    constexpr int padding = 3;
    SetColumnWidth(column, width + padding);
    *pResult = false;
}

void CWdsListControl::OnHdnItemchanging(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CallDefaultHandler();
    InvalidateRect(nullptr);

    *pResult = false;
}

void CWdsListControl::OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult) const
{
    auto* displayInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult = false;

    auto* item = GetItem(displayInfo->item.iItem);
    if (item == nullptr) return;

    if (displayInfo->item.mask & LVIF_PARAM)
    {
        displayInfo->item.lParam = reinterpret_cast<LPARAM>(item);
    }

    if ((displayInfo->item.mask & LVIF_TEXT) != 0 && displayInfo->item.cchTextMax > 0)
    {
        // The passed subitem value is actually the column id so translate it
        const int subitem = ColumnToSubItem(displayInfo->item.iSubItem);

        // Copy maximum allowed to the provided buffer
        wcsncpy_s(displayInfo->item.pszText, displayInfo->item.cchTextMax,
            item->GetText(subitem).c_str(), _TRUNCATE);
    }
}

void CWdsListControl::OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    *pResult = false;

    if (const int col = phdr->iItem; col == m_sorting.column1)
    {
        m_sorting.ascending1 = !m_sorting.ascending1;
    }
    else
    {
        SetSorting(col, GetAscendingDefault(ColumnToSubItem(col)));
    }

    SortItems();
}

void CWdsListControl::OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnHdnItemClick(pNMHDR, pResult);
}

void CWdsListControl::OnDestroy()
{
    SavePersistentAttributes();
    CListCtrl::OnDestroy();
}

LRESULT CWdsListControl::OnSelectionChanged(const WPARAM wParam, const LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_selectionChangePending = false;
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SELECTION_REFRESH);

    return 0;
}

void CWdsListControl::RemoveListItem(const int i, const int c)
{
    if (c <= 0) return;

    int itemCount = GetItemCount();
    assert(i >= 0 && i < itemCount);
    assert(i + c <= itemCount);

    std::vector<std::unique_ptr<CWdsListItem>> removedItems;
    if (m_ownsItems)
    {
        removedItems.reserve(c);
    }

    SelectionPreserver preserve(this);

    for (const int x : std::views::iota(i, i + c))
    {
        CWdsListItem* item = m_items[x];
        m_itemMap.erase(item);
        if (m_ownsItems)
        {
            removedItems.emplace_back(item);
        }
    }

    m_items.erase(m_items.begin() + i, m_items.begin() + i + c);

    itemCount -= c;
    for (const int x : std::views::iota(i, itemCount))
    {
        m_itemMap[m_items[x]] = x;
    }

    SetItemCountEx(itemCount, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    if (i < itemCount)
    {
        RedrawItems(i, itemCount - 1);
    }
    else
    {
        Invalidate();
    }
}

bool CWdsListControl::DeleteItem(const int i)
{
    RemoveListItem(i);
    return true;
}

bool CWdsListControl::DeleteAllItems()
{
    if (m_ownsItems) for (const auto* item : m_items) delete item;
    m_items.clear();
    m_itemMap.clear();
    SetItemCountEx(0, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    return true;
}
