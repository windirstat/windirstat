param($Path)

# Normalize module path issues
$env:PSModulePath = Join-Path ([System.Environment]::SystemDirectory) '\WindowsPowerShell\v1.0\Modules'

# Combine all language files into lang_combined.txt
$Encoding = [System.Text.UTF8Encoding]::new($false)
$Files = Get-ChildItem -Path "$Path\*.txt" -Recurse
$CombinedLines = Get-ChildItem -Path "${Path}\lang_*.txt" -Recurse |
    Where-Object Name -match '^lang_([a-z]{2}(?:-[A-Z]{2})?)\.txt$' | ForEach-Object `
{
    $Content = $_ | Get-Content -Encoding UTF8 | Sort-Object -Unique | Where-Object { $_ }
    [System.IO.File]::WriteAllLines($_.FullName, $Content, $Encoding)

    $LangCode = $Matches[1]
    $Content | Where-Object { $_ -notmatch '^MSI_' } | ForEach-Object { "${LangCode}:$_" }
    $Files = @($Files | Where-Object FullName -ne $_.FullName)
}
if ($CombinedLines) {
    $OutFile = Join-Path $Path 'lang_combined.txt'
    [System.IO.File]::WriteAllLines($OutFile, $CombinedLines, $Encoding)
    $Files += @(Get-Item $OutFile)
}

# Sort lines for normalization / comparison
Get-ChildItem -Path "${Path}\lang_*.txt" -Recurse | ForEach-Object {
    [System.IO.File]::WriteAllLines($_.FullName, ($_ | Get-Content -Encoding UTF8 | Sort-Object -Unique), $Encoding)
}

# Write out languages header file
$TempHeader = (New-TemporaryFile).FullName
@(
    '#pragma once'
    '#include <string_view>'
    ($CombinedLines | ForEach-Object { $_ -replace '=.*','' -replace '^.*?:','' } | Sort-Object -Unique | ForEach-Object { "constexpr std::wstring_view $_ = L""$_"";" })
) | Out-File $TempHeader -Encoding utf8 -Force
if ((Get-FileHash "$Path\LangStrings.h").Hash -ne (Get-FileHash $TempHeader).Hash) {
    Copy-Item -LiteralPath $TempHeader "$Path\LangStrings.h" -Force
}
Remove-Item -LiteralPath $TempHeader -Force

# Compress file data
foreach ($File in $Files) {
    makecab /D CompressionType=LZX /D CompressionMemory=21 $File.FullName ($File.FullName -replace '\.txt$', '.bin') | Out-Null
    if ($File.Name -eq 'lang_combined.txt') { Remove-Item $File.FullName -Force }
}
