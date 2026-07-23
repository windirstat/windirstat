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
#include "PagePrompts.h"

IMPLEMENT_DYNAMIC(CPagePrompts, COptionsPage)

CPagePrompts::CPagePrompts() : COptionsPage(IDD)
{
    BindCheck(IDC_DELETION_WARNING, COptions::ShowDeleteWarning, m_showDeleteWarning);
    BindCheck(IDC_ELEVATION_PROMPT, COptions::ShowElevationPrompt, m_showElevationPrompt);
    BindCheck(IDC_CLOUD_LINKS_WARNING, COptions::ShowDupeDetectionCloudLinksWarning,
        m_showDupeDetectionCloudLinksWarning);
    BindCheck(IDC_SHOW_MICROSOFT_PROGRESS, COptions::ShowMicrosoftProgress, m_showMicrosoftProgress);
}

BEGIN_MESSAGE_MAP(CPagePrompts, COptionsPage)
    ON_BN_CLICKED(IDC_DELETION_WARNING, OnSettingChanged)
    ON_BN_CLICKED(IDC_ELEVATION_PROMPT, OnSettingChanged)
    ON_BN_CLICKED(IDC_CLOUD_LINKS_WARNING, OnSettingChanged)
    ON_BN_CLICKED(IDC_SHOW_MICROSOFT_PROGRESS, OnSettingChanged)
END_MESSAGE_MAP()

void CPagePrompts::InitializePage()
{
    UpdateData(FALSE);
}

void CPagePrompts::OnOK()
{
    UpdateData();

    ApplyOptionBindings();
    CMFCPropertyPage::OnOK();
}
