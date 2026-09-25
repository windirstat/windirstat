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

class ExFatRecovery;

class NtfsRecovery final
{
public:
    struct Failure
    {
        std::wstring_view message = {};
        DWORD error = ERROR_SUCCESS;
    };

    enum class Condition { Resident, Unallocated };

    // Run positions are cluster indices; a negative physical index represents an NTFS sparse hole.
    struct Run
    {
        ULONGLONG vcn = 0;
        LONGLONG lcn = -1;
        ULONGLONG count = 0;
        bool operator==(const Run&) const = default;
    };

    struct Stream
    {
        ULONGLONG size = 0;
        ULONGLONG initialized = 0;
        std::vector<BYTE> resident;
        std::vector<Run> runs;
        bool nonresident = false;
        bool supported = true;
    };

    struct Record
    {
        ULONGLONG number = 0;
        ULONGLONG parent = 0;
        USHORT sequence = 0;
        bool inUse = false;
        bool directory = false;
        bool supported = true;
        std::wstring name;
        std::wstring path;
        FILETIME created = {};
        FILETIME modified = {};
        Stream data;
        std::vector<BYTE> snapshot;
        // exFAT entry sets may cross noncontiguous directory clusters.
        std::vector<ULONGLONG> entryOffsets;
        Condition condition = Condition::Resident;
    };

    struct Progress
    {
        std::atomic<bool> cancel = false;
        std::atomic<bool> paused = false;
        void Check() const;
    };

    struct ScanResult
    {
        std::vector<Record> records;
        ULONGLONG invalidRecords = 0;
    };

    explicit NtfsRecovery(const std::wstring& root, Progress* progress = nullptr);
    ~NtfsRecovery();

    // Validated records remain in the result when cancellation interrupts the scan.
    void Scan(Progress& progress, ScanResult& result, const std::function<void(const Record&)>& discovered = {});
    std::wstring Recover(const Record& record, const std::wstring& folder, Progress& progress);
    std::wstring ValidateDestination(const std::wstring& folder) const;
    const std::wstring& Name() const { return m_name; }

private:
    friend class ExFatRecovery;

    template <typename T>
    static T Read(const std::span<const BYTE> bytes, const size_t offset)
    {
        if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
            throw Failure{ {}, ERROR_INVALID_DATA };
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    static bool ParseRecord(std::span<const BYTE> bytes, ULONGLONG number, DWORD clusterSize,
        ULONGLONG clusterCount, Record& record, bool scanning = false);
    static std::vector<Run> DecodeRuns(std::span<const BYTE> bytes, ULONGLONG lowestVcn,
        ULONGLONG highestVcn, ULONGLONG clusterCount);
    static std::wstring SafeName(std::wstring_view name);

    class OutputFile final
    {
    public:
        OutputFile(const std::wstring& folder, const Record& record);
        ~OutputFile();
        void Write(std::span<const BYTE> bytes, Progress& progress) const;
        const std::wstring& Path() const { return m_path; }
        void Commit(const std::function<void()>& check, const FILETIME* created = nullptr,
            const FILETIME* modified = nullptr);

    private:
        std::wstring m_path;
        SmartPointer<HANDLE, decltype(&CloseHandle)> m_file{ CloseHandle };
        bool m_committed = false;
    };

    using Handle = SmartPointer<HANDLE, decltype(&CloseHandle)>;
    std::unique_ptr<ExFatRecovery> m_exfat;
    Handle m_volume{ CloseHandle };
    Handle m_mft{ CloseHandle };
    std::wstring m_name;
    NTFS_VOLUME_DATA_BUFFER m_info = {};
    std::vector<Run> m_mftRuns;
    mutable SmartPointer<void*, decltype(&_aligned_free)> m_buffer{ _aligned_free };
    mutable size_t m_bufferSize = 0;
    mutable std::vector<BYTE> m_mftBuffer;

    std::vector<Run> GetMftRuns() const;
    // Read views remain valid until the next raw read.
    std::span<const BYTE> ReadAt(ULONGLONG offset, DWORD length) const;
    std::span<const BYTE> ReadMft(ULONGLONG offset, DWORD length) const;
    bool ClustersFree(ULONGLONG lcn, ULONGLONG count, Progress& progress,
        std::vector<BYTE>* cached = nullptr) const;
    void ValidateRecord(const Record& record) const;

    static std::span<const BYTE> Slice(std::span<const BYTE> bytes, size_t offset, size_t size);
    static std::wstring ReadName(std::span<const BYTE> bytes, size_t offset, size_t length);
    static bool BitmapFree(std::span<const BYTE> bytes, ULONGLONG offset, ULONGLONG count);
};
