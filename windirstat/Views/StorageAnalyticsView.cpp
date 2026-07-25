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

BEGIN_MESSAGE_MAP(CCenteredEdit, CEdit)
    ON_WM_NCCALCSIZE()
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CCenteredEdit::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
    CEdit::OnNcCalcSize(bCalcValidRects, lpncsp);
    if (bCalcValidRects)
    {
        CRect rect(lpncsp->rgrc[0]);
        CFont* pFont = GetFont();
        LOGFONT lf{};
        if (pFont && pFont->GetLogFont(&lf))
        {
            int fontHeight = abs(lf.lfHeight);
            int rectHeight = rect.Height();
            if (rectHeight > fontHeight)
            {
                int topPadding = (rectHeight - fontHeight) / 2;
                if (topPadding > 1) topPadding--;
                lpncsp->rgrc[0].top += topPadding;
            }
        }
    }
}

void CCenteredEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar < 32)
    {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
        return;
    }

    CString text;
    GetWindowTextW(text);

    int selStart = 0;
    int selEnd = 0;
    GetSel(selStart, selEnd);

    CString candidate = text;
    candidate.Delete(selStart, selEnd - selStart);
    const CString insertion(static_cast<wchar_t>(nChar), static_cast<int>(nRepCnt));
    candidate.Insert(selStart, insertion.GetString());

    const std::wstring_view candidateView(candidate.GetString(), candidate.GetLength());
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
        ::MessageBeep(MB_ICONWARNING);
    }
}

IMPLEMENT_DYNCREATE(CStorageAnalyticsView, CWinDirStatPane)

BEGIN_MESSAGE_MAP(CStorageAnalyticsView, CWinDirStatPane)
    ON_WM_CREATE()
    ON_WM_SETFOCUS()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(1001, &CStorageAnalyticsView::OnBtnRecalculate)
    ON_CBN_SELCHANGE(1007, &CStorageAnalyticsView::OnComboUnitSelChange)
    ON_CONTROL_RANGE(EN_CHANGE, 2000, 2100, &CStorageAnalyticsView::OnEditChangeRange)
END_MESSAGE_MAP()

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

    // Create left panel fonts
    m_fontLeftPanelTitle.CreateFont(-DpiRest(14, this), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wds::strFontSegoeUI);
    m_fontLeftPanel.CreateFont(-DpiRest(11, this), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wds::strFontSegoeUI);

    // Dynamic Tier Parsing from comma-separated list
    std::vector<std::wstring> tierNames = SplitString(Localization::Lookup(IDS_TIERS), L',');

    struct ColorPreset {
        COLORREF bgLight, bgDark;
        COLORREF borderLight, borderDark;
        COLORREF accent;
    } presets[] = {
        { RGB(254, 242, 242), RGB(60, 32, 32), RGB(254, 202, 202), RGB(100, 48, 48), RGB(239, 68, 68) }, // Red (Hot)
        { RGB(239, 246, 255), RGB(32, 45, 60), RGB(191, 219, 254), RGB(48, 68, 90), RGB(59, 130, 246) }, // Blue (Cool)
        { RGB(245, 243, 255), RGB(42, 32, 55), RGB(216, 180, 254), RGB(68, 48, 90), RGB(139, 92, 246) }, // Purple (Cold)
        { RGB(240, 253, 250), RGB(20, 50, 45), RGB(153, 246, 228), RGB(30, 75, 65), RGB(13, 148, 136) }  // Teal (Archive)
    };
    const size_t numPresets = std::size(presets);

    m_tiers.clear();
    for (size_t i = 0; i < tierNames.size(); ++i)
    {
        TierInfo tier;
        tier.name = tierNames[i];

        size_t presetIdx = i % numPresets;
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
            tier.lblThreshold->SetFont(&m_fontLeftPanel);

            tier.editThreshold = std::make_unique<CCenteredEdit>();
            tier.editThreshold->Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rect, this, 2000 + static_cast<int>(i) * 2);
            tier.editThreshold->SetFont(&m_fontLeftPanel);
        }

        tier.lblCost = std::make_unique<CStatic>();
        tier.lblCost->Create(L"Cost:", WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this);
        tier.lblCost->SetFont(&m_fontLeftPanel);

        tier.editCost = std::make_unique<CCenteredEdit>();
        tier.editCost->m_isDecimal = true;
        tier.editCost->Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rect, this, 2000 + static_cast<int>(i) * 2 + 1);
        tier.editCost->SetFont(&m_fontLeftPanel);

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
    m_lblTitle.SetWindowTextW(Localization::Lookup(IDS_ANALYTICS_CONFIG).c_str());
    m_lblUnit.SetWindowTextW(Localization::Lookup(IDS_ANALYTICS_UNIT).c_str());
    UpdateCostLabels();
    m_btnRecalculate.SetWindowTextW(Localization::Lookup(IDS_RECALCULATE).c_str());

    m_lblTitle.SetFont(&m_fontLeftPanelTitle);
    m_lblUnit.SetFont(&m_fontLeftPanel);
    m_comboUnit.SetFont(&m_fontLeftPanel);
    m_btnRecalculate.SetFont(&m_fontLeftPanel);

    // Initialize edit fields with default values
    const std::vector<double> defaultThresholds = { 0.0, 30.0, 180.0, 365.0 };
    const std::vector<double> defaultCosts = { 0.03, 0.02, 0.1, 0.005 };
    const size_t numDefaults = defaultThresholds.size();

    for (size_t i = 0; i < m_tiers.size(); ++i)
    {
        if (m_tiers[i].editThreshold)
        {
            double defDays = (i < numDefaults) ? defaultThresholds[i] : (defaultThresholds.back() + (i - (numDefaults - 1)) * 100.0);
            m_tiers[i].editThreshold->SetWindowTextW(std::to_wstring(static_cast<int>(defDays)).c_str());
        }
        double defCost = (i < numDefaults) ? defaultCosts[i] : (defaultCosts.back() / static_cast<double>(i - (numDefaults - 2)));
        m_tiers[i].editCost->SetWindowTextW(std::format(L"{:.2f}", defCost).c_str());
    }

    DarkMode::AdjustControls(m_hWnd);

    OnEditChange();

    return 0;
}

