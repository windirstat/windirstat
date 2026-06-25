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
#include "FileTreeView.h"
#include "TreeMapView.h"

IMPLEMENT_DYNCREATE(CTreeMapView, CWinDirStatPane)

BEGIN_MESSAGE_MAP(CTreeMapView, CWinDirStatPane)
    ON_WM_SIZE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

void CTreeMapView::SuspendRecalculationDrawing(const bool suspend)
{
    m_drawingSuspended = suspend;
    if (!suspend)
    {
        Invalidate();
    }
}

bool CTreeMapView::IsShowTreeMap() const
{
    return m_showTreeMap;
}

void CTreeMapView::ShowTreeMap(const bool show)
{
    m_showTreeMap = show;
}

BOOL CTreeMapView::PreCreateWindow(CREATESTRUCT& cs)
{
    CWinDirStatPane::PreCreateWindow(cs);

    WNDCLASS wc;
    if (!::GetClassInfo(AfxGetInstanceHandle(), L"WinDirStatTreeMapClass", &wc))
    {
        ::GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"WinDirStatTreeMapClass";
        ::RegisterClass(&wc);
    }

    cs.lpszClass = wc.lpszClassName;
    return TRUE;
}

void CTreeMapView::DrawEmptyView()
{
    CClientDC dc(this);
    DrawEmptyView(&dc);
}

void CTreeMapView::DrawEmptyView(CDC* pDC)
{
    constexpr COLORREF emptyBg = RGB(15, 15, 15);

    Inactivate();

    const CRect rc = ClientRectOf(this);
    if (m_dimmed.m_hObject == nullptr)
    {
        pDC->FillSolidRect(rc, emptyBg);

        CTreeMap::Options options = COptions::TreeMapOptions;
        options.showExtensions = false;
        options.showFolderFrames = false;

        struct Tile { int x, y, w, h, shade; };
        static constexpr Tile tiles[] = {
            {  0,  0, 25, 58, 58 }, { 25,  0, 13, 34, 72 }, { 25, 34, 13, 24, 48 },
            {  0, 58, 18, 42, 76 }, { 18, 58, 20, 25, 54 }, { 18, 83, 20, 17, 88 },
            { 38,  0, 28, 44, 66 }, { 38, 44, 14, 31, 82 }, { 52, 44, 14, 31, 52 },
            { 38, 75, 28, 25, 92 }, { 66,  0, 19, 62, 60 }, { 85,  0, 15, 38, 78 },
            { 66, 62, 16, 38, 50 }, { 82, 38, 18, 36, 86 }, { 82, 74, 18, 26, 68 },
        };

        for (const Tile& t : tiles)
        {
            CRect tile(
                rc.left + rc.Width()  * t.x / 100,
                rc.top  + rc.Height() * t.y / 100,
                rc.left + rc.Width()  * (t.x + t.w) / 100,
                rc.top  + rc.Height() * (t.y + t.h) / 100);

            if (tile.Width() > 0 && tile.Height() > 0)
                m_treeMap.DrawColorPreview(pDC, tile, RGB(t.shade, t.shade, t.shade), &options);
        }
    }
    else
    {
        CDC dcmem;
        dcmem.CreateCompatibleDC(pDC);
        CSelectObject sobmp(&dcmem, &m_dimmed);
        pDC->BitBlt(rc.left, rc.top, m_dimmedSize.cx, m_dimmedSize.cy, &dcmem, 0, 0, SRCCOPY);

        if (rc.Width() > m_dimmedSize.cx)
        {
            CRect r = rc;
            r.left  = r.left + m_dimmedSize.cx;
            pDC->FillSolidRect(r, emptyBg);
        }

        if (rc.Height() > m_dimmedSize.cy)
        {
            CRect r = rc;
            r.top   = r.top + m_dimmedSize.cy;
            pDC->FillSolidRect(r, emptyBg);
        }
    }
}

