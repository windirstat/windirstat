// globalhelpers.cpp - Implementation of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
// Copyright (C) 2010 Chris Wimmer
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
#include "windirstat.h"
#include <common/mdexceptions.h>
#include <common/cotaskmem.h>
#include <common/commonhelpers.h>
#include "globalhelpers.h"
#include "options.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    CString FormatLongLongNormal(ULONGLONG n)
    {
        // Returns formatted number like "123.456.789".

        ASSERT(n >= 0);

        CString all;

        do
        {
            int rest = (int)(n % 1000);
            n/= 1000;

            CString s;
            if(n > 0)
            {
                s.Format(_T("%s%03d"), GetLocaleThousandSeparator().GetString(), rest);
            }
            else
            {
                s.Format(_T("%d"), rest);
            }

            all = s + all;
        } while(n > 0);

        return all;
    }

    void CacheString(CString& s, UINT resId, LPCTSTR defaultVal)
    {
        ASSERT(_tcslen(defaultVal) > 0);

        if(s.IsEmpty())
        {
            s = LoadString(resId);

            if(s.IsEmpty())
            {
                s = defaultVal;
            }
        }
    }

}

CString GetLocaleString(LCTYPE lctype, LANGID langid)
{
    LCID lcid = MAKELCID(langid, SORT_DEFAULT);

    int len = ::GetLocaleInfo(lcid, lctype, NULL, 0);
    CString s;

    ::GetLocaleInfo(lcid, lctype, s.GetBuffer(len), len);
    s.ReleaseBuffer();

    return s;
}

CString GetLocaleLanguage(LANGID langid)
{
    CString s = GetLocaleString(LOCALE_SNATIVELANGNAME, langid);

    // In the French case, the system returns "francais",
    // but we want "Francais".

    // TODO: need a Unicode version for this function

    if(s.GetLength() > 0)
    {
        s.SetAt(0, (TCHAR)_totupper(s[0])); // FIXME: same holds for Russian, but _toupper won't work there ...
    }

    return s + _T(" - ") + GetLocaleString(LOCALE_SNATIVECTRYNAME, langid);
}

CString GetLocaleThousandSeparator()
{
    return GetLocaleString(LOCALE_STHOUSAND, GetWDSApp()->GetEffectiveLangid());
}

CString GetLocaleDecimalSeparator()
{
    return GetLocaleString(LOCALE_SDECIMAL, GetWDSApp()->GetEffectiveLangid());
}

CString FormatBytes(ULONGLONG const& n)
{
    if(GetOptions()->IsHumanFormat())
    {
        return FormatLongLongHuman(n);
    }
    else
    {
        return FormatLongLongNormal(n);
    }
}

CString FormatLongLongHuman(ULONGLONG n)
{
    // Returns formatted number like "12,4 GB".
    ASSERT(n >= 0);
    const int base = 1024;
    const int half = base / 2;

    CString s;

    double B = (int)(n % base);
    n/= base;

    double KB = (int)(n % base);
    n/= base;

    double MB = (int)(n % base);
    n/= base;

    double GB = (int)(n % base);
    n/= base;

    double TB = (int)(n);

    if(TB != 0 || GB == base - 1 && MB >= half)
    {
        s.Format(_T("%s %s"), FormatDouble(TB + GB/base).GetString(), GetSpec_TB().GetString());
    }
    else if(GB != 0 || MB == base - 1 && KB >= half)
    {
        s.Format(_T("%s %s"), FormatDouble(GB + MB/base).GetString(), GetSpec_GB().GetString());
    }
    else if(MB != 0 || KB == base - 1 && B >= half)
    {
        s.Format(_T("%s %s"), FormatDouble(MB + KB/base).GetString(), GetSpec_MB().GetString());
    }
    else if(KB != 0)
    {
        s.Format(_T("%s %s"), FormatDouble(KB + B/base).GetString(), GetSpec_KB().GetString());
    }
    else if(B != 0)
    {
        s.Format(_T("%d %s"), (int)B, GetSpec_Bytes().GetString());
    }
    else
    {
        s = _T("0");
    }

    return s;
}


CString FormatCount(ULONGLONG const& n)
{
    return FormatLongLongNormal(n);
}

CString FormatDouble(double d) // "98,4" or "98.4"
{
    ASSERT(d >= 0);

    d += 0.05;

    int i = (int)floor(d);
    int r = (int)(10 * fmod(d, 1));

    CString s;
    s.Format(_T("%d%s%d"), i, GetLocaleDecimalSeparator().GetString(), r);

    return s;
}

