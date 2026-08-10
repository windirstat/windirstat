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
#include "StorageAnalyticsView.h"

void CCenteredEdit::OnNcCalcSize(const bool bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
    CEdit::OnNcCalcSize(bCalcValidRects, lpncsp);
    if (bCalcValidRects)
    {
        const CRect rect(lpncsp->rgrc[0]);
        const HFONT font = GetFont();
        LOGFONT lf{};
        if (font != nullptr && GetObjectW(font, sizeof(LOGFONTW), &lf) != 0)
        {
            const int fontHeight = abs(lf.lfHeight);
            if (const int rectHeight = rect.Height(); rectHeight > fontHeight)
            {
                int topPadding = (rectHeight - fontHeight) / 2;
                if (topPadding > 1) topPadding--;
                lpncsp->rgrc[0].top += topPadding;
            }
        }
    }
}

void CCenteredEdit::OnChar(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar < 32)
    {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
        return;
    }

    const std::wstring text = Text();

    const auto [selStart, selEnd] = Selection();

    std::wstring candidate = text;
    candidate.erase(static_cast<std::size_t>(selStart), static_cast<std::size_t>(selEnd - selStart));
    const std::wstring insertion(static_cast<std::size_t>(nRepCnt), static_cast<wchar_t>(nChar));
    candidate.insert(static_cast<std::size_t>(selStart), insertion);

    const std::wstring_view candidateView(candidate);
    const auto isDigit = [](const wchar_t ch) { return ch >= L'0' && ch <= L'9'; };
    const auto isValid = m_isDecimal
        ? std::ranges::count(candidateView, L'.') <= 1 &&
            std::ranges::all_of(candidateView, [&](const wchar_t ch) { return isDigit(ch) || ch == L'.'; })
        : std::ranges::all_of(candidateView, isDigit);

    if (isValid)
    {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
    }
    else
    {
        MessageBeep(MB_ICONWARNING);
    }
}

CStorageAnalyticsView::CStorageAnalyticsView() = default;

void CStorageAnalyticsView::OnSetFocus(CWnd* pOldWnd)
{
    CWinDirStatPane::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_STORAGEANALYTICS);
}

