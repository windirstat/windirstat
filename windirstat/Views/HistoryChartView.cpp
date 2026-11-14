// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "HistoryChartView.h"

namespace
{
    enum : UINT
    {
        CMD_REMOVE_POINT = 1, CMD_PURGE, CMD_STORAGE, CMD_COPY_VALUES, CMD_COPY_CHART, CMD_EXPORT, CMD_PRINT
    };
    const CSize OUTPUT_SIZE{ 1200, 700 };
}

void CHistoryChartView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rect)
{
    const int margin = ScaleForDpi(8);
    const int width = rect.Width() - 2 * margin;
    const int height = rect.Height() - 2 * margin;
    if (width < ScaleForDpi(40) || height < ScaleForDpi(40)) return;

    // Gray placeholder line with markers.
    constexpr std::array heights{ 85, 50, 55, 70, 60, 40, 30 };
    CPen pen(PS_SOLID, ScaleForDpi(3), RGB(70, 73, 80));
    const int slot = width / static_cast<int>(heights.size());
    {
        const GdiObjectSelection selectPen(pDC, &pen);
        for (const auto [index, percent] : std::views::enumerate(heights))
        {
            const CPoint pt(rect.left + margin + static_cast<int>(index) * slot + slot / 2,
                rect.bottom - margin - height * percent / 100);
            if (index == 0) pDC->MoveTo(pt); else pDC->LineTo(pt);
        }
    }
    for (const auto [index, percent] : std::views::enumerate(heights))
    {
        const int r = ScaleForDpi(4);
        const CPoint pt(rect.left + margin + static_cast<int>(index) * slot + slot / 2,
            rect.bottom - margin - height * percent / 100);
        pDC->FillSolidRect(CRect(pt.x - r, pt.y - r, pt.x + r, pt.y + r), RGB(95, 98, 106));
    }
}

// The folder whose files are analyzed: the selected folder (the parent for a
// selected file) or the zoom item when nothing is selected.
CItem* CHistoryChartView::GetTargetItem() const
{
    CWinDirStatModel* model = CWinDirStatModel::Get();
    if (const auto selected = model->GetSelectedItemsView(); !selected.empty())
    {
        CItem* item = selected.front();
        if (item->IsLeaf() && item->GetParent() != nullptr) item = item->GetParent();
        return item;
    }
    return model->GetZoomItem();
}

void CHistoryChartView::RenderVisualization(CDC* pDC, const CRect rect)
{
    m_shownItem = GetTargetItem();
    m_chart.DrawChart(pDC, rect, m_shownItem, CWinDirStatModel::Get()->GetScanPathSpec());
}

void CHistoryChartView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    CGraphView::OnUpdate(sender, change, item);

    // The base class only refreshes overlays on selection changes; this chart
    // has to be rebuilt as soon as the selection moves to another folder.
    switch (change)
    {
    case MODEL_CHANGE_SELECTION_ACTION:
    case MODEL_CHANGE_SELECTION_REFRESH:
    case MODEL_CHANGE_SELECTION_STYLE:
        if (IsWindowVisible() && CWinDirStatModel::Get()->IsRootDone()
            && m_shownItem != nullptr && GetTargetItem() != m_shownItem)
        {
            Inactivate();
            Invalidate(false);
        }
        break;
    default:
        break;
    }
}

