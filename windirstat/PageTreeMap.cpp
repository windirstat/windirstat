// PageTreeMap.cpp - Implementation of CPageTreeMap
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "Options.h"
#include "PageTreeMap.h"
#include "Localization.h"

namespace
{
    constexpr UINT c_MaxHeight = 200;
}

IMPLEMENT_DYNAMIC(CPageTreeMap, CPropertyPage)

CPageTreeMap::CPageTreeMap()
    : CPropertyPage(CPageTreeMap::IDD)
      , m_Options()
      , m_Undo()
{
}

void CPageTreeMap::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_PREVIEW, m_Preview);
    DDX_Control(pDX, IDC_TREEMAPHIGHLIGHTCOLOR, m_HighlightColor);
    DDX_Control(pDX, IDC_TREEMAPGRIDCOLOR, m_GridColor);
    DDX_Control(pDX, IDC_BRIGHTNESS, m_Brightness);
    DDX_Control(pDX, IDC_CUSHIONSHADING, m_CushionShading);
    DDX_Control(pDX, IDC_HEIGHT, m_Height);
    DDX_Control(pDX, IDC_SCALEFACTOR, m_ScaleFactor);
    DDX_Control(pDX, IDC_LIGHTSOURCE, m_LightSource);
    DDX_Control(pDX, IDC_RESET, m_ResetButton);

    if (!pDX->m_bSaveAndValidate)
    {
        UpdateOptions(false);
        UpdateStatics();
        m_Preview.SetOptions(&m_Options);
    }

    DDX_Radio(pDX, IDC_KDIRSTAT, m_Style);
    DDX_Check(pDX, IDC_TREEMAPGRID, m_Grid);

    DDX_Text(pDX, IDC_STATICBRIGHTNESS, m_SBrightness);
    DDX_Slider(pDX, IDC_BRIGHTNESS, m_NBrightness);

    DDX_Text(pDX, IDC_STATICCUSHIONSHADING, m_SCushionShading);
    DDX_Slider(pDX, IDC_CUSHIONSHADING, m_NCushionShading);

    DDX_Text(pDX, IDC_STATICHEIGHT, m_SHeight);
    DDX_Slider(pDX, IDC_HEIGHT, m_NHeight);

    DDX_Text(pDX, IDC_STATICSCALEFACTOR, m_SScaleFactor);
    DDX_Slider(pDX, IDC_SCALEFACTOR, m_NScaleFactor);

    DDX_XySlider(pDX, IDC_LIGHTSOURCE, m_PtLightSource);

    if (pDX->m_bSaveAndValidate)
    {
        UpdateOptions();
    }
}

BEGIN_MESSAGE_MAP(CPageTreeMap, CPropertyPage)
    ON_WM_VSCROLL()
    ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPGRIDCOLOR, OnColorChangedTreemapGrid)
    ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPHIGHLIGHTCOLOR, OnColorChangedTreemapHighlight)
    ON_BN_CLICKED(IDC_KDIRSTAT, OnSetModified)
    ON_BN_CLICKED(IDC_SEQUOIAVIEW, OnSetModified)
    ON_BN_CLICKED(IDC_TREEMAPGRID, OnSetModified)
    ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
    ON_NOTIFY(XYSLIDER_CHANGED, IDC_LIGHTSOURCE, OnLightSourceChanged)
END_MESSAGE_MAP()

BOOL CPageTreeMap::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    ValuesAltered(); // m_Undo is invalid

    m_Brightness.SetPageSize(10);
    m_CushionShading.SetPageSize(10);
    m_Height.SetRange(0, c_MaxHeight, true);
    m_Height.SetPageSize(c_MaxHeight / 10);
    m_ScaleFactor.SetPageSize(10);
    m_LightSource.SetRange(CSize(400, 400));

    m_Options = COptions::TreemapOptions;
    m_HighlightColor.SetColor(COptions::TreeMapHighlightColor);

    UpdateData(false);

    return TRUE;
}

void CPageTreeMap::OnOK()
{
    UpdateData();

    COptions::SetTreemapOptions(m_Options);
    COptions::TreeMapHighlightColor = m_HighlightColor.GetColor();
    GetDocument()->UpdateAllViews(nullptr, HINT_SELECTIONSTYLECHANGED);

    CPropertyPage::OnOK();
}

void CPageTreeMap::UpdateOptions(const bool save)
{
    if (save)
    {
        m_Options.SetBrightnessPercent(100 - m_NBrightness);
        m_Options.SetAmbientLightPercent(m_NCushionShading);
        m_Options.SetHeightPercent(c_MaxHeight - m_NHeight);
        m_Options.SetScaleFactorPercent(100 - m_NScaleFactor);
        m_Options.SetLightSourcePoint(m_PtLightSource);
        m_Options.style = m_Style == 0 ? CTreemap::KDirStatStyle : CTreemap::SequoiaViewStyle;
        m_Options.grid = FALSE != m_Grid;
        m_Options.gridColor = m_GridColor.GetColor();
    }
    else
    {
        m_NBrightness = 100 - m_Options.GetBrightnessPercent();
        m_NCushionShading = m_Options.GetAmbientLightPercent();
        m_NHeight = c_MaxHeight - m_Options.GetHeightPercent();
        m_NScaleFactor = 100 - m_Options.GetScaleFactorPercent();
        m_PtLightSource = m_Options.GetLightSourcePoint();
        m_Style = m_Options.style == CTreemap::KDirStatStyle ? 0 : 1;
        m_Grid = m_Options.grid;
        m_GridColor.SetColor(m_Options.gridColor);
    }
}

void CPageTreeMap::UpdateStatics()
{
    m_SBrightness.Format(L"%d", 100 - m_NBrightness);
    m_SCushionShading.Format(L"%d", 100 - m_NCushionShading);
    m_SHeight.Format(L"%d", (c_MaxHeight - m_NHeight) / (c_MaxHeight / 100));
    m_SScaleFactor.Format(L"%d", 100 - m_NScaleFactor);
}

void CPageTreeMap::OnSomethingChanged()
{
    UpdateData();
    UpdateData(false);
    SetModified();
}

void CPageTreeMap::ValuesAltered(const bool altered)
{
    m_Altered= altered;
    const std::wstring s = m_Altered ? Localization::Lookup(IDS_RESETTO_DEFAULTS) : Localization::Lookup(IDS_BACKTO_USERSETTINGS);
    m_ResetButton.SetWindowText(s.c_str());
}

void CPageTreeMap::OnColorChangedTreemapGrid(NMHDR*, LRESULT* result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreeMap::OnColorChangedTreemapHighlight(NMHDR*, LRESULT* result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreeMap::OnVScroll(UINT, UINT, CScrollBar*)
{
    OnSomethingChanged();
    ValuesAltered();
}

void CPageTreeMap::OnLightSourceChanged(NMHDR*, LRESULT*)
{
    OnSomethingChanged();
    ValuesAltered();
}

void CPageTreeMap::OnSetModified()
{
    OnSomethingChanged();
}

void CPageTreeMap::OnBnClickedReset()
{
    CTreemap::Options o;
    if (m_Altered)
    {
        o = CTreemap::GetDefaultOptions();
        m_Undo = m_Options;
    }
    else
    {
        o = m_Undo;
    }

    m_Options.brightness   = o.brightness;
    m_Options.ambientLight = o.ambientLight;
    m_Options.height       = o.height;
    m_Options.scaleFactor  = o.scaleFactor;
    m_Options.lightSourceX = o.lightSourceX;
    m_Options.lightSourceY = o.lightSourceY;

    ValuesAltered(!m_Altered);
    UpdateData(false);
    SetModified();
}
