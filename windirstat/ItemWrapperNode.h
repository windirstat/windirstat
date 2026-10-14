// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeListControl.h"

template <typename Node>
class CItemWrapperNode : public CTreeListItem
{
protected:
    std::shared_mutex m_protect;
    std::vector<Node*> m_children;
    CItem* m_item = nullptr;

    std::wstring GetLinkedText(const int subitem, const std::span<const ITEMCOLUMNS> mapping) const
    {
        // Render file rows through the linked scan item and each view's column mapping.
        if (m_item == nullptr || subitem < 0 || static_cast<size_t>(subitem) >= mapping.size()) return {};
        return subitem == 0 ? m_item->GetPath() : m_item->GetText(mapping[subitem]);
    }

    int CompareLinked(const CTreeListItem* other, const int subitem,
        const std::span<const ITEMCOLUMNS> mapping, const bool comparePath = false) const
    {
        // Search sorts full paths; other wrappers retain the scan item's sibling order.
        if (m_item == nullptr || subitem < 0 || static_cast<size_t>(subitem) >= mapping.size()) return 0;
        const auto* linked = static_cast<const Node*>(other)->m_item;
        return subitem == 0 && comparePath ? m_item->ComparePath(linked) :
            m_item->CompareSibling(linked, mapping[subitem]);
    }

    HICON GetWrapperIcon(const HICON groupIcon)
    {
        // Cache group and shell icons on visible rows.
        auto* viewState = GetViewState();
        if (viewState == nullptr) return nullptr;

        // Reflect the linked directory's scan state without replacing its cached icon.
        if (m_item != nullptr && m_item->IsTypeOrFlag(ITF_SCANERROR)) return GetIconHandler()->GetScanErrorImage();

        if (viewState->icon != nullptr) return viewState->icon;
        if (m_item == nullptr)
        {
            viewState->icon = groupIcon;
            return viewState->icon;
        }

        CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
            viewState->control, m_item->GetIconPath(), m_item->GetAttributes(), &viewState->icon, nullptr));
        return viewState->icon;
    }

public:
    CItemWrapperNode() = default;
    explicit CItemWrapperNode(CItem* item) : m_item(item) {}
    ~CItemWrapperNode() override
    {
        for (const auto* child : m_children) delete child;
    }

    bool DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state,
        int* width, int* focusLeft) override
    {
        // All wrapper views place their tree column first.
        return subitem == 0 && CTreeListItem::DrawSubItem(COL_NAME, pdc, rc, state, width, focusLeft);
    }

    int GetTreeListChildCount() const override { return static_cast<int>(m_children.size()); }
    CTreeListItem* GetTreeListChild(const int i) const override { return m_children[i]; }
    CItem* GetLinkedItem() noexcept override { return m_item; }
};
