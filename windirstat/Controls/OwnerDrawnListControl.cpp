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
#include "OwnerDrawnListControl.h"
#include "DrawTextCache.h"

namespace
{
    constexpr UINT TEXT_X_MARGIN = 6u; // Horizontal distance of the text from the edge of the item rectangle
    constexpr UINT LABEL_INFLATE_CX = 3u; // How much the label is enlarged, to get the selection and focus rectangle
    constexpr UINT LABEL_Y_MARGIN = 2u;
    constexpr UINT GENERAL_INDENT = 5u;
}

/////////////////////////////////////////////////////////////////////////////
// COwnerDrawnListItem

int COwnerDrawnListItem::CompareSort(const COwnerDrawnListItem* other, const SSorting& sorting) const
{
    int r = Compare(other, sorting.subitem1);
    if (abs(r) < 2 && !sorting.ascending1)
    {
        r = -r;
    }

    if (r == 0 && sorting.subitem1 != sorting.subitem2)
    {
        r = Compare(other, sorting.subitem2);
        if (abs(r) < 2 && !sorting.ascending2)
        {
            r = -r;
        }
    }
    return r;
}

// Draws an item label (icon, text) in all parts of the WinDirStat view
// the rest is drawn by DrawItem()
void COwnerDrawnListItem::DrawLabel(const COwnerDrawnListControl* list, CDC* pdc, CRect& rc, const UINT state, int* width, int* focusLeft, const bool indent)
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

    CSelectObject sofont(pdc, list->GetFont());

    rcRest.DeflateRect(list->GetTextXMargin(), 0);

    CRect rcLabel = rcRest;
    DrawTextCache::Get().DrawTextCached(pdc, GetText(0), rcLabel, true, true);

    rcLabel.InflateRect(LABEL_INFLATE_CX, 0);
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
    CSetTextColor stc(pdc, textColor);

    if (width == nullptr)
    {
        // Draw the actual text
        DrawTextCache::Get().DrawTextCached(pdc, GetText(0), rcRest);
    }

    rcLabel.InflateRect(1, 1);

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

void COwnerDrawnListItem::DrawSelection(const COwnerDrawnListControl* list, CDC* pdc, CRect rc, const UINT state) const
{
    if (!list->IsFullRowSelection())
    {
        return;
    }
    if ((state & ODS_SELECTED) == 0)
    {
        return;
    }

    rc.DeflateRect(0, LABEL_Y_MARGIN);
    pdc->FillSolidRect(rc, list->GetHighlightColor());
}

void COwnerDrawnListItem::DrawPercentage(CDC* pdc, const CRect rc, const double fraction, const COLORREF color) const
{
    COLORREF dark = RGB(118, 118, 118); // Light edge
    COLORREF light = RGB(198, 198, 198); // Dark edge
    COLORREF bg = RGB(225, 225, 225); // Background
    if (DarkMode::IsDarkModeActive())
    {
        dark = RGB(60, 60, 60); // Light edge
        light = RGB(50, 50, 50); // Dark edge
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
    rcLeft.DeflateRect(1, 1);
    if (rcLeft.right > rcLeft.left)
    {
        pdc->FillSolidRect(rcLeft, color);
    }

    if (rcRight.right > rcRight.left)
    {
        pdc->Draw3dRect(rcRight, light, light);
    }
    rcRight.DeflateRect(1, 1);
    if (rcRight.right > rcRight.left)
    {
        pdc->FillSolidRect(rcRight, bg);
    }
}

/////////////////////////////////////////////////////////////////////////////
// COwnerDrawnListControl

IMPLEMENT_DYNAMIC(COwnerDrawnListControl, CListCtrl)

COwnerDrawnListControl::COwnerDrawnListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths)
    : m_columnOrder(columnOrder)
    , m_columnWidths(columnWidths)
{
    InitializeColors();
}

