// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ControlView.h"

int CControlView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1) return -1;

    constexpr RECT rect = { 0, 0, 0, 0 };
    auto& control = GetControl();
    control.CreateExtended(LVS_EX_HEADERDRAGDROP, GetControlStyle(), rect, this, ID_WDS_CONTROL);
    control.ShowGrid(COptions::ListGrid);
    control.ShowStripes(COptions::ListStripes);
    control.ShowFullRowSelection(COptions::ListFullRowSelection);
    InitializeColumns();
    CreateBand();
    return 0;
}

int CControlView::InsertCol(const std::wstring_view& colName, const int nFormat, const int nWidth, const int nSubItem)
{
    return GetControl().InsertColumn(CHAR_MAX, Localization::Lookup(colName), nFormat, ScaleForDpi(nWidth), nSubItem);
}

void CControlView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{

    auto& control = GetControl();

    switch (change)
    {
    case MODEL_CHANGE_NEW_ROOT:
    {
        control.SetRootItem(item);
        control.Invalidate();
    }
    break;

    case MODEL_CHANGE_LIST_STYLE:
    {
        control.ShowGrid(COptions::ListGrid);
        control.ShowStripes(COptions::ListStripes);
        control.ShowFullRowSelection(COptions::ListFullRowSelection);
    }
    break;

    case MODEL_CHANGE_SIZE_MODE:
    {
        control.SortItems();
        control.Invalidate();
    }
    break;

    case MODEL_CHANGE_NONE:
    {
        CWinDirStatPane::OnUpdate(sender, change, item);
        control.Invalidate();
    }
    break;

    default:
        break;
    }
}

void CControlView::SysColorChanged()
{
    GetControl().SysColorChanged();
}

void CControlView::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (IsWindow(GetControl().m_hWnd))
    {
        m_bandHeight = LayoutBand(cx);
        CRect rc(0, m_bandHeight, cx, std::max(m_bandHeight, cy));
        GetControl().MoveWindow(rc);
    }
}

bool CControlView::OnEraseBkgnd(CDC* pDC)
{
    if (pDC != nullptr && m_bandHeight > 0)
    {
        CRect band = GetClientRect();
        band.bottom = band.top + m_bandHeight;
        pDC->FillSolidRect(band, DarkMode::SystemColor(COLOR_BTNFACE));
    }
    return true;
}

void CControlView::OnSetFocus(WindowRef /*pOldWnd*/)
{
    GetControl().SetFocus();
}

void CControlView::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    // only process state changes
    if (const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
        (pNMLV->uChanged & LVIF_STATE) == 0)
    {
        return;
    }

    // Defer selection processing for very large selections
    GetControl().PostSelectionChanged();

    *pResult = false;
}

void CControlView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetControl().SelectedItemCanToggle());
}

void CControlView::OnPopupToggle()
{
    GetControl().ToggleSelectedItem();
}
