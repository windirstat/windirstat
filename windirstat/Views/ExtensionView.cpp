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
#include "ExtensionView.h"
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

void CExtensionView::ShowTypes(const bool show)
{
    m_showTypes = show;
    OnUpdate(nullptr, 0, nullptr);
}

void CExtensionView::SetHighlightExtension(const std::wstring & ext)
{
    CDirStatDoc::Get()->SetHighlightExtension(ext);
    if (GetFocus() == &m_extensionListControl)
    {
        CDirStatDoc::Get()->UpdateAllViews(this, HINT_EXTENSIONSELECTIONCHANGED);
    }
}

int CExtensionView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
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

void CExtensionView::OnUpdate(CView* /*pSender*/, const LPARAM lHint, CObject*)
{
    switch (lHint)
    {
    case HINT_NEWROOT:
    case HINT_NULL:
        if (IsShowTypes() && CDirStatDoc::Get()->IsRootDone())
        {
            m_extensionListControl.SetRootSize(CDirStatDoc::Get()->GetRootSize());
            m_extensionListControl.SetExtensionData(CDirStatDoc::Get()->GetExtensionData());

            // If there is no vertical scroll bar, the header control doesn't repaint
            // correctly. Don't know why. But this helps:
            m_extensionListControl.GetHeaderCtrl()->InvalidateRect(nullptr);
        }
        else
        {
            m_extensionListControl.SetExtensionData(nullptr);
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
    // Get first extension from selected items
    const auto & items = CFileTreeControl::Get()->GetAllSelected<CItem>();
    CItem* validItem = nullptr;
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

void CExtensionView::OnDraw(CDC* pDC)
{
    CView::OnDraw(pDC);
}

void CExtensionView::OnSize(const UINT nType, const int cx, const int cy)
{
    CView::OnSize(nType, cx, cy);
    if (IsWindow(m_extensionListControl.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_extensionListControl.MoveWindow(rc);
    }
}

void CExtensionView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_extensionListControl.SetFocus();
}

BOOL CExtensionView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}
