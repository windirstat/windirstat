// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FinderRemote.h"
#include "ProgressDlg.h"
#include "RemoteStorage.h"
#include <winhttp.h>
#include <xmllite.h>
#include <charconv>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "crypt32.lib")

struct RemoteInterrupted {};

static bool RemoteIsInterrupted(BlockingQueue<CItem*>* queue, const CProgressDlg* progress)
{
    return queue != nullptr && queue->IsPauseOrCancelRequested() || progress != nullptr && progress->IsCancelled();
}

static std::string RemoteHex(const std::span<const BYTE> bytes)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}

static std::vector<BYTE> RemoteHash(const std::string_view data, const std::string_view key = {},
    const wchar_t* digest = BCRYPT_SHA256_ALGORITHM)
{
    const auto closeAlgorithm = [](BCRYPT_ALG_HANDLE handle) { BCryptCloseAlgorithmProvider(handle, 0); };
    SmartPointer algorithm(closeAlgorithm, BCRYPT_ALG_HANDLE{});
    SmartPointer hash(BCryptDestroyHash, BCRYPT_HASH_HANDLE{});
    std::vector<BYTE> result(std::wstring_view(digest) == BCRYPT_MD5_ALGORITHM ? 16 : 32);
    RemoteCheck(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, digest, nullptr,
        key.empty() ? 0 : BCRYPT_ALG_HANDLE_HMAC_FLAG)) &&
        BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0,
            key.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
            static_cast<ULONG>(key.size()), 0)) &&
        BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
            static_cast<ULONG>(data.size()), 0)) &&
        BCRYPT_SUCCESS(BCryptFinishHash(hash, result.data(), static_cast<ULONG>(result.size()), 0)));
    return result;
}

struct RemoteHttpState
{
    SmartPointer<HANDLE, decltype(&CloseHandle)> ready{ CloseHandle, CreateEvent(nullptr, false, false, nullptr) };
    SmartPointer<HANDLE, decltype(&CloseHandle)> closed{ CloseHandle, CreateEvent(nullptr, true, false, nullptr) };
    DWORD error = 0, count = 0;

    static void CALLBACK Notify(HINTERNET, DWORD_PTR context, DWORD status, void* information, DWORD length)
    {
        auto* state = reinterpret_cast<RemoteHttpState*>(context);
        if (state == nullptr) return;
        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) { SetEvent(state->closed); return; }
        if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
            state->error = static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
        else if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) state->count = length;
        else if (status != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE &&
            status != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) return;
        SetEvent(state->ready);
    }

    void Wait(const BOOL started, BlockingQueue<CItem*>* queue, const CProgressDlg* progress)
    {
        RemoteCheck(started || GetLastError() == ERROR_IO_PENDING, GetLastError());
        const auto deadline = GetTickCount64() + 30000;
        while (WaitForSingleObject(ready, 50) == WAIT_TIMEOUT)
        {
            if (RemoteIsInterrupted(queue, progress)) throw RemoteInterrupted{};
            RemoteCheck(GetTickCount64() < deadline, ERROR_WINHTTP_TIMEOUT);
        }
        RemoteCheck(error == 0, error);
    }
};

class RemoteTransport final
{
    SmartPointer<HINTERNET, decltype(&WinHttpCloseHandle)> m_session{ WinHttpCloseHandle, nullptr };
    SmartPointer<HINTERNET, decltype(&WinHttpCloseHandle)> m_connection{ WinHttpCloseHandle, nullptr };
    std::mutex m_mutex;

public:
    std::atomic<bool> basic = false; // the server accepted a Basic challenge for this connection

