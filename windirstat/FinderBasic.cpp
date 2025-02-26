// FinderBasic.cpp - Declaration of CFinderBasic
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "FinderBasic.h"
#include "Options.h"
#include "Tracer.h"

#pragma comment(lib,"ntdll.lib")

NTSTATUS(WINAPI* NtQueryDirectoryFile)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
    ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName, BOOLEAN RestartScan) = reinterpret_cast<decltype(NtQueryDirectoryFile)>(
        static_cast<LPVOID>(GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtQueryDirectoryFile")));

FinderBasic::~FinderBasic()
{
    if (m_Handle != nullptr) NtClose(m_Handle);
}

bool FinderBasic::FindNextFile()
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
        std::wmemcpy(m_Name.data(), m_CurrentInfo->FileName, nameLength);

        // special case for reparse on initial run points since it will
        // return the attributes on the destination folder and not the reparse
        // point attributes itself that we want
        if (m_Firstrun)
        {
            // Use cached value passed in from previous capture
            if (m_Name == L".")
            {
                m_CurrentInfo->FileAttributes = m_InitialAttributes;
            }

            // Fallback if cached value was not passed
            if (m_CurrentInfo->FileAttributes == INVALID_FILE_ATTRIBUTES)
            {
                std::wstring initialPath = GetFilePathLong();
                if (IsDots()) initialPath.pop_back();
                m_CurrentInfo->FileAttributes = GetFileAttributes(initialPath.c_str());
            }
        }
    }

    m_Firstrun = false;
    return success;
}

bool FinderBasic::FindFile(const std::wstring & strFolder, const std::wstring& strName, const DWORD attr)
{
    // stash the search pattern for later use
    m_Search = strName;
    m_InitialAttributes = attr;

    // convert the path to a long path that is compatible with the other call
    m_Base = strFolder;
    if (m_Base.find(L":\\", 1) == 1) m_Base = m_Dos.data() + m_Base;
    else if (m_Base.starts_with(L"\\\\?\\")) m_Base = m_Dos.data() + m_Base.substr(4) + L"\\";
    else if (m_Base.starts_with(L"\\\\")) m_Base = m_DosUNC.data() + m_Base.substr(2);
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

bool FinderBasic::IsDirectory() const
{
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FinderBasic::IsDots() const
{
    return m_Name == L"." || m_Name == L"..";
}

bool FinderBasic::IsHidden() const
{
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool FinderBasic::IsHiddenSystem() const
{
    constexpr DWORD hiddenSystem = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (m_CurrentInfo->FileAttributes & hiddenSystem) == hiddenSystem;
}

bool FinderBasic::IsProtectedReparsePoint() const
{
    constexpr DWORD protect = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT;
    return (m_CurrentInfo->FileAttributes & protect) == protect;
}

DWORD FinderBasic::GetAttributes() const
{
    return m_CurrentInfo->FileAttributes;
}

std::wstring FinderBasic::GetFileName() const
{
    return m_Name;
}

ULONGLONG FinderBasic::GetFileSizePhysical() const
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

ULONGLONG FinderBasic::GetFileSizeLogical() const
{
    return m_CurrentInfo->EndOfFile.QuadPart;
}

FILETIME FinderBasic::GetLastWriteTime() const
{
    return { m_CurrentInfo->LastWriteTime.LowPart,
        static_cast<DWORD>(m_CurrentInfo->LastWriteTime.HighPart) };
}

std::wstring FinderBasic::GetFilePath() const
{
    // Get full path to folder or file
    std::wstring path = m_Base.back() == L'\\'
        ? (m_Base + m_Name)
        : (m_Base + L"\\" + m_Name);

    // Strip special DOS chars
    if (path.starts_with(m_DosUNC)) return L"\\\\" + path.substr(m_DosUNC.size());
    if (path.starts_with(m_Dos)) return path.substr(m_Dos.size());
    return path;
}

std::wstring FinderBasic::GetFilePathLong() const
{
    return MakeLongPathCompatible(GetFilePath());
}

std::wstring FinderBasic::MakeLongPathCompatible(const std::wstring & path)
{
    if (path.find(L":\\", 1) == 1) return m_Long.data() + path;
    if (path.starts_with(L"\\\\?")) return path;
    if (path.starts_with(L"\\\\")) return m_LongUNC.data() + path.substr(2);
    return path;
}

bool FinderBasic::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    FinderBasic finder;
    return finder.FindFile(folder, file);
}
