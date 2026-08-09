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

CMessageBoxDlg::CMessageBoxDlg(const std::wstring& message, const std::wstring& title, const UINT type, CWnd* pParent,
    const std::vector<std::wstring>& listViewItems, const std::wstring& checkBoxText, const bool checkBoxValue)
    : MessageTarget(IDD_MESSAGEBOX, &m_windowRect, pParent)
    , m_message(message)
    , m_title(title)
    , m_checkboxText(checkBoxText)
    , m_listViewItems(listViewItems)
    , m_checkboxChecked(checkBoxValue)
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
    assert(buttonTypeContexts.contains(buttonType));
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

WdsMessageBoxResult CMessageBoxDlg::Show(const std::wstring& message, const std::vector<std::wstring>& listViewItems, const std::wstring& checkboxText, const bool checkboxValue, const UINT type, CWnd* pParent, const CSize& initialSize, const std::wstring& title)
{
    CWnd* parent = pParent ? pParent : GetMainWindow();

    CMessageBoxDlg dlg(message, title, type, parent, listViewItems, checkboxText, checkboxValue);

    if (initialSize.cx > 0 || initialSize.cy > 0)
        dlg.SetInitialWindowSize(initialSize);

    return { static_cast<int>(dlg.ShowModal()), dlg.IsCheckboxChecked() };
}

bool CMessageBoxDlg::IsCheckboxChecked() const
{
    return m_checkboxChecked;
}

void CMessageBoxDlg::ShiftControls(const std::vector<CWnd*>& controls, const int shiftAmount)
{
    for (auto* pCtrl : controls)
    {
        CRect rect = WindowRectInClient(pCtrl->Handle());
        rect.Offset(0, shiftAmount);
        pCtrl->MoveWindow(&rect);
    }

    // Resize dialog
    CRect dialogRect(Handle());
    dialogRect.bottom += shiftAmount;
    MoveWindow(&dialogRect);
}

void CMessageBoxDlg::ShiftControlsIfHidden(const CWnd* pTargetControl, const std::vector<CWnd*>& controlsToShift, const int padding)
{
    // Expand checkbox width to the leftmost visible button
    if (pTargetControl == &m_listView && (m_checkbox.GetStyle() & WS_VISIBLE))
    {
        for (const CWnd* pBtn : { &m_buttonLeft, &m_buttonMiddle, &m_buttonRight })
        {
            if (pBtn->Handle() && (pBtn->GetStyle() & WS_VISIBLE))
            {
                CRect cbRect = WindowRectInClient(m_checkbox.Handle());
                const CRect btnRect = WindowRectInClient(pBtn->Handle());

                cbRect.right = btnRect.left;
                if (cbRect.right > cbRect.left) m_checkbox.MoveWindow(&cbRect);
                break;
            }
        }
    }

    if (pTargetControl->GetStyle() & WS_VISIBLE) return;

    const CRect targetRect = WindowRectInClient(pTargetControl->Handle());

    // Find nearest control below target
    int minYBelow = INT_MAX;
    for (const auto* ctrl : controlsToShift)
    {
        const CRect ctrlRect = WindowRectInClient(ctrl->Handle());

        if (ctrlRect.top > targetRect.top)
            minYBelow = std::min<int>(minYBelow, ctrlRect.top);
    }

    // Calculate shift: control height + spacing to next control, and optional padding
    const int shiftAmount = std::max<int>(0, ((minYBelow != INT_MAX) ?
        (minYBelow - targetRect.top) : targetRect.Height()) - ScaleForDpi(padding));

    // Shift controls below target upward
    ShiftControls(controlsToShift, -shiftAmount);
}