    HINTERNET Connect(const std::wstring& endpoint)
    {
        std::lock_guard guard(m_mutex);
        if (m_connection != nullptr) return m_connection;
        const auto url = RemoteEndpoint(endpoint, true);

        // Follow the user's proxy configuration and current TLS versions wherever the system offers them.
        for (const DWORD access : { WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY })
        {
            m_session = WinHttpOpen(L"WinDirStat", access, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                WINHTTP_FLAG_ASYNC);
            if (m_session != nullptr) break;
        }
        RemoteCheck(m_session != nullptr, GetLastError());
        for (DWORD protocols : { WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3,
            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 })
            if (WinHttpSetOption(m_session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) break;
        WinHttpSetTimeouts(m_session, 10000, 10000, 10000, 10000);
        const std::wstring host(url.lpszHostName, url.dwHostNameLength);
        m_connection = WinHttpConnect(m_session, host.c_str(), url.nPort, 0);
        RemoteCheck(m_connection != nullptr, GetLastError());
        return m_connection;
    }
};

std::shared_ptr<RemoteTransport> OpenRemoteTransport()
{
    return std::make_shared<RemoteTransport>();
}

struct RemoteResponse
{
    std::string body;
    DWORD status = 0;
    std::wstring errorCode, contentType;
};

static std::wstring RemoteErrorBody(const std::string& body);

static RemoteResponse RemoteRequest(StorageSource& source, const std::wstring& endpoint, const std::wstring& resource,
    const wchar_t* method, const std::wstring& headers, const std::string& payload,
    BlockingQueue<CItem*>* queue, const RemoteCredentials& credentials = {}, const CProgressDlg* progress = nullptr)
{
    // Close asynchronous requests on pause/cancel before releasing their callback state and buffers.
    if (RemoteIsInterrupted(queue, progress)) throw RemoteInterrupted{};
    const auto url = RemoteEndpoint(endpoint, true);
    const std::wstring host(url.lpszHostName, url.dwHostNameLength);
    const auto connection = source.transport->Connect(endpoint);
    RemoteHttpState state;
    RemoteCheck(state.ready != nullptr && state.closed != nullptr, GetLastError());
    std::array<char, 16384> buffer{};
    const auto closeRequest = [&state](HINTERNET handle)
    {
        WinHttpCloseHandle(handle);
        WaitForSingleObject(state.closed, INFINITE);
    };
    SmartPointer request(WinHttpCloseHandle, WinHttpOpenRequest(connection, method, resource.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_DISABLE |
        WINHTTP_FLAG_ESCAPE_DISABLE_QUERY | (url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)));
    RemoteCheck(request != nullptr, GetLastError());
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER, logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    DWORD_PTR context = reinterpret_cast<DWORD_PTR>(&state);
    RemoteCheck(WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) &&
        WinHttpSetOption(request, WINHTTP_OPTION_AUTOLOGON_POLICY, &logon, sizeof(logon)) &&
        WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)), GetLastError());
    RemoteCheck(WinHttpSetStatusCallback(request, RemoteHttpState::Notify,
        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) != WINHTTP_INVALID_STATUS_CALLBACK,
        GetLastError());
    const SmartPointer active(closeRequest, request.Detach());

    // Repeat an accepted Basic scheme up front so later requests skip the unauthenticated round trip.
    if (source.transport->basic && !credentials.user.empty()) WinHttpSetCredentials(active,
        WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, credentials.user.c_str(),
        credentials.secret.c_str(), nullptr);
    RemoteResponse response;
    for (unsigned attempt = 0;; ++attempt)
    {
        state.Wait(WinHttpSendRequest(active, headers.c_str(), static_cast<DWORD>(headers.size()),
            payload.empty() ? nullptr : const_cast<char*>(payload.data()), static_cast<DWORD>(payload.size()),
            static_cast<DWORD>(payload.size()), context), queue, progress);
        state.Wait(WinHttpReceiveResponse(active, nullptr), queue, progress);
        DWORD size = sizeof(response.status);
        RemoteCheck(WinHttpQueryHeaders(active, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &response.status, &size, WINHTTP_NO_HEADER_INDEX), GetLastError());
        if (response.status != 401 || attempt != 0 || credentials.user.empty()) break;

        // Authenticate only to the requested endpoint; never forward credentials through redirects.
        DWORD supported = 0, first = 0, target = 0, scheme = 0;
        RemoteCheck(WinHttpQueryAuthSchemes(active, &supported, &first, &target), GetLastError());
        for (const DWORD candidate : { WINHTTP_AUTH_SCHEME_NEGOTIATE, WINHTTP_AUTH_SCHEME_NTLM,
            WINHTTP_AUTH_SCHEME_DIGEST, WINHTTP_AUTH_SCHEME_BASIC })
            if (supported & candidate) { scheme = candidate; break; }
        if (scheme == WINHTTP_AUTH_SCHEME_BASIC)
            RemoteCheck(url.nScheme == INTERNET_SCHEME_HTTPS || host == L"127.0.0.1" || host == L"[::1]" ||
                host == L"localhost", ERROR_WINHTTP_SECURE_FAILURE);
        RemoteCheck(scheme != 0 && WinHttpSetCredentials(active, WINHTTP_AUTH_TARGET_SERVER, scheme,
            credentials.user.c_str(), credentials.secret.c_str(), nullptr), ERROR_ACCESS_DENIED);
        source.transport->basic = scheme == WINHTTP_AUTH_SCHEME_BASIC;
    }
    const auto header = [&active](const DWORD query, const wchar_t* name)
    {
        std::wstring value;
        DWORD length = 0;
        WinHttpQueryHeaders(active, query, name, nullptr, &length, WINHTTP_NO_HEADER_INDEX);
        if (length <= sizeof(wchar_t) || length >= 4096) return value;
        value.resize(length / sizeof(wchar_t));
        value.resize(WinHttpQueryHeaders(active, query, name, value.data(), &length, WINHTTP_NO_HEADER_INDEX) ?
            length / sizeof(wchar_t) : 0);
        return value;
    };
    response.errorCode = header(WINHTTP_QUERY_CUSTOM, L"x-ms-error-code");
    response.contentType = header(WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX);
    for (;;)
    {
        if (RemoteIsInterrupted(queue, progress)) throw RemoteInterrupted{};
        state.Wait(WinHttpReadData(active, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr), queue, progress);
        if (state.count == 0) break;
        const size_t limit = response.status >= 400 ? 64 * 1024 :
            (response.status == 207 ? 64u : 16u) * 1024 * 1024;
        if (response.body.size() + state.count > limit && response.status >= 400) break;
        RemoteCheck(response.body.size() + state.count <= limit, ERROR_DS_SIZELIMIT_EXCEEDED);
        response.body.append(buffer.data(), state.count);
    }
    return response;
}

static void RemoteStatus(const DWORD status, const DWORD expected, const std::wstring& detail = {})
{
    if (status != expected) throw RemoteFailure{ std::format(L"HTTP {}", status) +
        (detail.empty() ? L"" : L" (" + detail + L")"),
        status == 429 || status == 500 || status == 502 || status == 503 || status == 504, status == 404 };
}

static void RemoteStatus(const RemoteResponse& response, const DWORD expected)
{
    if (response.status == expected) return;
    std::wstring detail = response.errorCode;
    const auto body = RemoteErrorBody(response.body);
    if (!body.empty()) detail += (detail.empty() ? L"" : L": ") + body;
    for (auto& c : detail) if (c < 32 || c == 127) c = L' ';
    if (detail.size() > 1024) detail.resize(1024);
    RemoteStatus(response.status, expected, detail);
}

static RemoteResponse RemoteS3Request(StorageSource& source, const RemoteTarget& target, const wchar_t* method,
    const std::wstring& resourcePath, const std::string& query, BlockingQueue<CItem*>* queue, CProgressDlg* progress,
    const std::string& body = {}, const std::wstring& extraHeaders = {})
{
    // Use only credentials supplied for this connection, keeping secrets outside scan targets and reports.
    auto [access, secret, token] = source.credentials;
    RemoteCheck(!access.empty() && !secret.empty(), ERROR_LOGON_FAILURE);
    for (const auto& value : { access, secret, token })
        RemoteCheck(std::ranges::none_of(value, [](wchar_t c) { return c < 32 || c == 127; }));
    const auto& region = target.region;
    const auto& endpoint = target.endpoint;

    // Sign a path-style request, including session tokens, using AWS Signature Version 4.
    const URL_COMPONENTS url = RemoteEndpoint(endpoint);
    const std::wstring host(url.lpszHostName, url.dwHostNameLength);
    std::wstring authority = host;
    if (url.nPort != (url.nScheme == INTERNET_SCHEME_HTTPS ? 443 : 80)) authority += L":" + std::to_wstring(url.nPort);
    const std::string resource = RemoteUtf8(resourcePath);
    SYSTEMTIME now{};
    GetSystemTime(&now);
    const std::string date = std::format("{:04}{:02}{:02}", now.wYear, now.wMonth, now.wDay);
    const std::string timestamp = date + std::format("T{:02}{:02}{:02}Z", now.wHour, now.wMinute, now.wSecond);
    const std::string payload = RemoteHex(RemoteHash(body));
    std::string signedHeaders = "host;x-amz-content-sha256;x-amz-date";
    std::string headers = "host:" + RemoteUtf8(authority) + "\nx-amz-content-sha256:" + payload +
        "\nx-amz-date:" + timestamp + "\n";
    if (!token.empty())
    {
        headers += "x-amz-security-token:" + RemoteUtf8(token) + "\n";
        signedHeaders += ";x-amz-security-token";
    }
    const std::string canonical = RemoteUtf8(method) + "\n" + resource + "\n" + query + "\n" +
        headers + "\n" + signedHeaders + "\n" + payload;
    const std::string scope = date + "/" + RemoteUtf8(region) + "/s3/aws4_request";
    const std::string toSign = "AWS4-HMAC-SHA256\n" + timestamp + "\n" + scope + "\n" +
        RemoteHex(RemoteHash(canonical));
    auto signingKey = RemoteHash(date, "AWS4" + RemoteUtf8(secret));
    for (const auto& part : { RemoteUtf8(region), std::string("s3"), std::string("aws4_request") })
        signingKey = RemoteHash(part, { reinterpret_cast<const char*>(signingKey.data()), signingKey.size() });
    const auto signature = RemoteHex(RemoteHash(toSign,
        { reinterpret_cast<const char*>(signingKey.data()), signingKey.size() }));
    SecureZeroMemory(signingKey.data(), signingKey.size());
    SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
    std::wstring requestHeaders = RemoteWide("x-amz-content-sha256: " + payload + "\r\nx-amz-date: " + timestamp +
        "\r\nAuthorization: AWS4-HMAC-SHA256 Credential=" + RemoteUtf8(access) + "/" + scope +
        ", SignedHeaders=" + signedHeaders + ", Signature=" + signature + "\r\n");
    if (!token.empty()) requestHeaders += L"x-amz-security-token: " + token + L"\r\n";

    return RemoteRequest(source, endpoint, resourcePath + (query.empty() ? L"" : L"?" + RemoteWide(query)),
        method, requestHeaders + extraHeaders, body, queue, {}, progress);
}

