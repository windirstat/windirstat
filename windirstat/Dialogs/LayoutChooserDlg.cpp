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
#include "LayoutChooserDlg.h"
#include "MainFrame.h"

IMPLEMENT_DYNAMIC(CLayoutChooserDlg, CDialog)

CLayoutChooserDlg::CLayoutChooserDlg(CWnd* pParent)
    : CDialog(IDD, pParent) {}

void CLayoutChooserDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Radio(pDX, IDC_LAYOUT_TREEMAP_RIGHT, m_treemapSide);
    DDX_Control(pDX, IDC_LAYOUT_TREEMAP_RIGHT, m_radioRight);
    DDX_Control(pDX, IDC_LAYOUT_TREEMAP_LEFT,  m_radioLeft);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL0,    m_comboCol[0]);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL1,    m_comboCol[1]);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL2,    m_comboCol[2]);
}

BEGIN_MESSAGE_MAP(CLayoutChooserDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_BN_CLICKED(IDC_LAYOUT_TREEMAP_RIGHT, OnBnClickedRadioRight)
    ON_BN_CLICKED(IDC_LAYOUT_TREEMAP_LEFT,  OnBnClickedRadioLeft)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_LAYOUT_WIDE_COL0, IDC_LAYOUT_WIDE_COL2, OnCbnSelchangeCombo)
    ON_BN_CLICKED(IDC_LAYOUT_RESET, OnBnClickedReset)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CLayoutChooserDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    m_layoutMode  = COptions::LayoutMode;
    m_treemapSide = COptions::LayoutSideTreeMapRight ? 0 : 1;

    UpdateData(FALSE); // binds DDX_Control variables and sets radio buttons
    FillCombos();      // now m_comboCol[] have valid HWNDs
    m_comboCol[0].SetCurSel(COptions::LayoutWideCol0);
    m_comboCol[1].SetCurSel(COptions::LayoutWideCol1);
    m_comboCol[2].SetCurSel(COptions::LayoutWideCol2);
    UpdateSubControls();
    return TRUE;
}

void CLayoutChooserDlg::FillCombos()
{
    static constexpr std::wstring_view panelIds[] = {
        IDS_PAGE_LAYOUT_PANEL_FILELIST,
        IDS_PAGE_LAYOUT_PANEL_TREEMAP,
        IDS_PAGE_LAYOUT_PANEL_EXTENSIONS,
    };
    for (auto& combo : m_comboCol)
    {
        combo.ResetContent();
        for (const auto id : panelIds)
        {
            combo.AddString(Localization::Lookup(id).data());
        }
    }
}

void CLayoutChooserDlg::UpdateSubControls()
{
    const bool isSide = (m_layoutMode == 1);
    const bool isWide = (m_layoutMode == 2);

    const int sideVis = isSide ? SW_SHOW : SW_HIDE;
    const int wideVis = isWide ? SW_SHOW : SW_HIDE;

    m_radioRight.ShowWindow(sideVis);
    m_radioLeft.ShowWindow(sideVis);

    m_comboCol[0].ShowWindow(wideVis);
    m_comboCol[1].ShowWindow(wideVis);
    m_comboCol[2].ShowWindow(wideVis);
    GetDlgItem(IDC_LAYOUT_LABEL_COL0)->ShowWindow(wideVis);
    GetDlgItem(IDC_LAYOUT_LABEL_COL1)->ShowWindow(wideVis);
    GetDlgItem(IDC_LAYOUT_LABEL_COL2)->ShowWindow(wideVis);
}

CRect CLayoutChooserDlg::GetCardArea() const
{
    CRect ctrlRect;
    GetDlgItem(IDC_LAYOUT_TREEMAP_RIGHT)->GetWindowRect(&ctrlRect);
    ScreenToClient(&ctrlRect);

    CRect client;
    GetClientRect(&client);
    return { 0, 0, client.right, ctrlRect.top - 4 };
}

CRect CLayoutChooserDlg::GetCardRect(int idx) const
{
    const CRect area = GetCardArea();
    const int   pad  = 8;
    const int   cw   = (area.Width() - pad * 4) / 3;
    const int   x    = pad + idx * (cw + pad);
    return { x, pad, x + cw, area.bottom - pad };
}

int CLayoutChooserDlg::HitCard(CPoint pt) const
{
    for (int i = 0; i < 3; i++)
    {
        if (GetCardRect(i).PtInRect(pt))
        {
            return i;
        }
    }
    return -1;
}

