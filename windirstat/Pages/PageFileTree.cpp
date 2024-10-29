// PageFileTree.cpp - Implementation of CPageFileTree
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

#include "stdafx.h"
#include "Options.h"
#include "PageFileTree.h"
#include "DirStatDoc.h"
#include "FileTreeView.h"
#include "Localization.h"
#include "MainFrame.h"

IMPLEMENT_DYNAMIC(CPageFileTree, CPropertyPageEx)

CPageFileTree::CPageFileTree() : CPropertyPageEx(IDD) {}

void CPageFileTree::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_PACMANANIMATION, m_PacmanAnimation);
    DDX_Check(pDX, IDC_SHOWTIMESPENT, m_ShowTimeSpent);
    DDX_Check(pDX, IDC_TREECOL_FOLDERS, m_ShowColumnFolders);
    DDX_Check(pDX, IDC_TREECOL_SIZE_PHYSICAL, m_ShowColumnSizePhysical);
    DDX_Check(pDX, IDC_TREECOL_SIZE_LOGICAL, m_ShowColumnSizeLogical);
    DDX_Check(pDX, IDC_TREECOL_ITEMS, m_ShowColumnItems);
    DDX_Check(pDX, IDC_TREECOL_FILES, m_ShowColumnFiles);
    DDX_Check(pDX, IDC_TREECOL_ATTRIBUTES, m_ShowColumnAttributes);
    DDX_Check(pDX, IDC_TREECOL_LASTCHANGE, m_ShowColumnLastChange);
    DDX_Check(pDX, IDC_TREECOL_OWNER, m_ShowColumnOwner);
    for (int i = 0; i < TREELISTCOLORCOUNT; i++)
    {
        DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_ColorButton[i]);
        if (pDX->m_bSaveAndValidate)
        {
            m_FileTreeColor[i] = m_ColorButton[i].GetColor();
        }
        else
        {
            m_ColorButton[i].SetColor(m_FileTreeColor[i]);
        }
    }
    DDX_Control(pDX, IDC_SLIDER, m_Slider);
}

BEGIN_MESSAGE_MAP(CPageFileTree, CPropertyPageEx)
    ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7, OnColorChanged)
    ON_WM_VSCROLL()
    ON_BN_CLICKED(IDC_PACMANANIMATION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOWTIMESPENT, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_FOLDERS, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_ITEMS, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_FILES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_ATTRIBUTES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_LASTCHANGE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_OWNER, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_LOGICAL, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_PHYSICAL, OnBnClickedSetModified)
END_MESSAGE_MAP()

BOOL CPageFileTree::OnInitDialog()
{
    CPropertyPageEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_PacmanAnimation= COptions::PacmanAnimation;
    m_ShowTimeSpent = COptions::ShowTimeSpent;
    m_ShowColumnFolders = COptions::ShowColumnFolders;
    m_ShowColumnItems = COptions::ShowColumnItems;
    m_ShowColumnFiles = COptions::ShowColumnFiles;
    m_ShowColumnAttributes = COptions::ShowColumnAttributes;
    m_ShowColumnLastChange = COptions::ShowColumnLastChange;
    m_ShowColumnOwner = COptions::ShowColumnOwner;
    m_ShowColumnSizePhysical = COptions::ShowColumnSizePhysical;
    m_ShowColumnSizeLogical = COptions::ShowColumnSizeLogical;

    m_FileTreeColorCount = COptions::FileTreeColorCount;
    m_FileTreeColor[0] = COptions::FileTreeColor0;
    m_FileTreeColor[1] = COptions::FileTreeColor1;
    m_FileTreeColor[2] = COptions::FileTreeColor2;
    m_FileTreeColor[3] = COptions::FileTreeColor3;
    m_FileTreeColor[4] = COptions::FileTreeColor4;
    m_FileTreeColor[5] = COptions::FileTreeColor5;
    m_FileTreeColor[6] = COptions::FileTreeColor6;
    m_FileTreeColor[7] = COptions::FileTreeColor7;

    m_Slider.SetRange(1, TREELISTCOLORCOUNT);
    m_Slider.SetPos(m_FileTreeColorCount);

    EnableButtons();
    UpdateData(FALSE);
    return TRUE;
}

