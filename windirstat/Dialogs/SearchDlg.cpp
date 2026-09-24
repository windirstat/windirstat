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
#include "SearchDlg.h"
#include "FileSearchControl.h"
#include "FileTabbedView.h"

// SearchDlg dialog

SearchDlg::SearchDlg(CWnd* pParent /*=nullptr*/)
    : MessageTarget(IDD_SEARCH, COptions::SearchWindowRect.Ptr(), pParent)
{
}

// SearchDlg message handlers

bool SearchDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_searchTerm.SubclassDlgItem(IDC_SEARCH_TERM, this);

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());

    ModifyStyle(0, WS_CLIPCHILDREN);

    CClientDC dc(this);
    const GdiObjectSelection font(&dc, GetDlgItem(IDC_SEARCH_SIZE_LABEL)->GetFont());
    const CRect labelRect = GetChildWindowRect(GetDlgItem(IDC_SEARCH_SIZE_LABEL)->Handle());
    LONG labelWidth = 0;
    for (CWnd* child = GetWindow(GW_CHILD); child != nullptr; child = child->GetWindow(GW_HWNDNEXT))
    {
        if (GetChildWindowRect(child->Handle()).right > labelRect.right) continue;
        std::wstring text = child->GetText();
        if (!text.ends_with(L':')) child->SetText(text += L":");
        labelWidth = std::max(labelWidth, dc.GetTextExtent(text).cx);
    }
    const int shift = labelRect.Width() - labelWidth - ScaleForDpi(2);
    for (CWnd* child = GetWindow(GW_CHILD); child != nullptr; child = child->GetWindow(GW_HWNDNEXT))
    {
        CRect rect = GetChildWindowRect(child->Handle());
        if (rect.right <= labelRect.right) rect.right -= shift;
        else rect.Offset(-shift, 0);
        child->MoveWindow(rect);
    }
    const CRect windowRect = GetWindowRect();
    SetWindowPos(nullptr, 0, 0, windowRect.Width() - shift, windowRect.Height(), SWP_NOMOVE | SWP_NOZORDER);

    m_layout.AddControl(IDOK, 1, 1, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 1, 0, 0);
    m_layout.AddControl(IDC_SEARCH_TERM, 0, 0, 1, 0);
    m_layout.AddControl(IDC_SEARCH_WHOLE_PHRASE, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_REGEX, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_CASE, 0, 0, 0, 0);
    for (const auto& [minimum, maximum, separator, units] : {
        std::array{ IDC_SEARCH_SIZE_MIN, IDC_SEARCH_SIZE_MAX, IDC_SEARCH_SIZE_SEPARATOR, IDC_SEARCH_SIZE_UNITS },
        std::array{ IDC_SEARCH_PHYSICAL_MIN, IDC_SEARCH_PHYSICAL_MAX,
            IDC_SEARCH_PHYSICAL_SEPARATOR, IDC_SEARCH_PHYSICAL_UNITS } })
    {
        m_layout.AddControl(minimum, 0, 0, 0, 0);
        m_layout.AddControl(separator, 0, 0, 0, 0);
        m_layout.AddControl(maximum, 0, 0, 0, 0);
        m_layout.AddControl(units, 0, 0, 0, 0);
        GetDlgItem(minimum)->SendNativeMessage(EM_SETCUEBANNER, true, L"0");
        GetDlgItem(maximum)->SendNativeMessage(EM_SETCUEBANNER, true, L"\u221e");
        for (const auto& unit : { GetSpec_Bytes(), GetSpec_KiB(), GetSpec_MiB(), GetSpec_GiB(), GetSpec_TiB() })
            GetDlgItem(units)->SendNativeMessage(CB_ADDSTRING, 0, unit.c_str());
    }
    m_layout.AddControl(IDC_SEARCH_FILES, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_FOLDERS, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_OWNER, 0, 0, 1, 0);

    const CSize minimumSize = GetWindowRect().Size();
    m_minWidth = minimumSize.cx;
    m_fixedHeight = minimumSize.cy;
    m_layout.OnInitDialog(true);

    const size_t historyLimit = static_cast<size_t>(COptions::SearchHistoryCount.Obj());
    const DWORD_PTR defaultFlags = CombineSearchFlags(COptions::SearchRegex.Obj(),
        COptions::SearchWholePhrase.Obj(), COptions::SearchCase.Obj());
    for (const auto line : std::views::split(COptions::SearchHistory.Obj(), L'\n'))
    {
        if (m_searchHistory.size() >= historyLimit) break;
        std::wstring term(std::from_range, line);
        if (term.ends_with(L'\r')) term.pop_back();
        auto flags = defaultFlags;
        if (term.size() >= 2 && term[0] == L'\x1f' && term[1] >= L'0' && term[1] <= L'7')
        {
            flags = term[1] - L'0';
            term.erase(0, 2);
        }
        if (term.empty() || std::ranges::find(m_searchHistory, term) != m_searchHistory.end()) continue;
        m_searchTerm.SetItemData(m_searchTerm.AddString(term), flags);
        m_searchHistory.push_back(std::move(term));
    }
    SaveSearchHistory();
    SetText(IDC_SEARCH_TERM, COptions::SearchTerm.Obj());
    SetChecked(IDC_SEARCH_WHOLE_PHRASE, COptions::SearchWholePhrase);
    SetChecked(IDC_SEARCH_CASE, COptions::SearchCase);
    SetChecked(IDC_SEARCH_REGEX, COptions::SearchRegex);
    SetText(IDC_SEARCH_SIZE_MIN, COptions::SearchSizeMinimum.Obj());
    SetText(IDC_SEARCH_SIZE_MAX, COptions::SearchSizeMaximum.Obj());
    SetComboSelection(IDC_SEARCH_SIZE_UNITS, COptions::SearchSizeUnits);
    SetText(IDC_SEARCH_PHYSICAL_MIN, COptions::SearchPhysicalMinimum.Obj());
    SetText(IDC_SEARCH_PHYSICAL_MAX, COptions::SearchPhysicalMaximum.Obj());
    SetComboSelection(IDC_SEARCH_PHYSICAL_UNITS, COptions::SearchPhysicalUnits);
    SetChecked(IDC_SEARCH_FILES, COptions::SearchIncludeFiles);
    SetChecked(IDC_SEARCH_FOLDERS, COptions::SearchIncludeFolders);
    SetText(IDC_SEARCH_OWNER, COptions::SearchOwner.Obj());

    UpdateControlStatus();
    return true;
}