CString PadWidthBlanks(CString n, int width)
{
    int blankCount = width - n.GetLength();
    if(blankCount > 0)
    {
        int i = 0;
        CString b;
        LPTSTR psz = b.GetBuffer(blankCount + 1);
        for(i = 0; i < blankCount; i++)
        {
            psz[i]= _T(' ');
        }
        psz[i]= 0;
        b.ReleaseBuffer();

        n = b + n;
    }
    return n;
}

CString FormatFileTime(const FILETIME& t)
{
    SYSTEMTIME st;
    if(!::FileTimeToSystemTime(&t, &st))
    {
        return MdGetWinErrorText(::GetLastError());
    }

    LCID lcid = MAKELCID(GetWDSApp()->GetEffectiveLangid(), SORT_DEFAULT);

    CString date;
    VERIFY(0 < ::GetDateFormat(lcid, DATE_SHORTDATE, &st, NULL, date.GetBuffer(256), 256));
    date.ReleaseBuffer();

    CString time;
    VERIFY(0 < GetTimeFormat(lcid, 0, &st, NULL, time.GetBuffer(256), 256));
    time.ReleaseBuffer();

    return date + _T("  ") + time;
}

CString FormatAttributes(DWORD attr)
{
    // order:
    // strAttributeReadonly
    // strAttributeHidden
    // strAttributeSystem
    // strAttributeArchive
    // strAttributeTemporary
    // strAttributeCompressed
    // strAttributeEncrypted
    // strAttributeIntegrityStream
    // strAttributeVirtual
    // strAttributeReparsePoint
    // strAttributeSparse
    // strAttributeOffline
    // strAttributeNotContentIndexed
    // strAttributeEA
    if(attr == INVALID_FILE_ATTRIBUTES)
    {
        return wds::strInvalidAttributes;
    }

    CString attributes;
    if(attr & FILE_ATTRIBUTE_READONLY)
    {
        attributes.Append(wds::strAttributeReadonly);
    }

    if(attr & FILE_ATTRIBUTE_HIDDEN)
    {
        attributes.Append(wds::strAttributeHidden);
    }

    if(attr & FILE_ATTRIBUTE_SYSTEM)
    {
        attributes.Append(wds::strAttributeSystem);
    }


    if(attr & FILE_ATTRIBUTE_ARCHIVE)
    {
        attributes.Append(wds::strAttributeArchive);
    }

    if(attr & FILE_ATTRIBUTE_COMPRESSED)
    {
        attributes.Append(wds::strAttributeCompressed   );
    }

    if(attr & FILE_ATTRIBUTE_ENCRYPTED)
    {
        attributes.Append(wds::strAttributeEncrypted);
    }

    return attributes;
}

CString FormatMilliseconds(DWORD ms)
{
    CString ret;
    DWORD sec = (ms + 500) / 1000;

    DWORD s = sec % 60;
    DWORD min = sec / 60;

    DWORD m = min % 60;

    DWORD h = min / 60;

    if(h > 0)
    {
        ret.Format(_T("%u:%02u:%02u"), h, m, s);
    }
    else
    {
        ret.Format(_T("%u:%02u"), m, s);
    }
    return ret;
}

bool GetVolumeName(LPCTSTR rootPath, CString& volumeName)
{
    CString ret;
    DWORD dummy;

    UINT old = SetErrorMode(SEM_FAILCRITICALERRORS);

    bool b = (FALSE != GetVolumeInformation(rootPath, volumeName.GetBuffer(256), 256, &dummy, &dummy, &dummy, NULL, 0));
    volumeName.ReleaseBuffer();

    if(!b)
    {
        VTRACE(_T("GetVolumeInformation(%s) failed: %u"), rootPath, ::GetLastError());
    }

    ::SetErrorMode(old);

    return b;
}

// Given a root path like "C:\", this function
// obtains the volume name and returns a complete display string
// like "BOOT (C:)".
CString FormatVolumeNameOfRootPath(CString rootPath)
{
    CString ret;
    CString volumeName;
    bool b = GetVolumeName(rootPath, volumeName);
    if(b)
    {
        ret = FormatVolumeName(rootPath, volumeName);
    }
    else
    {
        ret = rootPath;
    }
    return ret;
}

CString FormatVolumeName(CString rootPath, CString volumeName)
{
    CString ret;
    ret.Format(_T("%s (%s)"), volumeName.GetString(), rootPath.Left(2).GetString());
    return ret;
}

