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
#include "version.h"
#include "AboutDlg.h"

#pragma comment(lib,"version.lib")

namespace
{
    // Tabs
    enum : std::uint8_t
    {
        TAB_ABOUT,
        TAB_THANKSTO,
        TAB_LICENSE
    };

    // Retrieve the GPL text from our resources
    std::wstring GetTextResource(const UINT id, const HMODULE dll = AfxGetResourceHandle())
    {
        // Fetch the resource
        const HRSRC hrsrc = ::FindResource(dll, MAKEINTRESOURCE(id), L"TEXT");
        if (nullptr == hrsrc) return {};

        // Decompress the resource
        const auto resourceData = GetCompressedResource(hrsrc);
        if (resourceData.empty()) return {};

        return CComBSTR(static_cast<int>(resourceData.size()),
            reinterpret_cast<LPCSTR>(resourceData.data())).m_str;
    }
}

/////////////////////////////////////////////////////////////////////////////

void StartAboutDialog()
{
    AfxBeginThread(RUNTIME_CLASS(CAboutThread), NULL);
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CAboutThread, CWinThread);

BOOL CAboutThread::InitInstance()
{
    CWinThread::InitInstance();

    CAboutDlg dlg;
    dlg.DoModal();
    return false;
}

/////////////////////////////////////////////////////////////////////////////

void CAboutDlg::WdsTabControl::Initialize()
{
    SetLocation(LOCATION_TOP);
    CTabCtrlHelper::SetupTabControl(*this, STYLE_FLAT);

    // Helper to create and configure RichEdit controls
    auto createText = [&](CRichEditCtrl& ctrl, const DWORD align = ES_CENTER)
    {
        ctrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | align,
            CRect(), this, ID_WDS_CONTROL);
        ctrl.SetEventMask(ENM_LINK | ENM_KEYEVENTS);
        ctrl.SetOptions(ECOOP_OR, ECO_READONLY);
        ctrl.SetOptions(ECOOP_AND, static_cast<DWORD>(~ECO_SELECTIONBAR));
        ctrl.HideSelection(TRUE, FALSE);
        return &ctrl;
    };

    // Create all three pages and add them as tabs
    AddTab(createText(m_textAbout), Localization::Lookup(IDS_ABOUT_ABOUT).c_str(), TAB_ABOUT);
    AddTab(createText(m_textThanks), Localization::Lookup(IDS_ABOUT_THANKS).c_str(), TAB_THANKSTO);
    AddTab(createText(m_textLicense, ES_LEFT), Localization::Lookup(IDS_ABOUT_LICENSE).c_str(), TAB_LICENSE);

    // Use monospace font for license page
    m_monoFont.CreateFont(DpiRest(12), 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, FF_MODERN, L"Consolas");

    // Populate text
    m_textAbout.SetWindowText(Localization::Format(IDS_ABOUT_ABOUT_TEXTss,
        Localization::LookupNeutral(IDS_AUTHOR_EMAIL),
        Localization::LookupNeutral(IDS_URL_WEBSITE)).c_str());

    m_textThanks.SetWindowText(Localization::Lookup(IDS_ABOUT_THANKS_TEXT).c_str());

    m_textLicense.SetWindowText(GetTextResource(IDR_LICENSE, nullptr).c_str());
    m_textLicense.SetFont(&m_monoFont);

    // Set default rich edit settings
    CHARFORMAT2 charFormat = {{.cbSize = sizeof(CHARFORMAT2)}};
    charFormat.dwMask = CFM_COLOR;
    charFormat.crTextColor = DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
    const auto bgColor = DarkMode::WdsSysColor(COLOR_WINDOW);

    for (auto ctrl : { &m_textAbout, &m_textThanks, &m_textLicense })
    {
        ctrl->SetDefaultCharFormat(charFormat);
        ctrl->SetAutoURLDetect();
        ctrl->SetBackgroundColor(FALSE, bgColor);
    }
}

CRichEditCtrl& CAboutDlg::WdsTabControl::GetActiveRichEdit()
{
    const auto tabIndex = GetActiveTab();
    return tabIndex == TAB_ABOUT ? m_textAbout : 
           tabIndex == TAB_THANKSTO ? m_textThanks : 
           m_textLicense;
}

void CAboutDlg::WdsTabControl::ClearSelectionCursor()
{
    auto& active = GetActiveRichEdit();
    active.SetSel(0, 0);
    active.HideCaret();
    active.HideSelection(TRUE, FALSE);
}

bool CAboutDlg::WdsTabControl::HandleTabKey(bool shiftPressed)
{
    const int activeTab = GetActiveTab();
    const int tabCount = GetTabsNum();

    if (shiftPressed)
    {
        if (activeTab > 0)
        {
            SetActiveTab(activeTab - 1);
            return true;
        }
    }
    else if (activeTab < tabCount - 1)
    {
        SetActiveTab(activeTab + 1);
        return true;
    }

    // If we reach here, move focus to OK button
    GetParent()->GetDlgItem(IDOK)->SetFocus();
    return true;
}

