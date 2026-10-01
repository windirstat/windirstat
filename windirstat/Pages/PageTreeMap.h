// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "PageShared.h"
#include "ColorButton.h"
#include "XYSlider.h"

//
// CPageTreeMap. "Settings" property page "Graphs".
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
    void UpdatePresetSelection();
    void ApplyAppearance(const CTreeMap::Options& appearance);
    void OnSomethingChanged();

    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;

    CTreeMap::Options m_options{}; // Current options
    std::optional<CTreeMap::Options> m_customOptions;

    CTreeMapPreview m_preview;

    CComboBox m_styleCombo;
    CComboBox m_presetCombo;
    CColorButton m_highlightColor;
    CColorButton m_gridColor;

    CSliderCtrl m_brightness;
    CSliderCtrl m_cushionShading;
    CSliderCtrl m_height;
    CSliderCtrl m_scaleFactor;
    CXySlider m_lightSource;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnColorChangedTreeMapGrid(NMHDR*, LRESULT*);
    void OnColorChangedTreeMapHighlight(NMHDR*, LRESULT*);
    void OnHScroll(UINT nSBCode, UINT nPos, WindowRef scrollBar);
    void OnLightSourceChanged(NMHDR*, LRESULT*);
    void OnSetModified();
    void OnPresetChanged();
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
        Route::Control<&OnPresetChanged>(CBN_SELCHANGE, IDC_TREEMAPPRESET),
        Route::Notify<&OnLightSourceChanged>(CXySlider::XYSLIDER_CHANGED, IDC_LIGHTSOURCE),
    };
    return entries;
}
