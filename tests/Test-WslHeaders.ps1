$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$temporary = Join-Path $repo 'Build\Temp'
$cache = Join-Path $temporary ('WslHeaders-Test-' + [guid]::NewGuid().ToString('N'))
$script = Join-Path $repo 'windirstat\Build\Download Linux Headers.ps1'
$stamp = Join-Path $cache '.complete'
try {
    & $script -Path $cache

    # A complete cache preserves timestamps and does not download packages again.
    $timestamp = (Get-Item -LiteralPath $stamp).LastWriteTimeUtc
    $output = & $script -Path $cache 6>&1
    if ($output -or (Get-Item -LiteralPath $stamp).LastWriteTimeUtc -ne $timestamp) {
        throw 'A complete cache was downloaded again.'
    }

    # An interrupted download has no completion stamp and is replaced on the next run.
    Remove-Item -LiteralPath $stamp -Force
    $missing = Join-Path $cache 'x86_64\cpp\bits\range_access.h'
    Remove-Item -LiteralPath $missing -Force
    $stale = Join-Path $cache 'x86_64\cpp\stale.h'
    [IO.File]::WriteAllText($stale, '')
    & $script -Path $cache
    if (-not (Test-Path -LiteralPath $stamp) -or -not (Test-Path -LiteralPath $missing) -or
        (Test-Path -LiteralPath $stale)) {
        throw 'The incomplete cache was not replaced.'
    }
    Write-Host 'PASS: header cache reuse, interrupted-download recovery, and stale-file cleanup.'
} finally {
    $resolved = [IO.Path]::GetFullPath($cache)
    if (-not $resolved.StartsWith($temporary + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The test cache is outside the temporary directory.'
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
