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

#pragma once

#include "pch.h"

//
// CLayoutChooserDlg. Standalone popup dialog for choosing the window layout.
// Replaces the former "Layout" page in the Options property sheet.
// The top half shows three GDI-drawn mode cards; the bottom half shows
// mode-specific sub-options (treemap position or column order).
//
class CLayoutChooserDlg final : public CDialog
{
    DECLARE_DYNAMIC(CLayoutChooserDlg)
    enum : UINT { IDD = IDD_PAGE_LAYOUT };

public:
    explicit CLayoutChooserDlg(CWnd* pParent = nullptr);

private:
    // Dialog state (filled from COptions in OnInitDialog, written back in OnOK)
    int  m_layoutMode  = 0;
    int  m_treemapSide = 0; // DDX: 0 = right, 1 = left

    // Card hover tracking
    int  m_hovered = -1;
    bool m_trackingMouse = false;

    // Sub-option controls (DDX_Control)
    CButton   m_radioRight;
    CButton   m_radioLeft;
    CComboBox m_comboCol[3];

    // Card geometry helpers
    CRect GetCardArea() const;
    CRect GetCardRect(int idx) const;
    int   HitCard(CPoint pt) const;

    // Drawing
    void DrawCard(CDC& dc, int idx, const CRect& r) const;

    // Sub-option helpers
    void UpdateSubControls();
    void FillCombos();
    void AutoSwapCombos(int changedIdx);

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnBnClickedRadioRight();
    afx_msg void OnBnClickedRadioLeft();
    afx_msg void OnCbnSelchangeCombo(UINT nID); // ON_CONTROL_RANGE handler
    afx_msg void OnBnClickedReset();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
