// FileTreeControl.cpp - Implementation of CFileTreeView
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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
#include "DirStatDoc.h"
#include "Item.h"
#include "MainFrame.h"
#include "FileTreeView.h"
#include "Localization.h"

CFileTreeControl::CFileTreeControl() : CTreeListControl(20, COptions::FileTreeColumnOrder.Ptr(), COptions::FileTreeColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileTreeControl::GetAscendingDefault(const int column)
{
    return column == COL_NAME || column == COL_LASTCHANGE;
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CFileTreeControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_EX(HDN_ENDDRAG, 0, OnHeaderEndDrag)
END_MESSAGE_MAP()
#pragma warning(pop)

CFileTreeControl* CFileTreeControl::m_Singleton = nullptr;

BOOL CFileTreeControl::OnHeaderEndDrag(UINT, NMHDR* pNMHDR, LRESULT* pResult)
{
    // Do not allow first column to be re-ordered
    const LPNMHEADERW hdr = reinterpret_cast<LPNMHEADERW>(pNMHDR);
    const BOOL block = (hdr->iItem == COL_NAME || hdr->pitem->iOrder == COL_NAME);
    *pResult = block;
    return block;
}

void CFileTreeControl::OnItemDoubleClick(const int i)
{
    const auto item = reinterpret_cast<const CItem*>(GetItem(i));
    if (item->IsType(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

void CFileTreeControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_FILETREE);
}

void CFileTreeControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_EXTLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
