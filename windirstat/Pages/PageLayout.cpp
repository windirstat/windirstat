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
#include "PageLayout.h"
#include "MainFrame.h"

IMPLEMENT_DYNAMIC(CPageLayout, CMFCPropertyPage)

CPageLayout::CPageLayout() : CMFCPropertyPage(IDD) {}

COptionsPropertySheet* CPageLayout::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void CPageLayout::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Radio(pDX, IDC_LAYOUT_MODE_DEFAULT, m_layoutMode);
    DDX_Radio(pDX, IDC_LAYOUT_TREEMAP_RIGHT, m_treemapSide);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL0, m_comboCol0);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL1, m_comboCol1);
    DDX_Control(pDX, IDC_LAYOUT_WIDE_COL2, m_comboCol2);
}

BEGIN_MESSAGE_MAP(CPageLayout, CMFCPropertyPage)
    ON_BN_CLICKED(IDC_LAYOUT_MODE_DEFAULT, OnModeChanged)
    ON_BN_CLICKED(IDC_LAYOUT_MODE_SIDE,    OnModeChanged)
    ON_BN_CLICKED(IDC_LAYOUT_MODE_WIDE,    OnModeChanged)
    ON_BN_CLICKED(IDC_LAYOUT_TREEMAP_RIGHT, OnModeChanged)
    ON_BN_CLICKED(IDC_LAYOUT_TREEMAP_LEFT,  OnModeChanged)
    ON_CBN_SELENDOK(IDC_LAYOUT_WIDE_COL0, OnColChanged)
    ON_CBN_SELENDOK(IDC_LAYOUT_WIDE_COL1, OnColChanged)
    ON_CBN_SELENDOK(IDC_LAYOUT_WIDE_COL2, OnColChanged)
    ON_BN_CLICKED(IDC_LAYOUT_RESET, OnBnClickedReset)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageLayout::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CPageLayout::FillPanelCombo(CComboBox& combo, const int selectedPanel)
{
    combo.ResetContent();
    // Items in order: FileList=0, Treemap=1, Extensions=2
    combo.AddString(Localization::Lookup(IDS_PAGE_LAYOUT_PANEL_FILELIST).data());
    combo.AddString(Localization::Lookup(IDS_PAGE_LAYOUT_PANEL_TREEMAP).data());
    combo.AddString(Localization::Lookup(IDS_PAGE_LAYOUT_PANEL_EXTENSIONS).data());
    combo.SetCurSel(selectedPanel);
}

BOOL CPageLayout::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    m_layoutMode  = COptions::LayoutMode;
    m_treemapSide = COptions::LayoutSideTreeMapRight ? 0 : 1;

    FillPanelCombo(m_comboCol0, COptions::LayoutWideCol0);
    FillPanelCombo(m_comboCol1, COptions::LayoutWideCol1);
    FillPanelCombo(m_comboCol2, COptions::LayoutWideCol2);

    UpdateData(FALSE);
    UpdateControlStates();
    return TRUE;
}

void CPageLayout::UpdateControlStates()
{
    UpdateData(TRUE);

    const bool isSide = (m_layoutMode == 1);
    const bool isWide = (m_layoutMode == 2);

    GetDlgItem(IDC_LAYOUT_TREEMAP_RIGHT)->EnableWindow(isSide);
    GetDlgItem(IDC_LAYOUT_TREEMAP_LEFT)->EnableWindow(isSide);

    GetDlgItem(IDC_LAYOUT_WIDE_COL0)->EnableWindow(isWide);
    GetDlgItem(IDC_LAYOUT_WIDE_COL1)->EnableWindow(isWide);
    GetDlgItem(IDC_LAYOUT_WIDE_COL2)->EnableWindow(isWide);
}

void CPageLayout::OnModeChanged()
{
    UpdateControlStates();
    GetSheet()->SetRestartRequired(true);
    SetModified();
}

void CPageLayout::OnColChanged()
{
    // Auto-swap duplicate selections: if the newly selected panel is already used
    // in another combo, swap the values so each panel appears exactly once.
    UpdateData(TRUE);

    const int sel0 = m_comboCol0.GetCurSel();
    const int sel1 = m_comboCol1.GetCurSel();
    const int sel2 = m_comboCol2.GetCurSel();

    // Determine which combo was last changed by finding a duplicate
    if (sel0 == sel1)
    {
        // col0 and col1 clash — find the missing value for col1
        const int missing = 3 - sel0 - sel2; // sum 0+1+2=3
        m_comboCol1.SetCurSel(missing);
    }
    else if (sel0 == sel2)
    {
        const int missing = 3 - sel0 - sel1;
        m_comboCol2.SetCurSel(missing);
    }
    else if (sel1 == sel2)
    {
        const int missing = 3 - sel0 - sel1;
        m_comboCol2.SetCurSel(missing);
    }

    GetSheet()->SetRestartRequired(true);
    SetModified();
}

void CPageLayout::OnBnClickedReset()
{
    // Apply default divider positions immediately — no restart needed.
    CMainFrame::Get()->ResetDividers();
}

void CPageLayout::OnOK()
{
    UpdateData(TRUE);

    const int newMode         = m_layoutMode;
    const bool newTreemapRight = (m_treemapSide == 0);
    const int newCol0         = m_comboCol0.GetCurSel();
    const int newCol1         = m_comboCol1.GetCurSel();
    const int newCol2         = m_comboCol2.GetCurSel();

    COptions::LayoutMode           = newMode;
    COptions::LayoutSideTreeMapRight = newTreemapRight;
    COptions::LayoutWideCol0       = newCol0;
    COptions::LayoutWideCol1       = newCol1;
    COptions::LayoutWideCol2       = newCol2;

    CMFCPropertyPage::OnOK();
}
