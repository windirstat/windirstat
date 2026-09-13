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
#include "PacMan.h"

class CFileTreeView;
class CTreeListItem;
class CTreeListControl;
class CItem;

// Forward declaration for LOGICAL_FOCUS enum from MainFrame.h
enum LOGICAL_FOCUS : uint8_t;

//
// VIEWSTATE. Data needed to display the item.
// Stored in CTreeListControl for currently visible items only to save memory.
//
struct VIEWSTATE final
{
    CPacman pacman;
    std::wstring owner; // Owner of file or folder
    CSmallRect rcPlusMinus{}; // Coordinates of the little +/- rectangle, relative to the upper left corner of the item.
    CSmallRect rcTitle{}; // Coordinates of the label, relative to the upper left corner of the item.
    CTreeListControl* control = nullptr;
    HICON icon = nullptr;  // -1 as long as not needed, >= 0: valid index in IconHandler.
    unsigned char indent = 0; // 0 for the root item, 1 for its children, and so on.
    bool isExpanded = false; // Whether item is expanded.

    VIEWSTATE(const unsigned char iIndent, CTreeListControl* ctrl)
        : control(ctrl), indent(iIndent) {}
};

//
// CTreeListItem. An item in the CTreeListControl. (CItem is derived from CTreeListItem.)
//
class CTreeListItem : public CWdsListItem
{
public:
    CTreeListItem() = default;

    virtual int CompareSibling(const CTreeListItem* tlib, int subitem) const = 0;

    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override;
    int Compare(const CWdsListItem* baseOther, int subitem) const override;
    virtual CTreeListItem* GetTreeListChild(int i) const = 0;
    virtual int GetTreeListChildCount() const = 0;
    virtual CItem* GetLinkedItem() { return reinterpret_cast<CItem*>(this); }
    virtual CTreeListItem* GetAncestorCheckItem() { return this; }

    void DrawPacman(CDC* pdc, const CRect& rc) const;
    CTreeListItem* GetParent() const { return m_parent; }
    void SetParent(CTreeListItem* parent) { m_parent = parent; }
    bool IsAncestorOf(const CTreeListItem* item) const;
    bool HasChildren() const { return GetTreeListChildCount() > 0; }
    bool IsExpanded() const;
    void SetExpanded(bool expanded = true) const;
    bool IsVisible() const override;
    void SetVisible(CTreeListControl * control, bool visible = true);
    unsigned char GetIndent() const;
    CRect GetPlusMinusRect() const;
    void SetPlusMinusRect(const CRect& rc) const;
    CRect GetTitleRect() const;
    void SetTitleRect(const CRect& rc) const;
    void StartPacman() const;
    void StopPacman() const;
    void DrivePacman() const;

protected:
    // Call only from the UI thread; this does not keep the item visible.
    VIEWSTATE* GetViewState() const;

private:
    CTreeListItem* m_parent = nullptr;
};

//
// CTreeListControl. A CListCtrl, which additionally behaves and looks like a tree control.
//
class CTreeListControl : public MessageTarget<CTreeListControl, CWdsListControl>
{
public:
    CTreeListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths, std::vector<int>* columnVisibility, LOGICAL_FOCUS logicalFocus, bool blockFirstColumnReorder);
    ~CTreeListControl() override;
    virtual bool CreateExtended(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    virtual void SetRootItem(CTreeListItem* root = nullptr);
    virtual void AfterDeleteAllItems() {}
    bool DeleteAllItems();
    void OnChildAdded(const CTreeListItem* parent, CTreeListItem* child);
    void OnChildrenAdded(const CTreeListItem* parent, std::span<CTreeListItem* const> children);
    void OnChildRemoved(const CTreeListItem* parent, const CTreeListItem* child);
    void OnRemovingAllChildren(const CTreeListItem* parent);
    CTreeListItem* GetItem(const int i) const { return reinterpret_cast<CTreeListItem*>(CWdsListControl::GetItem(i)); }
    bool IsItemSelected(const CTreeListItem* item) const;
    void SelectItem(const CTreeListItem* item, bool deselect = false, bool focus = false, bool scroll = false);
    void ExpandPathToItem(const CTreeListItem* item);
    void DrawNode(CDC* pDC, CRect& rcRest, CRect& rcPlusMinus, const CTreeListItem* item, int* width) const;
    void EnsureItemVisible(const CTreeListItem* item);
    void ExpandItem(const CTreeListItem* item) { ExpandItem(FindTreeItem(item), false); }
    int FindTreeItem(const CTreeListItem* item) const { return FindListItem(item); }
    bool SelectedItemCanToggle();
    void ToggleSelectedItem();
    void EmulateInteractiveSelection(const CTreeListItem* item);

    template <class T = CTreeListItem> std::vector<T*> GetAllSelected(bool visual = false)
    {
        std::vector<T*> array;
        for (int i = GetFirstSelectedIndex(); i >= 0; i = GetNextSelectedIndex(i))
        {
            const auto item = visual ? reinterpret_cast<T*>(GetItem(i)) :
                reinterpret_cast<T*>(GetItem(i)->GetLinkedItem());
            if (item != nullptr) array.push_back(item);
        }
        return array;
    }

    template <class T = CTreeListItem> T* GetFirstSelectedItem()
    {
        const int selected = GetFirstSelectedIndex();
        if (selected < 0 || GetNextSelectedIndex(selected) >= 0) return nullptr;
        return reinterpret_cast<T*>(GetItem(selected));
    }

protected:
    void OnItemContextMenu(CPoint pt) override;
    void OnItemDoubleClick(int i);
    void InsertItem(int i, CTreeListItem* item);
    bool DeleteItem(int i) override;
    void CollapseItem(int i);
    void ExpandItem(int i, bool scroll = true);
    void ToggleExpansion(int i);

    //
    /////////////////////////////////////////////////////

    int m_lButtonDownItem = -1;        // Set in OnLButtonDown(). -1 if no item is hit.
    bool m_lButtonDownOnPlusMinusRect = false; // Set in OnLButtonDown(). True if plus-minus-rect is hit.
    LOGICAL_FOCUS m_logicalFocus = static_cast<LOGICAL_FOCUS>(0);
    bool m_blockFirstColumnReorder = false;
    CMenu m_contextMenu; // Keep the menu alive until its queued WM_MENUCOMMAND is dispatched.

private:
    friend class CTreeListItem;
    void ClearViewStates();
    inline static std::shared_mutex s_viewStateMutex;
    inline static std::unordered_map<const CTreeListItem*, VIEWSTATE> s_viewStates;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnSetFocus(CWnd* pOldWnd);
    bool OnHeaderEndDrag(UINT, NMHDR* pNMHDR, LRESULT* pResult) const;
    LRESULT OnSelectionChanged(WPARAM wParam, LPARAM lParam) override;
};

inline std::span<const RouteEntry> CTreeListControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&OnKeyDown>(WM_KEYDOWN),
        Route::Window<&OnLButtonDblClk>(WM_LBUTTONDBLCLK),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Notify<&OnHeaderEndDrag>(HDN_ENDDRAG, 0),
    };
    return entries;
}
