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
    constexpr std::array<std::wstring_view, 4> targets = {
        L"-", L"stdout", L"stdout-csv", L"stdout-json"
    };

    return std::ranges::any_of(targets, [path](const std::wstring_view target) noexcept {
        return IsEqualIgnoreCase(path, target);
    });
}

constexpr bool IsJsonPath(const std::wstring_view path) noexcept
{
    if (IsEqualIgnoreCase(path, L"stdout-json")) return true;

    constexpr std::wstring_view ext = L".json";
    return path.size() >= ext.size() &&
        IsEqualIgnoreCase(path.substr(path.size() - ext.size()), ext);
}

bool SaveResults(const std::wstring& path, CItem* rootItem);
CItem* LoadResults(const std::wstring& path);
bool SaveDuplicates(const std::wstring& path, const CItemDupe* rootDupe);
bool SavePermissions(const std::wstring& path, const std::vector<const CItemPerm*>& items);
