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
#include "AboutDlg.h"

/////////////////////////////////////////////////////////////////////////////

void CAboutDlg::WdsTabControl::Initialize()
{
    SetLocation(Location::Top);
    CTabCtrlHelper::SetupTabControl(*this);

    // Helper to create and configure RichEdit controls
    auto createText = [&](CRichEditCtrl& ctrl, const DWORD align = ES_CENTER)
    {
        ctrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | align,
            CRect(), this, ID_WDS_CONTROL);
        ctrl.SetEventMask(ENM_LINK | ENM_KEYEVENTS);
        ctrl.SetOptions(ECOOP_OR, ECO_READONLY);
        ctrl.SetOptions(ECOOP_AND, ~static_cast<DWORD>(ECO_SELECTIONBAR));
        ctrl.HideSelection();
        return &ctrl;
    };

    // Create all three pages and add them as tabs, remembering the index each was assigned
    m_tabAbout = AddTab(createText(m_textAbout), IDS_ABOUT_ABOUT.data());
    m_tabThanks = AddTab(createText(m_textThanks), IDS_ABOUT_THANKS.data());
    m_tabLicense = AddTab(createText(m_textLicense, ES_LEFT), IDS_ABOUT_LICENSE.data());
    Localization::UpdateTabControl(*this);

    // Use monospace font for license page
    m_monoFont.Create(ScaleForDpi(12), FW_DONTCARE, wds::strFontLucidaConsole, OUT_OUTLINE_PRECIS,
        CLEARTYPE_NATURAL_QUALITY, FF_MODERN);

    // Populate text
    m_textAbout.SetText(Localization::Format(IDS_ABOUT_ABOUT_TEXTss,
        Localization::LookupNeutral(IDS_AUTHOR_EMAIL),
        Localization::LookupNeutral(IDS_URL_WEBSITE)).c_str());

    m_textThanks.SetText(Localization::Lookup(IDS_ABOUT_THANKS_TEXT).c_str());

    m_textLicense.SetText(GetTextResource(IDR_LICENSE).c_str());
    m_textLicense.SetFont(m_monoFont);

    // Set default rich edit settings
    CHARFORMAT2 charFormat = {{.cbSize = sizeof(CHARFORMAT2)}};
    charFormat.dwMask = CFM_COLOR;
    charFormat.crTextColor = DarkMode::SystemColor(COLOR_WINDOWTEXT);
    const auto bgColor = DarkMode::SystemColor(COLOR_WINDOW);

    for (const auto ctrl : { &m_textAbout, &m_textThanks, &m_textLicense })
    {
        ctrl->SetDefaultCharFormat(charFormat);
        ctrl->EnableAutoUrlDetection();
        ctrl->SetBackgroundColor(bgColor);
    }
}

CRichEditCtrl& CAboutDlg::WdsTabControl::GetActiveRichEdit()
{
    const auto tabIndex = ActiveTab();
    return tabIndex == m_tabAbout ? m_textAbout :
           tabIndex == m_tabThanks ? m_textThanks :
           m_textLicense;
}

void CAboutDlg::WdsTabControl::ClearSelectionCursor()
{
    auto& active = GetActiveRichEdit();
    active.SetSel(0, 0);
    active.HideCaret();
    active.HideSelection();
}

bool CAboutDlg::WdsTabControl::HandleTabKey(const bool shiftPressed)
{
    const int activeTab = ActiveTab();
    const int tabCount = TabCount();

    if (shiftPressed)
    {
        if (activeTab > 0)
        {
            SelectTab(activeTab - 1);
            return true;
        }
    }
    else if (activeTab < tabCount - 1)
    {
        SelectTab(activeTab + 1);
        return true;
    }

    // If we reach here, move focus to OK button
    GetParent()->GetDlgItem(IDOK)->SetFocus();
    return true;
}

void CAboutDlg::WdsTabControl::OnSetFocus(CWnd* pOldWnd)
{
    CTabControl::OnSetFocus(pOldWnd);

    // Hide the caret in the active RichEdit control
    ClearSelectionCursor();
}

void CAboutDlg::WdsTabControl::OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult)
{
    const ENLINK* el = reinterpret_cast<ENLINK*>(pNMHDR);
    *pResult = 0;

    if (el->msg == WM_LBUTTONDOWN)
    {
        const auto& active = GetActiveRichEdit();
        const std::wstring link = active.TextRange(el->chrg.cpMin, el->chrg.cpMax);
        ::ShellExecute(*this, nullptr, link.c_str(), nullptr, wds::strEmpty, SW_SHOWNORMAL);
    }
}

void CAboutDlg::WdsTabControl::OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult)
{
    const MSGFILTER* mf = reinterpret_cast<MSGFILTER*>(pNMHDR);
    *pResult = 0;

    if (mf->msg == WM_KEYDOWN)
    {
        if (mf->wParam == VK_ESCAPE)
        {
            GetParent()->PostMessage(WM_COMMAND, IDOK, 0);
            *pResult = 1;
        }
        else if (mf->wParam == VK_TAB)
        {
            HandleTabKey(IsKeyDown(VK_SHIFT));
            *pResult = 1;
        }
    }
    else if (mf->msg == WM_LBUTTONDOWN || mf->msg == WM_LBUTTONDBLCLK || mf->msg == WM_RBUTTONDOWN)
    {
        auto& active = GetActiveRichEdit();
        active.SetSel(-1, 0);
        SetFocus();
        *pResult = 1;
    }
}

////////////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg()
    : MessageTarget(IDD_ABOUTBOX, COptions::AboutWindowRect.Ptr())
{
}

std::wstring CAboutDlg::GetAppVersion()
{
    return std::format(L"{} ({})\nGit Commit: {}", GetAppTitle(),
        _CRT_WIDE(GIT_DATE), _CRT_WIDE(GIT_COMMIT));
}

bool CAboutDlg::OnInitDialog()
{
    CLayoutDialog::OnInitDialog();

    m_caption.SubclassDlgItem(IDC_CAPTION, this);

    // Re-create the tab control
    CWnd* placeholderTabCtrl = GetDlgItem(IDC_TAB);
    const CRect placeholderRect = WindowRectInClient(placeholderTabCtrl->Handle());
    placeholderTabCtrl->DestroyWindow();
    m_tab.Create(placeholderRect, this, IDC_TAB);
    Localization::UpdateDialogs(*this);

    m_layout.AddControl(IDC_CAPTION, 0.5, 0, 0, 0);
    m_layout.AddControl(IDC_TAB, 0, 0, 1, 1);
    m_layout.AddControl(IDOK, 0.5, 1, 0, 0);
    m_layout.OnInitDialog(true);

    m_tab.Initialize();

    m_caption.SetText(GetAppVersion().c_str());

    DarkMode::AdjustControls(Handle());

    // Set initial focus to tab control
    m_tab.SetFocus();
    return false;
}

LRESULT CAboutDlg::OnTabChanged(WPARAM, LPARAM)
{
    m_tab.ClearSelectionCursor();
    return 0;
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

bool CAboutDlg::PreprocessMessage(MSG* pMsg)
{
    // Handle tab key when focus is on OK button
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    {
        if (GetFocus() == GetDlgItem(IDOK))
        {
            m_tab.SelectTab(IsKeyDown(VK_SHIFT) ? m_tab.TabCount() - 1 : 0);
            m_tab.SetFocus();
            return true;
        }

        // Force showing focus rectangles
        SendMessage(WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS));
    }

    return CLayoutDialog::PreprocessMessage(pMsg);
}
