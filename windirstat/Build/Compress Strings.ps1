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

$COMPRESSION_FORMAT_LZNT1 = 0x2
$COMPRESSION_ENGINE_MAXIMUM = 0x0100
$Alg = $COMPRESSION_FORMAT_LZNT1 -bor $COMPRESSION_ENGINE_MAXIMUM

[uint32]$workSpaceSize = 0
[uint32]$fragmentWorkSpaceSize = 0
if ($CompressLibrary::RtlGetCompressionWorkSpaceSize($Alg, [ref]$workSpaceSize, [ref]$fragmentWorkSpaceSize) -ne 0) { Exit 1 }
$workSpaceBuffer = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([int]$workSpaceSize)

$Files = Get-ChildItem -Path "$Path\*.txt" -Recurse
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
        $NewFile = $File.FullName -replace '.txt$','.bin'
        [System.IO.File]::WriteAllBytes($NewFile, $compressedData)
    }
}

$TempHeader = (New-TemporaryFile).FullName
'#pragma once' | Out-File $TempHeader -Force -Encoding utf8
'#include <string_view>' | Out-File $TempHeader -Encoding utf8 -Append
$Files | Where-Object Name -like 'lang_*.txt' | Get-Content | 
    ForEach-Object { $_ -replace '=.*','' } |
    Sort-Object -Unique | 
    ForEach { "constexpr std::wstring_view $_ = L""$_"";" } |
    Out-File $TempHeader -Encoding utf8 -Append
If ((Get-FileHash "$Path\LangStrings.h").Hash -ne (Get-FileHash $TempHeader).Hash)
{
    Copy-Item -LiteralPath $TempHeader "$Path\LangStrings.h" -Force
}
Remove-Item -LiteralPath $TempHeader -Force