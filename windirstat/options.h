// options.h		- Declaration of CRegistryUser, COptions and CPersistence
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

#pragma once

class COptions;

enum REFRESHPOLICY
{
	RP_NO_REFRESH,
	RP_REFRESH_THIS_ENTRY,
	RP_REFRESH_THIS_ENTRYS_PARENT,
	// RP_ASSUME_ENTRY_HAS_BEEN_DELETED, // feature not implemented.
	REFRESHPOLICYCOUNT
};

struct USERDEFINEDCLEANUP
{
	bool enabled;
	CString title;
	bool worksForDrives;
	bool worksForDirectories;
	bool worksForFilesFolder;
	bool worksForFiles;
	bool worksForUncPaths;
	CString commandLine;
	bool recurseIntoSubdirectories;
	bool askForConfirmation;
	bool showConsoleWindow;
	bool waitForCompletion;
	REFRESHPOLICY refreshPolicy;
};

#define USERDEFINEDCLEANUPCOUNT 10

#define TREELISTCOLORCOUNT 8


//
// CRegistryUser. (Base class for COptions and CPersistence.)
// Can read from and write to the registry.
//
class CRegistryUser
{
public:
	static void SetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value);
	static CString GetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue);

	static void SetProfileInt(LPCTSTR section, LPCTSTR entry, int value);
	static int GetProfileInt(LPCTSTR section, LPCTSTR entry, int defaultValue);

	static void SetProfileBool(LPCTSTR section, LPCTSTR entry, bool value);
	static bool GetProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue);

	static void CheckRange(int& value, int min, int max);
};


//
// CPersistence. Reads from and writes to the registry all the persistent settings
// like window position, column order etc.
//
class CPersistence: private CRegistryUser
{
public:
	static bool GetShowFreeSpace();
	static void SetShowFreeSpace(bool show);

	static bool GetShowUnknown();
	static void SetShowUnknown(bool show);

	static bool GetShowFileTypes();
	static void SetShowFileTypes(bool show);

	static bool GetShowTreemap();
	static void SetShowTreemap(bool show);

	static bool GetShowToolbar();
	static void SetShowToolbar(bool show);

	static bool GetShowStatusbar();
	static void SetShowStatusbar(bool show);

	static void GetMainWindowPlacement(/* [in/out] */ WINDOWPLACEMENT& wp);
	static void SetMainWindowPlacement(const WINDOWPLACEMENT& wp);

	static void SetSplitterPos(LPCTSTR name, bool valid, double userpos);
	static void GetSplitterPos(LPCTSTR name, bool& valid, double& userpos);

	static void SetColumnOrder(LPCTSTR name, const CArray<int, int>& arr);
	static void GetColumnOrder(LPCTSTR name, /* in/out */ CArray<int, int>& arr);

	static void SetColumnWidths(LPCTSTR name, const CArray<int, int>& arr);
	static void GetColumnWidths(LPCTSTR name, /* in/out */ CArray<int, int>& arr);

	static void SetDialogRectangle(LPCTSTR name, const CRect& rc);
	static void GetDialogRectangle(LPCTSTR name, CRect& rc);

//	static void SetSorting(LPCTSTR name, int column1, bool ascending1, int column2, bool ascending2);
//	static void GetSorting(LPCTSTR name, int columnCount, int& column1, bool& ascending1, int& column2, bool& ascending2);

	static int GetConfigPage(int max);
	static void SetConfigPage(int page);

	static void GetConfigPosition(/* in/out */ CPoint& pt);
	static void SetConfigPosition(CPoint pt);

	static CString GetBarStateSection();

	static int GetSelectDrivesRadio();
	static void SetSelectDrivesRadio(int radio);

	static CString GetSelectDrivesFolder();
	static void SetSelectDrivesFolder(LPCTSTR folder);

	static void GetSelectDrivesDrives(CStringArray& drives);
	static void SetSelectDrivesDrives(const CStringArray& drives);

