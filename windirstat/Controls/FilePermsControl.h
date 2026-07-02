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
#include "TreeListControl.h"
#include "ItemPerm.h"

class CFilePermsControl final : public CTreeListControl
{
public:
    CFilePermsControl();
    ~CFilePermsControl() override;

    static CFilePermsControl* Get() { return m_singleton; }

    bool StartScan();
    void StopScan();
    std::vector<const CItemPerm*> GetPermItems() const;

    // Headless parallel scan of the whole tree; used by the silent command-line export
    static std::vector<CItemPerm*> ScanTree(const CItem* docRoot);

protected:
    inline static CFilePermsControl* m_singleton = nullptr;

    // Collect the items to scan and flag those whose every ACE (incl. inherited) should be listed
    static std::vector<const CItem*> BuildScanList(const CItem* docRoot, std::unordered_set<const CItem*>& roots);
    // Scan items in parallel, reporting completion via onItemDone and honoring cancelled
    static std::vector<CItemPerm*> ScanItems(const std::vector<const CItem*>& items, const std::unordered_set<const CItem*>& roots,
        const std::function<bool()>& cancelled, const std::function<void()>& onItemDone);
    // Read one item's DACL and build a row per qualifying ACE; safe to call from worker threads
    static std::vector<CItemPerm*> ScanItem(const CItem* item, bool includeInherited, const std::optional<std::wregex>& excludeRegex);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnDestroy();
};
