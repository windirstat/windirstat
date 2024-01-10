// FileFindWDS.h - Declaration of CFileFindWDS
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
#include <common/Tracer.h>

#pragma comment(lib,"ntdll.lib")

static HMODULE rtl_library = LoadLibrary(L"ntdll.dll");

static NTSTATUS(WINAPI* NtQueryDirectoryFile)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
    ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName, BOOLEAN RestartScan) = (decltype(NtQueryDirectoryFile))GetProcAddress(rtl_library, "NtQueryDirectoryFile");

FileFindEnhanced::FileFindEnhanced()
{
    m_directory_info = new BYTE[BUFFER_SIZE];
}

FileFindEnhanced::~FileFindEnhanced()
{
    delete [] m_directory_info;
    if (m_handle != nullptr) NtClose(m_handle);
}

BOOL FileFindEnhanced::GetLastWriteTime(FILETIME* pTimeStamp) const
{
    pTimeStamp->dwLowDateTime = m_current_info->LastWriteTime.LowPart;
    pTimeStamp->dwHighDateTime = m_current_info->LastWriteTime.HighPart;
    return true;
}

BOOL FileFindEnhanced::FindNextFile()
{
    BOOL success = FALSE;
    if (m_firstrun || m_current_info->NextEntryOffset == 0)
    {
        // enumerate files in the directory
        constexpr auto FileDirectoryInformation = 1;
        IO_STATUS_BLOCK IoStatusBlock;
        const NTSTATUS Status = NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
            m_directory_info, BUFFER_SIZE, static_cast<FILE_INFORMATION_CLASS>(FileDirectoryInformation),
            FALSE, nullptr, (m_firstrun) ? TRUE : FALSE);

        // disable for next run
        m_current_info = reinterpret_cast<FILE_DIRECTORY_INFORMATION*>(m_directory_info);
        m_firstrun     = false;
        success        = (Status == 0);
    }
    else
    {
        m_current_info = (FILE_DIRECTORY_INFORMATION*)(&((BYTE*)(m_current_info))[m_current_info->NextEntryOffset]);
        success = true;
    }

    if (success)
    {
        const LPWSTR tmp = m_name.GetBufferSetLength(m_current_info->FileNameLength / sizeof(WCHAR));
        memcpy(tmp, m_current_info->FileName, m_current_info->FileNameLength);
    }

    return success;
}

BOOL FileFindEnhanced::FindFile(const CStringW & strName)
{
    // convert the path to a long path that is compatible with the other call
    m_base = strName;
    if (m_base.Find(L":\\", 1) == 1) m_base = L"\\??\\" + m_base;
    else if (m_base.Find(L"\\\\") == 0) m_base = L"\\??\\UNC\\" + m_base.Mid(2);
    UNICODE_STRING u_path = {};
    u_path.Length = static_cast<USHORT>(m_base.GetLength() * sizeof(WCHAR));
    u_path.MaximumLength = static_cast<USHORT>(m_base.GetLength() + 1) * sizeof(WCHAR);
    u_path.Buffer = m_base.GetBuffer();

    // update object attributes object
    OBJECT_ATTRIBUTES attributes = {};
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

BOOL FileFindEnhanced::IsDirectory() const
{
    return (m_current_info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

BOOL FileFindEnhanced::IsDots() const
{
    return (m_name == L"." || m_name == L"..");
}

BOOL FileFindEnhanced::IsHidden() const
{
    return (m_current_info->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

DWORD FileFindEnhanced::GetAttributes() const
{
    return m_current_info->FileAttributes;
}

CStringW FileFindEnhanced::GetFileName() const
{
    return m_name;
}

ULONGLONG FileFindEnhanced::GetCompressedLength() const
{
    return m_current_info->AllocationSize.QuadPart;
}

CStringW FileFindEnhanced::GetFilePath() const
{
    return m_base;
}

CStringW FileFindEnhanced::GetLongPathCompatible(const CStringW & path)
{
    CStringW ret;
    if (path.Find(L":\\", 1) == 1) ret = L"\\\\?\\" + path;
    else if (path.Find(L"\\\\") == 0) ret = L"\\\\?\\UNC\\" + path.Mid(2);
    else
    {
        ASSERT(0);
    }

    return ret;
}
