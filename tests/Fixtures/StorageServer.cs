using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Security;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Linq;

public sealed class WdsStorageFixture
{
    private const string Account = "windirstat";
    private const string Access = "windirstat-test";
    private const string Secret = "windirstat-test-secret";
    private const string Password = "windirstat-test-password";
    private const string Modified = "2025-01-01T00:00:00.000Z";
    private static readonly byte[] AzureKey = Encoding.UTF8.GetBytes("WinDirStat fixture key only" +
        "WinDirStat fixture key only");
    private readonly SortedDictionary<string, int> manifest = new(StringComparer.Ordinal);
    private readonly SortedDictionary<string, int> azureManifest;
    private readonly SortedDictionary<string, int> deletionManifest = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, byte> retried = new(StringComparer.Ordinal);
    private readonly bool s3;
    private volatile bool slow;
    private volatile string deleteMode = "";
    private volatile bool branchDenied = true;
    private string azureEndpoint, davEndpoint;
    private long signed, rejected, downloads, retries, delayed, deletions, deleted, deleteDelayed;
    private long listings, flatListings, batches, storageConnections, activeDav, maxActiveDav;

    private sealed record Request(string Method, string Path, string RawQuery,
        Dictionary<string, string> Headers, Dictionary<string, string> Query, byte[] Body);

    private sealed record Response(int Status, string Body, string Type = "application/xml");

    private WdsStorageFixture(bool s3)
    {
        // Keep exact object identities, including case distinctions and keys containing path syntax.
        this.s3 = s3;
        slow = s3;
        for (int i = 0; i < 1005; i++)
            manifest.Add(s3 ? $"page-{i:0000}.bin" : $"bulk/file-{i:0000}.bin", s3 ? i % 17 + 1 : i % 31);
        foreach (var entry in s3 ? new Dictionary<string, int>
        {
            ["Case.txt"] = 3, ["case.txt"] = 5, ["same"] = 7, ["same/"] = 11, ["same/child.dat"] = 13,
            ["unicode/日本語-é-ф.txt"] = 19, ["odd/a\\b|c\"d&%? #.txt"] = 23, ["odd/%2F+plus.txt"] = 29,
            ["dots/./file"] = 31, ["dots/../file"] = 37, ["empty//file"] = 41, ["newline/\nfile"] = 43,
            ["trailing/ space "] = 47, ["prefix-onlySuffix.dat"] = 53, ["/leading"] = 59
        } : new Dictionary<string, int>
        {
            ["unicode/café 雪.txt"] = 123, ["unicode/%23 &+.txt"] = 45, ["Case.txt"] = 7,
            ["case.txt"] = 9, ["empty.bin"] = 0, ["folder/item.txt"] = 17
        }) manifest.Add(entry.Key, entry.Value);
        azureManifest = new(manifest, StringComparer.Ordinal);
        if (!s3)
        {
            azureManifest.Add("folder/", 11);
            azureManifest.Add("prefix-only.bin", 31);
            azureManifest.Add("prefix-only/sub.txt", 13);
        }
    }

    public static void Run(string protocol, string readyFile, string azure, string dav)
    {
        // Bind only to loopback without requiring an HTTP namespace reservation or elevation.
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("Windows is required.");
        var fixture = new WdsStorageFixture(protocol == "S3");
        if (!fixture.s3) fixture.Seed(azure, dav).GetAwaiter().GetResult();
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        try
        {
            string endpoint = $"http://127.0.0.1:{((IPEndPoint)listener.LocalEndpoint).Port}";
            File.WriteAllText(readyFile + ".tmp", JsonSerializer.Serialize(new { endpoint }));
            File.Move(readyFile + ".tmp", readyFile);
            while (true)
            {
                var client = listener.AcceptTcpClient();
                _ = Task.Run(() => fixture.Serve(client));
            }
        }
        finally { listener.Stop(); }
    }

