function Convert-CIntegerLiteral {
    param([Parameter(Mandatory)] [string] $Value)

    $trimmed = $Value.Trim()
    if ($trimmed -match '^0[xX](?<hex>[0-9A-Fa-f]+)$') {
        return [Convert]::ToInt32($Matches.hex, 16)
    }
    return [int] $trimmed
}

function Read-CHeaderNumericDefines {
    param([Parameter(Mandatory)] [string] $Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required header not found: $Path"
    }

    $defines = [ordered] @{}
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line -match '^\s*#define\s+(?<name>[A-Za-z_][A-Za-z0-9_]*)\s+(?<value>(?:0[xX][0-9A-Fa-f]+)|-?\d+)\b') {
            $defines[$Matches.name] = Convert-CIntegerLiteral $Matches.value
        }
    }
    return $defines
}

function Read-CSequentialEnum {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $EnumName
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required header not found: $Path"
    }

    $text = [System.IO.File]::ReadAllText($Path)
    $pattern = "enum\s+$([regex]::Escape($EnumName))\s*\{(?<body>.*?)\};"
    $match = [regex]::Match($text, $pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) {
        throw "Enum '$EnumName' was not found in $Path"
    }

    $values = [ordered] @{}
    $nextValue = 0
    foreach ($rawLine in ($match.Groups['body'].Value -split '\r?\n')) {
        $line = ($rawLine -replace '//.*$', '').Trim().TrimEnd(',')
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        if ($line -match '^(?<name>[A-Za-z_][A-Za-z0-9_]*)(?:\s*=\s*(?<value>(?:0[xX][0-9A-Fa-f]+)|-?\d+))?$') {
            if ($Matches.ContainsKey('value') -and $Matches.value) {
                $nextValue = Convert-CIntegerLiteral $Matches.value
            }
            $values[$Matches.name] = $nextValue
            $nextValue++
        }
    }
    return $values
}

function Get-RequiredMapValue {
    param(
        [Parameter(Mandatory)] [System.Collections.IDictionary] $Map,
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [string] $Source
    )

    if (-not $Map.Contains($Name)) {
        throw "Required symbol '$Name' was not found in $Source"
    }
    return [int] $Map[$Name]
}

function Write-ColoredLine {
    param([Parameter(Mandatory)] [AllowEmptyString()] [string] $Message, [ConsoleColor] $Color = [ConsoleColor]::Gray)
    Write-Host $Message -ForegroundColor $Color
}

function Write-LabelValue {
    param(
        [Parameter(Mandatory)] [string] $Label,
        [AllowNull()] [object] $Value,
        [ConsoleColor] $ValueColor = [ConsoleColor]::Gray
    )
    Write-Host -NoNewline "${Label}: " -ForegroundColor DarkCyan
    Write-Host $Value -ForegroundColor $ValueColor
}

