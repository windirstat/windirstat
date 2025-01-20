// SelectDrivesDlg.h - Declaration of CDriveItem, CDrivesList and CSelectDrivesDlg
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "OwnerDrawnListControl.h"
#include "Layout.h"
#include "resource.h"

#include <shared_mutex>
#include <unordered_set>

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
    ~CDriveItem();

    void StartQuery(HWND dialog, UINT serial) const;

    void SetDriveInformation(bool success, const std::wstring& name, ULONGLONG total, ULONGLONG free);

    int Compare(const CSortingListItem* baseOther, int subitem) const override;

    std::wstring GetPath() const;
    std::wstring GetDrive() const;
    bool IsRemote() const;
    bool IsSUBSTed() const;
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override;

private:
    CDrivesList* m_List; // Backpointer
    std::wstring m_Path; // e.g. "C:\"
    HICON m_Icon = nullptr; // Cached icon icon
    bool m_IsRemote; // Whether the drive type is DRIVE_REMOTE (network drive)

    bool m_Querying = true; // Information thread is running.
    bool m_Success = false; // Drive is accessible. false while m_Querying is true.
    bool m_Subst = false; // Drive is subst'd

    std::wstring m_Name; // e.g. "BOOT (C:)"
    ULONGLONG m_TotalBytes = 0; // Capacity
    ULONGLONG m_FreeBytes = 0;  // Free space

    double m_Used = 0.0; // used space / total space
};

//
// CDriveInformationThread. Does the GetVolumeInformation() call, which
// may hang for ca. 30 sec, it a network drive is not accessible.
//
class CDriveInformationThread final : public CWinThread
{
    // Set of all running CDriveInformationThreads.
    // Used by InvalidateDialogHandle().
    static std::unordered_set<CDriveInformationThread*> _runningThreads;
    static std::shared_mutex _mutexRunningThreads;

    // The objects register and unregister themselves in _runningThreads
    void AddRunningThread();
    void RemoveRunningThread();

public:
    static void InvalidateDialogHandle();

    CDriveInformationThread(const std::wstring& path, LPARAM driveItem, HWND dialog, UINT serial);
    BOOL InitInstance() override;

    LPARAM GetDriveInformation(bool& success, std::wstring& name, ULONGLONG& total, ULONGLONG& free) const;

private:
    const std::wstring m_Path; // Path like "C:\"
    const LPARAM m_DriveItem;  // The list item, we belong to

    std::shared_mutex m_Mutex; // for m_Dialog
    HWND m_Dialog;             // synchronized by m_Cs
    const UINT m_Serial;       // serial number of m_Dialog

    // "[out]"-parameters
    std::wstring m_Name;            // Result: name like "BOOT (C:)", valid if m_Success
    ULONGLONG m_TotalBytes = 0; // Result: capacity of the drive, valid if m_Success
    ULONGLONG m_FreeBytes = 0;  // Result: free space on the drive, valid if m_Success
    bool m_Success = false;     // Result: false, iff drive is inaccessible.
};

//
// CDrivesList.
//
class CDrivesList final : public COwnerDrawnListControl
{
    DECLARE_DYNAMIC(CDrivesList)

    CDrivesList();
    CDriveItem* GetItem(int i) const;
    void SelectItem(const CDriveItem* item);
    bool IsItemSelected(int i) const;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
    afx_msg void OnDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
};

//
// CSelectDrivesDlg. The initial dialog, where the user can select
// one or more drives or a folder for scanning.
//
class CSelectDrivesDlg final : public CDialogEx
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

protected:

    // Dialog Data
    BOOL m_ScanDuplicates = false; // whether duplicate scanning is enable
    int m_Radio = 0;          // out.
    CStringW m_FolderName;    // out. Valid if m_Radio = RADIO_TARGET_FOLDER
    std::vector<std::wstring> m_Drives;    // out. Valid if m_Radio != RADIO_TARGET_FOLDER
    static UINT _serial; // Each Instance of this dialog gets a serial number
    CDrivesList m_List;
    CMFCEditBrowseCtrl m_Browse;
    CButton m_OkButton;
    std::vector<std::wstring> m_SelectedDrives;
    CLayout m_Layout;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedUpdateButtons();
    afx_msg void OnEnChangeFolderName();
    afx_msg void OnLvnItemChangedDrives(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnWmuOk(WPARAM, LPARAM);
    afx_msg LRESULT OnWmDriveInfoThreadFinished(WPARAM, LPARAM lparam);
    afx_msg void OnSysColorChange();
    afx_msg void OnBnClickedRadioTargetDrivesSubset();
    afx_msg void OnBnClickedRadioTargetFolder();
    afx_msg void OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult);
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
};
