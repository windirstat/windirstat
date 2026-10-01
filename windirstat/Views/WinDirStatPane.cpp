// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "WinDirStatPane.h"

void CWinDirStatPane::OnPaint()
{
    CPaintDC dc(this);
    OnDraw(&dc);
}

void CWinDirStatPane::OnDraw(CDC* /*pDC*/)
{
}

int CWinDirStatPane::OnMouseActivate(WindowRef pDesktopWnd, const UINT nHitTest, const UINT message)
{
    const int result = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
    if (result != MA_NOACTIVATE && result != MA_NOACTIVATEANDEAT)
    {
        if (const HWND focus = ::GetFocus(); m_hWnd != focus && !IsChild(focus) && IsTopParentActive())
        {
            SetFocus();
        }
    }
    return result;
}

void CWinDirStatPane::OnUpdate(CWnd* /*sender*/, MODEL_CHANGE /*change*/, CItem* /*item*/)
{
    Invalidate();
}

bool CWinDirStatPane::OnMouseWheel(const UINT nFlags, const short zDelta, const CPoint pt)
{
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

void CWinDirStatPane::NotifyOtherPanes(const MODEL_CHANGE change, CItem* item)
{
    CWinDirStatModel::Get()->NotifyPanesExcept(this, change, item);
}

void CWinDirStatPane::ShowGraphContextMenu(CItem* clickedItem, const CPoint point,
    const std::span<const UINT> persistentCommands)
{
    if (clickedItem == nullptr) return;

    if (std::ranges::none_of(CWinDirStatModel::Get()->GetAllSelected(),
        [clickedItem](const CItem* selected) {
            return selected == clickedItem || selected->IsAncestorOf(clickedItem);
        }))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, clickedItem);
    }

    CMenu menu = CMenu::LoadResource(IDR_POPUP_MAP);
    if (!menu) return;
    Localization::UpdateMenu(menu);

    const auto subMenu = menu.GetSubMenu(0);
    if (!subMenu) return;

    UINT command;
    do
    {
        command = subMenu.ShowPopup(
            TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            point, GetMainWindow());
        if (command != 0) GetMainWindow()->SendMessage(WM_COMMAND, command);
    } while (std::ranges::contains(persistentCommands, command));
}
