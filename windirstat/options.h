// options.h - Declaration of CRegistryUser, COptions and CPersistence
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#ifndef __NOT_WDS
#include "treemap.h"
#endif // __NOT_WDS
#include <common/wds_constants.h>
#include <common/SimpleIni.h>
#include <atlbase.h> // CRegKey
#include <memory>

#ifndef __NOT_WDS
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
    bool virginTitle;
    CStringW title;
    bool worksForDrives;
    bool worksForDirectories;
    bool worksForFilesFolder;
    bool worksForFiles;
    bool worksForUncPaths;
    CStringW commandLine;
    bool recurseIntoSubdirectories;
    bool askForConfirmation;
    bool showConsoleWindow;
    bool waitForCompletion;
    REFRESHPOLICY refreshPolicy;
};
#endif // __NOT_WDS

#define USERDEFINEDCLEANUPCOUNT 10

#define TREELISTCOLORCOUNT 8

// Base interface for retrieving/storing configuration
// The ctor of derived classes is allowed to throw an HRESULT if something
// goes wrong.
class ICfgStorage
{
public:
    virtual void setString(LPCWSTR section, LPCWSTR entry, LPCWSTR value) = 0;
    virtual CStringW getString(LPCWSTR section, LPCWSTR entry, LPCWSTR defaultValue) = 0;

    virtual void setInt(LPCWSTR section, LPCWSTR entry, int value) = 0;
    virtual int getInt(LPCWSTR section, LPCWSTR entry, int defaultValue) = 0;

    virtual void setUint(LPCWSTR section, LPCWSTR entry, unsigned int value) = 0;
    virtual unsigned int getUint(LPCWSTR section, LPCWSTR entry, unsigned int defaultValue) = 0;

    virtual void setBool(LPCWSTR section, LPCWSTR entry, bool value) = 0;
    virtual bool getBool(LPCWSTR section, LPCWSTR entry, bool defaultValue) = 0;

    virtual void flush() = 0;
    virtual long getLastError() const = 0;
};

class CRegistryStg : public ICfgStorage
{
    CRegistryStg(); // hide
    CRegistryStg& operator=(const CRegistryStg&); // hide
public:
    CRegistryStg(HKEY hKeyParent, LPCWSTR lpszKeyName);

    virtual void setString(LPCWSTR section, LPCWSTR entry, LPCWSTR value);
    virtual CStringW getString(LPCWSTR section, LPCWSTR entry, LPCWSTR defaultValue);

    virtual void setInt(LPCWSTR section, LPCWSTR entry, int value);
    virtual int getInt(LPCWSTR section, LPCWSTR entry, int defaultValue);

    virtual void setUint(LPCWSTR section, LPCWSTR entry, unsigned int value);
    virtual unsigned int getUint(LPCWSTR section, LPCWSTR entry, unsigned int defaultValue);

    virtual void setBool(LPCWSTR section, LPCWSTR entry, bool value);
    virtual bool getBool(LPCWSTR section, LPCWSTR entry, bool defaultValue);

    virtual void flush();
    virtual long getLastError() const;

private:
    mutable long m_lastError;
    REGSAM const m_sam;
    HKEY m_parentKey;
    CStringW m_lpszKeyName;

    CRegKey m_key;
};

class CIniFileStg : public ICfgStorage
{
    CIniFileStg(); // hide
    CIniFileStg& operator=(const CIniFileStg&); // hide
public:
    CIniFileStg(LPCWSTR lpszFilePath);
    ~CIniFileStg();

    virtual void setString(LPCWSTR section, LPCWSTR entry, LPCWSTR value);
    virtual CStringW getString(LPCWSTR section, LPCWSTR entry, LPCWSTR defaultValue);

    virtual void setInt(LPCWSTR section, LPCWSTR entry, int value);
    virtual int getInt(LPCWSTR section, LPCWSTR entry, int defaultValue);

