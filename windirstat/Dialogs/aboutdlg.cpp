// aboutdlg.cpp - Implementation of the StartAboutDialog() function
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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
#include "windirstat.h"
#include <common/wds_constants.h>
#include "aboutdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    enum
    {
        RE_CONTROL = 4711   // Id of the RichEdit Control
    };

    // Tabs
    enum
    {
        TAB_ABOUT,
        TAB_AUTHORS,
        TAB_THANKSTO,
        TAB_LICENSE
    };

    // Retrieve the GPL text from our resources
    CString GetTextResource(UINT id, HMODULE dll = AfxGetResourceHandle())
    {
        CString s;

        HGLOBAL hresource = NULL;
        try
        {
            HRSRC hrsrc = ::FindResource(dll, MAKEINTRESOURCE(id), _T("TEXT"));
            if(NULL == hrsrc)
            {
                MdThrowLastWinerror();
            }

            DWORD dwSize = ::SizeofResource(dll, hrsrc);
            if(0 == dwSize)
            {
                MdThrowLastWinerror();
            }

            hresource = ::LoadResource(dll, hrsrc);
            const BYTE *pData = (const BYTE *)::LockResource(hresource);

            CComBSTR bstr(dwSize, (LPCSTR)pData);

            s = bstr;
        }
        catch (CException *pe)
        {
            pe->ReportError();
            pe->Delete();
        }

        if(hresource != NULL)
        {
            ::FreeResource(hresource);
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

    InsertItem(TAB_ABOUT, LPCTSTR(LoadString(IDS_ABOUT_ABOUT)));
    InsertItem(TAB_AUTHORS, LPCTSTR(LoadString(IDS_ABOUT_AUTHORS)));
    InsertItem(TAB_THANKSTO, LPCTSTR(LoadString(IDS_ABOUT_THANKSTO)));
    InsertItem(TAB_LICENSE, LPCTSTR(LoadString(IDS_ABOUT_LICENSEAGREEMENT)));

    CRect rc;
    GetClientRect(rc);

    CRect rcItem;
    GetItemRect(0, rcItem);

    rc.top = rcItem.bottom;

    VERIFY(m_text.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_MULTILINE | ES_READONLY, rc, this, RE_CONTROL));
    SetPageText(TAB_ABOUT);
}

void CAboutDlg::CMyTabControl::SetPageText(int tab)
{
    CString text;
    DWORD newStyle = ES_CENTER;

    switch (tab)
    {
    case TAB_ABOUT:
        {
            text.FormatMessage(IDS_ABOUT_ABOUTTEXTss, GetAuthorEmail().GetString(), GetWinDirStatHomepage().GetString());
        }
        break;
    case TAB_AUTHORS:
        {
            CString translators;
            text.FormatMessage(IDS_ABOUT_AUTHORSTEXTs, GetDevelList().GetString());
            text += GetTranslatorList();
        }
        break;
    case TAB_THANKSTO:
        {
            text.LoadString(IDS_ABOUT_THANKSTOTEXT);
        }
        break;
    case TAB_LICENSE:
        {
            text = GetTextResource(IDR_LICENSE, NULL);
            newStyle = ES_LEFT;
        }
        break;
    default:
        {
            ASSERT(0);
        }
        break;
    }
    CRect rc;
    m_text.GetWindowRect(rc);
    ScreenToClient(rc);

    DWORD style = m_text.GetStyle();
    style &= ~ES_CENTER;
    style |= newStyle | WS_VSCROLL;

    DWORD exstyle = m_text.GetExStyle();

    m_text.DestroyWindow();

    m_text.Create(style, rc, this, RE_CONTROL);
    if(exstyle)
    {
        m_text.ModifyStyleEx(0, exstyle);
    }

    m_text.SetAutoURLDetect();
    m_text.SetEventMask(ENM_LINK | ENM_KEYEVENTS);
    m_text.SetFont(GetFont());

    m_text.SetWindowText(text);

    m_text.HideCaret();
}

BEGIN_MESSAGE_MAP(CAboutDlg::CMyTabControl, CTabCtrl)
    ON_NOTIFY(EN_LINK, RE_CONTROL, OnEnLinkText)
    ON_NOTIFY(EN_MSGFILTER, RE_CONTROL, OnEnMsgFilter)
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CAboutDlg::CMyTabControl::OnEnLinkText(NMHDR *pNMHDR, LRESULT *pResult)
{
    ENLINK *el = reinterpret_cast<ENLINK *>(pNMHDR);
    *pResult = 0;

    if(WM_LBUTTONDOWN == el->msg)
    {
        CString link;
        m_text.GetTextRange(el->chrg.cpMin, el->chrg.cpMax, link);

        // FIXME: should probably one of the helper variants of this function
        ::ShellExecute(*this, NULL, link, NULL, wds::strEmpty, SW_SHOWNORMAL);
    }
}

