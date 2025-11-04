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

#include "stdafx.h"
#include "WinDirStat.h"
#include "MessageBoxDlg.h"
#include "DarkMode.h"
#include "Localization.h"

IMPLEMENT_DYNAMIC(CMessageBoxDlg, CDialogEx)

CMessageBoxDlg::CMessageBoxDlg(HWND wnd, const std::wstring& message, const std::wstring& title, const UINT type, CWnd* pParent)
    : CDialogEx(IDD, pParent)
    , m_Message(message)
    , m_Title(title)
    , m_ButtonType(type & MB_TYPEMASK)
    , m_IconType(type& MB_ICONMASK)
    , m_hIcon(nullptr)
    , m_Hwnd(wnd)
{
}

void CMessageBoxDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MESSAGE_ICON, m_IconCtrl);
    DDX_Control(pDX, IDC_MESSAGE_TEXT, m_MessageCtrl);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONLEFT, m_ButtonLeft);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONMIDDLE, m_ButtonMiddle);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONRIGHT, m_ButtonRight);
}

BEGIN_MESSAGE_MAP(CMessageBoxDlg, CDialogEx)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONLEFT, OnButtonLeft)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONMIDDLE, OnButtonMiddle)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONRIGHT, OnButtonRight)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CMessageBoxDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Set window title and message
    SetWindowText(m_Title.c_str());
    m_MessageCtrl.SetWindowText(m_Message.c_str());

    // Configure buttons based on message box type
    m_ButtonLeft.ShowWindow(SW_HIDE);
    m_ButtonMiddle.ShowWindow(SW_HIDE);
    m_ButtonRight.ShowWindow(SW_HIDE);

    switch (m_ButtonType)
    {
    case MB_OKCANCEL:
        m_ButtonMiddle.SetWindowText(Localization::Lookup(IDS_GENERIC_OK).c_str());
        m_ButtonMiddle.ShowWindow(SW_SHOW);
        m_ButtonRight.SetWindowText(Localization::Lookup(IDS_GENERIC_CANCEL).c_str());
        m_ButtonRight.ShowWindow(SW_SHOW);
        m_ButtonMiddle.SetFocus();
        break;

    case MB_YESNO:
        m_ButtonMiddle.SetWindowText(Localization::Lookup(IDS_GENERIC_YES).c_str());
        m_ButtonMiddle.ShowWindow(SW_SHOW);
        m_ButtonRight.SetWindowText(Localization::Lookup(IDS_GENERIC_NO).c_str());
        m_ButtonRight.ShowWindow(SW_SHOW);
        m_ButtonMiddle.SetFocus();
        break;

    case MB_YESNOCANCEL:
        m_ButtonLeft.SetWindowText(Localization::Lookup(IDS_GENERIC_YES).c_str());
        m_ButtonLeft.ShowWindow(SW_SHOW);
        m_ButtonMiddle.SetWindowText(Localization::Lookup(IDS_GENERIC_NO).c_str());
        m_ButtonMiddle.ShowWindow(SW_SHOW);
        m_ButtonRight.SetWindowText(Localization::Lookup(IDS_GENERIC_CANCEL).c_str());
        m_ButtonRight.ShowWindow(SW_SHOW);
        m_ButtonLeft.SetFocus();
        break;

    default: // MB_OK
        m_ButtonRight.SetWindowText(Localization::Lookup(IDS_GENERIC_OK).c_str());
        m_ButtonRight.ShowWindow(SW_SHOW);
        m_ButtonRight.SetFocus();
        break;
    }

    // Set icon based on message box type
    LPCWSTR iconResource = IDI_INFORMATION;
    switch (m_IconType)
    {
    case MB_ICONERROR: // MB_ICONSTOP has the same value
        iconResource = IDI_ERROR;
        break;

    case MB_ICONQUESTION:
        iconResource = IDI_QUESTION;
        break;

    case MB_ICONWARNING: // MB_ICONEXCLAMATION has the same value
        iconResource = IDI_WARNING;
        break;

    default: // MB_ICONINFORMATION
        iconResource = IDI_INFORMATION;
        break;
    }
    
    m_hIcon = LoadIcon(nullptr, iconResource);
    if (m_hIcon != nullptr)
    {
        m_IconCtrl.SetIcon(m_hIcon);
    }

    // Apply dark mode
    DarkMode::AdjustControls(*this);

    // Auto-size dialog to fit content
    CRect rectMessage;
    m_MessageCtrl.GetWindowRect(&rectMessage);
    ScreenToClient(&rectMessage);

    CDC* pDC = m_MessageCtrl.GetDC();
    CRect rectText(0, 0, rectMessage.Width(), 0);
    pDC->DrawText(m_Message.c_str(), &rectText, DT_CALCRECT | DT_WORDBREAK);
    m_MessageCtrl.ReleaseDC(pDC);

    const int deltaHeight = rectText.Height() - rectMessage.Height();
    if (deltaHeight > 0)
    {
        CRect rectDlg;
        GetWindowRect(&rectDlg);
        SetWindowPos(nullptr, 0, 0, rectDlg.Width(), rectDlg.Height() + deltaHeight,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    // Center dialog
    CenterWindow();

    return TRUE;
}

void CMessageBoxDlg::OnButtonLeft()
{
    EndDialog(IDYES);
}

void CMessageBoxDlg::OnButtonMiddle()
{
    if (m_ButtonType == MB_OKCANCEL)
    {
        EndDialog(IDOK);
    }
    else if (m_ButtonType == MB_YESNO)
    {
        EndDialog(IDYES);
    }
    else if (m_ButtonType == MB_YESNOCANCEL)
    {
        EndDialog(IDNO);
    }
}

void CMessageBoxDlg::OnButtonRight()
{
    if (m_ButtonType == MB_OKCANCEL)
    {
        EndDialog(IDCANCEL);
    }
    else if (m_ButtonType == MB_YESNO)
    {
        EndDialog(IDNO);
    }
    else if (m_ButtonType == MB_YESNOCANCEL)
    {
        EndDialog(IDCANCEL);
    }
    else if (m_ButtonType == MB_OK)
    {
        EndDialog(IDOK);
    }
}

INT_PTR CMessageBoxDlg::DoModal()
{
    return CDialogEx::DoModal();
}

HBRUSH CMessageBoxDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

// Global wrapper functions
int WdsMessageBox(const std::wstring& message, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return AfxMessageBox(message.c_str(), type);
    }
    
    return WdsMessageBox(nullptr, message, Localization::Lookup(IDS_APP_TITLE), type);
}

int WdsMessageBox(HWND wnd, const std::wstring& message, const std::wstring& title, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return MessageBox(wnd, message.c_str(), title.c_str(), type);
    }

    CMessageBoxDlg dlg(wnd, message, title, type);
    return static_cast<int>(dlg.DoModal());
}
