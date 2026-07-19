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
#include "WinDirStatPane.h"

//
// CCenteredEdit. Custom edit control that vertically centers its text.
//
class CCenteredEdit final : public CEdit
{
public:
    bool m_isDecimal = false;

protected:
    afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    DECLARE_MESSAGE_MAP()
};

//
// CStorageAnalyticsView. Shows storage tier analytics and cloud cost estimations.
//
class CStorageAnalyticsView final : public CWinDirStatPane
{
protected:
    CStorageAnalyticsView();
    ~CStorageAnalyticsView() override = default;
    DECLARE_DYNCREATE(CStorageAnalyticsView)

    void OnDraw(CDC* pDC) override;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnBtnRecalculate();
    afx_msg void OnComboUnitSelChange();
    afx_msg void OnEditChange();
    afx_msg void OnEditChangeRange(UINT nID);

    DECLARE_MESSAGE_MAP()

public:
    struct TierInfo {
        std::wstring name;

        std::unique_ptr<CStatic> lblThreshold;
        std::unique_ptr<CCenteredEdit> editThreshold;
        std::unique_ptr<CStatic> lblCost;
        std::unique_ptr<CCenteredEdit> editCost;

        double thresholdDays = 0.0;
        double costGiB = 0.0;
        bool active = true;

        ULONGLONG filesCount = 0;
        ULONGLONG totalSize = 0;

        COLORREF bgLight, bgDark;
        COLORREF borderLight, borderDark;
        COLORREF accent;
    };

private:
    // Recalculates metrics by traversing the loaded directory tree
    void Recalculate();

    // Validates inputs and enables/disables the recalculate button
    bool AreParametersValid();

    // Performs a performant single-pass DFS traversal over CItem hierarchy
    void Traverse(CItem* item, FILETIME now);

    // Helper functions for unit selection and cost label generation
    double GetScaleForSelection(int sel) const;
    double GetActiveUnitScale() const;
    void UpdateCostLabels();

    std::vector<TierInfo> m_tiers;

    // UI Configuration controls on the left panel
    CStatic m_lblTitle;
    CStatic m_lblUnit;
    CComboBox m_comboUnit;
    CButton m_btnRecalculate;

    // GDI resources for dashboard painting
    CFont m_fontLeftPanel;
    CFont m_fontLeftPanelTitle;

    int m_lastUnitSel = 1;
    bool m_hasData = false;
};
