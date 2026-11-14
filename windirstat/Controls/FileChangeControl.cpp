// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FileChangeControl.h"

std::wstring CChangeItem::FormatValue(const ULONGLONG value) const
{
    return m_count ? FormatCount(value) : FormatSizeSuffixes(value);
}

std::wstring CChangeItem::GetText(const int subitem) const
{
    switch (subitem)
    {
    case COL_ITEMCHANGE_NAME:     return m_path;
    case COL_ITEMCHANGE_PREVIOUS: return FormatValue(m_previous);
    case COL_ITEMCHANGE_CURRENT:  return FormatValue(m_current);
    case COL_ITEMCHANGE_DELTA:
        if (m_current == m_previous) return FormatValue(0);
        return (m_current > m_previous ? L"+" : L"-") + FormatValue(Magnitude());
    case COL_ITEMCHANGE_PERCENT:
        if (m_previous == 0) return {};
        return std::wstring(m_current >= m_previous ? L"+" : L"-")
            + FormatDouble(100.0 * static_cast<double>(Magnitude()) / static_cast<double>(m_previous)) + L" %";
    default: return {};
    }
}

int CChangeItem::CompareSibling(const CTreeListItem* other, const int subitem) const
{
    const auto* o = static_cast<const CChangeItem*>(other);
    switch (subitem)
    {
    case COL_ITEMCHANGE_NAME:     return signum(_wcsicmp(m_path.c_str(), o->m_path.c_str()));
    case COL_ITEMCHANGE_PREVIOUS: return usignum(m_previous, o->m_previous);
    case COL_ITEMCHANGE_CURRENT:  return usignum(m_current, o->m_current);
    case COL_ITEMCHANGE_DELTA:    return usignum(Magnitude(), o->Magnitude());   // ranks by size of the change
    case COL_ITEMCHANGE_BAR:
    case COL_ITEMCHANGE_PERCENT:
    {
        // A folder that was empty before has no percentage and ranks as the largest relative change.
        const double a = Ratio(), b = o->Ratio();
        return a < b ? -1 : a == b ? 0 : 1;
    }
    default: return 0;
    }
}

double CChangeItem::Ratio() const
{
    return m_previous == 0 ? HUGE_VAL : static_cast<double>(Magnitude()) / static_cast<double>(m_previous);
}

bool CChangeItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem != COL_ITEMCHANGE_BAR) return CTreeListItem::DrawSubItem(subitem, pdc, rc, state, width, focusLeft);

    if (width != nullptr)
    {
        *width = ScaleForDpi(100);
        return true;
    }

    const auto* viewState = GetViewState();
    if (viewState == nullptr || viewState->control == nullptr) return true;
    DrawSelection(viewState->control, pdc, rc, state);
    rc.Deflate(3, 5);
    if (rc.Width() <= 0 || rc.Height() <= 0) return true;

    // Growth is drawn red, shrinkage green; the longest finite change fills the bar.
    const double ratio = Ratio();
    const double fraction = std::isinf(ratio) || m_barScale <= 0.0 ? 1.0 : std::min(1.0, ratio / m_barScale);
    DrawPercentage(pdc, rc, fraction, m_current >= m_previous ? RGB(190, 80, 80) : RGB(80, 160, 100));
    return true;
}

CItem* CChangeItem::GetLinkedItem()
{
    // Materialize an action target only for rows used by selection or shell commands.
    if (m_item == nullptr)
        m_item.reset(CItem::Create(IT_DIRECTORY, m_path, FILETIME{}, 0, 0, 0, FILE_ATTRIBUTE_DIRECTORY, 0, 0));
    return m_item.get();
}

HICON CChangeItem::GetIcon()
{
    auto* viewState = GetViewState();

    // No icon to return if not visible yet
    if (viewState == nullptr) return nullptr;
    if (viewState->icon != nullptr) return viewState->icon;

    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        viewState->control, m_path, static_cast<DWORD>(FILE_ATTRIBUTE_DIRECTORY), &viewState->icon, nullptr));
    return nullptr;
}

void CFileChangeControl::ReleaseRows()
{
    for (const auto& row : m_rows) if (row->IsVisible()) row->SetVisible(this, false);
    m_rows.clear();
}

void CFileChangeControl::AfterDeleteAllItems()
{
    ReleaseRows();
}

namespace
{
    std::wstring FormatScanTime(const std::int64_t seconds)
    {
        const auto ticks = (static_cast<ULONGLONG>(seconds) + 11644473600ULL) * 10'000'000ULL;
        return FormatFileTime(std::bit_cast<FILETIME>(ticks), true);
    }

    // Scans a comparison can start from: every stored scan except the newest, newest first.
    std::vector<std::int64_t> GetBaselineCandidates(const std::wstring& origin)
    {
        auto times = CScanHistory::GetScanTimes(origin);
        if (!times.empty()) times.pop_back();
        std::ranges::reverse(times);
        return times;
    }
}