int CStorageAnalyticsView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1)
        return -1;

    const CRect rect(0, 0, 0, 0);

    m_lblTitle.Create(L"Configuration", WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this);

    // Dynamic Tier Parsing from comma-separated list
    const std::vector<std::wstring> tierNames = SplitString(Localization::Lookup(IDS_TIERS), L',');

    constexpr struct ColorPreset {
        COLORREF bgLight, bgDark;
        COLORREF borderLight, borderDark;
        COLORREF accent;
    } presets[] = {
        { RGB(254, 242, 242), RGB(60, 32, 32), RGB(254, 202, 202), RGB(100, 48, 48), RGB(239, 68, 68) }, // Red (Hot)
        { RGB(239, 246, 255), RGB(32, 45, 60), RGB(191, 219, 254), RGB(48, 68, 90), RGB(59, 130, 246) }, // Blue (Cool)
        { RGB(245, 243, 255), RGB(42, 32, 55), RGB(216, 180, 254), RGB(68, 48, 90), RGB(139, 92, 246) }, // Purple (Cold)
        { RGB(240, 253, 250), RGB(20, 50, 45), RGB(153, 246, 228), RGB(30, 75, 65), RGB(13, 148, 136) }  // Teal (Archive)
    };
    constexpr size_t numPresets = std::size(presets);

    m_tiers.clear();
    for (size_t i = 0; i < tierNames.size(); ++i)
    {
        TierInfo tier;
        tier.name = tierNames[i];

        const size_t presetIdx = i % numPresets;
        tier.bgLight = presets[presetIdx].bgLight;
        tier.bgDark = presets[presetIdx].bgDark;
        tier.borderLight = presets[presetIdx].borderLight;
        tier.borderDark = presets[presetIdx].borderDark;
        tier.accent = presets[presetIdx].accent;

        if (i > 0)
        {
            tier.lblThreshold = std::make_unique<CStatic>();
            std::wstring lblText = tier.name + L" Threshold (Days):";
            tier.lblThreshold->Create(lblText.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this);

            tier.editThreshold = std::make_unique<CCenteredEdit>();
            tier.editThreshold->Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rect, this, 2000 + static_cast<int>(i) * 2);
        }

        tier.lblCost = std::make_unique<CStatic>();
        tier.lblCost->Create(L"Cost:", WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this);

        tier.editCost = std::make_unique<CCenteredEdit>();
        tier.editCost->m_isDecimal = true;
        tier.editCost->Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rect, this, 2000 + static_cast<int>(i) * 2 + 1);

        m_tiers.push_back(std::move(tier));
    }

    m_lblUnit.Create(L"Unit:", WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this);
    m_comboUnit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, rect, this, 1007);

    m_btnRecalculate.Create(L"Recalculate", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, rect, this, 1001);

    m_comboUnit.AddString(Localization::Lookup(IDS_SPEC_TiB).c_str());
    m_comboUnit.AddString(Localization::Lookup(IDS_SPEC_GiB).c_str());
    m_comboUnit.AddString(Localization::Lookup(IDS_SPEC_MiB).c_str());
    m_comboUnit.AddString(Localization::Lookup(IDS_SPEC_KiB).c_str());
    m_comboUnit.SetCurSel(1);

    // Apply localization texts
    m_lblTitle.SetText(Localization::Lookup(IDS_ANALYTICS_CONFIG).c_str());
    m_lblUnit.SetText(Localization::Lookup(IDS_ANALYTICS_UNIT).c_str());
    UpdateCostLabels();
    m_btnRecalculate.SetText(Localization::Lookup(IDS_RECALCULATE).c_str());

    OnFontSizeChanged(0, 0);

    // Initialize edit fields with default values
    static constexpr std::array defaultThresholds = { 0.0, 30.0, 180.0, 365.0 };
    static constexpr std::array defaultCosts = { 0.03, 0.02, 0.1, 0.005 };
    const size_t numDefaults = defaultThresholds.size();

    for (size_t i = 0; i < m_tiers.size(); ++i)
    {
        if (m_tiers[i].editThreshold)
        {
            const double defDays = (i < numDefaults) ? defaultThresholds[i] : (defaultThresholds.back() + (i - (numDefaults - 1)) * 100.0);
            m_tiers[i].editThreshold->SetText(std::to_wstring(static_cast<int>(defDays)).c_str());
        }
        double defCost = (i < numDefaults) ? defaultCosts[i] : (defaultCosts.back() / static_cast<double>(i - (numDefaults - 2)));
        m_tiers[i].editCost->SetText(std::format(L"{:.2f}", defCost).c_str());
    }

    DarkMode::AdjustControls(m_hWnd);

    OnEditChange();

    return 0;
}

void CStorageAnalyticsView::OnFontSizeChanged(int, int)
{
    m_fontLeftPanelTitle.Create(-ScaleForDpi(14), FW_BOLD, wds::strFontSegoeUI);
    m_fontLeftPanel.Create(-ScaleForDpi(11), FW_NORMAL, wds::strFontSegoeUI);
    m_lblTitle.SetFont(m_fontLeftPanelTitle);
    m_lblUnit.SetFont(m_fontLeftPanel);
    m_comboUnit.SetFont(m_fontLeftPanel);
    m_btnRecalculate.SetFont(m_fontLeftPanel);
    for (auto& tier : m_tiers)
    {
        if (tier.lblThreshold) tier.lblThreshold->SetFont(m_fontLeftPanel);
        if (tier.editThreshold) tier.editThreshold->SetFont(m_fontLeftPanel);
        tier.lblCost->SetFont(m_fontLeftPanel);
        tier.editCost->SetFont(m_fontLeftPanel);
    }
    const CRect rc = ClientRect();
    OnSize(SIZE_RESTORED, rc.Width(), rc.Height());
}