bool SearchDlg::PreprocessMessage(MSG* pMsg)
{
    if (const auto removed = RemoveSelectedHistoryEntry(pMsg, m_searchTerm, m_searchHistory))
    {
        if (COptions::SearchTerm.Obj() == *removed) COptions::SearchTerm = std::wstring{};
        SaveSearchHistory();
        OnSelectSearchTerm();
        return true;
    }

    return CLayoutDialog::PreprocessMessage(pMsg);
}

bool SearchDlg::ReadCriteria(SearchCriteria& criteria) const
{
    criteria = {
        .term = GetText(IDC_SEARCH_TERM),
        .caseSensitive = IsChecked(IDC_SEARCH_CASE),
        .wholePhrase = IsChecked(IDC_SEARCH_WHOLE_PHRASE),
        .regex = IsChecked(IDC_SEARCH_REGEX),
        .includeFiles = IsChecked(IDC_SEARCH_FILES),
        .includeFolders = IsChecked(IDC_SEARCH_FOLDERS),
        .owner = GetText(IDC_SEARCH_OWNER)
    };

    const auto readBound = [this](const int controlId, const int unitsId, std::optional<ULONGLONG>& bound)
    {
        const int shift = 10 * std::clamp<int>(GetComboSelection(unitsId), 0, 4);
        const ULONGLONG maximum = std::numeric_limits<ULONGLONG>::max() >> shift;
        const std::wstring text = GetText(controlId);
        if (text.empty()) return true;

        ULONGLONG value = 0;
        for (const wchar_t character : text)
        {
            const auto digit = static_cast<ULONGLONG>(character - L'0');
            if (digit > 9 || value > (maximum - digit) / 10) return false;
            value = value * 10 + digit;
        }
        bound = value << shift;
        return true;
    };

    return readBound(IDC_SEARCH_SIZE_MIN, IDC_SEARCH_SIZE_UNITS, criteria.sizeMinimum) &&
        readBound(IDC_SEARCH_SIZE_MAX, IDC_SEARCH_SIZE_UNITS, criteria.sizeMaximum) &&
        readBound(IDC_SEARCH_PHYSICAL_MIN, IDC_SEARCH_PHYSICAL_UNITS, criteria.physicalMinimum) &&
        readBound(IDC_SEARCH_PHYSICAL_MAX, IDC_SEARCH_PHYSICAL_UNITS, criteria.physicalMaximum) &&
        (!criteria.sizeMinimum || !criteria.sizeMaximum || *criteria.sizeMinimum <= *criteria.sizeMaximum) &&
        (!criteria.physicalMinimum || !criteria.physicalMaximum || *criteria.physicalMinimum <= *criteria.physicalMaximum);
}

