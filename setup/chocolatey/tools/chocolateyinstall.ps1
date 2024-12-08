$ErrorActionPreference = 'Stop';

$packageName= $env:ChocolateyPackageName
$toolsDir = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$metadata = Get-Content "${PSScriptRoot}\chocolateymetadata.json" | ConvertFrom-Json

$url = 'https://github.com/windirstat/windirstat/releases/download/release/v'
$urlx86 = $url + $Metadata.version + '/WinDirStat-x86.msi'
$urlx64 = $url + $Metadata.version + '/WinDirStat-x64.msi'

$packageArgs = @{
  packageName   = $packageName
  unzipLocation = $toolsDir
  fileType      = 'msi'
  url           = $urlx86
  url64bit      = $urlx64

  softwareName  = 'WinDirStat'

  checksum      = $metadata.hashX86
  checksumType  = 'sha256'
  checksum64    = $metadata.hashX64
  checksumType64= 'sha256'

  silentArgs    = "/qn /norestart /l*v `"$($env:TEMP)\$($packageName).$($env:chocolateyPackageVersion).MsiInstall.log`""
  validExitCodes= @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs
