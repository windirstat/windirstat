// FinderNtfs.h - Declaration of FinderNtfs
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

#pragma once

#include <stdafx.h>
#include "Finder.h"
#include "Item.h"

#include <unordered_map>
#include <string>
#include <set>

class FinderNtfsContext final
{
public:

    using FileRecordBase = struct FileRecordBase
    {
        ULONGLONG LogicalSize = 0;
        ULONGLONG PhysicalSize = 0;
        FILETIME LastModifiedTime = {};
        ULONG Attributes = 0;
    };

    using FileRecordName = struct FileRecordName
    {
        std::wstring FileName;
        ULONGLONG BaseRecord;

        bool operator<(const FileRecordName& other) const
        {
            return BaseRecord != other.BaseRecord ? BaseRecord < other.BaseRecord : FileName < other.FileName;
        }
    };

    std::unordered_map<ULONGLONG, FileRecordBase> m_BaseFileRecordMap;
    std::unordered_map<ULONGLONG, ULONGLONG> m_NonBaseToBaseMap;
    std::unordered_map<ULONGLONG, std::set<FileRecordName>> m_ParentToChildMap;

    bool LoadRoot(CItem* driveitem);
};

class FinderNtfs final : public Finder
{
    FinderNtfsContext* m_Master = nullptr;
    const FinderNtfsContext::FileRecordBase* m_CurrentRecord = nullptr;
    const FinderNtfsContext::FileRecordName* m_CurrentRecordName = nullptr;

    const std::set<FinderNtfsContext::FileRecordName>* m_ChildrenSet = nullptr;
    std::set<FinderNtfsContext::FileRecordName>::iterator m_RecordIterator;
    
    std::wstring m_Base;
    ULONGLONG m_Index = 0;

public:

    explicit FinderNtfs(FinderNtfsContext* master) : m_Master(master) {}

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    DWORD GetAttributes() const override;
    ULONG GetIndex() const override;
    std::wstring GetFileName() const override;
    ULONGLONG GetFileSizePhysical() const override;
    ULONGLONG GetFileSizeLogical() const override;
    FILETIME GetLastWriteTime() const override;
    std::wstring GetFilePath() const override;
};