void CStorageAnalyticsView::OnSize(UINT /*nType*/, int /*cx*/, int /*cy*/)
{
    const int panelX = ScaleForDpi(15);
    const int panelW = ScaleForDpi(180);
    const int controlH = ScaleForDpi(20);
    const int labelH = ScaleForDpi(18);
    const int spacing = ScaleForDpi(5);

    int currentY = ScaleForDpi(15);
    m_lblTitle.MoveWindow(panelX, currentY, panelW, controlH);
    currentY += controlH + spacing;

    for (const auto& tier : m_tiers)
    {
        if (tier.lblThreshold && tier.editThreshold && tier.lblThreshold->Handle())
        {
            tier.lblThreshold->MoveWindow(panelX, currentY, panelW, labelH);
            currentY += labelH;
            tier.editThreshold->MoveWindow(panelX, currentY, panelW, controlH);
            currentY += controlH + spacing;
        }
    }

    if (m_lblUnit.Handle())
    {
        m_lblUnit.MoveWindow(panelX, currentY, panelW, labelH);
        currentY += labelH;
        m_comboUnit.MoveWindow(panelX, currentY, panelW, ScaleForDpi(150));
        currentY += controlH + spacing;
    }

    for (const auto& tier : m_tiers)
    {
        if (tier.lblCost->Handle())
        {
            tier.lblCost->MoveWindow(panelX, currentY, panelW, labelH);
            currentY += labelH;
            tier.editCost->MoveWindow(panelX, currentY, panelW, controlH);
            currentY += controlH + spacing;
        }
    }

    if (m_btnRecalculate.Handle())
    {
        m_btnRecalculate.MoveWindow(panelX, currentY + ScaleForDpi(5), panelW, ScaleForDpi(28));
    }

    InvalidateRect(nullptr);
}

bool CStorageAnalyticsView::OnEraseBkgnd(CDC*)
{
    return true;
}

HBRUSH CStorageAnalyticsView::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_STATIC)
    {
        const COLORREF bg = DarkMode::SystemColor(COLOR_3DFACE);
        pDC->SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));
        pDC->SetBkColor(bg);
        pDC->SetBkMode(TRANSPARENT);
        static COLORREF lastBg = CLR_INVALID;
        static CBrush brush;
        if (bg != lastBg)
        {
            brush.CreateSolid(bg);
            lastBg = bg;
        }
        return brush;
    }
    if (DarkMode::IsDarkModeActive())
    {
        const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
        return brush ? brush : CWinDirStatPane::OnCtlColor(pDC, pWnd, nCtlColor);
    }
    return CWinDirStatPane::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CStorageAnalyticsView::OnBtnRecalculate()
{
    Recalculate();
}

bool CStorageAnalyticsView::PreprocessMessage(MSG* pMsg)
{
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
        const CWnd* pFocus = GetFocus();
        if (pFocus && pFocus->GetParent() == this)
        {
            TCHAR className[16]{};
            if (::GetClassName(pFocus->m_hWnd, className, 16) &&
                _tcsicmp(className, _T("Edit")) == 0)
            {
                bool shouldBypass = false;
                if (pMsg->wParam == VK_DELETE || pMsg->wParam == VK_BACK)
                {
                    shouldBypass = true;
                }
                else if (IsKeyDown(VK_CONTROL))
                {
                    const TCHAR ch = static_cast<TCHAR>(pMsg->wParam);
                    if (ch == _T('C') || ch == _T('V') || ch == _T('X') || ch == _T('A'))
                    {
                        shouldBypass = true;
                    }
                }

                if (shouldBypass)
                {
                    TranslateMessage(pMsg);
                    ::DispatchMessage(pMsg);
                    return true;
                }
            }
        }
    }
    return CWinDirStatPane::PreprocessMessage(pMsg);
}

bool CStorageAnalyticsView::ReadParameters(const bool apply)
{
    struct Parameters
    {
        bool active;
        double thresholdDays;
        double costGiB;
    };

    std::wstring text;
    std::vector<Parameters> parameters;
    parameters.reserve(m_tiers.size());
    double lastThreshold = 0.0;
    bool hasLastActive = false;

    const auto ParseText = [&](double& value, const bool allowZero) {
        wchar_t* end = nullptr;
        value = std::wcstod(text.c_str(), &end);
        return end != text.c_str() && *end == L'\0' && std::isfinite(value) &&
            (allowZero ? value >= 0.0 : value > 0.0);
    };

    for (size_t i = 0; i < m_tiers.size(); ++i)
    {
        auto& tier = m_tiers[i];
        bool active = (i == 0);
        double thresholdDays = 0.0;
        if (tier.editThreshold)
        {
            text = tier.editThreshold->Text();
            TrimString(text);
            active = !text.empty();
            if (active && (!ParseText(thresholdDays, false)
                || hasLastActive && lastThreshold >= thresholdDays)) return false;
            if (active) lastThreshold = thresholdDays;
            hasLastActive |= active;
        }

        double costGiB = 0.0;
        if (active)
        {
            text = tier.editCost->Text();
            TrimString(text);
            if (!ParseText(costGiB, true)) return false;
        }
        parameters.push_back({ active, thresholdDays, costGiB });
    }

    if (!apply) return true;
    for (size_t i = 0; i < m_tiers.size(); ++i)
    {
        auto& tier = m_tiers[i];
        tier.active = parameters[i].active;
        tier.thresholdDays = parameters[i].thresholdDays;
        tier.costGiB = parameters[i].costGiB;
        tier.filesCount = 0;
        tier.totalSize = 0;
    }
    return true;
}