struct RemoteXml
{
    std::wstring name, space, text;
    bool encoded = false;
    std::vector<RemoteXml> children;

    const RemoteXml* Child(const std::wstring_view local, const std::wstring_view ns = {}) const
    {
        const RemoteXml* found = nullptr;
        for (const auto& child : children) if (child.name == local && child.space == ns)
        {
            RemoteCheck(found == nullptr);
            found = &child;
        }
        return found;
    }
};

static RemoteXml RemoteReadXml(const std::string& body, BlockingQueue<CItem*>* queue,
    const CProgressDlg* progress = nullptr, const std::function<void(const RemoteXml&)>& each = {})
{
    // Bound untrusted XML and prohibit external entities before inspecting protocol-specific properties.
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(body.data()), static_cast<UINT>(body.size())));
    CComPtr<IXmlReader> reader;
    RemoteCheck(stream != nullptr && SUCCEEDED(CreateXmlReader(IID_PPV_ARGS(&reader), nullptr)) &&
        SUCCEEDED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) &&
        SUCCEEDED(reader->SetInput(stream)));
    RemoteXml document;
    std::vector<RemoteXml*> stack{ &document };
    XmlNodeType node;
    HRESULT result;
    size_t count = 0;
    while ((result = reader->Read(&node)) == S_OK)
    {
        if (RemoteIsInterrupted(queue, progress)) throw RemoteInterrupted{};
        bool completed = false;
        if (node == XmlNodeType_Element)
        {
            RemoteCheck(++count <= 1000000, ERROR_DS_SIZELIMIT_EXCEEDED);
            RemoteCheck(stack.size() <= 32);
            auto& element = stack.back()->children.emplace_back();
            const wchar_t* value = nullptr;
            UINT length = 0;
            RemoteCheck(SUCCEEDED(reader->GetLocalName(&value, &length)));
            element.name.assign(value, length);
            RemoteCheck(SUCCEEDED(reader->GetNamespaceUri(&value, &length)));
            element.space.assign(value, length);
            if (reader->MoveToAttributeByName(L"Encoded", nullptr) == S_OK)
            {
                RemoteCheck(SUCCEEDED(reader->GetValue(&value, &length)));
                element.encoded = std::wstring_view(value, length) == L"true";
                reader->MoveToElement();
            }
            if (!reader->IsEmptyElement()) stack.push_back(&element);
            else completed = true;
        }
        else if (node == XmlNodeType_EndElement)
        {
            RemoteCheck(stack.size() > 1);
            stack.pop_back();
            completed = true;
        }
        else if (node == XmlNodeType_Text || node == XmlNodeType_Whitespace || node == XmlNodeType_CDATA)
        {
            const wchar_t* value = nullptr;
            UINT length = 0;
            RemoteCheck(SUCCEEDED(reader->GetValue(&value, &length)));
            stack.back()->text.append(value, length);
        }

        // Hand each completed entry to its consumer so a large listing never accumulates in memory.
        if (completed && each && stack.size() == 2)
        {
            each(stack.back()->children.back());
            stack.back()->children.pop_back();
            count = 0;
        }
    }
    RemoteCheck(result == S_FALSE && stack.size() == 1 && document.children.size() == 1);
    return std::move(document.children.front());
}

static std::wstring RemoteErrorBody(const std::string& body)
{
    try
    {
        if (body.empty()) return {};
        const auto document = RemoteReadXml(body, nullptr, nullptr);
        const auto* code = document.Child(L"Code", document.space);
        const auto* message = document.Child(L"Message", document.space);
        return (code ? code->text : L"") + std::wstring(code && message ? L": " : L"") +
            (message ? message->text : L"");
    }
    catch (const RemoteFailure&) { return {}; }
}

static ULONGLONG RemoteSize(const RemoteXml* element)
{
    RemoteCheck(element != nullptr);
    const auto number = RemoteUtf8(element->text);
    ULONGLONG size = 0;
    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), size);
    RemoteCheck(parsed.ec == std::errc{} && parsed.ptr == number.data() + number.size());
    return size;
}

static FILETIME RemoteTime(const RemoteXml* element)
{
    SYSTEMTIME time{};
    FILETIME result{};
    RemoteCheck(element != nullptr && WinHttpTimeToSystemTime(element->text.c_str(), &time) &&
        SystemTimeToFileTime(&time, &result));
    return result;
}

FinderRemote::Page FinderRemote::ReadS3Page(const bool recursive)
{
    // Read a bounded page with exact keys and opaque continuation tokens.
    std::string query;
    if (!m_token.empty()) query = "continuation-token=" + RemoteEncode(m_token) + "&";
    if (!recursive) query += "delimiter=%2F&";
    query += "encoding-type=url&list-type=2&max-keys=1000&prefix=" + RemoteEncode(m_target.prefix);
    const auto response = RemoteS3Request(*m_source, m_target, L"GET", L"/" + m_target.bucket,
        query, m_queue, m_progress);
    RemoteStatus(response, 200);
    const auto document = RemoteReadXml(response.body, m_queue, m_progress);
    RemoteCheck(document.name == L"ListBucketResult");
    const auto* encoding = document.Child(L"EncodingType", document.space);
    const auto* truncated = document.Child(L"IsTruncated", document.space);
    const auto* token = document.Child(L"NextContinuationToken", document.space);
    RemoteCheck(encoding != nullptr && encoding->text == L"url" && truncated != nullptr &&
        (truncated->text == L"true" || truncated->text == L"false"));
    Page page;
    if (truncated->text == L"true")
    {
        RemoteCheck(token != nullptr && !token->text.empty());
        page.nextToken = token->text;
    }
    for (const auto& object : document.children)
    {
        if (object.space != document.space || (object.name != L"Contents" && object.name != L"CommonPrefixes"))
            continue;
        Entry entry;
        entry.directory = object.name == L"CommonPrefixes";
        const auto* key = object.Child(entry.directory ? L"Prefix" : L"Key", document.space);
        RemoteCheck(key != nullptr);

        // Listings form-encode keys: a literal plus arrives escaped and a bare plus is a space.
        std::wstring encoded = key->text;
        std::ranges::replace(encoded, L'+', L' ');
        entry.key = RemoteDecode(encoded);
        RemoteCheck(!entry.key.empty() && RemoteUtf8(entry.key).size() <= 1024 &&
            entry.key.starts_with(m_target.prefix) && page.entries.size() < 1000);
        const auto* size = object.Child(L"Size", document.space);
        const auto* modified = object.Child(L"LastModified", document.space);
        if (entry.directory)
        {
            RemoteCheck(!recursive && size == nullptr && modified == nullptr && entry.key.ends_with(L'/') &&
                entry.key.size() > m_target.prefix.size());
            entry.key.pop_back();
        }
        else
        {
            entry.size = RemoteSize(size);
            SYSTEMTIME time{};
            RemoteCheck(modified != nullptr && modified->text.ends_with(L'Z') &&
                swscanf_s(modified->text.c_str(), L"%4hu-%2hu-%2huT%2hu:%2hu:%2hu", &time.wYear, &time.wMonth,
                    &time.wDay, &time.wHour, &time.wMinute, &time.wSecond) == 6 &&
                SystemTimeToFileTime(&time, &entry.modified));
        }
        RemoteCheck(recursive || entry.key.substr(m_target.prefix.size()).find(L'/') == std::wstring::npos);
        page.entries.push_back(std::move(entry));
    }
    return page;
}

