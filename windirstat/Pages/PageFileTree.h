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
    void OnOK() override;
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
    void OnVScroll(UINT nSBCode, UINT nPos, CWnd* scrollBar);
};

inline std::span<const RouteEntry> CPageFileTree::Routes()
{
    using ThisClass = CPageFileTree;
    static constexpr std::array entries
    {
        Route::Notify<&ThisClass::OnSettingNotifyChanged>(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7),
        Route::Window<&ThisClass::OnVScroll>(WM_VSCROLL),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_PACMANANIMATION),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_SHOWTIMESPENT),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_FOLDERS),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_ITEMS),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_FILES),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_ATTRIBUTES),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_LAST_CHANGE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_OWNER),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_PERCENTAGE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_SIZE_LOGICAL),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_TREECOL_SIZE_PHYSICAL),
    };
    return entries;
}
