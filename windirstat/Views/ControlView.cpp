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

IMPLEMENT_DYNAMIC(CControlView, CView)

BEGIN_MESSAGE_MAP(CControlView, CView)
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_SETFOCUS()
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemChanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()

int CControlView::InsertCol(const std::wstring_view& colName, const int nFormat, const int nWidth, const int nSubItem)
{
    return GetControl().InsertColumn(CHAR_MAX, Localization::Lookup(colName).c_str(), nFormat, DpiRest(nWidth), nSubItem);
}

void CControlView::OnDraw(CDC*)
{
}

void CControlView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    auto& control = GetControl();

    switch (lHint)
    {
    case HINT_NEWROOT:
    {
        control.SetRootItem();
        control.Invalidate();
    }
    break;

    case HINT_LISTSTYLECHANGED:
    {
        control.ShowGrid(COptions::ListGrid);
        control.ShowStripes(COptions::ListStripes);
        control.ShowFullRowSelection(COptions::ListFullRowSelection);
    }
    break;

    case HINT_NULL:
    {
        CView::OnUpdate(pSender, lHint, pHint);
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

void CControlView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (IsWindow(GetControl().m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        GetControl().MoveWindow(rc);
    }
}

BOOL CControlView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
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

    *pResult = FALSE;
}

void CControlView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetControl().SelectedItemCanToggle());
}

void CControlView::OnPopupToggle()
{
    GetControl().ToggleSelectedItem();
}