static std::wstring RemoteEncodePath(const std::wstring_view path)
{
    // Preserve literal object-key components that an HTTP client would otherwise treat as navigation.
    std::string encoded;
    bool first = true;
    for (const auto part : std::views::split(path, L'/'))
    {
        if (!first) encoded += '/';
        first = false;
        const std::wstring_view component(part.begin(), part.end());
        if (component == L".") encoded += "%2E";
        else if (component == L"..") encoded += "%2E%2E";
        else encoded += RemoteEncode(component);
    }
    return RemoteWide(encoded);
}

static std::wstring RemoteBase64(const std::span<const BYTE> bytes)
{
    DWORD length = 0;
    constexpr DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    RemoteCheck(CryptBinaryToString(bytes.data(), static_cast<DWORD>(bytes.size()), flags, nullptr, &length));
    std::wstring result(length, L'\0');
    RemoteCheck(CryptBinaryToString(bytes.data(), static_cast<DWORD>(bytes.size()), flags, result.data(), &length));
    result.resize(length);
    return result;
}

struct RemoteAzureSpec { std::wstring resource, headers; };

// A shared access signature is recognized by its signature field; an account key has none.
static std::wstring_view RemoteSasToken(std::wstring_view secret)
{
    if (secret.starts_with(L'?')) secret.remove_prefix(1);
    return secret.starts_with(L"sig=") || secret.contains(L"&sig=") ? secret : std::wstring_view{};
}

static RemoteAzureSpec RemoteAzureAuthorize(StorageSource& source, const RemoteTarget& target, const wchar_t* method,
    const std::wstring& object, const std::map<std::wstring, std::wstring>& parameters,
    const std::string& body = {}, const std::wstring& contentType = {}, bool subrequest = false)
{
    // Share resource encoding and authorization between container listings and blob deletion.
    const bool deleting = std::wstring_view(method) == L"DELETE";
    std::wstring endpoint = target.endpoint;
    while (endpoint.ends_with(L'/')) endpoint.pop_back();
    const auto url = RemoteEndpoint(endpoint, true);
    const std::wstring base = url.dwUrlPathLength ?
        RemoteDecode(std::wstring_view(url.lpszUrlPath, url.dwUrlPathLength)) : std::wstring{};
    const std::wstring resource = RemoteEncodePath(base + L"/" + target.bucket + (deleting ? L"/" + object : L""));
    std::wstring query, canonical;
    for (const auto& [name, value] : parameters)
    {
        if (!query.empty()) query += L'&';
        query += name + L"=" + RemoteWide(RemoteEncode(value));
        canonical += L"\n" + name + L":" + value;
    }

    // Use the connection's key or SAS token without including credentials in scan identities.
    auto credentials = source.credentials;
    RemoteCheck(!credentials.secret.empty(), ERROR_LOGON_FAILURE);
    SYSTEMTIME now{};
    GetSystemTime(&now);
    wchar_t date[WINHTTP_TIME_FORMAT_BUFSIZE]{};
    RemoteCheck(WinHttpTimeFromSystemTime(&now, date));
    const std::wstring version = L"2021-12-02";
    std::wstring headers = L"x-ms-date: " + std::wstring(date) + L"\r\n";
    std::wstring canonicalHeaders = L"x-ms-date:" + std::wstring(date) + L"\n";
    if (deleting)
    {
        headers += L"x-ms-delete-snapshots: include\r\n";
        canonicalHeaders += L"x-ms-delete-snapshots:include\n";
    }
    if (!subrequest)
    {
        headers += L"x-ms-version: " + version + L"\r\n";
        canonicalHeaders += L"x-ms-version:" + version + L"\n";
    }
    if (!contentType.empty()) headers += L"Content-Type: " + contentType + L"\r\n";
    if (const auto sas = RemoteSasToken(credentials.secret); !sas.empty())
    {
        std::set<std::wstring> fields;
        for (const auto part : std::views::split(sas, L'&'))
        {
            const std::wstring_view field(part.begin(), part.end());
            const auto equal = field.find(L'=');
            RemoteCheck(equal != field.npos);
            const std::wstring name(field.substr(0, equal));
            constexpr std::wstring_view sasFields = L"|sv|ss|srt|sp|se|st|spr|sip|si|sr|sig|skoid|sktid|skt|ske|"
                L"sks|skv|saoid|suoid|scid|ses|sdd|rscc|rscd|rsce|rscl|rsct|";
            RemoteCheck(!name.empty() && fields.insert(name).second && sasFields.contains(L"|" + name + L"|"));
            if (!query.empty()) query += L'&';
            query += name + L"=" + RemoteWide(RemoteEncode(RemoteDecode(field.substr(equal + 1))));
        }
    }
    else
    {
        DWORD length = 0;
        RemoteCheck(CryptStringToBinary(credentials.secret.c_str(), 0, CRYPT_STRING_BASE64,
            nullptr, &length, nullptr, nullptr) && length > 0);
        std::string key(length, '\0');
        RemoteCheck(CryptStringToBinary(credentials.secret.c_str(), 0, CRYPT_STRING_BASE64,
            reinterpret_cast<BYTE*>(key.data()), &length, nullptr, nullptr));
        const auto signature = RemoteHash(RemoteUtf8(std::wstring(method) + L"\n\n\n" +
            (body.empty() ? L"" : std::to_wstring(body.size())) + L"\n\n" + contentType + L"\n\n\n\n\n\n\n" +
            canonicalHeaders + L"/" + target.account + resource + canonical), key);
        SecureZeroMemory(key.data(), key.size());
        headers += L"Authorization: SharedKey " + target.account + L":" + RemoteBase64(signature) + L"\r\n";
    }
    SecureZeroMemory(credentials.secret.data(), credentials.secret.size() * sizeof(wchar_t));
    return { resource + (query.empty() ? L"" : L"?" + query), headers };
}