void CTreeMapView::OnDraw(CDC * pDC)
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone() || m_drawingSuspended || !m_showTreeMap)
    {
        DrawEmptyView(pDC);
        return;
    }

    CRect rc = ClientRectOf(this);
    ASSERT(m_size == rc.Size());
    ASSERT(rc.TopLeft() == CPoint(0, 0));

    CDC dcmem;
    dcmem.CreateCompatibleDC(pDC);

    if (!IsDrawn())
    {
        CWaitCursor wc;

        m_bitmap.CreateCompatibleBitmap(pDC, m_size.cx, m_size.cy);

        CSelectObject sobmp(&dcmem, &m_bitmap);

        if (CWinDirStatModel::Get()->IsZoomed())
        {
            DrawZoomFrame(&dcmem, rc);
        }

        m_treeMap.DrawTreeMap(&dcmem, rc, CWinDirStatModel::Get()->GetZoomItem(), &COptions::TreeMapOptions);
    }

    CSelectObject sobmp2(&dcmem, &m_bitmap);

    pDC->BitBlt(0, 0, m_size.cx, m_size.cy, &dcmem, 0, 0, SRCCOPY);

    DrawHighlights(pDC);
}

void CTreeMapView::DrawZoomFrame(CDC* pdc, CRect& rc) const
{
    CRect r  = rc;
    r.bottom = r.top + ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.top = r.bottom - ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.right = r.left + ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.left = r.right - ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    rc.DeflateRect(ZoomFrameWidth, ZoomFrameWidth);
}

void CTreeMapView::DrawHighlights(CDC* pdc)
{
    switch (CMainFrame::Get()->GetLogicalFocus())
    {
    case LF_DUPELIST:
    case LF_TOPLIST:
    case LF_FILETREE:
    case LF_SEARCHLIST:
        DrawSelection(pdc);
        break;
    case LF_EXTLIST:
        DrawHighlightExtension(pdc);
        break;
    case LF_NONE:
        break;
    }
}

// True if the leaf is a file whose extension is in the highlighted (unregistered) set
static bool IsUnregisteredLeaf(const CItem* item, const std::unordered_set<std::wstring>& set)
{
    return item->IsTypeOrFlag(IT_FILE) && set.contains(item->GetExtension());
}

void CTreeMapView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    const CWinDirStatModel* model = CWinDirStatModel::Get();
    const std::wstring& highlightExt = model->GetHighlightExtension();
    const bool unregistered = model->IsHighlightUnregistered();
    const auto& highlightExtensions = model->GetHighlightExtensions();
    const bool isZoomed = model->IsZoomed();

    std::vector<const CItem*> stack;
    stack.reserve(128);
    stack.push_back(model->GetZoomItem());

    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();

        CRect rc(item->TmiGetRectangle());
        if (isZoomed)
        {
            rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
        }

        if (rc.Width() <= 0 || rc.Height() <= 0)
        {
            continue;
        }

        if (item->TmiIsLeaf())
        {
            if (unregistered ? IsUnregisteredLeaf(item, highlightExtensions) : item->HasExtension(highlightExt))
            {
                RenderHighlightRectangle(pdc, rc);
            }
        }
        else for (const auto& child : item->GetChildren())
        {
            if (child->TmiGetSize() == 0) break;
            if (child->TmiGetRectangle().left == -1) break;
            stack.push_back(child);
        }
    }
}

void CTreeMapView::DrawSelection(CDC* pdc) const
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    for (const auto& item : items)
    {
        // Ignore if not a child of the current zoomed item
        if (!CWinDirStatModel::Get()->GetZoomItem()->IsAncestorOf(item)) continue;

        const auto itemToSelect = item->IsTypeOrFlag(ITF_HARDLINK) ? item->FindHardlinksIndexItem() : item;
        HighlightSelectedItem(pdc, itemToSelect, items.size() == 1);
    }
}

// A pen and the null brush must be selected.
// Draws the highlight rectangle of item. If single, the rectangle is slightly
// bigger than the item rect, else it fits inside.
//
void CTreeMapView::HighlightSelectedItem(CDC* pdc, const CItem* item, const bool single) const
{
    CRect rc(item->TmiGetRectangle());

    // Offset the display rectangle if zoomed
    if (CWinDirStatModel::Get()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (single)
    {
        CRect rcClient = ClientRectOf(this);
        if (m_treeMap.GetOptions().grid)
        {
            rc.right++;
            rc.bottom++;
        }

        if (rcClient.left < rc.left) rc.left--;
        if (rcClient.top < rc.top) rc.top--;
        if (rc.right < rcClient.right) rc.right++;
        if (rc.bottom < rcClient.bottom) rc.bottom++;
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    RenderHighlightRectangle(pdc, rc);
}

// A pen and the null brush must be selected.
//
void CTreeMapView::RenderHighlightRectangle(CDC* pdc, CRect& rc) const
{
    ASSERT(rc.Width() >= 0);
    ASSERT(rc.Height() >= 0);

    // The documentation of CDC::Rectangle() says that the width
    // and height must be greater than 2. Experiment says that
    // it must be greater than 1. We follow the documentation.

    if (rc.Width() >= 7 && rc.Height() >= 7)
    {
        pdc->Rectangle(rc); // w = 7
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc); // w = 5
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc); // w = 3
    }
    else
    {
        pdc->FillSolidRect(rc, COptions::TreeMapHighlightColor);
    }
}

