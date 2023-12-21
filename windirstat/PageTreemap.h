// pagetreemap.h - Declaration of CDemoControl and CPageTreemap
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#pragma once

#include "colorbutton.h"
#include "treemap.h"
#include "xyslider.h"
#include "afxwin.h"

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
    void UpdateOptions(bool save = true);
    void UpdateStatics();
    void OnSomethingChanged();
    void ValuesAltered(bool altered = true);

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    CTreemap::Options m_options;    // Current options

    bool m_altered;                 // Values have been altered. Button reads "Reset to defaults".
    CTreemap::Options m_undo;       // Valid, if m_altered = false

    CTreemapPreview m_preview;

    int m_style;
    CColorButton m_highlightColor;
    BOOL m_grid;
    CColorButton m_gridColor;

    CSliderCtrl m_brightness;
    CString m_sBrightness;
    int m_nBrightness;

    CSliderCtrl m_cushionShading;
    CString m_sCushionShading;
    int m_nCushionShading;

    CSliderCtrl m_height;
    CString m_sHeight;
    int m_nHeight;

    CSliderCtrl m_scaleFactor;
    CString m_sScaleFactor;
    int m_nScaleFactor;

    CXySlider m_lightSource;
    CPoint m_ptLightSource;

    CButton m_resetButton;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChangedTreemapGrid(NMHDR *, LRESULT *);
    afx_msg void OnColorChangedTreemapHighlight(NMHDR *, LRESULT *);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnLightSourceChanged(NMHDR *, LRESULT *);
    afx_msg void OnBnClickedKdirstat();
    afx_msg void OnBnClickedSequoiaview();
    afx_msg void OnBnClickedTreemapgrid();
    afx_msg void OnBnClickedReset();

};
