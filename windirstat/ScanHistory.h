// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

struct HistoryPoint
{
    std::int64_t time = 0;      // seconds since 1970-01-01 UTC
    ULONGLONG size = 0;         // logical size in bytes
    ULONGLONG allocated = 0;    // physical (allocated) size in bytes
    ULONGLONG files = 0;        // number of files
};

// Size history of the folders of a scan. Every scan origin (the path specification the
// scan was started with, e.g. "C:\" or "D:\Data") has its own JSON file, which is read
// the first time the origin is needed and extended after every completed scan.
class CScanHistory final
{
public:
    // Appends a snapshot of the finished scan to the origin's file.
    static void RecordScan(const CItem* root, const std::wstring& origin);

    // Returns the stored points of a folder (oldest first) or nullptr if there are none.
    static const std::vector<HistoryPoint>* Find(const std::wstring& origin, const std::wstring& path);

    // Folders stored by the newest scan that also have an earlier point, with both points.
    struct FolderChange
    {
        std::wstring path;
        HistoryPoint previous;
        HistoryPoint current;
    };
    // baseline = 0 compares with the scan before the newest; otherwise with the scan stored at that time.
    static std::vector<FolderChange> GetChanges(const std::wstring& origin, std::int64_t baseline = 0);

    // Times of all stored scans of an origin, oldest first.
    static std::vector<std::int64_t> GetScanTimes(const std::wstring& origin);

    static std::wstring GetFilePath(const std::wstring& origin);

    // Removes the point stored at the given time for one folder.
    static bool RemovePoint(const std::wstring& origin, const std::wstring& path, std::int64_t time);

    // Deletes all history stored for an origin.
    static void Purge(const std::wstring& origin);

    // Moves the history files to another folder (empty = default location).
    static void SetStorageFolder(const std::wstring& folder);
};
