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
#include "WdsListControl.h"
#include "Layout.h"

inline constexpr UINT WM_WDS_SELECT_DRIVES_OK = WM_APP + 0x110;
inline constexpr UINT WM_WDS_DRIVE_INFO_FINISHED = WM_APP + 0x111;

//
// The dialog has these three radio buttons.
//
enum RADIO : std::uint8_t
{
    RADIO_TARGET_DRIVES_ALL,
    RADIO_TARGET_DRIVES_SUBSET,
    RADIO_TARGET_FOLDER
};

class CDrivesList;

//
// CDriveItem. An item in the CDrivesList Control.
// All methods are called by the gui thread.
//
class CDriveItem final : public CWdsListItem
{
public:
    CDriveItem(CDrivesList* list, const std::wstring& pszPath, std::wstring name = {});
    ~CDriveItem() override;

    void StartQuery(HWND dialog);
    void StopQuery();

    void SetDriveInformation(bool success);

    int Compare(const CWdsListItem* baseOther, int subitem) const override;

    std::wstring GetPath() const;
    std::wstring GetDrive() const;
    bool IsRemote() const;
    bool IsMtp() const;
    bool IsSUBSTed() const;
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override;

private:
    CDrivesList* m_driveList; // Backpointer
    std::wstring m_path; // e.g. "C:\""
    bool m_mtp = false;
    HICON m_icon = nullptr; // Cached icon
    bool m_isRemote; // Whether the drive type is DRIVE_REMOTE (network drive)

    bool m_querying = true; // Information thread is running.
    bool m_success = false; // Drive is accessible. false while m_querying is true.
    bool m_subst = false; // Drive is subst'd

    std::wstring m_name; // e.g. "BOOT (C:)"
    ULONGLONG m_totalBytes = 0; // Capacity
    ULONGLONG m_freeBytes = 0;  // Free space

    double m_used = 0.0; // used space / total space

    // Thread for querying drive information
    std::jthread m_queryThread;
    std::atomic<HWND> m_dialog{ nullptr };
};

//
// CDrivesList.
//
class CDrivesList final : public MessageTarget<CDrivesList, CWdsListControl>
{
    friend class CSelectDrivesDlg;
public:
    CDrivesList();
    CDriveItem* GetItem(int i) const;
    void SelectItem(const CDriveItem* item);
    bool IsItemSelected(int i) const;
    bool IsSorting() const { return m_sortInProgress; }
    void SortItems() override;

static std::span<const RouteEntry> Routes();

protected:
    void OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult) const;
    void OnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);

private:
    bool m_sortInProgress = false;
};

//
// CSelectDrivesDlg. The initial dialog, where the user can select
// one or more drives or a folder for scanning.
//
class CSelectDrivesDlg final : public MessageTarget<CSelectDrivesDlg, CLayoutDialog>
{
public:
    enum : std::uint8_t { IDD = IDD_SELECTDRIVES };

    CSelectDrivesDlg(CWnd* pParent = nullptr);
    ~CSelectDrivesDlg() override = default;

    std::vector<std::wstring> GetSelectedItems() const;
    bool OnInitDialog() override;
    void OnOK() override;
    void UpdateButtons(const std::wstring* folderOverride = nullptr);
    void UpdateFilterButton();
    void SetActiveRadio(int radio);

protected:

