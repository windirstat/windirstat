// OwnerDrawnListControl.cpp - Implementation of COwnerDrawnListItem and COwnerDrawnListControl
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "TreeMap.h"    // CColorSpace
#include "SelectObject.h"
#include "OwnerDrawnListControl.h"

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
void COwnerDrawnListItem::DrawLabel(const COwnerDrawnListControl* list, CImageList* il, CDC* pdc, CRect& rc, const UINT state, int* width, int* focusLeft, const bool indent) const
{
    CRect rcRest = rc;
    // Increase indentation according to tree-level
    if (indent)
    {
        rcRest.left += GENERAL_INDENT;
    }

    // Prepare to draw the file/folder icon
    const auto imageIndex = GetImage();
    ASSERT(GetImage() < il->GetImageCount());

    static CRect rcImageDefault(0, 0, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON));
    CRect rcImage = rcImageDefault;

    if (imageIndex != -1)
    {
        IMAGEINFO ii;
        il->GetImageInfo(GetImage(), &ii);
        rcImage = ii.rcImage;
    }

    if (width == nullptr)
    {
        // Draw the color with transparent background
        const CPoint pt(rcRest.left, rcRest.top + rcRest.Height() / 2 - rcImage.Height() / 2);
        il->SetBkColor(CLR_NONE);
        if (imageIndex != -1) il->Draw(pdc, GetImage(), pt, ILD_NORMAL);
    }

    // Decrease size of the remainder rectangle from left
    rcRest.left += rcImage.Width();

    CSelectObject sofont(pdc, list->GetFont());

    rcRest.DeflateRect(list->GetTextXMargin(), 0);

    CRect rcLabel = rcRest;
    pdc->DrawText(GetText(0).c_str(), rcLabel, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_CALCRECT | DT_NOPREFIX);

    rcLabel.InflateRect(LABEL_INFLATE_CX, 0);
    rcLabel.top    = rcRest.top + LABEL_Y_MARGIN;
    rcLabel.bottom = rcRest.bottom - LABEL_Y_MARGIN;

    CSetBkMode bk(pdc, TRANSPARENT);
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
    rc           = rcLabel;

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
    constexpr int LIGHT = 198; // light edge
    constexpr int DARK  = 118; // dark edge
    constexpr int BG    = 225; // background (lighter than light edge)

    constexpr COLORREF light = RGB(LIGHT, LIGHT, LIGHT);
    constexpr COLORREF dark  = RGB(DARK, DARK, DARK);
    constexpr COLORREF bg    = RGB(BG, BG, BG);

    CRect rcLeft = rc;
    rcLeft.right = static_cast<int>(rcLeft.left + rc.Width() * fraction);

    CRect rcRight = rc;
    rcRight.left  = rcLeft.right;

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
    return RGB(190, 190, 190);
}

// Highlight text color if we have no focus
COLORREF COwnerDrawnListControl::GetNonFocusHighlightTextColor() const
{
    return RGB(0, 0, 0);
}

COLORREF COwnerDrawnListControl::GetHighlightColor() const
{
    if (HasFocus())
    {
        return GetSysColor(COLOR_HIGHLIGHT);
    }

    return GetNonFocusHighlightColor();
}

COLORREF COwnerDrawnListControl::GetHighlightTextColor() const
{
    if (HasFocus())
    {
        return GetSysColor(COLOR_HIGHLIGHTTEXT);
    }

    return GetNonFocusHighlightTextColor();
}

bool COwnerDrawnListControl::IsItem_stripeColor(const int i) const
{
    return m_ShowStripes && i % 2 != 0;
}

