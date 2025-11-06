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

static const std::unordered_map<UINT, ButtonContexts> buttonTypeContexts
{
    // m_ButtonType          btnLeftSW btMidSW   btnRightSW  btnLeftID  btnMidID   btnRightID  btnLeftIDS         btnMidIDS          btnRightIDS
    { MB_OK,               { SW_HIDE,  SW_HIDE,  SW_SHOW,    0,         0,         IDOK,       IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_OK     } },
    { MB_OKCANCEL,         { SW_HIDE,  SW_SHOW,  SW_SHOW,    0,         IDOK,      IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_OK,    IDS_GENERIC_CANCEL } },
    { MB_YESNO,            { SW_HIDE,  SW_SHOW,  SW_SHOW,    0,         IDYES,     IDNO,       IDS_GENERIC_BLANK, IDS_GENERIC_YES,   IDS_GENERIC_NO     } },
    { MB_YESNOCANCEL,      { SW_SHOW,  SW_SHOW,  SW_SHOW,    IDYES,     IDNO,      IDCANCEL,   IDS_GENERIC_YES,   IDS_GENERIC_NO,    IDS_GENERIC_CANCEL } },
    // these MB types are not used by WinDirStat, but included for completeness and using IDS_GENERIC_BLANK as placeholder for button labels,
    // please add required IDS to localization engine upon use
    { MB_RETRYCANCEL,      { SW_HIDE,  SW_SHOW,  SW_SHOW,    0,         IDRETRY,   IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_CANCEL } },
    { MB_ABORTRETRYIGNORE, { SW_SHOW,  SW_SHOW,  SW_SHOW,    IDABORT,   IDRETRY,   IDIGNORE,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_BLANK  } },
};

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
    ASSERT(buttonTypeContexts.contains(m_ButtonType));
    m_buttonTypeContext = buttonTypeContexts.at(m_ButtonType);
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

    // Configure buttons
    m_ButtonLeft.ShowWindow(m_buttonTypeContext.btnLeftSW);
    m_ButtonMiddle.ShowWindow(m_buttonTypeContext.btnMidSW);
    m_ButtonRight.ShowWindow(m_buttonTypeContext.btnRightSW);

    // Set button texts
    m_ButtonLeft.SetWindowText(Localization::Lookup(m_buttonTypeContext.btnLeftIDS).c_str());
    m_ButtonMiddle.SetWindowText(Localization::Lookup(m_buttonTypeContext.btnMidIDS).c_str());
    m_ButtonRight.SetWindowText(Localization::Lookup(m_buttonTypeContext.btnRightIDS).c_str());

    // Set focus to default button
    CWnd* pFocusButton = nullptr;

    switch (m_ButtonType)
    {
    case MB_YESNOCANCEL:
    case MB_ABORTRETRYIGNORE:
        pFocusButton = &m_ButtonLeft;
        break;

    case MB_YESNO:
    case MB_OKCANCEL:
    case MB_RETRYCANCEL:
        pFocusButton = &m_ButtonMiddle;
        break;

    case MB_OK:
    default:
        pFocusButton = &m_ButtonRight;
        break;
    }

    pFocusButton->SetFocus();

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
    EndDialog(m_buttonTypeContext.btnLeftID);
}

void CMessageBoxDlg::OnButtonMiddle()
{
    EndDialog(m_buttonTypeContext.btnMidID);
}

void CMessageBoxDlg::OnButtonRight()
{
    EndDialog(m_buttonTypeContext.btnRightID);
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