void SearchDlg::OnBnClickedOk()
{
    SearchCriteria criteria;
    if (!ReadCriteria(criteria) || (!criteria.includeFiles && !criteria.includeFolders) ||
        (CFileSearchControl::ComputeSearchRegex(criteria.term, criteria.caseSensitive,
            criteria.regex).flags() & std::regex_constants::optimize) == 0) return;

    COptions::SearchTerm = criteria.term;
    SaveSearchHistory(&criteria);
    COptions::SearchWholePhrase = criteria.wholePhrase;
    COptions::SearchCase = criteria.caseSensitive;
    COptions::SearchRegex = criteria.regex;
    COptions::SearchSizeMinimum = GetText(IDC_SEARCH_SIZE_MIN);
    COptions::SearchSizeMaximum = GetText(IDC_SEARCH_SIZE_MAX);
    COptions::SearchSizeUnits = std::clamp<int>(GetComboSelection(IDC_SEARCH_SIZE_UNITS), 0, 4);
    COptions::SearchPhysicalMinimum = GetText(IDC_SEARCH_PHYSICAL_MIN);
    COptions::SearchPhysicalMaximum = GetText(IDC_SEARCH_PHYSICAL_MAX);
    COptions::SearchPhysicalUnits = std::clamp<int>(GetComboSelection(IDC_SEARCH_PHYSICAL_UNITS), 0, 4);
    COptions::SearchIncludeFiles = criteria.includeFiles;
    COptions::SearchIncludeFolders = criteria.includeFolders;
    COptions::SearchOwner = criteria.owner;

    CLayoutDialog::OnOK();

    // Process search request
    CFileSearchControl::Get()->ProcessSearch(CWinDirStatModel::Get()->GetRootItem(), criteria);

    // Switch focus to search results
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveSearchView();
}

void SearchDlg::SaveSearchHistory(const SearchCriteria* criteria) const
{
    // Prefix each term with U+001F and a digit: regex = 1, whole phrase = 2, case sensitive = 4.
    const size_t historyLimit = static_cast<size_t>(COptions::SearchHistoryCount.Obj());
    std::vector<std::wstring> history;
    if (criteria != nullptr && !criteria->term.empty() && historyLimit > 0)
    {
        const int flags = CombineSearchFlags(criteria->regex, criteria->wholePhrase, criteria->caseSensitive);
        history.push_back(std::format(L"\x1f{}{}", flags, criteria->term));
    }
    for (int i = 0; i < m_searchTerm.GetCount() && history.size() < historyLimit; ++i)
    {
        const std::wstring term = m_searchTerm.GetItemText(i);
        if (criteria != nullptr && term == criteria->term) continue;
        history.push_back(std::format(L"\x1f{}{}", m_searchTerm.GetItemData(i), term));
    }
    COptions::SearchHistory = JoinString(history, L'\n');
}

void SearchDlg::OnSelectSearchTerm()
{
    const int selection = m_searchTerm.GetCurSel();
    if (selection != CB_ERR)
    {
        const DWORD_PTR flags = m_searchTerm.GetItemData(selection);
        m_searchTerm.SetCurSel(selection);
        SetChecked(IDC_SEARCH_REGEX, (flags & 1) != 0);
        SetChecked(IDC_SEARCH_WHOLE_PHRASE, (flags & 2) != 0);
        SetChecked(IDC_SEARCH_CASE, (flags & 4) != 0);
    }
    UpdateControlStatus();
}

void SearchDlg::OnChangeSearchTerm()
{
    const std::wstring searchTerm = GetText(IDC_SEARCH_TERM);
    const bool searchRegex = IsChecked(IDC_SEARCH_REGEX);

    // Auto-enable whole phrase search if * is present and not in regex mode
    if (!searchRegex && searchTerm.contains(L'*'))
    {
        SetChecked(IDC_SEARCH_WHOLE_PHRASE, true);
    }

    UpdateControlStatus();
}

void SearchDlg::UpdateControlStatus()
{
    SearchCriteria criteria;
    const bool sizesValid = ReadCriteria(criteria);
    const auto regexTest = CFileSearchControl::ComputeSearchRegex(
        criteria.term, criteria.caseSensitive, criteria.regex);

    // Excluding both files and folders would ask for nothing at all, so refuse that
    // combination rather than running a search that cannot return anything
    GetDlgItem(IDOK)->EnableWindow(sizesValid && (criteria.includeFiles || criteria.includeFolders) &&
        (regexTest.flags() & std::regex_constants::optimize) != 0);
}

HBRUSH SearchDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void SearchDlg::OnGetMinMaxInfo(MINMAXINFO* pMMI)
{
    if (pMMI != nullptr && m_fixedHeight > 0)
    {
        pMMI->ptMinTrackSize.x = m_minWidth;
        pMMI->ptMinTrackSize.y = m_fixedHeight;
        pMMI->ptMaxTrackSize.y = m_fixedHeight;
    }
}