// The inverse of FormatVolumeNameOfRootPath().
// Given a name like "BOOT (C:)", it returns "C:" (without trailing backslash).
// Or, if name like "C:\", it returns "C:".
CString PathFromVolumeName(CString name)
{
    int i = name.ReverseFind(wds::chrBracketClose);
    if(i == -1)
    {
        ASSERT(name.GetLength() == 3);
        return name.Left(2);
    }

    ASSERT(i != -1);
    int k = name.ReverseFind(wds::chrBracketOpen);
    ASSERT(k != -1);
    ASSERT(k < i);
    CString path = name.Mid(k + 1, i - k - 1);
    ASSERT(path.GetLength() == 2);
    ASSERT(path[1] == wds::chrColon);

    return path;
}

// Retrieve the "fully qualified parse name" of "My Computer"
CString GetParseNameOfMyComputer()
{
    CComPtr<IShellFolder> sf;
    HRESULT hr = ::SHGetDesktopFolder(&sf);
    MdThrowFailed(hr, _T("::SHGetDesktopFolder"));

    CCoTaskMem<LPITEMIDLIST> pidl;
    hr = ::SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
    MdThrowFailed(hr, _T("SHGetSpecialFolderLocation(CSIDL_DRIVES)"));

    STRRET name;
    ZeroMemory(&name, sizeof(name));
    name.uType = STRRET_CSTR;
    hr = sf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &name);
    MdThrowFailed(hr, _T("GetDisplayNameOf(My Computer)"));

    return MyStrRetToString(pidl, &name);
}

void GetPidlOfMyComputer(LPITEMIDLIST *ppidl)
{
    CComPtr<IShellFolder> sf;
    HRESULT hr = ::SHGetDesktopFolder(&sf);
    MdThrowFailed(hr, _T("SHGetDesktopFolder"));

    hr = ::SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, ppidl);
    MdThrowFailed(hr, _T("SHGetSpecialFolderLocation(CSIDL_DRIVES)"));
}

void ShellExecuteWithAssocDialog(HWND hwnd, LPCTSTR filename)
{
    CWaitCursor wc;

    BOOL bExecuted = ShellExecuteNoThrow(hwnd, NULL, filename, NULL, NULL, SW_SHOWNORMAL);
    if((!bExecuted) && (ERROR_NO_ASSOCIATION == ::GetLastError()))
    {
        // Q192352
        CString sysDir;
        //-- Get the system directory so that we know where Rundll32.exe resides.
        ::GetSystemDirectory(sysDir.GetBuffer(_MAX_PATH), _MAX_PATH);
        sysDir.ReleaseBuffer();

        CString parameters = _T("shell32.dll,OpenAs_RunDLL ");
        bExecuted = ShellExecuteNoThrow(hwnd, _T("open"), _T("RUNDLL32.EXE"), parameters + filename, sysDir, SW_SHOWNORMAL);
    }

    if(!bExecuted)
    {
        MdThrowStringExceptionF(_T("ShellExecute failed: %1!s!"), MdGetWinErrorText(::GetLastError()).GetString());
    }
}

CString GetFolderNameFromPath(LPCTSTR path)
{
    CString s = path;
    int i = s.ReverseFind(wds::chrBackslash);
    if(i < 0)
    {
        return s;
    }
    return s.Left(i);
}

CString GetCOMSPEC()
{
    CString cmd;

    DWORD dw = ::GetEnvironmentVariable(_T("COMSPEC"), cmd.GetBuffer(_MAX_PATH), _MAX_PATH);
    cmd.ReleaseBuffer();

    if(dw == 0)
    {
        VTRACE(_T("COMSPEC not set."));
        cmd = _T("cmd.exe");
    }
    return cmd;
}