void CLayoutChooserDlg::DrawCard(CDC& dc, int idx, const CRect& r) const
{
    const bool sel  = (idx == m_layoutMode);
    const bool hov  = (idx == m_hovered);
    const bool dark = DarkMode::IsDarkModeActive();

    const COLORREF cardBg = hov ? (dark ? RGB(60, 65, 75) : RGB(228, 234, 244))
                                : (dark ? DarkMode::WdsSysColor(COLOR_WINDOW) : GetSysColor(COLOR_BTNFACE));
    dc.FillSolidRect(r, cardBg);

    CRect inner(r);
    inner.DeflateRect(5, 5);
    inner.bottom -= 20; // label area below

    const COLORREF fileC = dark ? RGB(50, 60, 80)  : RGB(195, 210, 235);
    const COLORREF mapC  = dark ? RGB(80, 55, 55)  : RGB(235, 205, 195);
    const COLORREF extC  = dark ? RGB(50, 75, 55)  : RGB(195, 235, 205);
    const COLORREF lineC = dark ? DarkMode::WdsSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_GRAYTEXT);

    CPen linePen(PS_SOLID, 1, lineC);
    CPen* oldPen = dc.SelectObject(&linePen);

    if (idx == 0)
    {
        const int splitY = inner.top + inner.Height() * 55 / 100;
        const int extX   = inner.left + inner.Width() * 75 / 100;
        dc.FillSolidRect(inner.left, inner.top, inner.Width(),    splitY - inner.top, fileC);
        dc.FillSolidRect(extX,       inner.top, inner.right - extX, splitY - inner.top, extC);
        dc.FillSolidRect(inner.left, splitY,    inner.Width(),    inner.bottom - splitY, mapC);
        dc.MoveTo(inner.left, splitY); dc.LineTo(inner.right, splitY);
        dc.MoveTo(extX, inner.top);    dc.LineTo(extX, splitY);
    }
    else if (idx == 1)
    {
        const int splitX     = inner.left + inner.Width() * 60 / 100;
        const int leftSplitY = inner.top  + inner.Height() * 65 / 100;
        dc.FillSolidRect(inner.left, inner.top,     splitX - inner.left, leftSplitY - inner.top, fileC);
        dc.FillSolidRect(inner.left, leftSplitY,    splitX - inner.left, inner.bottom - leftSplitY, extC);
        dc.FillSolidRect(splitX,     inner.top,     inner.right - splitX, inner.Height(), mapC);
        dc.MoveTo(splitX, inner.top);       dc.LineTo(splitX, inner.bottom);
        dc.MoveTo(inner.left, leftSplitY);  dc.LineTo(splitX, leftSplitY);
    }
    else
    {
        const int col0 = inner.left + inner.Width() / 3;
        const int col1 = inner.left + inner.Width() * 2 / 3;
        dc.FillSolidRect(inner.left, inner.top, col0 - inner.left,  inner.Height(), fileC);
        dc.FillSolidRect(col0,       inner.top, col1 - col0,        inner.Height(), mapC);
        dc.FillSolidRect(col1,       inner.top, inner.right - col1, inner.Height(), extC);
        dc.MoveTo(col0, inner.top); dc.LineTo(col0, inner.bottom);
        dc.MoveTo(col1, inner.top); dc.LineTo(col1, inner.bottom);
    }

    dc.SelectObject(oldPen);

    // Diagram frame
    CBrush frameBrush(lineC);
    dc.FrameRect(inner, &frameBrush);

    // Mode label
    static constexpr std::wstring_view labelIds[] = {
        IDS_PAGE_LAYOUT_MODE_DEFAULT,
        IDS_PAGE_LAYOUT_MODE_SIDE,
        IDS_PAGE_LAYOUT_MODE_WIDE,
    };
    std::wstring label = Localization::Lookup(labelIds[idx]);
    // Truncate at first ' (', ' -', or ' –' (en dash) to get a short name
    for (const auto& d : { std::wstring_view(L" ("), std::wstring_view(L" –"), std::wstring_view(L" -") })
    {
        const auto pos = label.find(d);
        if (pos != std::wstring::npos)
        {
            label.resize(pos);
            break;
        }
    }
    CRect labelRc(r.left, r.bottom - 22, r.right, r.bottom - 3);
    dc.SetBkMode(TRANSPARENT);
    const COLORREF textC = dark ? DarkMode::WdsSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_WINDOWTEXT);
    dc.SetTextColor(textC);
    dc.DrawText(label.c_str(), static_cast<int>(label.size()), &labelRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Card border (drawn on top of everything)
    const COLORREF borderC = sel ? GetSysColor(COLOR_HIGHLIGHT)
                           : hov ? GetSysColor(COLOR_HOTLIGHT)
                                 : (dark ? RGB(80, 80, 80) : GetSysColor(COLOR_WINDOWFRAME));
    const int bw = sel ? 3 : 1;
    CRect br(r);
    for (int i = 0; i < bw; i++)
    {
        CBrush bBrush(borderC);
        dc.FrameRect(br, &bBrush);
        br.DeflateRect(1, 1);
    }
}