bool CMessageBoxDlg::OnInitDialog()
{
    CLayoutDialog::OnInitDialog();

    m_iconCtrl.SubclassDlgItem(IDC_MESSAGE_ICON, this);
    m_messageCtrl.SubclassDlgItem(IDC_MESSAGE_TEXT, this);
    m_buttonLeft.SubclassDlgItem(IDC_MESSAGE_BUTTONLEFT, this);
    m_buttonMiddle.SubclassDlgItem(IDC_MESSAGE_BUTTONMIDDLE, this);
    m_buttonRight.SubclassDlgItem(IDC_MESSAGE_BUTTONRIGHT, this);
    m_checkbox.SubclassDlgItem(IDC_MESSAGE_CHECKBOX, this);
    m_listView.SubclassDlgItem(IDC_MESSAGE_LISTVIEW, this);

    // Set window title and message
    SetText(m_title.c_str());
    m_messageCtrl.SetText(m_message.c_str());

    // Configure buttons
    m_buttonLeft.ShowWindow(m_buttonContext.btnLeftID != 0 ? SW_SHOW : SW_HIDE);
    m_buttonMiddle.ShowWindow(m_buttonContext.btnMidID != 0 ? SW_SHOW : SW_HIDE);
    m_buttonRight.ShowWindow(m_buttonContext.btnRightID != 0 ? SW_SHOW : SW_HIDE);

    // Set button texts
    m_buttonLeft.SetText(Localization::Lookup(m_buttonContext.btnLeftIDS).c_str());
    m_buttonMiddle.SetText(Localization::Lookup(m_buttonContext.btnMidIDS).c_str());
    m_buttonRight.SetText(Localization::Lookup(m_buttonContext.btnRightIDS).c_str());

    // Set display icon
    m_iconCtrl.SetIcon(m_icon);

    // Configure optional owner-data list view
    m_listView.ShowWindow(m_listViewItems.empty() ? SW_HIDE : SW_SHOW);
    if (!m_listViewItems.empty())
    {
        std::ranges::sort(m_listViewItems);
        m_listView.SetExtendedStyle(m_listView.GetExtendedStyle() | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
        m_listView.InsertColumn(0, L"");
        m_listView.SetItemCountEx(static_cast<int>(m_listViewItems.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    }

    // Hide checkbox if no text set
    m_checkbox.SetText(m_checkboxText.c_str());
    SetChecked(IDC_MESSAGE_CHECKBOX, m_checkboxChecked);
    m_checkbox.ShowWindow(m_checkboxText.empty() ? SW_HIDE : SW_SHOW);

    // Apply dark mode
    DarkMode::AdjustControls(*this);
    if (DarkMode::IsDarkModeActive())
    {
        const COLORREF listBackColor = DarkMode::SystemColor(COLOR_WINDOW);
        m_listView.SetBkColor(listBackColor);
        m_listView.SetTextBkColor(listBackColor);
        m_listView.SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));
    }

    // Collapse hidden controls vertically and add padding to vertically center the message area
    ShiftControlsIfHidden(&m_listView, { &m_checkbox, &m_buttonLeft, &m_buttonMiddle, &m_buttonRight }, 16);

    // Measure message text
    const CRect rectMessage = WindowRectInClient(m_messageCtrl.Handle());

    // Account for control borders/margins
    const CRect rectMessageClient = m_messageCtrl.ClientRect();
    const int messageBorders = rectMessage.Width() - rectMessageClient.Width();

    // Calculate scaling for initial size requirements
    const CSize scaledInitialSize(
        ScaleForDpi(m_initialSize.cx),
        ScaleForDpi(m_initialSize.cy)
    );

    CRect rectWindow(Handle());
    const int initialWidthExpansion = std::max<int>(0, scaledInitialSize.cx - rectWindow.Width());

    CClientDC dc(&m_messageCtrl);
    GdiObjectSelection selectFont(&dc, m_messageCtrl.GetFont());

    CRect rectTextCalc = rectMessage;
    constexpr UINT baseFlags = DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS;

    if (m_autoWidth)
    {
        // Don't wrap words, calculate full width
        rectTextCalc.right = LONG_MAX;
        dc.DrawText(m_message, &rectTextCalc, baseFlags);
    }
    else
    {
        // Wrap words within the allowed width (current + initial expansion)
        rectTextCalc.right += initialWidthExpansion;

        // Ensure we respect borders when calculating available text width
        rectTextCalc.right -= messageBorders;
        dc.DrawText(m_message, &rectTextCalc, baseFlags | DT_WORDBREAK);
    }

    // Restore border width to the calculated rect for layout consistency
    rectTextCalc.right += messageBorders;

    // Determine Expansion Needed
    int deltaWidth = initialWidthExpansion;
    if (m_autoWidth)
    {
        // If auto-width, ensure we expand enough for the text
        const int textRequiredExpanded = std::max<int>(0, rectTextCalc.Width() - rectMessage.Width());
        deltaWidth = std::max(deltaWidth, textRequiredExpanded);
    }

    // Apply Vertical Expansion
    if (const int deltaHeight = std::max<int>(0, rectTextCalc.Height() - rectMessage.Height()); deltaHeight > 0)
    {
        const int padding = ScaleForDpi(16);
        // Expand message control
        m_messageCtrl.SetWindowPos(nullptr, 0, 0, rectMessage.Width(), rectMessage.Height() + deltaHeight + padding, SWP_NOMOVE | SWP_NOZORDER);

        // Push everything else down
        ShiftControls({ &m_listView, &m_checkbox, &m_buttonLeft, &m_buttonMiddle, &m_buttonRight }, deltaHeight + padding);
    }

    // Activate automatic layout management (snapshots current positions)
    m_layout.AddControl(IDC_MESSAGE_ICON, 0, 0, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_TEXT, 0, 0, 1, 0);
    m_layout.AddControl(IDC_MESSAGE_LISTVIEW, 0, 0, 1, 1);
    m_layout.AddControl(IDC_MESSAGE_CHECKBOX, 0, 1, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONLEFT, 1, 1, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONMIDDLE, 1, 1, 0, 0);
    m_layout.AddControl(IDC_MESSAGE_BUTTONRIGHT, 1, 1, 0, 0);
    m_layout.OnInitDialog(true);

    // Apply width and final height expansion
    rectWindow = CRect(Handle());
    const int newWidth = rectWindow.Width() + deltaWidth;
    int newHeight = rectWindow.Height();

    // Ensure minimum height from initial size
    newHeight = std::max(static_cast<int>(scaledInitialSize.cy), newHeight);
    if (newWidth != rectWindow.Width() || newHeight != rectWindow.Height())
    {
        SetWindowPos(nullptr, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
    }
    UpdateListViewColumnWidth();

    // Remove resizable border
    if (m_autoWidth) ModifyStyle(WS_THICKFRAME, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    // Center dialog
    CenterWindow();

    // Set focus to default button
    if (m_buttonContext.btnFocus)
    {
        m_buttonContext.btnFocus->ModifyStyle(BS_PUSHBUTTON, BS_DEFPUSHBUTTON);
        m_buttonContext.btnFocus->SetFocus();
        return false;
    }

    return true;
}

void CMessageBoxDlg::UpdateListViewColumnWidth()
{
    LVCOLUMN column{ .mask = LVCF_WIDTH };
    if (!m_listView.Handle() || !m_listView.GetColumn(0, &column))
    {
        return;
    }

    const CRect rect = m_listView.ClientRect();
    m_listView.SetColumnWidth(0, rect.Width());
}

void CMessageBoxDlg::OnSize(const UINT nType, const int cx, const int cy)
{
    CLayoutDialog::OnSize(nType, cx, cy);
    UpdateListViewColumnWidth();
}

void CMessageBoxDlg::OnListViewCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = CDRF_DODEFAULT;

    if (!DarkMode::IsDarkModeActive())
    {
        return;
    }

    if (auto* customDraw = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
        customDraw->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
        *pResult = CDRF_NOTIFYITEMDRAW;
    }
    else if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
    {
        customDraw->clrText = DarkMode::SystemColor(COLOR_WINDOWTEXT);
        customDraw->clrTextBk = DarkMode::SystemColor(COLOR_WINDOW);
        *pResult = CDRF_DODEFAULT;
    }
}

void CMessageBoxDlg::OnListViewGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult) const
{
    *pResult = false;

    const auto* displayInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    const int item = displayInfo->item.iItem;
    if ((displayInfo->item.mask & LVIF_TEXT) == 0 || displayInfo->item.iSubItem != 0 ||
        item < 0 || item >= static_cast<int>(m_listViewItems.size()) ||
        displayInfo->item.pszText == nullptr || displayInfo->item.cchTextMax <= 0)
    {
        return;
    }

    wcsncpy_s(displayInfo->item.pszText, displayInfo->item.cchTextMax,
        m_listViewItems[item].c_str(), _TRUNCATE);
}

void CMessageBoxDlg::OnListViewItemChanging(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* listView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    *pResult = ((listView->uChanged & LVIF_STATE) != 0 &&
        ((listView->uNewState ^ listView->uOldState) & LVIS_SELECTED) != 0);
}

void CMessageBoxDlg::OnButtonLeft()
{
    m_checkboxChecked = IsChecked(IDC_MESSAGE_CHECKBOX);
    CloseModal(m_buttonContext.btnLeftID);
}

void CMessageBoxDlg::OnButtonMiddle()
{
    m_checkboxChecked = IsChecked(IDC_MESSAGE_CHECKBOX);
    CloseModal(m_buttonContext.btnMidID);
}

void CMessageBoxDlg::OnButtonRight()
{
    m_checkboxChecked = IsChecked(IDC_MESSAGE_CHECKBOX);
    CloseModal(m_buttonContext.btnRightID);
}

INT_PTR CMessageBoxDlg::ShowModal()
{
    return CLayoutDialog::ShowModal();
}

HBRUSH CMessageBoxDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    // Let DarkMode handle setting the colors first
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);

    // Set checkbox background to match dialog background in dark mode
    if (const int nID = pWnd->GetDlgCtrlID(); nID == IDC_MESSAGE_CHECKBOX)
    {
        pDC->SetBkColor(DarkMode::SystemColor(COLOR_BTNFACE));
        return m_checkboxBrush;
    }

    // Set icon and message text backgrounds to white in light mode
    else if (!DarkMode::IsDarkModeActive() && (nID == IDC_MESSAGE_ICON || nID == IDC_MESSAGE_TEXT))
    {
        pDC->SetBkColor(GetSysColor(COLOR_WINDOW));
        return static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    }

    return brush ? brush : CLayoutDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

