@echo off
setlocal EnableDelayedExpansion

:: ---------------------------------------------------------------------------
:: Run-Tests.bat
:: Runs the WinDirStat test suite via PowerShell 7 (pwsh).
::
:: Usage:
::   Run-Tests.bat [options passed through to each test script]
::
:: The three automated test scripts are executed in order:
::   1. Test-FilteringRegexCsv.ps1   - CSV filtering / regex / glob / size
::   2. Test-NonVisualSettings.ps1   - Non-visual settings load/save round-trips
::   3. Test-UiNavigation.ps1        - UI automation / end-to-end navigation
::
:: Analysis tools (require MSBuild; invoked separately):
::   Show-LargestFunctions.ps1       - Largest functions by code size in MAP
::   Show-InlinedFunctions.ps1       - Functions fully inlined in Release build
::   Build-PgoTrainingProfiles.ps1   - PGO training data collection
::
:: All scripts default to publish\x64\WinDirStat.exe as the test binary.
:: Pass -ExePath <path> to override.
::
:: Requirements:
::   pwsh (PowerShell 7+)  -  https://github.com/PowerShell/PowerShell/releases
:: ---------------------------------------------------------------------------

where pwsh >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: pwsh ^(PowerShell 7^) was not found on PATH.
    echo Install from https://github.com/PowerShell/PowerShell/releases
    exit /b 1
)

set TESTS_DIR=%~dp0
set OVERALL=0

echo.
echo ===========================================================================
echo  WinDirStat Test Suite
echo ===========================================================================
echo.

:: ---------------------------------------------------------------------------
:: 1. Filtering / CSV / regex tests
:: ---------------------------------------------------------------------------
echo [1/3] Test-FilteringRegexCsv.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-FilteringRegexCsv.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo FAILED: Test-FilteringRegexCsv
    set OVERALL=1
) else (
    echo PASSED: Test-FilteringRegexCsv
)
echo.

:: ---------------------------------------------------------------------------
:: 2. Non-visual settings tests  (requires a full build of the settings-test
::    binary unless -SkipBuild -ExePath <path> is supplied)
:: ---------------------------------------------------------------------------
echo [2/3] Test-NonVisualSettings.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-NonVisualSettings.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo FAILED: Test-NonVisualSettings
    set OVERALL=1
) else (
    echo PASSED: Test-NonVisualSettings
)
echo.

:: ---------------------------------------------------------------------------
:: 3. UI automation / navigation tests
:: ---------------------------------------------------------------------------
echo [3/3] Test-UiNavigation.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-UiNavigation.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo FAILED: Test-UiNavigation
    set OVERALL=1
) else (
    echo PASSED: Test-UiNavigation
)
echo.

:: ---------------------------------------------------------------------------
:: Summary
:: ---------------------------------------------------------------------------
echo ===========================================================================
if %OVERALL% equ 0 (
    echo  All tests PASSED.
) else (
    echo  One or more tests FAILED.
)
echo ===========================================================================
echo.

exit /b %OVERALL%
