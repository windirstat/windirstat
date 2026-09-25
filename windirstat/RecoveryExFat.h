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

#include "RecoveryNtfs.h"

class ExFatRecovery final
{
public:
    explicit ExFatRecovery(const std::wstring& volumeName, NtfsRecovery::Progress* progress = nullptr);
    void Scan(NtfsRecovery::Progress& progress, NtfsRecovery::ScanResult& result,
        const std::function<void(const NtfsRecovery::Record&)>& discovered = {});
    std::wstring Recover(const NtfsRecovery::Record& record, const std::wstring& canonicalDestination,
        NtfsRecovery::Progress& progress);

private:
    using Condition = NtfsRecovery::Condition;
    using Failure = NtfsRecovery::Failure;
    using Progress = NtfsRecovery::Progress;
    using Record = NtfsRecovery::Record;

    static constexpr DWORD ReadSize = 1024 * 1024;
    static constexpr size_t MaxEntrySetSize = 256 * 32;

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
    static bool ParseEntrySet(std::span<const BYTE> bytes, const Geometry& geometry, NtfsRecovery::Record& record);

    static FILETIME Timestamp(DWORD packed, BYTE increment, BYTE offset);

    SmartPointer<HANDLE, decltype(&CloseHandle)> m_volume{ CloseHandle };
    SmartPointer<void*, decltype(&_aligned_free)> m_rawBuffer{ _aligned_free };
    size_t m_rawBufferSize = 0;
    ULONGLONG m_length = 0;
    DWORD m_alignment = 4096;
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

    void ReadRaw(ULONGLONG offset, std::span<BYTE> bytes);
    void ReadAt(ULONGLONG offset, std::span<BYTE> bytes);
    void Initialize(NtfsRecovery::Progress* progress);
    DWORD NextCluster(DWORD cluster);
    ULONGLONG ClusterOffset(DWORD cluster) const;
    std::vector<DWORD> Chain(DWORD first, ULONGLONG length, bool contiguous,
        NtfsRecovery::Progress& progress);
    void VisitEntries(const std::vector<DWORD>& clusters, NtfsRecovery::Progress& progress,
        const std::function<bool(std::span<const BYTE>, ULONGLONG)>& visit);
    void ReadBitmap(ULONGLONG offset, std::span<BYTE> bytes);
    bool ClustersHaveState(DWORD first, ULONGLONG count, bool allocated,
        NtfsRecovery::Progress& progress, bool fresh = false);
    void Validate(const NtfsRecovery::Record& record, NtfsRecovery::Progress& progress);
};
