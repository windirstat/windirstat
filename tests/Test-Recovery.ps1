[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Run')]
    [string] $DisposableVolumeRoot,
    [Parameter(ParameterSetName = 'Build')]
    [switch] $BuildOnly,
    [string] $OutputFolder,
    [ValidateSet('x64', 'Win32')]
    [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$builder = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
if ($builder) {
    $builderPath = $builder.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    $builderPath = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
}
& $builderPath (Join-Path $PSScriptRoot 'RecoveryTests.vcxproj') /t:Build /p:Configuration=Release `
    "/p:Platform=$Platform" /m:4 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$exe = Join-Path $repoRoot "intermediate\recovery-tests\$Platform\RecoveryTests.exe"
& $exe --filter-stress
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($BuildOnly) { exit 0 }

$administrator = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $administrator.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run the recovery drive tests from an elevated PowerShell prompt.'
}
if (-not $OutputFolder) {
    $OutputFolder = Join-Path $repoRoot ('intermediate\recovery-tests\run-' + [guid]::NewGuid().ToString('N'))
}
Write-Host "Creating and deleting recovery fixtures on $DisposableVolumeRoot"
& $exe $DisposableVolumeRoot $OutputFolder
exit $LASTEXITCODE
