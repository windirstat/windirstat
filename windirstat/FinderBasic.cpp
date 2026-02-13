// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "FinderBasic.h"

#pragma comment(lib,"ntdll.lib")

static NTSTATUS(WINAPI* NtQueryDirectoryFile)(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation,
    ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName, BOOLEAN RestartScan) = reinterpret_cast<decltype(NtQueryDirectoryFile)>(
        static_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryDirectoryFile")));

bool FinderBasic::FindNext()
{
    bool success = false;
    if (m_firstRun || m_currentInfo->NextEntryOffset == 0)
    {
        UNICODE_STRING uSearch
        {
            .Length = static_cast<USHORT>(m_search.size() * sizeof(WCHAR)),
            .MaximumLength = static_cast<USHORT>((m_search.size() + 1) * sizeof(WCHAR)),
            .Buffer = m_search.data()
        };

        constexpr auto BUFFER_SIZE = static_cast<ULONG>(4 * 1024 * 1024);
        thread_local std::vector<LARGE_INTEGER> m_directoryInfo(BUFFER_SIZE / sizeof(LARGE_INTEGER));
        constexpr auto FileFullDirectoryInformation = 2;
        constexpr auto FileIdFullDirectoryInformation = 38;
        IO_STATUS_BLOCK IoStatusBlock;

        // Determine if volume supports FileId
        if (!m_context->Initialized)
        {
            const NTSTATUS Status = NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
                m_directoryInfo.data(), BUFFER_SIZE, static_cast<FILE_INFORMATION_CLASS>(FileIdFullDirectoryInformation),
                FALSE, (uSearch.Length > 0) ? &uSearch : nullptr, TRUE);
            m_context->SupportsFileId = (Status == 0);
            m_context->Initialized = true;

            // Query cluster size if not already known
            DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
            if (GetDiskFreeSpace(m_base.c_str(), &sectorsPerCluster, &bytesPerSector,
                &numberOfFreeClusters, &totalNumberOfClusters))
            {
                m_context->ClusterSize = sectorsPerCluster * bytesPerSector;
            }
        }

        // Query directory with appropriate information class
        const NTSTATUS Status = NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
            m_directoryInfo.data(), BUFFER_SIZE, static_cast<FILE_INFORMATION_CLASS>(
                m_context->SupportsFileId ? FileIdFullDirectoryInformation : FileFullDirectoryInformation),
            FALSE, (uSearch.Length > 0) ? &uSearch : nullptr, (m_firstRun) ? TRUE : FALSE);

        success = (Status == 0);
        m_currentInfo = std::assume_aligned<8>(reinterpret_cast<FILE_DIR_INFORMATION*>(m_directoryInfo.data()));
    }
    else
    {
        m_currentInfo = std::assume_aligned<8>(ByteOffset<FILE_DIR_INFORMATION>(m_currentInfo, m_currentInfo->NextEntryOffset));
        success = true;
    }

    if (success)
    {
        // handle unexpected trailing null on some file systems
        const LPCWSTR fileNamePtr = m_context->SupportsFileId ? m_currentInfo->IdInfo.FileName :
            m_currentInfo->StandardInfo.FileName;
        ULONG nameLength = m_currentInfo->FileNameLength / sizeof(WCHAR);
        if (nameLength > 1 && fileNamePtr[nameLength - 1] == L'\0')
            nameLength -= 1;

        // copy name into local buffer
        m_name.resize(nameLength);
        std::wmemcpy(m_name.data(), fileNamePtr, nameLength);

        // special case for reparse on initial run points since it will
        // return the attributes on the destination folder and not the reparse
        // point attributes itself that we want
        if (m_firstRun)
        {
            // Use cached value passed in from previous capture
            if (m_name == L".")
            {
                m_currentInfo->FileAttributes = m_initialAttributes;
            }

            // Fallback if cached value was not passed
            if (m_currentInfo->FileAttributes == INVALID_FILE_ATTRIBUTES)
            {
                std::wstring initialPath = GetFilePathLong();
                if (m_name == L"." || m_name == L"..") initialPath.pop_back();
                m_currentInfo->FileAttributes = GetFileAttributes(initialPath.c_str());
            }
        }

        // Handle reparse points
        m_reparseTag = m_currentInfo->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ?
            m_currentInfo->ReparsePointTag : 0;

        // Mark file as compressed WOF compressed - this does not always
        // seem to be set with WOF compressed files 
        if (m_reparseTag == IO_REPARSE_TAG_WOF)
        {
            m_currentInfo->FileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
        }

        // Correct physical size
        if (!(m_currentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            m_currentInfo->AllocationSize.QuadPart == 0 &&
            ((m_currentInfo->EndOfFile.QuadPart > m_context->ClusterSize ||
             (m_currentInfo->FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0) ||
             (m_currentInfo->FileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0))
        {
            DWORD highPart;
            const DWORD lowPart = GetCompressedFileSize(GetFilePathLong().c_str(), &highPart);
            if (lowPart != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
            {
                m_currentInfo->AllocationSize.LowPart = lowPart;
                m_currentInfo->AllocationSize.HighPart = static_cast<LONG>(highPart);
            }
        }

        // Correct logical size
        if (m_currentInfo->EndOfFile.QuadPart == 0 &&
            m_currentInfo->AllocationSize.QuadPart != 0)
        {
            const SmartPointer<HANDLE> handle(CloseHandle, CreateFile(GetFilePathLong().c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));

            if (handle != INVALID_HANDLE_VALUE)
            {
                GetFileSizeEx(handle, &m_currentInfo->EndOfFile);
            }
        }
    }

    m_firstRun = false;

    if (success && !m_statMode && (m_name == L"." || m_name == L"..")) return FindNext();
    else return success;
}

bool FinderBasic::FindFile(const CItem* item)
{
    return FindFile(item->GetPath(), L"", item->GetAttributes());
}

bool FinderBasic::FindFile(const std::wstring & strFolder, const std::wstring& strName, const DWORD attr)
{
    // initialize run
    m_firstRun = true;
    m_initialAttributes = attr;
    m_base = strFolder;
    m_search = strName;

    // handle request to just get directory info
    if (m_statMode && strName.empty())
    {
        const std::filesystem::path path(strFolder);
        m_base = path.parent_path();
        m_search = path.filename();
    }

    // convert the path to a long path that is compatible with the other call
    if (m_base.find(L":\\", 1) == 1) m_base = s_dosPath.data() + m_base;
    else if (m_base.starts_with(L"\\\\?\\")) m_base = s_dosPath.data() + m_base.substr(4) + L"\\";
    else if (m_base.starts_with(L"\\\\")) m_base = s_dosUNCPath.data() + m_base.substr(2);
    UNICODE_STRING path
    {
        .Length = static_cast<USHORT>(m_base.size() * sizeof(WCHAR)),
        .MaximumLength = static_cast<USHORT>((m_base.size() + 1) * sizeof(WCHAR)),
        .Buffer = m_base.data()
    };

    // update object attributes object
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, nullptr, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    attributes.ObjectName = &path;

    // Close previous file handle if necessary
    m_handle.Release();

    // get an open file handle
    IO_STATUS_BLOCK statusBlock = {};
    if (const NTSTATUS status = NtOpenFile(&m_handle, FILE_LIST_DIRECTORY | SYNCHRONIZE,
        &attributes, &statusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT); status != 0)
    {
        VTRACE(L"File Access Error {:#08X}: {}", static_cast<DWORD>(status), m_base.data());
        return FALSE;
    }

    // do initial search
    return FindNext();
}

DWORD FinderBasic::GetAttributes() const
{
    return m_currentInfo->FileAttributes;
}

std::wstring FinderBasic::GetFileName() const
{
    return m_name;
}

ULONGLONG FinderBasic::GetFileSizePhysical() const
{
    return m_currentInfo->AllocationSize.QuadPart;
}

ULONGLONG FinderBasic::GetFileSizeLogical() const
{
    return m_currentInfo->EndOfFile.QuadPart;
}

ULONGLONG FinderBasic::GetIndex() const
{
    return m_context->SupportsFileId ? m_currentInfo->IdInfo.FileId.QuadPart : 0;
}

FILETIME FinderBasic::GetLastWriteTime() const
{
    return { m_currentInfo->LastWriteTime.LowPart,
        static_cast<DWORD>(m_currentInfo->LastWriteTime.HighPart) };
}

std::wstring FinderBasic::GetFilePath() const
{
    // Get full path to folder or file
    std::wstring path = m_base.back() == L'\\'
        ? (m_base + m_name)
        : (m_base + L"\\" + m_name);

    // Strip special DOS chars
    if (path.starts_with(s_dosUNCPath)) return L"\\\\" + path.substr(s_dosUNCPath.length());
    if (path.starts_with(s_dosPath)) return path.substr(s_dosPath.length());
    return path;
}

DWORD FinderBasic::GetReparseTag() const
{
    return m_reparseTag;
}

bool FinderBasic::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    const std::filesystem::path p = file.empty()
        ? std::filesystem::path::path(folder)
        : std::filesystem::path::path(folder) / file;

    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    return FinderBasic(true).FindFile(
        p.parent_path().wstring(),
        p.filename().wstring());
}
