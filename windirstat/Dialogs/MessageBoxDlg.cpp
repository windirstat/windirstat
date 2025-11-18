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

IMPLEMENT_DYNAMIC(CMessageBoxDlg, CLayoutDialogEx)

CMessageBoxDlg::CMessageBoxDlg(const std::wstring& message, const std::wstring& title, const UINT type, CWnd* pParent,
    const std::vector<std::wstring>& listViewItems, const std::wstring& checkBoxText, const bool checkBoxValue)
    : CLayoutDialogEx(IDD_MESSAGEBOX, &m_WindowRect, pParent)
    , m_Message(message)
    , m_Title(title)
    , m_CheckboxText(checkBoxText)
    , m_ListViewItems(listViewItems)
    , m_CheckboxChecked(checkBoxValue ? TRUE : FALSE)
{
    const std::unordered_map<UINT, ButtonContext> buttonTypeContexts
    {
        // m_ButtonType          btnLeftID  btnMidID   btnRightID  btnLeftIDS         btnMidIDS          btnRightIDS         btnFocus
        { MB_OK,               { 0,         0,         IDOK,       IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_OK,     &m_ButtonRight  } },
        { MB_OKCANCEL,         { 0,         IDOK,      IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_OK,    IDS_GENERIC_CANCEL, &m_ButtonMiddle } },
        { MB_YESNO,            { 0,         IDYES,     IDNO,       IDS_GENERIC_BLANK, IDS_GENERIC_YES,   IDS_GENERIC_NO,     &m_ButtonMiddle } },
        { MB_YESNOCANCEL,      { IDYES,     IDNO,      IDCANCEL,   IDS_GENERIC_YES,   IDS_GENERIC_NO,    IDS_GENERIC_CANCEL, &m_ButtonLeft   } },
        // these MB types are not used by WinDirStat, but included for completeness and using IDS_GENERIC_BLANK as placeholder for button labels,
        // please add required IDS to localization engine upon use
        { MB_RETRYCANCEL,      { 0,         IDRETRY,   IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_CANCEL, &m_ButtonMiddle } },
        { MB_ABORTRETRYIGNORE, { IDABORT,   IDRETRY,   IDIGNORE,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_BLANK,  &m_ButtonLeft   } },
    };

    const auto buttonType = type & MB_TYPEMASK;
    ASSERT(buttonTypeContexts.contains(buttonType));
    m_buttonContext = buttonTypeContexts.at(buttonType);

    // Set icon based on message box type
    const std::unordered_map<UINT, LPCWSTR> iconMap
    {
        { MB_ICONERROR,       IDI_ERROR },
        { MB_ICONQUESTION,    IDI_QUESTION },
        { MB_ICONWARNING,     IDI_WARNING },
        { MB_ICONINFORMATION, IDI_INFORMATION },
    };

    const auto iconType = type & MB_ICONMASK;
    m_Icon = LoadIcon(nullptr, iconMap.contains(iconType) ?
        iconMap.at(iconType) : IDI_INFORMATION);
}

bool CMessageBoxDlg::IsCheckboxChecked() const
{
    return m_CheckboxChecked;
}

void CMessageBoxDlg::DoDataExchange(CDataExchange* pDX)
{
    CLayoutDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MESSAGE_ICON, m_IconCtrl);
    DDX_Control(pDX, IDC_MESSAGE_TEXT, m_MessageCtrl);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONLEFT, m_ButtonLeft);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONMIDDLE, m_ButtonMiddle);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONRIGHT, m_ButtonRight);
    DDX_Control(pDX, IDC_MESSAGE_CHECKBOX, m_Checkbox);
    DDX_Control(pDX, IDC_MESSAGE_LISTVIEW, m_ListView);
    DDX_Check(pDX, IDC_MESSAGE_CHECKBOX, m_CheckboxChecked);
}

BEGIN_MESSAGE_MAP(CMessageBoxDlg, CLayoutDialogEx)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONLEFT, OnButtonLeft)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONMIDDLE, OnButtonMiddle)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONRIGHT, OnButtonRight)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

void CMessageBoxDlg::ShiftControls(const std::vector<CWnd*>& controls, const int shiftAmount)
{
    if (shiftAmount == 0)
        return;

    for (auto* pCtrl : controls)
    {
        CRect rect;
        pCtrl->GetWindowRect(&rect);
        ScreenToClient(&rect);
        rect.OffsetRect(0, shiftAmount);
        pCtrl->MoveWindow(&rect);
    }

    // Resize dialog
    CRect dialogRect;
    GetWindowRect(&dialogRect);
    dialogRect.bottom += shiftAmount;
    MoveWindow(&dialogRect);
}

void CMessageBoxDlg::ShiftControlsIfHidden(const CWnd* pTargetControl, const std::vector<CWnd*>& controlsToShift)
{
    if (pTargetControl->GetStyle() & WS_VISIBLE)
        return;

    CRect targetRect;
    pTargetControl->GetWindowRect(&targetRect);
    ScreenToClient(&targetRect);

    // Find nearest control below target
    int minYBelow = INT_MAX;
    for (const auto* ctrl : controlsToShift)
    {
        CRect ctrlRect;
        ctrl->GetWindowRect(&ctrlRect);
        ScreenToClient(&ctrlRect);

        if (ctrlRect.top > targetRect.top)
            minYBelow = min(minYBelow, ctrlRect.top);
    }

    // Calculate shift: control height + spacing to next control
    const int shiftAmount = (minYBelow != INT_MAX) ?
        (minYBelow - targetRect.top) : targetRect.Height();

    // Shift controls below target upward
    ShiftControls(controlsToShift, -shiftAmount);
}

