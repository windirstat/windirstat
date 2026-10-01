// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
