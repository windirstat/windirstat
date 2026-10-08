[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]] $MsiPath,
    [string] $OutputFolder,
    [string] $ExpectedVersion,
    [switch] $AllowBuildNumber
)

$ErrorActionPreference = 'Stop'
if (-not $OutputFolder) {
    $OutputFolder = Join-Path $PSScriptRoot ('..\intermediate\installer-tests\run-' + [guid]::NewGuid().ToString('N'))
}
$OutputFolder = [IO.Path]::GetFullPath($OutputFolder)
New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
$results = [Collections.Generic.List[object]]::new()
$packageVersion = $null

$sourceLanguages = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot '..\windirstat\res\langs') -Filter 'lang_*.txt' |
    ForEach-Object {
        $metadata = @{}
        Get-Content -LiteralPath $_.FullName | ForEach-Object {
            if ($_ -match '^MSI_(CULTURE|LCID|CODEPAGE)=(.+)$') {
                $metadata[$Matches[1]] = $Matches[2]
            }
        }
        if ($metadata.Count -ne 3) { throw "Installer language metadata is incomplete: $($_.FullName)" }
        [int] $metadata.LCID
    } | Sort-Object)
if ($sourceLanguages.Count -ne @($sourceLanguages | Select-Object -Unique).Count -or 1033 -notin $sourceLanguages) {
    throw 'Installer source languages are duplicated or omit English.'
}
$installer = New-Object -ComObject WindowsInstaller.Installer

function Read-MsiTable {
    param([object] $Database, [string] $Query)

    $view = $Database.OpenView($Query)
    try {
        $view.Execute() | Out-Null
        $columns = $view.ColumnInfo(0)
        try {
            while ($record = $view.Fetch()) {
                try {
                    $row = [ordered] @{}
                    for ($field = 1; $field -le $record.FieldCount(); $field++) {
                        $row[$columns.StringData($field)] = if ($record.IsNull($field)) {
                            $null
                        } else {
                            $record.StringData($field)
                        }
                    }
                    [pscustomobject] $row
                } finally {
                    [Runtime.InteropServices.Marshal]::FinalReleaseComObject($record) | Out-Null
                }
            }
        } finally {
            [Runtime.InteropServices.Marshal]::FinalReleaseComObject($columns) | Out-Null
        }
    } finally {
        $view.Close() | Out-Null
        [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) | Out-Null
    }
}