// This method MUST be called before the Control is shown.
void COwnerDrawnListControl::OnColumnsInserted()
{
    // The pacman shall not draw over our header control.
    ModifyStyle(0, WS_CLIPCHILDREN);
    LoadPersistentAttributes();

    // Calculate row height now that window is created
    CalculateRowHeight();

    // Force the list control to register a new row height
    // This is necessary for controls embedded in a dialog resource.
    CImageList imageList;
    imageList.Create(1, m_rowHeight, ILC_COLOR, 1, 1);
    SetImageList(&imageList, LVSIL_SMALL);
    SetImageList(nullptr, LVSIL_SMALL);
}

void COwnerDrawnListControl::SysColorChanged()
{
    InitializeColors();
    CalculateRowHeight();
}

int COwnerDrawnListControl::GetRowHeight() const
{
    return m_rowHeight;
}

void COwnerDrawnListControl::CalculateRowHeight()
{
    // Create a device context to get font metrics
    if (!IsWindow(m_hWnd)) return;
    CClientDC dc(this);
    CSelectObject sofont(&dc, GetFont());

    if (TEXTMETRIC tm; dc.GetTextMetrics(&tm))
    {
        // Row height = font height + padding
        // Make sure it's odd number for dotted connector mating
        m_rowHeight = (tm.tmHeight + (LABEL_Y_MARGIN * 2) + 1) | 1;
    }
}

