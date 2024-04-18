// PageTreelist.cpp - Implementation of CPageTreelist
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
#include "WinDirStat.h"
#include "Options.h"
#include "PageTreelist.h"
#include "DirStatDoc.h"
#include "FileTreeView.h"
#include "Localization.h"
#include "MainFrame.h"

IMPLEMENT_DYNAMIC(CPageTreelist, CPropertyPage)

CPageTreelist::CPageTreelist() : CPropertyPage(CPageTreelist::IDD) {}

void CPageTreelist::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_PACMANANIMATION, m_pacmanAnimation);
    DDX_Check(pDX, IDC_SHOWTIMESPENT, m_showTimeSpent);
    DDX_Check(pDX, IDC_TREECOL_FOLDERS, m_showColumnFolders);
    DDX_Check(pDX, IDC_TREECOL_SIZE_PHYSICAL, m_showColumnSizePhysical);
    DDX_Check(pDX, IDC_TREECOL_SIZE_LOGICAL, m_showColumnSizeLogical);
    DDX_Check(pDX, IDC_TREECOL_ITEMS, m_showColumnItems);
    DDX_Check(pDX, IDC_TREECOL_FILES, m_showColumnFiles);
    DDX_Check(pDX, IDC_TREECOL_ATTRIBUTES, m_showColumnAttributes);
    DDX_Check(pDX, IDC_TREECOL_LASTCHANGE, m_showColumnLastChange);
    DDX_Check(pDX, IDC_TREECOL_OWNER, m_showColumnOwner);
    for (int i = 0; i < TREELISTCOLORCOUNT; i++)
    {
        DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_colorButton[i]);
        if (pDX->m_bSaveAndValidate)
        {
            m_treeListColor[i] = m_colorButton[i].GetColor();
        }
        else
        {
            m_colorButton[i].SetColor(m_treeListColor[i]);
        }
    }
    DDX_Control(pDX, IDC_SLIDER, m_slider);
}

BEGIN_MESSAGE_MAP(CPageTreelist, CPropertyPage)
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

BOOL CPageTreelist::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_pacmanAnimation= COptions::PacmanAnimation;
    m_showTimeSpent = COptions::ShowTimeSpent;
    m_showColumnFolders = COptions::ShowColumnFolders;
    m_showColumnItems = COptions::ShowColumnItems;
    m_showColumnFiles = COptions::ShowColumnFiles;
    m_showColumnAttributes = COptions::ShowColumnAttributes;
    m_showColumnLastChange = COptions::ShowColumnLastChange;
    m_showColumnOwner = COptions::ShowColumnOwner;
    m_showColumnSizePhysical = COptions::ShowColumnSizePhysical;
    m_showColumnSizeLogical = COptions::ShowColumnSizeLogical;

    m_treeListColorCount = COptions::TreeListColorCount;
    m_treeListColor[0] = COptions::TreeListColor0;
    m_treeListColor[1] = COptions::TreeListColor1;
    m_treeListColor[2] = COptions::TreeListColor2;
    m_treeListColor[3] = COptions::TreeListColor3;
    m_treeListColor[4] = COptions::TreeListColor4;
    m_treeListColor[5] = COptions::TreeListColor5;
    m_treeListColor[6] = COptions::TreeListColor6;
    m_treeListColor[7] = COptions::TreeListColor7;

    m_slider.SetRange(1, TREELISTCOLORCOUNT);
    m_slider.SetPos(m_treeListColorCount);

    EnableButtons();
    UpdateData(false);
    return TRUE;
}

void CPageTreelist::OnOK()
{
    const bool cols_changed =
        COptions::ShowColumnFolders != (FALSE != m_showColumnFolders) ||
        COptions::ShowColumnItems != (FALSE != m_showColumnItems) ||
        COptions::ShowColumnFiles != (FALSE != m_showColumnFiles) ||
        COptions::ShowColumnAttributes != (FALSE != m_showColumnAttributes) ||
        COptions::ShowColumnLastChange != (FALSE != m_showColumnLastChange) ||
        COptions::ShowColumnOwner != (FALSE != m_showColumnOwner) ||
        COptions::ShowColumnSizePhysical != (FALSE != m_showColumnSizePhysical) ||
        COptions::ShowColumnSizeLogical != (FALSE != m_showColumnSizeLogical);

    UpdateData();
    COptions::PacmanAnimation = (FALSE != m_pacmanAnimation);
    COptions::ShowTimeSpent = (FALSE != m_showTimeSpent);
    COptions::ShowColumnFolders = (FALSE != m_showColumnFolders);
    COptions::ShowColumnItems = (FALSE != m_showColumnItems);
    COptions::ShowColumnFiles = (FALSE != m_showColumnFiles);
    COptions::ShowColumnAttributes = (FALSE != m_showColumnAttributes);
    COptions::ShowColumnLastChange = (FALSE != m_showColumnLastChange);
    COptions::ShowColumnOwner = (FALSE != m_showColumnOwner);
    COptions::ShowColumnSizePhysical = (FALSE != m_showColumnSizePhysical);
    COptions::ShowColumnSizeLogical = (FALSE != m_showColumnSizeLogical);
    COptions::TreeListColorCount = m_treeListColorCount;
    COptions::TreeListColor0 = m_treeListColor[0];
    COptions::TreeListColor1 = m_treeListColor[1];
    COptions::TreeListColor2 = m_treeListColor[2];
    COptions::TreeListColor3 = m_treeListColor[3];
    COptions::TreeListColor4 = m_treeListColor[4];
    COptions::TreeListColor5 = m_treeListColor[5];
    COptions::TreeListColor6 = m_treeListColor[6];
    COptions::TreeListColor7 = m_treeListColor[7];
    if (cols_changed) CMainFrame::Get()->GetFileTreeView()->CreateColumns();
    GetDocument()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    CPropertyPage::OnOK();
}

void CPageTreelist::OnBnClickedSetModified()
{
    SetModified();
}

void CPageTreelist::OnColorChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}

void CPageTreelist::EnableButtons()
{
    int i = 0;
    for (; i < m_treeListColorCount; i++)
    {
        m_colorButton[i].EnableWindow(true);
    }
    for (; i < TREELISTCOLORCOUNT; i++)
    {
        m_colorButton[i].EnableWindow(false);
    }
}

void CPageTreelist::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (reinterpret_cast<CSliderCtrl*>(pScrollBar) == &m_slider)
    {
        const int pos = m_slider.GetPos();
        ASSERT(pos > 0);
        ASSERT(pos <= TREELISTCOLORCOUNT);

        m_treeListColorCount = pos;
        EnableButtons();
        SetModified();
    }
    CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}
