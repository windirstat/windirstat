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
#include "TreeMapView.h"

IMPLEMENT_DYNCREATE(CTreeMapView, CGraphView)

BEGIN_MESSAGE_MAP(CTreeMapView, CGraphView)
END_MESSAGE_MAP()

void CTreeMapView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rc)
{
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

void CTreeMapView::RenderVisualization(CDC* pDC, CRect rect)
{
    if (CWinDirStatModel::Get()->IsZoomed()) DrawZoomFrame(pDC, rect);
    m_treeMap.DrawTreeMap(pDC, rect,
        CWinDirStatModel::Get()->GetZoomItem(), &COptions::TreeMapOptions);
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

void CTreeMapView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    const CWinDirStatModel* model = CWinDirStatModel::Get();
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
            if (IsExtensionHighlighted(item)) RenderHighlightRectangle(pdc, rc);
        }
        else for (const auto& child : item->GetChildren())
        {
            if (child->TmiGetSize() == 0) break;
            if (child->TmiGetRectangle().left == -1) break;
            stack.push_back(child);
        }
    }
}

void CTreeMapView::DrawSelection(CDC* pdc)
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& selectedItems = CWinDirStatModel::Get()->GetAllSelected();
    for (const CItem* item : selectedItems)
    {
        // Ignore if not a child of the current zoomed item
        if (!CWinDirStatModel::Get()->GetZoomItem()->IsAncestorOf(item)) continue;

        HighlightSelectedItem(pdc, GetDisplayItem(item), selectedItems.size() == 1);
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

CItem* CTreeMapView::FindItemAtPoint(CPoint point)
{
    // Offset the click point if zoomed
    CPoint pointClicked = point;

    if (CWinDirStatModel::Get()->IsZoomed())
    {
        pointClicked.Offset(-ZoomFrameWidth, -ZoomFrameWidth);
    }

    return static_cast<CItem*>(m_treeMap.FindItemByPoint(
        CWinDirStatModel::Get()->GetZoomItem(), pointClicked));
}

void CTreeMapView::DrillDown(CItem* item)
{
    if (item != CWinDirStatModel::Get()->GetZoomItem())
    {
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);
        AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_TREEMAP_ZOOMIN);
    }
}

std::span<const UINT> CTreeMapView::GetPersistentContextCommands() const
{
    static constexpr std::array<UINT, 9> persistentCommands{
        ID_TREEMAP_ZOOMIN,
        ID_TREEMAP_ZOOMOUT,
        ID_TREEMAP_SELECT_PARENT,
        ID_TREEMAP_RESELECT_CHILD,
        ID_VIEW_GROUP_TYPES,
        ID_TREEMAP_SHOW_FOLDER_FRAMES,
        ID_TREEMAP_SHOW_EXTENSIONS,
        ID_TREEMAP_LOGICAL_SIZE,
        ID_TREEMAP_PHYSICAL_SIZE,
    };
    return persistentCommands;
}
