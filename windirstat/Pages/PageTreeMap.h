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
#include "XYSlider.h"

//
// CPageTreeMap. "Settings" property page "TreeMap".
//
class CPageTreeMap final : public MessageTarget<CPageTreeMap, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_TREEMAP };

    CPageTreeMap();
    ~CPageTreeMap() override = default;

    bool PreprocessMessage(MSG* pMsg) override;

protected:
    void UpdateOptions(bool save = true);
    void UpdateStatics();
    void OnSomethingChanged();
    void ValuesAltered(bool altered = true);

    void InitializePage() override;
    void OnOK() override;

    CTreeMap::Options m_options{}; // Current options

    bool m_altered = false;   // Values have been altered. Button reads "Reset to defaults".
    CTreeMap::Options m_undo{}; // Valid, if m_altered = false

    CTreeMapPreview m_preview;

    CComboBox m_styleCombo;
    CColorButton m_highlightColor;
    CColorButton m_gridColor;

    CSliderCtrl m_brightness;
    CSliderCtrl m_cushionShading;
    CSliderCtrl m_height;
    CSliderCtrl m_scaleFactor;
    CXySlider m_lightSource;

    CButton m_resetButton;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnColorChangedTreeMapGrid(NMHDR*, LRESULT*);
    void OnColorChangedTreeMapHighlight(NMHDR*, LRESULT*);
    void OnHScroll(UINT nSBCode, UINT nPos, CWnd* scrollBar);
    void OnLightSourceChanged(NMHDR*, LRESULT*);
    void OnSetModified();
    void OnBnClickedReset();
};

inline std::span<const RouteEntry> CPageTreeMap::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnHScroll>(WM_HSCROLL),
        Route::Notify<&OnColorChangedTreeMapGrid>(COLBN_CHANGED, IDC_TREEMAPGRIDCOLOR),
        Route::Notify<&OnColorChangedTreeMapHighlight>(COLBN_CHANGED, IDC_TREEMAPHIGHLIGHTCOLOR),
        Route::Control<&OnSetModified>(CBN_SELCHANGE, IDC_TREEMAPSTYLE),
        Route::Control<&OnSetModified>(BN_CLICKED, IDC_TREEMAPGRID),
        Route::Control<&OnBnClickedReset>(BN_CLICKED, IDC_RESET),
        Route::Notify<&OnLightSourceChanged>(CXySlider::XYSLIDER_CHANGED, IDC_LIGHTSOURCE),
    };
    return entries;
}