static RemoteResponse RemoteAzureRequest(StorageSource& source, const RemoteTarget& target,
    const wchar_t* method, const std::wstring& object, const std::map<std::wstring, std::wstring>& parameters,
    BlockingQueue<CItem*>* queue, CProgressDlg* progress, const std::string& body = {},
    const std::wstring& contentType = {})
{
    const auto request = RemoteAzureAuthorize(source, target, method, object, parameters, body, contentType);
    return RemoteRequest(source, target.endpoint, request.resource, method, request.headers, body, queue, {}, progress);
}

FinderRemote::Page FinderRemote::ReadAzurePage(const bool recursive, const bool probe)
{
    // List current committed blobs, retaining opaque continuation markers and exact object names.
    std::wstring prefix = m_target.prefix;
    if (probe) prefix.pop_back();
    std::map<std::wstring, std::wstring> parameters{ { L"comp", L"list" }, { L"maxresults", probe ? L"1" : L"1000" },
        { L"prefix", prefix }, { L"restype", L"container" } };
    if (!recursive) parameters[L"delimiter"] = L"/";
    if (!m_token.empty()) parameters[L"marker"] = m_token;
    const auto response = RemoteAzureRequest(*m_source, m_target, L"GET", {}, parameters, m_queue, m_progress);
    RemoteStatus(response, 200);
    const auto document = RemoteReadXml(response.body, m_queue, m_progress);
    RemoteCheck(document.name == L"EnumerationResults");
    const auto* blobs = document.Child(L"Blobs");
    const auto* marker = document.Child(L"NextMarker");
    RemoteCheck(blobs != nullptr && marker != nullptr && blobs->children.size() <= 1000);
    Page page;
    for (const auto& blob : blobs->children)
    {
        RemoteCheck(blob.name == L"Blob" || blob.name == L"BlobPrefix");
        const auto* name = blob.Child(L"Name");
        RemoteCheck(name != nullptr && !name->text.empty());
        Entry entry;
        entry.key = name->encoded ? RemoteDecode(name->text) : name->text;
        entry.directory = blob.name == L"BlobPrefix";
        RemoteCheck(entry.key.starts_with(prefix) && (!recursive || !entry.directory));
        const auto* properties = blob.Child(L"Properties");
        if (entry.directory && properties != nullptr && properties->Child(L"Last-Modified") != nullptr)
            entry.modified = RemoteTime(properties->Child(L"Last-Modified"));
        if (!entry.directory)
        {
            RemoteCheck(properties != nullptr);
            const auto* type = properties->Child(L"ResourceType");
            entry.directory = type != nullptr && type->text == L"directory";
            if (!entry.directory) entry.size = RemoteSize(properties->Child(L"Content-Length"));
            entry.modified = RemoteTime(properties->Child(L"Last-Modified"));
        }
        if (recursive)
        {
            // Retain the selected concrete directory, but never include a similarly named sibling blob.
            if (probe && !(entry.directory && entry.key == prefix)) continue;
        }
        else if (entry.directory)
        {
            if (entry.key.ends_with(L'/')) entry.key.pop_back();
            if (entry.key + L"/" == m_target.prefix) continue;
        }
        RemoteCheck(recursive || entry.key.starts_with(m_target.prefix) &&
            entry.key.substr(m_target.prefix.size()).find(L'/') == std::wstring::npos);
        page.entries.push_back(std::move(entry));
    }
    page.nextToken = marker->text;

    // A concrete directory sorts first among names sharing its text, so one result finds it without its siblings.
    if (recursive && !probe && m_token.empty() && m_target.prefix.ends_with(L'/'))
        page.entries.append_range(ReadAzurePage(true, true).entries);
    return page;
}

static DWORD RemoteDavStatus(const RemoteXml* element)
{
    RemoteCheck(element != nullptr);
    unsigned major = 0, minor = 0, code = 0;
    RemoteCheck(swscanf_s(element->text.c_str(), L"HTTP/%u.%u %u", &major, &minor, &code) == 3);
    return code;
}

FinderRemote::Page FinderRemote::ReadWebDavPage()
{
    // Depth one avoids unbounded server recursion and obtains sizes without downloading file content.
    const std::string body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\"><d:prop><d:resourcetype/><d:getcontentlength/>"
        "<d:getlastmodified/></d:prop></d:propfind>";
    const auto& credentials = m_source->credentials;
    RemoteCheck(!credentials.user.empty() && !credentials.secret.empty(), ERROR_LOGON_FAILURE);
    const auto response = RemoteRequest(*m_source, m_target.endpoint,
        L"/" + RemoteEncodePath(m_target.prefix), L"PROPFIND",
        L"Depth: 1\r\nContent-Type: application/xml; charset=utf-8\r\n", body, m_queue, credentials, m_progress);
    RemoteStatus(response, 207);
    Page page;
    std::unordered_set<std::wstring> seen;
    bool self = false;
    const auto document = RemoteReadXml(response.body, m_queue, m_progress, [&](const RemoteXml& item)
    {
        if (item.name != L"response" || item.space != L"DAV:") return;
        const auto* href = item.Child(L"href", L"DAV:");
        RemoteCheck(href != nullptr);
        std::wstring address = href->text;
        if (address.starts_with(L'/')) address = m_target.endpoint + address;
        const auto identity = StorageSource::Parse(address);
        RemoteCheck(identity && identity->SameConnection(m_target) && seen.insert(identity->prefix).second);
        if (const auto* status = item.Child(L"status", L"DAV:")) RemoteStatus(RemoteDavStatus(status), 200);
        Entry entry;
        entry.key = identity->prefix;
        bool hasType = false, hasSize = false;
        for (const auto& propstat : item.children)
        {
            if (propstat.name != L"propstat" || propstat.space != L"DAV:") continue;
            if (RemoteDavStatus(propstat.Child(L"status", L"DAV:")) != 200) continue;
            const auto* properties = propstat.Child(L"prop", L"DAV:");
            RemoteCheck(properties != nullptr);
            if (const auto* type = properties->Child(L"resourcetype", L"DAV:"))
            {
                RemoteCheck(!hasType); hasType = true;
                entry.directory = type->Child(L"collection", L"DAV:") != nullptr;
            }
            if (const auto* size = properties->Child(L"getcontentlength", L"DAV:"))
            {
                RemoteCheck(!hasSize); hasSize = true;
                entry.size = RemoteSize(size);
            }
            if (const auto* modified = properties->Child(L"getlastmodified", L"DAV:"))
                entry.modified = RemoteTime(modified);
        }
        RemoteCheck(hasType && (entry.directory || hasSize));
        if (entry.directory)
        {
            entry.size = 0;
            if (!entry.key.empty() && entry.key.ends_with(L'/')) entry.key.pop_back();
            if (entry.key + L"/" == m_target.prefix || entry.key.empty() && m_target.prefix.empty())
            {
                RemoteCheck(!self); self = true; page.directoryTime = entry.modified; return;
            }
        }
        RemoteCheck(entry.key.starts_with(m_target.prefix) && entry.key.size() > m_target.prefix.size() &&
            entry.key.substr(m_target.prefix.size()).find(L'/') == std::wstring::npos);
        page.entries.push_back(std::move(entry));
    });
    RemoteCheck(document.name == L"multistatus" && document.space == L"DAV:" && self);
    return page;
}

