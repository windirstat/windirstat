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
    CTabCtrlHelper::SetupTabControl(*this);

    // Calculate initial client area
    auto createText = [&](CRichEditCtrl& ctrl, const DWORD align = ES_CENTER)
    {
        VERIFY(ctrl.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | align,
            CRect(), this, ID_WDS_CONTROL));
        ctrl.SetEventMask(ENM_LINK | ENM_KEYEVENTS);
        return &ctrl;
    };

    // Create all three pages and add them as tabs
    AddTab(createText(m_TextAbout), Localization::Lookup(IDS_ABOUT_ABOUT).c_str(), TAB_ABOUT);
    AddTab(createText(m_TextThanks), Localization::Lookup(IDS_ABOUT_THANKS).c_str(), TAB_THANKSTO);
    AddTab(createText(m_TextLicense, ES_LEFT), Localization::Lookup(IDS_ABOUT_LICENSE).c_str(), TAB_LICENSE);

    // Use monospace font for license page
    m_MonoFont.CreateFontW(12, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, FF_MODERN, L"Consolas");

    // Populate text
    const auto about = Localization::Format(IDS_ABOUT_ABOUT_TEXTss,
        Localization::LookupNeutral(IDS_AUTHOR_EMAIL),
        Localization::LookupNeutral(IDS_URL_WEBSITE));
    m_TextAbout.SetWindowText(about.c_str());

    const auto thanks = Localization::Lookup(IDS_ABOUT_THANKS_TEXT);
    m_TextThanks.SetWindowText(thanks.c_str());

    const auto license = GetTextResource(IDR_LICENSE, nullptr);
    m_TextLicense.SetWindowText(license.c_str());
    m_TextLicense.SetFont(&m_MonoFont);

    // Set default rich edit settings
    for (auto ctrl : { &m_TextAbout, &m_TextThanks, &m_TextLicense })
    {
        CHARFORMAT2 charFormat = { {} };
        charFormat.cbSize = sizeof(CHARFORMAT2);
        charFormat.dwMask = CFM_COLOR;
        charFormat.crTextColor = DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
        ctrl->SetDefaultCharFormat(charFormat);
        ctrl->SetAutoURLDetect();
        ctrl->SetBackgroundColor(FALSE, DarkMode::WdsSysColor(COLOR_WINDOW));
    }
}

void CAboutDlg::WdsTabControl::ClearSelectionCursor()
{
    const auto tabIndex = GetActiveTab();
    auto& active = tabIndex == TAB_ABOUT ? m_TextAbout : tabIndex == TAB_THANKSTO ? m_TextThanks : m_TextLicense;
    active.SetSel(0, 0);
    active.HideCaret();
}

BEGIN_MESSAGE_MAP(CAboutDlg::WdsTabControl, CMFCTabCtrl)
    ON_NOTIFY(EN_LINK, ID_WDS_CONTROL, OnEnLinkText)
    ON_NOTIFY(EN_MSGFILTER, ID_WDS_CONTROL, OnEnMsgFilter)
END_MESSAGE_MAP()

void CAboutDlg::WdsTabControl::OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult)
{
    const ENLINK* el = reinterpret_cast<ENLINK*>(pNMHDR);
    *pResult = 0;

    if (WM_LBUTTONDOWN == el->msg)
    {
        CStringW link;
        const auto& active = GetActiveTab() == TAB_ABOUT ? m_TextAbout : GetActiveTab() == TAB_THANKSTO ? m_TextThanks : m_TextLicense;
        active.GetTextRange(el->chrg.cpMin, el->chrg.cpMax, link);
        ::ShellExecute(*this, nullptr, link, nullptr, wds::strEmpty, SW_SHOWNORMAL);
    }
}

void CAboutDlg::WdsTabControl::OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult)
{
    const MSGFILTER* mf = reinterpret_cast<MSGFILTER*>(pNMHDR);
    *pResult = 0;

    if (WM_KEYDOWN == mf->msg && (VK_ESCAPE == mf->wParam || VK_TAB == mf->wParam))
    {
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
    UINT iQueriedSize = 0;
    auto tVersionInfo = std::vector<BYTE>(iVersionSize);
    VS_FIXEDFILEINFO* pVersion = nullptr;
    if (GetFileVersionInfo(file.c_str(), 0, iVersionSize, tVersionInfo.data()) != 0 &&
        VerQueryValue(tVersionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&pVersion), &iQueriedSize) != 0)
    {
        return std::format(L"WinDirStat {}{}.{}.{} ({})\nGit Commit: {}",
            PRODUCTION == 0 ? L"Beta " : L"",
            std::to_wstring(HIWORD(pVersion->dwFileVersionMS)),
            std::to_wstring(LOWORD(pVersion->dwFileVersionMS)),
            std::to_wstring(HIWORD(pVersion->dwFileVersionLS)),
            _CRT_WIDE(GIT_DATE), _CRT_WIDE(GIT_COMMIT));
    }

    return L"WinDirStat";
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CAPTION, m_Caption);
    DDX_Control(pDX, IDC_TAB, m_Tab);
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
    m_Tab.Create(CMFCTabCtrl::STYLE_FLAT, placeholderRect, this, IDC_TAB);
    Localization::UpdateDialogs(*this);

    m_Layout.AddControl(IDC_CAPTION, 0.5, 0, 0, 0);
    m_Layout.AddControl(IDC_TAB, 0, 0, 1, 1);
    m_Layout.AddControl(IDOK, 0.5, 1, 0, 0);

    m_Layout.OnInitDialog(true);

    m_Tab.Initialize();
    m_Caption.SetWindowText(GetAppVersion().c_str());

    DarkMode::AdjustControls(GetSafeHwnd());

    return TRUE;
}

LRESULT CAboutDlg::OnTabChanged(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_Tab.ClearSelectionCursor();
    return 0;
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}
