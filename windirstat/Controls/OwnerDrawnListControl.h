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

class COwnerDrawnListControl;
class CIconHandler;

//
// SSorting. A sorting specification. We sort by column1, and if two items
// equal in column1, we sort them by column2.
//
struct SSorting
{
    int column1 = 0;
    int column2 = 0;
    int subitem1 = 0;
    int subitem2 = 0;
    bool ascending1 = true;
    bool ascending2 = true;
};

//
// COwnerDrawnListItem. An item in a COwnerDrawnListControl.
// Some columns (subitems) may be owner drawn (DrawSubItem() returns true),
// COwnerDrawnListControl draws the texts (GetText()) of all others.
// DrawLabel() draws a standard label (width icon, text, selection and focus rect)
//

class COwnerDrawnListItem
{
public:
    COwnerDrawnListItem() = default;
    virtual ~COwnerDrawnListItem() = default;

    // This text is drawn, if DrawSubItem returns false
    virtual std::wstring GetText(int subitem) const = 0;
    // This color is used for the current item
    virtual COLORREF GetItemTextColor() const
    {
        return DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
    }

    // Comparison methods for sorting
    virtual int Compare(const COwnerDrawnListItem* other, int subitem) const = 0;
    int CompareSort(const COwnerDrawnListItem* other, const SSorting& sorting) const;

    // Return value is true, if the item draws itself.
    // width != NULL -> only determine width, do not draw.
    // If focus rectangle shall not begin leftmost, set *focusLeft
    // to the left edge of the desired focus rectangle.
    virtual bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) = 0;
    virtual void DrawAdditionalState(CDC* /*pdc*/, const CRect& /*rcLabel*/) const {}
    void DrawSelection(const COwnerDrawnListControl* list, CDC* pdc, CRect rc, UINT state) const;
    virtual HICON GetIcon() = 0;
    void DrawLabel(const COwnerDrawnListControl* list, CDC* pdc, CRect& rc, UINT state, int* width, int* focusLeft, bool indent = true);
    void DrawPercentage(CDC* pdc, CRect rc, double fraction, COLORREF color) const;
};

//
// COwnerDrawnListControl. Must be report view. Deals with COwnerDrawnListItems.
// Can have a grid or not (own implementation, don't set LVS_EX_GRIDLINES). Flicker-free.
// Also handles sorting functionality (merged from CSortingListControl).
//
class COwnerDrawnListControl : public CListCtrl
{
    DECLARE_DYNAMIC(COwnerDrawnListControl)

public:
    COwnerDrawnListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths);
    ~COwnerDrawnListControl() override = default;
    void OnColumnsInserted();
    virtual void SysColorChanged();

    int GetRowHeight() const;
    void CalculateRowHeight();
    void ShowGrid(bool show);
    void ShowStripes(bool show);
    void ShowFullRowSelection(bool show);
    bool IsFullRowSelection() const;

    COLORREF GetWindowColor() const;
    COLORREF GetStripeColor() const;
    COLORREF GetHighlightColor() const;
    COLORREF GetNonFocusHighlightColor() const;
    COLORREF GetNonFocusHighlightTextColor() const;
    COLORREF GetHighlightTextColor() const;

    bool IsItemStripColor(int i) const;
    COLORREF GetItemBackgroundColor(int i) const;
    COLORREF GetItemSelectionBackgroundColor(int i) const;
    COLORREF GetItemSelectionTextColor(int i) const;

    COwnerDrawnListItem* GetItem(int i) const;
    int FindListItem(const COwnerDrawnListItem* item) const;
    int GetTextXMargin() const;
    int GetGeneralLeftIndent() const;
    CRect GetWholeSubitemRect(int item, int subitem) const;
    void LoadPersistentAttributes();
    bool HasFocus() const;
    void AddExtendedStyle(DWORD exStyle);
    void RemoveExtendedStyle(DWORD exStyle);
    void InsertListItem(int i, COwnerDrawnListItem* item);

    // Sorting functionality
    const SSorting& GetSorting() const;
    int ColumnToSubItem(int col) const;
    void SetSorting(const SSorting& sorting);
    void SetSorting(int sortColumn1, bool ascending1, int sortColumn2, bool ascending2);
    void SetSorting(int sortColumn, bool ascending);
    virtual void SortItems();
    virtual bool GetAscendingDefault(int column);

protected:
    void InitializeColors();
    void DrawItem(LPDRAWITEMSTRUCT pdis) override;
    int GetSubItemWidth(COwnerDrawnListItem* item, int subitem);
    void SavePersistentAttributes() const;

    // Owner-drawn related members
    COLORREF m_windowColor = CLR_NONE; // The default background color if !m_showStripes
    COLORREF m_stripeColor = CLR_NONE; // The stripe color, used for every other item if m_showStripes
    int m_rowHeight = 20;              // Height of an item
    bool m_showGrid = false;           // Whether to draw a grid
    bool m_showStripes = false;        // Whether to show stripes
    bool m_showFullRowSelect = false;  // Whether to draw full row selection

    // Sorting related members (merged from CSortingListControl)
    std::vector<int>* m_columnOrder = nullptr;
    std::vector<int>* m_columnWidths = nullptr;
    SSorting m_sorting;
    int m_indicatedColumn = -1;

    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemchanging(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
};
