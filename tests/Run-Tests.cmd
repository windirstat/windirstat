@ECHO OFF
SETLOCAL

:: Runs the behavioral suite against the newest local x64 build or published executable.
:: All arguments are passed through unchanged; -List shows scenario selection.
:: Examples:
::   Run-Tests.cmd -List
::   Run-Tests.cmd -Only Rendering
::   Run-Tests.cmd -Tag Race -Repeat 5 -Shuffle -Seed 42
::   Run-Tests.cmd -Profile Extended -ExePath build\WinDirStat_x64.exe

WHERE pwsh >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: PowerShell 7.6 or newer ^(pwsh^) is required.
    EXIT /B 2
)
SET TESTS_DIR=%~dp0
pwsh -NoProfile -STA -ExecutionPolicy Bypass -File "%TESTS_DIR%Test-WinDirStat.ps1" %*
EXIT /B %ERRORLEVEL%
