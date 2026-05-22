@echo off
setlocal EnableDelayedExpansion

:: ---------------------------------------------------------------------------
:: Run-Tests.bat
:: Runs the WinDirStat test suite via PowerShell 7 (pwsh).
::
:: Usage:
::   Run-Tests.bat [options passed through to each test script]
::
:: The four automated test scripts are executed in order:
::   1. Test-FilteringRegexCsv.ps1       - CSV filtering / regex / glob / size
::   2. Test-NonVisualSettings.ps1       - Non-visual settings load/save round-trips
::   3. Test-UiNavigation.ps1            - UI automation / end-to-end navigation
::   4. Test-ReparseLinkBehavior.ps1     - Symlinks / junctions / mount points
::                                         (requires admin + two dedicated drives;
::                                          set LINK_TEST_DRIVE_ONE / LINK_TEST_DRIVE_TWO
::                                          before running, or relies on defaults E: / F:)
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

:: Default drive letters for the reparse/link test; override via environment.
if not defined LINK_TEST_DRIVE_ONE set LINK_TEST_DRIVE_ONE=E:
if not defined LINK_TEST_DRIVE_TWO set LINK_TEST_DRIVE_TWO=F:

echo.
echo ===========================================================================
echo  WinDirStat Test Suite
echo ===========================================================================
echo.

:: ---------------------------------------------------------------------------
:: 1. Filtering / CSV / regex tests
:: ---------------------------------------------------------------------------
echo [1/4] Test-FilteringRegexCsv.ps1
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
echo [2/4] Test-NonVisualSettings.ps1
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
echo [3/4] Test-UiNavigation.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-UiNavigation.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo FAILED: Test-UiNavigation
    set OVERALL=1
) else (
    echo PASSED: Test-UiNavigation
)
echo.

:: ---------------------------------------------------------------------------
:: 4. Reparse / link behavior tests
::    Requires administrator privileges and two dedicated test drives that will
::    be formatted.  Skipped automatically when either drive is unavailable or
::    the session is not elevated.
:: ---------------------------------------------------------------------------
echo [4/4] Test-ReparseLinkBehavior.ps1
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo SKIPPED: Test-ReparseLinkBehavior ^(requires administrator privileges^)
    goto :link_test_done
)
if not exist "%LINK_TEST_DRIVE_ONE%\" (
    echo SKIPPED: Test-ReparseLinkBehavior ^(drive %LINK_TEST_DRIVE_ONE% not found^)
    goto :link_test_done
)
if not exist "%LINK_TEST_DRIVE_TWO%\" (
    echo SKIPPED: Test-ReparseLinkBehavior ^(drive %LINK_TEST_DRIVE_TWO% not found^)
    goto :link_test_done
)
pwsh -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-ReparseLinkBehavior.ps1" -DriveOne %LINK_TEST_DRIVE_ONE% -DriveTwo %LINK_TEST_DRIVE_TWO% %*
if %ERRORLEVEL% neq 0 (
    echo FAILED: Test-ReparseLinkBehavior
    set OVERALL=1
) else (
    echo PASSED: Test-ReparseLinkBehavior
)
:link_test_done
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
