// TypeView.h - Declaration of CExtensionListControl and CTypeView
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

#include "DirStatDoc.h" // SExtensionRecord

class CTypeView;

//
// CExtensionListControl.
//
class CExtensionListControl final : public COwnerDrawnListControl
{
protected:
    // Columns
    enum
    {
        COL_EXTENSION,
        COL_COLOR,
        COL_DESCRIPTION,
        COL_BYTES,
        COL_BYTESPERCENT,
        COL_FILES
    };

    // CListItem. The items of the CExtensionListControl.
    class CListItem final : public COwnerDrawnListItem
    {
    public:
        CListItem(CExtensionListControl* list, LPCWSTR extension, const SExtensionRecord& r);

        bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
        CStringW GetText(int subitem) const override;

        CStringW GetExtension() const;
        int GetImage() const override;
        int Compare(const CSortingListItem* baseOther, int subitem) const override;

    private:
        void DrawColor(CDC* pdc, CRect rc, UINT state, int* width) const;

        CStringW GetDescription() const;
        CStringW GetBytesPercent() const;

        double GetBytesFraction() const;

        CExtensionListControl* m_list;
        CStringW m_extension;
        SExtensionRecord m_record;
        mutable CStringW m_description;
        mutable int m_image;
    };

public:
    CExtensionListControl(CTypeView* typeView);
    bool GetAscendingDefault(int column) override;
    void Initialize();
    void SetExtensionData(const CExtensionData* ed);
    void SetRootSize(ULONGLONG totalBytes);
    ULONGLONG GetRootSize() const;
    void SelectExtension(LPCWSTR ext);
    CStringW GetSelectedExtension() const;

protected:
    CListItem* GetListItem(int i) const;

    CTypeView* m_typeView;
    ULONGLONG m_rootSize;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnDestroy();
    afx_msg void OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};

//
// CTypeView. The upper right view, which shows the extensions and their
// cushion colors.
//
class CTypeView final : public CView
{
protected:
    CTypeView();
    DECLARE_DYNCREATE(CTypeView)

    ~CTypeView() override = default;
    CDirStatDoc* GetDocument() const
    {
        return static_cast<CDirStatDoc*>(m_pDocument);
    }
    void SysColorChanged();
    bool IsShowTypes() const;
    void ShowTypes(bool show);

    void SetHighlightExtension(LPCWSTR ext);

protected:
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
    void OnDraw(CDC* pDC) override;
    void SetSelection();

    bool m_showTypes;                             // Whether this view shall be shown (F8 option)
    CExtensionListControl m_extensionListControl; // The list control

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
};
