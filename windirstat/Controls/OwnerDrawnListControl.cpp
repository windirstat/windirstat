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

#include "stdafx.h"
#include "WinDirStat.h"
#include "TreeMap.h"
#include "SelectObject.h"
#include "OwnerDrawnListControl.h"
#include "DarkMode.h"

namespace
{
    constexpr UINT TEXT_X_MARGIN = 6; // Horizontal distance of the text from the edge of the item rectangle

    constexpr UINT LABEL_INFLATE_CX = 3; // How much the label is enlarged, to get the selection and focus rectangle
    constexpr UINT LABEL_Y_MARGIN = 2;

    constexpr UINT GENERAL_INDENT = 5;
}

/////////////////////////////////////////////////////////////////////////////

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
    static const CSize sizeImage(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON));

    if (width == nullptr)
    {
        // Draw the color with transparent background
        if (const auto icon = const_cast<const HICON>(GetIcon()); icon != nullptr)
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
    pdc->DrawText(GetText(0).c_str(), rcLabel, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_CALCRECT | DT_NOPREFIX);

    rcLabel.InflateRect(LABEL_INFLATE_CX, 0);
    rcLabel.top = rcRest.top + LABEL_Y_MARGIN;
    rcLabel.bottom = rcRest.bottom - LABEL_Y_MARGIN;

    COLORREF textColor = GetItemTextColor();
    if (width == nullptr && (state & ODS_SELECTED) != 0 && (list->HasFocus() || list->IsShowSelectionAlways()))
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
        pdc->DrawText(GetText(0).c_str(), rcRest, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX);
    }

    rcLabel.InflateRect(1, 1);

    *focusLeft = rcLabel.left;

    if ((state & ODS_FOCUS) != 0 && list->HasFocus() && width == nullptr && !list->IsFullRowSelection())
    {
        pdc->DrawFocusRect(rcLabel);
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
    if (!list->HasFocus() && !list->IsShowSelectionAlways())
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

IMPLEMENT_DYNAMIC(COwnerDrawnListControl, CSortingListControl)

COwnerDrawnListControl::COwnerDrawnListControl(int rowHeight, std::vector<int>* columnOrder, std::vector<int>* columnWidths)
    : CSortingListControl(columnOrder, columnWidths)
    , m_RowHeight(rowHeight)
{
    ASSERT(rowHeight > 0);
    InitializeColors();
}

// This method MUST be called before the Control is shown.
void COwnerDrawnListControl::OnColumnsInserted()
{
    // The pacman shall not draw over our header control.
    ModifyStyle(0, WS_CLIPCHILDREN);
    LoadPersistentAttributes();
}

void COwnerDrawnListControl::SysColorChanged()
{
    InitializeColors();
}

int COwnerDrawnListControl::GetRowHeight() const
{
    return m_RowHeight;
}

void COwnerDrawnListControl::ShowGrid(const bool show)
{
    m_ShowGrid = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowStripes(const bool show)
{
    m_ShowStripes = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowFullRowSelection(const bool show)
{
    m_ShowFullRowSelect = show;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

bool COwnerDrawnListControl::IsFullRowSelection() const
{
    return m_ShowFullRowSelect;
}

// Normal window background color
COLORREF COwnerDrawnListControl::GetWindowColor() const
{
    return m_WindowColor;
}

// Shaded window background color (for stripes)
COLORREF COwnerDrawnListControl::GetStripeColor() const
{
    return m_StripeColor;
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
    return m_ShowStripes && i % 2 != 0;
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(const int i) const
{
    return IsItemStripColor(i) ? GetStripeColor() : GetWindowColor();
}

COLORREF COwnerDrawnListControl::GetItemSelectionBackgroundColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection() && (HasFocus() || IsShowSelectionAlways()))
    {
        return GetHighlightColor();
    }

    return GetItemBackgroundColor(i);
}

COLORREF COwnerDrawnListControl::GetItemSelectionTextColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection() && (HasFocus() || IsShowSelectionAlways()))
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
    const auto item = reinterpret_cast<COwnerDrawnListItem*>(GetItemData(i));
    return item;
}

int COwnerDrawnListControl::FindListItem(const COwnerDrawnListItem* item) const
{
    LVFINDINFO fi;
    fi.flags = LVFI_PARAM;
    fi.lParam = reinterpret_cast<LPARAM>(item);
    return FindItem(&fi);
}

void COwnerDrawnListControl::InitializeColors()
{
    // I try to find a good contrast to COLOR_WINDOW (usually white or light grey).
    // This is a result of experiments.

    constexpr double diff = 0.07; // Try to alter the brightness by diff.
    constexpr double threshold = 1.04; // If result would be brighter, make color darker.

    m_WindowColor = DarkMode::WdsSysColor(COLOR_WINDOW);

    double b = CColorSpace::GetColorBrightness(m_WindowColor);

    if (b + diff > threshold)
    {
        b -= diff;
    }
    else
    {
        b += diff;
        b = std::min<double>(b, 1.0);
    }

    m_StripeColor = DarkMode::IsDarkModeActive() ? DarkMode::WdsSysColor(COLOR_WINDOWFRAME) :
        CColorSpace::MakeBrightColor(m_WindowColor, b);
}

void COwnerDrawnListControl::DrawItem(LPDRAWITEMSTRUCT pdis)
{
    COwnerDrawnListItem* item = reinterpret_cast<COwnerDrawnListItem*>(pdis->itemData);
    CDC* pdc = CDC::FromHandle(pdis->hDC);
    CRect rcItem(pdis->rcItem);

    CDC dcMem;
    dcMem.CreateCompatibleDC(pdc);

    CBitmap bm;
    bm.CreateCompatibleBitmap(pdc, rcItem.Width(), rcItem.Height());
    CSelectObject sobm(&dcMem, &bm);

    COLORREF backColor = GetItemBackgroundColor(static_cast<int>(pdis->itemID));
    dcMem.FillSolidRect(rcItem - rcItem.TopLeft(), backColor);

    // Set defaults for all text drawing
    CSetBkColor bkColor(&dcMem, backColor);
    CSelectObject sofont(&dcMem, GetFont());

    int focusLeft = 0;
    const int headerCount = GetHeaderCtrl()->GetItemCount();
    for (int i = 0; i < headerCount; i++)
    {
        // The subitem tracks the identifier that maps the column enum
        LVCOLUMN colInfo{ LVCF_SUBITEM | LVCF_FMT };
        GetColumn(i, &colInfo);
        const int subitem = colInfo.iSubItem;
        const int alignment = (colInfo.fmt & LVCFMT_RIGHT) != 0 ? DT_RIGHT : DT_LEFT;

        CRect rc = GetWholeSubitemRect(pdis->itemID, i);
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
            if (pdis->itemState & ODS_SELECTED && (HasFocus() || IsShowSelectionAlways()) && IsFullRowSelection())
            {
                backColor = GetItemSelectionBackgroundColor(pdis->itemID);
                textColor = GetItemSelectionTextColor(pdis->itemID);
            }

            // Set the text color
            CSetTextColor tc(&dcMem, textColor);
            CSetBkColor backColorSub(&dcMem, backColor);

            // Draw the (sub)item text
            dcMem.DrawText(s.c_str(), rcText, alignment | DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX);
        }

        if (m_ShowGrid)
        {
            constexpr COLORREF gridColor = RGB(212, 208, 200);
            CPen pen(PS_SOLID, 1, gridColor);
            CSelectObject sopen(&dcMem, &pen);

            dcMem.MoveTo(rcDraw.right, rcDraw.top);
            dcMem.LineTo(rcDraw.right, rcDraw.bottom);
            dcMem.MoveTo(rcDraw.left, rcDraw.bottom + 1);
            dcMem.LineTo(rcDraw.right, rcDraw.bottom + 1);
        }
    }

    if ((pdis->itemState & ODS_FOCUS) != 0 && HasFocus() && IsFullRowSelection())
    {
        CRect focusRect = rcItem - rcItem.TopLeft();
        focusRect.left = focusLeft - 1;
        dcMem.DrawFocusRect(focusRect);
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
        HDITEM hditem = { HDI_WIDTH };
        GetHeaderCtrl()->GetItem(0, &hditem);

        VERIFY(GetItemRect(item, rc, LVIR_LABEL));
        rc.left = rc.right - hditem.cxy;
    }
    else
    {
        VERIFY(GetSubItemRect(item, subitem, LVIR_LABEL, rc));
    }

    if (m_ShowGrid)
    {
        rc.right--;
        rc.bottom--;
    }
    return rc;
}