function New-MapAnalysisContext {
    param([Parameter(Mandatory)] [ValidateSet('x64', 'Win32', 'ARM64')] [string] $Platform)

    $root = Split-Path -Parent $PSScriptRoot
    $solution = Join-Path $root 'windirstat.sln'
    if (-not (Test-Path -LiteralPath $solution)) { throw "Solution not found: $solution" }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found — is Visual Studio installed?' }

    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw 'MSBuild.exe not found' }

    $vsRoot = & $vswhere -latest -property installationPath
    $undname = Get-ChildItem -Path "$vsRoot\VC\Tools\MSVC" -Filter 'undname.exe' -Recurse `
        -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -match 'Hostx64.x64$|HostX86.x86$' } |
        Select-Object -First 1 -ExpandProperty FullName

    $platformShort = @{ 'Win32' = 'x86'; 'x64' = 'x64'; 'ARM64' = 'arm64' }[$Platform]
    return [pscustomobject] @{
        Root         = $root
        Solution     = $solution
        AnalysisRoot = Join-Path $root 'build_analysis'
        Platform     = $Platform
        ExeName      = "WinDirStat_$platformShort"
        MSBuild      = $msbuild
        Undname      = $undname
    }
}

function New-MapOverrideTargets {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $MapFile,
        [hashtable] $ClCompileExtra = @{},
        [hashtable] $LinkExtra = @{}
    )

    $clLines = $ClCompileExtra.GetEnumerator() | ForEach-Object {
        "      <$($_.Key)>$([System.Security.SecurityElement]::Escape([string] $_.Value))</$($_.Key)>"
    }
    $linkLines = $LinkExtra.GetEnumerator() | ForEach-Object {
        "      <$($_.Key)>$([System.Security.SecurityElement]::Escape([string] $_.Value))</$($_.Key)>"
    }
    $mapFileXml = [System.Security.SecurityElement]::Escape($MapFile.Replace('\', '/'))

    Set-Content -LiteralPath $Path -Encoding UTF8 -Value @"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup>
    <ClCompile>
$($clLines -join "`n")
    </ClCompile>
    <Link>
      <GenerateMapFile>true</GenerateMapFile>
      <MapFileName>$mapFileXml</MapFileName>
$($linkLines -join "`n")
    </Link>
  </ItemDefinitionGroup>
</Project>
"@
}

function Invoke-MapBuild {
    param(
        [Parameter(Mandatory)] [object] $Context,
        [Parameter(Mandatory)] [string] $Label,
        [Parameter(Mandatory)] [string] $TargetsFile,
        [Parameter(Mandatory)] [string] $OutDir,
        [Parameter(Mandatory)] [string] $IntDir
    )

    Write-ColoredLine ''
    Write-ColoredLine ('─' * 72) DarkGray
    Write-ColoredLine " $Label" Cyan
    Write-ColoredLine ('─' * 72) DarkGray
    Write-ColoredLine ''

    & $Context.MSBuild $Context.Solution /t:Build /p:Configuration=Release "/p:Platform=$($Context.Platform)" `
        "/p:OutDir=$OutDir\" "/p:IntDir=$IntDir" "/p:ForceImportAfterCppTargets=$TargetsFile" /m
    if ($LASTEXITCODE -ne 0) { throw "Build failed — $Label" }
}

function Get-MapSymbols {
    param([Parameter(Mandatory)] [string] $Path)

    $table = [ordered] @{}
    $active = $false
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line -match 'Publics by Value|Static symbols') { $active = $true; continue }
        if (-not $active) { continue }
        if ($line -match '^\s+(0001:[0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{16})\s+(.+)') {
            $seg, $sym, $rva, $mod = $Matches[1], $Matches[2], $Matches[3], $Matches[4].Trim()
            if ($sym -notmatch '^(__imp_|_NULL_THUNK|__ImageBase|__safe_se)') {
                $table[$sym] = @{ Loc = "$seg  $mod"; RVA = [Convert]::ToInt64($rva, 16) }
            }
        }
    }
    return $table
}

function Get-FunctionSizes {
    param([Parameter(Mandatory)] [object] $Symbols)

    $sorted = @($Symbols.GetEnumerator() | Sort-Object { $_.Value.RVA })
    $sizes = @{}
    for ($i = 0; $i -lt $sorted.Count; $i++) {
        $current = $sorted[$i].Value.RVA
        $next = ($i + 1 -lt $sorted.Count) ? $sorted[$i + 1].Value.RVA : $current
        $sizes[$sorted[$i].Key] = [Math]::Max(0L, $next - $current)
    }
    return $sizes
}

function Get-PrettyName {
    param(
        [Parameter(Mandatory)] [string] $Mangled,
        [AllowNull()] [string] $Undname
    )

    if (-not $Undname) { return $null }
    try {
        $output = $Mangled | & $Undname 2>$null | Select-Object -Last 1
        if ($output -match 'is "(.+)"') { return $Matches[1] }
    } catch {}
    return $null
}
