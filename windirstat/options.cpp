// options.cpp	- Implementation of CPersistence, COptions and CRegistryUser
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include "dirstatdoc.h"
#include "options.h"

namespace
{
	COptions _theOptions;

	const LPCTSTR sectionPersistence		= _T("persistence");
	const LPCTSTR entryShowFreeSpace		= _T("showFreeSpace");
	const LPCTSTR entryShowUnknown			= _T("showUnknown");
	const LPCTSTR entryShowFileTypes		= _T("showFileTypes");
	const LPCTSTR entryShowTreemap			= _T("showTreemap");
	const LPCTSTR entryShowToolbar			= _T("showToolbar");
	const LPCTSTR entryShowStatusbar		= _T("showStatusbar");
	const LPCTSTR entryMainWindowPlacement	= _T("mainWindowPlacement");
	const LPCTSTR entrySplitterPosS			= _T("%s-splitterPos");
	const LPCTSTR entryColumnOrderS			= _T("%s-columnOrder");
	const LPCTSTR entryColumnWidthsS		= _T("%s-columnWidths");
	const LPCTSTR entryDialogRectangleS		= _T("%s-rectangle");
//	const LPCTSTR entrySortingColumnSN		= _T("%s-sortColumn%d");
//	const LPCTSTR entrySortingAscendingSN	= _T("%s-sortAscending%d");
	const LPCTSTR entryConfigPage			= _T("configPage");
	const LPCTSTR entryConfigPositionX		= _T("configPositionX");
	const LPCTSTR entryConfigPositionY		= _T("configPositionY");
	const LPCTSTR entrySelectDrivesRadio	= _T("selectDrivesRadio");
	const LPCTSTR entrySelectDrivesFolder	= _T("selectDrivesFolder");
	const LPCTSTR entrySelectDrivesDrives	= _T("selectDrivesDrives");
	const LPCTSTR entryShowDeleteWarning	= _T("showDeleteWarning");
	const LPCTSTR sectionBarState			= _T("persistence\\barstate");

	const LPCTSTR entryLanguage				= _T("language");

	const LPCTSTR sectionOptions			= _T("options");
	const LPCTSTR entryTreelistGrid			= _T("treelistGrid");
	const LPCTSTR entryTreelistColorCount	= _T("treelistColorCount");
	const LPCTSTR entryTreelistColorN		= _T("treelistColor%d");
	const LPCTSTR entryHumanFormat			= _T("humanFormat");
	const LPCTSTR entryPacmanAnimation		= _T("pacmanAnimation");
	const LPCTSTR entryCushionShading		= _T("cushionShading");
	const LPCTSTR entryTreemapGrid			= _T("treemapGrid");
	const LPCTSTR entryTreemapGridColor		= _T("treemapGridColor");
	const LPCTSTR entryTreemapHighlightColor= _T("treemapHighlightColor");
	const LPCTSTR entryHeightFactor			= _T("heightFactor");
	const LPCTSTR entryScaleFactor			= _T("scaleFactor");
	const LPCTSTR entryAmbientLight			= _T("ambientLight");
	const LPCTSTR entryFollowMountPoints	= _T("followMountPoints");

	const LPCTSTR sectionUserDefinedCleanupD	= _T("options\\userDefinedCleanup%02d");
	const LPCTSTR entryEnabled					= _T("enabled");
	const LPCTSTR entryTitle					= _T("title");
	const LPCTSTR entryWorksForDrives			= _T("worksForDrives");
	const LPCTSTR entryWorksForDirectories		= _T("worksForDirectories");
	const LPCTSTR entryWorksForFilesFolder		= _T("worksForFilesFolder");
	const LPCTSTR entryWorksForFiles			= _T("worksForFiles");
	const LPCTSTR entryCommandLine				= _T("commandLine");
	const LPCTSTR entryRecurseIntoSubdirectories= _T("recurseIntoSubdirectories");
	const LPCTSTR entryAskForConfirmation		= _T("askForConfirmation");
	const LPCTSTR entryShowConsoleWindow		= _T("showConsoleWindow");
	const LPCTSTR entryWaitForCompletion		= _T("waitForCompletion");
	const LPCTSTR entryRefreshPolicy			= _T("refreshPolicy");

