// PageTreeMap.h - Declaration of CPageTreeMap
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

#pragma once

#include "ColorButton.h"
#include "TreeMap.h"
#include "XYSlider.h"

//
// CPageTreeMap. "Settings" property page "Treemap".
//
class CPageTreeMap final : public CPropertyPage
{
    DECLARE_DYNAMIC(CPageTreeMap)

    enum { IDD = IDD_PAGE_TREEMAP };

    CPageTreeMap();
    ~CPageTreeMap() override = default;

protected:
    void UpdateOptions(bool save = true);
    void UpdateStatics();
    void OnSomethingChanged();
    void ValuesAltered(bool altered = true);

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    CTreemap::Options m_Options; // Current options

    bool m_Altered = false;   // Values have been altered. Button reads "Reset to defaults".
    CTreemap::Options m_Undo; // Valid, if m_Altered = false

    CTreemapPreview m_Preview;

    int m_Style = 0;
    CColorButton m_HighlightColor;
    BOOL m_Grid = 0;
    CColorButton m_GridColor;

    CSliderCtrl m_Brightness;
    CStringW m_SBrightness;
    int m_NBrightness = 0;

    CSliderCtrl m_CushionShading;
    CStringW m_SCushionShading;
    int m_NCushionShading = 0;

    CSliderCtrl m_Height;
    CStringW m_SHeight;
    int m_NHeight = 0;

    CSliderCtrl m_ScaleFactor;
    CStringW m_SScaleFactor;
    int m_NScaleFactor = 0;

    CXySlider m_LightSource;
    CPoint m_PtLightSource;

    CButton m_ResetButton;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChangedTreemapGrid(NMHDR*, LRESULT*);
    afx_msg void OnColorChangedTreemapHighlight(NMHDR*, LRESULT*);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnLightSourceChanged(NMHDR*, LRESULT*);
    afx_msg void OnSetModified();
    afx_msg void OnBnClickedReset();
};
