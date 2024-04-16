// FileFindEnhanced.h - Declaration of CFileFindEnhanced
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

#include <stdafx.h>
#include <winternl.h>

#include "FileFind.h"
#include "Options.h"
#include <common/Tracer.h>

#pragma comment(lib,"ntdll.lib")

static HMODULE rtl_library = LoadLibrary(L"ntdll.dll");

static NTSTATUS(WINAPI* NtQueryDirectoryFile)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
    ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName, BOOLEAN RestartScan) = reinterpret_cast<decltype(NtQueryDirectoryFile)>(
        static_cast<LPVOID>(GetProcAddress(rtl_library, "NtQueryDirectoryFile")));

FileFindEnhanced::~FileFindEnhanced()
{
    if (m_handle != nullptr) NtClose(m_handle);
}

bool FileFindEnhanced::FindNextFile()
{
    BOOL success = FALSE;
    if (m_firstrun || m_current_info->NextEntryOffset == 0)
    {
        constexpr auto BUFFER_SIZE = 64 * 1024;
        thread_local BYTE m_directory_info[BUFFER_SIZE];

        // handle optional pattern mask
        UNICODE_STRING u_search = {};
        u_search.Length = static_cast<USHORT>(m_search.GetLength() * sizeof(WCHAR));
        u_search.MaximumLength = static_cast<USHORT>(m_search.GetLength() + 1) * sizeof(WCHAR);
        u_search.Buffer = m_search.GetBuffer();

        // enumerate files in the directory
        constexpr auto FileDirectoryInformation = 1;
        IO_STATUS_BLOCK IoStatusBlock;
        const NTSTATUS Status = NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
            m_directory_info, BUFFER_SIZE, static_cast<FILE_INFORMATION_CLASS>(FileDirectoryInformation),
            FALSE, (u_search.Length > 0) ? &u_search : nullptr, (m_firstrun) ? TRUE : FALSE);

        // fetch point to current node 
        success = (Status == 0);
        m_current_info = reinterpret_cast<FILE_DIRECTORY_INFORMATION*>(m_directory_info);

        // special case for reparse on initial run points - update attributes
        if (success && m_firstrun) m_current_info->FileAttributes = GetFileAttributes(GetFilePath());

        // disable for next run
        m_firstrun = false;
        
    }
    else
    {
        m_current_info = reinterpret_cast<FILE_DIRECTORY_INFORMATION*>(
            &reinterpret_cast<BYTE*>(m_current_info)[m_current_info->NextEntryOffset]);
        success = true;
    }

    if (success)
    {
        LPWSTR tmp = m_name.GetBufferSetLength(m_current_info->FileNameLength / sizeof(WCHAR));
        memcpy(tmp, m_current_info->FileName, m_current_info->FileNameLength);
    }

    return success;
}

bool FileFindEnhanced::FindFile(const CStringW & strFolder, const CStringW& strName)
{
    // stash the search pattern for later use
    m_search = strName;

    // convert the path to a long path that is compatible with the other call
    m_base = strFolder;
    if (m_base.Find(L":\\", 1) == 1) m_base = m_dos + m_base;
    else if (m_base.Find(L"\\\\") == 0) m_base = m_dosunc + m_base.Mid(2);
    UNICODE_STRING u_path = {};
    u_path.Length = static_cast<USHORT>(m_base.GetLength() * sizeof(WCHAR));
    u_path.MaximumLength = static_cast<USHORT>(m_base.GetLength() + 1) * sizeof(WCHAR);
    u_path.Buffer = m_base.GetBuffer();

    // update object attributes object
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, nullptr, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    attributes.ObjectName = &u_path;

    // get an open file handle
    IO_STATUS_BLOCK status_block = {};
    if (const NTSTATUS status = NtOpenFile(&m_handle, FILE_LIST_DIRECTORY | SYNCHRONIZE,
        &attributes, &status_block, FILE_SHARE_READ | FILE_SHARE_WRITE, 
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT); status != 0)
    {
        VTRACE(L"File Access Error (%08X): %s", status, m_base.GetBuffer());
        return FALSE;
    }

    // do initial search
    return FindNextFile();
}

bool FileFindEnhanced::IsDirectory() const
{
    return (m_current_info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileFindEnhanced::IsDots() const
{
    return m_name == L"." || m_name == L"..";
}

bool FileFindEnhanced::IsHidden() const
{
    return (m_current_info->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool FileFindEnhanced::IsHiddenSystem() const
{
    constexpr DWORD hidden_system = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (m_current_info->FileAttributes & hidden_system) == hidden_system;
}

bool FileFindEnhanced::IsProtectedReparsePoint() const
{
    constexpr DWORD protect = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT;
    return (m_current_info->FileAttributes & protect) == protect;
}

DWORD FileFindEnhanced::GetAttributes() const
{
    return m_current_info->FileAttributes;
}

CStringW FileFindEnhanced::GetFileName() const
{
    return m_name;
}

ULONGLONG FileFindEnhanced::GetLogicalFileSize() const
{
    return m_current_info->EndOfFile.QuadPart;
}

ULONGLONG FileFindEnhanced::GetFileSize() const
{
    // Optionally retrieve the compressed file size
    if (m_current_info->FileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE) && COptions::ShowUncompressedFileSizes)
    {
        return m_current_info->EndOfFile.QuadPart;
    }

    return m_current_info->AllocationSize.QuadPart;
}

FILETIME FileFindEnhanced::GetLastWriteTime() const
{
    return { m_current_info->LastWriteTime.LowPart,
        static_cast<DWORD>(m_current_info->LastWriteTime.HighPart) };
}

CStringW FileFindEnhanced::GetFilePath() const
{
    if (m_base.GetAt(m_base.GetLength() - 1) == L'\\') return m_base + m_name;
    return m_base + L"\\" + m_name;
}

CStringW FileFindEnhanced::GetFileLongPath() const
{
    return GetLongPathCompatible(StripDosPathCharts(GetFilePath()));
}

CStringW FileFindEnhanced::StripDosPathCharts(const CStringW& path)
{
    if (wcsncmp(path.GetString(), m_dos, wcslen(m_dos) - 1) == 0)
        return path.Mid(static_cast<int>(wcslen(m_dos)));

    if (wcsncmp(path.GetString(), m_dosunc, wcslen(m_dosunc) - 1) == 0)
        return path.Mid(static_cast<int>(wcslen(m_dosunc)));
    return path;
}

CStringW FileFindEnhanced::GetLongPathCompatible(const CStringW & path)
{
    CStringW ret;
    if (path.Find(L":\\", 1) == 1) return { L"\\\\?\\" + path };
    if (path.Find(L"\\\\") == 0) return { L"\\\\?\\UNC\\" + path.Mid(2) };
    return path;
}

bool FileFindEnhanced::DoesFileExist(const CStringW& folder, const CStringW& file)
{
    FileFindEnhanced finder;
    return finder.FindFile(folder, file);
}
