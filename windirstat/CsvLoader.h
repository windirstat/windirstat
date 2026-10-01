// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemPerm.h"

bool SaveResults(const std::wstring& path, CItem* rootItem);
CItem* LoadResults(const std::wstring& path);
bool SaveDuplicates(const std::wstring& path, const CItemDupe* rootDupe);
bool SavePermissions(const std::wstring& path, const std::vector<const CItemPerm*>& items);