bool CFileChangeControl::OnHeaderClick(const int subitem)
{
    if (subitem != COL_ITEMCHANGE_PREVIOUS) return false;

    const auto* model = CWinDirStatModel::Get();
    if (!model->HasRootItem()) return true;
    const auto times = GetBaselineCandidates(model->GetScanPathSpec());
    if (times.empty()) return true;

    CMenu menu = CMenu::CreatePopup();
    if (!menu) return true;
    for (std::size_t i = 0; i < times.size(); ++i)
    {
        const bool selected = m_baseline == 0 ? i == 0 : times[i] == m_baseline;
        menu.Append(MF_STRING | (selected ? MF_CHECKED : MF_UNCHECKED), static_cast<UINT>(i + 1),
            FormatScanTime(times[i]).c_str());
    }

    CPoint point;
    GetCursorPos(&point);
    const UINT command = menu.ShowPopup(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point, this);
    if (command == 0 || command > times.size()) return true;

    m_baseline = times[command - 1];
    Refresh();
    return true;
}

namespace
{
    std::wstring DescribeScan(const std::int64_t time, const HistoryPoint* point)
    {
        std::wstring text = FormatScanTime(time);
        if (point != nullptr)
        {
            text += L"  ·  " + FormatBytes(point->size) + L"  ·  " + Localization::Lookup(IDS_HISTORYCHART_FILES)
                + L": " + FormatCount(point->files);
        }
        return text;
    }

    const HistoryPoint* FindRootPoint(const std::int64_t time)
    {
        const auto* model = CWinDirStatModel::Get();
        if (!model->HasRootItem()) return nullptr;
        const auto* points = CScanHistory::Find(model->GetScanPathSpec(), model->GetRootItem()->GetPath());
        if (points == nullptr) return nullptr;
        const auto found = std::ranges::find(*points, time, &HistoryPoint::time);
        return found == points->end() ? nullptr : &*found;
    }
}

std::vector<CFileChangeControl::ScanChoice> CFileChangeControl::GetScanChoices() const
{
    std::vector<ScanChoice> choices;
    const auto* model = CWinDirStatModel::Get();
    if (!model->HasRootItem()) return choices;
    for (const std::int64_t time : GetBaselineCandidates(model->GetScanPathSpec()))
        choices.push_back({ time, DescribeScan(time, FindRootPoint(time)) });
    return choices;
}

std::wstring CFileChangeControl::GetLatestText() const
{
    const auto* model = CWinDirStatModel::Get();
    if (!model->HasRootItem()) return {};
    const auto times = CScanHistory::GetScanTimes(model->GetScanPathSpec());
    if (times.empty()) return {};
    return Localization::Lookup(IDS_CHANGES_LATEST) + L" " + DescribeScan(times.back(), FindRootPoint(times.back()));
}

void CFileChangeControl::UpdatePreviousHeader(const std::vector<std::int64_t>& times)
{
    // Show which scan the "Previous" column refers to.
    const int column = SubItemToColumn(COL_ITEMCHANGE_PREVIOUS);
    if (column < 0) return;

    std::wstring text = Localization::Lookup(IDS_COL_PREVIOUS);
    if (!times.empty())
        text += L" (" + FormatScanTime(m_baseline == 0 ? times.front() : m_baseline) + L")";
    LVCOLUMN info{ .mask = LVCF_TEXT, .pszText = text.data() };
    SetColumn(column, &info);
}

void CFileChangeControl::Refresh()
{
    const ScopedRedrawPause lock(this);
    DeleteAllItems();
    ReleaseRows();

    const auto* model = CWinDirStatModel::Get();
    std::vector<std::int64_t> times;
    if (model->HasRootItem())
    {
        const std::wstring& origin = model->GetScanPathSpec();
        times = GetBaselineCandidates(origin);

        // A remembered scan may be gone (other origin, history purged).
        if (m_baseline != 0 && std::ranges::find(times, m_baseline) == times.end()) m_baseline = 0;

        for (const auto& change : CScanHistory::GetChanges(origin, m_baseline))
        {
            const ULONGLONG previous = m_count ? change.previous.files : change.previous.size;
            const ULONGLONG current = m_count ? change.current.files : change.current.size;
            if (previous == current) continue;
            m_rows.push_back(std::make_unique<CChangeItem>(change.path, previous, current, m_count));
        }
    }

    double maxRatio = 0.0;
    for (const auto& row : m_rows) if (!std::isinf(row->Ratio())) maxRatio = std::max(maxRatio, row->Ratio());
    for (const auto& row : m_rows) row->SetBarScale(maxRatio);

    std::vector<CWdsListItem*> items;
    items.reserve(m_rows.size());
    for (const auto& row : m_rows)
    {
        row->SetVisible(this, true);
        items.push_back(row.get());
    }
    InsertSortedListItems(items);
    UpdatePreviousHeader(times);
    Invalidate();
    if (m_refreshed) m_refreshed();
}