	COLORREF treelistColorDefault[TREELISTCOLORCOUNT] = {
		RGB(0, 128, 192),
		RGB(255, 128, 128),
		RGB(128, 255, 128),
		RGB(180, 180, 0),
		RGB(0, 0, 255),
		RGB(255, 0, 0),
		RGB(0, 255, 0),
		RGB(255, 255, 0)
	};

}

/////////////////////////////////////////////////////////////////////////////
bool CPersistence::GetShowFreeSpace()
{
	return GetProfileBool(sectionPersistence, entryShowFreeSpace, false);
}

void CPersistence::SetShowFreeSpace(bool show)
{
	SetProfileBool(sectionPersistence, entryShowFreeSpace, show);
}

bool CPersistence::GetShowUnknown()
{
	return GetProfileBool(sectionPersistence, entryShowUnknown, false);
}

void CPersistence::SetShowUnknown(bool show)
{
	SetProfileBool(sectionPersistence, entryShowUnknown, show);
}

bool CPersistence::GetShowFileTypes()
{
	return GetProfileBool(sectionPersistence, entryShowFileTypes, true);
}

void CPersistence::SetShowFileTypes(bool show)
{
	SetProfileBool(sectionPersistence, entryShowFileTypes, show);
}

bool CPersistence::GetShowTreemap()
{
	return GetProfileBool(sectionPersistence, entryShowTreemap, true);
}

void CPersistence::SetShowTreemap(bool show)
{
	SetProfileBool(sectionPersistence, entryShowTreemap, show);
}

bool CPersistence::GetShowToolbar()
{
	return GetProfileBool(sectionPersistence, entryShowToolbar, true);
}

void CPersistence::SetShowToolbar(bool show)
{
	SetProfileBool(sectionPersistence, entryShowToolbar, show);
}

bool CPersistence::GetShowStatusbar()
{
	return GetProfileBool(sectionPersistence, entryShowStatusbar, true);
}

void CPersistence::SetShowStatusbar(bool show)
{
	SetProfileBool(sectionPersistence, entryShowStatusbar, show);
}

void CPersistence::GetMainWindowPlacement(/* [in/out] */ WINDOWPLACEMENT& wp)
{
	ASSERT(wp.length == sizeof(wp));
	CString s= GetProfileString(sectionPersistence, entryMainWindowPlacement, _T(""));
	DecodeWindowPlacement(s, wp);
	SanifyRect((CRect &)wp.rcNormalPosition);
}

void CPersistence::SetMainWindowPlacement(const WINDOWPLACEMENT& wp)
{
	CString s= EncodeWindowPlacement(wp);
	SetProfileString(sectionPersistence, entryMainWindowPlacement, s);
}

void CPersistence::SetSplitterPos(LPCTSTR name, bool valid, double userpos)
{
	int pos;
	if (valid)
		pos= (int)(userpos * 100);
	else
		pos= -1;

	SetProfileInt(sectionPersistence, MakeSplitterPosEntry(name), pos);
}

void CPersistence::GetSplitterPos(LPCTSTR name, bool& valid, double& userpos)
{
	int pos= GetProfileInt(sectionPersistence, MakeSplitterPosEntry(name), -1);
	if (pos < 0 || pos > 100)
	{
		valid= false;
		userpos= 0.5;
	}
	else
	{
		valid= true;
		userpos= (double)pos / 100;
	}
}

void CPersistence::SetColumnOrder(LPCTSTR name, const CArray<int, int>& arr)
{
	SetArray(MakeColumnOrderEntry(name), arr);
}

void CPersistence::GetColumnOrder(LPCTSTR name, /* in/out */ CArray<int, int>& arr)
{
	GetArray(MakeColumnOrderEntry(name), arr);
}

void CPersistence::SetColumnWidths(LPCTSTR name, const CArray<int, int>& arr)
{
	SetArray(MakeColumnWidthsEntry(name), arr);
}

