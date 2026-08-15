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

#pragma once

#include "pch.h"
#include "Finder.h"
#include "SmartPointer.h"

class FinderBasicContext final
{
public:
    std::atomic<bool> SupportsFileId = false;
    ULONG ClusterSize = 0;
    bool IsRemoteVolume = false;
    std::once_flag InitOnce;
};

class FinderBasic final : public Finder
{
    #pragma pack(push, 1)
    using FILE_DIR_INFORMATION = struct
    {
        ULONG NextEntryOffset;
        ULONG FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG FileAttributes;
        ULONG FileNameLength;
        ULONG ReparsePointTag;
        union
        {
            struct
            {
                WCHAR FileName[1];
            }
            StandardInfo;

            struct
            {
                BYTE Padding[4];
                LARGE_INTEGER FileId;
                WCHAR FileName[1];
            }
            IdInfo;
        };
    };
    #pragma pack(pop)

    std::wstring m_search;
    std::wstring m_base;
    std::wstring m_baseNt;
    std::wstring m_name;
    FILE_DIR_INFORMATION* m_currentInfo = nullptr;
    FinderBasicContext m_default{};
    FinderBasicContext* m_context = &m_default;
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_handle{CloseHandle, HANDLE{}};
    DWORD m_initialAttributes = INVALID_FILE_ATTRIBUTES;
    DWORD m_reparseTag = 0;
    bool m_statMode = false;
    bool m_isUncPath = false;

public:

    FinderBasic() = default;
    FinderBasic(const bool statMode) : m_statMode(statMode) {}
    FinderBasic(FinderBasicContext* context) : m_context(context) {}
    ~FinderBasic() override = default;

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    bool FindFile(const std::wstring& strFolder, const std::wstring& strName = L"", DWORD attr = INVALID_FILE_ATTRIBUTES);
    DWORD GetAttributes() const override { return m_currentInfo->FileAttributes; }
    std::wstring GetFileName() const override { return m_name; }
    ULONGLONG GetFileSizePhysical() const override { return m_currentInfo->AllocationSize.QuadPart; }
    ULONGLONG GetFileSizeLogical() const override { return m_currentInfo->EndOfFile.QuadPart; }
    FILETIME GetLastWriteTime() const override { return std::bit_cast<FILETIME>(m_currentInfo->LastWriteTime); }
    // m_base is kept in clean Win32 form (the NT \??\ form lives only in
    // m_baseNt), so the full path is a direct concatenation with the name
    std::wstring GetFilePath() const override { return m_base + m_name; }
    ULONGLONG GetIndex() const override;
    DWORD GetReparseTag() const override { return m_reparseTag; }
    bool IsReserved() const override { return false; }

    static bool DoesFileExist(const std::wstring& folder, const std::wstring& file = {});
};
