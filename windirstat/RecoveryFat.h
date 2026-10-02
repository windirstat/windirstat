// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "RecoveryShared.h"

class FatRecovery final : public RecoveryShared
{
public:
    explicit FatRecovery(const std::wstring& volumeName, Progress* progress = nullptr);
    void Scan(Progress& progress, ScanResult& result,
        const std::function<void(const Record&)>& discovered = {}) override;

private:
    static constexpr size_t BootSectorBytes = 512;

    // Offsets and sizes are bytes; FAT cluster numbers begin at two.
    struct Geometry
    {
        DWORD bits = 0;
        DWORD sectorSize = 0;
        DWORD clusterSize = 0;
        DWORD clusterCount = 0;
        DWORD fatCount = 0;
        DWORD activeFat = 0;
        bool mirrored = true;
        DWORD rootEntries = 0;
        DWORD rootCluster = 0;
        ULONGLONG volumeSize = 0;
        ULONGLONG fatOffset = 0;
        ULONGLONG fatSize = 0;
        ULONGLONG rootOffset = 0;
        ULONGLONG dataOffset = 0;
        bool operator==(const Geometry&) const = default;
    };

    struct FatPage
    {
        ULONGLONG offset = ULLONG_MAX;
        std::vector<BYTE> bytes;
    };

    static bool ParseBoot(std::span<const BYTE> bytes, ULONGLONG deviceLength, Geometry& geometry);
    static bool ParseEntry(std::span<const BYTE> bytes, const Geometry& geometry, Record& record);
    static std::wstring LongName(std::span<const BYTE> bytes);

    Geometry m_geometry;
    ULONGLONG m_length = 0;
    std::array<BYTE, BootSectorBytes> m_boot{};
    std::vector<FatPage> m_fatPages;
    std::unordered_map<DWORD, std::vector<DWORD>> m_directories;

    void ReadAt(ULONGLONG offset, std::span<BYTE> bytes);
    ULONGLONG ClusterOffset(DWORD cluster) const;
    DWORD FatValue(DWORD cluster);
    std::vector<DWORD> Chain(DWORD first, Progress& progress);
    void VisitEntries(DWORD first, const std::vector<DWORD>& clusters, Progress& progress,
        const std::function<bool(std::span<const BYTE>, ULONGLONG)>& visit);
    bool ClustersFree(DWORD first, ULONGLONG count, Progress& progress, bool fresh = false);
    void Validate(const Record& record, Progress& progress);
    std::wstring RecoverFile(const Record& record,
        const std::wstring& canonicalDestination, Progress& progress) override;
};