void FinderRemote::LoadPage(const bool recursive)
{
    // Validate the complete page before advancing the cursor or publishing entries.
    auto page = m_target.protocol == RemoteProtocol::S3 ? ReadS3Page(recursive) :
        m_target.protocol == RemoteProtocol::Azure ? ReadAzurePage(recursive) : ReadWebDavPage();
    if (!page.nextToken.empty()) RemoteCheck(m_tokens.insert(page.nextToken).second);
    if (m_target.protocol == RemoteProtocol::Azure && !recursive)
    {
        // Azure can return the same directory as both a prefix and a concrete blob, including across pages.
        std::erase_if(page.entries, [this](const Entry& entry) {
            return entry.directory && !m_directories.insert(entry.key).second;
        });
    }
    m_more = !page.nextToken.empty();
    m_token = std::move(page.nextToken);
    m_directoryTime = page.directoryTime;
    m_entries = std::move(page.entries);
    m_position = 0;
}

bool FinderRemote::FindFile(const CItem* item)
{
    m_directoryTime = {};
    m_result = {};
    m_published = false;

    // Queued directories do not each wait out a connection that keeps failing.
    const auto target = StorageSource::Parse(item->GetPath());
    const bool offline = m_source != nullptr && m_source->failures >= 3;
    if (!target || m_source == nullptr || offline)
    {
        m_result = { ScanOutcome::ConnectionError, item->GetPath() + L": " +
            TranslateError(offline ? ERROR_UNEXP_NET_ERR : ERROR_INVALID_NAME) };
        return false;
    }
    m_target = *target;
    m_entries.clear(); m_position = 0; m_token.clear(); m_tokens.clear(); m_directories.clear(); m_more = true;
    return FindNext();
}

bool FinderRemote::FindNext()
{
    if (!m_entries.empty()) ++m_position;
    try
    {
        while (m_position >= m_entries.size() && m_more)
        {
            for (unsigned attempt = 0;;)
            {
                m_queue->WaitIfSuspended();
                try { LoadPage(m_flat); m_source->failures = 0; break; }
                catch (const RemoteInterrupted&) { m_queue->WaitIfSuspended(); }
                catch (const RemoteFailure& failure)
                {
                    if (!failure.retry || ++attempt >= 3) throw;
                    for (unsigned tick = 0; tick < (10u << attempt); ++tick)
                        { m_queue->WaitIfSuspended(); Sleep(25); }
                }
            }
        }
        if (m_position >= m_entries.size())
        {
            m_result.outcome = ScanOutcome::Complete;
            return false;
        }

        // Reject names that cannot be represented by an item before they reach the scan worker.
        m_name = GetRelativeKey();
        RemoteCheck(StorageSource::EscapeName(m_name).size() <= std::numeric_limits<USHORT>::max(),
            ERROR_FILENAME_EXCED_RANGE);
        m_published = true;
        return true;
    }
    catch (const RemoteFailure& failure)
    {
        if (failure.retry) ++m_source->failures;
        m_result = { m_published ? ScanOutcome::Partial : failure.missing ? ScanOutcome::Missing :
            ScanOutcome::ConnectionError, m_target.ToString() + L": " + failure.message };
        return false;
    }
    catch (const std::exception&)
    {
        m_result.outcome = ScanOutcome::Cancelled;
        throw;
    }
}

std::wstring FinderRemote::GetFilePath() const
{
    auto target = m_target;
    target.prefix = Current().key + (Current().directory ? L"/" : L"");
    return target.ToString();
}

struct RemoteDeleteObject
{
    std::wstring key;
    bool directory = false;
};

// Discovered file keys are spooled to disk in listing order; the few directory objects are ordered in memory.
class RemoteDeletePlan final
{
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_spool{ CloseHandle, INVALID_HANDLE_VALUE };
    std::vector<std::wstring> m_directories;
    size_t m_directory = 0;

public:
    RemoteDeletePlan()
    {
        wchar_t directory[MAX_PATH]{}, path[MAX_PATH]{};
        const DWORD length = GetTempPath(MAX_PATH, directory);
        RemoteCheck(length > 0 && length < MAX_PATH && GetTempFileName(directory, L"wds", 0, path), GetLastError());
        m_spool = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (m_spool == INVALID_HANDLE_VALUE) { DeleteFile(path); RemoteCheck(false, GetLastError()); }
    }
    void Add(const std::wstring& key, const bool directory)
    {
        RemoteCheck(key.size() <= std::numeric_limits<USHORT>::max());
        if (directory) { m_directories.push_back(key); return; }
        const DWORD length = static_cast<DWORD>(key.size());
        DWORD count = 0;
        RemoteCheck(WriteFile(m_spool, &length, sizeof(length), &count, nullptr) && count == sizeof(length) &&
            WriteFile(m_spool, key.data(), length * sizeof(wchar_t), &count, nullptr) &&
            count == length * sizeof(wchar_t), GetLastError());
    }
    void Finish()
    {
        // Directory objects are removed deepest first, after every file.
        std::ranges::sort(m_directories, [](const std::wstring& lhs, const std::wstring& rhs)
        {
            return lhs.size() != rhs.size() ? lhs.size() > rhs.size() : lhs < rhs;
        });
        RemoteCheck(SetFilePointerEx(m_spool, {}, nullptr, FILE_BEGIN), GetLastError());
    }
    std::optional<RemoteDeleteObject> Next()
    {
        DWORD length = 0, count = 0;
        RemoteCheck(ReadFile(m_spool, &length, sizeof(length), &count, nullptr), GetLastError());
        if (count == 0 && m_directory == m_directories.size()) return {};
        if (count == 0) return RemoteDeleteObject{ m_directories[m_directory++], true };
        RemoteCheck(count == sizeof(length) && length <= std::numeric_limits<USHORT>::max());
        RemoteDeleteObject object{ std::wstring(length, L'\0') };
        RemoteCheck(ReadFile(m_spool, object.key.data(), length * sizeof(wchar_t), &count, nullptr) &&
            count == length * sizeof(wchar_t));
        return object;
    }
};

