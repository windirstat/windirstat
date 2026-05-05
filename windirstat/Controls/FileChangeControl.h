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
#include "ItemChange.h"
#include "TreeListControl.h"

class CFileChangeControl final : public CTreeListControl
{
public:
    CFileChangeControl();
    ~CFileChangeControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileChangeControl* Get() { return m_singleton; }
    CItemChange* GetRootItem() const { return m_rootItem; }
    void SetChanges(const SnapshotGrowthResult& result);
    void ClearChanges();
    bool HasChanges() const { return m_hasChanges; }
    void AfterDeleteAllItems() override;

protected:
    static CFileChangeControl* m_singleton;
    CItemChange* m_rootItem = nullptr;
    bool m_hasChanges = false;

    DECLARE_MESSAGE_MAP()
};
