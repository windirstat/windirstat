// AboutDlg.cpp - Implementation of the StartAboutDialog() function
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

#include "stdafx.h"
#include "WinDirStat.h"
#include <Constants.h>
#include "MdExceptions.h"
#include "CommonHelpers.h"
#include "AboutDlg.h"
#include "Localization.h"
#include "Options.h"
#include "GlobalHelpers.h"

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
        std::wstring s;

        HGLOBAL hresource = nullptr;
        try
        {
            const HRSRC hrsrc = ::FindResource(dll, MAKEINTRESOURCE(id), L"TEXT");
            if (nullptr == hrsrc)
            {
                MdThrowLastWinerror();
            }

            const DWORD dwSize = SizeofResource(dll, hrsrc);
            if (0 == dwSize)
            {
                MdThrowLastWinerror();
            }

            hresource = LoadResource(dll, hrsrc);
            if (!hresource)
            {
                MdThrowLastWinerror();
            }

            const auto pData = static_cast<const BYTE*>(LockResource(hresource));

            const CComBSTR bstr(dwSize, reinterpret_cast<LPCSTR>(pData));

            s = bstr;
        }
        catch (CException* pe)
        {
            pe->ReportError();
            pe->Delete();
        }

        if (hresource != nullptr)
        {
            FreeResource(hresource);
        }

        return s;
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

void CAboutDlg::CMyTabControl::Initialize()
{
    ModifyStyle(0, WS_CLIPCHILDREN);

    InsertItem(TAB_ABOUT, Localization::Lookup(IDS_ABOUT_ABOUT).c_str());
    InsertItem(TAB_THANKSTO, Localization::Lookup(IDS_ABOUT_THANKSTO).c_str());
    InsertItem(TAB_LICENSE, Localization::Lookup(IDS_ABOUT_LICENSEAGREEMENT).c_str());

    CRect rc;
    GetClientRect(rc);

    CRect rcItem;
    GetItemRect(0, rcItem);

    rc.top = rcItem.bottom;

    VERIFY(m_Text.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_MULTILINE | ES_READONLY, rc, this, ID_WDS_CONTROL));
    SetPageText(TAB_ABOUT);
}

void CAboutDlg::CMyTabControl::SetPageText(const int tab)
{
    std::wstring text;
    DWORD newStyle = ES_CENTER;

    switch (tab)
    {
    case TAB_ABOUT:
        {
            text = Localization::Format(IDS_ABOUT_ABOUTTEXTss,
                Localization::LookupNeutral(IDS_AUTHOR_EMAIL),
                Localization::LookupNeutral(IDS_URL_WEBSITE));
        }
        break;
    case TAB_THANKSTO:
        {
            text = Localization::Lookup(IDS_ABOUT_THANKSTOTEXT);
        }
        break;
    case TAB_LICENSE:
        {
            text = GetTextResource(IDR_LICENSE, nullptr);
            newStyle = ES_LEFT;
        }
        break;
    default:
        {
            ASSERT(FALSE);
        }
        break;
    }
    CRect rc;
    m_Text.GetWindowRect(rc);
    ScreenToClient(rc);

    DWORD style = m_Text.GetStyle();
    style &= ~ES_CENTER;
    style |= newStyle | WS_VSCROLL;

    const DWORD exstyle = m_Text.GetExStyle();

    m_Text.DestroyWindow();

    m_Text.Create(style, rc, this, ID_WDS_CONTROL);
    if (exstyle)
    {
        m_Text.ModifyStyleEx(0, exstyle);
    }

    m_Text.SetAutoURLDetect();
    m_Text.SetEventMask(ENM_LINK | ENM_KEYEVENTS);
    m_Text.SetFont(GetFont());

    m_Text.SetWindowText(text.c_str());

    m_Text.HideCaret();
}

