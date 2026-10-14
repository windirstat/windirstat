#Requires -Version 7.6
param(
    [string] $ExePath,
    [string] $Scenario = 's3.*',
    [string] $OutputPath,
    [switch] $KeepArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Harness/Storage.ps1')
$endpoint = $env:WDS_S3_TEST_ENDPOINT
$fixture = $null
$exitCode = 2
try {
    $fixture = Start-StorageFixture S3
    $env:WDS_S3_TEST_ENDPOINT = $fixture.Endpoint
    $arguments = @('-NoProfile', '-STA', '-File', (Join-Path $PSScriptRoot 'Test-WinDirStat.ps1'),
        '-Scenario', $Scenario, '-RequireAll')
    if ($ExePath) { $arguments += @('-ExePath', $ExePath) }
    if ($OutputPath) { $arguments += @('-OutputPath', $OutputPath) }
    if ($KeepArtifacts) { $arguments += '-KeepArtifacts' }
    & (Join-Path $PSHOME 'pwsh.exe') @arguments
    $exitCode = $LASTEXITCODE
} catch {
    Write-Error $_ -ErrorAction Continue
} finally {
    $env:WDS_S3_TEST_ENDPOINT = $endpoint
    Stop-StorageFixture $fixture
}
exit $exitCode
