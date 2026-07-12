@echo off
setlocal EnableDelayedExpansion

:: ---------------------------------------------------------------------------
:: Run-Tests.cmd
:: Runs the WinDirStat test suite via PowerShell 7.6 (pwsh).
::
:: Usage:
::   Run-Tests.cmd [options passed through to Test-WinDirStat.ps1]
::
:: All tests live in a single script, Test-WinDirStat.ps1, which runs nine
:: suites in one pass and prints one unified summary:
::
::   Filtering  - CSV filtering / regex / glob / size (headless CLI scans)
::   Settings   - non-visual settings load/clamping/derived behavior. Auto-builds an
::                instrumented (/DWDS_SETTINGS_TEST) binary when MSBuild is
::                available; otherwise the suite skips.  Pass
::                -SettingsExePath <exe> to use a prebuilt one.
::   Ui         - UIAutomation end-to-end navigation, plus file-operation
::                verification (Compress / Sparsify / Deduplicate-with-Hardlink /
::                Remove Mark-Of-The-Web) on single AND multiple selections,
::                each validated against the real file system; large-corpus
::                count/extension/dupe checks; and a pause/resume stress phase.
::   Reparse    - symlinks / junctions / mount points.  FORMATS the two scratch
::                drives (LINK_TEST_DRIVE_ONE / LINK_TEST_DRIVE_TWO, default
::                E: / F:) and runs only when elevated AND both drives exist;
::                otherwise the suite skips.  Never uses C:.
::   EdgeCases  - deep paths, unicode, attributes, file properties.
::   Enumeration - path spelling, redirect, engine, size, and file-system parity.
::   Unc        - UNC share-root scan regression coverage.
::   Permissions - permissions scanner CSV / JSON exports.
::   Cli        - command-line parsing, validation, and quiet-mode termination.
::
:: EVERY suite runs by default -- no opt-in switches are required.  Suites whose
:: prerequisites are not met skip gracefully.  Narrow a run with -Only / -Skip,
:: e.g.:  Run-Tests.cmd -Only Filtering,EdgeCases
::
:: The binary under test defaults to publish\x64\WinDirStat.exe; override with
:: -ExePath <path>.
::
:: Requirements:
::   pwsh (PowerShell 7.6+)  -  https://github.com/PowerShell/PowerShell/releases
:: ---------------------------------------------------------------------------

where pwsh >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: pwsh ^(PowerShell 7.6^) was not found on PATH.
    echo Install from https://github.com/PowerShell/PowerShell/releases
    exit /b 1
)

set TESTS_DIR=%~dp0

pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-WinDirStat.ps1" %*
exit /b %ERRORLEVEL%
