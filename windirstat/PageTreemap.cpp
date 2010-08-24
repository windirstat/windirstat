// PageTreemap.cpp - Implementation of CDemoControl and CPageTreemap
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//

#include "stdafx.h"
#include "windirstat.h"
#include "PageTreemap.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const UINT _maxHeight = 200;
}

IMPLEMENT_DYNAMIC(CPageTreemap, CPropertyPage)

CPageTreemap::CPageTreemap()
    : CPropertyPage(CPageTreemap::IDD)
{
}

CPageTreemap::~CPageTreemap()
{
}

void CPageTreemap::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_PREVIEW, m_preview);
    DDX_Control(pDX, IDC_TREEMAPHIGHLIGHTCOLOR, m_highlightColor);
    DDX_Control(pDX, IDC_TREEMAPGRIDCOLOR, m_gridColor);
    DDX_Control(pDX, IDC_BRIGHTNESS, m_brightness);
    DDX_Control(pDX, IDC_CUSHIONSHADING, m_cushionShading);
    DDX_Control(pDX, IDC_HEIGHT, m_height);
    DDX_Control(pDX, IDC_SCALEFACTOR, m_scaleFactor);
    DDX_Control(pDX, IDC_LIGHTSOURCE, m_lightSource);
    DDX_Control(pDX, IDC_RESET, m_resetButton);

    if(!pDX->m_bSaveAndValidate)
    {
        UpdateOptions(false);
        UpdateStatics();
        m_preview.SetOptions(&m_options);
    }

    DDX_Radio(pDX, IDC_KDIRSTAT, m_style);
    DDX_Check(pDX, IDC_TREEMAPGRID, m_grid);

    DDX_Text(pDX, IDC_STATICBRIGHTNESS, m_sBrightness);
    DDX_Slider(pDX, IDC_BRIGHTNESS, m_nBrightness);

    DDX_Text(pDX, IDC_STATICCUSHIONSHADING, m_sCushionShading);
    DDX_Slider(pDX, IDC_CUSHIONSHADING, m_nCushionShading);

    DDX_Text(pDX, IDC_STATICHEIGHT, m_sHeight);
    DDX_Slider(pDX, IDC_HEIGHT, m_nHeight);

    DDX_Text(pDX, IDC_STATICSCALEFACTOR, m_sScaleFactor);
    DDX_Slider(pDX, IDC_SCALEFACTOR, m_nScaleFactor);

    DDX_XySlider(pDX, IDC_LIGHTSOURCE, m_ptLightSource);

    if(pDX->m_bSaveAndValidate)
    {
        UpdateOptions();
    }
}


BEGIN_MESSAGE_MAP(CPageTreemap, CPropertyPage)
    ON_WM_VSCROLL()
    ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPGRIDCOLOR, OnColorChangedTreemapGrid)
    ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPHIGHLIGHTCOLOR, OnColorChangedTreemapHighlight)
    ON_BN_CLICKED(IDC_KDIRSTAT, OnBnClickedKdirstat)
    ON_BN_CLICKED(IDC_SEQUOIAVIEW, OnBnClickedSequoiaview)
    ON_BN_CLICKED(IDC_TREEMAPGRID, OnBnClickedTreemapgrid)
    ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
    ON_NOTIFY(XYSLIDER_CHANGED, IDC_LIGHTSOURCE, OnLightSourceChanged)
END_MESSAGE_MAP()


BOOL CPageTreemap::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    ValuesAltered(); // m_undo is invalid

    m_brightness.SetPageSize(10);
    m_cushionShading.SetPageSize(10);
    m_height.SetRange(0, _maxHeight, true);
    m_height.SetPageSize(_maxHeight / 10);
    m_scaleFactor.SetPageSize(10);
    m_lightSource.SetRange(CSize(400, 400));

    m_options = *GetOptions()->GetTreemapOptions();
    m_highlightColor.SetColor(GetOptions()->GetTreemapHighlightColor());

    UpdateData(false);

    return TRUE;
}

void CPageTreemap::OnOK()
{
    UpdateData();

    GetOptions()->SetTreemapOptions(m_options);
    GetOptions()->SetTreemapHighlightColor(m_highlightColor.GetColor());

    CPropertyPage::OnOK();
}

void CPageTreemap::UpdateOptions(bool save)
{
    if(save)
    {
        m_options.SetBrightnessPercent(100 - m_nBrightness);
        m_options.SetAmbientLightPercent(m_nCushionShading);
        m_options.SetHeightPercent(_maxHeight - m_nHeight);
        m_options.SetScaleFactorPercent(100 - m_nScaleFactor);
        m_options.SetLightSourcePoint(m_ptLightSource);
        m_options.style = (m_style == 0 ? CTreemap::KDirStatStyle : CTreemap::SequoiaViewStyle);
        m_options.grid = (FALSE != m_grid);
        m_options.gridColor = m_gridColor.GetColor();
    }
    else
    {
        m_nBrightness = 100 - m_options.GetBrightnessPercent();
        m_nCushionShading = m_options.GetAmbientLightPercent();
        m_nHeight = _maxHeight - m_options.GetHeightPercent();
        m_nScaleFactor = 100 - m_options.GetScaleFactorPercent();
        m_ptLightSource = m_options.GetLightSourcePoint();
        m_style = (m_options.style == CTreemap::KDirStatStyle ? 0 : 1);
        m_grid = m_options.grid;
        m_gridColor.SetColor(m_options.gridColor);
    }
}

void CPageTreemap::UpdateStatics()
{
    m_sBrightness       .Format(_T("%d"), 100 - m_nBrightness);
    m_sCushionShading   .Format(_T("%d"), 100 - m_nCushionShading);
    m_sHeight           .Format(_T("%d"), (_maxHeight - m_nHeight) / (_maxHeight / 100));
    m_sScaleFactor      .Format(_T("%d"), 100 - m_nScaleFactor);
}

void CPageTreemap::OnSomethingChanged()
{
    UpdateData();
    UpdateData(false);
    SetModified();
}

void CPageTreemap::ValuesAltered(bool altered)
{
    m_altered = altered;
    CString s = LoadString(m_altered ? IDS_RESETTO_DEFAULTS : IDS_BACKTO_USERSETTINGS);
    m_resetButton.SetWindowText(s);
}

void CPageTreemap::OnColorChangedTreemapGrid(NMHDR *, LRESULT *result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreemap::OnColorChangedTreemapHighlight(NMHDR *, LRESULT *result)
{
    *result = 0;
    OnSomethingChanged();
}

void CPageTreemap::OnVScroll(UINT, UINT, CScrollBar*)
{
    OnSomethingChanged();
    ValuesAltered();
}

void CPageTreemap::OnLightSourceChanged(NMHDR *, LRESULT *)
{
    OnSomethingChanged();
    ValuesAltered();
}

void CPageTreemap::OnBnClickedKdirstat()
{
    OnSomethingChanged();
}

void CPageTreemap::OnBnClickedSequoiaview()
{
    OnSomethingChanged();
}

void CPageTreemap::OnBnClickedTreemapgrid()
{
    OnSomethingChanged();
}

void CPageTreemap::OnBnClickedReset()
{
    CTreemap::Options o;
    if(m_altered)
    {
        o = CTreemap::GetDefaultOptions();
        m_undo = m_options;
    }
    else
    {
        o = m_undo;
    }

    m_options.brightness = o.brightness;
    m_options.ambientLight = o.ambientLight;
    m_options.height = o.height;
    m_options.scaleFactor = o.scaleFactor;
    m_options.lightSourceX = o.lightSourceX;
    m_options.lightSourceY = o.lightSourceY;

    ValuesAltered(!m_altered);
    UpdateData(false);
    SetModified();
}
