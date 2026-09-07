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
#include "FilePermsControl.h"
#include "ProgressDlg.h"

CFilePermsControl::CFilePermsControl()
    : MessageTarget(COptions::PermsViewColumnOrder.Ptr(), COptions::PermsViewColumnWidths.Ptr(), COptions::PermsViewColumnVisibility.Ptr(), LF_PERMSLIST, false)
{
    SetOwnsItems(true);
    m_singleton = this;
}

CFilePermsControl::~CFilePermsControl()
{
    m_singleton = nullptr;
}

void CFilePermsControl::OnDestroy()
{
    DeleteAllItems();

    CWdsListControl::OnDestroy();
}

std::vector<CItemPerm*> CFilePermsControl::ScanItem(const CItem* item, const bool includeInherited, const std::optional<std::wregex>& excludeRegex)
{
    static GENERIC_MAPPING fileMapping =
        { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };

    // Fetch the discretionary access control list for this item
    std::vector<CItemPerm*> rows;
    SmartPointer sd(LocalFree, static_cast<PSECURITY_DESCRIPTOR>(nullptr));
    PACL dacl = nullptr;
    if (GetNamedSecurityInfo(item->GetPathLong().c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &dacl, nullptr, &sd) != ERROR_SUCCESS || dacl == nullptr) return rows;

    // A protected DACL does not inherit from its parent (broken/disabled inheritance)
    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    GetSecurityDescriptorControl(sd, &control, &revision);
    const bool inheritanceDisabled = (control & SE_DACL_PROTECTED) != 0;

    // Build a row for each qualifying entry (root items also list inherited entries)
    for (const auto a : std::views::iota(0u, static_cast<unsigned>(dacl->AceCount)))
    {
        ACE_HEADER* header = nullptr;
        if (GetAce(dacl, a, reinterpret_cast<LPVOID*>(&header)) == 0) continue;
        if (!includeInherited && (header->AceFlags & INHERITED_ACE) != 0) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE && header->AceType != ACCESS_DENIED_ACE_TYPE) continue;

        const auto ace = reinterpret_cast<ACCESS_ALLOWED_ACE*>(header);
        std::wstring account = GetNameFromSid(&ace->SidStart);
        if (excludeRegex.has_value() && std::regex_search(account, *excludeRegex)) continue;

        ACCESS_MASK mask = ace->Mask;
        MapGenericMask(&mask, &fileMapping);
        rows.push_back(new CItemPerm(item->GetPath(), item->GetAttributes(), std::move(account), mask,
            header->AceType == ACCESS_DENIED_ACE_TYPE, header->AceFlags, inheritanceDisabled));
    }
    return rows;
}

std::vector<const CItem*> CFilePermsControl::BuildScanList(const CItem* docRoot, std::unordered_set<const CItem*>& roots)
{
    // Root-level items list every permission; deeper items only their explicit (non-inherited) ones
    // Exclude shell-only roots because security descriptor queries require filesystem paths
    if (docRoot->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        for (const CItem* child : docRoot->GetChildren())
            if (child->SupportsFilesystemApis()) roots.insert(child);
    }
    else if (docRoot->SupportsFilesystemApis()) roots.insert(docRoot);

    // Snapshot every real-path item so worker threads never touch the live tree
    std::vector<const CItem*> items;
    std::vector stack{ docRoot };
    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();
        if (!item->SupportsFilesystemApis()) continue;
        if (!item->IsLeaf()) for (const CItem* child : item->GetChildren()) stack.push_back(child);
        if (item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY, IT_FILE) && !item->IsTypeOrFlag(ITF_RESERVED)) items.push_back(item);
    }
    return items;
}

std::vector<CItemPerm*> CFilePermsControl::ScanItems(const std::vector<const CItem*>& items, const std::unordered_set<const CItem*>& roots,
    const std::function<bool()>& cancelled, const std::function<void()>& onItemDone)
{
    // Compile the optional account exclusion expression (partial, case-insensitive match)
    std::optional<std::wregex> excludeRegex;
    if (const std::wstring& pattern = COptions::PermsExcludeRegex.Obj(); !pattern.empty())
        try { excludeRegex.emplace(pattern, std::regex::icase | std::regex::optimize); } catch (const std::regex_error&) {}

    // Pull items off a shared cursor across worker threads, joined before returning
    std::vector<CItemPerm*> results;
    {
        std::atomic<size_t> cursor = 0;
        std::mutex resultsMutex;
        std::vector<std::jthread> workers;
        for (int t = 0, n = std::clamp<int>(COptions::ScanningThreads, 1, 16); t < n; t++)
        {
            workers.emplace_back([&]
            {
                for (size_t i = cursor.fetch_add(1); i < items.size() && !cancelled(); i = cursor.fetch_add(1))
                {
                    if (auto rows = ScanItem(items[i], roots.contains(items[i]), excludeRegex); !rows.empty())
                    {
                        std::scoped_lock lock(resultsMutex);
                        results.insert(results.end(), rows.begin(), rows.end());
                    }
                    onItemDone();
                }
            });
        }
    }
    return results;
}

std::vector<CItemPerm*> CFilePermsControl::ScanTree(const CItem* docRoot)
{
    std::unordered_set<const CItem*> roots;
    const auto items = BuildScanList(docRoot, roots);
    return ScanItems(items, roots, [] { return false; }, [] {});
}

bool CFilePermsControl::StartScan()
{
    DeleteAllItems();

    const auto* model = CWinDirStatModel::Get();
    if (!model->IsScanSettled()) return false;

    std::unordered_set<const CItem*> roots;
    const auto items = BuildScanList(model->GetRootItem(), roots);

    // Scan in parallel under a modal progress dialog so large trees show progress
    std::vector<CItemPerm*> rows;
    CProgressDlg progress(items.size(), CProgressDlg::Flags::None, CMainFrame::Get(), [&](CProgressDlg* pdlg)
    {
        rows = ScanItems(items, roots, [pdlg] { return pdlg->IsCancelled(); }, [pdlg] { pdlg->Increment(); });
    });
    progress.ShowModal();

    // Discard results if the user cancelled; the caller hides the tab in that case
    if (progress.WasCancelled())
    {
        for (const auto* r : rows) delete r;
        return false;
    }

    // Publish on the UI thread (an empty result still counts as a completed scan)
    if (!rows.empty())
    {
        for (auto* r : rows) r->SetVisible(this, true);
        const std::vector<CWdsListItem*> listItems(rows.begin(), rows.end());
        const ScopedRedrawPause lock(this);
        InsertListItem(GetItemCount(), listItems);
        SortItems();
    }
    return true;
}

std::vector<const CItemPerm*> CFilePermsControl::GetPermItems() const
{
    std::vector<const CItemPerm*> items;
    for (const int i : std::views::iota(0, GetItemCount()))
    {
        items.push_back(static_cast<const CItemPerm*>(GetItem(i)));
    }
    return items;
}