void CPersistence::GetColumnWidths(LPCTSTR name, /* in/out */ CArray<int, int>& arr)
{
	GetArray(MakeColumnWidthsEntry(name), arr);
}

void CPersistence::SetDialogRectangle(LPCTSTR name, const CRect& rc)
{
	SetRect(MakeDialogRectangleEntry(name), rc);
}

void CPersistence::GetDialogRectangle(LPCTSTR name, CRect& rc)
{
	GetRect(MakeDialogRectangleEntry(name), rc);
	SanifyRect(rc);
}
/*
void CPersistence::SetSorting(LPCTSTR name, int column1, bool ascending1, int column2, bool ascending2)
{
	SetProfileInt(sectionPersistence, MakeSortingColumnEntry(name, 1), column1);
	SetProfileBool(sectionPersistence, MakeSortingAscendingEntry(name, 1), ascending1);
	SetProfileInt(sectionPersistence, MakeSortingColumnEntry(name, 2), column2);
	SetProfileBool(sectionPersistence, MakeSortingAscendingEntry(name, 2), ascending2);
}

void CPersistence::GetSorting(LPCTSTR name, int columnCount, int& column1, bool& ascending1, int& column2, bool& ascending2)
{
	column1= GetProfileInt(sectionPersistence, MakeSortingColumnEntry(name, 1), column1);
	CheckRange(column1, 0, columnCount - 1);
	ascending1= GetProfileBool(sectionPersistence, MakeSortingAscendingEntry(name, 1), ascending1);

	column2= GetProfileInt(sectionPersistence, MakeSortingColumnEntry(name, 2), column2);
	CheckRange(column2, 0, columnCount - 1);
	ascending2= GetProfileBool(sectionPersistence, MakeSortingAscendingEntry(name, 2), ascending2);
}
*/

int CPersistence::GetConfigPage(int max)
{
	int n= GetProfileInt(sectionPersistence, entryConfigPage, 0);
	CheckRange(n, 0, max);
	return n;
}

void CPersistence::SetConfigPage(int page)
{
	SetProfileInt(sectionPersistence, entryConfigPage, page);
}

void CPersistence::GetConfigPosition(/* in/out */ CPoint& pt)
{
	pt.x= GetProfileInt(sectionPersistence, entryConfigPositionX, pt.x);
	pt.y= GetProfileInt(sectionPersistence, entryConfigPositionY, pt.y);
	
	CRect rc(pt, CSize(100, 100));
	SanifyRect(rc);
	pt= rc.TopLeft();
}

void CPersistence::SetConfigPosition(CPoint pt)
{
	SetProfileInt(sectionPersistence, entryConfigPositionX, pt.x);
	SetProfileInt(sectionPersistence, entryConfigPositionY, pt.y);
}

CString CPersistence::GetBarStateSection()
{
	return sectionBarState;
}

int CPersistence::GetSelectDrivesRadio()
{
	int radio= GetProfileInt(sectionPersistence, entrySelectDrivesRadio, 0);
	CheckRange(radio, 0, 2);
	return radio;
}

void CPersistence::SetSelectDrivesRadio(int radio)
{
	SetProfileInt(sectionPersistence, entrySelectDrivesRadio, radio);
}

CString CPersistence::GetSelectDrivesFolder()
{
	return GetProfileString(sectionPersistence, entrySelectDrivesFolder, _T(""));
}

void CPersistence::SetSelectDrivesFolder(LPCTSTR folder)
{
	SetProfileString(sectionPersistence, entrySelectDrivesFolder, folder);
}

void CPersistence::GetSelectDrivesDrives(CStringArray& drives)
{
	drives.RemoveAll();
	CString s= GetProfileString(sectionPersistence, entrySelectDrivesDrives, _T(""));
	int i=0;
	while (i < s.GetLength())
	{
		CString drive;
		while (i < s.GetLength() && s[i] != _T('|'))
		{
			drive+= s[i];
			i++;
		}
		if (i < s.GetLength())
			i++;
		drives.Add(drive);
	}
}