BEGIN_MESSAGE_MAP(CAboutDlg::CMyTabControl, CTabCtrl)
    ON_NOTIFY(EN_LINK, ID_WDS_CONTROL, OnEnLinkText)
    ON_NOTIFY(EN_MSGFILTER, ID_WDS_CONTROL, OnEnMsgFilter)
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CAboutDlg::CMyTabControl::OnEnLinkText(NMHDR* pNMHDR, LRESULT* pResult)
{
    const ENLINK* el = reinterpret_cast<ENLINK*>(pNMHDR);
    *pResult         = 0;

    if (WM_LBUTTONDOWN == el->msg)
    {
        CStringW link;
        m_Text.GetTextRange(el->chrg.cpMin, el->chrg.cpMax, link);

        // FIXME: should probably one of the helper variants of this function
        ::ShellExecute(*this, nullptr, link, nullptr, wds::strEmpty, SW_SHOWNORMAL);
    }
}

void CAboutDlg::CMyTabControl::OnEnMsgFilter(NMHDR* pNMHDR, LRESULT* pResult)
{
    const MSGFILTER* mf = reinterpret_cast<MSGFILTER*>(pNMHDR);
    *pResult            = 0;

    if (WM_KEYDOWN == mf->msg && (VK_ESCAPE == mf->wParam || VK_TAB == mf->wParam))
    {
        // Move the focus back to the Tab control
        SetFocus();

        // If we didn't ignore VK_ESCAPE here, strange things happen:
        // both m_Text and the Tab control would disappear.
        *pResult = 1;
    }
}

void CAboutDlg::CMyTabControl::OnSize(const UINT nType, const int cx, const int cy)
{
    CTabCtrl::OnSize(nType, cx, cy);

    if (IsWindow(m_Text.m_hWnd))
    {
        CRect rc;
        GetClientRect(rc);

        CRect rcItem;
        GetItemRect(0, rcItem);

        rc.top = rcItem.bottom;

        m_Text.MoveWindow(rc);
    }
}

////////////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg()
    : CDialogEx(IDD)
      , m_Layout(this, COptions::AboutWindowRect.Ptr())
{
}

std::wstring CAboutDlg::GetAppVersion()
{
    const std::wstring file = GetAppFileName();
    const DWORD iVersionSize = GetFileVersionInfoSize(file.c_str(), nullptr);
    UINT iQueriedSize = 0;
    std::vector<BYTE> tVersionInfo = std::vector<BYTE>(iVersionSize);
    VS_FIXEDFILEINFO* pVersion = nullptr;
    if (GetFileVersionInfo(file.c_str(), 0, iVersionSize, tVersionInfo.data()) != 0 &&
        VerQueryValue(tVersionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&pVersion), &iQueriedSize) != 0)
    {
        return std::format(L"WinDirStat {}.{}.{} ({})\nGit Commit: {}",
            std::to_wstring(HIWORD(pVersion->dwFileVersionMS)),
            std::to_wstring(LOWORD(pVersion->dwFileVersionMS)),
            std::to_wstring(HIWORD(pVersion->dwFileVersionLS)),
            _CRT_WIDE(GIT_DATE), _CRT_WIDE(GIT_COMMIT));
    }

    return L"WinDirStat";
}

std::wstring CAboutDlg::GetDevelList()
{
    std::wstring retval;
    return retval;
}

std::wstring CAboutDlg::GetTranslatorList()
{
    std::wstring retval;
    return retval;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CAPTION, m_Caption);
    DDX_Control(pDX, IDC_TAB, m_Tab);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, OnTcnSelchangeTab)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
END_MESSAGE_MAP()
#pragma warning(pop)

BOOL CAboutDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_Layout.AddControl(IDC_CAPTION, 0.5, 0, 0, 0);
    m_Layout.AddControl(IDC_TAB, 0, 0, 1, 1);
    m_Layout.AddControl(IDOK, 0.5, 1, 0, 0);

    m_Layout.OnInitDialog(true);

    m_Tab.Initialize();
    m_Caption.SetWindowText(GetAppVersion().c_str());

    return TRUE;
}

void CAboutDlg::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = FALSE;
    m_Tab.SetPageText(m_Tab.GetCurSel());
}

void CAboutDlg::OnSize(const UINT nType, const int cx, const int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    m_Layout.OnSize();
}

void CAboutDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_Layout.OnGetMinMaxInfo(lpMMI);
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CAboutDlg::OnDestroy()
{
    m_Layout.OnDestroy();
    CDialogEx::OnDestroy();
}
