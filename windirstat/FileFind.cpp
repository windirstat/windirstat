// FileFind.cpp - Declaration of CFileFindEnhanced
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

#include "FileFind.h"
#include "Options.h"
#include "Tracer.h"

#pragma comment(lib,"ntdll.lib")

NTSTATUS(WINAPI* NtQueryDirectoryFile)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
    ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName, BOOLEAN RestartScan) = reinterpret_cast<decltype(NtQueryDirectoryFile)>(
        static_cast<LPVOID>(GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtQueryDirectoryFile")));

FileFindEnhanced::~FileFindEnhanced()
{
    if (m_Handle != nullptr) NtClose(m_Handle);
}

bool FileFindEnhanced::FindNextFile()
{
    bool success = false;
    if (m_Firstrun || m_CurrentInfo->NextEntryOffset == 0)
    {
        constexpr auto BUFFER_SIZE = 64 * 1024;
        thread_local std::vector<BYTE> m_DirectoryInfo(BUFFER_SIZE);

        // handle optional pattern mask
        UNICODE_STRING uSearch;
        uSearch.Length = static_cast<USHORT>(m_Search.size() * sizeof(WCHAR));
        uSearch.MaximumLength = static_cast<USHORT>(m_Search.size() + 1) * sizeof(WCHAR);
        uSearch.Buffer = m_Search.data();

        // enumerate files in the directory
        constexpr auto FileFullDirectoryInformation = 2;
        IO_STATUS_BLOCK IoStatusBlock;
        const NTSTATUS Status = NtQueryDirectoryFile(m_Handle, nullptr, nullptr, nullptr, &IoStatusBlock,
            m_DirectoryInfo.data(), BUFFER_SIZE, static_cast<FILE_INFORMATION_CLASS>(FileFullDirectoryInformation),
            FALSE, (uSearch.Length > 0) ? &uSearch : nullptr, (m_Firstrun) ? TRUE : FALSE);

        // fetch point to current node 
        success = (Status == 0);
        m_CurrentInfo = reinterpret_cast<FILE_FULL_DIR_INFORMATION*>(m_DirectoryInfo.data());
    }
    else
    {
        m_CurrentInfo = reinterpret_cast<FILE_FULL_DIR_INFORMATION*>(
            &reinterpret_cast<BYTE*>(m_CurrentInfo)[m_CurrentInfo->NextEntryOffset]);
        success = true;
    }

    if (success)
    {
        // handle unexpected trailing null on some file systems
        ULONG nameLength = m_CurrentInfo->FileNameLength / sizeof(WCHAR);
        if (nameLength > 1 && m_CurrentInfo->FileName[nameLength - 1] == L'\0')
            nameLength -= 1;

        // copy name into local buffer
        m_Name.resize(nameLength);
        std::memcpy(m_Name.data(), m_CurrentInfo->FileName, nameLength * sizeof(WCHAR));

        // special case for reparse on initial run points - update attributes
        if (m_Firstrun)
        {
            std::wstring initialPath = GetFilePathLong();
            if (IsDots()) initialPath.pop_back();
            m_CurrentInfo->FileAttributes = GetFileAttributes(initialPath.c_str());
        }
    }

    m_Firstrun = false;
    return success;
}

bool FileFindEnhanced::FindFile(const std::wstring & strFolder, const std::wstring& strName)
{
    // stash the search pattern for later use
    m_Search = strName;

    // convert the path to a long path that is compatible with the other call
    m_Base = strFolder;
    if (m_Base.find(L":\\", 1) == 1) m_Base = m_Dos + m_Base;
    else if (m_Base.starts_with(L"\\\\")) m_Base = m_DosUNC + m_Base.substr(2);
    UNICODE_STRING path;
    path.Length = static_cast<USHORT>(m_Base.size() * sizeof(WCHAR));
    path.MaximumLength = static_cast<USHORT>(m_Base.size() + 1) * sizeof(WCHAR);
    path.Buffer = m_Base.data();

    // update object attributes object
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, nullptr, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    attributes.ObjectName = &path;

    // get an open file handle
    IO_STATUS_BLOCK statusBlock = {};
    if (const NTSTATUS status = NtOpenFile(&m_Handle, FILE_LIST_DIRECTORY | SYNCHRONIZE,
        &attributes, &statusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT); status != 0)
    {
        VTRACE(L"File Access Error {:#08X}: {}", static_cast<DWORD>(status), m_Base.data());
        return FALSE;
    }

    // do initial search
    return FindNextFile();
}

bool FileFindEnhanced::IsDirectory() const
{
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileFindEnhanced::IsDots() const
{
    return m_Name == L"." || m_Name == L"..";
}

bool FileFindEnhanced::IsHidden() const
{
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool FileFindEnhanced::IsHiddenSystem() const
{
    constexpr DWORD hiddenSystem = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (m_CurrentInfo->FileAttributes & hiddenSystem) == hiddenSystem;
}

bool FileFindEnhanced::IsProtectedReparsePoint() const
{
    constexpr DWORD protect = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT;
    return (m_CurrentInfo->FileAttributes & protect) == protect;
}

DWORD FileFindEnhanced::GetAttributes() const
{
    return m_CurrentInfo->FileAttributes;
}

std::wstring FileFindEnhanced::GetFileName() const
{
    return m_Name;
}

ULONGLONG FileFindEnhanced::GetFileSizePhysical() const
{
    if (m_CurrentInfo->AllocationSize.QuadPart == 0 &&
        m_CurrentInfo->EndOfFile.QuadPart != 0)
    {
        DWORD highPart;
        DWORD lowPart = GetCompressedFileSize(GetFilePathLong().c_str(), &highPart);
        if (lowPart != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
        {
            m_CurrentInfo->AllocationSize.LowPart = lowPart;
            m_CurrentInfo->AllocationSize.HighPart = static_cast<LONG>(highPart);
        }
    }

    return m_CurrentInfo->AllocationSize.QuadPart;
}

ULONGLONG FileFindEnhanced::GetFileSizeLogical() const
{
    return m_CurrentInfo->EndOfFile.QuadPart;
}

FILETIME FileFindEnhanced::GetLastWriteTime() const
{
    return { m_CurrentInfo->LastWriteTime.LowPart,
        static_cast<DWORD>(m_CurrentInfo->LastWriteTime.HighPart) };
}

std::wstring FileFindEnhanced::GetFilePath() const
{
    // Get full path to folder or file
    std::wstring path = (m_Base.at(m_Base.size() - 1) == L'\\') ?
        (m_Base + m_Name) : (m_Base + L"\\" + m_Name);

    // Strip special dos chars
    if (wcsncmp(path.data(), m_DosUNC, wcslen(m_DosUNC) - 1) == 0)
        path = L"\\\\" + path.substr(static_cast<int>(wcslen(m_DosUNC)));
    else if (wcsncmp(path.data(), m_Dos, wcslen(m_Dos) - 1) == 0)
        path = path.substr(static_cast<int>(wcslen(m_Dos)));
    return path;
}

std::wstring FileFindEnhanced::GetFilePathLong() const
{
    return MakeLongPathCompatible(GetFilePath());
}

std::wstring FileFindEnhanced::MakeLongPathCompatible(const std::wstring & path)
{
    if (path.find(L":\\", 1) == 1) return { m_Long + path };
    if (path.starts_with(L"\\\\")) return { m_LongUNC + path.substr(2) };
    return path;
}

bool FileFindEnhanced::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    FileFindEnhanced finder;
    return finder.FindFile(folder, file);
}
