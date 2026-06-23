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
#include "ExtensionView.h"
#include "ExtensionListControl.h"
#include "FileSearchControl.h"
#include "Filtering.h"
#include "Options.h"

/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(CExtensionListControl* list, std::wstring extension, const SExtensionRecord& r, const bool aggregate)
    : m_extension(std::move(extension))
    , m_extensionList(list)
    , m_bytes(r.GetBytes())
    , m_files(r.GetFiles())
    , m_color(r.color)
    , m_aggregate(aggregate)
{
    // The aggregate row has no shell type, so give it a fixed description
    if (aggregate) m_description = Localization::Lookup(IDS_EXTENSION_GROUPED);
}

bool CExtensionListControl::CListItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    switch (subitem)
    {
    case COL_EXT_EXTENSION:
        DrawLabel(m_extensionList, pdc, rc, state, width, focusLeft);
        return true;
    case COL_EXT_COLOR:
        DrawColor(pdc, rc, state, width);
        return true;
    default:
        return false;
    }
}

void CExtensionListControl::CListItem::DrawColor(CDC* pdc, CRect rc, const UINT state, int* width) const
{
    if (width != nullptr)
    {
        *width = 40;
        return;
    }

    DrawSelection(m_extensionList, pdc, rc, state);

    rc.DeflateRect(2, 3);

    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    CTreeMap treemap;
    treemap.DrawColorPreview(pdc, rc, m_color, &COptions::TreeMapOptions);
}

std::wstring CExtensionListControl::CListItem::GetText(const int subitem) const
{
    switch (subitem)
    {
        case COL_EXT_EXTENSION: return GetExtension();
        case COL_EXT_COLOR: return {};
        case COL_EXT_BYTES: return FormatBytes(m_bytes);
        case COL_EXT_FILES: return FormatCount(m_files);
        case COL_EXT_DESCRIPTION: return GetDescription();
        case COL_EXT_BYTESPERCENT: return GetBytesPercent();
        default: ASSERT(FALSE); return {};
    }
}

const std::wstring& CExtensionListControl::CListItem::GetExtension() const
{
    return m_extension;
}

HICON CExtensionListControl::CListItem::GetIcon()
{
    if (m_icon != nullptr) return m_icon;

    // The aggregate row covers many extensions, so use a generic icon and skip the shell lookup
    if (m_aggregate)
    {
        m_icon = GetIconHandler()->GetUnknownImage();
        return m_icon;
    }

    GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(const_cast<CListItem*>(this),
        m_extensionList, m_extension, FILE_ATTRIBUTE_NORMAL, &m_icon, &m_description));

    return m_icon;
}

const std::wstring& CExtensionListControl::CListItem::GetDescription() const
{
    static const std::wstring empty;
    return (m_aggregate || m_icon != nullptr) ? m_description : empty;
}

std::wstring CExtensionListControl::CListItem::GetBytesPercent() const
{
    return FormatDouble(GetBytesFraction() * 100) + L"%";
}

double CExtensionListControl::CListItem::GetBytesFraction() const
{
    if (m_extensionList->GetRootSize() == 0)
    {
        return 0;
    }

    return static_cast<double>(m_bytes) /
        static_cast<double>(m_extensionList->GetRootSize());
}

int CExtensionListControl::CListItem::Compare(const CWdsListItem* baseOther, const int subitem) const
{
    const auto other = static_cast<const CListItem*>(baseOther);

    switch (subitem)
    {
        case COL_EXT_COLOR:
        case COL_EXT_BYTES: return usignum(m_bytes, other->m_bytes);
        case COL_EXT_EXTENSION: return signum(_wcsicmp(GetExtension().c_str(),other->GetExtension().c_str()));
        case COL_EXT_FILES: return usignum(m_files, other->m_files);
        case COL_EXT_DESCRIPTION: return signum(_wcsicmp(GetDescription().c_str(), other->GetDescription().c_str()));
        case COL_EXT_BYTESPERCENT: return signum(GetBytesFraction() - other->GetBytesFraction());
        default: ASSERT(FALSE); return 0;
    }
}

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CExtensionListControl, CWdsListControl)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteItem)
    ON_WM_SETFOCUS()
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnLvnItemChanged)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_EXTLIST_SEARCH_EXTENSION, &CExtensionListControl::OnSearchExtension)
    ON_COMMAND(ID_FILTER_EXCLUDE_ITEM, &CExtensionListControl::OnExcludeExtension)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CExtensionListControl::CExtensionListControl(CExtensionView* extensionView)
    : CWdsListControl(COptions::ExtViewColumnOrder.Ptr(), COptions::ExtViewColumnWidths.Ptr())
    , m_extensionView(extensionView)
{
    SetOwnsItems(true);
}