void CStorageAnalyticsView::OnEditChange()
{
    m_btnRecalculate.EnableWindow(ReadParameters(false));
}

void CStorageAnalyticsView::OnEditChangeRange(UINT)
{
    OnEditChange();
}

void CStorageAnalyticsView::OnUpdate(CWnd* /*sender*/, const MODEL_CHANGE change, CItem* /*item*/)
{
    if (change == MODEL_CHANGE_NEW_ROOT || change == MODEL_CHANGE_NONE)
    {
        const auto* model = CWinDirStatModel::Get();
        if (model->IsScanSettled())
        {
            Recalculate();
        }
        else
        {
            m_hasData = false;
            InvalidateRect(nullptr);
        }
    }
}

void CStorageAnalyticsView::Recalculate()
{
    const auto* model = CWinDirStatModel::Get();
    if (!model->IsScanSettled() || !ReadParameters(true))
    {
        m_hasData = false;
        InvalidateRect(nullptr);
        return;
    }

    const FILETIME now = CurrentSystemFileTime();

    CWaitCursor wait;
    Traverse(model->GetRootItem(), now);

    m_hasData = true;
    InvalidateRect(nullptr);
}

void CStorageAnalyticsView::Traverse(CItem* item, const FILETIME now)
{
    if (item->HasChildren())
    {
        for (CItem* child : item->GetChildren())
        {
            Traverse(child, now);
        }
    }
    else if (item->IsTypeOrFlag(IT_FILE))
    {
        const ULONGLONG size = item->GetSizeLogical();
        const FILETIME lastChange = item->GetLastChange();
        const ULONGLONG nowVal = std::bit_cast<std::uint64_t>(now);
        const ULONGLONG changeVal = std::bit_cast<std::uint64_t>(lastChange);
        const double ageInDays = nowVal > changeVal ?
            static_cast<double>(nowVal - changeVal) / 864000000000.0 : 0.0;

        bool binned = false;
        for (int i = static_cast<int>(m_tiers.size()) - 1; i >= 1; --i)
        {
            if (m_tiers[i].active && ageInDays >= m_tiers[i].thresholdDays)
            {
                m_tiers[i].filesCount++;
                m_tiers[i].totalSize += size;
                binned = true;
                break;
            }
        }
        if (!binned)
        {
            m_tiers[0].filesCount++;
            m_tiers[0].totalSize += size;
        }
    }
}

double CStorageAnalyticsView::GetScaleForSelection(const int sel) const
{
    static constexpr std::array scales = {
        static_cast<double>(wds::Ti),
        static_cast<double>(wds::Gi),
        static_cast<double>(wds::Mi),
        static_cast<double>(wds::Ki),
    };
    return scales[(sel >= 0 && sel < static_cast<int>(scales.size())) ? sel : 3];
}

double CStorageAnalyticsView::GetActiveUnitScale() const
{
    int sel = m_comboUnit.GetCurSel();
    if (sel == CB_ERR) sel = 1;
    return GetScaleForSelection(sel);
}

void CStorageAnalyticsView::UpdateCostLabels() const
{
    int sel = m_comboUnit.GetCurSel();
    if (sel == CB_ERR) sel = 1;

    const std::wstring unit = m_comboUnit.ItemText(sel);

    for (auto& tier : m_tiers)
    {
        std::wstring lblText = tier.name + L" Cost ($/" + unit + L"/mo):";
        tier.lblCost->SetText(lblText.c_str());
    }
}

void CStorageAnalyticsView::OnComboUnitSelChange()
{
    const int newSel = m_comboUnit.GetCurSel();
    if (newSel == CB_ERR) return;

    if (newSel != m_lastUnitSel)
    {
        const double oldScale = GetScaleForSelection(m_lastUnitSel);
        const double newScale = GetScaleForSelection(newSel);
        const double ratio = newScale / oldScale;

        auto ScaleEditField = [&](CCenteredEdit& edit) {
            std::wstring text = edit.Text();
            TrimString(text);
            wchar_t* end = nullptr;
            double val = std::wcstod(text.c_str(), &end);
            if (val > 0 && std::isfinite(val) && end != text.c_str() && *end == L'\0')
            {
                val *= ratio;
                std::wstring formatted = std::format(L"{:.8f}", val);
                TrimString(formatted, L'0', true);
                TrimString(formatted, L'.', true);
                edit.SetText(formatted.c_str());
            }
        };

        for (auto& tier : m_tiers)
        {
            ScaleEditField(*tier.editCost);
        }

        m_lastUnitSel = newSel;
    }

    UpdateCostLabels();
    Recalculate();
}