CItem* CTreeMapView::ResolveItemAtPoint(CPoint point, bool isScreenCoords)
{
    // Validate that the document and root are in a ready state
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone() || !IsDrawn())
    {
        return nullptr;
    }

    // Offset the click point if zoomed
    CPoint pointClicked = point;
    if (isScreenCoords) ScreenToClient(&pointClicked);

    if (CWinDirStatModel::Get()->IsZoomed())
    {
        pointClicked.Offset(-ZoomFrameWidth, -ZoomFrameWidth);
    }

    // Perform the hit-test against the current tree map projection
    return static_cast<CItem*>(m_treeMap.FindItemByPoint(
        CWinDirStatModel::Get()->GetZoomItem(), pointClicked));
}

void CTreeMapView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);
    const CSize sz(cx, cy);
    if (sz != m_size)
    {
        Inactivate();
        m_size = sz;
    }
}

void CTreeMapView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (CItem* item = ResolveItemAtPoint(point); item && item != CWinDirStatModel::Get()->GetZoomItem())
    {
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);
        AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_TREEMAP_ZOOMIN);
    }
    CWinDirStatPane::OnLButtonDblClk(nFlags, point);
}

void CTreeMapView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    if (auto* item = ResolveItemAtPoint(point))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);
    }
    CWinDirStatPane::OnLButtonDown(nFlags, point);
}

void CTreeMapView::OnMButtonDown(UINT nFlags, CPoint point)
{
    if (ResolveItemAtPoint(point))
    {
        AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_TREEMAP_ZOOMRESET);
    }

    CWinDirStatPane::OnMButtonDown(nFlags, point);
}

bool CTreeMapView::IsDrawn() const
{
    return m_bitmap.m_hObject != nullptr;
}

void CTreeMapView::Inactivate()
{
    if (m_bitmap.m_hObject == nullptr) return;

    // Move the old bitmap to m_dimmed for later dimmed display
    m_dimmed.DeleteObject();
    m_dimmed.Attach(m_bitmap.Detach());
    m_dimmedSize = m_size;

    // Dim m_dimmed contents to indicate inactive/refresh state
    CClientDC dc(this);
    CDC dcmem;
    dcmem.CreateCompatibleDC(&dc);
    CSelectObject sobmp(&dcmem, &m_dimmed);

    // Apply the dimming overlay
    constexpr BLENDFUNCTION blendFunc{
        .BlendOp = AC_SRC_OVER, .BlendFlags = 0,
        .SourceConstantAlpha = 175, .AlphaFormat = 0 };
    dcmem.FillSolidRect(CRect(0, 0, m_dimmedSize.cx, m_dimmedSize.cy), RGB(0, 0, 0));
    dcmem.AlphaBlend(0, 0, m_dimmedSize.cx, m_dimmedSize.cy, &dc,
        0, 0, m_dimmedSize.cx, m_dimmedSize.cy, blendFunc);
}

void CTreeMapView::EmptyView()
{
    if (m_bitmap.m_hObject != nullptr)
    {
        m_bitmap.DeleteObject();
    }

    if (m_dimmed.m_hObject != nullptr)
    {
        m_dimmed.DeleteObject();
    }
}

void CTreeMapView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    CMainFrame::Get()->GetFileTreeView()->SetFocus();
}

