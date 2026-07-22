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
    const bool firstRun = (m_currentInfo == nullptr);
    bool success = false;
    if (firstRun || m_currentInfo->NextEntryOffset == 0)
    {
        UNICODE_STRING uSearch
        {
            .Length = static_cast<USHORT>(m_search.size() * sizeof(WCHAR)),
            .MaximumLength = static_cast<USHORT>((m_search.size() + 1) * sizeof(WCHAR)),
            .Buffer = m_search.data()
        };

        constexpr auto LOCAL_BUFFER_SIZE = static_cast<ULONG>(4 * 1024 * 1024);
        constexpr auto REMOTE_BUFFER_SIZE = static_cast<ULONG>(64 * 1024);
        thread_local std::vector<LARGE_INTEGER> m_directoryInfo(LOCAL_BUFFER_SIZE / sizeof(LARGE_INTEGER));
        constexpr auto FileFullDirectoryInformation = 2;
        constexpr auto FileIdFullDirectoryInformation = 38;
        IO_STATUS_BLOCK IoStatusBlock;

        std::call_once(m_context->InitOnce, [&]
        {
            // Larger buffers fail on older network redirectors (issue #631).
            m_context->IsRemoteVolume = m_isUncPath || (m_base.find(L":\\", 1) == 1 &&
                GetDriveType(m_base.substr(0, 3).c_str()) == DRIVE_REMOTE);
            const ULONG bufferSize = m_context->IsRemoteVolume ? REMOTE_BUFFER_SIZE : LOCAL_BUFFER_SIZE;

            if (!m_isUncPath)
            {
                const NTSTATUS status = NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
                    m_directoryInfo.data(), bufferSize,
                    static_cast<FILE_INFORMATION_CLASS>(FileIdFullDirectoryInformation), FALSE,
                    (uSearch.Length > 0) ? &uSearch : nullptr, TRUE);
                m_context->SupportsFileId = (status == 0);

                DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
                if (GetDiskFreeSpace(m_base.c_str(), &sectorsPerCluster, &bytesPerSector,
                    &numberOfFreeClusters, &totalNumberOfClusters))
                {
                    m_context->ClusterSize = sectorsPerCluster * bytesPerSector;
                }
            }
        });

        const ULONG bufferSize = m_context->IsRemoteVolume ? REMOTE_BUFFER_SIZE : LOCAL_BUFFER_SIZE;
        const auto QueryDirectory = [&](const FILE_INFORMATION_CLASS infoClass)
        {
            return NtQueryDirectoryFile(m_handle, nullptr, nullptr, nullptr, &IoStatusBlock,
                m_directoryInfo.data(), bufferSize, infoClass, FALSE,
                (uSearch.Length > 0) ? &uSearch : nullptr, firstRun ? TRUE : FALSE);
        };

        constexpr NTSTATUS STATUS_INVALID_INFO_CLASS = static_cast<NTSTATUS>(0xC0000003L);
        constexpr NTSTATUS STATUS_NOT_SUPPORTED = static_cast<NTSTATUS>(0xC00000BBL);

        NTSTATUS status = QueryDirectory(static_cast<FILE_INFORMATION_CLASS>(
            m_context->SupportsFileId ? FileIdFullDirectoryInformation : FileFullDirectoryInformation));

        if (status == STATUS_INVALID_PARAMETER || status == STATUS_INVALID_INFO_CLASS || status == STATUS_NOT_SUPPORTED)
        {
            m_context->SupportsFileId = false;
            status = QueryDirectory(static_cast<FILE_INFORMATION_CLASS>(FileFullDirectoryInformation));
        }

        m_currentInfo = std::assume_aligned<8>(reinterpret_cast<FILE_DIR_INFORMATION*>(m_directoryInfo.data()));
        success = (status == 0);
    }
    else
    {
        m_currentInfo = std::assume_aligned<8>(ByteOffset<FILE_DIR_INFORMATION>(m_currentInfo, m_currentInfo->NextEntryOffset));
        success = true;
    }

    if (success)
    {
        std::optional<std::wstring> filePathLong;
        const auto GetFilePathLongCached = [&]() -> const std::wstring&
        {
            if (!filePathLong.has_value()) filePathLong = GetFilePathLong();
            return filePathLong.value();
        };

        // handle unexpected trailing null on some file systems
        const LPCWSTR fileNamePtr = m_context->SupportsFileId ? m_currentInfo->IdInfo.FileName :
            m_currentInfo->StandardInfo.FileName;
        ULONG nameLength = m_currentInfo->FileNameLength / sizeof(WCHAR);
        if (nameLength > 1 && fileNamePtr[nameLength - 1] == L'\0')
            nameLength -= 1;

        // copy name into local buffer
        m_name.resize(nameLength);
        std::wmemcpy(m_name.data(), fileNamePtr, nameLength);

        // special case for reparse points on the initial run since it will
        // return the attributes on the destination folder and not the reparse
        // point attributes itself that we want
        if (firstRun)
        {
            // Use cached value passed in from previous capture
            if (m_name == L".")
            {
                m_currentInfo->FileAttributes = m_initialAttributes;
            }

            // Fallback if cached value was not passed
            if (m_currentInfo->FileAttributes == INVALID_FILE_ATTRIBUTES)
            {
                std::wstring initialPath = GetFilePathLongCached();
                if (m_name == L"." || m_name == L"..") initialPath.pop_back();
                m_currentInfo->FileAttributes = GetFileAttributes(initialPath.c_str());
            }
        }

        // Handle reparse points
        m_reparseTag = (m_currentInfo->FileAttributes != INVALID_FILE_ATTRIBUTES &&
            m_currentInfo->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ?
            m_currentInfo->ReparsePointTag : 0;

        // Mark file as compressed WOF compressed - this does not always
        // seem to be set with WOF compressed files
        if (m_reparseTag == IO_REPARSE_TAG_WOF)
        {
            m_currentInfo->FileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
        }

        // NtQueryDirectoryFile returns IO_REPARSE_TAG_MOUNT_POINT for both volume mount
        // points and junctions. Read the reparse data buffer to tell them apart so that
        // junction-vs-mount-point exclusion options work correctly in all code paths.
        if (m_reparseTag == IO_REPARSE_TAG_MOUNT_POINT && IsDirectory() &&
            m_name != L"." && m_name != L"..")
        {
            if (const SmartPointer handle(CloseHandle, CreateFile(GetFilePathLongCached().c_str(),
                FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                handle != INVALID_HANDLE_VALUE)
            {
                DWORD returned = 0;
                if (auto buf = std::make_unique<std::array<BYTE, MAXIMUM_REPARSE_DATA_BUFFER_SIZE>>();
                    DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0,
                        buf->data(), static_cast<DWORD>(buf->size()), &returned, nullptr))
                {
                    auto& rp = *reinterpret_cast<Finder::REPARSE_DATA_BUFFER*>(buf->data());
                    if (Finder::IsJunction(rp))
                        m_reparseTag = IO_REPARSE_TAG_JUNCTION_POINT;
                }
            }
        }

        // Correct physical size. Skip for UNC paths: GetCompressedFileSize issues
        // IOCTLs (e.g. FileCompressionInformation) that some redirectors (RDP
        // tsclient) do not implement and may block indefinitely.
        if (m_currentInfo->FileAttributes != INVALID_FILE_ATTRIBUTES &&
            !(m_currentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            m_currentInfo->AllocationSize.QuadPart == 0 &&
            !m_isUncPath &&
            ((m_currentInfo->EndOfFile.QuadPart > m_context->ClusterSize ||
             (m_currentInfo->FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0) ||
             (m_currentInfo->FileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0))
        {
            DWORD highPart;
            const DWORD lowPart = GetCompressedFileSize(GetFilePathLongCached().c_str(), &highPart);
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
            const SmartPointer handle(CloseHandle, CreateFile(GetFilePathLongCached().c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));

            if (handle != INVALID_HANDLE_VALUE)
            {
                GetFileSizeEx(handle, &m_currentInfo->EndOfFile);
            }
        }
    }

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
    m_initialAttributes = attr;
    m_currentInfo = nullptr;
    m_reparseTag = 0;
    m_base = strFolder;
    m_search = strName;

    // handle request to just get directory info
    if (m_statMode && strName.empty())
    {
        const std::filesystem::path path(strFolder);
        m_base = path.parent_path().wstring();
        m_search = path.filename().wstring();
    }

    // normalize any long-path prefix back to a clean Win32 path so m_base stays
    // canonical for display and concatenation; the NT form is derived below
    if (m_base.starts_with(s_longUNCPath)) m_base = L"\\\\" + m_base.substr(s_longUNCPath.length());
    else if (m_base.starts_with(s_longPath)) m_base.erase(0, s_longPath.length());

    // ensure base ends with a separator so concatenation with the name is exact
    if (m_base.empty() || m_base.back() != L'\\') m_base += L'\\';

    // derive the NT-namespace path used to open the directory
    m_isUncPath = m_base.starts_with(L"\\\\");
    if (m_isUncPath) m_baseNt = s_dosUNCPath.data() + m_base.substr(2);
    else if (m_base.find(L":\\", 1) == 1) m_baseNt = s_dosPath.data() + m_base;
    else m_baseNt = m_base;

    UNICODE_STRING path
    {
        .Length = static_cast<USHORT>(m_baseNt.size() * sizeof(WCHAR)),
        .MaximumLength = static_cast<USHORT>((m_baseNt.size() + 1) * sizeof(WCHAR)),
        .Buffer = m_baseNt.data()
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
        VTRACE(L"File Access Error {:#08X}: {}", static_cast<DWORD>(status), m_baseNt.data());
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
    // m_base is kept in clean Win32 form (the NT \??\ form lives only in
    // m_baseNt), so the full path is a direct concatenation with the name
    return m_base + m_name;
}

DWORD FinderBasic::GetReparseTag() const
{
    return m_reparseTag;
}

bool FinderBasic::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    const std::filesystem::path p = file.empty()
        ? std::filesystem::path(folder)
        : std::filesystem::path(folder) / file;

    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    return FinderBasic(true).FindFile(
        p.parent_path().wstring(),
        p.filename().wstring());
}