BOOL CMessageBoxDlg::OnInitDialog()
{
    CLayoutDialogEx::OnInitDialog();

    // Set window title and message
    SetWindowText(m_Title.c_str());
    m_MessageCtrl.SetWindowText(m_Message.c_str());

    // Configure buttons
    m_ButtonLeft.ShowWindow(m_buttonContext.btnLeftID != 0 ? SW_SHOW : SW_HIDE);
    m_ButtonMiddle.ShowWindow(m_buttonContext.btnMidID != 0 ? SW_SHOW : SW_HIDE);
    m_ButtonRight.ShowWindow(m_buttonContext.btnRightID != 0 ? SW_SHOW : SW_HIDE);

    // Set button texts
    m_ButtonLeft.SetWindowText(Localization::Lookup(m_buttonContext.btnLeftIDS).c_str());
    m_ButtonMiddle.SetWindowText(Localization::Lookup(m_buttonContext.btnMidIDS).c_str());
    m_ButtonRight.SetWindowText(Localization::Lookup(m_buttonContext.btnRightIDS).c_str());

    // Set focus to default button
    m_buttonContext.btnFocus->SetFocus();

    // Set display icon
    m_IconCtrl.SetIcon(m_Icon);

    // Add strings to optional listview
    m_ListView.ShowWindow(m_ListViewItems.empty() ? SW_HIDE : SW_SHOW);
    for (const auto& item : m_ListViewItems)
    {
        m_ListView.AddString(item.c_str());
    }

    // Hide checkbox if no text set
    m_Checkbox.SetWindowText(m_CheckboxText.c_str());
    m_Checkbox.ShowWindow(m_CheckboxText.empty() ? SW_HIDE : SW_SHOW);

    // Apply dark mode
    DarkMode::AdjustControls(*this);

    // Determine if the dialog needs to be shifted down for the message
    CRect rectMessage;
    m_MessageCtrl.GetWindowRect(&rectMessage);
    ScreenToClient(&rectMessage);
    CDC* pDC = m_MessageCtrl.GetDC();
    CRect rectText(0, 0, rectMessage.Width(), 0);
    pDC->DrawText(m_Message.c_str(), &rectText, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    m_MessageCtrl.ReleaseDC(pDC);

    // Shift down if message height exceeds icon height
    CRect iconRect;
    m_IconCtrl.GetWindowRect(&iconRect);
    const int deltaHeight = rectText.Height() - max(iconRect.Height(), rectMessage.Height());
    ShiftControls({ &m_ListView, &m_ButtonLeft, &m_ButtonMiddle, &m_ButtonRight, &m_Checkbox }, deltaHeight);

    // Hide controls if hidden
    ShiftControlsIfHidden(&m_ListView, { &m_ButtonLeft, &m_ButtonMiddle, &m_ButtonRight, &m_Checkbox });
    ShiftControlsIfHidden(&m_Checkbox, { &m_ButtonLeft, &m_ButtonMiddle, &m_ButtonRight });

    // Active automatic layout management
    m_Layout.AddControl(IDC_MESSAGE_ICON, 0, 0, 0, 0);
    m_Layout.AddControl(IDC_MESSAGE_TEXT, 0, 0, 1, 0);
    m_Layout.AddControl(IDC_MESSAGE_LISTVIEW, 0, 0, 1, 1);
    m_Layout.AddControl(IDC_MESSAGE_CHECKBOX, 0, 1, 1, 0);
    m_Layout.AddControl(IDC_MESSAGE_BUTTONLEFT, 1, 1, 0, 0);
    m_Layout.AddControl(IDC_MESSAGE_BUTTONMIDDLE, 1, 1, 0, 0);
    m_Layout.AddControl(IDC_MESSAGE_BUTTONRIGHT, 1, 1, 0, 0);
    m_Layout.OnInitDialog(true);

    // Adjust dialog size if requested
    CRect rectWindow;
    GetWindowRect(&rectWindow);
    if (m_InitialSize.cx > 0)
    {
        rectWindow.right = rectWindow.left + m_InitialSize.cx;
        rectWindow.bottom = rectWindow.top + m_InitialSize.cy;
        MoveWindow(&rectWindow);
    }

    // Center dialog
    CenterWindow();

    return TRUE;
}

void CMessageBoxDlg::OnButtonLeft()
{
    UpdateData(TRUE);
    EndDialog(m_buttonContext.btnLeftID);
}

void CMessageBoxDlg::OnButtonMiddle()
{
    UpdateData(TRUE);
    EndDialog(m_buttonContext.btnMidID);
}

void CMessageBoxDlg::OnButtonRight()
{
    UpdateData(TRUE);
    EndDialog(m_buttonContext.btnRightID);
}

INT_PTR CMessageBoxDlg::DoModal()
{
    return CLayoutDialogEx::DoModal();
}

HBRUSH CMessageBoxDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

// Global wrapper functions
int WdsMessageBox(const std::wstring& message, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return AfxMessageBox(message.c_str(), type);
    }

    return WdsMessageBox(nullptr, message, Localization::LookupNeutral(AFX_IDS_APP_TITLE), type);
}

int WdsMessageBox(const HWND wnd, const std::wstring& message, const std::wstring& title, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return MessageBox(wnd, message.c_str(), title.c_str(), type);
    }

    CMessageBoxDlg dlg(message, title, type, CWnd::FromHandle(wnd));
    return static_cast<int>(dlg.DoModal());
}