BEGIN_MESSAGE_MAP(CAboutDlg::WdsTabControl, CMFCTabCtrl)
    ON_NOTIFY(EN_LINK, ID_WDS_CONTROL, OnEnLinkText)
    ON_NOTIFY(EN_MSGFILTER, ID_WDS_CONTROL, OnEnMsgFilter)
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

void CAboutDlg::WdsTabControl::OnSetFocus(CWnd* pOldWnd)
{
    CMFCTabCtrl::OnSetFocus(pOldWnd);
    
    // Hide the caret in the active RichEdit control
    ClearSelectionCursor();
}

void CAboutDlg::WdsTabControl::OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult)
{
    const ENLINK* el = reinterpret_cast<ENLINK*>(pNMHDR);
    *pResult = 0;

    if (el->msg == WM_LBUTTONDOWN)
    {
        CStringW link;
        auto& active = GetActiveRichEdit();
        active.GetTextRange(el->chrg.cpMin, el->chrg.cpMax, link);
        ::ShellExecute(*this, nullptr, link, nullptr, wds::strEmpty, SW_SHOWNORMAL);
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
            HandleTabKey(IsShiftKeyDown());
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
    : CLayoutDialogEx(IDD_ABOUTBOX, COptions::AboutWindowRect.Ptr())
{
}

std::wstring CAboutDlg::GetAppVersion()
{
    const std::wstring file = GetAppFileName();
    const DWORD iVersionSize = GetFileVersionInfoSize(file.c_str(), nullptr);
    if (iVersionSize == 0)
    {
        return wds::strWinDirStat;
    }

    auto tVersionInfo = std::vector<BYTE>(iVersionSize);
    VS_FIXEDFILEINFO* pVersion = nullptr;
    UINT iQueriedSize = 0;

    if (GetFileVersionInfo(file.c_str(), 0, iVersionSize, tVersionInfo.data()) != 0 &&
        VerQueryValue(tVersionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&pVersion), &iQueriedSize) != 0)
    {
        return std::format(L"{} {}{}.{}.{} ({})\nGit Commit: {}",
            wds::strWinDirStat, PRODUCTION == 0 ? L"Beta " : L"",
            HIWORD(pVersion->dwFileVersionMS),
            LOWORD(pVersion->dwFileVersionMS),
            HIWORD(pVersion->dwFileVersionLS),
            _CRT_WIDE(GIT_DATE), _CRT_WIDE(GIT_COMMIT));
    }

    return wds::strWinDirStat;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CAPTION, m_caption);
    DDX_Control(pDX, IDC_TAB, m_tab);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CLayoutDialogEx)
    ON_WM_CTLCOLOR()
    ON_REGISTERED_MESSAGE(AFX_WM_CHANGE_ACTIVE_TAB, OnTabChanged)
END_MESSAGE_MAP()

BOOL CAboutDlg::OnInitDialog()
{
    CLayoutDialogEx::OnInitDialog();

    // Re-create the tab control
    CWnd* placeholderTabCtrl = GetDlgItem(IDC_TAB);
    CRect placeholderRect;
    placeholderTabCtrl->GetWindowRect(&placeholderRect);
    ScreenToClient(&placeholderRect);
    placeholderTabCtrl->DestroyWindow();
    m_tab.Create(CMFCTabCtrl::STYLE_FLAT, placeholderRect, this, IDC_TAB);
    Localization::UpdateDialogs(*this);

    m_layout.AddControl(IDC_CAPTION, 0.5, 0, 0, 0);
    m_layout.AddControl(IDC_TAB, 0, 0, 1, 1);
    m_layout.AddControl(IDOK, 0.5, 1, 0, 0);
    m_layout.OnInitDialog(true);

    m_tab.Initialize();

    m_caption.SetWindowText(GetAppVersion().c_str());

    DarkMode::AdjustControls(GetSafeHwnd());

    // Set initial focus to tab control
    m_tab.SetFocus();
    return FALSE;
}

LRESULT CAboutDlg::OnTabChanged(WPARAM, LPARAM)
{
    m_tab.ClearSelectionCursor();
    return 0;
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CAboutDlg::PreTranslateMessage(MSG* pMsg)
{
    // Handle tab key when focus is on OK button
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    {
        if (GetFocus() == GetDlgItem(IDOK))
        {
            m_tab.SetActiveTab(IsShiftKeyDown() ? m_tab.GetTabsNum() - 1 : 0);
            m_tab.SetFocus();
            return TRUE;
        }
        
        // Force showing focus rectangles
        SendMessage(WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS));
    }
    
    return CLayoutDialogEx::PreTranslateMessage(pMsg);
}
