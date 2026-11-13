#Requires -Version 7.6
param(
    [Parameter(Mandatory)] [string] $Runner,
    [string] $Distro,
    [string] $Emulator
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Runner = (Resolve-Path -LiteralPath $Runner).Path
if ($IsWindows -and -not $Distro) { throw 'Specify a WSL distribution with -Distro.' }

function Invoke-Linux {
    param([string] $Script, [string[]] $Arguments = @())
    $command = @('-c', $Script.Replace("`r", ''), 'wds-runner-test') + $Arguments
    $output = if ($IsWindows) { & wsl --distribution $Distro --user root --exec /bin/sh @command 2>&1 }
        else { & /bin/sh @command 2>&1 }
    if ($LASTEXITCODE) { throw "Linux fixture failed: $output" }
    return $output
}

function Invoke-Runner {
    param([string[]] $Arguments = @("$root/tree"), [switch] $CloseOutput)
    $info = [Diagnostics.ProcessStartInfo]@{
        FileName = if ($IsWindows) { 'wsl.exe' } else { '/bin/sh' }
        UseShellExecute = $false; RedirectStandardOutput = $true; RedirectStandardError = $true; CreateNoWindow = $true
    }
    $command = @('-c', 'exec "$@"', 'wds-runner-test')
    if ($Emulator) { $command += $Emulator }
    $command += @("$root/runner") + $Arguments
    if ($IsWindows) { $command = @('--distribution', $Distro, '--user', 'root', '--exec', '/bin/sh') + $command }
    foreach ($argument in $command) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    $bytes = [IO.MemoryStream]::new()
    try {
        $copy = if ($CloseOutput) { $process.StandardOutput.Close(); $null }
            else { $process.StandardOutput.BaseStream.CopyToAsync($bytes) }
        $errors = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) { $process.Kill($true); throw 'The runner did not exit.' }
        if ($copy) { [void]$copy.GetAwaiter().GetResult() }
        return [pscustomobject]@{ Code = $process.ExitCode; Bytes = $bytes.ToArray(); Error = $errors.Result }
    }
    finally { $bytes.Dispose(); $process.Dispose() }
}

function Assert-True {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw $Message }
}

function Read-Scan {
    param($Result)
    Assert-True ($Result.Code -eq 0) "Runner failed ($($Result.Code)): $($Result.Error)"
    $reader = [IO.BinaryReader]::new([IO.MemoryStream]::new($Result.Bytes, $false))
    $entries = [Collections.Generic.List[object]]::new()
    $paths = @{}
    $ended = $false
    try {
        $header = [Convert]::ToHexString($reader.ReadBytes(16))
        Assert-True ($header -eq '57445357534C01000100000000000000') "Invalid header: $header"
        while ($reader.BaseStream.Position -lt $reader.BaseStream.Length) {
            Assert-True (-not $ended) 'Data follows the end record.'
            $kind = $reader.ReadByte()
            if ($kind -eq 1) {
                $type = $reader.ReadByte(); $flags = $reader.ReadByte()
                [void]$reader.ReadByte()
                $length = $reader.ReadUInt16(); [void]$reader.ReadUInt16()
                [void]$reader.ReadUInt32(); $inode = $reader.ReadUInt64(); $parent = $reader.ReadUInt64()
                $size = $reader.ReadUInt64(); [void]$reader.ReadBytes(24)
                $nameBytes = $reader.ReadBytes($length)
                Assert-True ($nameBytes.Length -eq $length) 'Truncated name.'
                $name = [Text.Encoding]::UTF8.GetString($nameBytes)
                if ($entries.Count -eq 0) {
                    Assert-True ($parent -eq 0 -and $name -eq '' -and $type -eq 1) 'Invalid root.'
                }
                $entry = [pscustomobject]@{ Path = ''; Name = $name; Parent = $parent; Size = $size }
                if ($type -eq 1 -and $flags -eq 0) { $paths[$inode] = $entry }
                $entries.Add($entry)
            }
            elseif ($kind -eq 3) {
                [void]$reader.ReadBytes(3)
                [void]$reader.ReadUInt32(); $value = $reader.ReadUInt64()
                Assert-True ($value -eq $entries.Count) 'Incorrect end count.'
                $ended = $true
            }
            else { throw "Unexpected record: $kind" }
        }
        Assert-True $ended 'Missing end record.'
        foreach ($entry in $entries) {
            $parts = [Collections.Generic.List[string]]::new()
            for ($node = $entry; $node.Parent -ne 0; $node = $paths[$node.Parent]) {
                Assert-True ($paths.ContainsKey($node.Parent) -and $parts.Count -lt $entries.Count) 'Invalid parent.'
                $parts.Add($node.Name)
            }
            $parts.Reverse()
            $entry.Path = $parts -join '/'
        }
        return ,$entries
    }
    finally { $reader.Dispose() }
}

$root = [string](Invoke-Linux 'mktemp -d /tmp/wds-runner.XXXXXXXX')
try {
    $destination = if ($IsWindows) { "\\wsl.localhost\$Distro" + $root.Replace('/', '\') + '\runner' }
        else { "$root/runner" }
    Copy-Item -LiteralPath $Runner -Destination $destination -Force
    Invoke-Linux @'
set -e
r=$1
chmod 700 "$r/runner"
mkdir -p "$r/tree/deep"
for branch in 1 2 3 4; do
    mkdir "$r/tree/bulk$branch"
    i=0
    while [ "$i" -lt 5000 ]; do printf x > "$r/tree/bulk$branch/file$i"; i=$((i+1)); done
done
cd "$r/tree/deep"
i=0
while [ "$i" -lt 80 ]; do
    n=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl
    mkdir "$n"; cd -P "$n"; i=$((i+1))
done
printf deep > leaf
'@ @($root) | Out-Null
    $scan = Read-Scan (Invoke-Runner)
    Assert-True (@($scan | Where-Object { $_.Path.Length -gt 4096 -and $_.Size -eq 4 }).Count -eq 1) `
        'The deep path was not scanned.'
    Assert-True (@($scan | Where-Object { $_.Path -like 'bulk*/*' -and $_.Size -eq 1 }).Count -eq 20000) `
        'Concurrent output lost or split records.'
    $closed = Invoke-Runner @('--self-delete', "$root/tree") -CloseOutput
    Assert-True ($closed.Code -ne 0) 'The runner did not fail when its consumer disconnected.'
    Invoke-Linux 'test ! -e "$1/runner"' @($root) | Out-Null
    Write-Host 'PASS: concurrent streams, deep paths, consumer disconnect, and self-deletion.'
}
finally {
    Invoke-Linux @'
case "$1" in /tmp/wds-runner.*) ;; *) exit 1;; esac
if cd "$1/tree/deep"; then
    n=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl
    i=0
    while [ -d "$n" ]; do cd -P "$n" || exit; i=$((i+1)); done
    rm -f leaf
    while [ "$i" -gt 0 ]; do cd -P .. || exit; rmdir "$n" || exit; i=$((i-1)); done
fi
cd /
rm -rf "$1"
'@ @($root) | Out-Null
}
