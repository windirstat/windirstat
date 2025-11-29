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

#pragma once

#include "stdafx.h"
#include "Layout.h"

//
// CMessageBoxDlg. Custom message box dialog with dark mode support.
// Emulates the functionality of MessageBox/AfxMessageBox.
//
class CMessageBoxDlg final : public CLayoutDialogEx
{
    DECLARE_DYNAMIC(CMessageBoxDlg)

    CMessageBoxDlg(const std::wstring& message, const std::wstring& title, UINT type, CWnd* pParent = nullptr,
        const std::vector<std::wstring>& listViewItems = {}, const std::wstring& checkBoxText = {}, bool checkBoxValue = false);
    ~CMessageBoxDlg() override = default;

    INT_PTR DoModal() override;
    void SetInitialWindowSize(const CSize size) { m_InitialSize = size; }

    // Optional checkbox support
    bool IsCheckboxChecked() const;

protected:
    enum : std::uint8_t { IDD = IDD_MESSAGEBOX };

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnButtonLeft();
    afx_msg void OnButtonMiddle();
    afx_msg void OnButtonRight();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    // Helper methods for control layout
    void ShiftControls(const std::vector<CWnd*>& controls, int shiftAmount);
    void ShiftControlsIfHidden(const CWnd* pTargetControl, const std::vector<CWnd*>& controlsToShift);

private:

    using ButtonContext = struct ButtonContext
    {
        BYTE btnLeftID = 0;
        BYTE btnMidID = 0;
        BYTE btnRightID = 0;
        std::wstring_view btnLeftIDS;
        std::wstring_view btnMidIDS;
        std::wstring_view btnRightIDS;
        CButton * btnFocus = nullptr;
    };

    std::wstring m_Message;
    std::wstring m_Title;
    ButtonContext m_buttonContext;
    RECT m_WindowRect {};

    HICON m_Icon;
    CStatic m_IconCtrl;
    CStatic m_MessageCtrl;
    CButton m_ButtonLeft;
    CButton m_ButtonMiddle;
    CButton m_ButtonRight;
    CSize m_InitialSize{};

    // Optional controls
    CButton m_Checkbox;
    CListBox m_ListView;
    std::wstring m_CheckboxText;
    std::vector<std::wstring> m_ListViewItems;
    BOOL m_CheckboxChecked = FALSE;
};

// Global wrapper functions that emulate MessageBox/AfxMessageBox
int WdsMessageBox(const std::wstring& message, UINT type = MB_OK);
int WdsMessageBox(HWND wnd, const std::wstring& message, const std::wstring& title, UINT type = MB_OK);
