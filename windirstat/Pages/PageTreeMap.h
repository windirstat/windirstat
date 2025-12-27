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
#include "ColorButton.h"
#include "XYSlider.h"

//
// CPageTreeMap. "Settings" property page "TreeMap".
//
class CPageTreeMap final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageTreeMap)

    enum : std::uint8_t { IDD = IDD_PAGE_TREEMAP };

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

    CTreeMap::Options m_options; // Current options

    bool m_altered = false;   // Values have been altered. Button reads "Reset to defaults".
    CTreeMap::Options m_undo; // Valid, if m_altered = false

    CTreeMapPreview m_preview;

    int m_style = 0;
    CColorButton m_highlightColor;
    BOOL m_grid = 0;
    CColorButton m_gridColor;

    CSliderCtrl m_brightness;
    CStringW m_sBrightness;
    int m_nBrightness = 0;

    CSliderCtrl m_cushionShading;
    CStringW m_sCushionShading;
    int m_nCushionShading = 0;

    CSliderCtrl m_height;
    CStringW m_sHeight;
    int m_nHeight = 0;

    CSliderCtrl m_scaleFactor;
    CStringW m_sScaleFactor;
    int m_nScaleFactor = 0;

    CXySlider m_lightSource;
    CPoint m_ptLightSource;

    CButton m_resetButton;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChangedTreeMapGrid(NMHDR*, LRESULT*);
    afx_msg void OnColorChangedTreeMapHighlight(NMHDR*, LRESULT*);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnLightSourceChanged(NMHDR*, LRESULT*);
    afx_msg void OnSetModified();
    afx_msg void OnBnClickedReset();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