void CPersistence::SetSelectDrivesDrives(const CStringArray& drives)
{
	CString s;
	for (int i=0; i < drives.GetSize(); i++)
	{
		if (i > 0)
			s+= _T("|");
		s+= drives[i];
	}
	SetProfileString(sectionPersistence, entrySelectDrivesDrives, s);
}

bool CPersistence::GetShowDeleteWarning()
{
	return GetProfileBool(sectionPersistence, entryShowDeleteWarning, true);
}

void CPersistence::SetShowDeleteWarning(bool show)
{
	SetProfileBool(sectionPersistence, entryShowDeleteWarning, show);
}

void CPersistence::SetArray(LPCTSTR entry, const CArray<int, int>& arr)
{
	CString value;
	for (int i=0; i < arr.GetSize(); i++)
	{
		CString s;
		s.Format(_T("%d"), arr[i]);
		if (i > 0)
			value+= _T(",");
		value+= s;
	}
	SetProfileString(sectionPersistence, entry, value);
}

void CPersistence::GetArray(LPCTSTR entry, /* in/out */ CArray<int, int>& rarr)
{
	CString s= GetProfileString(sectionPersistence, entry, _T(""));
	CArray<int, int> arr;
	int i=0;
	while (i < s.GetLength())
	{
		int n= 0;
		while (i < s.GetLength() && isdigit(s[i]))
		{
			n*= 10;
			n+= s[i] - _T('0');
			i++;
		}
		arr.Add(n);
		if (i >= s.GetLength() || s[i] != _T(','))
			break;
		i++;
	}
	if (i >= s.GetLength() && arr.GetSize() == rarr.GetSize())
	{
		for (i=0; i < rarr.GetSize(); i++)
			rarr[i]= arr[i];
	}
}

void CPersistence::SetRect(LPCTSTR entry, const CRect& rc)
{
	CString s;
	s.Format(_T("%d,%d,%d,%d"), rc.left, rc.top, rc.right, rc.bottom);
	SetProfileString(sectionPersistence, entry, s);
}

void CPersistence::GetRect(LPCTSTR entry, CRect& rc)
{
	CString s= GetProfileString(sectionPersistence, entry, _T(""));
	CRect tmp;
	int r= _stscanf(s, _T("%d,%d,%d,%d"), &tmp.left, &tmp.top, &tmp.right, &tmp.bottom);
	if (r == 4)
		rc= tmp;
}

void CPersistence::SanifyRect(CRect& rc)
{
	const int visible = 30;

	rc.NormalizeRect();

	CRect rcDesktop;
	CWnd::GetDesktopWindow()->GetWindowRect(rcDesktop);
	
	if (rc.Width() > rcDesktop.Width())
		rc.right= rc.left + rcDesktop.Width();
	if (rc.Height() > rcDesktop.Height())
		rc.bottom= rc.top + rcDesktop.Height();
	
	if (rc.left < 0)
		rc.OffsetRect(-rc.left, 0);
	if (rc.left > rcDesktop.right - visible)
		rc.OffsetRect(-visible, 0);
	
	if (rc.top < 0)
		rc.OffsetRect(-rc.top, 0);
	if (rc.top > rcDesktop.bottom - visible)
		rc.OffsetRect(0, -visible);
}

CString CPersistence::MakeSplitterPosEntry(LPCTSTR name)
{
	CString entry;
	entry.Format(entrySplitterPosS, name);
	return entry;
}

CString CPersistence::MakeColumnOrderEntry(LPCTSTR name)
{
	CString entry;
	entry.Format(entryColumnOrderS, name);
	return entry;
}

CString CPersistence::MakeDialogRectangleEntry(LPCTSTR name)
{
	CString entry;
	entry.Format(entryDialogRectangleS, name);
	return entry;
}

CString CPersistence::MakeColumnWidthsEntry(LPCTSTR name)
{
	CString entry;
	entry.Format(entryColumnWidthsS, name);
	return entry;
}

/*
CString CPersistence::MakeSortingColumnEntry(LPCTSTR name, int n)
{
	CString entry;
	entry.Format(entrySortingColumnSN, name, n);
	return entry;
}

CString CPersistence::MakeSortingAscendingEntry(LPCTSTR name, int n)
{
	CString entry;
	entry.Format(entrySortingAscendingSN, name, n);
	return entry;
}
*/

