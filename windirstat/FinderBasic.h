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

class FinderBasicContext final
{
public:
    bool Initialized = false;
    bool VolumeSupportsFileId = false;
};

class FinderBasic final : public Finder
{
    using FILE_DIR_INFORMATION = struct {
        ULONG         NextEntryOffset;
        ULONG         FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG         FileAttributes;
        ULONG         FileNameLength;
        ULONG         ReparsePointTag;
        union
        {
            struct
            {
                WCHAR         FileName[1];
            }
            StandardInfo;
            
            struct
            {        
                LARGE_INTEGER FileId;
                WCHAR         FileName[1];
            }
            IdInfo;
        };
    };

    std::wstring m_Search;
    std::wstring m_Base;
    std::wstring m_Name;
    std::vector<BYTE> m_DirectoryInfo;
    FILE_DIR_INFORMATION* m_CurrentInfo = nullptr;
    FinderBasicContext* m_Context = nullptr;
    HANDLE m_Handle = nullptr;
    DWORD m_InitialAttributes = INVALID_FILE_ATTRIBUTES;
    DWORD m_ReparseTag = 0;
    bool m_Firstrun = true;
    bool m_UseFileId = false;

public:

    FinderBasic() = default;
    FinderBasic(FinderBasicContext* context) : m_Context(context) {}
    ~FinderBasic();

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    bool FindFile(const std::wstring& strFolder, const std::wstring& strName = L"", DWORD attr = INVALID_FILE_ATTRIBUTES);
    bool IsDots() const override;
    inline DWORD GetAttributes() const override;
    inline std::wstring GetFileName() const override;
    inline ULONGLONG GetFileSizePhysical() const override;
    inline ULONGLONG GetFileSizeLogical() const override;
    inline FILETIME GetLastWriteTime() const override;
    std::wstring GetFilePath() const override;
    inline ULONG GetIndex() const override;
    inline DWORD GetReparseTag() const override;

    static bool DoesFileExist(const std::wstring& folder, const std::wstring& file = {});
};
