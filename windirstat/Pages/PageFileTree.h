// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "PageShared.h"
#include "ColorButton.h"

//
// CPageFileTree. "Settings" property page "Folder List".
//
class CPageFileTree final : public MessageTarget<CPageFileTree, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_TREELIST };

    CPageFileTree();
    ~CPageFileTree() override = default;

protected:
    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;
    void EnableButtons();

    static constexpr std::array<std::pair<UINT, int>, 9> c_columns = {{
        { IDC_TREECOL_FOLDERS, COL_FOLDERS },
        { IDC_TREECOL_ITEMS, COL_ITEMS },
        { IDC_TREECOL_FILES, COL_FILES },
        { IDC_TREECOL_ATTRIBUTES, COL_ATTRIBUTES },
        { IDC_TREECOL_LAST_CHANGE, COL_LAST_CHANGE },
        { IDC_TREECOL_OWNER, COL_OWNER },
        { IDC_TREECOL_PERCENTAGE, COL_PERCENTAGE },
        { IDC_TREECOL_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
        { IDC_TREECOL_SIZE_LOGICAL, COL_SIZE_LOGICAL },
    }};

    int m_fileTreeColorCount = TREELISTCOLORCOUNT;
    COLORREF m_fileTreeColor[TREELISTCOLORCOUNT] = {};

    CColorButton m_colorButton[TREELISTCOLORCOUNT];
    CSliderCtrl m_slider;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnVScroll(UINT nSBCode, UINT nPos, WindowRef scrollBar);
};

inline std::span<const RouteEntry> CPageFileTree::Routes()
{
    static constexpr std::array entries
    {
        Route::Notify<&OnSettingChanged>(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7),
        Route::Window<&OnVScroll>(WM_VSCROLL),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_PACMANANIMATION),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SHOWTIMESPENT),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_TREECOL_ATTRIBUTES, IDC_TREECOL_SIZE_PHYSICAL),
    };
    return entries;
}
