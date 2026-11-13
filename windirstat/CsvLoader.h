// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemPerm.h"

constexpr bool IsStdoutPath(const std::wstring_view path) noexcept
{
    return path == L"-" ||
        path == L"-stdout" ||
        path == L"-stdout-csv" ||
        path == L"-stdout-json";
}

constexpr bool IsJsonPath(const std::wstring_view path) noexcept
{
    if (path == L"-stdout-json") return true;

    constexpr std::wstring_view ext = L".json";
    return path.size() >= ext.size() &&
        path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

bool SaveResults(const std::wstring& path, CItem* rootItem);
CItem* LoadResults(const std::wstring& path);
bool SaveDuplicates(const std::wstring& path, const CItemDupe* rootDupe);
bool SavePermissions(const std::wstring& path, const std::vector<const CItemPerm*>& items);
