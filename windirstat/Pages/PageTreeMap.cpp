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

#include "pch.h"
#include "PageTreeMap.h"

namespace
{
    constexpr UINT c_MaxHeight = 200;
}

CPageTreeMap::CPageTreeMap()
    : MessageTarget(IDD)
{
}

bool CPageTreeMap::PreprocessMessage(MSG* pMsg)
{
    if (pMsg->message == WM_MOUSEWHEEL)
    {
        const CPoint pt = ToClient(pMsg->pt);

        if (CWnd* pWnd = ChildWindowFromPoint(pt); pWnd != nullptr)
        {
            if (const int nID = pWnd->GetDlgCtrlID();
                nID == IDC_BRIGHTNESS || nID == IDC_CUSHIONSHADING ||
                nID == IDC_HEIGHT || nID == IDC_SCALEFACTOR)
            {
                CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(pWnd);
                const short zDelta = static_cast<short>(HIWORD(pMsg->wParam));

                const int currentPos = pSlider->GetPos();

                // Perform "Natural Scroll" (Up = Increase)
                if (zDelta > 0)
                    pSlider->SetPos(currentPos + 1);
                else
                    pSlider->SetPos(currentPos - 1);

                OnSomethingChanged();

                return true;
            }
        }
    }
    return CSettingsPage::PreprocessMessage(pMsg);
}

void CPageTreeMap::InitializePage()
{
    m_preview.SubclassDlgItem(IDC_PREVIEW, this);
    m_styleCombo.SubclassDlgItem(IDC_TREEMAPSTYLE, this);
    m_presetCombo.SubclassDlgItem(IDC_TREEMAPPRESET, this);
    m_highlightColor.SubclassDlgItem(IDC_TREEMAPHIGHLIGHTCOLOR, this);
    m_gridColor.SubclassDlgItem(IDC_TREEMAPGRIDCOLOR, this);
    m_brightness.SubclassDlgItem(IDC_BRIGHTNESS, this);
    m_cushionShading.SubclassDlgItem(IDC_CUSHIONSHADING, this);
    m_height.SubclassDlgItem(IDC_HEIGHT, this);
    m_scaleFactor.SubclassDlgItem(IDC_SCALEFACTOR, this);
    m_lightSource.SubclassDlgItem(IDC_LIGHTSOURCE, this);

    m_brightness.SetPageSize(10);
    m_cushionShading.SetPageSize(10);
    m_height.SetRange(0, c_MaxHeight, true);
    m_height.SetPageSize(c_MaxHeight / 10);
    m_scaleFactor.SetPageSize(10);
    m_lightSource.SetRange(CSize(400, 400));

    m_options = COptions::TreeMapOptions;
    m_highlightColor.SetColor(COptions::TreeMapHighlightColor);
    for (const std::wstring& style : SplitString(
        Localization::Lookup(IDS_PAGE_TREEMAP_STYLES), L','))
    {
        m_styleCombo.AddString(style.c_str());
    }
    assert(m_styleCombo.GetCount() == static_cast<int>(TreeMapLayout::Style::Moore) + 1);
    for (const std::wstring& preset : SplitString(Localization::Lookup(IDS_PAGE_TREEMAP_PRESETS), L','))
    {
        m_presetCombo.AddString(preset.c_str());
    }
    assert(m_presetCombo.GetCount() == static_cast<int>(CTreeMap::Preset::HighContrast) + 2);

    UpdateOptions(false);
    UpdatePresetSelection();
    UpdateStatics();
    m_preview.SetOptions(&m_options);
}

void CPageTreeMap::OnOK()
{
    UpdateOptions(true);

    COptions::SetTreeMapOptions(m_options);
    COptions::TreeMapHighlightColor = m_highlightColor.GetColor();
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SELECTION_STYLE);
}