bool COwnerDrawnListControl::HasFocus() const
{
    return ::GetFocus() == m_hWnd;
}

bool COwnerDrawnListControl::IsShowSelectionAlways() const
{
    return (GetStyle() & LVS_SHOWSELALWAYS) != 0;
}

int COwnerDrawnListControl::GetSubItemWidth(COwnerDrawnListItem* item, const int subitem)
{
    CClientDC dc(this);
    CRect rc(0, 0, 3500, 20);

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
    dc.DrawText(s.c_str(), rc, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP);

    rc.InflateRect(TEXT_X_MARGIN, 0);
    return rc.Width();
}

BEGIN_MESSAGE_MAP(COwnerDrawnListControl, CSortingListControl)
    ON_WM_ERASEBKGND()
    ON_NOTIFY(HDN_DIVIDERDBLCLICK, 0, OnHdnDividerdblclick)
    ON_NOTIFY(HDN_ITEMCHANGING, 0, OnHdnItemchanging)
    ON_WM_SHOWWINDOW()
    ON_NOTIFY(NM_CUSTOMDRAW, 0, OnCustomDraw)
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
    else if(pCustomDraw->dwDrawStage == CDDS_ITEMPREPAINT)
    {
        ::SetTextColor(pCustomDraw->hdc, DarkMode::WdsSysColor(COLOR_BTNTEXT));
    }
}

