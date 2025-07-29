// FinderNtfs.cpp - Declaration of FinderNtfs
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

#include <stdafx.h>
#include "FinderNtfs.h"
#include "SmartPointer.h"

#include <vector>
#include <execution>
#include <set>

enum ATTRIBUTE_TYPE_CODE : ULONG
{
    AttributeStandardInformation = 0x10,
    AttributeFileName = 0x30,
    AttributeData = 0x80,
    AttributeReparsePoint = 0xC0,
    AttributeEnd = 0xFFFFFFFF,
};

using FILE_RECORD = struct FILE_RECORD
{
    ULONG Signature;
    USHORT UsaOffset;
    USHORT UsaCount;
    ULONGLONG Lsn;
    USHORT SequenceNumber;
    USHORT LinkCount;
    USHORT FirstAttributeOffset;
    USHORT Flags;
    ULONG FirstFreeByte;
    ULONG BytesAvailable;
    ULONGLONG BaseFileRecordNumber : 48;
    ULONGLONG BaseFileRecordSequence : 16;
    USHORT NextAttributeNumber;
    USHORT SegmentNumberHighPart;
    ULONG SegmentNumberLowPart;

    constexpr ULONGLONG SegmentNumber() const
    {
        return static_cast<ULONGLONG>(SegmentNumberHighPart) << 32ul | SegmentNumberLowPart;
    }

    constexpr bool IsValid() const
    {
        return Signature == 0x454C4946; // 'FILE'
    } 
    constexpr bool IsInUse() const
    {
        return Flags & 0x0001;
    }

    constexpr bool IsDirectory() const
    {
        return Flags & 0x0002;
    }
};

using ATTRIBUTE_RECORD = struct ATTRIBUTE_RECORD
{
    ATTRIBUTE_TYPE_CODE TypeCode;
    ULONG RecordLength;
    UCHAR FormCode;
    UCHAR NameLength;
    USHORT NameOffset;
    USHORT Flags;
    USHORT Instance;

    union
    {
        struct
        {
            ULONG ValueLength;
            USHORT ValueOffset;
            UCHAR Reserved[2];
        } Resident;

        struct
        {
            LONGLONG LowestVcn;
            LONGLONG HighestVcn;
            USHORT MappingPairsOffset;
            USHORT CompressionSize;
            UCHAR Padding[4];
            LONGLONG AllocatedLength;
            LONGLONG FileSize;
            LONGLONG ValidDataLength;
            LONGLONG Compressed;
        } Nonresident;
    } Form;

    constexpr bool IsNonResident() const
    {
        return FormCode & 0x0001;
    }

    constexpr bool IsCompressed() const
    {
        return Flags & 0x0001;
    }

    ATTRIBUTE_RECORD* next() const
    {
        return ByteOffset<ATTRIBUTE_RECORD>(const_cast<ATTRIBUTE_RECORD*>(this), RecordLength);
    }

    static constexpr std::pair<ATTRIBUTE_RECORD*, ATTRIBUTE_RECORD*> bounds(FILE_RECORD* FileRecord, auto TotalLength)
    {
        return {
            ByteOffset<ATTRIBUTE_RECORD>(FileRecord, FileRecord->FirstAttributeOffset),
            ByteOffset<ATTRIBUTE_RECORD>(FileRecord, FileRecord->FirstAttributeOffset + TotalLength)
        };
    }
};

using FILE_NAME = struct FILE_NAME
{
    ULONGLONG ParentDirectory : 48;
    ULONGLONG ParentSequence : 16;
    LONGLONG CreationTime;
    LONGLONG LastModificationTime;
    LONGLONG MftChangeTime;
    LONGLONG LastAccessTime;
    LONGLONG AllocatedLength;
    LONGLONG FileSize;
    ULONG FileAttributes;
    USHORT PackedEaSize;
    USHORT Reserved;
    UCHAR FileNameLength;
    UCHAR Flags;
    WCHAR FileName[1];

    constexpr bool IsShortNameRecord() const
    {
        return Flags == 0x02;
    }
};

using STANDARD_INFORMATION = struct STANDARD_INFORMATION
{
    FILETIME CreationTime;
    FILETIME LastModificationTime;
    FILETIME MftChangeTime;
    FILETIME LastAccessTime;
    ULONG FileAttributes;
};

constexpr auto NtfsNodeRoot = 5;
constexpr auto NtfsReservedMax = 16;