DWORD WaitForHandleWithRepainting(HANDLE h, DWORD TimeOut /*= INFINITE*/)
{
    DWORD r = 0;
    // Code derived from MSDN sample "Waiting in a Message Loop".

    while(true)
    {
        // Read all of the messages in this next loop, removing each message as we read it.
        MSG msg;
        while(::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
        {
            ::DispatchMessage(&msg);
        }

        // Wait for WM_PAINT message sent or posted to this queue
        // or for one of the passed handles be set to signaled.
        r = ::MsgWaitForMultipleObjects(1, &h, FALSE, TimeOut, QS_PAINT);

        // The result tells us the type of event we have.
        if(r == WAIT_OBJECT_0 + 1)
        {
            // New messages have arrived.
            // Continue to the top of the always while loop to dispatch them and resume waiting.
            continue;
        }
        else
        {
            // The handle became signaled.
            break;
        }
    }

    return r;
}

bool FolderExists(LPCTSTR path)
{
    CFileFind finder;
    BOOL b = finder.FindFile(path);
    if(b)
    {
        finder.FindNextFile();
        return (FALSE != finder.IsDirectory());
    }
    else
    {
        // Here we land, if path is an UNC drive. In this case we
        // try another FindFile:
        b = finder.FindFile(CString(path) + _T("\\*.*"));
        return (b != false);
    }
}

bool DriveExists(const CString& path)
{
    if(path.GetLength() != 3 || path[1] != wds::chrColon || path[2] != wds::chrBackslash)
    {
        return false;
    }

    CString letter = path.Left(1);
    letter.MakeLower();
    int d = letter[0] - wds::chrSmallA;

    DWORD mask = 0x1 << d;

    if((mask & ::GetLogicalDrives()) == 0)
    {
        return false;
    }

    CString dummy;
    if(!::GetVolumeName(path, dummy))
    {
        return false;
    }

    return true;
}

#ifndef UNLEN
#   define UNLEN MAX_PATH
#endif

CString GetUserName()
{
    CString s;
    DWORD size = UNLEN + 1;
    (void)::GetUserName(s.GetBuffer(size), &size);
    s.ReleaseBuffer();
    return s;
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
//
// This function returns
// "", if QueryDosDevice is unsupported or drive doesn't begin with a drive letter,
// 'Information about MS-DOS device names' otherwise:
// Something like
//
// \Device\Harddisk\Volume1                               for a local drive
// \Device\LanmanRedirector\;T:0000000011e98\spock\temp   for a network drive
// \??\C:\programme                                       for a SUBSTed local path
// \??\T:\Neuer Ordner                                    for a SUBSTed SUBSTed path
// \??\UNC\spock\temp                                     for a SUBSTed UNC path
//
// As always, I had to experimentally determine these strings, Microsoft
// didn't think it necessary to document them. (Sometimes I think, they
// even don't document such things internally...)
//
// I hope that a drive is SUBSTed iff this string starts with \??\.
//
// assarbad:
//   It cannot be safely determined whether a path is or is not SUBSTed on NT
//   via this API. You would have to look up the volume mount points because
//   SUBST only works per session by definition whereas volume mount points
//   work across sessions (after restarts).
//
CString MyQueryDosDevice(LPCTSTR drive)
{
    CString d = drive;

    if(d.GetLength() < 2 || d[1] != wds::chrColon)
    {
        return wds::strEmpty;
    }

    d = d.Left(2);

    CString info;
    DWORD dw = ::QueryDosDevice(d, info.GetBuffer(512), 512);
    info.ReleaseBuffer();

    if(dw == 0)
    {
        VTRACE(_T("QueryDosDevice(%s) failed: %s"), d.GetString(), MdGetWinErrorText(::GetLastError()).GetString());
        return wds::strEmpty;
    }

    return info;
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
// 
// This function returnes true, if QueryDosDevice() is supported
// and drive is a SUBSTed drive.
//
bool IsSUBSTedDrive(LPCTSTR drive)
{
    CString info = MyQueryDosDevice(drive);
    return (info.GetLength() >= 4 && info.Left(4) == "\\??\\");
}

CString GetSpec_Bytes()
{
    static CString s;
    CacheString(s, IDS_SPEC_BYTES, _T("Bytes"));
    return s;
}

CString GetSpec_KB()
{
    static CString s;
    CacheString(s, IDS_SPEC_KB, _T("KiB"));
    return s;
}

CString GetSpec_MB()
{
    static CString s;
    CacheString(s, IDS_SPEC_MB, _T("MiB"));
    return s;
}

CString GetSpec_GB()
{
    static CString s;
    CacheString(s, IDS_SPEC_GB, _T("GiB"));
    return s;
}

CString GetSpec_TB()
{
    static CString s;
    CacheString(s, IDS_SPEC_TB, _T("TiB"));
    return s;
}

BOOL IsAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID pSid;
    if (::AllocateAndInitializeSid(&NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pSid))
    {
        BOOL bResult = FALSE;
        if (!::CheckTokenMembership( NULL, pSid, &bResult))
        {
            ::FreeSid(pSid);
            return FALSE;
        }
        ::FreeSid(pSid);
        return bResult;
    }

    return FALSE;
}
