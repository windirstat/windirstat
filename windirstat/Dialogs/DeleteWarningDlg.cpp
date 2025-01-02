// DeleteWarningDlg.cpp - implementation of CDeleteWarningDlg
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
#include "DeleteWarningDlg.h"
#include "Localization.h"

IMPLEMENT_DYNAMIC(CDeleteWarningDlg, CDialogEx)

CDeleteWarningDlg::CDeleteWarningDlg(const std::vector<CItem*> & items, CWnd* pParent)
    : CDialogEx(IDD, pParent), m_Items(items)
{
}

void CDeleteWarningDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_DONTSHOWAGAIN, m_DontShowAgain);
    DDX_Control(pDX, IDC_FILENAMES, m_Files);
}

BEGIN_MESSAGE_MAP(CDeleteWarningDlg, CDialogEx)
    ON_BN_CLICKED(IDNO, OnBnClickedNo)
    ON_BN_CLICKED(IDYES, OnBnClickedYes)
END_MESSAGE_MAP()

void CDeleteWarningDlg::OnBnClickedNo()
{
    UpdateData();
    EndDialog(IDNO);
}

void CDeleteWarningDlg::OnBnClickedYes()
{
    UpdateData();
    EndDialog(IDYES);
}

BOOL CDeleteWarningDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    int extent = 0;
    const CClientDC dc(this);
    for (const auto& item : m_Items)
    {
        extent = max(extent, dc.GetTextExtent(item->GetPath().c_str()).cx);
        m_Files.AddString(item->GetPath().c_str());
    }
    m_Files.SetHorizontalExtent(extent);

    GotoDlgCtrl(GetDlgItem(IDNO));
    return TRUE;
}
