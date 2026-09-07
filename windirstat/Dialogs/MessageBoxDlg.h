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

#include "pch.h"
#include "Layout.h"

struct WdsMessageBoxResult { int nID; bool isChecked; };

//
// CMessageBoxDlg. Custom message box dialog with dark mode support.
// Provides consistent light and dark message boxes.
//
class CMessageBoxDlg final : public MessageTarget<CMessageBoxDlg, CLayoutDialog>
{
public:
    CMessageBoxDlg(const std::wstring& message, const std::wstring& title, UINT type, CWnd* pParent = nullptr,
        const std::vector<std::wstring>& listViewItems = {}, const std::wstring& checkBoxText = {}, bool checkBoxValue = false);
    ~CMessageBoxDlg() override = default;

    static int Show(const std::wstring& message, const UINT type = MB_OK, CWnd* pParent = nullptr, const CSize& initialSize = {}, const std::wstring& title = Localization::LookupNeutral(IDS_APP_TITLE)) { return Show(message, {}, {}, false, type, pParent, initialSize, title).nID; }
    static WdsMessageBoxResult Show(const std::wstring& message, const std::wstring& checkboxText, const bool checkboxValue = false, const UINT type = MB_YESNO | MB_ICONQUESTION, CWnd* pParent = nullptr, const CSize& initialSize = {}, const std::wstring& title = Localization::LookupNeutral(IDS_APP_TITLE)) { return Show(message, {}, checkboxText, checkboxValue, type, pParent, initialSize, title); }
    static WdsMessageBoxResult Show(const std::wstring& message, const std::vector<std::wstring>& listViewItems, const std::wstring& checkboxText, bool checkboxValue = false, UINT type = MB_YESNO | MB_ICONWARNING, CWnd* pParent = nullptr, const CSize& initialSize = {}, const std::wstring& title = Localization::LookupNeutral(IDS_APP_TITLE));

    INT_PTR ShowModal() override;
    void SetInitialWindowSize(const CSize size) { m_initialSize = size; }
    void SetWidthAuto() { m_autoWidth = true; }

    // Optional checkbox support
    bool IsCheckboxChecked() const { return m_checkboxChecked; }

protected:
    enum : std::uint8_t { IDD = IDD_MESSAGEBOX };

    bool OnInitDialog() override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnButtonLeft();
    void OnButtonMiddle();
    void OnButtonRight();
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    bool OnEraseBkgnd(CDC* pDC) const;
    void OnSize(UINT nType, int cx, int cy);
    void OnListViewCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    void OnListViewGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult) const;
    void OnListViewItemChanging(NMHDR* pNMHDR, LRESULT* pResult);

    // Helper methods for control layout
    void ShiftControls(const std::vector<CWnd*>& controls, int shiftAmount);
    void ShiftControlsIfHidden(const CWnd* pTargetControl, const std::vector<CWnd*>& controlsToShift, int padding = 0);
    void UpdateListViewColumnWidth();

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

    std::wstring m_message;
    std::wstring m_title;
    ButtonContext m_buttonContext;
    RECT m_windowRect {};

    HICON m_icon;
    CStatic m_iconCtrl;
    CStatic m_messageCtrl;
    CButton m_buttonLeft;
    CButton m_buttonMiddle;
    CButton m_buttonRight;
    CSize m_initialSize{};
    bool m_autoWidth = false;

    // Optional controls
    CButton m_checkbox;
    CBrush m_checkboxBrush{ DarkMode::SystemColor(COLOR_BTNFACE) };
    CListCtrl m_listView;
    std::wstring m_checkboxText;
    std::vector<std::wstring> m_listViewItems;
    bool m_checkboxChecked = false;
};

// Global message-box helpers
int ShowMessageBox(const std::wstring& message, UINT type = MB_OK);
int ShowMessageBox(HWND wnd, const std::wstring& message, const std::wstring& title, UINT type = MB_OK);

inline std::span<const RouteEntry> CMessageBoxDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnButtonLeft>(BN_CLICKED, IDC_MESSAGE_BUTTONLEFT),
        Route::Control<&OnButtonMiddle>(BN_CLICKED, IDC_MESSAGE_BUTTONMIDDLE),
        Route::Control<&OnButtonRight>(BN_CLICKED, IDC_MESSAGE_BUTTONRIGHT),
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Notify<&OnListViewCustomDraw>(NM_CUSTOMDRAW, IDC_MESSAGE_LISTVIEW),
        Route::Notify<&OnListViewGetDispInfo>(LVN_GETDISPINFO, IDC_MESSAGE_LISTVIEW),
        Route::Notify<&OnListViewItemChanging>(LVN_ITEMCHANGING, IDC_MESSAGE_LISTVIEW),
    };
    return entries;
}
