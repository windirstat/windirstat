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

IMPLEMENT_DYNAMIC(CMessageBoxDlg, CLayoutDialogEx)

CMessageBoxDlg::CMessageBoxDlg(const std::wstring& message, const std::wstring& title, const UINT type, CWnd* pParent,
    const std::vector<std::wstring>& listViewItems, const std::wstring& checkBoxText, const bool checkBoxValue)
    : CLayoutDialogEx(IDD_MESSAGEBOX, &m_windowRect, pParent)
    , m_message(message)
    , m_title(title)
    , m_checkboxText(checkBoxText)
    , m_listViewItems(listViewItems)
    , m_checkboxChecked(checkBoxValue ? TRUE : FALSE)
{
    const std::unordered_map<UINT, ButtonContext> buttonTypeContexts
    {
        // m_buttonType          btnLeftID  btnMidID   btnRightID  btnLeftIDS         btnMidIDS          btnRightIDS         btnFocus
        { MB_OK,               { 0,         0,         IDOK,       IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_OK,     &m_buttonRight  } },
        { MB_OKCANCEL,         { 0,         IDOK,      IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_OK,    IDS_GENERIC_CANCEL, &m_buttonMiddle } },
        { MB_YESNO,            { 0,         IDYES,     IDNO,       IDS_GENERIC_BLANK, IDS_GENERIC_YES,   IDS_GENERIC_NO,     &m_buttonMiddle } },
        { MB_YESNOCANCEL,      { IDYES,     IDNO,      IDCANCEL,   IDS_GENERIC_YES,   IDS_GENERIC_NO,    IDS_GENERIC_CANCEL, &m_buttonLeft   } },
        // these MB types are not used by WinDirStat, but included for completeness and using IDS_GENERIC_BLANK as placeholder for button labels,
        // please add required IDS to localization engine upon use
        { MB_RETRYCANCEL,      { 0,         IDRETRY,   IDCANCEL,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_CANCEL, &m_buttonMiddle } },
        { MB_ABORTRETRYIGNORE, { IDABORT,   IDRETRY,   IDIGNORE,   IDS_GENERIC_BLANK, IDS_GENERIC_BLANK, IDS_GENERIC_BLANK,  &m_buttonLeft   } },
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
    const auto iconIter = iconMap.find(iconType);
    m_icon = LoadIcon(nullptr, iconIter != iconMap.end() ?
        iconIter->second : IDI_INFORMATION);
}

WdsMessageBoxResult CMessageBoxDlg::Show(const std::wstring& message, const std::vector<std::wstring>& listViewItems, const std::wstring& checkboxText, bool checkboxValue, UINT type, CWnd* pParent, const CSize& initialSize, const std::wstring& title)
{
    CWnd* parent = pParent ? pParent : AfxGetMainWnd();

    CMessageBoxDlg dlg(message, title, type, parent, listViewItems, checkboxText, checkboxValue);

    if (initialSize.cx > 0 || initialSize.cy > 0)
        dlg.SetInitialWindowSize(initialSize);

    return { static_cast<int>(dlg.DoModal()), dlg.IsCheckboxChecked() };
}

bool CMessageBoxDlg::IsCheckboxChecked() const
{
    return m_checkboxChecked;
}

void CMessageBoxDlg::DoDataExchange(CDataExchange* pDX)
{
    CLayoutDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MESSAGE_ICON, m_iconCtrl);
    DDX_Control(pDX, IDC_MESSAGE_TEXT, m_messageCtrl);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONLEFT, m_buttonLeft);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONMIDDLE, m_buttonMiddle);
    DDX_Control(pDX, IDC_MESSAGE_BUTTONRIGHT, m_buttonRight);
    DDX_Control(pDX, IDC_MESSAGE_CHECKBOX, m_checkbox);
    DDX_Control(pDX, IDC_MESSAGE_LISTVIEW, m_listView);
    DDX_Check(pDX, IDC_MESSAGE_CHECKBOX, m_checkboxChecked);
}

BEGIN_MESSAGE_MAP(CMessageBoxDlg, CLayoutDialogEx)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONLEFT, OnButtonLeft)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONMIDDLE, OnButtonMiddle)
    ON_BN_CLICKED(IDC_MESSAGE_BUTTONRIGHT, OnButtonRight)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

