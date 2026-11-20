// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "RecoveryShared.h"

class NtfsRecovery final : public RecoveryShared
{
public:
    explicit NtfsRecovery(const std::wstring& volumeName, Progress* progress = nullptr);
    void Scan(Progress& progress, ScanResult& result,
        const std::function<void(const Record&)>& discovered = {}) override;

private:
    static bool ParseRecord(std::span<const BYTE> bytes, ULONGLONG number, DWORD clusterSize,
        ULONGLONG clusterCount, Record& record, bool scanning = false);
    static std::vector<Run> DecodeRuns(std::span<const BYTE> bytes, ULONGLONG lowestVcn,
        ULONGLONG highestVcn, ULONGLONG clusterCount);
    static bool BitmapFree(std::span<const BYTE> bytes, ULONGLONG offset, ULONGLONG count);

    Handle m_mft{ CloseHandle };
    NTFS_VOLUME_DATA_BUFFER m_info = {};
    std::vector<Run> m_mftRuns;
    std::vector<BYTE> m_mftBuffer;

    std::vector<Run> GetMftRuns() const;

    std::span<const BYTE> ReadAt(ULONGLONG offset, DWORD length);
    std::span<const BYTE> ReadMft(ULONGLONG offset, DWORD length);
    bool ClustersFree(ULONGLONG lcn, ULONGLONG count, Progress& progress,
        std::vector<BYTE>* cached = nullptr) const;
    void ValidateRecord(const Record& record);

    std::wstring RecoverFile(const Record& record,
        const std::wstring& canonicalDestination, Progress& progress) override;
};
