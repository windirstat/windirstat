﻿// FileTreeControl.h - Declaration of CFileTreeControl and CFileTreeView
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

#pragma once

#include "Item.h"
#include "TreeListControl.h"

class CFileTreeControl final : public CTreeListControl
{
public:
    CFileTreeControl();
    ~CFileTreeControl() override { m_Singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileTreeControl* Get() { return m_Singleton; }

protected:

    static CFileTreeControl * m_Singleton;

    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnHeaderEndDrag(UINT, NMHDR* pNMHDR, LRESULT* pResult);
};
