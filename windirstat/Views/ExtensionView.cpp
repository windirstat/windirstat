// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ExtensionView.h"
#include "ExtensionListControl.h"

/////////////////////////////////////////////////////////////////////////////

CExtensionView::CExtensionView()
    : m_extensionListControl(this) {}

void CExtensionView::SysColorChanged()
{
    m_extensionListControl.SysColorChanged();
}

void CExtensionView::ShowTypes(const bool show)
{
    m_showTypes = show;
    OnUpdate(nullptr, MODEL_CHANGE_NONE, nullptr);
}

void CExtensionView::SetHighlightExtension(const std::wstring & ext, const bool unregistered)
{
    CWinDirStatModel::Get()->SetHighlightExtension(ext, unregistered);
    if (m_extensionListControl.HasFocus())
    {
        NotifyOtherPanes(MODEL_CHANGE_EXTENSION_SELECTION);
    }
}

int CExtensionView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    m_extensionListControl.Create(LVS_OWNERDATA | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_REPORT, rect, this, ID_WDS_CONTROL);
    m_extensionListControl.SetExtendedStyle(m_extensionListControl.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);
    m_extensionListControl.ShowGrid(COptions::ListGrid);
    m_extensionListControl.ShowStripes(COptions::ListStripes);
    m_extensionListControl.ShowFullRowSelection(COptions::ListFullRowSelection);

    m_extensionListControl.Initialize();
    return 0;
}

void CExtensionView::OnUpdate(CWnd* /*sender*/, const MODEL_CHANGE change, CItem* /*item*/)
{
    switch (change)
    {
    case MODEL_CHANGE_NEW_ROOT:
    case MODEL_CHANGE_NONE:
        RefreshData();

        // If there is no vertical scroll bar, the header control doesn't repaint
        // correctly. Don't know why. But this helps:
        if (IsShowTypes() && CWinDirStatModel::Get()->IsRootDone())
            m_extensionListControl.GetHeader().Invalidate();

        [[fallthrough]];
    case MODEL_CHANGE_SELECTION_REFRESH:
        if (IsShowTypes())
        {
            // Navigating to another folder updates the list to that folder's files
            if (change == MODEL_CHANGE_SELECTION_REFRESH && GetScopeItem() != m_scope) RefreshData();
            SetSelection();
        }
        break;

    case MODEL_CHANGE_ZOOM:
        break;

    case MODEL_CHANGE_TREEMAP_STYLE:
        {
            Invalidate();
            m_extensionListControl.Invalidate();
            m_extensionListControl.GetHeader().Invalidate();
        }
        break;

    case MODEL_CHANGE_LIST_STYLE:
        {
            m_extensionListControl.ShowGrid(COptions::ListGrid);
            m_extensionListControl.ShowStripes(COptions::ListStripes);
            m_extensionListControl.ShowFullRowSelection(COptions::ListFullRowSelection);
        }
        break;
    default:
        break;
    }
}

// The folder selected in the file tree (the parent for a selected file), or null when the
// whole scan applies: nothing selected, the root selected, or no usable folder.
const CItem* CExtensionView::GetScopeItem() const
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    const auto& items = CFileTreeControl::Get()->GetAllSelected<CItem>();
    if (items.empty()) return nullptr;

    const CItem* scope = items.front();
    if (!scope->IsTypeOrFlag(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE)) scope = scope->GetParent();
    return scope == root ? nullptr : scope;
}

void CExtensionView::RefreshData()
{
    CWinDirStatModel* model = CWinDirStatModel::Get();
    if (!IsShowTypes() || !model->IsRootDone())
    {
        m_scope = nullptr;
        m_extensionListControl.SetExtensionData(nullptr);
        return;
    }

    m_scope = GetScopeItem();
    if (m_scope == nullptr)
    {
        m_extensionListControl.SetRootSize(model->GetRootSize());
        m_extensionListControl.SetExtensionData(model->GetExtensionData());
        return;
    }

    // Accumulate the files below the folder; colors stay those of the whole scan
    CExtensionData local;
    const CExtensionData& global = *model->GetExtensionData();
    std::vector<const CItem*> stack{ m_scope };
    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();
        if (item->IsTypeOrFlag(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE))
        {
            for (const CItem* child : item->GetChildren()) stack.push_back(child);
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            const std::wstring& ext = item->GetExtension();
            const auto [it, inserted] = local.try_emplace(ext);
            if (inserted)
            {
                if (const auto found = global.find(ext); found != global.end()) it->second.color = found->second.color;
            }
            it->second.AddFile(item->GetSizeLogical());
        }
    }

    m_extensionListControl.SetRootSize(m_scope->GetSizeLogical());
    m_extensionListControl.SetExtensionData(&local);
}

void CExtensionView::SetSelection()
{
    // Get first extension from selected items
    const auto & items = CFileTreeControl::Get()->GetAllSelected<CItem>();
    const CItem* validItem = nullptr;
    for (const auto& item : items)
    {
        if (item->IsTypeOrFlag(IT_FILE))
        {
            validItem = item;
            break;
        }
    }

    // Set selection if not already selected
    if (validItem == nullptr) return;
    if (const std::wstring& ext = validItem->GetExtension();
        m_extensionListControl.GetSelectedExtension() != ext)
    {
        m_extensionListControl.SelectExtension(ext);
    }
}

void CExtensionView::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (IsWindow(m_extensionListControl.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_extensionListControl.MoveWindow(rc);
    }
}

void CExtensionView::OnSetFocus(WindowRef /*pOldWnd*/)
{
    m_extensionListControl.SetFocus();
}