void CStorageAnalyticsView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);

    const int panelX = DpiRest(15, this);
    const int panelW = DpiRest(180, this);
    const int controlH = DpiRest(20, this);
    const int labelH = DpiRest(18, this);
    const int spacing = DpiRest(5, this);

    int currentY = DpiRest(15, this);
    m_lblTitle.MoveWindow(panelX, currentY, panelW, controlH);
    currentY += controlH + spacing;

    for (auto& tier : m_tiers)
    {
        if (tier.lblThreshold && tier.editThreshold && tier.lblThreshold->GetSafeHwnd())
        {
            tier.lblThreshold->MoveWindow(panelX, currentY, panelW, labelH);
            currentY += labelH;
            tier.editThreshold->MoveWindow(panelX, currentY, panelW, controlH);
            currentY += controlH + spacing;
        }
    }

    if (m_lblUnit.GetSafeHwnd())
    {
        m_lblUnit.MoveWindow(panelX, currentY, panelW, labelH);
        currentY += labelH;
        m_comboUnit.MoveWindow(panelX, currentY, panelW, DpiRest(150, this));
        currentY += controlH + spacing;
    }

    for (auto& tier : m_tiers)
    {
        if (tier.lblCost->GetSafeHwnd())
        {
            tier.lblCost->MoveWindow(panelX, currentY, panelW, labelH);
            currentY += labelH;
            tier.editCost->MoveWindow(panelX, currentY, panelW, controlH);
            currentY += controlH + spacing;
        }
    }

    if (m_btnRecalculate.GetSafeHwnd())
    {
        m_btnRecalculate.MoveWindow(panelX, currentY + DpiRest(5, this), panelW, DpiRest(28, this));
    }

    InvalidateRect(nullptr);
}

BOOL CStorageAnalyticsView::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

