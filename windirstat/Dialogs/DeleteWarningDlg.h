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

#pragma once

#include "Item.h"

#include <vector>

//
// CDeleteWarningDlg. As WinDirStat can delete all files and folders,
// e.g. C:\Windows, we show an additional warning, when the user
// selects "Delete" (before the shell shows its warning).
//
class CDeleteWarningDlg final : public CDialogEx
{
    DECLARE_DYNAMIC(CDeleteWarningDlg)

    enum : std::uint8_t { IDD = IDD_DELETE_WARNING };

    CDeleteWarningDlg(const std::vector<CItem*> & items, CWnd* pParent = nullptr);
    ~CDeleteWarningDlg() override = default;

    CListBox m_Files;
    std::vector<CItem*> m_Items;
    BOOL m_DontShowAgain = FALSE; // [out]

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedNo();
    afx_msg void OnBnClickedYes();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
