// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

enum class RemoteProtocol { S3, Azure, WebDav };

struct RemoteTarget
{
    RemoteProtocol protocol = RemoteProtocol::S3;
    std::wstring bucket, prefix, region, endpoint, account;
    std::wstring ToString() const;
    bool SameConnection(const RemoteTarget& other) const;
};

struct RemoteCredentials
{
    std::wstring user, secret, token;
};

struct StorageIdentity
{
    std::wstring source, key;
    bool directory = false;
    bool operator==(const StorageIdentity&) const = default;
};

struct StorageIdentityHash
{
    size_t operator()(const StorageIdentity& identity) const noexcept
    {
        return std::hash<std::wstring>{}(identity.source) ^ (std::hash<std::wstring>{}(identity.key) << 1) ^
            static_cast<size_t>(identity.directory);
    }
};

struct StorageCapabilities
{
    bool flatListing = false;
    bool batchDelete = false;
};

class RemoteTransport;

// One connection and credential scope belongs to each remote enumeration root.
struct StorageSource
{
    RemoteTarget target;
    RemoteCredentials credentials;
    StorageCapabilities capabilities;
    std::shared_ptr<RemoteTransport> transport;
    ~StorageSource();

    static RemoteCredentials GetCredentials(const RemoteTarget& target);
    static DWORD SetCredentials(const RemoteTarget& target, const RemoteCredentials& credentials, bool remember);
    static std::shared_ptr<StorageSource> Open(std::wstring_view path);
    static bool IsPath(std::wstring_view path) noexcept
    {
        return path.starts_with(L"s3://") || path.starts_with(L"azure://") ||
            path.starts_with(L"https://") || path.starts_with(L"http://");
    }
    static std::optional<RemoteTarget> Parse(std::wstring_view path);
    static std::optional<RemoteTarget> Parse(const RemoteTarget& target);
    static std::wstring EscapeName(std::wstring_view value, bool keepSlash = false);
    static std::wstring DisplayName(std::wstring_view value);
    static std::wstring ItemPath(const CItem* item);
    static std::optional<StorageIdentity> Identity(const CItem* item);
    static std::optional<StorageIdentity> Identity(std::wstring_view path, bool directory);
};
