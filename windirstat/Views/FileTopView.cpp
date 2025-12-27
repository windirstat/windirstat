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
#include "FileTopView.h"

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CFileTopView, CView)

CFileTopView::CFileTopView() = default;

void CFileTopView::SysColorChanged()
{
    m_control.SysColorChanged();
}

void CFileTopView::OnDraw(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
}

BEGIN_MESSAGE_MAP(CFileTopView, CView)
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SETFOCUS()
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemChanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()

void CFileTopView::OnSize(const UINT nType, const int cx, const int cy)
{
    CView::OnSize(nType, cx, cy);
    if (IsWindow(m_control.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_control.MoveWindow(rc);
    }
}

int CFileTopView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_control.CreateExtended(0, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL));

    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    m_control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, 500, COL_ITEMTOP_NAME);
    m_control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_PHYSICAL).c_str(), LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_PHYSICAL);
    m_control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_LOGICAL).c_str(), LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_LOGICAL);
    m_control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_LAST_CHANGE).c_str(), LVCFMT_LEFT, 120, COL_ITEMTOP_LAST_CHANGE);
    m_control.SetSorting(COL_ITEMTOP_SIZE_PHYSICAL, false);

    m_control.OnColumnsInserted();

    return 0;
}

BOOL CFileTopView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CFileTopView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_control.SetFocus();
}

void CFileTopView::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // only process state changes
    if ((pNMLV->uChanged & LVIF_STATE) == 0)
    {
        return;
    }
  
    // Signal to listeners that selection has changed
    CDirStatDoc::Get()->UpdateAllViews(this, HINT_SELECTIONREFRESH);
     
    *pResult = FALSE;
}

void CFileTopView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    switch (lHint)
    {
        case HINT_NEWROOT:
        {
            m_control.SetRootItem(CDirStatDoc::Get()->GetRootItemTop());
            m_control.Invalidate();
        }
        break;

        case HINT_LISTSTYLECHANGED:
        {
            m_control.ShowGrid(COptions::ListGrid);
            m_control.ShowStripes(COptions::ListStripes);
            m_control.ShowFullRowSelection(COptions::ListFullRowSelection);
            m_control.SortItems();
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

void CFileTopView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_control.SelectedItemCanToggle());
}

void CFileTopView::OnPopupToggle()
{
    m_control.ToggleSelectedItem();
}