CString CPersistence::EncodeWindowPlacement(const WINDOWPLACEMENT& wp)
{
	CString s;
	s.Format(
		_T("%u,%u,")
		_T("%ld,%ld,%ld,%ld,")
		_T("%ld,%ld,%ld,%ld"),
		wp.flags, wp.showCmd, 
		wp.ptMinPosition.x, wp.ptMinPosition.y, wp.ptMaxPosition.x, wp.ptMaxPosition.y,
		wp.rcNormalPosition.left, wp.rcNormalPosition.right, wp.rcNormalPosition.top, wp.rcNormalPosition.bottom
	);
	return s;
}

void CPersistence::DecodeWindowPlacement(const CString& s, WINDOWPLACEMENT& rwp)
{
	WINDOWPLACEMENT wp;
	wp.length= sizeof(wp);

	int r= _stscanf(s, 		
		_T("%u,%u,")
		_T("%ld,%ld,%ld,%ld,")
		_T("%ld,%ld,%ld,%ld"),
		&wp.flags, &wp.showCmd, 
		&wp.ptMinPosition.x, &wp.ptMinPosition.y, &wp.ptMaxPosition.x, &wp.ptMaxPosition.y,
		&wp.rcNormalPosition.left, &wp.rcNormalPosition.right, &wp.rcNormalPosition.top, &wp.rcNormalPosition.bottom
	);

	if (r == 10)
		rwp= wp;
}


/////////////////////////////////////////////////////////////////////////////

LANGID CLanguageOptions::GetLanguage()
{
	LANGID defaultLangid= LANGIDFROMLCID(GetUserDefaultLCID());
	LANGID id= (LANGID)GetProfileInt(sectionOptions, entryLanguage, defaultLangid);
	return id;
}

void CLanguageOptions::SetLanguage(LANGID langid)
{
	SetProfileInt(sectionOptions, entryLanguage, langid);
}


/////////////////////////////////////////////////////////////////////////////
COptions *GetOptions()
{
	return &_theOptions;
}


COptions::COptions()
{
	m_treelistGrid= true;
}

bool COptions::IsTreelistGrid()
{
	return m_treelistGrid;
}

void COptions::SetTreelistGrid(bool show)
{
	if (m_treelistGrid != show)
	{
		m_treelistGrid= show;
		GetDocument()->UpdateAllViews(NULL, HINT_TREELISTSTYLECHANGED);
	}
}

void COptions::GetTreelistColors(COLORREF color[TREELISTCOLORCOUNT])
{
	for (int i=0; i < TREELISTCOLORCOUNT; i++)
		color[i]= m_treelistColor[i];
}

void COptions::SetTreelistColors(const COLORREF color[TREELISTCOLORCOUNT])
{
	for (int i=0; i < TREELISTCOLORCOUNT; i++)
		m_treelistColor[i]= color[i];
	GetDocument()->UpdateAllViews(NULL, HINT_TREELISTSTYLECHANGED);
}

COLORREF COptions::GetTreelistColor(int i)
{
	ASSERT(i >= 0);
	ASSERT(i < m_treelistColorCount);
	return m_treelistColor[i];
}

int COptions::GetTreelistColorCount()
{
	return m_treelistColorCount;
}

void COptions::SetTreelistColorCount(int count)
{
	if (m_treelistColorCount != count)
	{
		m_treelistColorCount= count;
		GetDocument()->UpdateAllViews(NULL, HINT_TREELISTSTYLECHANGED);
	}
}

bool COptions::IsHumanFormat()
{
	return m_humanFormat;
}

void COptions::SetHumanFormat(bool human)
{
	if (m_humanFormat != human)
	{
		m_humanFormat= human;
		GetDocument()->UpdateAllViews(NULL, HINT_NULL);
		GetApp()->UpdateRamUsage();
	}
}

bool COptions::IsPacmanAnimation()
{
	return m_pacmanAnimation;
}