static std::optional<std::string> RemoteXmlKey(const std::wstring& key)
{
    if (key.contains(L'\uFFFE') || key.contains(L'\uFFFF')) return {};
    std::string result;
    for (const unsigned char c : RemoteUtf8(key))
    {
        if (c < 32 && c != 9 && c != 10 && c != 13) return {};
        if (c == '&') result += "&amp;";
        else if (c == '<') result += "&lt;";
        else if (c == '>') result += "&gt;";
        else if (c == '"') result += "&quot;";
        else if (c == 9) result += "&#9;";
        else if (c == 10) result += "&#10;";
        else if (c == 13) result += "&#13;";
        else result += c;
    }
    return result;
}

static bool RemoteS3DeleteBatch(StorageSource& source, const RemoteTarget& target,
    const std::vector<RemoteDeleteObject>& objects, CProgressDlg* progress)
{
    std::string body = "<Delete xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">";
    for (const auto& object : objects)
    {
        const auto key = RemoteXmlKey(object.key);
        if (!key) return false;
        body += "<Object><Key>" + *key + "</Key></Object>";
    }
    body += "</Delete>";
    const auto response = RemoteS3Request(source, target, L"POST", L"/" + target.bucket, "delete=", nullptr,
        progress, body, L"Content-Type: application/xml\r\nContent-MD5: " +
        RemoteBase64(RemoteHash(body, {}, BCRYPT_MD5_ALGORITHM)) + L"\r\n");
    if (response.status == 405 || response.status == 501) { source.capabilities.batchDelete = false; return false; }
    RemoteStatus(response, 200);
    const auto document = RemoteReadXml(response.body, nullptr, progress);
    RemoteCheck(document.name == L"DeleteResult");
    std::unordered_set<std::wstring> pending;
    for (const auto& object : objects) pending.insert(object.key);
    std::wstring errors;
    for (const auto& result : document.children)
    {
        RemoteCheck((result.name == L"Deleted" || result.name == L"Error") && result.space == document.space);
        const auto* key = result.Child(L"Key", document.space);
        RemoteCheck(key != nullptr && pending.erase(key->text) == 1);
        if (result.name == L"Error")
        {
            const auto* code = result.Child(L"Code", document.space);
            RemoteCheck(code != nullptr);
            auto identity = target; identity.prefix = key->text;
            if (errors.size() < 4096) errors += identity.ToString() + L": " + code->text + L"\n";
        }
        else progress->Increment();
    }
    RemoteCheck(pending.empty());
    if (!errors.empty()) throw RemoteFailure{ errors };
    return true;
}

static bool RemoteAzureDeleteBatch(StorageSource& source, const RemoteTarget& target,
    const std::vector<RemoteDeleteObject>& objects, CProgressDlg* progress)
{
    // A parent batch requires write permission in addition to each blob's delete permission.
    if (const auto sas = RemoteSasToken(source.credentials.secret); !sas.empty())
    {
        bool writable = false;
        for (const auto part : std::views::split(sas, L'&'))
        {
            const std::wstring_view field(part.begin(), part.end());
            if (field.starts_with(L"sp=")) writable = RemoteDecode(field.substr(3)).contains(L'w');
        }
        if (!writable) return false;
    }
    const std::string boundary = "batch_" + std::to_string(GetTickCount64());
    std::string body;
    for (const auto [index, object] : std::views::enumerate(objects))
    {
        const auto request = RemoteAzureAuthorize(source, target, L"DELETE", object.key, {}, {}, {}, true);
        body += "--" + boundary + "\r\nContent-Type: application/http\r\nContent-Transfer-Encoding: binary\r\n" +
            "Content-ID: " + std::to_string(index) + "\r\n\r\nDELETE " + RemoteUtf8(request.resource) +
            " HTTP/1.1\r\n" + RemoteUtf8(request.headers) + "Content-Length: 0\r\n\r\n";
    }
    body += "--" + boundary + "--\r\n";
    RemoteCheck(body.size() <= 4 * 1024 * 1024);
    const auto response = RemoteAzureRequest(source, target, L"POST", {},
        { { L"comp", L"batch" }, { L"restype", L"container" } }, nullptr, progress, body,
        L"multipart/mixed; boundary=" + RemoteWide(boundary));
    if (response.status == 405 || response.status == 501) { source.capabilities.batchDelete = false; return false; }
    RemoteStatus(response, 202);
    std::wstring type = response.contentType;
    const auto marker = type.find(L"boundary=");
    RemoteCheck(type.starts_with(L"multipart/mixed") && marker != type.npos);
    std::wstring responseBoundary = type.substr(marker + 9);
    while (!responseBoundary.empty() && (responseBoundary.back() == 0 || iswspace(responseBoundary.back())))
        responseBoundary.pop_back();
    if (responseBoundary.size() >= 2 && responseBoundary.front() == L'"' && responseBoundary.back() == L'"')
        responseBoundary = responseBoundary.substr(1, responseBoundary.size() - 2);
    RemoteCheck(!responseBoundary.empty() && responseBoundary.size() <= 200 && !responseBoundary.contains(L'\r') &&
        !responseBoundary.contains(L'\n'));
    const std::string delimiter = "--" + RemoteUtf8(responseBoundary);
    std::unordered_set<size_t> seen;
    std::wstring errors;
    size_t position = 0;
    for (;;)
    {
        RemoteCheck(response.body.compare(position, delimiter.size(), delimiter) == 0);
        position += delimiter.size();
        if (response.body.compare(position, 2, "--") == 0) break;
        RemoteCheck(response.body.compare(position, 2, "\r\n") == 0);
        position += 2;
        const auto next = response.body.find("\r\n" + delimiter, position);
        RemoteCheck(next != response.body.npos);
        const std::string part = response.body.substr(position, next - position);
        const auto headerEnd = part.find("\r\n\r\n"), id = part.find("Content-ID: ");
        RemoteCheck(headerEnd != part.npos && id < headerEnd);
        size_t index = 0;
        const auto number = std::from_chars(part.data() + id + 12, part.data() + headerEnd, index);
        RemoteCheck(number.ec == std::errc{} && index < objects.size() && seen.insert(index).second &&
            (number.ptr == part.data() + headerEnd ||
            std::string_view(number.ptr, part.data() + headerEnd).starts_with("\r\n")));
        unsigned major = 0, minor = 0, status = 0;
        RemoteCheck(sscanf_s(part.c_str() + headerEnd + 4, "HTTP/%u.%u %u", &major, &minor, &status) == 3);
        if (status != 202 && status != 204 && status != 404)
        {
            auto identity = target; identity.prefix = objects[index].key;
            const auto content = part.find("\r\n\r\n", headerEnd + 4);
            const auto detail = content == part.npos ? L"" : RemoteErrorBody(part.substr(content + 4));
            if (errors.size() < 4096) errors += identity.ToString() + std::format(L": HTTP {} {}\n", status, detail);
        }
        else progress->Increment();
        position = next + 2;
    }
    RemoteCheck(seen.size() == objects.size());
    if (!errors.empty()) throw RemoteFailure{ errors };
    return true;
}

