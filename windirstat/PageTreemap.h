// pagetreemap.h	- Declaration of CDemoControl and CPageTreemap
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

#pragma once

#include "colorbutton.h"

//
// CDemoControl. Shows the effect of alteration of H and F.
//
class CDemoControl: public CStatic
{
public:
	CDemoControl();
	void SetParameters(int heightFactor, int scaleFactor); // Percent

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();

	void AddRidge(int left, int right, double *surface, double h);
	void DrawSurface(CDC *pdc, const CRect& rc, int left, int right, const double *surface, COLORREF color, int thick);

	double m_h;
	double m_f;

	double m_bigSurface[3];		// y(x) = [0] * x^2  +  [1] * x + [2]
	double m_smallSurface1[3];
	double m_smallSurface2[3];
	double m_sum1[3];
	double m_sum2[3];
	int m_middle;
};


//
// CPageTreemap. "Settings" property page "Treemap".
//
class CPageTreemap : public CPropertyPage
{
	DECLARE_DYNAMIC(CPageTreemap)
	enum { IDD = IDD_PAGE_TREEMAP };

public:
	CPageTreemap();
	virtual ~CPageTreemap();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	void UpdateControlStatus();

	BOOL m_treemapGrid;
	CColorButton m_treemapGridColor;
	CColorButton m_treemapHighlightColor;
	BOOL m_cushionShading;
	UINT m_scaleFactor;
	UINT m_heightFactor;
	UINT m_ambientLight;
	CSpinButtonCtrl m_ambientLightSpin;
	CSpinButtonCtrl m_heightFactorSpin;
	CSpinButtonCtrl m_scaleFactorSpin;
	CEdit m_ctlAmbientLight;
	CEdit m_ctlHeightFactor;
	CEdit m_ctlScaleFactor;
	CButton m_resetToDefaults;
	CDemoControl m_demo;

	bool m_inited;	// true after OnInitDialog

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedCushionshading();
	afx_msg void OnEnChangeScalefactor();
	afx_msg void OnEnChangeHeightfactor();
	afx_msg void OnEnChangeAmbientlight();
	afx_msg void OnBnClickedResettodefaults();
	afx_msg void OnBnClickedTreemapgrid();
	afx_msg void OnColorChangedTreemapGrid(NMHDR *, LRESULT *);
	afx_msg void OnColorChangedTreemapHighlight(NMHDR *, LRESULT *);
};
