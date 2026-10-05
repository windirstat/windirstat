// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
        bool HasIgnoredStream = false;
        bool HasWofData = false;
    };

    using FileRecordName = struct FileRecordName
    {
        std::wstring FileName;
        ULONGLONG FileReference;

        FileRecordName(std::wstring fileName, const ULONGLONG fileReference) :
            FileName(std::move(fileName)), FileReference(fileReference) {}
    };

    std::unordered_map<ULONGLONG, FileRecordBase> m_baseFileRecordMap;
    std::unordered_map<ULONGLONG, std::vector<FileRecordName>> m_parentToChildMap;

    mutable std::shared_mutex m_baseFileRecordMutex;
    mutable std::shared_mutex m_parentToChildMutex;

    bool m_isLoaded = false;

public:

    FinderNtfsContext() = default;
    bool LoadRoot(CItem* driveitem, BlockingQueue<CItem*>* queue);
    bool IsLoaded() const { return m_isLoaded; }

    static constexpr ULONGLONG NtfsRecordMask = (1ull << 48) - 1;
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
    ScanResult m_result{ ScanOutcome::Complete, {} };

public:

    explicit FinderNtfs(FinderNtfsContext* master) : m_master(master) {}

    bool FindNext() override;
    bool FindFile(const CItem* item) override;
    DWORD GetAttributes() const override { return m_currentRecord->Attributes; }
    ULONGLONG GetIndex() const override { return m_currentRecordName->FileReference; }
    DWORD GetReparseTag() const override { return m_currentRecord->ReparsePointTag; }
    std::wstring GetFileName() const override { return m_currentRecordName->FileName; }
    ULONGLONG GetFileSizePhysical() const override { return m_currentRecord->PhysicalSize; }
    ULONGLONG GetFileSizeLogical() const override { return m_currentRecord->LogicalSize; }
    FILETIME GetLastWriteTime() const override { return m_currentRecord->LastModifiedTime; }
    std::wstring GetFilePath() const override;
    bool IsReserved() const override { return m_index < FinderNtfsContext::NtfsReservedMax; }
    bool HasIgnoredStream() const override { return m_currentRecord->HasIgnoredStream; }
    const ScanResult& GetResult() const noexcept override { return m_result; }
};
