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

class CWdsListControl;

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

    bool operator==(const SSorting&) const = default;
};

//
// CWdsListItem. An item in a CWdsListControl.
// Some columns (subitems) may be owner drawn (DrawSubItem() returns true),
// CWdsListControl draws the texts (GetText()) of all others.
// DrawLabel() draws a standard label (width icon, text, selection and focus rect)
//

class CWdsListItem
{
public:
    CWdsListItem() = default;
    virtual ~CWdsListItem() = default;

    // This text is drawn, if DrawSubItem returns false
    virtual std::wstring GetText(int subitem) const = 0;
    // This color is used for the current item
    virtual COLORREF GetItemTextColor() const
    {
        return DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
    }

    // Comparison methods for sorting
    virtual int Compare(const CWdsListItem* other, int subitem) const = 0;
    int CompareSort(const CWdsListItem* other, const SSorting& sorting) const;

    // Return value is true, if the item draws itself.
    // width != nullptr -> only determine width, do not draw.
    // If focus rectangle shall not begin leftmost, set *focusLeft
    // to the left edge of the desired focus rectangle.
    virtual bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) = 0;
    virtual void DrawAdditionalState(CDC* /*pdc*/, const CRect& /*rcLabel*/) const {}
    void DrawSelection(const CWdsListControl* list, CDC* pdc, CRect rc, UINT state) const;
    virtual HICON GetIcon() = 0;
    virtual bool IsVisible() const { return true; }
    void DrawLabel(const CWdsListControl* list, CDC* pdc, CRect& rc, UINT state, int* width, int* focusLeft, bool indent = true);
    void DrawPercentage(CDC* pdc, CRect rc, double fraction, COLORREF color) const;
};

//
// CSetRedrawLock. RAII wrapper for SetRedraw(FALSE)/SetRedraw(TRUE).
//
class CSetRedrawLock final
{
public:
    explicit CSetRedrawLock(CWnd* wnd) : m_wnd(wnd) { m_wnd->SetRedraw(FALSE); }
    ~CSetRedrawLock() { m_wnd->SetRedraw(TRUE); m_wnd->Invalidate(); }
    CSetRedrawLock(const CSetRedrawLock&) = delete;
    CSetRedrawLock& operator=(const CSetRedrawLock&) = delete;

private:
    CWnd* m_wnd;
};

//
// CWdsListControl. Must be report view. Deals with CWdsListItems.
// Can have a grid or not (own implementation, don't set LVS_EX_GRIDLINES). Flicker-free.
// Also handles sorting functionality (merged from CSortingListControl).
//
class CWdsListControl : public CListCtrl
{
    DECLARE_DYNAMIC(CWdsListControl)

public:
    CWdsListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths, std::vector<int>* columnVisibility);
    ~CWdsListControl() override = default;
    void OnColumnsInserted(std::initializer_list<int> requiredColumns = {}, std::initializer_list<int> defaultHiddenColumns = {});
    virtual void SysColorChanged();

    int GetRowHeight() const;
    void CalculateRowHeight();
    void ShowGrid(bool show);
    void ShowStripes(bool show);
    void ShowFullRowSelection(bool show);
    bool IsFullRowSelection() const;

    COLORREF GetHighlightColor() const;
    COLORREF GetHighlightTextColor() const;

    bool IsItemStripColor(int i) const;
    COLORREF GetItemBackgroundColor(int i) const;
    COLORREF GetItemSelectionBackgroundColor(int i) const;
    COLORREF GetItemSelectionTextColor(int i) const;

    CWdsListItem* GetItem(int i) const;
    int FindListItem(const CWdsListItem* item) const;
    int GetGeneralLeftIndent() const;
    CRect GetWholeSubitemRect(int item, int subitem) const;
    void LoadPersistentAttributes();
    bool HasFocus() const;
    void InsertListItem(int i, std::span<CWdsListItem* const> items);
    void InsertListItem(int i, CWdsListItem* item) { InsertListItem(i, std::span<CWdsListItem* const>(&item, 1)); }
    void RemoveListItem(int i, int c = 1);

    // Shadow CListCtrl methods for Owner Data management.
    // Use these instead of standard CListCtrl methods to ensure proper data management in LVS_OWNERDATA mode.
    BOOL DeleteItem(int i);
    BOOL DeleteAllItems();

    // Sorting functionality
    int ColumnToSubItem(int col) const;
    int SubItemToColumn(int subitem) const;
    bool IsColumnVisible(int subitem) const;
    void SetColumnVisible(int subitem, bool visible);
    void SetSorting(const SSorting& sorting);
    void SetSorting(int sortColumn1, bool ascending1, int sortColumn2, bool ascending2);
    void SetSorting(int sortColumn, bool ascending);
    virtual void SortItems();
    virtual bool GetAscendingDefault(int column);
    int GetItemCount() const noexcept { return static_cast<int>(m_items.size()); }
    void SetOwnsItems(const bool owns) { m_ownsItems = owns; }

    // Selection change batching
    void PostSelectionChanged();
    void DeselectAll();
    CFont* GetFont() const;

protected:
    void InitializeColors();
    void ApplyColumnVisibility(int column);
    void DrawItem(LPDRAWITEMSTRUCT pdis) override;
    int GetSubItemWidth(CWdsListItem* item, int subitem, CDC* pDC = nullptr);
    bool IsColumnRequired(int subitem) const;
    virtual void OnItemContextMenu(CPoint /*point*/) {}
    void SavePersistentAttributes() const;
    void ShowColumnContextMenu(CPoint point);

    // Owner-drawn related members
    std::vector<CWdsListItem*> m_items;
    std::unordered_map<CWdsListItem*, int> m_itemMap;
    bool m_ownsItems = false;
    COLORREF m_windowColor = CLR_NONE; // The default background color if !m_showStripes
    COLORREF m_stripeColor = CLR_NONE; // The stripe color, used for every other item if m_showStripes
    int m_rowHeight = 20;              // Height of an item
    int m_columnCount = 0;
    bool m_showGrid = false;           // Whether to draw a grid
    bool m_showStripes = false;        // Whether to show stripes
    bool m_showFullRowSelect = false;  // Whether to draw full row selection

    // Sorting related members (merged from CSortingListControl)
    std::vector<int>* m_columnOrder = nullptr;
    std::vector<int>* m_columnWidths = nullptr;
    std::vector<int>* m_columnVisibility = nullptr;
    std::vector<int> m_defaultColumnWidths;
    std::vector<int> m_requiredColumns;
    SSorting m_sorting;
    int m_indicatedColumn = -1;

    // Selection change batching
    static constexpr DWORD WM_SELECTION_CHANGED = WM_APP + 1;
    bool m_selectionChangePending = false;
    mutable HFONT m_cachedFont = NULL;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemchanging(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
    afx_msg virtual LRESULT OnSelectionChanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSetFont(WPARAM wParam, LPARAM lParam);
};