void CPageTreeMap::UpdateOptions(const bool save)
{
    if (save)
    {
        m_options.SetBrightnessPercent(m_brightness.GetPos());
        m_options.SetAmbientLightPercent(100 - m_cushionShading.GetPos());
        m_options.SetHeightPercent(m_height.GetPos());
        m_options.SetScaleFactorPercent(m_scaleFactor.GetPos());
        m_options.SetLightSourcePoint(m_lightSource.GetPos());
        m_options.style = static_cast<TreeMapLayout::Style>(ComboSelection(IDC_TREEMAPSTYLE));
        m_options.grid = IsChecked(IDC_TREEMAPGRID);
        m_options.gridColor = m_gridColor.GetColor();
    }
    else
    {
        m_brightness.SetPos(m_options.GetBrightnessPercent());
        m_cushionShading.SetPos(100 - m_options.GetAmbientLightPercent());
        m_height.SetPos(m_options.GetHeightPercent());
        m_scaleFactor.SetPos(m_options.GetScaleFactorPercent());
        m_lightSource.SetPos(m_options.GetLightSourcePoint());
        SetComboSelection(IDC_TREEMAPSTYLE, static_cast<int>(m_options.style));
        SetChecked(IDC_TREEMAPGRID, m_options.grid);
        m_gridColor.SetColor(m_options.gridColor);
    }
}

void CPageTreeMap::UpdateStatics()
{
    SetText(IDC_STATICBRIGHTNESS, std::to_wstring(m_brightness.GetPos()));
    SetText(IDC_STATICCUSHIONSHADING, std::to_wstring(m_cushionShading.GetPos()));
    SetText(IDC_STATICHEIGHT, std::to_wstring(m_height.GetPos() / (c_MaxHeight / 100)));
    SetText(IDC_STATICSCALEFACTOR, std::to_wstring(m_scaleFactor.GetPos()));
}

void CPageTreeMap::UpdatePresetSelection()
{
    const auto appearance = [](const CTreeMap::Options& options)
    {
        return std::tuple(options.grid, options.grid ? options.gridColor : CLR_INVALID, options.contrastLabels,
            options.GetBrightnessPercent(), options.GetSaturationPercent(), options.GetAmbientLightPercent(),
            options.GetHeightPercent(), options.GetScaleFactorPercent(),
            options.GetLightSourceXPercent(), options.GetLightSourceYPercent());
    };
    const auto current = appearance(m_options);
    int selection = static_cast<int>(CTreeMap::Preset::HighContrast) + 1;
    for (int preset = 0; preset <= static_cast<int>(CTreeMap::Preset::HighContrast); ++preset)
    {
        if (current != appearance(CTreeMap::GetPreset(static_cast<CTreeMap::Preset>(preset)))) continue;
        selection = preset;
        break;
    }
    m_presetCombo.SetCurSel(selection);
}

void CPageTreeMap::OnSomethingChanged()
{
    if (!IsInitialized())
        return;

    UpdateOptions(true);
    UpdatePresetSelection();
    UpdateStatics();
    m_preview.SetOptions(&m_options);
    m_preview.Invalidate();
    SetModified();
}

void CPageTreeMap::OnColorChangedTreeMapGrid(NMHDR*, LRESULT* result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreeMap::OnColorChangedTreeMapHighlight(NMHDR*, LRESULT* result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreeMap::OnHScroll(UINT, UINT, CWnd*)
{
    OnSomethingChanged();
}

void CPageTreeMap::OnLightSourceChanged(NMHDR*, LRESULT*)
{
    OnSomethingChanged();
}

void CPageTreeMap::OnSetModified()
{
    OnSomethingChanged();
}

void CPageTreeMap::OnPresetChanged()
{
    const int preset = m_presetCombo.GetCurSel();
    if (preset < 0 || preset > static_cast<int>(CTreeMap::Preset::HighContrast)) return;

    ApplyAppearance(CTreeMap::GetPreset(static_cast<CTreeMap::Preset>(preset)));
}

void CPageTreeMap::ApplyAppearance(const CTreeMap::Options& appearance)
{
    m_options.grid = appearance.grid;
    m_options.gridColor = appearance.gridColor;
    m_options.brightness = appearance.brightness;
    m_options.saturation = appearance.saturation;
    m_options.contrastLabels = appearance.contrastLabels;
    m_options.ambientLight = appearance.ambientLight;
    m_options.height = appearance.height;
    m_options.scaleFactor = appearance.scaleFactor;
    m_options.lightSourceX = appearance.lightSourceX;
    m_options.lightSourceY = appearance.lightSourceY;

    UpdateOptions(false);
    OnSomethingChanged();
}