void CAboutDlg::CMyTabControl::OnEnMsgFilter(NMHDR *pNMHDR, LRESULT *pResult)
{
    MSGFILTER *mf = reinterpret_cast<MSGFILTER *>(pNMHDR);
    *pResult = 0;

    if(WM_KEYDOWN == mf->msg && (VK_ESCAPE == mf->wParam || VK_TAB == mf->wParam))
    {
        // Move the focus back to the Tab control
        SetFocus();

        // If we didn't ignore VK_ESCAPE here, strange things happen:
        // both m_text and the Tab control would disappear.
        *pResult = 1;
    }
}

void CAboutDlg::CMyTabControl::OnSize(UINT nType, int cx, int cy)
{
    CTabCtrl::OnSize(nType, cx, cy);

    if(::IsWindow(m_text.m_hWnd))
    {
        CRect rc;
        GetClientRect(rc);

        CRect rcItem;
        GetItemRect(0, rcItem);

        rc.top = rcItem.bottom;

        m_text.MoveWindow(rc);
    }
}


////////////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg()
    : CDialog(CAboutDlg::IDD)
    , m_layout(this, _T("aboutdlg"))
{
}

CString CAboutDlg::GetAppVersion()
{
    CString s;
    s.Format(_T("WinDirStat %s"), _T("1.x.y.z")); // FIXME
    return s;
}

CString CAboutDlg::GetDevelList()
{
    CString retval;
    using wds::authors;
    using wds::contact_t;
    
    for(size_t i = 0; authors[i].name; i++)
    {
        contact_t* c = &authors[i];
        if(c->name)
        {
            CString tmp;
            if(c->mail && c->weburl)
                tmp.Format(_T("\r\n%s\r\n(mailto:%s)\r\n%s\r\n"), c->name, c->mail, c->weburl);
            else if(c->mail)
                tmp.Format(_T("\r\n%s\r\n(mailto:%s)\r\n"), c->name, c->mail);
            else if(c->weburl)
                tmp.Format(_T("\r\n%s\r\n%s\r\n"), c->name, c->weburl);
            else
                tmp.Format(_T("\r\n%s\r\n"), c->name);
            if(!tmp.IsEmpty())
            {
                retval += tmp;
            }
        }
    }
    return retval;
}

CString CAboutDlg::GetTranslatorList()
{
    CString retval;
    using wds::translators;
    using wds::translator_t;

    for(size_t i = 0; translators[i].id && translators[i].lngNative; i++)
    {
        translator_t* t = &translators[i];
        if(t->lngNative && t->lngEnglish && t->lngISO639_1)
        {
            CString tmp;
            tmp.Format(_T("--- %s/%s (%s) ---\n\n"), t->lngNative, t->lngEnglish, t->lngISO639_1);
            for(size_t j = 0; t->translators[j].name; j++)
            {
                CString tmp2;
                const wds::contact_t& c = t->translators[j];
                if(c.mail && c.weburl)
                    tmp2.Format(_T("%s\n(mailto:%s)\n%s\n"), c.name, c.mail, c.weburl);
                else if(c.mail)
                    tmp2.Format(_T("%s\n(mailto:%s)\n"), c.name, c.mail);
                else if(c.weburl)
                    tmp2.Format(_T("%s\n%s\n"), c.name, c.weburl);
                else
                    tmp2.Format(_T("%s\n"), c.name);
                if(!tmp2.IsEmpty())
                    tmp += tmp2;
            }
            if(!tmp.IsEmpty())
            {
                tmp += _T("\n");
                retval += tmp;
            }
        }
    }
    return retval;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CAPTION, m_caption);
    DDX_Control(pDX, IDC_TAB, m_tab);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, OnTcnSelchangeTab)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CAboutDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_layout.AddControl(IDC_CAPTION,    0.5, 0, 0, 0);
    m_layout.AddControl(IDC_TAB,        0,   0, 1, 1);
    m_layout.AddControl(IDOK,           0.5, 1, 0, 0);

    m_layout.OnInitDialog(true);

    m_tab.Initialize();
    m_caption.SetWindowText(GetAppVersion());

    return true;
}

void CAboutDlg::OnTcnSelchangeTab(NMHDR * /* pNMHDR */, LRESULT *pResult)
{
    *pResult = 0;
    m_tab.SetPageText(m_tab.GetCurSel());
}

void CAboutDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    m_layout.OnSize();
}


void CAboutDlg::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
    m_layout.OnGetMinMaxInfo(mmi);
    CDialog::OnGetMinMaxInfo(mmi);
}

void CAboutDlg::OnDestroy()
{
    m_layout.OnDestroy();
    CDialog::OnDestroy();
}