bool CMessageBoxDlg::OnEraseBkgnd(CDC* pDC) const
{
    const CRect rect = ClientRect();
    const bool bDark = DarkMode::IsDarkModeActive();

    const COLORREF topColor = DarkMode::SystemColor(COLOR_WINDOW);
    const COLORREF footerColor = DarkMode::SystemColor(COLOR_BTNFACE);
    const COLORREF lineColor = bDark ? DarkMode::SystemColor(COLOR_BTNHIGHLIGHT) : GetSysColor(COLOR_3DSHADOW);

    int lineY = rect.bottom - ScaleForDpi(46);

    if (m_buttonRight.Handle())
    {
        const CRect btnRect = WindowRectInClient(m_buttonRight.Handle());
        lineY = btnRect.top - ScaleForDpi(12);
    }

    // Paint the top area
    pDC->FillSolidRect(CRect(rect.left, rect.top, rect.right, lineY), topColor);

    // Paint the footer area
    pDC->FillSolidRect(CRect(rect.left, lineY, rect.right, rect.bottom), footerColor);

    // Draw the 1px separator
    const CPen pen(PS_SOLID, 1, lineColor);
    const GdiObjectSelection soPen(pDC, &pen);
    pDC->MoveTo(0, lineY);
    pDC->LineTo(rect.right, lineY);

    return true;
}

// Global wrapper functions
int ShowMessageBox(const std::wstring& message, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return MessageBoxW(GetDialogOwner(), message.c_str(), L"WinDirStat", type);
    }

    return ShowMessageBox(nullptr, message, wds::strWinDirStat, type);
}

int ShowMessageBox(const HWND wnd, const std::wstring& message, const std::wstring& title, const UINT type)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return MessageBox(wnd, message.c_str(), title.c_str(), type);
    }

    CMessageBoxDlg dlg(message, title, type, CWnd::FromHandle(wnd));
    return static_cast<int>(dlg.ShowModal());
}