void CPageFileTree::OnOK()
{
    const bool colsChanged =
        COptions::ShowColumnFolders != (FALSE != m_ShowColumnFolders) ||
        COptions::ShowColumnItems != (FALSE != m_ShowColumnItems) ||
        COptions::ShowColumnFiles != (FALSE != m_ShowColumnFiles) ||
        COptions::ShowColumnAttributes != (FALSE != m_ShowColumnAttributes) ||
        COptions::ShowColumnLastChange != (FALSE != m_ShowColumnLastChange) ||
        COptions::ShowColumnOwner != (FALSE != m_ShowColumnOwner) ||
        COptions::ShowColumnSizePhysical != (FALSE != m_ShowColumnSizePhysical) ||
        COptions::ShowColumnSizeLogical != (FALSE != m_ShowColumnSizeLogical);

    UpdateData();
    COptions::PacmanAnimation = (FALSE != m_PacmanAnimation);
    COptions::ShowTimeSpent = (FALSE != m_ShowTimeSpent);
    COptions::ShowColumnFolders = (FALSE != m_ShowColumnFolders);
    COptions::ShowColumnItems = (FALSE != m_ShowColumnItems);
    COptions::ShowColumnFiles = (FALSE != m_ShowColumnFiles);
    COptions::ShowColumnAttributes = (FALSE != m_ShowColumnAttributes);
    COptions::ShowColumnLastChange = (FALSE != m_ShowColumnLastChange);
    COptions::ShowColumnOwner = (FALSE != m_ShowColumnOwner);
    COptions::ShowColumnSizePhysical = (FALSE != m_ShowColumnSizePhysical);
    COptions::ShowColumnSizeLogical = (FALSE != m_ShowColumnSizeLogical);
    COptions::FileTreeColorCount = m_FileTreeColorCount;
    COptions::FileTreeColor0 = m_FileTreeColor[0];
    COptions::FileTreeColor1 = m_FileTreeColor[1];
    COptions::FileTreeColor2 = m_FileTreeColor[2];
    COptions::FileTreeColor3 = m_FileTreeColor[3];
    COptions::FileTreeColor4 = m_FileTreeColor[4];
    COptions::FileTreeColor5 = m_FileTreeColor[5];
    COptions::FileTreeColor6 = m_FileTreeColor[6];
    COptions::FileTreeColor7 = m_FileTreeColor[7];
    if (colsChanged) CMainFrame::Get()->GetFileTreeView()->CreateColumns();
    CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    CPropertyPageEx::OnOK();
}

void CPageFileTree::OnBnClickedSetModified()
{
    SetModified();
}

void CPageFileTree::OnColorChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}

void CPageFileTree::EnableButtons()
{
    int i = 0;
    for (; i < m_FileTreeColorCount; i++)
    {
        m_ColorButton[i].EnableWindow(true);
    }
    for (; i < TREELISTCOLORCOUNT; i++)
    {
        m_ColorButton[i].EnableWindow(false);
    }
}

void CPageFileTree::OnVScroll(const UINT nSBCode, const UINT nPos, CScrollBar* pScrollBar)
{
    if (reinterpret_cast<CSliderCtrl*>(pScrollBar) == &m_Slider)
    {
        const int pos = m_Slider.GetPos();
        ASSERT(pos > 0);
        ASSERT(pos <= TREELISTCOLORCOUNT);

        m_FileTreeColorCount = pos;
        EnableButtons();
        SetModified();
    }
    CPropertyPageEx::OnVScroll(nSBCode, nPos, pScrollBar);
}
