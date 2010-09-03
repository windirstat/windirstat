// options.h - Declaration of CRegistryUser, COptions and CPersistence
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//

#ifndef __WDS_OPTIONS_H__
#define __WDS_OPTIONS_H__
#pragma once

#include "treemap.h"
#include "../common/wds_constants.h"
#include "../common/SimpleIni.h"
#include <atlbase.h> // CRegKey

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

// Base interface for retrieving/storing configuration
// The ctor of derived classes is allowed to throw an HRESULT if something
// goes wrong.
class ICfgStorage
{
public:
    virtual void setString(LPCTSTR section, LPCTSTR entry, LPCTSTR value) = 0;
    virtual CString getString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue) = 0;

    virtual void setInt(LPCTSTR section, LPCTSTR entry, int value) = 0;
    virtual int getInt(LPCTSTR section, LPCTSTR entry, int defaultValue) = 0;

    virtual void setUint(LPCTSTR section, LPCTSTR entry, unsigned int value) = 0;
    virtual unsigned int getUint(LPCTSTR section, LPCTSTR entry, unsigned int defaultValue) = 0;

    virtual void setProfileBool(LPCTSTR section, LPCTSTR entry, bool value) = 0;
    virtual bool getProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue) = 0;

    virtual void flush() = 0;
    virtual long getLastError() const = 0;
};

class CRegistryStg : public ICfgStorage
{
    CRegistryStg(); // hide
    CRegistryStg& operator=(const CRegistryStg&); // hide
public:
    CRegistryStg(HKEY hKeyParent, LPCTSTR lpszKeyName);

    virtual void setString(LPCTSTR section, LPCTSTR entry, LPCTSTR value);
    virtual CString getString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue);

    virtual void setInt(LPCTSTR section, LPCTSTR entry, int value);
    virtual int getInt(LPCTSTR section, LPCTSTR entry, int defaultValue);

    virtual void setUint(LPCTSTR section, LPCTSTR entry, unsigned int value);
    virtual unsigned int getUint(LPCTSTR section, LPCTSTR entry, unsigned int defaultValue);

    virtual void setProfileBool(LPCTSTR section, LPCTSTR entry, bool value);
    virtual bool getProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue);

    virtual void flush();
    virtual long getLastError() const;

private:
    mutable long m_lastError;
    REGSAM const m_sam;
    HKEY m_parentKey;
    CString m_lpszKeyName;

    CRegKey m_key;
};

class CIniFileStg : public ICfgStorage
{
    CIniFileStg(); // hide
    CIniFileStg& operator=(const CIniFileStg&); // hide
public:
    CIniFileStg(LPCTSTR lpszFilePath);
    ~CIniFileStg();

    virtual void setString(LPCTSTR section, LPCTSTR entry, LPCTSTR value);
    virtual CString getString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue);

    virtual void setInt(LPCTSTR section, LPCTSTR entry, int value);
    virtual int getInt(LPCTSTR section, LPCTSTR entry, int defaultValue);

    virtual void setUint(LPCTSTR section, LPCTSTR entry, unsigned int value);
    virtual unsigned int getUint(LPCTSTR section, LPCTSTR entry, unsigned int defaultValue);

    virtual void setProfileBool(LPCTSTR section, LPCTSTR entry, bool value);
    virtual bool getProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue);

    virtual void flush();
    virtual long getLastError() const;

private:
    mutable long m_lastError;
    CString m_lpszFilePath;
    CSimpleIni m_ini;

    static HRESULT siErrorToHR_(SI_Error err);
};

class CRegistryUser
{
public:
    static void setProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value);
    static CString getProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue = wds::strEmpty);

    static void setProfileInt(LPCTSTR section, LPCTSTR entry, int value);
    static int getProfileInt(LPCTSTR section, LPCTSTR entry, int defaultValue);

    static void setProfileBool(LPCTSTR section, LPCTSTR entry, bool value);
    static bool getProfileBool(LPCTSTR section, LPCTSTR entry, bool defaultValue);

    static void checkRange(int& value, int min, int max);
    static void checkRange(unsigned int& value, unsigned int min, unsigned int max);
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

//   static void SetSorting(LPCTSTR name, int column1, bool ascending1, int column2, bool ascending2);
//   static void GetSorting(LPCTSTR name, int columnCount, int& column1, bool& ascending1, int& column2, bool& ascending2);

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
    static void SanitizeRect(CRect& rc);

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

    void GetUserDefinedCleanups(USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);
    void SetUserDefinedCleanups(const USERDEFINEDCLEANUP udc[USERDEFINEDCLEANUPCOUNT]);

    void GetEnabledUserDefinedCleanups(CArray<int, int>& indices);
    bool IsUserDefinedCleanupEnabled(int i);
    const USERDEFINEDCLEANUP *GetUserDefinedCleanup(int i);

    CString GetReportSubject();
    CString GetReportDefaultSubject();
    void SetReportSubject(LPCTSTR subject);

    CString GetReportPrefix();
    CString GetReportDefaultPrefix();
    void SetReportPrefix(LPCTSTR prefix);

    CString GetReportSuffix();
    CString GetReportDefaultSuffix();
    void SetReportSuffix(LPCTSTR suffix);

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

    USERDEFINEDCLEANUP m_userDefinedCleanup[USERDEFINEDCLEANUPCOUNT];

    CString m_reportSubject;
    CString m_reportPrefix;
    CString m_reportSuffix;
};

#endif // __WDS_OPTIONS_H__