	static bool GetShowDeleteWarning();
	static void SetShowDeleteWarning(bool show);

private:
	static void SetArray(LPCTSTR entry, const CArray<int, int>& arr);
	static void GetArray(LPCTSTR entry, /* in/out */ CArray<int, int>& arr);
	static void SetRect(LPCTSTR entry, const CRect& rc);
	static void GetRect(LPCTSTR entry, CRect& rc);
	static void SanifyRect(CRect& rc);

	static CString EncodeWindowPlacement(const WINDOWPLACEMENT& wp);
	static void DecodeWindowPlacement(const CString& s, WINDOWPLACEMENT& wp);
	static CString MakeSplitterPosEntry(LPCTSTR name);
	static CString MakeColumnOrderEntry(LPCTSTR name);
	static CString MakeColumnWidthsEntry(LPCTSTR name);
	//static CString MakeSortingColumnEntry(LPCTSTR name, int n);
	//static CString MakeSortingAscendingEntry(LPCTSTR name, int n);
	static CString MakeDialogRectangleEntry(LPCTSTR name);


};

//
// CLanguageOptions. Is separated from COptions because it
// must be loaded earlier.
//

class CLanguageOptions: private CRegistryUser
{
public:
	static LANGID GetLanguage();
	static void SetLanguage(LANGID langid);
};




//
// COptions. Represents all the data which can be viewed
// and modified in the "Configure WinDirStat" dialog.
//

// COptions is a singleton.
COptions *GetOptions();

class COptions: private CRegistryUser
{
public:
	COptions();

	void LoadFromRegistry();
	void SaveToRegistry();

	bool IsTreelistGrid();
	void SetTreelistGrid(bool show);

	void GetTreelistColors(COLORREF color[TREELISTCOLORCOUNT]);
	void SetTreelistColors(const COLORREF color[TREELISTCOLORCOUNT]);
	COLORREF GetTreelistColor(int i);

	int GetTreelistColorCount();
	void SetTreelistColorCount(int count);

	bool IsHumanFormat();
	void SetHumanFormat(bool human);

	bool IsPacmanAnimation();
	void SetPacmanAnimation(bool animate);

	bool IsShowTimeSpent();
	void SetShowTimeSpent(bool show);

	bool IsCushionShading();
	void SetCushionShading(bool shade);

	bool IsTreemapGrid();
	void SetTreemapGrid(bool show);

	COLORREF GetTreemapGridColor();
	void SetTreemapGridColor(COLORREF color);

	COLORREF GetTreemapHighlightColor();
	void SetTreemapHighlightColor(COLORREF color);

	int GetHeightFactor(); // Percent
	void SetHeightFactor(int h); // Percent
	int GetHeightFactorDefault();
	void GetHeightFactorRange(int& min, int& max);

	int GetScaleFactor(); // Percent
	void SetScaleFactor(int f); // Percent
	int GetScaleFactorDefault();
	void GetScaleFactorRange(int& min, int& max);

	int GetAmbientLight(); // Percent
	void SetAmbientLight(int a); // Percent
	int GetAmbientLightDefault();
	void GetAmbientLightRange(int& min, int& max);

	bool IsFollowMountPoints();
	void SetFollowMountPoints(bool follow);

	void GetUserDefinedCleanups(USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);
	void SetUserDefinedCleanups(const USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);

	void GetEnabledUserDefinedCleanups(CArray<int, int>& indices);
	bool IsUserDefinedCleanupEnabled(int i);
	const USERDEFINEDCLEANUP *GetUserDefinedCleanup(int i);


private:
	void ReadUserDefinedCleanup(int i);
	void SaveUserDefinedCleanup(int i);

	bool m_treelistGrid;
	COLORREF m_treelistColor[TREELISTCOLORCOUNT];
	int m_treelistColorCount;
	bool m_humanFormat;
	bool m_pacmanAnimation;
	bool m_showTimeSpent;
	bool m_cushionShading;
	bool m_treemapGrid;
	COLORREF m_treemapGridColor;
	COLORREF m_treemapHighlightColor;
	int m_heightFactor;	// H  (percent)
	int m_scaleFactor;	// F  (percent)
	int m_ambientLight;	// Ia (percent)
	bool m_followMountPoints;
	USERDEFINEDCLEANUP m_userDefinedCleanup[USERDEFINEDCLEANUPCOUNT];
};


