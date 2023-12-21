// sortinglistcontrol.h - Declaration of CSortingListItem and CSortingListControl
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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
//

#pragma once

//
// SSorting. A sorting specification. We sort by column1, and if two items
// equal in column1, we sort them by column2.
//
struct SSorting
{
    SSorting() { column1 = column2 = 0; ascending1 = ascending2 = true; }
    int  column1;
    bool ascending1;
    int  column2;
    bool ascending2;
};

//
// CSortingListItem. An item in a CSortingListControl.
//
class CSortingListItem
{
public:
    virtual CString GetText(int subitem) const;
    virtual int GetImage() const;
    virtual int Compare(const CSortingListItem *other, int subitem) const;
    int CompareS(const CSortingListItem *other, const SSorting& sorting) const;
};


//
// CSortingListControl. The base class for all our ListControls.
// The lParams of the items are pointers to CSortingListItems.
// The items use LPSTR_TEXTCALLBACK and I_IMAGECALLBACK.
// And the items can compare to one another.
// CSortingListControl maintains a SSorting and handles clicks
// on the header items. It also indicates the sorting to the user
// by adding a "<" or ">" to the header items.
//
class CSortingListControl: public CListCtrl
{
    DECLARE_DYNAMIC(CSortingListControl)
public:
    // Construction
    CSortingListControl(LPCTSTR name);
    virtual ~CSortingListControl();

    // Public methods
    void LoadPersistentAttributes();

    void AddExtendedStyle(DWORD exStyle);
    void RemoveExtendedStyle(DWORD exStyle);

    const SSorting& GetSorting();
    void GetSorting(int& sortColumn1, bool& ascending1, int& sortColumn2, bool& ascending2);

    void SetSorting(const SSorting& sorting);
    void SetSorting(int sortColumn1, bool ascending1, int sortColumn2, bool ascending2);
    void SetSorting(int sortColumn, bool ascending);

    void InsertListItem(int i, CSortingListItem *item);
    CSortingListItem *GetSortingListItem(int i);

    // Overridables
    virtual void SortItems();
    virtual bool GetAscendingDefault(int column);
    virtual bool HasImages();

#   if (_MFC_VER <=0x0800)
    BOOL GetColumnOrderArray(LPINT piArray, INT_PTR iCount = -1);
#   endif

private:
    void SavePersistentAttributes();
    static int CALLBACK _CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

    CString m_name; // for persistence
    SSorting m_sorting;

    int m_indicatedColumn;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnHdnItemclick(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnHdnItemdblclick(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnDestroy();
};
