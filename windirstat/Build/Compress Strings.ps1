param($Path)

# Combine all language files into lang.txt
$Encoding = New-Object System.Text.UTF8Encoding $False
$Files = Get-ChildItem -Path "$Path\*.txt" -Recurse
$CombinedLines = Get-ChildItem -Path "${Path}\lang_*.txt" -Recurse |
    Where-Object Name -match '^lang_([a-z]{2}(?:-[A-Z]{2})?)\.txt$' | ForEach-Object `
{
    $Content = $_ | Get-Content -Encoding UTF8 | Sort-Object -Unique | Where-Object { -not [string]::IsNullOrEmpty($_) }
    [System.IO.File]::WriteAllLines($_.FullName, $Content, $Encoding)

    $LangCode = $Matches[1]
    Get-Content $_ -Encoding UTF8 | ForEach-Object { "${LangCode}:$_" }
    $Files = @($Files | Where-Object FullName -ne $_.FullName)
}
if ($CombinedLines) {
    $OutFile = (Join-Path $Path 'lang_combined.txt')
    [System.IO.File]::WriteAllLines($OutFile, $CombinedLines, $Encoding)
    $Files += @(Get-Item $OutFile)
}

# Sort lines for normalization / comparison
Get-ChildItem -Path "${Path}\lang_*.txt" -Recurse | ForEach-Object { 
   $FileData = $_ | Get-Content -Encoding UTF8 | Sort-Object -Unique
   [System.IO.File]::WriteAllLines($_.FullName, $FileData, $Encoding)
}

# Write out languages header file
$TempHeader = (New-TemporaryFile).FullName
'#pragma once' | Out-File $TempHeader -Force -Encoding utf8
'#include <string_view>' | Out-File $TempHeader -Encoding utf8 -Append
$CombinedLines | 
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
    $TxtFile = $File.FullName
    $BinFile = $TxtFile -replace '.txt$','.bin'
    makecab /D CompressionType=LZX /D CompressionMemory=21 $TxtFile $BinFile | Out-Null
    If ($File.Name -eq 'lang_combined.txt') { Remove-Item $TxtFile -Force } 
}

