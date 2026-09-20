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
    m_customOptions = COptions::GetCustomTreeMapPreset();
    m_highlightColor.SetColor(COptions::TreeMapHighlightColor);
    for (const std::wstring& style : SplitString(
        Localization::Lookup(IDS_PAGE_GRAPHS_STYLES), L','))
    {
        m_styleCombo.AddString(style);
    }
    assert(m_styleCombo.GetCount() == static_cast<int>(TreeMapLayout::Style::Moore) + 1);
    for (const auto preset : { IDS_MENU_GRAPH_PRESET_CLASSIC, IDS_MENU_GRAPH_PRESET_CALM,
        IDS_MENU_GRAPH_PRESET_FLAT, IDS_MENU_GRAPH_PRESET_PASTEL,
        IDS_MENU_GRAPH_PRESET_HIGH_CONTRAST, IDS_MENU_GRAPH_PRESET_CUSTOM })
    {
        m_presetCombo.AddString(GetLocalizedMenuText(preset));
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

    if (m_customOptions) COptions::SaveCustomTreeMapPreset(*m_customOptions);
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
        m_options.style = static_cast<TreeMapLayout::Style>(GetComboSelection(IDC_TREEMAPSTYLE));
        m_options.grid = IsChecked(IDC_TREEMAPGRID);
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
        m_gridColor.SetColor(CTreeMap::GetGridColor(m_options));
    }
}

void CPageTreeMap::UpdateStatics()
{
    m_gridColor.SetText(Localization::Lookup(m_options.gridColor == CLR_DEFAULT
        ? IDS_PAGE_GRAPHS_GRID_AUTO : IDS_PAGE_GRAPHS_GRID_COLOR));
    SetText(IDC_STATICBRIGHTNESS, m_brightness.GetPos());
    SetText(IDC_STATICCUSHIONSHADING, m_cushionShading.GetPos());
    SetText(IDC_STATICHEIGHT, m_height.GetPos() / (c_MaxHeight / 100));
    SetText(IDC_STATICSCALEFACTOR, m_scaleFactor.GetPos());
}

void CPageTreeMap::UpdatePresetSelection()
{
    const auto preset = CTreeMap::GetMatchingPreset(m_options);
    m_presetCombo.SetCurSel(preset ? std::to_underlying(*preset)
        : std::to_underlying(CTreeMap::Preset::HighContrast) + 1);
}

void CPageTreeMap::OnSomethingChanged()
{
    if (!IsInitialized())
        return;

    UpdateOptions(true);
    if (!CTreeMap::GetMatchingPreset(m_options)) m_customOptions = m_options;
    UpdatePresetSelection();
    UpdateStatics();
    m_preview.SetOptions(&m_options);
    m_preview.Invalidate();
    SetModified();
}

void CPageTreeMap::OnColorChangedTreeMapGrid(NMHDR*, LRESULT* result)
{
    *result = 0;
    m_options.gridColor = m_gridColor.GetColor();
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
    if (preset < 0) return;
    if (preset > static_cast<int>(CTreeMap::Preset::HighContrast))
    {
        if (m_customOptions) ApplyAppearance(*m_customOptions);
        else UpdatePresetSelection();
        return;
    }

    ApplyAppearance(CTreeMap::GetPreset(static_cast<CTreeMap::Preset>(preset)));
}

void CPageTreeMap::ApplyAppearance(const CTreeMap::Options& appearance)
{
    m_options.SetAppearance(appearance);

    UpdateOptions(false);
    OnSomethingChanged();
}