void CMessageBoxDlg::ShiftControls(const std::vector<CWnd*>& controls, const int shiftAmount)
{
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
    if (pTargetControl->GetStyle() & WS_VISIBLE) return;

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
    SetWindowText(m_title.c_str());
    m_messageCtrl.SetWindowText(m_message.c_str());

    // Configure buttons
    m_buttonLeft.ShowWindow(m_buttonContext.btnLeftID != 0 ? SW_SHOW : SW_HIDE);
    m_buttonMiddle.ShowWindow(m_buttonContext.btnMidID != 0 ? SW_SHOW : SW_HIDE);
    m_buttonRight.ShowWindow(m_buttonContext.btnRightID != 0 ? SW_SHOW : SW_HIDE);

    // Set button texts
    m_buttonLeft.SetWindowText(Localization::Lookup(m_buttonContext.btnLeftIDS).c_str());
    m_buttonMiddle.SetWindowText(Localization::Lookup(m_buttonContext.btnMidIDS).c_str());
    m_buttonRight.SetWindowText(Localization::Lookup(m_buttonContext.btnRightIDS).c_str());

    // Set display icon
    m_iconCtrl.SetIcon(m_icon);

    // Add strings to optional listview
    m_listView.ShowWindow(m_listViewItems.empty() ? SW_HIDE : SW_SHOW);
    for (const auto& item : m_listViewItems)
    {
        m_listView.AddString(item.c_str());
    }

    // Hide checkbox if no text set
    m_checkbox.SetWindowText(m_checkboxText.c_str());
    m_checkbox.ShowWindow(m_checkboxText.empty() ? SW_HIDE : SW_SHOW);

    // Apply dark mode
    DarkMode::AdjustControls(*this);

    // Collapse hidden controls vertically
    ShiftControlsIfHidden(&m_listView, { &m_checkbox, &m_buttonLeft, &m_buttonMiddle, &m_buttonRight });
    ShiftControlsIfHidden(&m_checkbox, { &m_buttonLeft, &m_buttonMiddle, &m_buttonRight });

    // Measure message text
    CRect rectMessage;
    m_messageCtrl.GetWindowRect(&rectMessage);
    ScreenToClient(&rectMessage);

    // Account for control borders/margins
    CRect rectMessageClient;
    m_messageCtrl.GetClientRect(&rectMessageClient);
    const int messageBorders = rectMessage.Width() - rectMessageClient.Width();

    // Calculate scaling for initial size requirements
    const CSize scaledInitialSize(
        DpiRest(m_initialSize.cx, this),
        DpiRest(m_initialSize.cy, this)
    );

    CRect rectWindow;
    GetWindowRect(&rectWindow);
    const int initialWidthExpansion = max(0, scaledInitialSize.cx - rectWindow.Width());

    CClientDC dc(&m_messageCtrl);
    CSelectObject selectFont(&dc, m_messageCtrl.GetFont());

    CRect rectTextCalc = rectMessage;
    constexpr UINT baseFlags = DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS;

    if (m_autoWidth)
    {
        // Don't wrap words, calculate full width
        rectTextCalc.right = LONG_MAX;
        dc.DrawText(m_message.c_str(), &rectTextCalc, baseFlags);
    }
    else
    {
        // Wrap words within the allowed width (current + initial expansion)
        rectTextCalc.right += initialWidthExpansion;

        // Ensure we respect borders when calculating available text width
        rectTextCalc.right -= messageBorders;
        dc.DrawText(m_message.c_str(), &rectTextCalc, baseFlags | DT_WORDBREAK);
    }

    // Restore border width to the calculated rect for layout consistency
    rectTextCalc.right += messageBorders;

    // Determine Expansion Needed
    int deltaWidth = initialWidthExpansion;
    if (m_autoWidth)
    {
        // If auto-width, ensure we expand enough for the text
        const int textRequiredExpanded = max(0, rectTextCalc.Width() - rectMessage.Width());
        deltaWidth = max(deltaWidth, textRequiredExpanded);
    }

    // Apply Vertical Expansion
    if (const int deltaHeight = max(0, rectTextCalc.Height() - rectMessage.Height()); deltaHeight > 0)
    {
        // Expand message control
        m_messageCtrl.SetWindowPos(nullptr, 0, 0, rectMessage.Width(), rectMessage.Height() + deltaHeight, SWP_NOMOVE | SWP_NOZORDER);

        // Push everything else down
        ShiftControls({ &m_listView, &m_checkbox, &m_buttonLeft, &m_buttonMiddle, &m_buttonRight }, deltaHeight);
    }

    // Activate automatic layout management (snapshots current positions)
    m_layout.AddControl(IDC_MESSAGE_ICON, 0, 0, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_TEXT, 0, 0, 1, 0);
    m_layout.AddControl(IDC_MESSAGE_LISTVIEW, 0, 0, 1, 1);
    m_layout.AddControl(IDC_MESSAGE_CHECKBOX, 0, 1, 1, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONLEFT, 1, 1, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONMIDDLE, 1, 1, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONRIGHT, 1, 1, 0, 0);
    m_layout.OnInitDialog(true);

    // Apply width and final height expansion
    GetWindowRect(&rectWindow);
    const int newWidth = rectWindow.Width() + deltaWidth;
    int newHeight = rectWindow.Height();

    // Ensure minimum height from initial size
    newHeight = max(scaledInitialSize.cy, newHeight);
    if (newWidth != rectWindow.Width() || newHeight != rectWindow.Height())
    {
        SetWindowPos(nullptr, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
    }

    // Remove resizable border
    if (m_autoWidth) ModifyStyle(WS_THICKFRAME, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    // Center dialog
    CenterWindow();

    // Set focus to default button
    if (m_buttonContext.btnFocus)
    {
        m_buttonContext.btnFocus->ModifyStyle(BS_PUSHBUTTON, BS_DEFPUSHBUTTON);
        m_buttonContext.btnFocus->SetFocus();
        return FALSE;
    }

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

    return WdsMessageBox(nullptr, message, wds::strWinDirStat, type);
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
