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

class CAboutDlg final : public MessageTarget<CAboutDlg, CLayoutDialog>
{
    class WdsTabControl final : public MessageTarget<WdsTabControl, CTabControl>
    {
    public:
        void Initialize();
        void ClearSelectionCursor();
        bool HandleTabKey(bool shiftPressed);

    protected:
        CFont m_monoFont;
        CRichEditCtrl m_textAbout;
        CRichEditCtrl m_textThanks;
        CRichEditCtrl m_textLicense;
        int m_tabAbout = 0;
        int m_tabThanks = 0;
        int m_tabLicense = 0;

        CRichEditCtrl& GetActiveRichEdit();

    public:
        static std::span<const RouteEntry> Routes();

    protected:
        void OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult);
        void OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult);
        void OnSetFocus(CWnd* pOldWnd);
    };

public:
    CAboutDlg();
    static std::wstring GetAppVersion();

protected:
    bool OnInitDialog() override;
    bool PreprocessMessage(MSG* pMsg) override;

    CStatic m_caption;
    WdsTabControl m_tab;

public:
    static std::span<const RouteEntry> Routes();

protected:
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    LRESULT OnTabChanged(WPARAM wParam, LPARAM lParam);
};

inline std::span<const RouteEntry> CAboutDlg::WdsTabControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Notify<&OnEnLinkText>(EN_LINK, ID_WDS_CONTROL),
        Route::Notify<&OnEnMsgFilter>(EN_MSGFILTER, ID_WDS_CONTROL),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
    };
    return entries;
}

inline std::span<const RouteEntry> CAboutDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&OnTabChanged>(WM_WDS_TAB_CHANGED),
    };
    return entries;
}