bool CExtensionListControl::GetAscendingDefault(const int subitem)
{
    switch (subitem)
    {
        case COL_EXT_EXTENSION:
        case COL_EXT_BYTESPERCENT:
        case COL_EXT_DESCRIPTION: return true;
        case COL_EXT_COLOR:
        case COL_EXT_BYTES:
        case COL_EXT_FILES: return false;
        default: ASSERT(FALSE); return true;
    }
}

// As we will not receive WM_CREATE, we must do initialization
// in this extra method. The counterpart is OnDestroy().
void CExtensionListControl::Initialize()
{
    // Columns should be in the order of definition in order for sort to work
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_EXTENSION).c_str(), LVCFMT_LEFT, DpiRest(60), COL_EXT_EXTENSION);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_COLOR).c_str(), LVCFMT_LEFT, DpiRest(40), COL_EXT_COLOR);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_DESCRIPTION).c_str(), LVCFMT_LEFT, DpiRest(170), COL_EXT_DESCRIPTION);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_BYTES).c_str(), LVCFMT_RIGHT, DpiRest(60), COL_EXT_BYTES);
    InsertColumn(CHAR_MAX, (L"% " + Localization::Lookup(IDS_COL_BYTES)).c_str(), LVCFMT_RIGHT, DpiRest(50), COL_EXT_BYTESPERCENT);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FILES).c_str(), LVCFMT_RIGHT, DpiRest(50), COL_EXT_FILES);

    SetSorting(COL_EXT_BYTES, GetAscendingDefault(COL_EXT_BYTES));

    OnColumnsInserted();
}

void CExtensionListControl::SetExtensionData(const CExtensionData* ed)
{
    // Replace all existing rows
    const CSetRedrawLock lock(this);
    DeleteAllItems();

    if (ed != nullptr)
    {
        const bool group = COptions::GroupUnregisteredTypes;

        // Running totals for the single "unregistered types" row
        ULONGLONG groupBytes = 0, groupFiles = 0, largestBytes = 0;
        COLORREF groupColor = 0;

        std::vector<CWdsListItem*> items;
        items.reserve(ed->size());
        for (const auto& [ext, rec] : *ed)
        {
            // Fold unregistered extensions together; "no extension" (empty key) stays separate
            if (group && !ext.empty() && !CDirStatDoc::Get()->IsExtensionRegistered(ext))
            {
                const ULONGLONG bytes = rec.GetBytes();
                groupBytes += bytes;
                groupFiles += rec.GetFiles();

                // Show the group with its largest contributor's color
                if (bytes >= largestBytes) { largestBytes = bytes; groupColor = rec.color; }
                continue;
            }

            items.emplace_back(new CListItem(this, ext, rec));
        }

        // Append the aggregate row only if any unregistered extensions were folded in
        if (groupFiles > 0)
        {
            SExtensionRecord r;
            r.bytes = groupBytes;
            r.files = groupFiles;
            r.color = groupColor;
            items.emplace_back(new CListItem(this, L"⋯", r, true));
        }

        InsertListItem(0, items);
    }

    SortItems();
}

void CExtensionListControl::SetRootSize(const ULONGLONG totalBytes)
{
    m_rootSize = totalBytes;
}

ULONGLONG CExtensionListControl::GetRootSize() const
{
    return m_rootSize;
}

void CExtensionListControl::SelectExtension(const std::wstring & ext)
{
    auto view = std::views::iota(0, GetItemCount());
    auto it = std::ranges::find_if(view, [&](int i) {
        const CListItem* item = GetListItem(i);
        if (item->IsAggregate())
        {
            return !ext.empty() && !CDirStatDoc::Get()->IsExtensionRegistered(ext);
        }
        return _wcsicmp(item->GetExtension().c_str(), ext.c_str()) == 0;
    });

    if (it != view.end())
    {
        SetItemState(*it, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        EnsureVisible(*it, FALSE);
    }
}

std::wstring CExtensionListControl::GetSelectedExtension() const
{
    POSITION pos = GetFirstSelectedItemPosition();
    if (pos == nullptr)
    {
        return wds::strEmpty;
    }

    const int i = GetNextSelectedItem(pos);
    const CListItem* item = GetListItem(i);
    return item->GetExtension();
}

CExtensionListControl::CListItem* CExtensionListControl::GetListItem(const int i) const
{
    return static_cast<CListItem*>(GetItem(i));
}

bool CExtensionListControl::IsSelectedAggregate() const
{
    // The synthetic aggregate row has no concrete extension to act on
    POSITION pos = GetFirstSelectedItemPosition();
    return pos != nullptr && GetListItem(GetNextSelectedItem(pos))->IsAggregate();
}

void CExtensionListControl::OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto lv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete std::bit_cast<CListItem*>(lv->lParam);
    *pResult = FALSE;
}