void COptions::SetPacmanAnimation(bool animate)
{
	if (m_pacmanAnimation != animate)
	{
		m_pacmanAnimation= animate;
	}
}

bool COptions::IsCushionShading()
{
	return m_cushionShading;
}

void COptions::SetCushionShading(bool shade)
{
	if (m_cushionShading != shade)
	{
		m_cushionShading= shade;
		GetDocument()->UpdateAllViews(NULL, HINT_CUSHIONSHADINGCHANGED);
	}
}

bool COptions::IsTreemapGrid()
{
	return m_treemapGrid;
}

void COptions::SetTreemapGrid(bool show)
{
	if (m_treemapGrid != show)
	{
		m_treemapGrid= show;
		GetDocument()->UpdateAllViews(NULL, HINT_TREEMAPSTYLECHANGED);
	}
}

COLORREF COptions::GetTreemapGridColor()
{
	return m_treemapGridColor;
}

void COptions::SetTreemapGridColor(COLORREF color)
{
	if (m_treemapGridColor != color)
	{
		m_treemapGridColor= color;
		GetDocument()->UpdateAllViews(NULL, HINT_TREEMAPSTYLECHANGED);
	}
}

COLORREF COptions::GetTreemapHighlightColor()
{
	return m_treemapHighlightColor;
}

void COptions::SetTreemapHighlightColor(COLORREF color)
{
	if (m_treemapHighlightColor != color)
	{
		m_treemapHighlightColor= color;
		GetDocument()->UpdateAllViews(NULL, HINT_SELECTIONCHANGED);
	}
}

int COptions::GetHeightFactor() // "H"
{
	return m_heightFactor;
}

void COptions::SetHeightFactor(int h)
{
	if (m_heightFactor != h)
	{
		m_heightFactor= h;
		GetDocument()->UpdateAllViews(NULL, HINT_TREEMAPSTYLECHANGED);
	}
}

int COptions::GetHeightFactorDefault()
{
	return 40;
}

void COptions::GetHeightFactorRange(int& min, int& max)
{
	min= 0;
	max= 200;
}


int COptions::GetScaleFactor() // "F"
{
	return m_scaleFactor;
}

void COptions::SetScaleFactor(int f)
{
	if (m_scaleFactor != f)
	{
		m_scaleFactor= f;
		GetDocument()->UpdateAllViews(NULL, HINT_TREEMAPSTYLECHANGED);
	}
}

int COptions::GetScaleFactorDefault()
{
	return 90;
}

void COptions::GetScaleFactorRange(int& min, int& max)
{
	min= 20;
	max= 100;
}

int COptions::GetAmbientLight()
{
	return m_ambientLight;
}

void COptions::SetAmbientLight(int a)
{
	if (m_ambientLight != a)
	{
		m_ambientLight= a;
		GetDocument()->UpdateAllViews(NULL, HINT_TREEMAPSTYLECHANGED);
	}
}

int COptions::GetAmbientLightDefault()
{
	return 15;
}

void COptions::GetAmbientLightRange(int& min, int& max)
{
	min= 0;
	max= 100;
}

