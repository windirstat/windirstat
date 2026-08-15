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
    return 0;
}

int CControlView::InsertCol(const std::wstring_view& colName, const int nFormat, const int nWidth, const int nSubItem)
{
    return GetControl().InsertColumn(CHAR_MAX, Localization::Lookup(colName).c_str(), nFormat, ScaleForDpi(nWidth), nSubItem);
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
        CRect rc(0, 0, cx, cy);
        GetControl().MoveWindow(rc);
    }
}

void CControlView::OnSetFocus(CWnd* /*pOldWnd*/)
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
