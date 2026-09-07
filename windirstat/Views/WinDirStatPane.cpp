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

#include "pch.h"
#include "WinDirStatPane.h"

void CWinDirStatPane::PostNcDestroy()
{
    delete this;
}

void CWinDirStatPane::OnPaint()
{
    CPaintDC dc(this);
    OnDraw(&dc);
}

void CWinDirStatPane::OnDraw(CDC* /*pDC*/)
{
}

int CWinDirStatPane::OnMouseActivate(CWnd* pDesktopWnd, const UINT nHitTest, const UINT message)
{
    const int result = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
    if (result != MA_NOACTIVATE && result != MA_NOACTIVATEANDEAT)
    {
        if (const HWND focus = ::GetFocus(); m_hWnd != focus && !::IsChild(m_hWnd, focus) && IsTopParentActive())
        {
            SetFocus();
        }
    }
    return result;
}

void CWinDirStatPane::OnUpdate(CWnd* /*sender*/, MODEL_CHANGE /*change*/, CItem* /*item*/)
{
    InvalidateRect(nullptr);
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

    const CMenu* subMenu = menu.SubmenuAt(0);
    if (subMenu == nullptr) return;

    UINT command;
    do
    {
        command = subMenu->ShowPopup(
            TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            point.x, point.y, GetMainWindow());
        if (command != 0) GetMainWindow()->SendMessage(WM_COMMAND, command);
    } while (std::ranges::find(persistentCommands, command) != persistentCommands.end());
}