static constexpr auto& getMapBinRef(auto* mapArray, std::mutex* mutexArray, auto key, auto binSize)
{
    const auto binIndex = key / binSize;
    std::lock_guard<std::mutex> lock(mutexArray[binIndex]);
    return mapArray[binIndex][key];
}

bool FinderNtfsContext::LoadRoot(CItem* driveitem)
{
    // Trim off excess characters
    std::wstring volumePath = driveitem->GetPathLong();
    if (volumePath.back() == L'\\') volumePath.pop_back();
    while (!volumePath.empty() && volumePath.back() == L'\\') volumePath.pop_back();
    if (!volumePath.empty() && volumePath[0] != L'\\' && volumePath[0] != L'/') volumePath.insert(0, L"\\\\.\\");

    // Open volume handle without FILE_FLAG_OVERLAPPED for synchronous I/O
    SmartPointer<HANDLE> volumeHandle(CloseHandle,CreateFile(volumePath.c_str(), FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
    if (volumeHandle == INVALID_HANDLE_VALUE) return false;

    // Get volume information
    NTFS_VOLUME_DATA_BUFFER volumeInfo = {};
    ULONG bytesReturned;
    if (!DeviceIoControl(volumeHandle, FSCTL_GET_NTFS_VOLUME_DATA, nullptr, 0, &volumeInfo, sizeof(volumeInfo), &bytesReturned, nullptr)) return
        false;

    // Get MFT retrieval pointers
    SmartPointer<HANDLE> fileHandle(CloseHandle, CreateFile((volumePath + L"\\$MFT::$DATA").c_str(), FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED, nullptr));
    if (fileHandle == INVALID_HANDLE_VALUE) return {};

    std::vector<BYTE> dataRunsBuffer(sizeof(RETRIEVAL_POINTERS_BUFFER) + 32 * sizeof(LARGE_INTEGER));
    STARTING_VCN_INPUT_BUFFER input = {};
    while (!DeviceIoControl(fileHandle, FSCTL_GET_RETRIEVAL_POINTERS, &input, sizeof(input), dataRunsBuffer.data(),
        static_cast<DWORD>(dataRunsBuffer.size()), &bytesReturned, nullptr) && GetLastError() == ERROR_MORE_DATA)
    {
        dataRunsBuffer.resize(dataRunsBuffer.size() * 2);
    }
    if (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_MORE_DATA)
    {
        return false;
    }

    // Extract data run origins and cluster counts
    RETRIEVAL_POINTERS_BUFFER* retrievalBuffer = ByteOffset<RETRIEVAL_POINTERS_BUFFER>(dataRunsBuffer.data(), 0);
    std::vector<std::pair<ULONGLONG, ULONGLONG>> dataRuns(retrievalBuffer->ExtentCount, {});
    for (DWORD i = 0; i < retrievalBuffer->ExtentCount; i++)
    {
        dataRuns.emplace_back(retrievalBuffer->Extents[i].Lcn.QuadPart,
            retrievalBuffer->Extents[i].NextVcn.QuadPart - (i == 0
                ? retrievalBuffer->StartingVcn.QuadPart : retrievalBuffer->Extents[i - 1].NextVcn.QuadPart));
    }

    // This is a binning approach to reduce contention on the maps
    constexpr auto numBins = 256;
    const auto numRecords = volumeInfo.MftValidDataLength.QuadPart / volumeInfo.BytesPerFileRecordSegment;
    const auto binSize = max(1, numRecords / numBins);
    std::unordered_map<ULONGLONG, FileRecordBase> baseFileRecordMapTemp[numBins];
    std::unordered_map<ULONGLONG, ULONGLONG> nonBaseToBaseMapTemp[numBins];
    std::unordered_map<ULONGLONG, std::set<FileRecordName>> parentToChildMapTemp[numBins];
    std::mutex baseFileRecordMapMutex[numBins];
    std::mutex nonBaseToBaseMapMutex[numBins];
    std::mutex parentToChildMapMutex[numBins];

    // Process MFT records
    std::for_each(std::execution::par_unseq, dataRuns.begin(), dataRuns.end(), [&](const auto& dataRun)
    {
        constexpr size_t bufferSize = 4ull * 1024 * 1024;
        std::vector<UCHAR> buffer;
        buffer.reserve(bufferSize);
        const auto& [clusterStart, clusterCount] = dataRun;

        // Enumerate over the data run in buffer-sized chunks
        ULONGLONG bytesToRead = clusterCount * volumeInfo.BytesPerCluster;
        LARGE_INTEGER fileOffset{ .QuadPart = static_cast<LONGLONG>(clusterStart * volumeInfo.BytesPerCluster) };
        for (ULONG bytesRead = 0; bytesToRead > 0; bytesToRead -= bytesRead, fileOffset.QuadPart += bytesRead)
        {
            // Animate pacman
            driveitem->UpwardDrivePacman();

            // Set file pointer for synchronous read
            const ULONG bytesThisRead = static_cast<ULONG>(min(bytesToRead, bufferSize));
            OVERLAPPED overlapped = { .Offset = fileOffset.LowPart, .OffsetHigh = static_cast<DWORD>(fileOffset.HighPart) };
            if (ReadFile(volumeHandle, buffer.data(), bytesThisRead, &bytesRead, &overlapped) == 0 ||
                GetOverlappedResult(volumeHandle, &overlapped, &bytesRead, INFINITE) == 0)
            {
                break;
            }

            for (ULONG offset = 0; offset + volumeInfo.BytesPerFileRecordSegment <= bytesRead; offset += volumeInfo.BytesPerFileRecordSegment)
            {
                // Process MFT record inline
                const auto fileRecord = ByteOffset<FILE_RECORD>(buffer.data(), offset);

                // Apply fixup
                const auto wordsPerSector = volumeInfo.BytesPerSector / sizeof(USHORT);
                const auto fixupArray = ByteOffset<USHORT>(fileRecord, fileRecord->UsaOffset);
                const auto usn = fixupArray[0];
                const auto recordWords = reinterpret_cast<PUSHORT>(ByteOffset<UCHAR>(buffer.data(), offset));
                for (ULONG i = 1; i < fileRecord->UsaCount; ++i)
                {
                    const auto sectorEnd = recordWords + i * wordsPerSector - 1;
                    if (*sectorEnd == usn) *sectorEnd = fixupArray[i];
                }

                // Only process records with valid headers and are in use
                if (!fileRecord->IsValid() || !fileRecord->IsInUse()) continue;
                const auto currentRecord = fileRecord->SegmentNumber();
                auto baseRecordIndex = fileRecord->BaseFileRecordNumber > 0 ? fileRecord->BaseFileRecordNumber : currentRecord;
                getMapBinRef(nonBaseToBaseMapTemp, nonBaseToBaseMapMutex, currentRecord, binSize) = baseRecordIndex;

                for (auto [curAttribute, endAttribute] = ATTRIBUTE_RECORD::bounds(fileRecord, volumeInfo.BytesPerFileRecordSegment); curAttribute <
                    endAttribute && curAttribute->TypeCode != AttributeEnd; curAttribute = curAttribute->next())
                {
                    if (curAttribute->TypeCode == AttributeStandardInformation)
                    {
                        if (curAttribute->IsNonResident()) continue;
                        const auto si = ByteOffset<STANDARD_INFORMATION>(curAttribute, curAttribute->Form.Resident.ValueOffset);
                        auto& baseRecord = getMapBinRef(baseFileRecordMapTemp, baseFileRecordMapMutex, baseRecordIndex, binSize);
                        baseRecord.LastModifiedTime = si->LastModificationTime;
                        baseRecord.Attributes = si->FileAttributes;
                        if (fileRecord->IsDirectory()) baseRecord.Attributes |= FILE_ATTRIBUTE_DIRECTORY;
                    }
                    else if (curAttribute->TypeCode == AttributeFileName)
                    {
                        if (curAttribute->IsNonResident()) continue;
                        const auto fn = ByteOffset<FILE_NAME>(curAttribute, curAttribute->Form.Resident.ValueOffset);
                        if (fn->IsShortNameRecord()) continue;
                        auto& parentToChildEntry = getMapBinRef(parentToChildMapTemp, parentToChildMapMutex, fn->ParentDirectory, binSize);
                        parentToChildEntry.emplace(std::wstring{ fn->FileName, fn->FileNameLength }, baseRecordIndex);
                    }
                    else if (curAttribute->TypeCode == AttributeData)
                    {
                        if (curAttribute->NameLength != 0) continue; // only process default data stream
                        auto& baseRecord = getMapBinRef(baseFileRecordMapTemp, baseFileRecordMapMutex, baseRecordIndex, binSize);
                        if (curAttribute->IsNonResident())
                        {
                            if (curAttribute->Form.Nonresident.LowestVcn != 0) continue;
                            baseRecord.LogicalSize = curAttribute->Form.Nonresident.FileSize;
                            baseRecord.PhysicalSize = curAttribute->IsCompressed() ?
                                curAttribute->Form.Nonresident.Compressed : curAttribute->Form.Nonresident.AllocatedLength;
                        }
                        else
                        {
                            baseRecord.LogicalSize = curAttribute->Form.Resident.ValueLength;
                            baseRecord.PhysicalSize = curAttribute->Form.Resident.ValueLength;
                        }
                    }
                    else if (curAttribute->TypeCode == AttributeReparsePoint)
                    {
                        if (curAttribute->IsNonResident()) continue;
                        const auto fn = ByteOffset<Finder::REPARSE_DATA_BUFFER>(curAttribute, curAttribute->Form.Resident.ValueOffset);
                        auto& baseRecord = getMapBinRef(baseFileRecordMapTemp, baseFileRecordMapMutex, baseRecordIndex, binSize);
                        baseRecord.ReparsePointTag = fn->ReparseTag;
                        if (!Finder::IsMountPoint(*fn))
                        {
                            baseRecord.ReparsePointTag = ~IO_REPARSE_TAG_MOUNT_POINT;
                        }
                    }
                }
            }
        }
    });

    {
        // Merge temporary maps into the main maps using jthread for parallel execution
        std::jthread t1([&]() { for (auto& map : baseFileRecordMapTemp) { m_BaseFileRecordMap.merge(map); }});
        std::jthread t2([&]() { for (auto& map : nonBaseToBaseMapTemp) { m_NonBaseToBaseMap.merge(map); }});
        std::jthread t3([&]() { for (auto& map : parentToChildMapTemp) { m_ParentToChildMap.merge(map); }});
    }

    if (!m_ParentToChildMap.contains(NtfsNodeRoot))
    {
        return false;
    }

    // Remove bad cluster node
    std::erase_if(m_ParentToChildMap[NtfsNodeRoot], [](const auto& child) {
        return child.BaseRecord < NtfsReservedMax && child.BaseRecord != NtfsNodeRoot;
    });

    driveitem->SetIndex(NtfsNodeRoot);
    return true;
}


bool FinderNtfs::FindNext()
{
    if (m_RecordIterator == m_ChildrenSet->end()) return false;;
    m_Index = m_RecordIterator->BaseRecord;
    m_CurrentRecord = &m_Master->m_BaseFileRecordMap[m_Index];
    m_CurrentRecordName = &(*m_RecordIterator);
    ++m_RecordIterator;

    return true;
}

bool FinderNtfs::FindFile(const CItem* item)
{
    m_Base = item->GetPath();
    const auto& result = m_Master->m_ParentToChildMap.find(item->GetIndex());
    if (result == m_Master->m_ParentToChildMap.end()) return false;
    m_ChildrenSet = &result->second;
    m_RecordIterator = m_ChildrenSet->begin();
    return FindNext();
}

DWORD FinderNtfs::GetAttributes() const
{
    return m_CurrentRecord->Attributes;
}

ULONG FinderNtfs::GetIndex() const
{
    return static_cast<ULONG>(m_CurrentRecordName->BaseRecord);
}

DWORD FinderNtfs::GetReparseTag() const
{
    return m_CurrentRecord->ReparsePointTag;
}

std::wstring FinderNtfs::GetFileName() const
{
    return m_CurrentRecordName->FileName;
}

ULONGLONG FinderNtfs::GetFileSizePhysical() const
{
    return m_CurrentRecord->PhysicalSize;
}

ULONGLONG FinderNtfs::GetFileSizeLogical() const
{
    return m_CurrentRecord->LogicalSize;
}

FILETIME FinderNtfs::GetLastWriteTime() const
{
    return m_CurrentRecord->LastModifiedTime;
}

std::wstring FinderNtfs::GetFilePath() const
{
    // Get full path to folder or file
    std::wstring path = (m_Base.at(m_Base.size() - 1) == L'\\') ?
        (m_Base + GetFileName()) : (m_Base + L"\\" + GetFileName());

    // Strip special dos chars
    if (wcsncmp(path.data(), m_DosUNC.data(), m_DosUNC.length() - 1) == 0)
        path = L"\\\\" + path.substr(m_DosUNC.length());
    else if (wcsncmp(path.data(), m_Dos.data(), m_Dos.length() - 1) == 0)
        path = path.substr(m_Dos.length());
    return path;
}

bool FinderNtfs::IsDots() const
{
    return m_CurrentRecordName->FileName == L"." || m_CurrentRecordName->FileName == L"..";
}
