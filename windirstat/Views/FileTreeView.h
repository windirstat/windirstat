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
#include "FileWatcherControl.h"
#include "FilePermsControl.h"
#include "FileTopControl.h"
#include "FileDupeControl.h"
#include "FileSearchControl.h"
#include "FileTreeControl.h"
#include "ControlView.h"

class CFileTreeView final : public CControlViewT<CFileTreeControl>
{
public:
    void RefreshPercentages() { GetControl().Invalidate(); }
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

protected:
    void InitializeColumns() override;
};

class CFileWatcherView final : public CControlViewT<CFileWatcherControl, LVS_SINGLESEL>
{
protected:
    void InitializeColumns() override;
};

class CFilePermsView final : public CControlViewT<CFilePermsControl>
{
protected:
    void InitializeColumns() override;
};

class CFileTopView final : public CControlViewT<CFileTopControl>
{
protected:
    void InitializeColumns() override;
};

class CFileDupeView final : public CControlViewT<CFileDupeControl>
{
protected:
    void InitializeColumns() override;
};

class CFileSearchView final : public CControlViewT<CFileSearchControl>
{
protected:
    void InitializeColumns() override;
};