void CHistoryChartView::OnContextMenu(WindowRef /*pWnd*/, CPoint point)
{
    const CItem* item = GetTargetItem();
    if (item == nullptr || !CWinDirStatModel::Get()->IsRootDone()) return;
    const std::wstring origin = CWinDirStatModel::Get()->GetScanPathSpec();

    // Keyboard invocation passes (-1, -1): open the menu in the middle of the view.
    if (point.x == -1 && point.y == -1) point = ToScreen(GetClientRect().Center());
    // The point under the cursor wins; otherwise the point selected with the left button.
    std::int64_t pointTime = m_chart.FindPointTime(ToClient(point));
    if (pointTime >= 0 && m_chart.SelectPointAt(ToClient(point)))
    {
        // Show the values of the plotted series before the (modal) menu opens.
        Inactivate();
        Invalidate(false);
        UpdateWindow();
    }
    if (pointTime < 0) pointTime = m_chart.GetSelectedTime();
    const auto* history = CScanHistory::Find(origin, item->GetPath());
    const UINT hasData = history != nullptr && !history->empty() ? MF_ENABLED : MF_GRAYED;

    CMenu menu = CMenu::CreatePopup();
    if (!menu) return;
    menu.Append(MF_STRING | (pointTime >= 0 ? MF_ENABLED : MF_GRAYED), CMD_REMOVE_POINT,
        Localization::Lookup(IDS_HISTORYCHART_REMOVEPOINT));
    menu.Append(MF_STRING, CMD_PURGE, Localization::Lookup(IDS_HISTORYCHART_PURGE));
    menu.Append(MF_STRING, CMD_STORAGE, Localization::Lookup(IDS_HISTORYCHART_STORAGE));
    menu.Append(MF_SEPARATOR);
    menu.Append(MF_STRING | hasData, CMD_COPY_VALUES, Localization::Lookup(IDS_HISTORYCHART_COPYVALUES));
    menu.Append(MF_STRING, CMD_COPY_CHART, Localization::Lookup(IDS_HISTORYCHART_COPYCHART));
    menu.Append(MF_STRING, CMD_EXPORT, Localization::Lookup(IDS_HISTORYCHART_EXPORT));
    menu.Append(MF_STRING, CMD_PRINT, Localization::Lookup(IDS_HISTORYCHART_PRINT));

    const UINT command = menu.ShowPopup(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point, GetMainWindow());
    switch (command)
    {
    case CMD_REMOVE_POINT:
        if (CScanHistory::RemovePoint(origin, item->GetPath(), pointTime))
        {
            m_chart.ClearSelection();
            Inactivate(), Invalidate(false);
        }
        break;
    case CMD_PURGE:
        if (CMessageBoxDlg::Show(Localization::Lookup(IDS_HISTORYCHART_PURGE_CONFIRM) + L"\n\n" + origin,
            MB_YESNO | MB_ICONWARNING, GetMainWindow()) == IDYES)
        {
            CScanHistory::Purge(origin);
            Inactivate(), Invalidate(false);
        }
        break;
    case CMD_STORAGE:
        if (const auto folder = CDialog::PickFolder(GetMainWindow()))
        {
            CScanHistory::SetStorageFolder(*folder);
            Inactivate(), Invalidate(false);
        }
        break;
    case CMD_COPY_VALUES:
        CopyTextToClipboard(CHistoryChart::GetValuesText(item, origin));
        break;
    case CMD_COPY_CHART: CopyChart(item, origin); break;
    case CMD_EXPORT: ExportChart(item, origin); break;
    case CMD_PRINT: PrintChart(item, origin); break;
    default: break;
    }
}

void CHistoryChartView::CopyChart(const CItem* item, const std::wstring& origin)
{
    CHistoryChart chart = m_chart;
    CBitmap bitmap;
    if (!chart.RenderToBitmap(item, origin, OUTPUT_SIZE, bitmap) || !OpenClipboard(m_hWnd)) return;
    if (EmptyClipboard() && SetClipboardData(CF_BITMAP, bitmap.Handle()) != nullptr) bitmap.Detach();  // clipboard owns it now
    CloseClipboard();
}