try {
    foreach ($path in $MsiPath) {
        $path = (Resolve-Path -LiteralPath $path).Path
        $database = $installer.OpenDatabase($path, 0)
        try {
            $summary = $database.SummaryInformation(0)
            try {
                $template = $summary.Property(7)
            } finally {
                [Runtime.InteropServices.Marshal]::FinalReleaseComObject($summary) | Out-Null
            }
            $languages = @($template.Split(';')[1].Split(',') | ForEach-Object { [int] $_ })
            $transforms = @(Read-MsiTable $database 'SELECT `Name` FROM `_Storages`')
            $baseProperties = @{}
            Read-MsiTable $database 'SELECT * FROM `Property`' | ForEach-Object {
                $baseProperties[$_.Property] = $_.Value
            }
        } finally {
            [Runtime.InteropServices.Marshal]::FinalReleaseComObject($database) | Out-Null
        }

        if ([int] $baseProperties.ProductLanguage -ne 1033 -or $languages[0] -ne 1033) {
            throw "The package base language is not English: $path"
        }
        if (Compare-Object $sourceLanguages @($languages | Sort-Object)) {
            throw "Embedded languages differ from the source languages: $path"
        }
        if ($packageVersion -and $baseProperties.ProductVersion -ne $packageVersion) {
            throw "Installer packages have different ProductVersion values: $path"
        }
        $packageVersion = $baseProperties.ProductVersion
        if ($ExpectedVersion) {
            $versionMatches = $packageVersion -eq $ExpectedVersion
            if ($AllowBuildNumber) {
                $version = [version] $packageVersion
                $versionMatches = $version.Revision -ge 0 -and $version.ToString(3) -eq $ExpectedVersion
            }
            if (-not $versionMatches) {
                throw "ProductVersion does not match ${ExpectedVersion}: $path"
            }
        }
        $expected = @($languages | Where-Object { $_ -ne [int] $baseProperties.ProductLanguage } | Sort-Object)
        $actual = @($transforms.Name | ForEach-Object { [int] $_ } | Sort-Object)
        if (Compare-Object $expected $actual) { throw "Embedded languages differ from the summary: $path" }

        foreach ($language in $languages) {
            # Apply each embedded transform to a disposable database without committing changes.
            $copy = Join-Path $OutputFolder ([guid]::NewGuid().ToString('N') + '.msi')
            Copy-Item -LiteralPath $path -Destination $copy
            $database = $null
            try {
                $database = $installer.OpenDatabase($copy, 1)
                if ($language -ne [int] $baseProperties.ProductLanguage) {
                    $database.ApplyTransform(":$language", 32) | Out-Null
                }
                $properties = @{}
                Read-MsiTable $database 'SELECT * FROM `Property`' | ForEach-Object {
                    $properties[$_.Property] = $_.Value
                }
                foreach ($property in @('ProductCode', 'ProductVersion', 'UpgradeCode')) {
                    if ($properties[$property] -ne $baseProperties[$property]) {
                        throw "Transform $language changes ${property}: $path"
                    }
                }
                if ([int] $properties.ProductLanguage -ne $language) {
                    throw "Transform $language does not set the matching ProductLanguage: $path"
                }

                # Check both removal and downgrade detection, including the alternate release channel.
                $upgrades = @(Read-MsiTable $database 'SELECT * FROM `Upgrade`')
                foreach ($property in @('WIX_UPGRADE_DETECTED', 'WIX_DOWNGRADE_DETECTED', 'OLDPRODUCTS')) {
                    $row = @($upgrades | Where-Object ActionProperty -eq $property)
                    if ($row.Count -ne 1 -or $null -ne $row[0].Language) {
                        throw "Transform $language filters or omits ${property}: $path"
                    }
                    if ($property -ne 'OLDPRODUCTS' -and $row[0].UpgradeCode -ne $properties.UpgradeCode) {
                        throw "UpgradeCode differs for ${property}: $path"
                    }
                    if ($property -notin $properties.SecureCustomProperties.Split(';')) {
                        throw "Upgrade detection property is not secure: $property"
                    }
                }
                $upgrade = $upgrades | Where-Object ActionProperty -eq 'WIX_UPGRADE_DETECTED'
                $downgrade = $upgrades | Where-Object ActionProperty -eq 'WIX_DOWNGRADE_DETECTED'
                if ($upgrade.VersionMax -ne $properties.ProductVersion -or
                    $downgrade.VersionMin -ne $properties.ProductVersion -or
                    ([int] $upgrade.Attributes -band 2) -or -not ([int] $downgrade.Attributes -band 2)) {
                    throw "Version boundaries or detection attributes are incorrect: $path"
                }

                # Detection must run in both UI and silent installs, with removal inside the transaction.
                $sequence = @{}
                Read-MsiTable $database 'SELECT * FROM `InstallExecuteSequence`' | ForEach-Object {
                    $sequence[$_.Action] = [int] $_.Sequence
                }
                $uiDetection = @(Read-MsiTable $database (
                    'SELECT * FROM `InstallUISequence` WHERE `Action` = ''FindRelatedProducts'''))
                if ($uiDetection.Count -ne 1 -or -not $sequence.ContainsKey('FindRelatedProducts') -or
                    $sequence.FindRelatedProducts -ge $sequence.MigrateFeatureStates -or
                    $sequence.MigrateFeatureStates -ge $sequence.RemoveExistingProducts -or
                    $sequence.InstallInitialize -ge $sequence.RemoveExistingProducts -or
                    $sequence.RemoveExistingProducts -ge $sequence.InstallFiles) {
                    throw "Upgrade actions are missing or incorrectly ordered: $path"
                }
                $results.Add([pscustomobject] @{
                    Package = $path; Language = $language; ProductVersion = $properties.ProductVersion
                    UpgradeCode = $properties.UpgradeCode; Result = 'Passed'
                })
            } finally {
                if ($database) {
                    [Runtime.InteropServices.Marshal]::FinalReleaseComObject($database) | Out-Null
                }
                Remove-Item -LiteralPath $copy -Force
            }
        }
        Write-Host "Passed $($languages.Count) language variants: $path"
    }
    $results | Export-Csv -LiteralPath (Join-Path $OutputFolder 'results.csv') -NoTypeInformation
    Write-Host "Passed $($results.Count) package/language checks. Results: $OutputFolder"
} finally {
    [Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer) | Out-Null
}
