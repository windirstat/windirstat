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
#include "ModalApiShuttle.h"

#include <functional>

IMPLEMENT_DYNAMIC(CModalApiShuttle, CDialogEx)

CModalApiShuttle::CModalApiShuttle(const std::function<void()>& task, CWnd* pParent) : CDialogEx(IDD, pParent), m_task(task)
{
}

BEGIN_MESSAGE_MAP(CModalApiShuttle, CDialogEx)
END_MESSAGE_MAP()

INT_PTR CModalApiShuttle::DoModal()
{
    return CDialogEx::DoModal();
}

BOOL CModalApiShuttle::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CRect rc;
    AfxGetMainWnd()->GetWindowRect(rc);
    rc.right  = rc.left;
    rc.bottom = rc.top;

    MoveWindow(rc, false);

    EnableWindow(true);
    ShowWindow(SW_SHOW);

    CWaitCursor wc;
    m_task();

    EndDialog(IDOK);
    return TRUE;
}