void CHistoryChartView::ExportChart(const CItem* item, const std::wstring& origin)
{
    const auto file = CDialog::PickFile(CDialog::FilePickerMode::Save,
        std::format(L"{} (*.png)|*.png||", Localization::Lookup(IDS_HISTORYCHART_PNG_FILTER)),
        GetMainWindow(), L"png");
    if (!file) return;

    CHistoryChart chart = m_chart;
    CBitmap bitmap;
    if (!chart.RenderToBitmap(item, origin, OUTPUT_SIZE, bitmap)) return;
    constexpr CLSID pngEncoder{ 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    Gdiplus::Bitmap image(static_cast<HBITMAP>(bitmap.Handle()), nullptr);
    image.Save(file->c_str(), &pngEncoder, nullptr);
}

void CHistoryChartView::PrintChart(const CItem* item, const std::wstring& origin)
{
    PRINTDLGW dialog{ .lStructSize = sizeof(dialog) };
    dialog.hwndOwner = GetMainWindow()->Handle();
    dialog.Flags = PD_RETURNDC | PD_NOSELECTION | PD_NOPAGENUMS | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!PrintDlgW(&dialog)) return;

    if (dialog.hDC != nullptr)
    {
        // Half an inch of margin; the chart keeps a landscape aspect ratio on tall pages.
        const int marginX = GetDeviceCaps(dialog.hDC, LOGPIXELSX) / 2, marginY = GetDeviceCaps(dialog.hDC, LOGPIXELSY) / 2;
        const int width = GetDeviceCaps(dialog.hDC, HORZRES) - 2 * marginX;
        const int height = std::min(GetDeviceCaps(dialog.hDC, VERTRES) - 2 * marginY, width * 3 / 5);
        const std::wstring name = Localization::LookupNeutral(IDS_APP_TITLE);
        const DOCINFOW doc{ .cbSize = sizeof(doc), .lpszDocName = name.c_str() };
        if (width > 0 && height > 0 && StartDocW(dialog.hDC, &doc) > 0)
        {
            if (StartPage(dialog.hDC) > 0)
            {
                CDC printer = CDC::Borrow(dialog.hDC);
                CHistoryChart chart = m_chart;
                chart.DrawChart(&printer, CRect(marginX, marginY, marginX + width, marginY + height), item, origin, true);
                EndPage(dialog.hDC);
            }
            EndDoc(dialog.hDC);
        }
        DeleteDC(dialog.hDC);
    }
    if (dialog.hDevMode != nullptr) GlobalFree(dialog.hDevMode);
    if (dialog.hDevNames != nullptr) GlobalFree(dialog.hDevNames);
}

void CHistoryChartView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    if (m_chart.ToggleLegendAt(point))
    {
        Inactivate();
        Invalidate(false);
        return;
    }
    using enum CHistoryChart::BarPart;
    switch (m_chart.HitBar(point))
    {
    case Left: if (m_chart.ScrollBy(-0.25)) Redraw(); return;
    case Right: if (m_chart.ScrollBy(0.25)) Redraw(); return;
    case TrackBefore: if (m_chart.ScrollBy(-1.0)) Redraw(); return;
    case TrackAfter: if (m_chart.ScrollBy(1.0)) Redraw(); return;
    case All: if (m_chart.ResetView()) Redraw(); return;
    case Thumb:
        m_panning = true;
        m_dragThumb = true;
        m_panX = point.x;
        ::SetCapture(m_hWnd);
        return;
    default: break;
    }

    if (m_chart.SelectPointAt(point))
    {
        Inactivate();
        Invalidate(false);
    }

    // Dragging inside a zoomed plot moves the visible range.
    if (m_chart.IsZoomed() && m_chart.IsInPlot(point))
    {
        m_panning = true;
        m_dragThumb = false;
        m_panX = point.x;
        ::SetCapture(m_hWnd);
    }
    CGraphView::OnLButtonDown(nFlags, point);
}

void CHistoryChartView::OnLButtonDblClk(const UINT /*nFlags*/, const CPoint /*point*/)
{
    if (m_chart.ResetView()) Redraw();
}

void CHistoryChartView::OnMouseMove(const UINT nFlags, const CPoint point)
{
    if (m_panning)
    {
        if ((nFlags & MK_LBUTTON) == 0)
        {
            m_panning = false;
            ::ReleaseCapture();
        }
        else if (m_dragThumb ? m_chart.ScrollThumb(point.x - m_panX) : m_chart.PanBy(point.x - m_panX))
        {
            m_panX = point.x;
            Redraw();
        }
        return;
    }
    CGraphView::OnMouseMove(nFlags, point);
}

bool CHistoryChartView::OnMouseWheel(const UINT nFlags, const short zDelta, const CPoint pt)
{
    if (m_chart.ZoomAt(ToClient(pt), zDelta > 0 ? 1.25 : 0.8))
    {
        Redraw();
        return true;
    }
    return CGraphView::OnMouseWheel(nFlags, zDelta, pt);
}
