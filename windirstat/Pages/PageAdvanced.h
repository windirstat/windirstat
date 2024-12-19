// PageAdvanced.h - Declaration of CPageAdvanced
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

#include "WinDirStat.h"

class COptionsPropertySheet;

//
// CPageAdvanced. "Settings" property page "General".
//
class CPageAdvanced final : public CPropertyPageEx
{
    DECLARE_DYNAMIC(CPageAdvanced)

    enum : std::uint8_t { IDD = IDD_PAGE_ADVANCED };

    CPageAdvanced();
    ~CPageAdvanced() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_ExcludeJunctions = TRUE;
    BOOL m_ExcludeVolumeMountPoints = TRUE;
    BOOL m_ExcludeSymbolicLinksDirectory = TRUE;
    BOOL m_SkipDupeDetectionCloudLinks = TRUE;
    BOOL m_SkipHiddenDirectory = FALSE;
    BOOL m_SkipProtectedDirectory = FALSE;
    BOOL m_ExcludeSymbolicLinksFile = TRUE;
    BOOL m_SkipHiddenFile = FALSE;
    BOOL m_SkipProtectedFile = FALSE;
    BOOL m_UseBackupRestore = FALSE;
    int m_ScanningThreads = 0;
    CStringW m_LargestFileCount;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSettingChanged();
public:
    afx_msg void OnBnClickedResetPreferences();
};