void COptions::GetUserDefinedCleanups(USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT])
{
	for (int i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		udc[i]= m_userDefinedCleanup[i];
}

void COptions::SetUserDefinedCleanups(const USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT])
{
	for (int i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		m_userDefinedCleanup[i]= udc[i];
}

void COptions::GetEnabledUserDefinedCleanups(CArray<int, int>& indices)
{
	indices.RemoveAll();
	for (int i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		if (m_userDefinedCleanup[i].enabled)
			indices.Add(i);
}

bool COptions::IsUserDefinedCleanupEnabled(int i)
{
	ASSERT(i >= 0);
	ASSERT(i < USERDEFINEDCLEANUPCOUNT);
	return m_userDefinedCleanup[i].enabled;
}

const USERDEFINEDCLEANUP *COptions::GetUserDefinedCleanup(int i)
{
	ASSERT(i >= 0);
	ASSERT(i < USERDEFINEDCLEANUPCOUNT);
	ASSERT(m_userDefinedCleanup[i].enabled);

	return &m_userDefinedCleanup[i];
}

bool COptions::IsFollowMountPoints()
{
	return m_followMountPoints;
}

void COptions::SetFollowMountPoints(bool follow)
{
	if (m_followMountPoints != follow)
	{
		m_followMountPoints= follow;
		GetDocument()->RefreshMountPointItems();
	}
}

void COptions::SaveToRegistry()
{
	SetProfileBool(sectionOptions, entryTreelistGrid, m_treelistGrid);
	SetProfileInt(sectionOptions, entryTreelistColorCount, m_treelistColorCount);
	for (int i=0; i < TREELISTCOLORCOUNT; i++)
	{
		CString entry;
		entry.Format(entryTreelistColorN, i);
		SetProfileInt(sectionOptions, entry, m_treelistColor[i]);
	}
	SetProfileBool(sectionOptions, entryHumanFormat, m_humanFormat);
	SetProfileBool(sectionOptions, entryPacmanAnimation, m_pacmanAnimation);
	SetProfileBool(sectionOptions, entryCushionShading, m_cushionShading);
	SetProfileBool(sectionOptions, entryTreemapGrid, m_treemapGrid);
	SetProfileInt(sectionOptions, entryTreemapGridColor, m_treemapGridColor);
	SetProfileInt(sectionOptions, entryTreemapHighlightColor, m_treemapHighlightColor);
	SetProfileInt(sectionOptions, entryHeightFactor, m_heightFactor);
	SetProfileInt(sectionOptions, entryScaleFactor, m_scaleFactor);
	SetProfileInt(sectionOptions, entryAmbientLight, m_ambientLight);
	SetProfileBool(sectionOptions, entryFollowMountPoints, m_followMountPoints);
	for (i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		SaveUserDefinedCleanup(i);
}

void COptions::LoadFromRegistry()
{
	int min;
	int max;
	m_treelistGrid= GetProfileBool(sectionOptions, entryTreelistGrid, false);
	m_treelistColorCount= GetProfileInt(sectionOptions, entryTreelistColorCount, 4);
	CheckRange(m_treelistColorCount, 1, TREELISTCOLORCOUNT);
	for (int i=0; i < TREELISTCOLORCOUNT; i++)
	{
		CString entry;
		entry.Format(entryTreelistColorN, i);
		m_treelistColor[i]= GetProfileInt(sectionOptions, entry, treelistColorDefault[i]);
	}
	m_humanFormat= GetProfileBool(sectionOptions, entryHumanFormat, true);
	m_pacmanAnimation= GetProfileBool(sectionOptions, entryPacmanAnimation, true);
	m_cushionShading= GetProfileBool(sectionOptions, entryCushionShading, true);
	m_treemapGrid= GetProfileBool(sectionOptions, entryTreemapGrid, false);
	m_treemapGridColor= GetProfileInt(sectionOptions, entryTreemapGridColor, RGB(0, 0, 0));
	m_treemapHighlightColor= GetProfileInt(sectionOptions, entryTreemapHighlightColor, RGB(0,255,255));
	m_heightFactor= GetProfileInt(sectionOptions, entryHeightFactor, GetHeightFactorDefault());
	GetHeightFactorRange(min, max);
	CheckRange(m_heightFactor, min, max);
	m_scaleFactor= GetProfileInt(sectionOptions, entryScaleFactor, GetScaleFactorDefault());
	GetScaleFactorRange(min, max);
	CheckRange(m_scaleFactor, min, max);
	m_ambientLight= GetProfileInt(sectionOptions, entryAmbientLight, GetAmbientLightDefault());
	GetAmbientLightRange(min, max);
	CheckRange(m_ambientLight, min, max);
	m_followMountPoints= GetProfileBool(sectionOptions, entryFollowMountPoints, false);
	for (i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		ReadUserDefinedCleanup(i);
}

void COptions::ReadUserDefinedCleanup(int i)
{
	CString section;
	section.Format(sectionUserDefinedCleanupD, i);

	CString defaultTitle;
	defaultTitle.FormatMessage(IDS_USERDEFINEDCLEANUPd, i);

	m_userDefinedCleanup[i].enabled= GetProfileBool(section, entryEnabled, false);
	m_userDefinedCleanup[i].title= GetProfileString(section, entryTitle, defaultTitle);
	m_userDefinedCleanup[i].worksForDrives= GetProfileBool(section, entryWorksForDrives, false);
	m_userDefinedCleanup[i].worksForDirectories= GetProfileBool(section, entryWorksForDirectories, false);
	m_userDefinedCleanup[i].worksForFilesFolder= GetProfileBool(section, entryWorksForFilesFolder, false);
	m_userDefinedCleanup[i].worksForFiles= GetProfileBool(section, entryWorksForFiles, false);
	m_userDefinedCleanup[i].commandLine= GetProfileString(section, entryCommandLine, _T(""));
	m_userDefinedCleanup[i].recurseIntoSubdirectories= GetProfileBool(section, entryRecurseIntoSubdirectories, false);
	m_userDefinedCleanup[i].askForConfirmation= GetProfileBool(section, entryAskForConfirmation, true);
	m_userDefinedCleanup[i].showConsoleWindow= GetProfileBool(section, entryShowConsoleWindow, true);
	m_userDefinedCleanup[i].waitForCompletion= GetProfileBool(section, entryWaitForCompletion, true);
	int r= GetProfileInt(section, entryRefreshPolicy, RP_NO_REFRESH);
	CheckRange(r, 0, REFRESHPOLICYCOUNT);
	m_userDefinedCleanup[i].refreshPolicy= (REFRESHPOLICY)r;
}

void COptions::SaveUserDefinedCleanup(int i)
{
	CString section;
	section.Format(sectionUserDefinedCleanupD, i);

	SetProfileBool(section, entryEnabled, m_userDefinedCleanup[i].enabled);
	SetProfileString(section, entryTitle, m_userDefinedCleanup[i].title);
	SetProfileBool(section, entryWorksForDrives, m_userDefinedCleanup[i].worksForDrives);
	SetProfileBool(section, entryWorksForDirectories, m_userDefinedCleanup[i].worksForDirectories);
	SetProfileBool(section, entryWorksForFilesFolder, m_userDefinedCleanup[i].worksForFilesFolder);
	SetProfileBool(section, entryWorksForFiles, m_userDefinedCleanup[i].worksForFiles);
	SetProfileString(section, entryCommandLine, m_userDefinedCleanup[i].commandLine);
	SetProfileBool(section, entryRecurseIntoSubdirectories, m_userDefinedCleanup[i].recurseIntoSubdirectories);
	SetProfileBool(section, entryAskForConfirmation, m_userDefinedCleanup[i].askForConfirmation);
	SetProfileBool(section, entryShowConsoleWindow, m_userDefinedCleanup[i].showConsoleWindow);
	SetProfileBool(section, entryWaitForCompletion, m_userDefinedCleanup[i].waitForCompletion);
	SetProfileInt(section, entryRefreshPolicy, m_userDefinedCleanup[i].refreshPolicy);
}




/////////////////////////////////////////////////////////////////////////////

void CRegistryUser::SetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value)
{
	AfxGetApp()->WriteProfileString(section, entry, value);
}

CString CRegistryUser::GetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue)
{
	return AfxGetApp()->GetProfileString(section, entry, defaultValue);
}

void CRegistryUser::SetProfileInt(LPCTSTR section, LPCTSTR entry, int value)
{
	AfxGetApp()->WriteProfileInt(section, entry, value);
}

int CRegistryUser::GetProfileInt(LPCTSTR section, LPCTSTR entry, int defaultValue)
{
	return AfxGetApp()->GetProfileInt(section, entry, defaultValue);
}

void CRegistryUser::SetProfileBool(LPCTSTR section, LPCTSTR entry, bool value)
{
	SetProfileInt(section, entry, (int)value);
}

bool CRegistryUser::GetProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue)
{
	return GetProfileInt(section, entry, defaultValue) != 0;
}

void CRegistryUser::CheckRange(int& value, int min, int max)
{
	if (value < min)
		value= min;
	if (value > max)
		value= max;
}

