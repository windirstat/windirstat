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
#include "SelectObject.h"

/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(CExtensionListControl* list, const std::wstring& extension, const SExtensionRecord& r)
    : m_driveList(list)
    , m_extension(extension)
    , m_bytes(r.bytes)
    , m_files(r.files)
    , m_color(r.color)
{
}

bool CExtensionListControl::CListItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem == COL_EXT_EXTENSION)
    {
        DrawLabel(m_driveList, pdc, rc, state, width, focusLeft);
    }
    else if (subitem == COL_EXT_COLOR)
    {
        DrawColor(pdc, rc, state, width);
    }
    else
    {
        return false;
    }

    return true;
}

void CExtensionListControl::CListItem::DrawColor(CDC* pdc, CRect rc, const UINT state, int* width) const
{
    if (width != nullptr)
    {
        *width = 40;
        return;
    }

    DrawSelection(m_driveList, pdc, rc, state);

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

std::wstring CExtensionListControl::CListItem::GetExtension() const
{
    return m_extension;
}

HICON CExtensionListControl::CListItem::GetIcon()
{
    if (m_icon != nullptr) return m_icon;

    GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(const_cast<CListItem*>(this),
        m_driveList, m_extension, FILE_ATTRIBUTE_NORMAL, &m_icon, &m_description));

    return m_icon;
}

std::wstring CExtensionListControl::CListItem::GetDescription() const
{
    return m_icon == nullptr ? L"" : m_description;
}

std::wstring CExtensionListControl::CListItem::GetBytesPercent() const
{
    return FormatDouble(GetBytesFraction() * 100) + L"%";
}

double CExtensionListControl::CListItem::GetBytesFraction() const
{
    if (m_driveList->GetRootSize() == 0)
    {
        return 0;
    }

    return static_cast<double>(m_bytes) /
        static_cast<double>(m_driveList->GetRootSize());
}

int CExtensionListControl::CListItem::Compare(const COwnerDrawnListItem* baseOther, const int subitem) const
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

BEGIN_MESSAGE_MAP(CExtensionListControl, COwnerDrawnListControl)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteItem)
    ON_WM_SETFOCUS()
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnLvnItemChanged)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_EXTLIST_SEARCH_EXTENSION, &CExtensionListControl::OnSearchExtension)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CExtensionListControl::CExtensionListControl(CExtensionView* extensionView)
    : COwnerDrawnListControl(COptions::ExtViewColumnOrder.Ptr(), COptions::ExtViewColumnWidths.Ptr())
    , m_extensionView(extensionView) {}

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
    // Cleanup visual nodes
    SetRedraw(FALSE);
    DeleteAllItems();

    // Insert new items
    if (ed != nullptr)
    {
        int i = 0;
        for (const auto& [ext, rec] : *ed)
        {
            InsertListItem(i++, new CListItem(this, ext, rec));
        }
    }

    SetRedraw(TRUE);
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
    for (const int i : std::views::iota(0, GetItemCount()))
    {
        if (_wcsicmp(GetListItem(i)->GetExtension().c_str(), ext.c_str()) == 0)
        {
            SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            EnsureVisible(i, false);
            return;
        }
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
    return reinterpret_cast<CListItem*>(GetItemData(i));
}

void CExtensionListControl::OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto lv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete reinterpret_cast<CListItem*>(lv->lParam);
    *pResult = FALSE;
}

void CExtensionListControl::OnSetFocus(CWnd* pOldWnd)
{
    COwnerDrawnListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_EXTLIST);
}

void CExtensionListControl::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNMLV->uNewState & LVIS_SELECTED) != 0)
    {
        m_extensionView->SetHighlightExtension(GetSelectedExtension());
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

    COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CExtensionListControl::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_EXTLIST_SEARCH_EXTENSION, std::format(
        L"{} - {}", Localization::Lookup(IDS_COL_EXTENSION), Localization::Lookup(IDS_SEARCH_TITLE)).c_str());

    // Add search bitmap to menu
    if (m_searchBitmap.GetSafeHandle() == NULL)
    {
        m_searchBitmap.Attach(LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_SEARCH), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        DarkMode::LightenBitmap(&m_searchBitmap);
    }

    menu.SetMenuItemBitmaps(ID_EXTLIST_SEARCH_EXTENSION, MF_BYCOMMAND, &m_searchBitmap, &m_searchBitmap);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CExtensionListControl::OnSearchExtension()
{
    const auto searchTerm = GetSelectedExtension().empty() ? std::wstring(LR"(^[^\.]+$)") :
        (GlobToRegex(GetSelectedExtension(), false) + L"$");
    CFileSearchControl::Get()->ProcessSearch(CDirStatDoc::Get()->GetRootItem(),
        searchTerm, false, false, true, true);

    CMainFrame::Get()->GetFileTabbedView()->SetActiveSearchView();
}
