// ExtensionView.h - Declaration of CExtensionListControl and CExtensionView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#include "DirStatDoc.h"

class CExtensionView;

//
// CExtensionListControl.
//
class CExtensionListControl final : public COwnerDrawnListControl
{
protected:
    // Columns
    enum ListColumns
    {
        COL_EXT_EXTENSION,
        COL_EXT_COLOR,
        COL_EXT_DESCRIPTION,
        COL_EXT_BYTES,
        COL_EXT_BYTESPERCENT,
        COL_EXT_FILES
    };

    // CListItem. The items of the CExtensionListControl.
    class CListItem final : public COwnerDrawnListItem
    {
    public:
        CListItem(CExtensionListControl* list, const std::wstring & extension, const SExtensionRecord& r);

        bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
        std::wstring GetText(int subitem) const override;

        std::wstring GetExtension() const;
        int GetImage() const override;
        int Compare(const CSortingListItem* baseOther, int subitem) const override;

    private:
        void DrawColor(CDC* pdc, CRect rc, UINT state, int* width) const;

        std::wstring GetDescription() const;
        std::wstring GetBytesPercent() const;

        double GetBytesFraction() const;

        CExtensionListControl* m_List;
        std::wstring m_Extension;
        SExtensionRecord m_Record;
        mutable std::wstring m_Description;
        mutable int m_Image = -1;
    };

public:
    CExtensionListControl(CExtensionView* extensionView);
    bool GetAscendingDefault(int subitem) override;
    void Initialize();
    void SetExtensionData(const CExtensionData* ed);
    void SetRootSize(ULONGLONG totalBytes);
    ULONGLONG GetRootSize() const;
    void SelectExtension(const std::wstring& ext);
    std::wstring GetSelectedExtension() const;

protected:
    CListItem* GetListItem(int i) const;

    CExtensionView* m_ExtensionView;
    ULONGLONG m_RootSize = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnDestroy();
    afx_msg void OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
