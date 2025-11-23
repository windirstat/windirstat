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

#include "stdafx.h"
#include "WinDirStat.h"
#include "MainFrame.h"
#include "PagePrompts.h"
#include "DirStatDoc.h"
#include "Options.h"
#include "GlobalHelpers.h"
#include "Localization.h"
#include "DarkMode.h"

IMPLEMENT_DYNAMIC(CPagePrompts, CMFCPropertyPage)

CPagePrompts::CPagePrompts() : CMFCPropertyPage(IDD) {}

CPagePrompts::~CPagePrompts() = default;

COptionsPropertySheet* CPagePrompts::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void CPagePrompts::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_DELETION_WARNING, m_ShowDeleteWarning);
    DDX_Check(pDX, IDC_ELEVATION_PROMPT, m_ShowElevationPrompt);
    DDX_Check(pDX, IDC_FAST_SCAN_PROMPT, m_ShowFastScanPrompt);
    DDX_Check(pDX, IDC_CLOUD_LINKS_WARNING, m_SkipDupeDetectionCloudLinksWarning);
}

BEGIN_MESSAGE_MAP(CPagePrompts, CMFCPropertyPage)
    ON_BN_CLICKED(IDC_DELETION_WARNING, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_ELEVATION_PROMPT, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_FAST_SCAN_PROMPT, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_CLOUD_LINKS_WARNING, OnBnClickedSetModified)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPagePrompts::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPagePrompts::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    m_ShowDeleteWarning = COptions::ShowDeleteWarning;
    m_ShowElevationPrompt = COptions::ShowElevationPrompt;
    m_ShowFastScanPrompt = COptions::ShowFastScanPrompt;
    m_SkipDupeDetectionCloudLinksWarning = COptions::SkipDupeDetectionCloudLinksWarning;

    UpdateData(FALSE);
    return TRUE;
}

void CPagePrompts::OnOK()
{
    UpdateData();

    COptions::ShowDeleteWarning = (FALSE != m_ShowDeleteWarning);
    COptions::ShowElevationPrompt = (FALSE != m_ShowElevationPrompt);
    COptions::ShowFastScanPrompt = (FALSE != m_ShowFastScanPrompt);
    COptions::SkipDupeDetectionCloudLinksWarning = (FALSE != m_SkipDupeDetectionCloudLinksWarning);

    CMFCPropertyPage::OnOK();
}

void CPagePrompts::OnBnClickedSetModified()
{
    SetModified();
}