void CExtensionListControl::OnSetFocus(CWnd* pOldWnd)
{
    CWdsListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_EXTLIST);
}

void CExtensionListControl::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNMLV->uNewState & LVIS_SELECTED) != 0)
    {
        // The aggregate row highlights every unregistered extension at once
        m_extensionView->SetHighlightExtension(GetSelectedExtension(), IsSelectedAggregate());
    }
    *pResult = FALSE;
}

void CExtensionListControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        if (!IsShiftKeyDown())
        {
            CMainFrame::Get()->MoveFocus(LF_FILETREE);
        }
        else
        {
            auto* tabbedView = CMainFrame::Get()->GetFileTabbedView();
            if (tabbedView->IsSearchTabVisible())
            {
                tabbedView->SetActiveSearchView();
                CMainFrame::Get()->MoveFocus(LF_SEARCHLIST);
            }
            else if (tabbedView->IsDupeTabVisible())
            {
                tabbedView->SetActiveDupeView();
                CMainFrame::Get()->MoveFocus(LF_DUPELIST);
            }
            else
            {
                tabbedView->SetActiveTopView();
                CMainFrame::Get()->MoveFocus(LF_TOPLIST);
            }
        }
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }

    CWdsListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CExtensionListControl::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    if (point == CPoint(-1, -1))
    {
        const int i = GetNextItem(-1, LVNI_FOCUSED);
        if (i == -1) return;

        CRect rc;
        if (!GetItemRect(i, rc, LVIR_LABEL)) return;

        point = CPoint(rc.left, rc.bottom);
        ClientToScreen(&point);
    }

    // Searching and excluding need a concrete extension; disable them on the aggregate row
    const UINT aggregateFlags = IsSelectedAggregate() ? MF_GRAYED : 0;

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING | aggregateFlags, ID_EXTLIST_SEARCH_EXTENSION, std::format(
        L"{} - {}", Localization::Lookup(IDS_COL_EXTENSION), Localization::Lookup(IDS_SEARCH_TITLE)).c_str());
    menu.AppendMenu(MF_STRING | aggregateFlags, ID_FILTER_EXCLUDE_ITEM, Localization::Lookup(IDS_MENU_EXCLUDE_ITEM).c_str());

    // Add search bitmap to menu
    if (m_searchBitmap.GetSafeHandle() == nullptr)
    {
        m_searchBitmap.Attach(Icons::MakeBitmap(16, Icons::Char(L'⌕', RGB(140, 140, 140))));
    }

    MENUITEMINFO mii{
        .cbSize = sizeof(MENUITEMINFO),
        .fMask = MIIM_BITMAP,
        .hbmpItem = static_cast<HBITMAP>(m_searchBitmap.GetSafeHandle())
    };
    SetMenuItemInfo(menu.GetSafeHmenu(), ID_EXTLIST_SEARCH_EXTENSION, FALSE, &mii);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CExtensionListControl::OnSearchExtension()
{
    if (IsSelectedAggregate()) return;

    const auto searchTerm = GetSelectedExtension().empty() ? std::wstring(LR"(^[^\.]+$)") :
        (GlobToRegex(GetSelectedExtension(), false) + L"$");
    CFileSearchControl::Get()->ProcessSearch(CDirStatDoc::Get()->GetRootItem(),
        searchTerm, false, false, true, true);

    CMainFrame::Get()->GetFileTabbedView()->SetActiveSearchView();
}

void CExtensionListControl::OnExcludeExtension()
{
    if (IsSelectedAggregate()) return;

    const std::wstring ext = GetSelectedExtension();
    if (ext.empty()) return;

    // Build a glob pattern: e.g. "*.txt"; for no-extension files use "*."
    const std::wstring pattern = (ext == L".") ? L"*." : (L"*" + ext);

    std::wstring& current = COptions::FilteringExcludeFiles.Obj();
    if (!current.empty() && current.back() != L'\n') current += L"\r\n";
    current += pattern;

    CFiltering::CompileFilters();
    AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_REFRESH_ALL);
}
