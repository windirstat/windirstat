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
#include "ExtensionListControl.h"

//
// CExtensionView. The upper right view, which shows the extensions and their
// cushion colors.
//
class CExtensionView final : public CView
{
protected:
    CExtensionView();
    DECLARE_DYNCREATE(CExtensionView)

    ~CExtensionView() override = default;
    CDirStatDoc* GetDocument() const
    {
        return static_cast<CDirStatDoc*>(m_pDocument);
    }
    void SysColorChanged();
    bool IsShowTypes() const;
    void ShowTypes(bool show);

    void SetHighlightExtension(const std::wstring& ext);

    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
    void OnDraw(CDC* pDC) override;
    void SetSelection();

    bool m_ShowTypes = true;                      // Whether this view shall be shown (F8 option)
    CMFCTabCtrl m_Tabs;                              // The tab control
    CExtensionListControl m_ExtensionListControl; // The list control

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};