void COwnerDrawnListControl::ShowGrid(const bool show)
{
    m_showGrid = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowStripes(const bool show)
{
    m_showStripes = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowFullRowSelection(const bool show)
{
    m_showFullRowSelect = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

bool COwnerDrawnListControl::IsFullRowSelection() const
{
    return m_showFullRowSelect;
}

// Normal window background color
COLORREF COwnerDrawnListControl::GetWindowColor() const
{
    return m_windowColor;
}

// Shaded window background color (for stripes)
COLORREF COwnerDrawnListControl::GetStripeColor() const
{
    return m_stripeColor;
}

// Highlight color if we have no focus
COLORREF COwnerDrawnListControl::GetNonFocusHighlightColor() const
{
    return DarkMode::IsDarkModeActive() ? RGB(90, 90, 90) : RGB(190, 190, 190);
}

// Highlight text color if we have no focus
COLORREF COwnerDrawnListControl::GetNonFocusHighlightTextColor() const
{
    return DarkMode::IsDarkModeActive() ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

COLORREF COwnerDrawnListControl::GetHighlightColor() const
{
    if (HasFocus())
    {
        return DarkMode::WdsSysColor(COLOR_HIGHLIGHT);
    }

    return GetNonFocusHighlightColor();
}

COLORREF COwnerDrawnListControl::GetHighlightTextColor() const
{
    if (HasFocus())
    {
        return DarkMode::WdsSysColor(COLOR_HIGHLIGHTTEXT);
    }

    return GetNonFocusHighlightTextColor();
}

bool COwnerDrawnListControl::IsItemStripColor(const int i) const
{
    return m_showStripes && i % 2 != 0;
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(const int i) const
{
    return IsItemStripColor(i) ? GetStripeColor() : GetWindowColor();
}

COLORREF COwnerDrawnListControl::GetItemSelectionBackgroundColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection())
    {
        return GetHighlightColor();
    }

    return GetItemBackgroundColor(i);
}

COLORREF COwnerDrawnListControl::GetItemSelectionTextColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection())
    {
        return GetHighlightTextColor();
    }

    return DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
}

int COwnerDrawnListControl::GetTextXMargin() const
{
    return TEXT_X_MARGIN;
}

int COwnerDrawnListControl::GetGeneralLeftIndent() const
{
    return GENERAL_INDENT;
}

COwnerDrawnListItem* COwnerDrawnListControl::GetItem(const int i) const
{
    const auto item = std::bit_cast<COwnerDrawnListItem*>(GetItemData(i));
    return item;
}

int COwnerDrawnListControl::FindListItem(const COwnerDrawnListItem* item) const
{
    LVFINDINFO fi{ .flags = LVFI_PARAM, .lParam = reinterpret_cast<LPARAM>(item) };
    return FindItem(&fi);
}

void COwnerDrawnListControl::InitializeColors()
{
    // I try to find a good contrast to COLOR_WINDOW (usually white or light grey).
    // This is a result of experiments.

    constexpr double diff = 0.07; // Try to alter the brightness by diff.
    constexpr double threshold = 1.04; // If result would be brighter, make color darker.

    m_windowColor = DarkMode::WdsSysColor(COLOR_WINDOW);

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

    m_stripeColor = DarkMode::IsDarkModeActive() ? DarkMode::WdsSysColor(COLOR_WINDOWFRAME) :
        CColorSpace::MakeBrightColor(m_windowColor, b);
}

void COwnerDrawnListControl::DrawItem(LPDRAWITEMSTRUCT pdis)
{
    auto* item = std::bit_cast<COwnerDrawnListItem*>(pdis->itemData);
    auto* pdc = CDC::FromHandle(pdis->hDC);
    CRect rcItem(pdis->rcItem);

    CDC dcMem;
    dcMem.CreateCompatibleDC(pdc);

    CBitmap bm;
    bm.CreateCompatibleBitmap(pdc, rcItem.Width(), rcItem.Height());
    CSelectObject sobm(&dcMem, &bm);

    const COLORREF backColor = GetItemBackgroundColor(static_cast<int>(pdis->itemID));
    dcMem.FillSolidRect(rcItem - rcItem.TopLeft(), backColor);

    // Set defaults for all text drawing
    CSetBkColor bkColor(&dcMem, backColor);
    CSelectObject sofont(&dcMem, GetFont());

    int focusLeft = 0;
    for (const int i : std::views::iota(0, GetHeaderCtrl()->GetItemCount()))
    {
        // The subitem tracks the identifier that maps the column enum
        LVCOLUMN colInfo{ .mask = LVCF_SUBITEM | LVCF_FMT };
        GetColumn(i, &colInfo);
        const int subitem = colInfo.iSubItem;
        const bool leftAlign = (colInfo.fmt & LVCFMT_RIGHT) == 0;

        const CRect rc = GetWholeSubitemRect(pdis->itemID, i);
        const CRect rcDraw = rc - rcItem.TopLeft();

        if (!item->DrawSubItem(subitem, &dcMem, rcDraw, pdis->itemState, nullptr, &focusLeft))
        {
            item->DrawSelection(this, &dcMem, rcDraw, pdis->itemState);

            CRect rcText = rcDraw;
            rcText.DeflateRect(TEXT_X_MARGIN, 0);
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
            CSetTextColor tc(&dcMem, textColor);
            CSetBkColor backColorObj(&dcMem, backColorSub);

            // Draw the (sub)item text
            DrawTextCache::Get().DrawTextCached(&dcMem, s, rcText, leftAlign);
        }

        if (m_showGrid)
        {
            constexpr COLORREF gridColor = RGB(212, 208, 200);
            constexpr COLORREF gridColorDark = RGB(99, 99, 99);
            CPen pen(PS_SOLID, 1, DarkMode::IsDarkModeActive() ? gridColorDark : gridColor);
            CSelectObject sopen(&dcMem, &pen);

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

CRect COwnerDrawnListControl::GetWholeSubitemRect(const int item, const int subitem) const
{
    CRect rc;
    if (subitem == 0)
    {
        // Special case column 0:
        // If we did GetSubItemRect(item 0, LVIR_LABEL, rc)
        // and we have an icon list, then we would get the rectangle
        // excluding the icon.
        HDITEM hditem = { .mask = HDI_WIDTH };
        GetHeaderCtrl()->GetItem(0, &hditem);

        VERIFY(GetItemRect(item, rc, LVIR_LABEL));
        rc.left = rc.right - hditem.cxy;
    }
    else
    {
        VERIFY(GetSubItemRect(item, subitem, LVIR_LABEL, rc));
    }

    return rc;
}

bool COwnerDrawnListControl::HasFocus() const
{
    return ::GetFocus() == m_hWnd;
}

int COwnerDrawnListControl::GetSubItemWidth(COwnerDrawnListItem* item, const int subitem)
{
    CClientDC dc(this);
    const CRect rc(0, 0, 3500, 20);

    int width;
    int dummy = rc.left;
    if (item->DrawSubItem(subitem, &dc, rc, 0, &width, &dummy))
    {
        return width;
    }

    const std::wstring s = item->GetText(subitem);
    if (s.empty())
    {
        return 0;
    }

    CSelectObject sofont(&dc, GetFont());

    SIZE size;
    GetTextExtentPoint32(dc, s.c_str(), static_cast<int>(s.size()), &size);
    return TEXT_X_MARGIN + size.cx;
}

/////////////////////////////////////////////////////////////////////////////
// Sorting functionality (merged from CSortingListControl)

void COwnerDrawnListControl::LoadPersistentAttributes()
{
    // Fetch casted column count to avoid signed comparison warnings
    const auto columnCount = static_cast<size_t>(GetHeaderCtrl()->GetItemCount());

    // Load default column order values from resource
    if (m_columnOrder->size() != columnCount)
    {
        m_columnOrder->resize(columnCount);
        GetColumnOrderArray(m_columnOrder->data(), static_cast<int>(m_columnOrder->size()));
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
    SetColumnOrderArray(static_cast<int>(m_columnOrder->size()), m_columnOrder->data());
    for (const int i : std::views::iota(0, static_cast<int>(m_columnWidths->size())))
    {
        SetColumnWidth(i, min((*m_columnWidths)[i], (*m_columnWidths)[i] * 2));
    }
}

void COwnerDrawnListControl::SavePersistentAttributes() const
{
    GetColumnOrderArray(m_columnOrder->data(), static_cast<int>(m_columnOrder->size()));
    for (const int i : std::views::iota(0, static_cast<int>(m_columnWidths->size())))
    {
        (*m_columnWidths)[i] = GetColumnWidth(i);
    }
}

void COwnerDrawnListControl::AddExtendedStyle(const DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() | exStyle);
}

void COwnerDrawnListControl::RemoveExtendedStyle(const DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() & ~exStyle);
}

const SSorting& COwnerDrawnListControl::GetSorting() const
{
    return m_sorting;
}

int COwnerDrawnListControl::ColumnToSubItem(const int col) const
{
    LVCOLUMN column_info{ .mask = LVCF_SUBITEM };
    GetColumn(col, &column_info);
    return column_info.iSubItem;
}

void COwnerDrawnListControl::SetSorting(const SSorting& sorting)
{
    m_sorting = sorting;
}

void COwnerDrawnListControl::SetSorting(const int sortColumn1, const bool ascending1, const int sortColumn2, const bool ascending2)
{
    m_sorting.column1    = sortColumn1;
    m_sorting.subitem1   = ColumnToSubItem(sortColumn1);
    m_sorting.ascending1 = ascending1;
    m_sorting.column2    = sortColumn2;
    m_sorting.subitem2   = ColumnToSubItem(sortColumn2);
    m_sorting.ascending2 = ascending2;
}

void COwnerDrawnListControl::SetSorting(const int sortColumn, const bool ascending)
{
    m_sorting.column2    = m_sorting.column1;
    m_sorting.subitem2   = m_sorting.subitem1;
    m_sorting.ascending2 = m_sorting.ascending1;
    m_sorting.column1    = sortColumn;
    m_sorting.ascending1 = ascending;
    m_sorting.subitem1   = ColumnToSubItem(sortColumn);
}

void COwnerDrawnListControl::InsertListItem(const int i, COwnerDrawnListItem* item)
{
    LVITEM lvitem;
    lvitem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    lvitem.iItem = i;
    lvitem.pszText = LPSTR_TEXTCALLBACK;
    lvitem.iImage = I_IMAGECALLBACK;
    lvitem.lParam = reinterpret_cast<LPARAM>(item);
    lvitem.stateMask = 0;
    lvitem.iSubItem = 0;
    VERIFY(i == CListCtrl::InsertItem(&lvitem));
}

/*
 * Sorts the list control's items and updates the header to display the correct sorting indicator.
 * This method reorders the list control's items based on the current sorting column and direction.
 * It then updates the header control by using native Windows header flags (HDF_SORTUP and HDF_SORTDOWN)
 * to display a platform-consistent sorting arrow.
 */
void COwnerDrawnListControl::SortItems()
{
    // Reorder the list items based on the current sorting criteria using a lambda comparison function.
    CListCtrl::SortItems([](LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
        const auto* item1 = std::bit_cast<COwnerDrawnListItem*>(lParam1);
        const auto* item2 = std::bit_cast<COwnerDrawnListItem*>(lParam2);
        const auto* sorting = std::bit_cast<SSorting*>(lParamSort);
        return item1->CompareSort(item2, *sorting); }, reinterpret_cast<DWORD_PTR>(&m_sorting));

    auto* pHeaderCtrl = GetHeaderCtrl();

    // Exit if the header control is unavailable, to prevent a null pointer crash.
    if (pHeaderCtrl == nullptr)
    {
        return;
    }

    HDITEM hditem;
    hditem.mask = HDI_FORMAT;

    // Remove the sort indicator from the previously sorted column if one exists.
    if (m_indicatedColumn != -1)
    {
        pHeaderCtrl->GetItem(m_indicatedColumn, &hditem);
        // Use a bitwise operation to clear both the UP and DOWN sort flags.
        hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        pHeaderCtrl->SetItem(m_indicatedColumn, &hditem);
    }

    // Retrieve the newly sorted column's current format flags.
    pHeaderCtrl->GetItem(m_sorting.column1, &hditem);
    // Clear any existing sort flags to ensure a clean state before applying the new one.
    hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

    // Apply the correct native sorting indicator based on the sort direction.
    hditem.fmt |= m_sorting.ascending1 ? HDF_SORTUP : HDF_SORTDOWN;

    pHeaderCtrl->SetItem(m_sorting.column1, &hditem);

    // Store the current sorted column's index to be cleared next time.
    m_indicatedColumn = m_sorting.column1;
}

bool COwnerDrawnListControl::GetAscendingDefault(int /*column*/)
{
    return true;
}

void COwnerDrawnListControl::PostSelectionChanged()
{
    // Only post if there isn't already a pending message
    if (!m_selectionChangePending)
    {
        m_selectionChangePending = true;
        PostMessage(WM_SELECTION_CHANGED, 0, 0);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Message Map

BEGIN_MESSAGE_MAP(COwnerDrawnListControl, CListCtrl)
    ON_WM_ERASEBKGND()
    ON_NOTIFY(HDN_DIVIDERDBLCLICK, 0, OnHdnDividerdblclick)
    ON_NOTIFY(HDN_ITEMCHANGING, 0, OnHdnItemchanging)
    ON_WM_SHOWWINDOW()
    ON_NOTIFY(NM_CUSTOMDRAW, 0, OnCustomDraw)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
    ON_NOTIFY(HDN_ITEMCLICK, 0, OnHdnItemClick)
    ON_NOTIFY(HDN_ITEMDBLCLICK, 0, OnHdnItemDblClick)
    ON_WM_DESTROY()
    ON_MESSAGE(WM_SELECTION_CHANGED, OnSelectionChanged)
END_MESSAGE_MAP()

void COwnerDrawnListControl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Check if this is a notification from the header control
    *pResult = CDRF_DODEFAULT;
    if (!DarkMode::IsDarkModeActive())
    {
        return;
    }

    // Handle custom text color for headers in dark mode
    NMCUSTOMDRAW* pCustomDraw = reinterpret_cast<NMCUSTOMDRAW*>(pNMHDR);
    if (pCustomDraw->dwDrawStage == CDDS_PREPAINT)
    {
        *pResult = CDRF_NOTIFYITEMDRAW;
    }
    else if (pCustomDraw->dwDrawStage == CDDS_ITEMPREPAINT)
    {
        ::SetTextColor(pCustomDraw->hdc, DarkMode::WdsSysColor(COLOR_BTNTEXT));
    }
}

BOOL COwnerDrawnListControl::OnEraseBkgnd(CDC* pDC)
{
    // Fetch coordinate of the last item
    CRect lastRect(0, 0, 0, 0);
    if (const int itemCount = GetItemCount(); itemCount > 0)
    {
        GetItemRect(itemCount - 1, &lastRect, LVIR_BOUNDS);
    }

    // Erase unused area to the right of all items
    CRect rectClient;
    GetClientRect(&rectClient);
    if (lastRect.right < rectClient.right)
    {
        pDC->FillSolidRect(lastRect.right, 0, rectClient.right - lastRect.right,
            lastRect.bottom, DarkMode::WdsSysColor(COLOR_WINDOW));
    }

    // Erase unused area at the bottom of the last item
    if (lastRect.bottom < rectClient.bottom)
    {
        pDC->FillSolidRect(0, lastRect.bottom, rectClient.right,
            rectClient.bottom - lastRect.bottom, DarkMode::WdsSysColor(COLOR_WINDOW));
    }

    return TRUE;
}

void COwnerDrawnListControl::OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const int column = reinterpret_cast<LPNMHEADER>(pNMHDR)->iItem;
    const int subitem = ColumnToSubItem(column);

    // fetch size of rendered column header text
    // temporarily insert a false column to the finalize column does
    // not autosize to fit the whole control width
    SetRedraw(FALSE);
    const int falseColumn = InsertColumn(GetHeaderCtrl()->GetItemCount() + 1, L"");
    SetColumnWidth(column, LVSCW_AUTOSIZE_USEHEADER);
    int width = GetColumnWidth(column);
    DeleteColumn(falseColumn);

    // fetch size of sub-elements
    for (const int i : std::views::iota(0, GetItemCount()))
    {
        width = max(width, GetSubItemWidth(GetItem(i), subitem));
    }

    // update final column width
    constexpr int padding = 3;
    SetColumnWidth(column, width + padding);

    SetRedraw(TRUE);
    *pResult = FALSE;
}

void COwnerDrawnListControl::OnHdnItemchanging(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    Default();
    InvalidateRect(nullptr);

    *pResult = FALSE;
}

void COwnerDrawnListControl::OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* displayInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult = FALSE;

    const auto* item = std::bit_cast<COwnerDrawnListItem*>(displayInfo->item.lParam);

    if ((displayInfo->item.mask & LVIF_TEXT) != 0)
    {
        // The passed subitem value is actually the column id so translate it
        const int subitem = ColumnToSubItem(displayInfo->item.iSubItem);

        // Copy maximum allowed to the provided buffer
        wcsncpy_s(displayInfo->item.pszText, displayInfo->item.cchTextMax,
            item->GetText(subitem).c_str(), displayInfo->item.cchTextMax - 1);
    }
}

void COwnerDrawnListControl::OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    *pResult = FALSE;
    const int col = phdr->iItem;

    if (col == m_sorting.column1)
    {
        m_sorting.ascending1 = !m_sorting.ascending1;
    }
    else
    {
        SetSorting(col, GetAscendingDefault(ColumnToSubItem(col)));
    }

    SortItems();
}

void COwnerDrawnListControl::OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnHdnItemClick(pNMHDR, pResult);
}

void COwnerDrawnListControl::OnDestroy()
{
    SavePersistentAttributes();
    CListCtrl::OnDestroy();
}

LRESULT COwnerDrawnListControl::OnSelectionChanged(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_selectionChangePending = false;
    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);

    return 0;
}
