// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "RecoveryShared.h"

class ExFatRecovery final : public RecoveryShared
{
public:
    explicit ExFatRecovery(const std::wstring& volumeName, Progress* progress = nullptr);
    void Scan(Progress& progress, ScanResult& result,
        const std::function<void(const Record&)>& discovered = {}) override;

private:
    std::wstring RecoverFile(const Record& record,
        const std::wstring& canonicalDestination, Progress& progress) override;

    // One primary entry, one stream entry and at least one filename entry; up to 255 secondary entries.
    static constexpr size_t EntryBytes = 32;
    static constexpr size_t MinEntrySetSize = 3 * EntryBytes;
    static constexpr size_t MaxEntrySetSize = (1 + size_t(UCHAR_MAX)) * EntryBytes;

    // Sizes and offsets are in bytes; cluster indices retain exFAT's numbering from two.
    struct Geometry
    {
        DWORD sectorSize = 0;
        DWORD clusterSize = 0;
        DWORD clusterCount = 0;
        DWORD rootCluster = 0;
        ULONGLONG volumeSize = 0;
        ULONGLONG fatOffset = 0;
        ULONGLONG heapOffset = 0;
        bool operator==(const Geometry&) const = default;
    };

    static bool ParseBootRegion(std::span<const BYTE> bytes, ULONGLONG deviceLength, Geometry& geometry);
    static bool ParseEntrySet(std::span<const BYTE> bytes, const Geometry& geometry, Record& record);

    static FILETIME Timestamp(DWORD packed, BYTE increment, BYTE offset);

    ULONGLONG m_length = 0;
    DWORD m_alignment = InitialSectorAlignment;
    Geometry m_geometry;
    std::vector<BYTE> m_boot;
    std::vector<DWORD> m_rootClusters;
    std::vector<DWORD> m_bitmapClusters;
    std::vector<BYTE> m_bitmapEntry;
    ULONGLONG m_bitmapEntryOffset = 0;
    ULONGLONG m_bitmapSize = 0;
    std::vector<BYTE> m_fatPage;
    ULONGLONG m_fatPageOffset = ULLONG_MAX;
    std::vector<BYTE> m_bitmapPage;
    ULONGLONG m_bitmapPageOffset = ULLONG_MAX;

    void ReadAt(ULONGLONG offset, std::span<BYTE> bytes);
    void Initialize(Progress* progress);
    DWORD NextCluster(DWORD cluster);
    ULONGLONG ClusterOffset(DWORD cluster) const;
    std::vector<DWORD> Chain(DWORD first, ULONGLONG length, bool contiguous, Progress& progress);
    void VisitEntries(const std::vector<DWORD>& clusters, Progress& progress,
        const std::function<bool(std::span<const BYTE>, ULONGLONG)>& visit);
    void ReadBitmap(ULONGLONG offset, std::span<BYTE> bytes);
    bool ClustersHaveState(DWORD first, ULONGLONG count, bool allocated, Progress& progress, bool fresh = false);
    void Validate(const Record& record, Progress& progress);
};
