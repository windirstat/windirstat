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

#include "pch.h"
#include "Layout.h"

void StartAboutDialog();

class CAboutThread final : public CWinThread
{
    DECLARE_DYNCREATE(CAboutThread)

protected:
    BOOL InitInstance() override;
};

class CAboutDlg final : public CLayoutDialogEx
{
    class WdsTabControl final : public CMFCTabCtrl
    {
    public:
        void Initialize();
        void ClearSelectionCursor();

    protected:
        CFont m_monoFont;
        CRichEditCtrl m_textAbout;
        CRichEditCtrl m_textThanks;
        CRichEditCtrl m_textLicense;

        DECLARE_MESSAGE_MAP()
        afx_msg void OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult);
        afx_msg void OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult);
    };

public:
    CAboutDlg();
    static std::wstring GetAppVersion();

protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    CStatic m_caption;
    WdsTabControl m_tab;

    DECLARE_MESSAGE_MAP()
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg LRESULT OnTabChanged(WPARAM wParam, LPARAM lParam);
};
