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
// CTreeListItem. An item in the CTreeListControl. (CItem is derived from CTreeListItem.)
// In order to save memory, once the item is actually inserted in the List,
// we allocate the VISIBLEINFO structure (m_visualInfo).
// m_visualInfo is freed as soon as the item is removed from the List.
//
class CTreeListItem : public CWdsListItem
{
    // Data needed to display the item.
    struct VISIBLEINFO final
    {
        CPacman pacman;
        std::wstring owner; // Owner of file or folder
        CSmallRect rcPlusMinus{}; // Coordinates of the little +/- rectangle, relative to the upper left corner of the item.
        CSmallRect rcTitle{}; // Coordinates of the label, relative to the upper left corner of the item.
        CTreeListControl* control = nullptr;
        HICON icon = nullptr;  // -1 as long as not needed, >= 0: valid index in IconHandler.
        unsigned char indent; // 0 for the root item, 1 for its children, and so on.
        bool isExpanded = false; // Whether item is expanded.

        VISIBLEINFO(const unsigned char iIndent) : indent(iIndent) {}
    };

public:
    CTreeListItem() = default;

    virtual int CompareSibling(const CTreeListItem* tlib, int subitem) const = 0;

    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override { return m_visualInfo->icon; }
    int Compare(const CWdsListItem* baseOther, int subitem) const override;
    virtual CTreeListItem* GetTreeListChild(int i) const = 0;
    virtual int GetTreeListChildCount() const = 0;
    virtual CItem* GetLinkedItem() { return reinterpret_cast<CItem*>(this); }

    void DrawPacman(CDC* pdc, const CRect& rc) const;
    CTreeListItem* GetParent() const;
    void SetParent(CTreeListItem* parent);
    bool IsAncestorOf(const CTreeListItem* item) const;
    bool HasChildren() const;
    bool IsExpanded() const;
    void SetExpanded(bool expanded = true) const;
    bool IsVisible() const { return m_visualInfo.get() != nullptr; }
    void SetVisible(CTreeListControl * control, bool visible = true);
    unsigned char GetIndent() const;
    void SetIndent(unsigned char indent) const;
    CRect GetPlusMinusRect() const;
    void SetPlusMinusRect(const CRect& rc) const;
    CRect GetTitleRect() const;
    void SetTitleRect(const CRect& rc) const;
    int GetScrollPosition() const;
    void SetScrollPosition(int top) const;
    void StartPacman() const;
    void StopPacman() const;
    void DrivePacman() const;

protected:
    std::unique_ptr<VISIBLEINFO> m_visualInfo;

private:
    CTreeListItem* m_parent = nullptr;
};

//
// CTreeListControl. A CListCtrl, which additionally behaves and looks like a tree control.
//
class CTreeListControl : public CWdsListControl
{
    DECLARE_DYNAMIC(CTreeListControl)

    CTreeListControl(std::vector<int>* columnOrder = {}, std::vector<int>* columnWidths = {}, LOGICAL_FOCUS logicalFocus = static_cast<LOGICAL_FOCUS>(0), bool blockFirstColumnReorder = false);
    ~CTreeListControl() override = default;
    virtual BOOL CreateExtended(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    void SysColorChanged() override;
    virtual void SetRootItem(CTreeListItem* root = nullptr);
    virtual void AfterDeleteAllItems() {}
    void OnChildAdded(const CTreeListItem* parent, CTreeListItem* child);
    void OnChildRemoved(const CTreeListItem* parent, const CTreeListItem* child);
    void OnRemovingAllChildren(const CTreeListItem* parent);
    CTreeListItem* GetItem(int i) const;
    bool IsItemSelected(const CTreeListItem* item) const;
    void SelectItem(const CTreeListItem* item, bool deselect = false, bool focus = false);
    void ExpandPathToItem(const CTreeListItem* item);
    void DrawNode(CDC* pdc, CRect& rc, CRect& rcPlusMinus, const CTreeListItem* item, int* width);
    void EnsureItemVisible(const CTreeListItem* item);
    void ExpandItem(const CTreeListItem* item);
    int FindTreeItem(const CTreeListItem* item) const;
    int GetItemScrollPosition(const CTreeListItem* item) const;
    void SetItemScrollPosition(const CTreeListItem* item, int top);
    bool SelectedItemCanToggle();
    void ToggleSelectedItem();
    void EmulateInteractiveSelection(const CTreeListItem* item);

    template <class T = CTreeListItem> std::vector<T*> GetAllSelected(bool visual = false)
    {
        std::vector<T*> array;
        for (POSITION pos = GetFirstSelectedItemPosition(); pos != nullptr;)
        {
            const int i = GetNextSelectedItem(pos);
            const auto item = visual ? reinterpret_cast<T*>(GetItem(i)) :
                reinterpret_cast<T*>(GetItem(i)->GetLinkedItem());
            if (item != nullptr) array.push_back(item);
        }
        return array;
    }

    template <class T = CTreeListItem> T* GetFirstSelectedItem()
    {
        POSITION pos = GetFirstSelectedItemPosition();
        if (pos == nullptr) return nullptr;
        const int i = GetNextSelectedItem(pos);
        if (GetNextSelectedItem(pos) != -1) return nullptr;

        return reinterpret_cast<T*>(GetItem(i));
    }

protected:
    void OnItemDoubleClick(int i);
    void InsertItem(int i, CTreeListItem* item);
    void DeleteItem(int i);
    void CollapseItem(int i);
    void ExpandItem(int i, bool scroll = true);
    void ToggleExpansion(int i);

    //
    /////////////////////////////////////////////////////

    int m_lButtonDownItem = -1;        // Set in OnLButtonDown(). -1 if not item hit.
    bool m_lButtonDownOnPlusMinusRect = false; // Set in OnLButtonDown(). True, if plus-minus-rect hit.
    LOGICAL_FOCUS m_logicalFocus = static_cast<LOGICAL_FOCUS>(0);
    bool m_blockFirstColumnReorder = false;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLvnItemChangingList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg BOOL OnHeaderEndDrag(UINT, NMHDR* pNMHDR, LRESULT* pResult);
};
