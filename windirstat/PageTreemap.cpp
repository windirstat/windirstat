// PageTreemap.cpp		- Implementation of CDemoControl and CPageTreemap
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include ".\pagetreemap.h"


/////////////////////////////////////////////////////////////////////////////

CDemoControl::CDemoControl()
{
}

void CDemoControl::SetParameters(int heightFactor, int scaleFactor)
{
	m_h= heightFactor / 100.0;
	m_f= scaleFactor / 100.0;

	for (int i=0; i < 3; i++)
	{
		m_bigSurface[i]= 0;
		m_smallSurface1[i]= 0;
		m_smallSurface2[i]= 0;
	}

	CRect rc;
	GetClientRect(rc);

	m_middle= rc.left + (int)(0.7 * rc.Width());

	AddRidge(rc.left, rc.right, m_bigSurface, m_h);
	AddRidge(rc.left, m_middle, m_smallSurface1, m_h * m_f);
	AddRidge(m_middle, rc.right, m_smallSurface2, m_h * m_f);

	for (i=0; i < 3; i++)
	{
		m_sum1[i]= m_bigSurface[i] + m_smallSurface1[i];
		m_sum2[i]= m_bigSurface[i] + m_smallSurface2[i];
	}


	InvalidateRect(NULL);
}

void CDemoControl::AddRidge(int left, int right, double *surface, double h)
{
	surface[1]+= 4 * h * (right + left) / (right - left);
	surface[0]-= 4 * h / (right - left);
	surface[2]-= surface[0] * left * left  + surface[1] * left;
}

BEGIN_MESSAGE_MAP(CDemoControl, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CDemoControl::OnPaint()
{
	CPaintDC dc(this);

	CRect rc;
	GetClientRect(rc);

	dc.IntersectClipRect(rc);
	dc.FillSolidRect(rc, GetSysColor(COLOR_BTNFACE));

	DrawSurface(&dc, rc, rc.left, rc.right, m_bigSurface, RGB(0, 0, 255), 1);
	DrawSurface(&dc, rc, rc.left, m_middle, m_smallSurface1, RGB(0, 200, 0), 1);
	DrawSurface(&dc, rc, m_middle, rc.right, m_smallSurface2, RGB(0, 200, 0), 1);

	DrawSurface(&dc, rc, rc.left, m_middle, m_sum1, RGB(255, 0, 0), 2);
	DrawSurface(&dc, rc, m_middle, rc.right, m_sum2, RGB(255, 0, 0), 2);

}

void CDemoControl::DrawSurface(CDC *pdc, const CRect& rc, int left, int right, const double *surface, COLORREF color, int thick)
{
	for (int x=left; x < right; x++)
	{
		int y= (int)(surface[0] * x * x + surface[1] * x + surface[2]);
		y/= 2;
		
		y= rc.bottom - y;

		CRect r(x-thick, y-thick, x+thick, y+thick);
		pdc->FillSolidRect(r, color);
	}
}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CPageTreemap, CPropertyPage)

CPageTreemap::CPageTreemap()
	: CPropertyPage(CPageTreemap::IDD)
	, m_treemapGrid(FALSE)
	, m_cushionShading(FALSE)
	, m_scaleFactor(0)
	, m_heightFactor(0)
	, m_ambientLight(0)
{
	m_inited= false;
}

CPageTreemap::~CPageTreemap()
{
}

void CPageTreemap::DoDataExchange(CDataExchange* pDX)
{
	int min, max;

	CPropertyPage::DoDataExchange(pDX);

	DDX_Check(pDX, IDC_TREEMAPGRID, m_treemapGrid);
	DDX_Control(pDX, IDC_TREEMAPGRIDCOLOR, m_treemapGridColor);
	DDX_Control(pDX, IDC_TREEMAPHIGHLIGHTCOLOR, m_treemapHighlightColor);

	DDX_Check(pDX, IDC_CUSHIONSHADING, m_cushionShading);

	DDX_Text(pDX, IDC_SCALEFACTOR, m_scaleFactor);
	GetOptions()->GetScaleFactorRange(min, max);
	DDV_MinMaxUInt(pDX, m_scaleFactor, min, max);

	DDX_Text(pDX, IDC_HEIGHTFACTOR, m_heightFactor);
	GetOptions()->GetHeightFactorRange(min, max);
	DDV_MinMaxUInt(pDX, m_heightFactor, min, max);

	DDX_Text(pDX, IDC_AMBIENTLIGHT, m_ambientLight);
	GetOptions()->GetAmbientLightRange(min, max);
	DDV_MinMaxUInt(pDX, m_ambientLight, min, max);

	DDX_Control(pDX, IDC_AMBIENTLIGHTSPIN, m_ambientLightSpin);
	DDX_Control(pDX, IDC_HEIGHTFACTORSPIN, m_heightFactorSpin);
	DDX_Control(pDX, IDC_SCALEFACTORSPIN, m_scaleFactorSpin);
	DDX_Control(pDX, IDC_AMBIENTLIGHT, m_ctlAmbientLight);
	DDX_Control(pDX, IDC_HEIGHTFACTOR, m_ctlHeightFactor);
	DDX_Control(pDX, IDC_SCALEFACTOR, m_ctlScaleFactor);
	DDX_Control(pDX, IDC_RESETTODEFAULTS, m_resetToDefaults);
	DDX_Control(pDX, IDC_DEMO, m_demo);
}


