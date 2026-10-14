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

    // A WebDAV scan target always names a collection.
    void EndCollection()
    {
        if (protocol == RemoteProtocol::WebDav && !prefix.empty() && !prefix.ends_with(L'/')) prefix += L'/';
    }
};

struct RemoteCredentials
{
    std::wstring user, secret, token;
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
    std::atomic<unsigned> failures = 0; // consecutive listings lost to an unavailable connection
    ~StorageSource();

    static RemoteCredentials GetCredentials(const RemoteTarget& target);
    static DWORD SetCredentials(const RemoteTarget& target, const RemoteCredentials& credentials, bool remember);
    static void ForgetCredentials(const RemoteTarget& target, std::span<const std::wstring> retained);
    static std::shared_ptr<StorageSource> Open(std::wstring_view path, bool renew = false);
    static bool IsPath(std::wstring_view path) noexcept
    {
        return path.starts_with(L"s3://") || path.starts_with(L"azure://") ||
            path.starts_with(L"https://") || path.starts_with(L"http://");
    }

    // Remote paths carry connection options after '?'; path logic operates on the address before it.
    static std::pair<std::wstring_view, std::wstring_view> SplitOptions(const std::wstring_view path) noexcept
    {
        const auto query = (std::min)(path.find(L'?'), path.size());
        return { path.substr(0, query), path.substr(query) };
    }
    static std::optional<RemoteTarget> Parse(std::wstring_view path);
    static std::optional<RemoteTarget> Parse(const RemoteTarget& target);
    static std::wstring EscapeName(std::wstring_view value, bool keepSlash = false);
    static std::wstring DisplayName(std::wstring_view value);
    static std::wstring ItemPath(const CItem* item);
};
