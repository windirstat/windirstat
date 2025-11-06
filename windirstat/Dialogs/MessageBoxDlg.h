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
#include <string>

struct ButtonContexts
{
    int btnLeftSW;
    int btnMidSW;
    int btnRightSW;
    int btnLeftID = 0;
    int btnMidID = 0;
    int btnRightID = 0;
    std::wstring_view btnLeftIDS = wds::strEmpty;
    std::wstring_view btnMidIDS = wds::strEmpty;
    std::wstring_view btnRightIDS = wds::strEmpty;
};

//
// CMessageBoxDlg. Custom message box dialog with dark mode support.
// Emulates the functionality of MessageBox/AfxMessageBox.
//
class CMessageBoxDlg final : public CDialogEx
{
    DECLARE_DYNAMIC(CMessageBoxDlg)

    CMessageBoxDlg(HWND wnd, const std::wstring& message, const std::wstring& title, UINT type, CWnd* pParent = nullptr);
    ~CMessageBoxDlg() override = default;

    INT_PTR DoModal() override;

protected:
    enum : std::uint8_t { IDD = IDD_MESSAGEBOX };

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnButtonLeft();
    afx_msg void OnButtonMiddle();
    afx_msg void OnButtonRight();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

private:
    std::wstring m_Message;
    std::wstring m_Title;
    UINT m_ButtonType;
    UINT m_IconType;
    HICON m_hIcon;
    HWND m_Hwnd;
    ButtonContexts m_buttonTypeContext;

    CStatic m_IconCtrl;
    CStatic m_MessageCtrl;
    CButton m_ButtonLeft;
    CButton m_ButtonMiddle;
    CButton m_ButtonRight;
};

// Global wrapper functions that emulate MessageBox/AfxMessageBox
int WdsMessageBox(const std::wstring& message, UINT type = MB_OK);
int WdsMessageBox(HWND wnd, const std::wstring& message, const std::wstring& title, UINT type = MB_OK);