void CStorageAnalyticsView::OnDraw(CDC* pDC)
{
    const CRect clientRect = ClientRect();

    CDC memDC(pDC);
    CBitmap memBitmap(pDC, clientRect.Width(), clientRect.Height());

    GdiObjectSelection selectBitmap(&memDC, &memBitmap);

    const bool isDark = DarkMode::IsDarkModeActive();
    const COLORREF bgControl = DarkMode::SystemColor(COLOR_3DFACE);
    const COLORREF fgText = DarkMode::SystemColor(COLOR_WINDOWTEXT);
    const COLORREF fgMuted = DarkMode::SystemColor(COLOR_GRAYTEXT);
    const COLORREF clrBorder = DarkMode::SystemColor(COLOR_3DSHADOW);

    memDC.FillSolidRect(&clientRect, bgControl);

    const int leftWidth = ScaleForDpi(210);
    CRect rightRect = clientRect;
    rightRect.left = leftWidth;
    memDC.FillSolidRect(&rightRect, isDark ? RGB(26, 26, 28) : RGB(246, 246, 249));

    CPen penBorder(PS_SOLID, 1, clrBorder);
    {
        GdiObjectSelection selectPen(&memDC, &penBorder);
        memDC.MoveTo(leftWidth, 0);
        memDC.LineTo(leftWidth, clientRect.Height());
    }

    CFont fontTitle(-ScaleForDpi(20), FW_BOLD, wds::strFontSegoeUI);
    CFont fontCardVal(-ScaleForDpi(18), FW_BOLD, wds::strFontSegoeUI);
    CFont fontCardLbl(-ScaleForDpi(11), FW_NORMAL, wds::strFontSegoeUI);
    CFont fontBody(-ScaleForDpi(12), FW_NORMAL, wds::strFontSegoeUI);

    ScopedBkMode setBkMode(&memDC, TRANSPARENT);
    ScopedTextColor setTextColor(&memDC, fgText);
    const auto DrawText = [&](CFont& font, const COLORREF color, const int x, const int y,
        const std::wstring& text) {
        GdiObjectSelection selectFont(&memDC, &font);
        ScopedTextColor selectColor(&memDC, color);
        memDC.TextOut(x, y, text);
    };
    DrawText(fontTitle, fgText, leftWidth + ScaleForDpi(20), ScaleForDpi(20),
        Localization::Lookup(IDS_ANALYTICS_TITLE));

    if (!m_hasData || m_tiers.empty())
    {
        GdiObjectSelection selectFont(&memDC, &fontTitle);
        ScopedTextColor setMuted(&memDC, fgMuted);
        CRect msgRect = rightRect;
        msgRect.Deflate(ScaleForDpi(50), ScaleForDpi(150));
        memDC.DrawTextW(L"No statistics available.\n\nPlease scan a drive or folder, then click Recalculate to view the dashboard.", &msgRect, DT_CENTER | DT_WORDBREAK);
    }
    else
    {
        const double scale = GetActiveUnitScale();
        const ULONGLONG totalSize = std::accumulate(m_tiers.begin(), m_tiers.end(), static_cast<ULONGLONG>(0),
            [](const ULONGLONG total, const TierInfo& tier) { return total + tier.totalSize; });
        const double totalUnit = static_cast<double>(totalSize) / scale;

        const double currentCost = totalUnit * m_tiers[0].costGiB;
        const double optimizedCost = std::accumulate(m_tiers.begin(), m_tiers.end(), 0.0,
            [scale](const double total, const TierInfo& tier) {
                return tier.active ? total + static_cast<double>(tier.totalSize) / scale * tier.costGiB : total;
            });
        const double savings = std::max(0.0, currentCost - optimizedCost);
        const double savingsPct = currentCost > 0.0 ? (savings / currentCost) * 100.0 : 0.0;

        // Draw Metric Cards
        const int barW = ScaleForDpi(765);
        const int cardGap = ScaleForDpi(15);
        const int cardH = ScaleForDpi(95);
        const int cardY = ScaleForDpi(70);

        struct CardDrawData {
            const TierInfo* tier;
            std::wstring legendDesc;
        };
        std::vector<CardDrawData> activeCards;

        std::wstring daysStr = Localization::Lookup(IDS_GENERIC_DAYS);
        const auto findActiveTierFrom = [&](const size_t index) {
            return std::ranges::find_if(m_tiers.begin() + static_cast<std::ptrdiff_t>(index), m_tiers.end(),
                [](const TierInfo& tier) { return tier.active; });
        };

        for (size_t i = 0; i < m_tiers.size(); ++i)
        {
            const auto& tier = m_tiers[i];
            if (tier.active)
            {
                CardDrawData card{ &tier, tier.name };
                if (i == 0)
                {
                    const auto firstActiveTier = findActiveTierFrom(1);
                    if (firstActiveTier != m_tiers.end())
                    {
                        card.legendDesc += std::format(L" (<{:.0f} {})", firstActiveTier->thresholdDays, daysStr);
                    }
                    else card.legendDesc += L" (All Files)";
                }
                else
                {
                    const auto nextActiveTier = findActiveTierFrom(i + 1);
                    if (nextActiveTier != m_tiers.end())
                    {
                        card.legendDesc += std::format(L" ({:.0f}-{:.0f} {})", tier.thresholdDays,
                            nextActiveTier->thresholdDays, daysStr);
                    }
                    else card.legendDesc += std::format(L" (>{:.0f} {})", tier.thresholdDays, daysStr);
                }

                activeCards.push_back(std::move(card));
            }
        }

        const int activeCount = static_cast<int>(activeCards.size());
        const int cardW = (barW - (activeCount - 1) * cardGap) / (activeCount > 0 ? activeCount : 1);

        for (int i = 0; i < activeCount; ++i)
        {
            const auto& card = activeCards[i];
            const auto& tier = *card.tier;
            const int cardX = leftWidth + ScaleForDpi(20) + i * (cardW + cardGap);
            CRect rcCard(cardX, cardY, cardX + cardW, cardY + cardH);

            memDC.FillSolidRect(&rcCard, isDark ? tier.bgDark : tier.bgLight);
            CBrush brBorder(isDark ? tier.borderDark : tier.borderLight);
            memDC.FrameRect(&rcCard, &brBorder);

            constexpr int accentBarW = 4;
            CRect rcAccent = rcCard;
            rcAccent.right = rcAccent.left + ScaleForDpi(accentBarW);
            memDC.FillSolidRect(&rcAccent, tier.accent);

            const int textX = rcCard.left + ScaleForDpi(12);
            DrawText(fontCardLbl, fgMuted, textX, rcCard.top + ScaleForDpi(10), tier.name);
            DrawText(fontCardVal, fgText, textX, rcCard.top + ScaleForDpi(30), FormatSizeSuffixes(tier.totalSize));
            DrawText(fontBody, fgMuted, textX, rcCard.top + ScaleForDpi(53),
                FormatCount(tier.filesCount) + L" files");
            DrawText(fontCardVal, tier.accent, textX, rcCard.top + ScaleForDpi(70),
                std::format(L"${:.2f}/mo", static_cast<double>(tier.totalSize) / scale * tier.costGiB));
        }

        // Draw Distribution Bars
        const int barH = ScaleForDpi(16);
        const int barX = leftWidth + ScaleForDpi(20);

        auto DrawSegmentedBar = [&](const int yPos, const std::wstring& barLabel, const bool sizeBar) {
            {
                GdiObjectSelection selectFont(&memDC, &fontBody);
                ScopedTextColor setLabelColor(&memDC, fgText);
                memDC.TextOut(barX, yPos - ScaleForDpi(18), barLabel);
            }

            const double totalVal = std::accumulate(m_tiers.begin(), m_tiers.end(), 0.0,
                [&](const double total, const TierInfo& tier) {
                    return tier.active
                        ? total + (sizeBar ? static_cast<double>(tier.totalSize) : static_cast<double>(tier.filesCount))
                        : total;
                });

            int currentX = barX;

            for (const auto& tier : m_tiers)
            {
                if (tier.active)
                {
                    const double val = sizeBar ? static_cast<double>(tier.totalSize) : static_cast<double>(tier.filesCount);
                    if (val > 0 && totalVal > 0)
                    {
                        const int w = static_cast<int>((val / totalVal) * barW);
                        if (w > 0)
                        {
                            CRect rc(currentX, yPos, currentX + w, yPos + barH);
                            memDC.FillSolidRect(&rc, tier.accent);
                            currentX += w;
                        }
                    }
                }
            }

            if (totalVal > 0 && currentX < barX + barW)
            {
                const CRect rcRemainder(currentX, yPos, barX + barW, yPos + barH);
                COLORREF remainderColor = m_tiers[0].accent;
                auto reversedTiers = m_tiers | std::views::reverse;
                if (const auto it = std::ranges::find_if(reversedTiers, [&](const TierInfo& tier) {
                    return tier.active && (sizeBar ? tier.totalSize > 0 : tier.filesCount > 0);
                }); it != reversedTiers.end())
                {
                    remainderColor = it->accent;
                }
                memDC.FillSolidRect(&rcRemainder, remainderColor);
            }

            const CRect rcFrame(barX, yPos, barX + barW, yPos + barH);
            CBrush brFrame(clrBorder);
            memDC.FrameRect(&rcFrame, &brFrame);
        };

        DrawSegmentedBar(ScaleForDpi(205), L"File Count Distribution", false);
        DrawSegmentedBar(ScaleForDpi(245), L"Capacity Size Distribution", true);

        // Draw Legends
        const int legendY = ScaleForDpi(270);
        const int legendColW = barW / (activeCount > 0 ? activeCount : 1);
        for (int i = 0; i < activeCount; ++i)
        {
            const int legendX = barX + i * legendColW;
            CRect rcColor(legendX, legendY + ScaleForDpi(2), legendX + ScaleForDpi(10), legendY + ScaleForDpi(12));
            memDC.FillSolidRect(&rcColor, activeCards[i].tier->accent);
            CBrush brLegend(clrBorder);
            memDC.FrameRect(&rcColor, &brLegend);

            {
                GdiObjectSelection selectFont(&memDC, &fontBody);
                ScopedTextColor setLegendColor(&memDC, fgText);
                memDC.TextOut(legendX + ScaleForDpi(16), legendY, activeCards[i].legendDesc);
            }
        }

        // Draw Storage Cost Savings Banner
        CRect rcSavings(barX, ScaleForDpi(300), barX + barW, ScaleForDpi(385));
        const COLORREF bgSavings = isDark ? RGB(32, 50, 36) : RGB(240, 253, 244);
        const COLORREF borderSavings = isDark ? RGB(48, 80, 52) : RGB(187, 247, 208);
        const COLORREF textSavings = isDark ? RGB(74, 222, 128) : RGB(22, 163, 74);

        memDC.FillSolidRect(&rcSavings, bgSavings);
        CBrush brSavings(borderSavings);
        memDC.FrameRect(&rcSavings, &brSavings);

        constexpr int numSavingsColumns = 3;
        const int colW = barW / numSavingsColumns;

        const int labelY = rcSavings.top + ScaleForDpi(15);
        DrawText(fontCardLbl, fgMuted, rcSavings.left + ScaleForDpi(15), labelY,
            Localization::Lookup(IDS_CURRENT_COST_LABEL));
        DrawText(fontCardVal, fgText, rcSavings.left + ScaleForDpi(15),
            rcSavings.top + ScaleForDpi(38), std::format(L"${:.2f}/mo", currentCost));
        DrawText(fontCardLbl, fgMuted, rcSavings.left + colW + ScaleForDpi(10), labelY,
            Localization::Lookup(IDS_OPTIMIZED_COST_LABEL));
        DrawText(fontCardVal, fgText, rcSavings.left + colW + ScaleForDpi(10),
            rcSavings.top + ScaleForDpi(38), std::format(L"${:.2f}/mo", optimizedCost));
        DrawText(fontCardLbl, textSavings, rcSavings.left + colW * 2 + ScaleForDpi(10), labelY,
            Localization::Lookup(IDS_STORAGE_SAVINGS));
        DrawText(fontTitle, textSavings, rcSavings.left + colW * 2 + ScaleForDpi(10),
            rcSavings.top + ScaleForDpi(36), std::format(L"${:.2f}/mo ({:.1f}%)", savings, savingsPct));
    }

    pDC->BitBlt(0, 0, clientRect.Width(), clientRect.Height(), &memDC, 0, 0, SRCCOPY);
}
