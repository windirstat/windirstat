// aboutdlg.h - Declaration of StartAboutDialog(), CAboutThread and CAboutDlg
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#include "layout.h"
#include <common/wds_constants.h>


void StartAboutDialog();


class CAboutThread: public CWinThread
{
    DECLARE_DYNCREATE(CAboutThread);
protected:
    BOOL InitInstance() override;
};


class CAboutDlg final : public CDialog
{
    enum { IDD = IDD_ABOUTBOX };

    class CMyTabControl: public CTabCtrl
    {
    public:
        void Initialize();
        void SetPageText(int tab);

    protected:
        CRichEditCtrl m_text;

        DECLARE_MESSAGE_MAP()
        afx_msg void OnEnLinkText(NMHDR *pNMHDR, LRESULT *pResult);
        afx_msg void OnEnMsgFilter(NMHDR *pNMHDR, LRESULT *pResult);
        afx_msg void OnSize(UINT nType, int cx, int cy);
    };

public:
    CAboutDlg();
    static CStringW GetAppVersion();
    static CStringW GetDevelList();
    static CStringW GetTranslatorList();

protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    CStatic m_caption;
    CMyTabControl m_tab;
    CLayout m_layout;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnTcnSelchangeTab(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();
};
