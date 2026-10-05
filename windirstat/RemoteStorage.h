// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "StorageSource.h"
#include <winhttp.h>

struct RemoteFailure { std::wstring message; bool retry = false; bool missing = false; };

// Encoding and transport primitives shared by the source boundary and its remote enumerator.
void RemoteCheck(bool success, DWORD error = ERROR_INVALID_DATA);
std::string RemoteUtf8(std::wstring_view value);
std::wstring RemoteWide(std::string_view value);
std::string RemoteEncode(std::wstring_view value);
std::wstring RemoteDecode(std::wstring_view value);
URL_COMPONENTS RemoteEndpoint(const std::wstring& endpoint, bool allowPath = false);
std::shared_ptr<RemoteTransport> OpenRemoteTransport();
