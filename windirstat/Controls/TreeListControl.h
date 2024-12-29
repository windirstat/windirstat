// TreeListControl.h - Declaration of CTreeListItem and CTreeListControl
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

#include "OwnerDrawnListControl.h"
#include "PacMan.h"

#include <vector>
#include <memory>

class CFileTreeView;
class CTreeListItem;
class CTreeListControl;

//
// CTreeListItem. An item in the CTreeListControl. (CItem is derived from CTreeListItem.)
// In order to save memory, once the item is actually inserted in the List,
// we allocate the VISIBLEINFO structure (m_VisualInfo).
// m_VisualInfo is freed as soon as the item is removed from the List.
//
class CTreeListItem : public COwnerDrawnListItem
{
    // Data needed to display the item.
    struct VISIBLEINFO final
    {
        CPacman pacman;
        CRect rcPlusMinus{}; // Coordinates of the little +/- rectangle, relative to the upper left corner of the item.
        CRect rcTitle{}; // Coordinates of the label, relative to the upper left corner of the item.
        std::wstring owner; // Owner of file or folder
        CTreeListControl* control = nullptr;
        HICON icon = nullptr;  // -1 as long as not needed, >= 0: valid index in IconHandler.
        unsigned char indent; // 0 for the root item, 1 for its children, and so on.
        bool isExpanded = false; // Whether item is expanded.

        VISIBLEINFO(const unsigned char iIndent) : indent(iIndent) {}
        ~VISIBLEINFO() { if (icon != nullptr) DestroyIcon(icon); }
    };

public:
    CTreeListItem() = default;

    virtual int CompareSibling(const CTreeListItem* tlib, int subitem) const = 0;

    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    HICON GetIcon() override { return m_VisualInfo->icon; }
    int Compare(const CSortingListItem* baseOther, int subitem) const override;
    virtual CTreeListItem* GetTreeListChild(int i) const = 0;
    virtual int GetTreeListChildCount() const = 0;
    virtual CTreeListItem* GetLinkedItem() { return this; }

    void DrawPacman(const CDC* pdc, const CRect& rc, COLORREF bgColor) const;
    CTreeListItem* GetParent() const;
    void SetParent(CTreeListItem* parent);
    bool IsAncestorOf(const CTreeListItem* item) const;
    bool HasSiblings() const;
    bool HasChildren() const;
    bool IsExpanded() const;
    void SetExpanded(bool expanded = true) const;
    bool IsVisible() const { return m_VisualInfo != nullptr; }
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
    std::unique_ptr<VISIBLEINFO> m_VisualInfo;

private:
    CTreeListItem* m_Parent = nullptr;
};

//
// CTreeListControl. A CListCtrl, which additionally behaves an looks like a tree control.
//
class CTreeListControl : public COwnerDrawnListControl
{
    DECLARE_DYNAMIC(CTreeListControl)

    CTreeListControl(int rowHeight = -1, std::vector<int>* columnOrder = {}, std::vector<int>* columnWidths = {});
    ~CTreeListControl() override = default;
    virtual BOOL CreateExtended(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    void SysColorChanged() override;
    virtual void SetRootItem(CTreeListItem* root);
    void OnChildAdded(const CTreeListItem* parent, CTreeListItem* child);
    void OnChildRemoved(const CTreeListItem* parent, CTreeListItem* child);
    void OnRemovingAllChildren(const CTreeListItem* parent);
    CTreeListItem* GetItem(int i) const;
    bool IsItemSelected(const CTreeListItem* item) const;
    void SelectItem(const CTreeListItem* item, bool deselect = false, bool focus = false);
    void DeselectAll();
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
            const auto item = reinterpret_cast<T*>(visual ? GetItem(i) : GetItem(i)->GetLinkedItem());
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
    virtual void OnItemDoubleClick(int i);
    void InitializeNodeBitmaps();
    void InsertItem(int i, CTreeListItem* item);
    void DeleteItem(int i);
    void CollapseItem(int i);
    void ExpandItem(int i, bool scroll = true);
    void ToggleExpansion(int i);

    //
    /////////////////////////////////////////////////////

    CBitmap m_BmNodes0;                // The bitmaps needed to draw the treecontrol-like branches
    CBitmap m_BmNodes1;                // The same bitmaps with stripe-background color
    int m_LButtonDownItem = -1;        // Set in OnLButtonDown(). -1 if not item hit.
    bool m_LButtonDownOnPlusMinusRect = false; // Set in OnLButtonDown(). True, if plus-minus-rect hit.

    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLvnItemChangingList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
