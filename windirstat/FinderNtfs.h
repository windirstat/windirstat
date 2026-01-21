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

class FinderNtfsContext final
{
    friend class FinderNtfs;

    using FileRecordBase = struct FileRecordBase
    {
        ULONGLONG LogicalSize = 0;
        ULONGLONG PhysicalSize = 0;
        FILETIME LastModifiedTime = {};
        ULONG Attributes = 0;
        DWORD ReparsePointTag = 0;
    };

    using FileRecordName = struct FileRecordName
    {
        std::wstring FileName;
        ULONGLONG BaseRecord;

        FileRecordName(std::wstring fileName, ULONGLONG baseRecord) : 
            FileName(std::move(fileName)), BaseRecord(baseRecord) {}
    };

    std::unordered_map<ULONGLONG, FileRecordBase> m_baseFileRecordMap;
    std::unordered_map<ULONGLONG, std::vector<FileRecordName>> m_parentToChildMap;

    mutable std::shared_mutex m_baseFileRecordMutex;
    mutable std::shared_mutex m_parentToChildMutex;

    bool m_isLoaded = false;

public:

    FinderNtfsContext() = default;
    bool LoadRoot(CItem* driveitem);
    bool IsLoaded() const { return m_isLoaded; };

    static constexpr ULONGLONG NtfsNodeRoot = 5;
    static constexpr ULONGLONG NtfsReservedMax = 16;
};

class FinderNtfs final : public Finder
{
    FinderNtfsContext* m_master = nullptr;
    FinderNtfsContext::FileRecordBase* m_currentRecord = nullptr;
    const FinderNtfsContext::FileRecordName* m_currentRecordName = nullptr;

    std::vector<FinderNtfsContext::FileRecordName>::const_iterator m_recordIteratorEnd;
    std::vector<FinderNtfsContext::FileRecordName>::const_iterator m_recordIterator;
    
    std::wstring m_base;
    ULONGLONG m_index = 0;

public:

    explicit FinderNtfs(FinderNtfsContext* master) : m_master(master) {}

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    DWORD GetAttributes() const override;
    ULONGLONG GetIndex() const override;
    DWORD GetReparseTag() const override;
    std::wstring GetFileName() const override;
    ULONGLONG GetFileSizePhysical() const override;
    ULONGLONG GetFileSizeLogical() const override;
    FILETIME GetLastWriteTime() const override;
    std::wstring GetFilePath() const override;
    bool IsReserved() const override;
};
