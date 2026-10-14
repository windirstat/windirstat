// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "StorageSource.h"
#include "RemoteStorage.h"
#include <wincred.h>

void RemoteCheck(const bool success, const DWORD error)
{
    if (!success) throw RemoteFailure{ TranslateError(error),
        error == ERROR_WINHTTP_TIMEOUT || error == ERROR_WINHTTP_CONNECTION_ERROR };
}

std::string RemoteUtf8(const std::wstring_view value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    RemoteCheck(size > 0);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring RemoteWide(const std::string_view value)
{
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    RemoteCheck(size > 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string RemoteEncode(const std::wstring_view value)
{
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const unsigned char c : RemoteUtf8(value))
    {
        if (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' ||
            c == '-' || c == '_' || c == '.' || c == '~') result += c;
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}

std::wstring RemoteDecode(const std::wstring_view value)
{
    const auto hex = [](const char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string result;
    const std::string bytes = RemoteUtf8(value);
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (bytes[i] != '%') { result += bytes[i]; continue; }
        RemoteCheck(i + 2 < bytes.size() && hex(bytes[i + 1]) >= 0 && hex(bytes[i + 2]) >= 0);
        result += static_cast<char>((hex(bytes[i + 1]) << 4) | hex(bytes[i + 2]));
        i += 2;
    }
    return RemoteWide(result);
}

URL_COMPONENTS RemoteEndpoint(const std::wstring& endpoint, const bool allowPath)
{
    URL_COMPONENTS url{ .dwStructSize = sizeof(URL_COMPONENTS) };
    url.dwHostNameLength = url.dwUrlPathLength = url.dwExtraInfoLength =
        url.dwUserNameLength = url.dwPasswordLength = static_cast<DWORD>(-1);
    RemoteCheck(WinHttpCrackUrl(endpoint.c_str(), 0, 0, &url) && url.dwHostNameLength != 0 &&
        (url.nScheme == INTERNET_SCHEME_HTTPS || url.nScheme == INTERNET_SCHEME_HTTP) &&
        url.dwUserNameLength == 0 && url.dwPasswordLength == 0 && url.dwExtraInfoLength == 0 &&
        (allowPath || url.dwUrlPathLength == 0 || std::wstring_view(url.lpszUrlPath, url.dwUrlPathLength) == L"/"));
    return url;
}

std::wstring StorageSource::EscapeName(const std::wstring_view value, const bool keepSlash)
{
    // Keep Unicode readable while escaping characters with path, report, or display semantics.
    constexpr wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring result;
    for (const wchar_t c : value)
    {
        if (c <= 32 || c == 127 || std::wstring_view(L"%\\|?&#\"").contains(c) || c == '/' && !keepSlash)
        {
            result += L'%'; result += hex[(c >> 4) & 15]; result += hex[c & 15];
        }
        else result += c;
    }
    return result;
}

std::wstring StorageSource::DisplayName(const std::wstring_view value)
{
    try
    {
        std::wstring result = RemoteDecode(value);
        for (auto& c : result) if (c < 32 || c == 127) c = L'\uFFFD';
        return result;
    }
    catch (const RemoteFailure&) { return std::wstring(value); }
}

std::wstring RemoteTarget::ToString() const
{
    if (protocol == RemoteProtocol::WebDav) return endpoint + L"/" + StorageSource::EscapeName(prefix, true);
    std::wstring result = (protocol == RemoteProtocol::Azure ? L"azure://" + account + L"/" : L"s3://") +
        bucket + L"/" + StorageSource::EscapeName(prefix, true);
    bool first = true;
    for (const auto& [name, value] : { std::pair{L"region", region}, {L"endpoint", endpoint} })
    {
        if (value.empty()) continue;
        result += first ? L'?' : L'&';
        first = false;
        result += name; result += L'='; result += RemoteWide(RemoteEncode(value));
    }
    return result;
}

bool RemoteTarget::SameConnection(const RemoteTarget& other) const
{
    return protocol == other.protocol && bucket == other.bucket && account == other.account &&
        region == other.region && endpoint == other.endpoint;
}

std::optional<RemoteTarget> StorageSource::Parse(const std::wstring_view path)
{
    if (!IsPath(path)) return {};
    try
    {
        RemoteTarget target;
        if (path.starts_with(L"http://") || path.starts_with(L"https://"))
        {
            // Keep server names canonical while preserving case and escaped characters in resource paths.
            const std::wstring address(path);
            const auto url = RemoteEndpoint(address, true);
            const auto offset = url.dwUrlPathLength ?
                static_cast<size_t>(url.lpszUrlPath - address.data()) : path.size();
            target.protocol = RemoteProtocol::WebDav;
            target.endpoint = url.nScheme == INTERNET_SCHEME_HTTPS ? L"https://" : L"http://";
            target.endpoint += MakeLower(std::wstring(url.lpszHostName, url.dwHostNameLength));
            if (url.nPort != (url.nScheme == INTERNET_SCHEME_HTTPS ? 443 : 80))
                target.endpoint += L":" + std::to_wstring(url.nPort);
            target.prefix = url.dwUrlPathLength ? RemoteDecode(std::wstring_view(url.lpszUrlPath + 1,
                url.dwUrlPathLength - 1)) : std::wstring{};
            if (std::ranges::any_of(target.prefix,
            [](wchar_t c) { return c < 32 || c == 127 || c == L'\\'; })) return {};
            for (const auto part : std::views::split(target.prefix, L'/'))
            {
                const std::wstring_view component(part.begin(), part.end());
                if (component == L"." || component == L"..") return {};
            }

            // An encoded slash is not a directory boundary in a WebDAV resource name.
            if (MakeLower(address.substr(offset)).contains(L"%2f")) return {};
            return target;
        }
        const bool azure = path.starts_with(L"azure://");
        const size_t scheme = azure ? 8 : 5;
        if (azure) target.protocol = RemoteProtocol::Azure;
        const auto [location, options] = SplitOptions(path);
        auto address = location.substr(scheme);
        if (azure)
        {
            const auto slash = address.find(L'/');
            if (slash == address.npos) return {};
            target.account = address.substr(0, slash);
            if (target.account.size() < 3 || target.account.size() > 24 || !std::ranges::all_of(target.account,
            [](wchar_t c) { return c >= 'a' && c <= 'z' || c >= '0' && c <= '9'; })) return {};
            address.remove_prefix(slash + 1);
        }
        const size_t slash = address.find(L'/');
        target.bucket = address.substr(0, slash);
        const bool reserved = azure && (target.bucket == L"$root" || target.bucket == L"$web");
        if (!reserved && (target.bucket.empty() || target.bucket.size() > (azure ? 63u : 255u) ||
            !std::ranges::all_of(target.bucket,
        [azure](wchar_t c) { return c >= 'a' && c <= 'z' || c >= '0' && c <= '9' ||
            c == '-' || !azure && c == '.'; })))
            return {};
        if (slash != address.npos) target.prefix = RemoteDecode(address.substr(slash + 1));
        if ((azure ? target.prefix.size() : RemoteUtf8(target.prefix).size()) > 1024) return {};
        std::set<std::wstring> seen;
        if (!options.empty()) for (const auto part : std::views::split(options.substr(1), L'&'))
        {
            const std::wstring_view field(part.begin(), part.end());
            const auto equal = field.find(L'=');
            if (equal == field.npos) return {};
            const std::wstring name(field.substr(0, equal));
            if (!seen.insert(name).second) return {};
            const auto value = RemoteDecode(field.substr(equal + 1));
            if (std::ranges::any_of(value, [](wchar_t c) { return c < 32 || c == 127; })) return {};
            if (name == L"region" && !azure) target.region = value;
            else if (name == L"endpoint") target.endpoint = value;
            else return {};
        }
        if (target.endpoint.empty() || !azure && target.region.empty()) return {};
        RemoteEndpoint(target.endpoint, azure);
        if (!std::ranges::all_of(target.region,
        [](wchar_t c) { return c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '-'; })) return {};
        return target;
    }
    catch (const RemoteFailure&) { return {}; }
}

std::optional<RemoteTarget> StorageSource::Parse(const RemoteTarget& target)
{
    // Treat incomplete Unicode input as invalid settings.
    try { return Parse(target.ToString()); }
    catch (const RemoteFailure&) { return {}; }
}

std::wstring StorageSource::ItemPath(const CItem* item)
{
    const CItem* root = item->GetEnumRoot();
    const auto [address, options] = SplitOptions(root->GetNameView());
    std::wstring result(address);
    std::vector<const CItem*> parts;
    for (; item != root; item = item->GetParent()) parts.push_back(item);
    for (const auto* part : parts | std::views::reverse)
    {
        result += part->GetNameView();
        if (part->IsTypeOrFlag(IT_DIRECTORY)) result += L'/';
    }
    return result.append(options);
}

static std::mutex remoteCredentialsMutex;
static std::unordered_map<std::wstring, RemoteCredentials> remoteCredentials;

static std::wstring RemoteCredentialName(RemoteTarget target)
{
    if (target.protocol != RemoteProtocol::WebDav) target.prefix.clear();
    target.EndCollection();
    return L"WinDirStat/" + target.ToString();
}

RemoteCredentials StorageSource::GetCredentials(const RemoteTarget& target)
{
    // Credentials are scoped to a connection and never form part of an item URI.
    const auto name = RemoteCredentialName(target);
    std::lock_guard guard(remoteCredentialsMutex);
    if (const auto found = remoteCredentials.find(name); found != remoteCredentials.end()) return found->second;
    SmartPointer credential(CredFree, PCREDENTIALW{});
    if (!CredRead(name.c_str(), CRED_TYPE_GENERIC, 0, &credential)) return {};
    RemoteCredentials result{ credential->UserName ? credential->UserName : L"" };
    try
    {
        result.secret = RemoteWide({ reinterpret_cast<const char*>(credential->CredentialBlob),
            credential->CredentialBlobSize });
    }
    catch (const RemoteFailure&) { return {}; }
    if (target.protocol == RemoteProtocol::S3)
    {
        // Separate the optional S3 session token from the secret key within the stored credential.
        if (const auto separator = result.secret.find(L'\n'); separator != result.secret.npos)
        {
            result.token = result.secret.substr(separator + 1);
            result.secret.resize(separator);
        }
    }
    return result;
}

DWORD StorageSource::SetCredentials(const RemoteTarget& target, const RemoteCredentials& credentials,
    const bool remember)
{
    // Persist only when requested; other credentials remain within this process.
    const auto name = RemoteCredentialName(target);
    if (remember)
    {
        // UTF-8 keeps long session tokens within the credential blob limit.
        std::string secret;
        try
        {
            secret = RemoteUtf8(target.protocol == RemoteProtocol::S3 && !credentials.token.empty() ?
                credentials.secret + L"\n" + credentials.token : credentials.secret);
        }
        catch (const RemoteFailure&) { return ERROR_NO_UNICODE_TRANSLATION; }
        CREDENTIAL credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<wchar_t*>(name.c_str());
        credential.UserName = const_cast<wchar_t*>(credentials.user.c_str());
        credential.CredentialBlob = reinterpret_cast<BYTE*>(secret.data());
        credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (!CredWrite(&credential, 0)) return GetLastError();
    }
    std::lock_guard guard(remoteCredentialsMutex);
    remoteCredentials[name] = credentials;
    return ERROR_SUCCESS;
}

void StorageSource::ForgetCredentials(const RemoteTarget& target, const std::span<const std::wstring> retained)
{
    // Connections sharing a credential scope keep it until the last one is removed.
    const auto name = RemoteCredentialName(target);
    for (const auto& path : retained)
        if (const auto other = Parse(path); other && RemoteCredentialName(*other) == name) return;
    CredDelete(name.c_str(), CRED_TYPE_GENERIC, 0);
    std::lock_guard guard(remoteCredentialsMutex);
    remoteCredentials.erase(name);
}

StorageSource::~StorageSource()
{
    SecureZeroMemory(credentials.secret.data(), credentials.secret.size() * sizeof(wchar_t));
    SecureZeroMemory(credentials.token.data(), credentials.token.size() * sizeof(wchar_t));
}

std::shared_ptr<StorageSource> StorageSource::Open(const std::wstring_view path, const bool renew)
{
    // Each enumeration root shares one source until an operation renews its credentials.
    static std::mutex mutex;
    static std::unordered_map<std::wstring, std::shared_ptr<StorageSource>> sources;
    const auto target = Parse(path);
    if (!target) return {};
    std::lock_guard guard(mutex);
    auto& source = sources[std::wstring(path)];
    if (source != nullptr && !renew) return source;
    source = std::make_shared<StorageSource>();
    source->target = *target;
    source->credentials = GetCredentials(*target);
    source->capabilities.flatListing = target->protocol != RemoteProtocol::WebDav;
    source->capabilities.batchDelete = target->protocol != RemoteProtocol::WebDav;
    source->transport = OpenRemoteTransport();
    return source;
}