BOOL COwnerDrawnListControl::OnEraseBkgnd(CDC* pDC)
{
    ASSERT(GetHeaderCtrl()->GetItemCount() > 0);

    // Calculate bottom of control
    int itemTopPos = 0;
    if (GetItemCount() > 0)
    {
        CRect rc;
        GetItemRect(GetTopIndex(), rc, LVIR_BOUNDS);
        itemTopPos = rc.top;
    }

    const int lineCount = GetCountPerPage() + 1;
    const int firstItem = GetTopIndex();
    const int lastItem = min(firstItem + lineCount, GetItemCount()) - 1;

    ASSERT(GetItemCount() == 0 || firstItem < GetItemCount());
    ASSERT(GetItemCount() == 0 || lastItem < GetItemCount());
    ASSERT(GetItemCount() == 0 || lastItem >= firstItem);

    const int tableBottom = itemTopPos + (lastItem - firstItem + 1) * GetRowHeight();

    // Calculate where the columns end on the right
    int tableRight = -GetScrollPos(SB_HORZ);
    for (int i = 0, itemMax = GetHeaderCtrl()->GetItemCount(); i < itemMax; i++)
    {
        HDITEM hdi{ HDI_WIDTH };
        GetHeaderCtrl()->GetItem(i, &hdi);
        tableRight += hdi.cxy;
    }

    CRect rcClient;
    GetClientRect(rcClient);
    const COLORREF bgcolor = DarkMode::WdsSysColor(COLOR_WINDOW);

    // draw blank space on right
    CRect fillRight(tableRight, rcClient.top, rcClient.right, rcClient.bottom);
    pDC->FillSolidRect(fillRight, bgcolor);

    // draw blank space on bottom
    CRect fillLeft(rcClient.left, tableBottom, rcClient.right, rcClient.bottom);
    pDC->FillSolidRect(fillLeft, bgcolor);

    return true;
}

void COwnerDrawnListControl::OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const int column = reinterpret_cast<LPNMHEADER>(pNMHDR)->iItem;
    const int subitem = ColumnToSubItem(column);

    // fetch size of rendered column header text
    // temporarily insert a false column to the finalize column does 
    // not autosize to fit the whole control width
    SetRedraw(FALSE);
    const int falseColumn = InsertColumn(column + 1, L"");
    SetColumnWidth(column, LVSCW_AUTOSIZE_USEHEADER);
    int width = GetColumnWidth(column);
    DeleteColumn(falseColumn);

    // fetch size of sub-elements
    for (int i = 0, itemMax = GetItemCount(); i < itemMax; i++)
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
