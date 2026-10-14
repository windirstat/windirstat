function Start-StorageFixture {
    param([string] $Protocol, [string] $AzureEndpoint, [string] $DavEndpoint)
    $directory = Join-Path ([IO.Path]::GetTempPath()) ('WinDirStatStorage-' + [guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $ready = Join-Path $directory 'ready.json'
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = Join-Path $PSHOME 'pwsh.exe'
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-NonInteractive', '-File',
        (Join-Path $PSScriptRoot '../Fixtures/StorageServer.ps1'), '-Protocol', $Protocol, '-ReadyFile', $ready)) {
        $info.ArgumentList.Add($argument)
    }
    if ($AzureEndpoint) {
        foreach ($argument in '-AzureEndpoint', $AzureEndpoint, '-DavEndpoint', $DavEndpoint) {
            $info.ArgumentList.Add($argument)
        }
    }
    $fixture = [pscustomobject]@{
        Process = $null; Directory = $directory; Output = $null; Errors = $null; Endpoint = ''
    }
    try {
        $fixture.Process = [Diagnostics.Process]::Start($info)
        $fixture.Output = $fixture.Process.StandardOutput.ReadToEndAsync()
        $fixture.Errors = $fixture.Process.StandardError.ReadToEndAsync()
        $deadline = [datetime]::UtcNow.AddSeconds(90)
        do {
            if ($fixture.Process.WaitForExit(100)) {
                throw ('The storage fixture exited during startup. ' + $fixture.Errors.GetAwaiter().GetResult())
            }
            if (Test-Path -LiteralPath $ready) {
                $fixture.Endpoint = (Get-Content -LiteralPath $ready -Raw | ConvertFrom-Json).endpoint
                if ($fixture.Endpoint -notmatch '^http://127\.0\.0\.1:\d+$') {
                    throw 'The storage fixture did not bind to loopback.'
                }
                return $fixture
            }
        } until ([datetime]::UtcNow -ge $deadline)
        throw 'The storage fixture did not become ready.'
    } catch {
        Stop-StorageFixture $fixture
        throw
    }
}

function Stop-StorageFixture {
    param([object] $Fixture)
    if (-not $Fixture) { return }
    if ($Fixture.Process) {
        if (-not $Fixture.Process.HasExited) {
            $Fixture.Process.Kill($true)
            [void]$Fixture.Process.WaitForExit(5000)
        }
        $Fixture.Process.Dispose()
    }
    foreach ($name in 'ready.json', 'ready.json.tmp') {
        Remove-Item -LiteralPath (Join-Path $Fixture.Directory $name) -Force -ErrorAction SilentlyContinue
    }
    [IO.Directory]::Delete($Fixture.Directory, $false)
}
