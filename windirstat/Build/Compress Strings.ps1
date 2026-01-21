param($Path)

$env:LIB = ''
$CompressLibrary = Add-Type -TypeDefinition @"
    using System;
    using System.Runtime.InteropServices;

    public class RtlCompression
    {
        [DllImport("ntdll.dll", SetLastError=true)]
        public static extern uint RtlCompressBuffer(
            ushort CompressionFormat,
            byte[] UncompressedBuffer,
            uint UncompressedBufferSize,
            byte[] CompressedBuffer,
            uint CompressedBufferSize,
            uint chunkSize,
            out uint FinalCompressedSize,
            IntPtr WorkSpace
        );

        [DllImport("ntdll.dll", SetLastError=true)]
        public static extern uint RtlGetCompressionWorkSpaceSize(
            ushort CompressionFormat,
            out uint CompressBufferWorkSpaceSize,
            out uint CompressFragmentWorkSpaceSize
        );
    }
"@ -PassThru

$COMPRESSION_FORMAT_XPRESS_HUFF = 0x4
$COMPRESSION_ENGINE_MAXIMUM = 0x0100
$Alg = $COMPRESSION_FORMAT_XPRESS_HUFF -bor $COMPRESSION_ENGINE_MAXIMUM

[uint32]$workSpaceSize = 0
[uint32]$fragmentWorkSpaceSize = 0
if ($CompressLibrary::RtlGetCompressionWorkSpaceSize($Alg, [ref]$workSpaceSize, [ref]$fragmentWorkSpaceSize) -ne 0) { Exit 1 }
$workSpaceBuffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$workSpaceSize)

# Combine all language files into lang.txt
$Files = Get-ChildItem -Path "$Path\*.txt" -Recurse
$CombinedLines = Get-ChildItem -Path "${Path}\lang_*.txt" -Recurse |
    Where-Object Name -match '^lang_([a-z]{2}(?:-[A-Z]{2})?)\.txt$' | ForEach-Object `
{
    $LangCode = $Matches[1]
    Get-Content $_ -Encoding UTF8 | ForEach-Object { "${LangCode}:$_" }
}
if ($CombinedLines) {
    $CombinedLines = $CombinedLines | Sort-Object -Unique
    $Encoding = New-Object System.Text.UTF8Encoding $False
    $OutFile = (Join-Path $Path 'lang_combined.txt')
    [System.IO.File]::WriteAllLines($OutFile, $CombinedLines, $Encoding)
    $Files = @(Get-Item -LiteralPath $OutFile)
}

# Write out languages header file
$TempHeader = (New-TemporaryFile).FullName
'#pragma once' | Out-File $TempHeader -Force -Encoding utf8
'#include <string_view>' | Out-File $TempHeader -Encoding utf8 -Append
$Files | Where-Object Name -like 'lang_*.txt' | Get-Content | 
    ForEach-Object { $_ -replace '=.*','' -replace '^.*?:','' } |
    Sort-Object -Unique | 
    ForEach { "constexpr std::wstring_view $_ = L""$_"";" } |
    Out-File $TempHeader -Encoding utf8 -Append
If ((Get-FileHash "$Path\LangStrings.h").Hash -ne (Get-FileHash $TempHeader).Hash)
{
    Copy-Item -LiteralPath $TempHeader "$Path\LangStrings.h" -Force
}
Remove-Item -LiteralPath $TempHeader -Force

# Compress file data
ForEach ($File in $Files)
{
    If ($File.Name -like 'lang_*.txt')
    {
        $Content = $File | Get-Content -Encoding UTF8 | Sort-Object -Unique | Where-Object { -not [string]::IsNullOrEmpty($_) }
        $Encoding = New-Object System.Text.UTF8Encoding $False
        [System.IO.File]::WriteAllLines($File.FullName, $Content, $Encoding)
    }

    $bytesToCompress = [System.IO.File]::ReadAllBytes($File.FullName)
    $compressedData = New-Object byte[] ($bytesToCompress.Length)

    [uint32]$compressedSize = 0
    if ($CompressLibrary::RtlCompressBuffer($Alg, $bytesToCompress, $bytesToCompress.Length,
        $compressedData, $compressedData.Length, 4096, [ref]$compressedSize, $workSpaceBuffer) -eq 0)
    {
        [Array]::Resize([ref] $compressedData, $compressedSize)
        
        # Prepend uncompressed size as ULONG (4 bytes, little-endian)
        $uncompressedSize = [uint32]$bytesToCompress.Length
        $sizeBytes = [System.BitConverter]::GetBytes($uncompressedSize)
        $finalData = $sizeBytes + $compressedData
        
        $NewFile = $File.FullName -replace '.txt$','.bin'
        [System.IO.File]::WriteAllBytes($NewFile, $finalData)
        If ($File.Name -eq 'lang_combined.txt') { Remove-Item $File.FullName -Force } 
    }
}

