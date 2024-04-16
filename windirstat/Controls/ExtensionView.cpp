// ExtensionView.cpp - Implementation of CExtensionListControl and CExtensionView
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

#include "stdafx.h"
#include "WinDirStat.h"
#include "Item.h"
#include "MainFrame.h"
#include "DirStatDoc.h"
#include <common/CommonHelpers.h>
#include "ExtensionView.h"
#include "Localization.h"
#include "ExtensionListControl.h"

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CExtensionView, CView)

BEGIN_MESSAGE_MAP(CExtensionView, CView)
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

CExtensionView::CExtensionView()
    : m_extensionListControl(this) {};

void CExtensionView::SysColorChanged()
{
    m_extensionListControl.SysColorChanged();
}

bool CExtensionView::IsShowTypes() const
{
    return m_showTypes;
}

void CExtensionView::ShowTypes(bool show)
{
    m_showTypes = show;
    OnUpdate(nullptr, 0, nullptr);
}

void CExtensionView::SetHighlightExtension(const LPCWSTR ext)
{
    GetDocument()->SetHighlightExtension(ext);
    if (GetFocus() == &m_extensionListControl)
    {
        GetDocument()->UpdateAllViews(this, HINT_EXTENSIONSELECTIONCHANGED);
    }
}

int CExtensionView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_extensionListControl.Create(LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_REPORT, rect, this, ID_WDS_CONTROL));
    m_extensionListControl.SetExtendedStyle(m_extensionListControl.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);

    m_extensionListControl.ShowGrid(COptions::ListGrid);
    m_extensionListControl.ShowStripes(COptions::ListStripes);
    m_extensionListControl.ShowFullRowSelection(COptions::ListFullRowSelection);

    m_extensionListControl.Initialize();
    return 0;
}

void CExtensionView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject*)
{
    switch (lHint)
    {
    case HINT_NEWROOT:
    case HINT_NULL:
        if (IsShowTypes() && GetDocument()->IsRootDone())
        {
            m_extensionListControl.SetRootSize(GetDocument()->GetRootSize());
            m_extensionListControl.SetExtensionData(GetDocument()->GetExtensionData());

            // If there is no vertical scroll bar, the header control doen't repaint
            // correctly. Don't know why. But this helps:
            m_extensionListControl.GetHeaderCtrl()->InvalidateRect(nullptr);
        }
        else
        {
            m_extensionListControl.DeleteAllItems();
        }

        [[fallthrough]];
    case HINT_SELECTIONREFRESH:
        if (IsShowTypes())
        {
            SetSelection();
        }
        break;

    case HINT_ZOOMCHANGED:
        break;

    case HINT_TREEMAPSTYLECHANGED:
        {
            InvalidateRect(nullptr);
            m_extensionListControl.InvalidateRect(nullptr);
            m_extensionListControl.GetHeaderCtrl()->InvalidateRect(nullptr);
        }
        break;

    case HINT_LISTSTYLECHANGED:
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

void CExtensionView::SetSelection()
{
    const auto & items = CFileTreeControl::Get()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        if (item->GetType() != IT_FILE)
        {
            m_extensionListControl.EnsureVisible(0, false);
        }
        else
        {
            m_extensionListControl.SelectExtension(item->GetExtension());
        }
    }
}

void CExtensionView::OnDraw(CDC* pDC)
{
    CView::OnDraw(pDC);
}

void CExtensionView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (::IsWindow(m_extensionListControl.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_extensionListControl.MoveWindow(rc);
    }
}

void CExtensionView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_extensionListControl.SetFocus();
}