bool COwnerDrawnListControl::IsItem_stripeColor(const COwnerDrawnListItem* item) const
{
    return IsItem_stripeColor(FindListItem(item));
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(const int i) const
{
    return IsItem_stripeColor(i) ? GetStripeColor() : GetWindowColor();
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(const COwnerDrawnListItem* item) const
{
    return GetItemBackgroundColor(FindListItem(item));
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

COLORREF COwnerDrawnListControl::GetItemSelectionBackgroundColor(const COwnerDrawnListItem* item) const
{
    return GetItemSelectionBackgroundColor(FindListItem(item));
}

COLORREF COwnerDrawnListControl::GetItemSelectionTextColor(const int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection() && (HasFocus() || IsShowSelectionAlways()))
    {
        return GetHighlightTextColor();
    }

    return GetSysColor(COLOR_WINDOWTEXT);
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
    fi.flags  = LVFI_PARAM;
    fi.lParam = reinterpret_cast<LPARAM>(item);
    return FindItem(&fi);
}

void COwnerDrawnListControl::InitializeColors()
{
    // I try to find a good contrast to COLOR_WINDOW (usually white or light grey).
    // This is a result of experiments.

    constexpr double diff      = 0.07; // Try to alter the brightness by diff.
    constexpr double threshold = 1.04; // If result would be brighter, make color darker.

    m_WindowColor = GetSysColor(COLOR_WINDOW);

    double b = CColorSpace::GetColorBrightness(m_WindowColor);

    if (b + diff > threshold)
    {
        b -= diff;
    }
    else
    {
        b += diff;
        if (b > 1.0)
        {
            b = 1.0;
        }
    }

    m_StripeColor = CColorSpace::MakeBrightColor(m_WindowColor, b);
}

void COwnerDrawnListControl::DrawItem(LPDRAWITEMSTRUCT pdis)
{
    const COwnerDrawnListItem* item = reinterpret_cast<COwnerDrawnListItem*>(pdis->itemData);
    CDC* pdc = CDC::FromHandle(pdis->hDC);
    CRect rcItem(pdis->rcItem);

    CDC dcMem;
    dcMem.CreateCompatibleDC(pdc);

    CBitmap bm;
    bm.CreateCompatibleBitmap(pdc, rcItem.Width(), rcItem.Height());
    CSelectObject sobm(&dcMem, &bm);

    dcMem.FillSolidRect(rcItem - rcItem.TopLeft(),
        GetItemBackgroundColor(static_cast<int>(pdis->itemID)));

    int focusLeft = 0;
    const int headerCount = GetHeaderCtrl()->GetItemCount();
    for (int i = 0; i < headerCount; i++)
    {
        // The subitem tracks the identifier that maps the column enum
        const int subitem = ColumnToSubItem(i);
        
        CRect rc = GetWholeSubitemRect(pdis->itemID, i);
        const CRect rcDraw = rc - rcItem.TopLeft();

        if (!item->DrawSubitem(subitem, &dcMem, rcDraw, pdis->itemState, nullptr, &focusLeft))
        {
            item->DrawSelection(this, &dcMem, rcDraw, pdis->itemState);

            CRect rcText = rcDraw;
            rcText.DeflateRect(TEXT_X_MARGIN, 0);
            CSetBkMode bk(&dcMem, TRANSPARENT);
            CSelectObject sofont(&dcMem, GetFont());
            const std::wstring s = item->GetText(subitem);

            // Get the correct color in case of compressed or encrypted items
            COLORREF textColor = item->GetItemTextColor();

            // Except if the item is selected - in this case just use standard colors
            if (pdis->itemState & ODS_SELECTED && (HasFocus() || IsShowSelectionAlways()) && IsFullRowSelection())
            {
                textColor = GetItemSelectionTextColor(pdis->itemID);
            }

            // Set the text color
            CSetTextColor tc(&dcMem, textColor);

            // Draw the (sub)item text
            dcMem.DrawText(s.c_str(), rcText, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX);
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

void COwnerDrawnListControl::RedrawItem(const COwnerDrawnListItem * item) const
{
    const auto i = FindListItem(item);
    ::PostMessage(m_hWnd, LVM_REDRAWITEMS, i, i);
}

CRect COwnerDrawnListControl::GetWholeSubitemRect(const int item, const int subitem) const
{
    CRect rc;
    if (subitem == 0)
    {
        // Special case column 0:
        // If we did GetSubItemRect(item 0, LVIR_LABEL, rc)
        // and we have an image list, then we would get the rectangle
        // excluding the image.
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

int COwnerDrawnListControl::GetSubItemWidth(const COwnerDrawnListItem* item, const int subitem)
{
    CClientDC dc(this);
    CRect rc(0, 0, 1000, 1000);

    int width;
    int dummy = rc.left;
    if (item->DrawSubitem(subitem, &dc, rc, 0, &width, &dummy))
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

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(COwnerDrawnListControl, CSortingListControl)
    ON_WM_ERASEBKGND()
    ON_NOTIFY(HDN_DIVIDERDBLCLICK, 0, OnHdnDividerdblclick)
    ON_NOTIFY(HDN_ITEMCHANGING, 0, OnHdnItemchanging)
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()
#pragma warning(pop)

BOOL COwnerDrawnListControl::OnEraseBkgnd(CDC* pDC)
{
    ASSERT(GetHeaderCtrl()->GetItemCount() > 0);

    // Calculate bottom of control
    int itemTopPos = 0;
    if (GetItemCount() > 0)
    {
        CRect rc;
        GetItemRect(GetTopIndex(), rc, LVIR_BOUNDS);
        itemTopPos= rc.top;
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
    const COLORREF bgcolor = GetSysColor(COLOR_WINDOW);

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

    int width = 10;
    for (int i = 0, itemMax = GetItemCount(); i < itemMax; i++)
    {
        width = max(width, GetSubItemWidth(GetItem(i), subitem));
    }
    SetColumnWidth(column, width + 5);

    *pResult = FALSE;
}

void COwnerDrawnListControl::OnHdnItemchanging(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    Default();
    InvalidateRect(nullptr);

    *pResult = FALSE;
}
