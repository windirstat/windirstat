// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include <stddef.h>
#include <stdint.h>

enum WslRecordKind : uint8_t { WslKindEntry = 1, WslKindFailure = 2, WslKindEnd = 3 };
enum WslEntryType : uint8_t { WslTypeFile, WslTypeDirectory, WslTypeSymlink, WslTypeOther };
enum WslEntryFlags : uint8_t { WslFlagMount = 1 };

// Records use little-endian integers. An entry is followed by nameLength bytes without a terminator.
// The first entry describes the root with an empty name and parent zero. The end record counts all entries.
#pragma pack(push, 1)
struct WslStreamHeader
{
    uint8_t magic[8];
    uint32_t version, reserved;
};

struct WslEntryRecord
{
    uint8_t kind, type, flags, reserved;
    uint16_t nameLength, reserved2;
    uint32_t links;
    uint64_t inode, parent, size, allocated;
    int64_t seconds;
    uint32_t nanoseconds, reserved3;
};

struct WslShortRecord
{
    uint8_t kind, reserved[3];
    uint32_t error;
    uint64_t value; // Directory inode for a failure, entry count for the end record.
};
#pragma pack(pop)

static constexpr struct WslStreamHeader WslStreamHeaderValue = {
    .magic = { 'W', 'D', 'S', 'W', 'S', 'L', 1, 0 }, .version = 1, .reserved = 0
};
