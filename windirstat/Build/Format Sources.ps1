param(
    [string]$Path = $PSScriptRoot
)

# Ensure we have UTF-8 encoding with BOM
$Utf8WithBom = New-Object System.Text.UTF8Encoding($true)

# Find all .cpp and .h files recursively
$Files = Get-ChildItem -Path $Path -Include *.cpp, *.h -Recurse

foreach ($File in $Files) {
    $FilePath = $File.FullName

    # Read text using .NET API to auto-detect encoding correctly
    $Content = [System.IO.File]::ReadAllText($FilePath)

    # 1. Normalize line endings to Windows standard CRLF (\r\n) for processing
    $Content = [System.Text.RegularExpressions.Regex]::Replace($Content, "\r?\n", "`r`n")

    # 2. No trailing whitespace at the end of lines
    $Content = [System.Text.RegularExpressions.Regex]::Replace($Content, "[ \t]+(?=\r?\n|$)", "")

    # 3. No three consecutive line breaks (maximum of one blank line between lines of content)
    $Content = [System.Text.RegularExpressions.Regex]::Replace($Content, "(\r\n){3,}", "`r`n`r`n")

    # 4. No unnecessary newlines at the end of the file
    $Content = [System.Text.RegularExpressions.Regex]::Replace($Content, "(?s)\s*\z", "")
    $Content = $Content + "`r`n"

    # Only write the file back (as UTF-8 with BOM) if the bytes actually
    # changed, so we don't bump the timestamp and trigger needless rebuilds.
    $NewBytes = $Utf8WithBom.GetPreamble() + $Utf8WithBom.GetBytes($Content)
    $OldBytes = [System.IO.File]::ReadAllBytes($FilePath)
    if (-not [System.Linq.Enumerable]::SequenceEqual([byte[]]$NewBytes, [byte[]]$OldBytes)) {
        [System.IO.File]::WriteAllBytes($FilePath, $NewBytes)
    }
}