void CTreeMapView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (!CWinDirStatModel::Get()->IsRootDone())
    {
        Inactivate();
    }

    switch (change)
    {
    case MODEL_CHANGE_NEW_ROOT:
        {
            EmptyView();
            CWinDirStatPane::OnUpdate(sender, change, item);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case MODEL_CHANGE_SELECTION_ACTION:
    case MODEL_CHANGE_SELECTION_REFRESH:
    case MODEL_CHANGE_SELECTION_STYLE:
    case MODEL_CHANGE_EXTENSION_SELECTION:
        {
            CWinDirStatPane::OnUpdate(sender, change, item);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case MODEL_CHANGE_TREEMAP_STYLE:
    case MODEL_CHANGE_ZOOM:
        {
            Inactivate();
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    case MODEL_CHANGE_NONE:
        {
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    default:
        break;
    }
}

std::tuple<std::wstring, ULONGLONG>  CTreeMapView::GetTreeMapHoverInfo()
{
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    if (const CRect rc = ClientRectOf(this); !rc.PtInRect(point))
    {
        m_paneTextOverride = {};
        m_paneSizeOverride = 0;
    }

    return { m_paneTextOverride, m_paneSizeOverride };
}

void CTreeMapView::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    // List of context menu command IDs and whether the menu
    // should remain open after executing the command
    static constexpr struct {
        UINT id;
        bool isPersistent;
    } contextMenuPersistent[] = {
        { ID_TREEMAP_ZOOMIN,            true  },
        { ID_TREEMAP_ZOOMOUT,           true  },
        { ID_TREEMAP_SELECT_PARENT,     true  },
        { ID_TREEMAP_RESELECT_CHILD,    true  },
        { ID_EDIT_COPY_CLIPBOARD,       false },
        { ID_CLEANUP_EXPLORER_SELECT,   false },
        { ID_CLEANUP_OPEN_IN_CONSOLE,   false },
        { ID_CLEANUP_OPEN_IN_PWSH,      false },
        { ID_POPUP_CANCEL,              false }
    };

    // Helper lambda to check if a command ID is in the list of persistent context menu commands
    [[msvc::flatten]] static constexpr auto IsContextMenuPersistent = [](UINT id) -> bool {
        return std::ranges::any_of(contextMenuPersistent, [id](const auto& cmd) {
            return cmd.id == id && cmd.isPersistent;
        });
    };

    auto* clickedItem = ResolveItemAtPoint(point, true);
    if (clickedItem == nullptr) return;

    if (!std::ranges::any_of(CWinDirStatModel::Get()->GetAllSelected(), [&](auto* s)
        { return s == clickedItem || s->IsAncestorOf(clickedItem); }))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, clickedItem);
    }

    if (CMenu menu; menu.LoadMenu(IDR_POPUP_MAP))
    {
        Localization::UpdateMenu(menu);
        if (CMenu* sub = menu.GetSubMenu(0))
        {
            UINT cmdId = 0;
            do {
                cmdId = sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                    point.x, point.y, AfxGetMainWnd());
                if (cmdId > 0) AfxGetMainWnd()->SendMessage(WM_COMMAND, cmdId);
            } while (cmdId > 0 && IsContextMenuPersistent(cmdId));
        }
    }
}

void CTreeMapView::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    if (auto* item = ResolveItemAtPoint(point))
    {
        m_paneTextOverride = item->GetPath();
        m_paneSizeOverride = item->GetSizeLogical();
        CMainFrame::Get()->UpdatePaneText();
    }
}

BOOL CTreeMapView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (!CMainFrame::Get())
        return CWinDirStatPane::OnMouseWheel(nFlags, zDelta, pt);

    const int clicks = zDelta / WHEEL_DELTA;
    if (clicks == 0) return TRUE;

    // Determine the Command IDs based on the Modifier State
    // Use the Ctrl key to toggle between Navigation and Zooming
    const UINT cmdUp = (nFlags & MK_CONTROL) ? ID_TREEMAP_ZOOMIN : ID_TREEMAP_SELECT_PARENT;
    const UINT cmdDown = (nFlags & MK_CONTROL) ? ID_TREEMAP_ZOOMOUT : ID_TREEMAP_RESELECT_CHILD;

    // Resolve the active command based on scroll direction
    const UINT targetCmd = (clicks > 0) ? cmdUp : cmdDown;
    const int absoluteClicks = std::abs(clicks);

    // Dispatch the commands through the central routing engine
    for (int i = 0; i < absoluteClicks; ++i)
    {
        CMainFrame::Get()->SendMessage(WM_COMMAND, targetCmd);
    }

    return TRUE;
}
