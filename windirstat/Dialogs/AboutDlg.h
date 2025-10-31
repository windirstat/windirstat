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

#include "Layout.h"

void StartAboutDialog();

class CAboutThread final : public CWinThread
{
    DECLARE_DYNCREATE(CAboutThread)

protected:
    BOOL InitInstance() override;
};

class CAboutDlg final : public CDialogEx
{
    enum : std::uint8_t { IDD = IDD_ABOUTBOX };

    class WdsTabControl final : public CTabCtrl
    {
    public:
        void Initialize();
        void SetPageText(int tab);

    protected:
        CFont m_MonoFont;
        CRichEditCtrl m_Text;

        DECLARE_MESSAGE_MAP()
        afx_msg void OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult);
        afx_msg void OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult);
        afx_msg void OnSize(UINT nType, int cx, int cy);
    };

public:
    CAboutDlg();
    static std::wstring GetAppVersion();

protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    CStatic m_Caption;
    WdsTabControl m_Tab;
    CLayout m_Layout;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();
};
