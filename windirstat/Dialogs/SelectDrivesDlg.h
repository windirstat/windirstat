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
#include "OwnerDrawnListControl.h"
#include "Layout.h"

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
class CDriveItem final : public COwnerDrawnListItem
{
public:
    CDriveItem(CDrivesList* list, const std::wstring& pszPath);

    void StartQuery(HWND dialog, UINT serial) const;

    void SetDriveInformation(bool success, const std::wstring& name, ULONGLONG total, ULONGLONG free);

    int Compare(const COwnerDrawnListItem* baseOther, int subitem) const override;

    std::wstring GetPath() const;
    std::wstring GetDrive() const;
    bool IsRemote() const;
    bool IsSUBSTed() const;
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override;

private:
    CDrivesList* m_driveList; // Backpointer
    std::wstring m_path; // e.g. "C:\""
    HICON m_icon = nullptr; // Cached icon
    bool m_isRemote; // Whether the drive type is DRIVE_REMOTE (network drive)

    bool m_querying = true; // Information thread is running.
    bool m_success = false; // Drive is accessible. false while m_querying is true.
    bool m_subst = false; // Drive is subst'd

    std::wstring m_name; // e.g. "BOOT (C:)"
    ULONGLONG m_totalBytes = 0; // Capacity
    ULONGLONG m_freeBytes = 0;  // Free space

    double m_used = 0.0; // used space / total space
};

//
// CDriveInformationThread. Does the GetVolumeInformation() call, which
// may hang for ca. 30 sec, if a network drive is not accessible.
//
class CDriveInformationThread final : public CWinThread
{
    // Set of all running CDriveInformationThreads.
    // Used by InvalidateDialogHandle().
    static std::unordered_set<CDriveInformationThread*> s_runningThreads;
    static std::mutex s_mutexRunningThreads;

    // The objects register and unregister themselves in _runningThreads
    void AddRunningThread();
    void RemoveRunningThread();

public:
    static void InvalidateDialogHandle();

    CDriveInformationThread(const std::wstring& path, LPARAM driveItem, HWND dialog, UINT serial);
    BOOL InitInstance() override;

    LPARAM GetDriveInformation(bool& success, std::wstring& name, ULONGLONG& total, ULONGLONG& free) const;

private:
    const std::wstring m_path; // Path like "C:\""
    const LPARAM m_driveItem;  // The list item, we belong to
    const UINT m_serial;       // serial number of m_dialog
    std::atomic<HWND> m_dialog;

    // "[out]"-parameters
    std::wstring m_name;        // Result: name like "BOOT (C:)", valid if m_success
    ULONGLONG m_totalBytes = 0; // Result: capacity of the drive, valid if m_success
    ULONGLONG m_freeBytes = 0;  // Result: free space on the drive, valid if m_success
    bool m_success = false;     // Result: false, iff drive is inaccessible.
};

//
// CDrivesList.
//
class CDrivesList final : public COwnerDrawnListControl
{
    friend class CSelectDrivesDlg;
    DECLARE_DYNAMIC(CDrivesList)

    CDrivesList();
    CDriveItem* GetItem(int i) const;
    void SelectItem(const CDriveItem* item);
    bool IsItemSelected(int i) const;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
};

//
// CSelectDrivesDlg. The initial dialog, where the user can select
// one or more drives or a folder for scanning.
//
class CSelectDrivesDlg final : public CLayoutDialogEx
{
    DECLARE_DYNAMIC(CSelectDrivesDlg)

    enum : std::uint8_t { IDD = IDD_SELECTDRIVES };

    static std::wstring GetFullPathName(const std::wstring& relativePath);

    CSelectDrivesDlg(CWnd* pParent = nullptr);
    ~CSelectDrivesDlg() override = default;

    std::vector<std::wstring> GetSelectedItems() const;
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void UpdateButtons();
    void SetActiveRadio(int radio);

protected:

    // Dialog Data
    BOOL m_scanDuplicates = false; // whether duplicate scanning is enable
    BOOL m_useFastScan = false; // whether fast scan is enable
    int m_radio = 0;          // out.
    CStringW m_folderName;    // out. Valid if m_radio = RADIO_TARGET_FOLDER
    std::vector<std::wstring> m_drives;    // out. Valid if m_radio != RADIO_TARGET_FOLDER
    static UINT s_serial; // Each Instance of this dialog gets a serial number
    CDrivesList m_driveList;
    CComboBox m_browseList;
    CButton m_okButton;
    CButton m_browseButton;
    std::vector<std::wstring> m_selectedDrives;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedUpdateButtons();
    afx_msg void OnLvnItemChangedDrives(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnWmuOk(WPARAM, LPARAM);
    afx_msg LRESULT OnWmDriveInfoThreadFinished(WPARAM, LPARAM lparam);
    afx_msg void OnSysColorChange();
    afx_msg void OnBnClickedRadioTargetDrivesSubset();
    afx_msg void OnBnClickedRadioTargetFolder();
    afx_msg void OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnBnClickedBrowseButton();
    BOOL PreTranslateMessage(MSG* pMsg) override;
public:
    afx_msg void OnEditchangeBrowseFolder();
    afx_msg void OnCbnSelchangeBrowseFolder();
};