    virtual void setUint(LPCWSTR section, LPCWSTR entry, unsigned int value);
    virtual unsigned int getUint(LPCWSTR section, LPCWSTR entry, unsigned int defaultValue);

    virtual void setBool(LPCWSTR section, LPCWSTR entry, bool value);
    virtual bool getBool(LPCWSTR section, LPCWSTR entry, bool defaultValue);

    virtual void flush();
    virtual long getLastError() const;

private:
    mutable long m_lastError;
    CStringW m_lpszFilePath;
    CSimpleIni m_ini;

    static HRESULT siErrorToHR_(SI_Error err);
};

// It's an aggregate, but provides the same interface
class CConfigStorage : public ICfgStorage
{
public:
    // primary *must* be given, secondary is optional
    CConfigStorage(ICfgStorage* primary, ICfgStorage* secondary);
    ~CConfigStorage();

    virtual void setString(LPCWSTR section, LPCWSTR entry, LPCWSTR value);
    virtual CStringW getString(LPCWSTR section, LPCWSTR entry, LPCWSTR defaultValue);

    virtual void setInt(LPCWSTR section, LPCWSTR entry, int value);
    virtual int getInt(LPCWSTR section, LPCWSTR entry, int defaultValue);

    virtual void setUint(LPCWSTR section, LPCWSTR entry, unsigned int value);
    virtual unsigned int getUint(LPCWSTR section, LPCWSTR entry, unsigned int defaultValue);

    virtual void setBool(LPCWSTR section, LPCWSTR entry, bool value);
    virtual bool getBool(LPCWSTR section, LPCWSTR entry, bool defaultValue);

private:
    std::auto_ptr<ICfgStorage> m_primaryStore;
    std::auto_ptr<ICfgStorage> m_secondaryStore;
};

class CRegistryUser
{
public:
    static void setProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR value);
    static CStringW getProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR defaultValue = wds::strEmpty);

    static void setProfileInt(LPCWSTR section, LPCWSTR entry, int value);
    static int getProfileInt(LPCWSTR section, LPCWSTR entry, int defaultValue);

    static void setProfileBool(LPCWSTR section, LPCWSTR entry, bool value);
    static bool getProfileBool(LPCWSTR section, LPCWSTR entry, bool defaultValue);

    static void checkRange(int& value, int min, int max);
    static void checkRange(unsigned int& value, unsigned int min, unsigned int max);
};


#ifndef __NOT_WDS
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

    static void SetSplitterPos(LPCWSTR name, bool valid, double userpos);
    static void GetSplitterPos(LPCWSTR name, bool& valid, double& userpos);

    static void SetColumnOrder(LPCWSTR name, const CArray<int, int>& arr);
    static void GetColumnOrder(LPCWSTR name, /* in/out */ CArray<int, int>& arr);

    static void SetColumnWidths(LPCWSTR name, const CArray<int, int>& arr);
    static void GetColumnWidths(LPCWSTR name, /* in/out */ CArray<int, int>& arr);

    static void SetDialogRectangle(LPCWSTR name, const CRect& rc);
    static void GetDialogRectangle(LPCWSTR name, CRect& rc);

//   static void SetSorting(LPCWSTR name, int column1, bool ascending1, int column2, bool ascending2);
//   static void GetSorting(LPCWSTR name, int columnCount, int& column1, bool& ascending1, int& column2, bool& ascending2);

    static int GetConfigPage(int max);
    static void SetConfigPage(int page);

    static void GetConfigPosition(/* in/out */ CPoint& pt);
    static void SetConfigPosition(CPoint pt);

    static CStringW GetBarStateSection();

    static int GetSelectDrivesRadio();
    static void SetSelectDrivesRadio(int radio);

    static CStringW GetSelectDrivesFolder();
    static void SetSelectDrivesFolder(LPCWSTR folder);

    static void GetSelectDrivesDrives(CStringArray& drives);
    static void SetSelectDrivesDrives(const CStringArray& drives);

    static bool GetShowDeleteWarning();
    static void SetShowDeleteWarning(bool show);

