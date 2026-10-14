#Requires -Version 7.6
param(
    [ValidateSet('S3', 'Remote')] [string] $Protocol,
    [string] $ReadyFile,
    [string] $AzureEndpoint,
    [string] $DavEndpoint
)

$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'StorageServer.cs')
[WdsStorageFixture]::Run($Protocol, $ReadyFile, $AzureEndpoint, $DavEndpoint)