BEGIN_MESSAGE_MAP(CPageTreemap, CPropertyPage)
	ON_BN_CLICKED(IDC_TREEMAPGRID, OnBnClickedTreemapgrid)
	ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPGRIDCOLOR, OnColorChangedTreemapGrid)
	ON_NOTIFY(COLBN_CHANGED, IDC_TREEMAPHIGHLIGHTCOLOR, OnColorChangedTreemapHighlight)
	ON_BN_CLICKED(IDC_CUSHIONSHADING, OnBnClickedCushionshading)
	ON_EN_CHANGE(IDC_SCALEFACTOR, OnEnChangeScalefactor)
	ON_EN_CHANGE(IDC_HEIGHTFACTOR, OnEnChangeHeightfactor)
	ON_EN_CHANGE(IDC_AMBIENTLIGHT, OnEnChangeAmbientlight)
	ON_BN_CLICKED(IDC_RESETTODEFAULTS, OnBnClickedResettodefaults)
END_MESSAGE_MAP()


BOOL CPageTreemap::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_cushionShading= GetOptions()->IsCushionShading();
	m_treemapGrid= GetOptions()->IsTreemapGrid();
	m_treemapGridColor.SetColor(GetOptions()->GetTreemapGridColor());
	m_treemapHighlightColor.SetColor(GetOptions()->GetTreemapHighlightColor());
	m_heightFactor= GetOptions()->GetHeightFactor();
	m_scaleFactor= GetOptions()->GetScaleFactor();
	m_ambientLight= GetOptions()->GetAmbientLight();

	int min, max;

	GetOptions()->GetHeightFactorRange(min, max);
	m_heightFactorSpin.SetRange32(min, max);
	
	GetOptions()->GetScaleFactorRange(min, max);
	m_scaleFactorSpin.SetRange32(min, max);

	GetOptions()->GetAmbientLightRange(min, max);
	m_ambientLightSpin.SetRange32(min, max);

	UpdateData(false);
	m_inited= true;
	UpdateControlStatus();
	return TRUE;
}

void CPageTreemap::OnOK()
{
	UpdateData();

	GetOptions()->SetCushionShading(m_cushionShading);
	GetOptions()->SetTreemapGrid(m_treemapGrid);
	GetOptions()->SetTreemapGridColor(m_treemapGridColor.GetColor());
	GetOptions()->SetTreemapHighlightColor(m_treemapHighlightColor.GetColor());
	GetOptions()->SetHeightFactor(m_heightFactor);
	GetOptions()->SetScaleFactor(m_scaleFactor);
	GetOptions()->SetAmbientLight(m_ambientLight);

	CPropertyPage::OnOK();
}

void CPageTreemap::OnBnClickedTreemapgrid()
{
	SetModified();
	UpdateControlStatus();
}

void CPageTreemap::OnColorChangedTreemapGrid(NMHDR *, LRESULT *result)
{
	*result= 0;
	SetModified();
}

void CPageTreemap::OnColorChangedTreemapHighlight(NMHDR *, LRESULT *result)
{
	*result= 0;
	SetModified();
}

void CPageTreemap::UpdateControlStatus()
{
	if (!m_inited)
		return;

	UpdateData();

	m_ctlHeightFactor.EnableWindow(m_cushionShading);
	m_heightFactorSpin.EnableWindow(m_cushionShading);
	m_ctlScaleFactor.EnableWindow(m_cushionShading);
	m_scaleFactorSpin.EnableWindow(m_cushionShading);
	m_ctlAmbientLight.EnableWindow(m_cushionShading);
	m_ambientLightSpin.EnableWindow(m_cushionShading);
	m_resetToDefaults.EnableWindow(m_cushionShading);

	m_demo.SetParameters(m_heightFactor, m_scaleFactor);

	m_treemapGridColor.EnableWindow(m_treemapGrid);
}

void CPageTreemap::OnBnClickedCushionshading()
{
	SetModified();
	UpdateData();
	if (!m_cushionShading)
	{
		m_treemapGrid= true;
		UpdateData(false);
	}
	UpdateControlStatus();
}

void CPageTreemap::OnEnChangeScalefactor()
{
	SetModified();
	UpdateControlStatus();
}

void CPageTreemap::OnEnChangeHeightfactor()
{
	SetModified();
	UpdateControlStatus();
}

void CPageTreemap::OnEnChangeAmbientlight()
{
	SetModified();
}

void CPageTreemap::OnBnClickedResettodefaults()
{
	UpdateData();
	m_heightFactor	= GetOptions()->GetHeightFactorDefault();
	m_scaleFactor	= GetOptions()->GetScaleFactorDefault();
	m_ambientLight	= GetOptions()->GetAmbientLightDefault();
	UpdateData(false);
	UpdateControlStatus();
	SetModified();
}
