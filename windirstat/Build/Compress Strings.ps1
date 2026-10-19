param($Path)

# Normalize module path issues
$env:PSModulePath = Join-Path ([System.Environment]::SystemDirectory) '\WindowsPowerShell\v1.0\Modules'
$Encoding = [System.Text.UTF8Encoding]::new($false)

# Write files only when content changes so timestamps stay stable for incremental builds
function Write-Lines($File, $Lines) {
    $Text = ($Lines -join "`r`n") + "`r`n"
    if ((Test-Path -LiteralPath $File) -and [System.IO.File]::ReadAllText($File, $Encoding) -ceq $Text) { return }
    [System.IO.File]::WriteAllText($File, $Text, $Encoding)
}

# Normalize language files and combine them into one [language] section per file
Write-Host "Compiling language resource"
$Combined = foreach ($File in Get-ChildItem -Path "$Path\langs\lang_*.txt") {
    if ($File.Name -notmatch '^lang_([a-z]{2}(?:-[A-Z]{2})?)\.txt$') { continue }
    $Lines = @([System.IO.File]::ReadAllLines($File.FullName, $Encoding) -ne '' | Sort-Object -Unique)
    Write-Lines $File.FullName $Lines
    "[$($Matches[1])]"
    $Lines -notmatch '^MSI_'
}
Write-Lines "$Path\lang_combined.txt" $Combined

# Write out language header file
Write-Host "Generating language header file"
$Keys = $Combined -notmatch '^\[' -replace '=.*' | Sort-Object -Unique
Write-Lines "$Path\LangStrings.h" (@('#pragma once') + ($Keys -replace '.+', 'constexpr std::wstring_view $0 = L"$0";'))

# Compress text resources whose cabinet is older than its source
Write-Host "Generating language resource"
foreach ($Source in "$Path\license.txt", "$Path\lang_combined.txt") {
    $Target = [System.IO.Path]::ChangeExtension($Source, '.bin')
    if ([System.IO.File]::GetLastWriteTime($Target) -ge [System.IO.File]::GetLastWriteTime($Source)) { continue }
    makecab /D CompressionType=LZX /D CompressionMemory=21 $Source $Target | Out-Null
}
