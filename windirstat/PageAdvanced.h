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

class COptionsPropertySheet;

//
// CPageAdvanced. "Settings" property page "General".
//
class CPageAdvanced final : public CPropertyPage
{
    DECLARE_DYNAMIC(CPageAdvanced)

    enum { IDD = IDD_PAGE_ADVANCED };

    CPageAdvanced();
    ~CPageAdvanced() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_excludeJunctions = TRUE;
    BOOL m_excludeVolumeMountPoints = TRUE;
    BOOL m_excludeSymbolicLinks = TRUE;
    BOOL m_skipDupeDetectionCloudLinks = TRUE;
    BOOL m_skipHidden = FALSE;
    BOOL m_skipProtected = FALSE;
    BOOL m_useBackupRestore = FALSE;
    int m_scanningThreads = 0;

    /*
    CButton m_ctlFollowReparseDfs;
    CButton m_ctlFollowReparseJunctions;
    CButton m_ctlFollowReparseOneDrive;
    CButton m_ctlFollowReparseSymlinks;
    CButton m_ctlFollowReparseMountPoints;
    CButton m_ctlFollowReparseOthers;

    CButton m_ctlFollowMountPoints;
    CButton m_ctlFollowJunctions;
    CButton m_ctlSkipHidden;*/

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSettingChanged();
};