RemoteDeleteResult FinderRemote::Delete(const std::span<CItem* const> items,
    CProgressDlg* progress, const bool emptyOnly)
{
    struct Group
    {
        const CItem* root;
        RemoteTarget target;
        std::shared_ptr<StorageSource> source;
        std::unique_ptr<RemoteDeletePlan> plan;
        std::vector<CItem*> selected;
    };
    std::vector<Group> groups;
    std::vector<CItem*> refresh;
    std::wstring failingPath;
    try
    {
        // Validate selections and retain each root's credential scope before discovering remote members.
        const std::unordered_set<const CItem*> selection(items.begin(), items.end());
        for (auto* item : items)
        {
            failingPath = item->GetPath();
            const auto target = StorageSource::Parse(failingPath);
            const auto root = StorageSource::Parse(item->GetEnumRoot()->GetPath());
            const bool directory = item->IsTypeOrFlag(IT_DIRECTORY);
            RemoteCheck(item->IsTypeOrFlag(ITF_REMOTE) && !item->IsScanRoot() && target && root &&
                target->SameConnection(*root) && !target->prefix.empty() && target->prefix.starts_with(root->prefix) &&
                (!emptyOnly || directory), ERROR_NOT_SUPPORTED);

            // A selected ancestor already covers every key beneath it.
            const CItem* parent = item->GetParent();
            while (parent != nullptr && !selection.contains(parent)) parent = parent->GetParent();
            if (parent != nullptr) continue;
            auto group = std::ranges::find(groups, item->GetEnumRoot(), &Group::root);
            if (group == groups.end())
            {
                groups.push_back({ item->GetEnumRoot(), *root, item->GetStorageSource(),
                    std::make_unique<RemoteDeletePlan>(), {} });
                group = groups.end() - 1;
            }
            RemoteCheck(group->source != nullptr, ERROR_INVALID_NAME);
            group->selected.push_back(item);
        }

        // Complete every selected prefix's discovery before issuing any destructive request.
        for (auto& group : groups)
        {
            for (const auto* item : group.selected)
            {
                failingPath = item->GetPath();
                FinderRemote finder(group.source, nullptr);
                finder.m_target = *StorageSource::Parse(failingPath);
                finder.m_progress = progress;
                const bool directory = item->IsTypeOrFlag(IT_DIRECTORY);
                const auto& target = finder.m_target;
                if (target.protocol == RemoteProtocol::WebDav)
                {
                    // Collection deletion includes descendants; emptying uses current immediate members.
                    if (emptyOnly)
                    {
                        finder.LoadPage();
                        for (const auto& object : finder.m_entries)
                            group.plan->Add(object.key + (object.directory ? L"/" : L""), object.directory);
                    }
                    else group.plan->Add(target.prefix, directory);
                }
                else if (directory)
                {
                    do
                    {
                        finder.LoadPage(true);
                        for (const auto& object : finder.m_entries)
                        {
                            if (emptyOnly && (object.key == target.prefix ||
                                object.directory && object.key + L"/" == target.prefix)) continue;
                            group.plan->Add(object.key, object.directory);
                        }
                    } while (finder.m_more);
                }
                else group.plan->Add(target.prefix, false);
            }
            group.plan->Finish();
        }

        // Reconcile completed plans, including stale selections whose remote contents are already absent.
        for (auto& group : groups)
        {
            if (progress->IsCancelled()) throw RemoteInterrupted{};
            refresh.append_range(group.selected);
            failingPath = group.selected.front()->GetPath();

            // Each discovered identity is sent once; never retry an ambiguous destructive response.
            for (auto pending = group.plan->Next(); pending;)
            {
                if (progress->IsCancelled()) throw RemoteInterrupted{};
                std::vector<RemoteDeleteObject> batch{ *pending };
                const auto limit = group.target.protocol == RemoteProtocol::S3 ? 1000u : 256u;
                auto next = group.plan->Next();
                if (group.source->capabilities.batchDelete && !pending->directory)
                    while (next && !next->directory && batch.size() < limit)
                    {
                        batch.push_back(std::move(*next));
                        next = group.plan->Next();
                    }
                const bool tryBatch = batch.size() > 1 && group.source->capabilities.batchDelete;
                const bool batched = tryBatch && (group.target.protocol == RemoteProtocol::S3 ?
                    RemoteS3DeleteBatch(*group.source, group.target, batch, progress) :
                    group.target.protocol == RemoteProtocol::Azure &&
                    RemoteAzureDeleteBatch(*group.source, group.target, batch, progress));
                if (batched) { pending = std::move(next); continue; }
                for (const auto& entry : batch)
                {
                    if (progress->IsCancelled()) throw RemoteInterrupted{};
                    const auto& object = entry;
                    auto identity = group.target; identity.prefix = object.key;
                    failingPath = identity.ToString();
                    if (group.target.protocol == RemoteProtocol::S3)
                    {
                        const auto response = RemoteS3Request(*group.source, group.target, L"DELETE",
                            L"/" + group.target.bucket + L"/" + RemoteEncodePath(object.key), {}, nullptr, progress);
                        if (response.status != 404) RemoteStatus(response, 204);
                    }
                    else if (group.target.protocol == RemoteProtocol::Azure)
                    {
                        const auto response = RemoteAzureRequest(*group.source, group.target, L"DELETE",
                            object.key, {}, nullptr, progress);
                        if (response.status != 404) RemoteStatus(response, 202);
                    }
                    else
                    {
                        const auto& credentials = group.source->credentials;
                        RemoteCheck(!credentials.user.empty() && !credentials.secret.empty(), ERROR_LOGON_FAILURE);
                        const auto response = RemoteRequest(*group.source, group.target.endpoint,
                            L"/" + RemoteEncodePath(object.key), L"DELETE", {}, {}, nullptr, credentials, progress);
                        if (response.status == 207)
                        {
                            // A WebDAV multistatus can report partial deletion failures within a collection.
                            const auto document = RemoteReadXml(response.body, nullptr, progress);
                            RemoteCheck(document.name == L"multistatus" && document.space == L"DAV:");
                            for (const auto& member : document.children)
                            {
                                if (member.name != L"response" || member.space != L"DAV:") continue;
                                const DWORD status = RemoteDavStatus(member.Child(L"status", L"DAV:"));
                                if (status < 200 || status >= 300) RemoteStatus(status, 204);
                            }
                        }
                        else if (response.status != 200 && response.status != 404) RemoteStatus(response, 204);
                    }
                    progress->Increment();
                }
                pending = std::move(next);
            }
        }
        return { {}, std::move(refresh) };
    }
    catch (const RemoteInterrupted&) { return { {}, std::move(refresh) }; }
    catch (const RemoteFailure& failure) { return { failingPath + L"\n" + failure.message, std::move(refresh) }; }
}