    // Dialog Data
    int m_radio = 0;          // out.
    std::wstring m_folderName;    // out. Valid if m_radio = RADIO_TARGET_FOLDER
    std::vector<std::wstring> m_drives;    // out. Valid if m_radio != RADIO_TARGET_FOLDER
    CDrivesList m_driveList;
    CComboBox m_browseList;
    CButton m_okButton;
    CStatic m_browseButton;
    CStatic m_filterButton;
    SmartPointer<HICON, decltype(&DestroyIcon)> m_browseIcon{ &DestroyIcon };
    SmartPointer<HICON, decltype(&DestroyIcon)> m_filterIcon{ &DestroyIcon };
    std::vector<std::wstring> m_selectedDrives;
    bool m_suppressItemChanged = false;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnBnClickedUpdateButtons();
    void OnBnClickedFastScanCheckbox();
    void OnLvnItemChangedDrives(NMHDR* pNMHDR, LRESULT* pResult);
    LRESULT OnWmuOk(WPARAM, LPARAM);
    LRESULT OnWmDriveInfoThreadFinished(WPARAM wParam, LPARAM lparam);
    void OnSysColorChange();
    void OnBnClickedRadioTargetDrivesAll();
    void OnBnClickedRadioTargetDrivesSubset();
    void OnBnClickedRadioTargetFolder();
    void OnBnDoubleclickedRadio();
    void OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult);
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    void OnBnClickedBrowseButton();
    void OnBnClickedFilterButton();
    bool PreprocessMessage(MSG* pMsg) override;
    void OnEditchangeBrowseFolder();
    void OnSelchangeBrowseFolder();
};

inline std::span<const RouteEntry> CDrivesList::Routes()
{
    using ThisClass = CDrivesList;
    static constexpr std::array entries
    {
        Route::ReflectNotify<&ThisClass::OnLvnDeleteItem>(LVN_DELETEITEM),
        Route::ReflectNotify<&ThisClass::OnDoubleClick>(NM_DBLCLK),
    };
    return entries;
}

inline std::span<const RouteEntry> CSelectDrivesDlg::Routes()
{
    using ThisClass = CSelectDrivesDlg;
    static constexpr std::array entries
    {
        Route::Control<&CSelectDrivesDlg::OnBnClickedBrowseButton>(STN_CLICKED, IDC_BROWSE_BUTTON),
        Route::Control<&CSelectDrivesDlg::OnBnClickedFilterButton>(STN_CLICKED, IDC_FILTER_BUTTON),
        Route::Control<&ThisClass::OnBnClickedFastScanCheckbox>(BN_CLICKED, IDC_FAST_SCAN_CHECKBOX),
        Route::Control<&ThisClass::OnBnClickedRadioTargetDrivesAll>(BN_CLICKED, IDC_RADIO_TARGET_DRIVES_ALL),
        Route::Control<&ThisClass::OnBnClickedRadioTargetDrivesSubset>(BN_CLICKED, IDC_RADIO_TARGET_DRIVES_SUBSET),
        Route::Control<&CSelectDrivesDlg::OnBnClickedRadioTargetFolder>(BN_CLICKED, IDC_RADIO_TARGET_FOLDER),
        Route::Control<&ThisClass::OnBnClickedUpdateButtons>(BN_CLICKED, IDC_SCAN_DUPLICATES),
        Route::Control<&ThisClass::OnBnDoubleclickedRadio>(BN_DOUBLECLICKED, IDC_RADIO_TARGET_DRIVES_ALL),
        Route::Control<&ThisClass::OnBnDoubleclickedRadio>(BN_DOUBLECLICKED, IDC_RADIO_TARGET_DRIVES_SUBSET),
        Route::Control<&CSelectDrivesDlg::OnBnDoubleclickedRadio>(BN_DOUBLECLICKED, IDC_RADIO_TARGET_FOLDER),
        Route::Control<&CSelectDrivesDlg::OnEditchangeBrowseFolder>(CBN_EDITCHANGE, IDC_BROWSE_FOLDER),
        Route::Control<&CSelectDrivesDlg::OnSelchangeBrowseFolder>(CBN_SELCHANGE, IDC_BROWSE_FOLDER),
        Route::Notify<&ThisClass::OnLvnItemChangedDrives>(LVN_ITEMCHANGED, IDC_TARGET_DRIVES_LIST),
        Route::Notify<&CSelectDrivesDlg::OnNMSetfocusTargetDrivesList>(NM_SETFOCUS, IDC_TARGET_DRIVES_LIST),
        Route::Window<&ThisClass::OnWmuOk>(WM_WDS_SELECT_DRIVES_OK),
        Route::Window<&ThisClass::OnWmDriveInfoThreadFinished>(WM_WDS_DRIVE_INFO_FINISHED),
        Route::Window<&ThisClass::OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&ThisClass::OnSysColorChange>(WM_SYSCOLORCHANGE),
    };
    return entries;
}