void CLayoutChooserDlg::AutoSwapCombos(int changedIdx)
{
    const int sel0 = m_comboCol[0].GetCurSel();
    const int sel1 = m_comboCol[1].GetCurSel();
    const int sel2 = m_comboCol[2].GetCurSel();

    if (changedIdx == 0)
    {
        if (sel0 == sel1)      m_comboCol[1].SetCurSel(3 - sel0 - sel2);
        else if (sel0 == sel2) m_comboCol[2].SetCurSel(3 - sel0 - sel1);
    }
    else if (changedIdx == 1)
    {
        if (sel1 == sel0)      m_comboCol[0].SetCurSel(3 - sel1 - sel2);
        else if (sel1 == sel2) m_comboCol[2].SetCurSel(3 - sel0 - sel1);
    }
    else
    {
        if (sel2 == sel0)      m_comboCol[0].SetCurSel(3 - sel1 - sel2);
        else if (sel2 == sel1) m_comboCol[1].SetCurSel(3 - sel0 - sel2);
    }
}

BOOL CLayoutChooserDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect client;
    GetClientRect(&client);
    const COLORREF bg = DarkMode::IsDarkModeActive()
        ? DarkMode::WdsSysColor(CTLCOLOR_DLG)
        : GetSysColor(COLOR_BTNFACE);
    pDC->FillSolidRect(client, bg);
    return TRUE;
}

void CLayoutChooserDlg::OnPaint()
{
    CPaintDC dc(this);
    for (int i = 0; i < 3; i++)
    {
        DrawCard(dc, i, GetCardRect(i));
    }
}

void CLayoutChooserDlg::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitCard(point);
    if (hit >= 0 && hit != m_layoutMode)
    {
        m_layoutMode = hit;
        UpdateSubControls();
        InvalidateRect(GetCardArea());
    }
}

void CLayoutChooserDlg::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitCard(point);
    if (hit != m_hovered)
    {
        m_hovered = hit;
        InvalidateRect(GetCardArea());
    }
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, GetSafeHwnd(), HOVER_DEFAULT };
        TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }
}

void CLayoutChooserDlg::OnMouseLeave()
{
    m_hovered       = -1;
    m_trackingMouse = false;
    InvalidateRect(GetCardArea());
}

void CLayoutChooserDlg::OnBnClickedRadioRight()
{
    m_treemapSide = 0;
}

void CLayoutChooserDlg::OnBnClickedRadioLeft()
{
    m_treemapSide = 1;
}

void CLayoutChooserDlg::OnCbnSelchangeCombo(UINT nID)
{
    AutoSwapCombos(nID - IDC_LAYOUT_WIDE_COL0);
}

void CLayoutChooserDlg::OnBnClickedReset()
{
    CMainFrame::Get()->ResetDividers();
}

HBRUSH CLayoutChooserDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CLayoutChooserDlg::OnOK()
{
    UpdateData(TRUE);

    const int  newMode  = m_layoutMode;
    const bool newRight = (m_treemapSide == 0);
    const int  newCol0  = m_comboCol[0].GetCurSel();
    const int  newCol1  = m_comboCol[1].GetCurSel();
    const int  newCol2  = m_comboCol[2].GetCurSel();

    const bool changed =
        newMode  != static_cast<int>(COptions::LayoutMode)              ||
        newRight != static_cast<bool>(COptions::LayoutSideTreeMapRight) ||
        newCol0  != static_cast<int>(COptions::LayoutWideCol0)          ||
        newCol1  != static_cast<int>(COptions::LayoutWideCol1)          ||
        newCol2  != static_cast<int>(COptions::LayoutWideCol2);

    COptions::LayoutMode             = newMode;
    COptions::LayoutSideTreeMapRight = newRight;
    COptions::LayoutWideCol0         = newCol0;
    COptions::LayoutWideCol1         = newCol1;
    COptions::LayoutWideCol2         = newCol2;

    CDialog::OnOK();

    if (changed)
    {
        AfxMessageBox(Localization::Lookup(IDS_PAGE_LAYOUT_RESTART_NOTE).c_str(), MB_OK | MB_ICONINFORMATION);
    }
}