HBRUSH CStorageAnalyticsView::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_STATIC)
    {
        COLORREF bg = DarkMode::WdsSysColor(COLOR_3DFACE);
        pDC->SetTextColor(DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        pDC->SetBkColor(bg);
        pDC->SetBkMode(TRANSPARENT);
        static COLORREF lastBg = CLR_INVALID;
        static HBRUSH hBrush = nullptr;
        if (bg != lastBg)
        {
            if (hBrush) ::DeleteObject(hBrush);
            hBrush = ::CreateSolidBrush(bg);
            lastBg = bg;
        }
        return hBrush;
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

BOOL CStorageAnalyticsView::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
        CWnd* pFocus = GetFocus();
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
                else if (::GetKeyState(VK_CONTROL) & 0x8000)
                {
                    const TCHAR ch = static_cast<TCHAR>(pMsg->wParam);
                    if (ch == _T('C') || ch == _T('V') || ch == _T('X') || ch == _T('A'))
                    {
                        shouldBypass = true;
                    }
                }

                if (shouldBypass)
                {
                    ::TranslateMessage(pMsg);
                    ::DispatchMessage(pMsg);
                    return TRUE;
                }
            }
        }
    }
    return CWinDirStatPane::PreTranslateMessage(pMsg);
}

bool CStorageAnalyticsView::ReadParameters(const bool apply)
{
    struct Parameters
    {
        bool active;
        double thresholdDays;
        double costGiB;
    };

    CString text;
    std::vector<Parameters> parameters;
    parameters.reserve(m_tiers.size());
    double lastThreshold = 0.0;
    bool hasLastActive = false;

    const auto ParseText = [&](double& value, const bool allowZero) {
        wchar_t* end = nullptr;
        value = std::wcstod(text.GetString(), &end);
        return end != text.GetString() && *end == L'\0' && std::isfinite(value) &&
            (allowZero ? value >= 0.0 : value > 0.0);
    };

    for (size_t i = 0; i < m_tiers.size(); ++i)
    {
        auto& tier = m_tiers[i];
        bool active = (i == 0);
        double thresholdDays = 0.0;
        if (tier.editThreshold)
        {
            tier.editThreshold->GetWindowTextW(text);
            text.Trim();
            active = !text.IsEmpty();
            if (active && (!ParseText(thresholdDays, false)
                || hasLastActive && lastThreshold >= thresholdDays)) return false;
            if (active) lastThreshold = thresholdDays;
            hasLastActive |= active;
        }

        double costGiB = 0.0;
        if (active)
        {
            tier.editCost->GetWindowTextW(text);
            text.Trim();
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

    FILETIME now;
    ::GetSystemTimeAsFileTime(&now);

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
        double ageInDays = 0.0;
        if (nowVal > changeVal)
        {
            ageInDays = static_cast<double>(nowVal - changeVal) / 864000000000.0;
        }

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

double CStorageAnalyticsView::GetScaleForSelection(int sel) const
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

void CStorageAnalyticsView::UpdateCostLabels()
{
    int sel = m_comboUnit.GetCurSel();
    if (sel == CB_ERR) sel = 1;

    CString unitText;
    m_comboUnit.GetLBText(sel, unitText);
    std::wstring unit = unitText.GetString();

    for (auto& tier : m_tiers)
    {
        std::wstring lblText = tier.name + L" Cost ($/" + unit + L"/mo):";
        tier.lblCost->SetWindowTextW(lblText.c_str());
    }
}

void CStorageAnalyticsView::OnComboUnitSelChange()
{
    int newSel = m_comboUnit.GetCurSel();
    if (newSel == CB_ERR) return;

    if (newSel != m_lastUnitSel)
    {
        double oldScale = GetScaleForSelection(m_lastUnitSel);
        double newScale = GetScaleForSelection(newSel);
        double ratio = newScale / oldScale;

        auto ScaleEditField = [&](CCenteredEdit& edit) {
            CString text;
            edit.GetWindowTextW(text);
            text.Trim();
            wchar_t* end = nullptr;
            double val = std::wcstod(text.GetString(), &end);
            if (val > 0 && std::isfinite(val) && end != text.GetString() && *end == L'\0')
            {
                val *= ratio;
                CString formatted(std::format(L"{:.8f}", val).c_str());
                formatted.TrimRight(L'0');
                formatted.TrimRight(L'.');
                edit.SetWindowTextW(formatted.GetString());
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
    CRect clientRect;
    GetClientRect(&clientRect);

    CDC memDC;
    CBitmap memBitmap;
    memDC.CreateCompatibleDC(pDC);
    memBitmap.CreateCompatibleBitmap(pDC, clientRect.Width(), clientRect.Height());

    CSelectObject selectBitmap(&memDC, &memBitmap);

    const bool isDark = DarkMode::IsDarkModeActive();
    const COLORREF bgControl = DarkMode::WdsSysColor(COLOR_3DFACE);
    const COLORREF fgText = DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
    const COLORREF fgMuted = DarkMode::WdsSysColor(COLOR_GRAYTEXT);
    const COLORREF clrBorder = DarkMode::WdsSysColor(COLOR_3DSHADOW);

    memDC.FillSolidRect(&clientRect, bgControl);

    const int leftWidth = DpiRest(210, this);
    CRect rightRect = clientRect;
    rightRect.left = leftWidth;
    memDC.FillSolidRect(&rightRect, isDark ? RGB(26, 26, 28) : RGB(246, 246, 249));

    CPen penBorder(PS_SOLID, 1, clrBorder);
    {
        CSelectObject selectPen(&memDC, &penBorder);
        memDC.MoveTo(leftWidth, 0);
        memDC.LineTo(leftWidth, clientRect.Height());
    }

    CFont fontTitle;
    CFont fontCardVal;
    CFont fontCardLbl;
    CFont fontBody;
    const auto CreateFont = [&](CFont& font, const int size, const int weight) {
        font.CreateFont(-DpiRest(size, this), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, wds::strFontSegoeUI);
    };
    CreateFont(fontTitle, 20, FW_BOLD);
    CreateFont(fontCardVal, 18, FW_BOLD);
    CreateFont(fontCardLbl, 11, FW_NORMAL);
    CreateFont(fontBody, 12, FW_NORMAL);

    CSetBkMode setBkMode(&memDC, TRANSPARENT);
    CSetTextColor setTextColor(&memDC, fgText);
    const auto DrawText = [&](CFont& font, const COLORREF color, const int x, const int y,
        const std::wstring& text) {
        CSelectObject selectFont(&memDC, &font);
        CSetTextColor selectColor(&memDC, color);
        memDC.TextOutW(x, y, text.c_str());
    };
    DrawText(fontTitle, fgText, leftWidth + DpiRest(20, this), DpiRest(20, this),
        Localization::Lookup(IDS_ANALYTICS_TITLE));

    if (!m_hasData || m_tiers.empty())
    {
        CSelectObject selectFont(&memDC, &fontTitle);
        CSetTextColor setMuted(&memDC, fgMuted);
        CRect msgRect = rightRect;
        msgRect.DeflateRect(DpiRest(50, this), DpiRest(150, this));
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
        const int barW = DpiRest(765, this);
        const int cardGap = DpiRest(15, this);
        const int cardH = DpiRest(95, this);
        const int cardY = DpiRest(70, this);

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
            const int cardX = leftWidth + DpiRest(20, this) + i * (cardW + cardGap);
            CRect rcCard(cardX, cardY, cardX + cardW, cardY + cardH);

            memDC.FillSolidRect(&rcCard, isDark ? tier.bgDark : tier.bgLight);
            CBrush brBorder(isDark ? tier.borderDark : tier.borderLight);
            memDC.FrameRect(&rcCard, &brBorder);

            const int accentBarW = 4;
            CRect rcAccent = rcCard;
            rcAccent.right = rcAccent.left + DpiRest(accentBarW, this);
            memDC.FillSolidRect(&rcAccent, tier.accent);

            const int textX = rcCard.left + DpiRest(12, this);
            DrawText(fontCardLbl, fgMuted, textX, rcCard.top + DpiRest(10, this), tier.name);
            DrawText(fontCardVal, fgText, textX, rcCard.top + DpiRest(30, this), FormatSizeSuffixes(tier.totalSize));
            DrawText(fontBody, fgMuted, textX, rcCard.top + DpiRest(53, this),
                FormatCount(tier.filesCount) + L" files");
            DrawText(fontCardVal, tier.accent, textX, rcCard.top + DpiRest(70, this),
                std::format(L"${:.2f}/mo", static_cast<double>(tier.totalSize) / scale * tier.costGiB));
        }

        // Draw Distribution Bars
        const int barH = DpiRest(16, this);
        const int barX = leftWidth + DpiRest(20, this);

        auto DrawSegmentedBar = [&](const int yPos, const std::wstring& barLabel, bool sizeBar) {
            {
                CSelectObject selectFont(&memDC, &fontBody);
                CSetTextColor setLabelColor(&memDC, fgText);
                memDC.TextOutW(barX, yPos - DpiRest(18, this), barLabel.c_str());
            }

            const double totalVal = std::accumulate(m_tiers.begin(), m_tiers.end(), 0.0,
                [&](const double total, const TierInfo& tier) {
                    return tier.active
                        ? total + (sizeBar ? static_cast<double>(tier.totalSize) : static_cast<double>(tier.filesCount))
                        : total;
                });

            int currentX = barX;

            for (size_t i = 0; i < m_tiers.size(); ++i)
            {
                const auto& tier = m_tiers[i];
                if (tier.active)
                {
                    double val = sizeBar ? static_cast<double>(tier.totalSize) : static_cast<double>(tier.filesCount);
                    if (val > 0 && totalVal > 0)
                    {
                        int w = static_cast<int>((val / totalVal) * barW);
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
                CRect rcRemainder(currentX, yPos, barX + barW, yPos + barH);
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

            CRect rcFrame(barX, yPos, barX + barW, yPos + barH);
            CBrush brFrame(clrBorder);
            memDC.FrameRect(&rcFrame, &brFrame);
        };

        DrawSegmentedBar(DpiRest(205, this), L"File Count Distribution", false);
        DrawSegmentedBar(DpiRest(245, this), L"Capacity Size Distribution", true);

        // Draw Legends
        const int legendY = DpiRest(270, this);
        const int legendColW = barW / (activeCount > 0 ? activeCount : 1);
        for (int i = 0; i < activeCount; ++i)
        {
            const int legendX = barX + i * legendColW;
            CRect rcColor(legendX, legendY + DpiRest(2, this), legendX + DpiRest(10, this), legendY + DpiRest(12, this));
            memDC.FillSolidRect(&rcColor, activeCards[i].tier->accent);
            CBrush brLegend(clrBorder);
            memDC.FrameRect(&rcColor, &brLegend);

            {
                CSelectObject selectFont(&memDC, &fontBody);
                CSetTextColor setLegendColor(&memDC, fgText);
                memDC.TextOutW(legendX + DpiRest(16, this), legendY, activeCards[i].legendDesc.c_str());
            }
        }

        // Draw Storage Cost Savings Banner
        CRect rcSavings(barX, DpiRest(300, this), barX + barW, DpiRest(385, this));
        const COLORREF bgSavings = isDark ? RGB(32, 50, 36) : RGB(240, 253, 244);
        const COLORREF borderSavings = isDark ? RGB(48, 80, 52) : RGB(187, 247, 208);
        const COLORREF textSavings = isDark ? RGB(74, 222, 128) : RGB(22, 163, 74);

        memDC.FillSolidRect(&rcSavings, bgSavings);
        CBrush brSavings(borderSavings);
        memDC.FrameRect(&rcSavings, &brSavings);

        const int numSavingsColumns = 3;
        const int colW = barW / numSavingsColumns;

        const int labelY = rcSavings.top + DpiRest(15, this);
        DrawText(fontCardLbl, fgMuted, rcSavings.left + DpiRest(15, this), labelY,
            Localization::Lookup(IDS_CURRENT_COST_LABEL));
        DrawText(fontCardVal, fgText, rcSavings.left + DpiRest(15, this),
            rcSavings.top + DpiRest(38, this), std::format(L"${:.2f}/mo", currentCost));
        DrawText(fontCardLbl, fgMuted, rcSavings.left + colW + DpiRest(10, this), labelY,
            Localization::Lookup(IDS_OPTIMIZED_COST_LABEL));
        DrawText(fontCardVal, fgText, rcSavings.left + colW + DpiRest(10, this),
            rcSavings.top + DpiRest(38, this), std::format(L"${:.2f}/mo", optimizedCost));
        DrawText(fontCardLbl, textSavings, rcSavings.left + colW * 2 + DpiRest(10, this), labelY,
            Localization::Lookup(IDS_STORAGE_SAVINGS));
        DrawText(fontTitle, textSavings, rcSavings.left + colW * 2 + DpiRest(10, this),
            rcSavings.top + DpiRest(36, this), std::format(L"${:.2f}/mo ({:.1f}%)", savings, savingsPct));
    }

    pDC->BitBlt(0, 0, clientRect.Width(), clientRect.Height(), &memDC, 0, 0, SRCCOPY);
}