private:
    static void SetArray(LPCWSTR entry, const CArray<int, int>& arr);
    static void GetArray(LPCWSTR entry, /* in/out */ CArray<int, int>& arr);
    static void SetRect(LPCWSTR entry, const CRect& rc);
    static void GetRect(LPCWSTR entry, CRect& rc);
    static void SanitizeRect(CRect& rc);

    static CStringW EncodeWindowPlacement(const WINDOWPLACEMENT& wp);
    static void DecodeWindowPlacement(const CStringW& s, WINDOWPLACEMENT& wp);
    static CStringW MakeSplitterPosEntry(LPCWSTR name);
    static CStringW MakeColumnOrderEntry(LPCWSTR name);
    static CStringW MakeColumnWidthsEntry(LPCWSTR name);
    //static CStringW MakeSortingColumnEntry(LPCWSTR name, int n);
    //static CStringW MakeSortingAscendingEntry(LPCWSTR name, int n);
    static CStringW MakeDialogRectangleEntry(LPCWSTR name);


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

// m_pszProfileName|m_pszRegistryKey
// CWinApp::SetRegistryKey
// CWinApp::DelRegTree
// CWinApp::WriteProfileBinary


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

    bool IsListGrid();
    void SetListGrid(bool show);

    bool IsListStripes();
    void SetListStripes(bool show);

    bool IsListFullRowSelection();
    void SetListFullRowSelection(bool show);

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

    COLORREF GetTreemapHighlightColor();
    void SetTreemapHighlightColor(COLORREF color);

    const CTreemap::Options *GetTreemapOptions();
    void SetTreemapOptions(const CTreemap::Options& options);

    bool IsFollowMountPoints();
    void SetFollowMountPoints(bool follow);

    // Option to ignore junction points which are not volume mount points
    bool IsFollowJunctionPoints();
    void SetFollowJunctionPoints(bool ignore);

    // Option to use CDirStatApp::m_langid for date/time and number formatting
    bool IsUseWdsLocale();
    void SetUseWdsLocale(bool use);

    // Option to ignore hidden files and folders
    bool IsSkipHidden();
    void SetSkipHidden(bool skip);

    void GetUserDefinedCleanups(USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);
    void SetUserDefinedCleanups(const USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);

    void GetEnabledUserDefinedCleanups(CArray<int, int>& indices);
    bool IsUserDefinedCleanupEnabled(int i);
    const USERDEFINEDCLEANUP *GetUserDefinedCleanup(int i);

    CStringW GetReportSubject();
    CStringW GetReportDefaultSubject();
    void SetReportSubject(LPCWSTR subject);

    CStringW GetReportPrefix();
    CStringW GetReportDefaultPrefix();
    void SetReportPrefix(LPCWSTR prefix);

    CStringW GetReportSuffix();
    CStringW GetReportDefaultSuffix();
    void SetReportSuffix(LPCWSTR suffix);

private:
    void ReadUserDefinedCleanup(int i);
    void SaveUserDefinedCleanup(int i);
    void ReadTreemapOptions();
    void SaveTreemapOptions();

    bool m_listGrid;
    bool m_listStripes;
    bool m_listFullRowSelection;
    COLORREF m_treelistColor[TREELISTCOLORCOUNT];
    int m_treelistColorCount;
    bool m_humanFormat;
    bool m_pacmanAnimation;
    bool m_showTimeSpent;
    COLORREF m_treemapHighlightColor;

    CTreemap::Options m_treemapOptions;

    bool m_followMountPoints;
    bool m_followJunctionPoints;
    bool m_useWdsLocale;
    bool m_skipHidden;

    USERDEFINEDCLEANUP m_userDefinedCleanup[USERDEFINEDCLEANUPCOUNT];

    CStringW m_reportSubject;
    CStringW m_reportPrefix;
    CStringW m_reportSuffix;
};
#endif // __NOT_WDS