    private async Task Serve(TcpClient client)
    {
        // Handle delayed responses independently so pause, resume, and control requests remain responsive.
        using (client)
        using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30)))
        {
            try
            {
                using var stream = client.GetStream();
                using var reader = new StreamReader(stream, Encoding.Latin1, false, 4096, true);
                bool storageConnection = false;
                while (true)
                {
                    string first = await reader.ReadLineAsync(timeout.Token);
                    if (first == null) return;
                    var line = first.Split(' ', 3);
                    if (line.Length != 3 || first.Length > 32768) return;
                    var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    int bytes = first.Length;
                    while (!string.IsNullOrEmpty(first = await reader.ReadLineAsync(timeout.Token)))
                    {
                        bytes += first.Length;
                        if (bytes > 65536 || !first.Contains(':')) return;
                        var field = first.Split(':', 2);
                        headers[field[0]] = field[1].Trim();
                    }
                    int length = headers.TryGetValue("Content-Length", out var value) ? int.Parse(value) : 0;
                    if (length < 0 || length > 4 * 1024 * 1024 || headers.ContainsKey("Transfer-Encoding")) return;
                    var body = new char[length];
                    if (length > 0 && await reader.ReadBlockAsync(body.AsMemory(), timeout.Token) != length) return;
                    int question = line[1].IndexOf('?');
                    string path = question < 0 ? line[1] : line[1][..question];
                    string query = question < 0 ? "" : line[1][(question + 1)..];
                    if (!path.StartsWith("/__", StringComparison.Ordinal) && !storageConnection)
                    {
                        storageConnection = true;
                        Interlocked.Increment(ref storageConnections);
                    }
                    var response = await Respond(new(line[0], path, query, headers, ParseQuery(query),
                        Encoding.Latin1.GetBytes(body)));
                    byte[] payload = Encoding.UTF8.GetBytes(response.Body);
                    byte[] head = Encoding.ASCII.GetBytes($"HTTP/1.1 {response.Status} Fixture\r\n" +
                        $"Content-Type: {response.Type}; charset=utf-8\r\nContent-Length: {payload.Length}\r\n" +
                        "Connection: keep-alive\r\n\r\n");
                    await stream.WriteAsync(head, timeout.Token);
                    await stream.WriteAsync(payload, timeout.Token);
                }
            }
            catch (Exception error) when (error is IOException or SocketException or OperationCanceledException)
            {
                // A cancelled scan closes its connection before the delayed fixture response is sent.
            }
        }
    }

    private static Dictionary<string, string> ParseQuery(string query)
    {
        var result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (string part in query.Split('&', StringSplitOptions.RemoveEmptyEntries))
        {
            var field = part.Split('=', 2);
            result[WebUtility.UrlDecode(field[0])] = field.Length == 2 ? WebUtility.UrlDecode(field[1]) : "";
        }
        return result;
    }

    private async Task<Response> Respond(Request request)
    {
        // Expose fixture controls separately from protocol requests and retain observable request counters.
        var query = request.Query;
        if (request.Method == "GET")
        {
            if (request.Path == "/__manifest") return Json(s3 ? (object)manifest :
                new Dictionary<string, object> { ["azure"] = azureManifest, ["webdav"] = manifest });
            if (request.Path == "/__settings")
                return Json(new { key = Convert.ToBase64String(AzureKey),
                    sas = AccountSas(), deleteSas = AccountSas(true) });
            if (request.Path == "/__stats") return Json(new
            {
                signed = Interlocked.Read(ref signed), rejected = Interlocked.Read(ref rejected),
                downloads = Interlocked.Read(ref downloads), retries = Interlocked.Read(ref retries),
                slow = Interlocked.Read(ref delayed), deletions = Interlocked.Read(ref deletions),
                deleted = Interlocked.Read(ref deleted), deleteSlow = Interlocked.Read(ref deleteDelayed),
                listings = Interlocked.Read(ref listings), flatListings = Interlocked.Read(ref flatListings),
                batches = Interlocked.Read(ref batches), storageConnections = Interlocked.Read(ref storageConnections),
                maxActiveDav = Interlocked.Read(ref maxActiveDav)
            });
            if (request.Path == "/__deletions")
            {
                lock (deletionManifest) return Json(deletionManifest);
            }
            if (request.Path == "/__control")
            {
                if (query.ContainsKey("slow")) slow = query["slow"] == "1";
                if (query.ContainsKey("delete")) deleteMode = query["delete"];
                if (query.ContainsKey("branch")) branchDenied = query["branch"] != "healthy";
                if (query.ContainsKey("parallel")) Interlocked.Exchange(ref maxActiveDav, 0);
                if (query.GetValueOrDefault("seed") is "delete" or "late")
                    await SeedDeletion(query["seed"] == "late");
                return Json(new { });
            }
        }
        bool dav = request.Method == "PROPFIND";
        if (s3)
        {
            bool valid = SignatureValid(request) && (!(request.Path == "/windirstat-token" ||
                request.Path.StartsWith("/windirstat-delete", StringComparison.Ordinal)) ||
                request.Headers.GetValueOrDefault("x-amz-security-token") == "temporary-test-token");
            if (valid) Interlocked.Increment(ref signed);
            else Interlocked.Increment(ref rejected);
            if (!valid) return new(403, "<Error><Code>SignatureDoesNotMatch</Code></Error>");
        }
        if (s3 && request.Method == "POST" && request.Query.ContainsKey("delete"))
        {
            Interlocked.Increment(ref batches);
            Interlocked.Increment(ref deletions);
            if (deleteMode == "unsupported") return new(501, "<Error><Code>NotImplemented</Code></Error>");
            if (deleteMode == "denied") return new(403, "<Error><Code>AccessDenied</Code></Error>");
            if (deleteMode == "slow")
            {
                Interlocked.Increment(ref deleteDelayed);
                await Task.Delay(15000);
                return new(503, "");
            }
            string checksum = Convert.ToBase64String(MD5.HashData(request.Body));
            if (request.Headers.GetValueOrDefault("Content-MD5") != checksum) return new(400, "Bad digest");
            using var input = new MemoryStream(request.Body);
            using var xmlReader = XmlReader.Create(input, new XmlReaderSettings { DtdProcessing = DtdProcessing.Prohibit });
            var document = XDocument.Load(xmlReader);
            var keys = document.Root.Elements().Where(element => element.Name.LocalName == "Object")
                .Select(element => element.Elements().Single(child => child.Name.LocalName == "Key").Value).ToArray();
            if (keys.Length == 0 || keys.Length > 1000 || keys.Distinct(StringComparer.Ordinal).Count() != keys.Length)
                return new(400, "Invalid batch");
            var result = new StringBuilder("<DeleteResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">");
            for (int i = 0; i < keys.Length; ++i)
            {
                if (deleteMode == "batch-partial" && i == 0)
                {
                    result.Append("<Error><Key>" + Xml(keys[i]) + "</Key><Code>AccessDenied</Code></Error>");
                    continue;
                }
                lock (deletionManifest) deletionManifest.Remove(keys[i]);
                Interlocked.Increment(ref deleted);
                if (deleteMode == "batch-incomplete" && i == keys.Length - 1) continue;
                result.Append("<Deleted><Key>" + Xml(keys[i]).Replace("\r", "&#13;") + "</Key></Deleted>");
            }
            return new(200, result.Append("</DeleteResult>").ToString());
        }
        if (request.Method == "DELETE")
        {
            Interlocked.Increment(ref deletions);
            if (!s3) return new(207, "<d:multistatus xmlns:d=\"DAV:\"><d:response>" +
                "<d:href>/delete-partial/locked/file.txt</d:href><d:status>HTTP/1.1 423 Locked</d:status>" +
                "</d:response></d:multistatus>");
            if (!request.Path.StartsWith("/windirstat-delete/", StringComparison.Ordinal)) return new(403, "");
            if (deleteMode == "denied") return new(403, "");
            if (deleteMode == "slow")
            {
                Interlocked.Increment(ref deleteDelayed);
                await Task.Delay(15000);
                return new(503, "");
            }
            string key = Uri.UnescapeDataString(request.Path["/windirstat-delete/".Length..]);
            lock (deletionManifest) deletionManifest.Remove(key);
            Interlocked.Increment(ref deleted);
            return new(204, "");
        }
        if (s3 ? request.Method != "GET" || query.GetValueOrDefault("list-type") != "2" :
            !dav && (request.Method != "GET" || query.GetValueOrDefault("comp") != "list"))
        {
            Interlocked.Increment(ref downloads);
            return new(405, "");
        }
        if (dav && request.Headers.GetValueOrDefault("Depth") != "1") return new(400, "");
        if (s3 && request.Path == "/windirstat-delete" && deleteMode == "list-failure" &&
            !query.ContainsKey("delimiter") && query.ContainsKey("continuation-token")) return new(403, "");
        Interlocked.Increment(ref listings);
        if (!query.ContainsKey("delimiter")) Interlocked.Increment(ref flatListings);
        string mode = dav ? request.Path.Trim('/') : request.Path.Trim('/').Split('/')[^1];
        if (s3) mode = mode.Replace("windirstat-", "", StringComparison.Ordinal);
        if (dav && mode.StartsWith("parallel", StringComparison.Ordinal))
        {
            long current = Interlocked.Increment(ref activeDav), maximum = Interlocked.Read(ref maxActiveDav);
            while (current > maximum)
            {
                long previous = Interlocked.CompareExchange(ref maxActiveDav, current, maximum);
                if (previous == maximum) break;
                maximum = previous;
            }
            try
            {
                await Task.Delay(200);
                string prefix = "/" + mode + "/";
                string Collection(string key) => "<d:response><d:href>" + key +
                    "</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype>" +
                    "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
                var xml = new StringBuilder("<d:multistatus xmlns:d=\"DAV:\">" + Collection(prefix));
                if (mode == "parallel") for (int i = 0; i < 12; ++i) xml.Append(Collection(prefix + $"dir-{i}/"));
                else xml.Append("<d:response><d:href>" + prefix + "file.txt</d:href><d:propstat><d:prop>" +
                    "<d:resourcetype/><d:getcontentlength>1</d:getcontentlength></d:prop>" +
                    "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>");
                return new(207, xml.Append("</d:multistatus>").ToString());
            }
            finally { Interlocked.Decrement(ref activeDav); }
        }
        if (mode == "branch" && branchDenied && query.GetValueOrDefault("prefix") == "z-denied/")
            return new(403, "<Error xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><Code>AccessDenied</Code>" +
                "<Message>Fixture branch denied</Message></Error>");
        if (mode == "denied" || mode == "partial" &&
            query.ContainsKey(s3 ? "continuation-token" : "marker")) return new(403, "");
        if (mode == "malformed") return new(dav ? 207 : 200, "<broken>");
        if (mode == "entities") return new(dav ? 207 : 200,
            "<!DOCTYPE x [<!ENTITY y SYSTEM \"file:///C:/Windows/win.ini\">]><x>&y;</x>");
        string retryKey = s3 ? request.Path + "?" + request.RawQuery : mode + "|" + dav;
        if (mode == "retry" && retried.TryAdd(retryKey, 0))
        {
            Interlocked.Increment(ref retries);
            return new(503, "<Error><Code>SlowDown</Code></Error>");
        }
        if (mode == "slow" && slow)
        {
            Interlocked.Increment(ref delayed);
            await Task.Delay(s3 ? 15000 : 10000);
        }
        return s3 ? S3Listing(request, mode) : dav ? DavListing(mode) : AzureListing(mode);
    }

    private static Response Json(object value) => new(200, JsonSerializer.Serialize(value), "application/json");
    private static string Xml(string value) => SecurityElement.Escape(value);
    private static byte[] Hmac(byte[] key, string value) => HMACSHA256.HashData(key, Encoding.UTF8.GetBytes(value));
    private static string Hex(byte[] value) => Convert.ToHexString(value).ToLowerInvariant();

    private static bool SignatureValid(Request request)
    {
        // Verify the transmitted canonical request with .NET cryptography independently of the application signer.
        try
        {
            var authorization = request.Headers["Authorization"].Split(' ', 2);
            var fields = authorization[1].Split(',').Select(part => part.Trim().Split('=', 2))
                .ToDictionary(part => part[0], part => part[1], StringComparer.Ordinal);
            var credential = fields["Credential"].Split('/');
            if (authorization[0] != "AWS4-HMAC-SHA256" || credential.Length != 5 || credential[0] != Access ||
                credential[3] != "s3" || credential[4] != "aws4_request") return false;
            string signedHeaders = fields["SignedHeaders"];
            string headers = string.Concat(signedHeaders.Split(';').Select(name => name + ":" +
                Regex.Replace(request.Headers[name].Trim(), @"\s+", " ") + "\n"));
            string canonical = string.Join("\n", request.Method, request.Path, request.RawQuery, headers,
                signedHeaders, Hex(SHA256.HashData(request.Body)));
            string scope = string.Join("/", credential.Skip(1));
            string toSign = string.Join("\n", authorization[0], request.Headers["x-amz-date"], scope,
                Hex(SHA256.HashData(Encoding.UTF8.GetBytes(canonical))));
            byte[] key = Encoding.UTF8.GetBytes("AWS4" + Secret);
            foreach (string part in credential.Skip(1)) key = Hmac(key, part);
            return CryptographicOperations.FixedTimeEquals(Hmac(key, toSign),
                Convert.FromHexString(fields["Signature"]));
        }
        catch (Exception error) when (error is ArgumentException or KeyNotFoundException or
            IndexOutOfRangeException or FormatException) { return false; }
    }

    private Response S3Listing(Request request, string mode)
    {
        // Group exact prefixes before paging so directory markers and opaque continuation tokens retain S3 semantics.
        if (mode == "missing-metadata") return new(200, "<ListBucketResult><EncodingType>url</EncodingType>" +
            "<IsTruncated>false</IsTruncated><Contents><Key>missing</Key></Contents></ListBucketResult>");
        if (mode == "loop") return new(200, "<ListBucketResult><EncodingType>url</EncodingType>" +
            "<IsTruncated>true</IsTruncated><NextContinuationToken>repeated</NextContinuationToken>" +
            "</ListBucketResult>");
        string prefix = request.Query.GetValueOrDefault("prefix", "");
        string delimiter = request.Query.GetValueOrDefault("delimiter", "");
        KeyValuePair<string, int>[] objects;
        lock (deletionManifest) objects = (mode == "delete" ? deletionManifest : manifest).ToArray();
        if (mode == "wide") objects = Enumerable.Range(0, 1000)
            .Select(i => new KeyValuePair<string, int>($"dir-{i:0000}/file.txt", 1)).ToArray();
        if (mode == "branch") objects = new[]
        {
            new KeyValuePair<string, int>("a-good/file.txt", 3),
            new KeyValuePair<string, int>("z-denied/file.txt", 5)
        };
        var sizes = objects.ToDictionary(entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        var entries = new SortedDictionary<string, bool>(StringComparer.Ordinal);
        foreach (var entry in objects)
        {
            if (!entry.Key.StartsWith(prefix, StringComparison.Ordinal)) continue;
            int slash = delimiter.Length == 0 ? -1 : entry.Key.IndexOf(delimiter, prefix.Length,
                StringComparison.Ordinal);
            if (slash < 0) entries[entry.Key] = false;
            else entries[entry.Key[..(slash + delimiter.Length)]] = true;
        }
        string token = request.Query.GetValueOrDefault("continuation-token", "");
        int offset = token.Length == 0 ? 0 : int.Parse(Encoding.UTF8.GetString(Convert.FromBase64String(token)));
        int limit = Math.Clamp(int.Parse(request.Query.GetValueOrDefault("max-keys", "1000")), 1, 1000);
        var page = entries.Skip(offset).Take(limit).ToArray();
        bool more = offset + page.Length < entries.Count;
        var xml = new StringBuilder("<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" +
            "<EncodingType>url</EncodingType><IsTruncated>" + (more ? "true" : "false") + "</IsTruncated>");
        foreach (var entry in page)
        {
            string key = Xml(Uri.EscapeDataString(entry.Key));
            if (entry.Value) xml.Append("<CommonPrefixes><Prefix>" + key + "</Prefix></CommonPrefixes>");
            else xml.Append($"<Contents><Key>{key}</Key><Size>{sizes[entry.Key]}</Size>" +
                $"<LastModified>{Modified}</LastModified></Contents>");
        }
        if (more)
        {
            string next = (offset + page.Length).ToString(CultureInfo.InvariantCulture);
            xml.Append("<NextContinuationToken>" + Convert.ToBase64String(Encoding.UTF8.GetBytes(next)) +
                "</NextContinuationToken>");
        }
        return new(200, xml.Append("</ListBucketResult>").ToString());
    }

    private static Response AzureListing(string mode)
    {
        string marker = mode is "loop" or "partial" ? "again" : "";
        string size = mode == "missing-metadata" ? "" : "<Content-Length>23</Content-Length>";
        string name = mode == "encoded" ? "<Name Encoded=\"true\">special%EF%BF%BE%252F.txt</Name>" :
            "<Name>file.txt</Name>";
        return new(200, "<EnumerationResults><Blobs><Blob>" + name + "<Properties>" + size +
            "<Last-Modified>Wed, 01 Jan 2025 00:00:00 GMT</Last-Modified></Properties></Blob></Blobs>" +
            "<NextMarker>" + marker + "</NextMarker></EnumerationResults>");
    }

    private static Response DavListing(string mode)
    {
        // Exercise self-collection metadata, unrepresentable names, and collection work queued before an error.
        string prefix = "/" + mode + "/";
        string self = "<d:response><d:href>" + prefix + "</d:href><d:propstat><d:prop>" +
            "<d:resourcetype><d:collection/></d:resourcetype></d:prop>" +
            "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
        if (mode == "delete-partial") self += "<d:response><d:href>" + prefix +
            "locked/</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype>" +
            "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
        if (mode == "empty-date")
        {
            self = self.Replace("</d:prop>",
                "<d:getlastmodified>Wed, 01 Jan 2025 00:00:00 GMT</d:getlastmodified></d:prop>");
            return new(207, "<d:multistatus xmlns:d=\"DAV:\">" + self + "</d:multistatus>");
        }
        string href = prefix + "file.txt";
        if (mode is "oversize" or "oversize-queued") href = prefix + new string('a', 65536);
        if (mode == "oversize-escaped") href = prefix + string.Concat(Enumerable.Repeat("%25", 22000));
        if (mode == "oversize-queued") self += "<d:response><d:href>" + prefix +
            "dir/</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop>" +
            "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
        if (mode == "foreign") href = "https://unrelated.invalid/file.txt";
        if (mode == "escape") href = prefix + "../file.txt";
        string size = mode == "missing-metadata" ? "" : "<d:getcontentlength>23</d:getcontentlength>";
        return new(207, "<d:multistatus xmlns:d=\"DAV:\">" + self + "<d:response><d:href>" + Xml(href) +
            "</d:href><d:propstat><d:prop><d:resourcetype/>" + size + "</d:prop>" +
            "<d:status>HTTP/1.1 200 OK</d:status></d:propstat><d:propstat><d:prop><d:getlastmodified/></d:prop>" +
            "<d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response></d:multistatus>");
    }

    private static string AccountSas(bool allowDelete = false)
    {
        string expiry = "2099-01-01T00:00:00Z";
        string rights = allowDelete ? "dl" : "l";
        string[] fields = { Account, rights, "b", "co", "", expiry, "", "", "2021-12-02", "", "" };
        return "sv=2021-12-02&ss=b&srt=co&sp=" + rights + "&se=" + Uri.EscapeDataString(expiry) + "&sig=" +
            Uri.EscapeDataString(Convert.ToBase64String(Hmac(AzureKey, string.Join("\n", fields))));
    }

    private static async Task Put(HttpClient client, string endpoint, string resource, byte[] data,
        bool azure, string method = "PUT", bool existing = false)
    {
        // Authenticate fixture uploads using the same storage permissions exercised by enumeration.
        using var request = new HttpRequestMessage(new HttpMethod(method), endpoint + resource);
        request.Content = new ByteArrayContent(data);
        if (azure)
        {
            var headers = new SortedDictionary<string, string>(StringComparer.Ordinal)
            {
                ["x-ms-date"] = DateTime.UtcNow.ToString("R", CultureInfo.InvariantCulture),
                ["x-ms-version"] = "2021-12-02"
            };
            if (!resource.Contains('?')) headers.Add("x-ms-blob-type", "BlockBlob");
            foreach (var header in headers) request.Headers.Add(header.Key, header.Value);
            string path = request.RequestUri.AbsolutePath;
            string canonical = "/" + Account + path;
            foreach (var parameter in ParseQuery(request.RequestUri.Query.TrimStart('?')).OrderBy(
                entry => entry.Key, StringComparer.Ordinal)) canonical += "\n" + parameter.Key + ":" + parameter.Value;
            string[] fields = { method, "", "", data.Length == 0 ? "" : data.Length.ToString(
                CultureInfo.InvariantCulture), "", "", "", "", "", "", "", "" };
            string toSign = string.Join("\n", fields) + "\n" +
                string.Concat(headers.Select(header => header.Key + ":" + header.Value + "\n")) + canonical;
            request.Headers.TryAddWithoutValidation("Authorization", "SharedKey " + Account + ":" +
                Convert.ToBase64String(Hmac(AzureKey, toSign)));
        }
        else request.Headers.TryAddWithoutValidation("Authorization", "Basic " +
            Convert.ToBase64String(Encoding.UTF8.GetBytes("fixture:" + Password)));
        using var response = await client.SendAsync(request);
        if (existing && (int)response.StatusCode is 405 or 409) return;
        response.EnsureSuccessStatusCode();
    }

    private async Task Seed(string azure, string dav)
    {
        // Populate the actual storage servers through authenticated HTTP requests, keeping fixture endpoints private.
        azureEndpoint = azure;
        davEndpoint = new Uri(new Uri(dav), "../delete/").AbsoluteUri;
        using var handler = new HttpClientHandler { UseProxy = false, AllowAutoRedirect = false };
        using var client = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(30) };
        for (int attempt = 0; ; attempt++)
        {
            try
            {
                await Put(client, azure, "/objects?restype=container", Array.Empty<byte>(), true, existing: true);
                await Put(client, dav, "", Array.Empty<byte>(), false, "MKCOL", true);
                break;
            }
            catch (Exception error) when (attempt < 20 && error is HttpRequestException or TaskCanceledException)
            {
                await Task.Delay(200);
            }
        }
        foreach (string folder in new[] { "bulk", "unicode", "folder", "empty-folder" })
            await Put(client, dav, folder + "/", Array.Empty<byte>(), false, "MKCOL");
        await Parallel.ForEachAsync(azureManifest, new ParallelOptions { MaxDegreeOfParallelism = 16 },
        async (entry, cancellation) =>
        {
            string name = string.Join("/", entry.Key.Split('/').Select(Uri.EscapeDataString));
            byte[] data = Enumerable.Repeat((byte)'x', entry.Value).ToArray();
            await Put(client, azure, "/objects/" + name, data, true);
            if (manifest.ContainsKey(entry.Key)) await Put(client, dav, name, data, false);
        });
    }

    private async Task SeedDeletion(bool late)
    {
        // Keep mutable deletion data separate from the enumeration fixtures and seed real services through HTTP.
        var objects = new SortedDictionary<string, int>(StringComparer.Ordinal)
        {
            ["Case.txt"] = 3, ["case.txt"] = 5, ["same"] = 7, ["same/child.txt"] = 13,
            ["unicode/café 雪.txt"] = 19, ["odd/%23 &+.txt"] = 29,
            ["folder/visible.txt"] = 31, ["folder/filtered.bin"] = 37, ["folder/nested/new.txt"] = 41,
            ["folderSibling.txt"] = 43, ["empty/member.txt"] = 47, ["keep.txt"] = 53
        };
        if (late) objects = new(StringComparer.Ordinal) { ["folder/after-scan.txt"] = 61 };
        else for (int i = 0; i < 1005; i++) objects.Add($"bulk/item-{i:0000}.bin", i % 17);
        if (s3)
        {
            if (!late)
            {
                objects.Add("same/", 11);
                objects.Add("empty/", 17);
                objects.Add("invalid/control\u0001.bin", 1);
                objects.Add("invalid/normal.bin", 2);
                objects.Add("xml/line\r\n.txt", 3);
                objects.Add("xml/% literal&.txt", 4);
                objects.Add("dots/../file", 23);
                objects.Add("dots/./file", 31);
                objects.Add("empty//file", 37);
                objects.Add("odd/a\\b|c\"d&%? #.txt", 59);
            }
            lock (deletionManifest)
            {
                if (!late) deletionManifest.Clear();
                foreach (var entry in objects) deletionManifest.Add(entry.Key, entry.Value);
            }
            return;
        }
        using var handler = new HttpClientHandler { UseProxy = false, AllowAutoRedirect = false };
        using var client = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(30) };
        await Put(client, azureEndpoint, "/deleteobjects?restype=container", Array.Empty<byte>(), true, existing: true);
        if (!late)
        {
            foreach (string folder in new[] { "bulk", "unicode", "odd", "same", "folder", "folder/nested", "empty" })
                await Put(client, davEndpoint, folder + "/", Array.Empty<byte>(), false, "MKCOL", true);
        }
        await Parallel.ForEachAsync(objects, new ParallelOptions { MaxDegreeOfParallelism = 16 },
        async (entry, cancellation) =>
        {
            string name = string.Join("/", entry.Key.Split('/').Select(Uri.EscapeDataString));
            byte[] data = Enumerable.Repeat((byte)'x', entry.Value).ToArray();
            await Put(client, azureEndpoint, "/deleteobjects/" + name, data, true);
            if (entry.Key != "same") await Put(client, davEndpoint, name, data, false);
        });
        if (late) return;
        foreach (string marker in new[] { "same/", "empty/" })
            await Put(client, azureEndpoint, "/deleteobjects/" + marker, new byte[11], true);
        await Put(client, azureEndpoint, "/deleteobjects/unicode/" + Uri.EscapeDataString("café 雪.txt") +
            "?comp=snapshot", Array.Empty<byte>(), true);
    }
}
