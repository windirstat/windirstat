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
class CCenteredEdit final : public MessageTarget<CCenteredEdit, CEdit>
{
public:
    bool m_isDecimal = false;

protected:
    void OnNcCalcSize(bool bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
    void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
public:
    static std::span<const RouteEntry> Routes();

};

//
// CStorageAnalyticsView. Shows storage tier analytics and cloud cost estimations.
//
class CStorageAnalyticsView final : public MessageTarget<CStorageAnalyticsView, CWinDirStatPane>
{
public:
    CStorageAnalyticsView();
    ~CStorageAnalyticsView() override = default;

    void OnDraw(CDC* pDC) override;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    bool PreprocessMessage(MSG* pMsg) override;
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(CWnd* pOldWnd);
    void OnSize(UINT nType, int cx, int cy);
    bool OnEraseBkgnd(CDC* pDC);
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    void OnBtnRecalculate();
    void OnComboUnitSelChange();
    void OnEditChange();
    void OnEditChangeRange(UINT nID);

static std::span<const RouteEntry> Routes();

struct TierInfo {
        std::wstring name;

        std::unique_ptr<CStatic> lblThreshold;
        std::unique_ptr<CCenteredEdit> editThreshold;
        std::unique_ptr<CStatic> lblCost;
        std::unique_ptr<CCenteredEdit> editCost;

        double thresholdDays = 0.0;
        double costGiB = 0.0;

        ULONGLONG filesCount = 0;
        ULONGLONG totalSize = 0;

        COLORREF bgLight{}, bgDark{};
        COLORREF borderLight{}, borderDark{};
        COLORREF accent{};

        bool active = true;
    };

private:
    // Recalculates metrics by traversing the loaded directory tree
    void Recalculate();

    // Validates inputs and optionally applies them to the tiers
    bool ReadParameters(bool apply);

    // Performs a performant single-pass DFS traversal over CItem hierarchy
    void Traverse(CItem* item, FILETIME now);

    // Helper functions for unit selection and cost label generation
    double GetScaleForSelection(int sel) const;
    double GetActiveUnitScale() const;
    void UpdateCostLabels() const;

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

inline std::span<const RouteEntry> CCenteredEdit::Routes()
{
    using ThisClass = CCenteredEdit;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnNcCalcSize>(WM_NCCALCSIZE),
        Route::Window<&ThisClass::OnChar>(WM_CHAR),
    };
    return entries;
}

inline std::span<const RouteEntry> CStorageAnalyticsView::Routes()
{
    using ThisClass = CStorageAnalyticsView;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnCreate>(WM_CREATE),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&ThisClass::OnCtlColor>(WM_CTLCOLOR),
        Route::Control<&CStorageAnalyticsView::OnBtnRecalculate>(BN_CLICKED, 1001),
        Route::Control<&CStorageAnalyticsView::OnComboUnitSelChange>(CBN_SELCHANGE, 1007),
        Route::Control<&CStorageAnalyticsView::OnEditChangeRange>(EN_CHANGE, 2000, 2100),
    };
    return entries;
}
