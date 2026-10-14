#Requires -Version 7.6
param([string] $ExePath, [string] $Scenario = 'remote.*', [string] $OutputPath, [switch] $KeepArtifacts)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Harness/Storage.ps1')
$network = 'windirstat-remote-' + [guid]::NewGuid().ToString('N')
$created = [Collections.Generic.List[string]]::new()
$fixture = $null
$previous = @{}
$exitCode = 2
try {
    docker network create $network | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not create the isolated fixture network.' }
    $key = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('WinDirStat fixture key only' * 2))
    $azure = "$network-azure"
    $image = 'mcr.microsoft.com/azure-storage/azurite@sha256:' +
        '830430c1da1a2d537e08f3e6764dd1f5ae00cf0346bcaf625b968ec3f0971fd5'
    docker run --detach --rm --name $azure --network $network --network-alias azurite --publish 127.0.0.1::10000 `
        --env "AZURITE_ACCOUNTS=windirstat:$key" $image azurite-blob `
        --blobHost 0.0.0.0 --disableProductStyleUrl | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not start Azurite.' }
    $created.Add($azure)
    $dav = "$network-dav"
    $configuration = Join-Path $PSScriptRoot 'Fixtures/WebDav.conf'
    $startDav = 'mkdir -p /usr/local/apache2/htdocs/dav /usr/local/apache2/htdocs/delete && ' +
        'chmod 777 /usr/local/apache2/htdocs/dav /usr/local/apache2/htdocs/delete && ' +
        'htpasswd -bc /tmp/dav-passwords fixture windirstat-test-password && exec httpd-foreground'
    docker run --detach --rm --name $dav --network $network --network-alias dav --publish 127.0.0.1::80 `
        --mount "type=bind,source=$configuration,target=/usr/local/apache2/conf/httpd.conf,readonly" `
        --entrypoint sh httpd@sha256:03f858efb82c25cb0f9962946615cdcdaf26927cb971722149b563197cfd0fdd `
        -c $startDav | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not start Apache WebDAV.' }
    $created.Add($dav)
    foreach ($entry in @(
        @{ Name = 'WDS_REMOTE_AZURE_ENDPOINT'; Container = $azure; Port = '10000/tcp'; Suffix = '/windirstat' },
        @{ Name = 'WDS_REMOTE_DAV_ENDPOINT'; Container = $dav; Port = '80/tcp'; Suffix = '/dav/' }
    )) {
        $binding = docker port $entry.Container $entry.Port
        if ($LASTEXITCODE -ne 0 -or $binding -notmatch '^127\.0\.0\.1:\d+$') {
            throw 'Missing loopback fixture binding.'
        }
        $previous[$entry.Name] = [Environment]::GetEnvironmentVariable($entry.Name)
        [Environment]::SetEnvironmentVariable($entry.Name, "http://$binding" + $entry.Suffix)
    }
    $fixture = Start-StorageFixture Remote $env:WDS_REMOTE_AZURE_ENDPOINT $env:WDS_REMOTE_DAV_ENDPOINT
    $previous['WDS_REMOTE_TEST_ENDPOINT'] = $env:WDS_REMOTE_TEST_ENDPOINT
    $env:WDS_REMOTE_TEST_ENDPOINT = $fixture.Endpoint
    $arguments = @('-NoProfile', '-STA', '-File', (Join-Path $PSScriptRoot 'Test-WinDirStat.ps1'),
        '-Scenario', $Scenario, '-RequireAll')
    if ($ExePath) { $arguments += @('-ExePath', $ExePath) }
    if ($OutputPath) { $arguments += @('-OutputPath', $OutputPath) }
    if ($KeepArtifacts) { $arguments += '-KeepArtifacts' }
    & (Join-Path $PSHOME 'pwsh.exe') @arguments
    $exitCode = $LASTEXITCODE
    $davLog = docker logs $dav 2>&1 | Out-String
    $azureLog = docker logs $azure 2>&1 | Out-String
    if ($davLog -match '"GET /(?:dav|delete)/' -or
        $azureLog -match '"(?:GET|HEAD) /windirstat/(?:objects|deleteobjects)/') {
        $exitCode = 1
        throw 'Enumeration requested file content or per-object metadata from a storage server.'
    }
} catch {
    Write-Error $_ -ErrorAction Continue
    foreach ($container in $created) { docker logs --tail 12 $container }
} finally {
    foreach ($entry in $previous.GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value) }
    Stop-StorageFixture $fixture
    foreach ($container in $created) { docker stop --time 1 $container | Out-Null }
    docker network rm $network | Out-Null
}
exit $exitCode
