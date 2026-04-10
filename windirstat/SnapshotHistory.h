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

struct SnapshotGrowthEntry
{
    std::wstring path;
    ITEMTYPE type = IT_NONE;
    CItem* currentItem = nullptr;
    ULONGLONG currentSizePhysical = 0;
    ULONGLONG previousSizePhysical = 0;
    LONGLONG deltaSizePhysical = 0;
    LONGLONG deltaFiles = 0;
    LONGLONG deltaFolders = 0;
};

struct SnapshotGrowthResult
{
    std::wstring previousSnapshotLabel;
    std::vector<SnapshotGrowthEntry> entries;
};

SnapshotGrowthResult UpdateSnapshotHistory(const std::wstring& rootSpec, CItem* rootItem);
SnapshotGrowthResult CompareSnapshotTrees(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotLabel, CItem* previousRootItem);
SnapshotGrowthResult CompareSnapshotFileToCurrent(const std::wstring& currentRootSpec, CItem* currentRootItem,
    const std::wstring& previousSnapshotPath);
