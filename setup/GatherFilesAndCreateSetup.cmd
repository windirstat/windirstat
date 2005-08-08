@echo off
:: ----------------------------------------------------------------------------
:: Script to create the NSIS installer from a prepared NSIS script
::
:: Requirements:
:: - NSIS must be installed in %ProgramFiles%\NSIS
:: - WDS.nsi and GetVersion.nsi must exist in the current directory
::
:: This script will do this:
:: 1. Copy required files into %install%
:: 2. Create %chkver% from GetVersion.nsi
:: 3. Call the created EXE file
:: 4. Parse the output in %verfile% into %vernsi%
:: 5. Search for wdsr* subdirectories in the ..\make directory
:: 6. Extract the names from the .<language>.txt files located there
:: 7. Putting all language information into WDS_Languages.nsi
:: ----------------------------------------------------------------------------
set install=.\Installer
set nsis=%ProgramFiles%\NSIS\makensis.exe
set verfile=Version.txt
set vernsi=%verfile:.txt=.nsi%
set chkver=GetVersionWDS.exe
set chkfile=%install%\windirstatA.exe
set langfile=WDS_Languages.nsi
set err=Unknown error
if NOT EXIST %nsis% @(
  set err=Could not find NSIS compiler at "%nsis%"
  goto ERROR
)
echo Copying files to temporary directory ...
echo ------------------------------------------------------------------------------
if NOT EXIST %install% @(
  md %install% > NUL
)
echo license.txt (License)
copy /y /v ..\make\windirstat\res\license.txt "%install%" > NUL
echo *.ico (Icons)
copy /y /v ..\make\windirstat\res\icon1.ico "%install%" > NUL
copy /y /v ..\make\windirstat\res\icon2.ico "%install%" > NUL
echo *.dll (Resource DLLs and shfolder.dll)
copy /y /v ..\release\*.dll "%install%" > NUL
copy /y /v ..\urelease\*.dll "%install%" > NUL
echo *.chm (Help files)
copy /y /v ..\urelease\*.chm "%install%" > NUL
echo *.exe (ANSI and Unicode version)
copy /y /v ..\urelease\windirstat.exe "%install%\windirstatU.exe" > NUL
copy /y /v ..\release\windirstat.exe "%install%\windirstatA.exe" > NUL
echo.
echo Now retrieving the current version
echo ^(from "%install%\windirstatA.exe"^)
echo ------------------------------------------------------------------------------
if NOT EXIST GetVersion.nsi @(
  set err=The NSIS script file which retrieves the version could not be found. Make sure it is in the current directory and accessible!
  goto ERROR
)
"%nsis%" "/DInstallerOut=%chkver%" "/DVersionOut=%verfile%" "/DVersionOfFile=%chkfile%" GetVersion.nsi > NUL
:: Delete the old version file, if it exists
if EXIST "%verfile%" del /f /q "%verfile%"
:: Try to call the EXE file created from the script
if EXIST "%chkver%" @(
  %chkver%
) else (
  set err=The program to retrieve the WDS version could not be found. Is the current directory read-only? Make sure it's not and try again.
  goto ERROR
)
:: Check for existance of the version file
if EXIST "%verfile%" @(
  echo Successfully retrieved file version:
) else (
  set err=Could not retrieve the file version of "%chkfile%", please make sure the file is at its place and accessible.
  goto ERROR
)
:: Delete the temporary EXE file
if EXIST "%chkver%" del /f /q "%chkver%"
:: Split the version string and then create several defines
for /f "tokens=1,2,3,4 delims=." %%i in (%verfile%) do (
  echo Version %%i.%%j.%%k ^(Build %%l^)
  echo ^!define sVersion "%%i.%%j.%%k.%%l" > %vernsi%
  echo ^!define sVersionS "%%i.%%j.%%k" >> %vernsi%
  echo ^!define sVersionBuild "%%l" >> %vernsi%
  echo ^!define dVersionMajor %%i >> %vernsi%
  echo ^!define dVersionMinor %%j >> %vernsi%
  echo ^!define dVersionRev %%k >> %vernsi%
  echo ^!define dVersionBuild %%l >> %vernsi%
  echo ^!define sVersionFile "%%i_%%j_%%k" >> %vernsi%
  echo ^!define sVersionFull "WinDirStat %%i.%%j.%%k" >> %vernsi%
  echo ^!define Installer "%install%" >> %vernsi%
)
:: Delete the Version.txt
if EXIST "%verfile%" del /f /q "%verfile%"
echo.
echo Going to collect information about available languages ...
echo ------------------------------------------------------------------------------
echo ^!ifdef LANGSECTIONS > "%langfile%"
for /f %%i in ('dir /b /A:D ..\make\wdsr*') do (
  for /f %%j in ('dir /b ..\make\%%i\.*.txt') do (
    call :GetLanguage %%i %%j "%langfile%" AppendLanguage
  )
)
echo ^!endif >> "%langfile%"
echo. >> "%langfile%"
echo ^!ifdef DESCRIBING >> "%langfile%"
for /f %%i in ('dir /b /A:D ..\make\wdsr*') do (
  for /f %%j in ('dir /b ..\make\%%i\.*.txt') do (
    call :GetLanguage %%i %%j "%langfile%" AppendLanguageDesc
  )
)
echo ^!endif >> "%langfile%"
echo. >> "%langfile%"
echo ^!ifdef DELLANGFILES >> "%langfile%"
for /f %%i in ('dir /b /A:D ..\make\wdsr*') do (
  for /f %%j in ('dir /b ..\make\%%i\.*.txt') do (
    call :GetLanguage %%i %%j "%langfile%" AppendLanguageFileDel
  )
)
echo ^!endif >> "%langfile%"
echo. >> "%langfile%"
echo.
echo Now creating the installer. Please wait, this may take a while ...
echo ------------------------------------------------------------------------------
"%nsis%" WDS.nsi > NUL
if EXIST "%langfile%" del /f /q "%langfile%"
if EXIST "%vernsi%" del /f /q "%vernsi%"
echo Finished!
echo.
goto :END
:ERROR
echo An error occurred:
echo -^> %err%
echo.
goto :END
:: ----------------------------------------------------------------------------
:: START of functions
:: ----------------------------------------------------------------------------
goto :EOF
:GetLanguage
setlocal & set RET=%2
set lang=%1
set langno=%lang:wdsr=%
set RET=%RET:.txt=%
set RET=%RET:~1%
call :%4
endlocal & set RET=%RET%
goto :EOF

:AppendLanguage
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  echo %RET%
  echo ; ----------------------------------------------- >> %langfile%
  if EXIST "%install%\wdsh%langno%.chm" (
    echo   Section "%RET% + help" %lang% >> %langfile%
  ) else (
    echo   Section "%RET%" %lang% >> %langfile%
  )
  echo     SectionIn 1 2 >> %langfile%
rem  echo     SetOutPath ^$INSTDIR >> %langfile%
  echo     File %install%\wdsr%langno%.dll >> %langfile%
  if EXIST "%install%\wdsh%langno%.chm" (
    echo     File %install%\wdsh%langno%.chm >> %langfile%
    echo     CreateShortCut "$SMPROGRAMS\WinDirStat\Help (%RET%).lnk" "$INSTDIR\wdsh%langno%.chm" >> %langfile%
  ) else (
    echo     ; No helpfile available: %install%\wdsh%langno%.chm >> %langfile%
  )
  echo   SectionEnd >> %langfile%
  echo. >> %langfile%
)
endlocal
goto :EOF

:AppendLanguageDesc
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  if EXIST "%install%\wdsh%langno%.chm" (
    echo ^!insertmacro MUI_DESCRIPTION_TEXT ^${%lang%} "Required for %RET% user interface support (includes help file)" >> %langfile%
  ) else (
    echo ^!insertmacro MUI_DESCRIPTION_TEXT ^${%lang%} "Required for %RET% user interface support" >> %langfile%
  )
)
endlocal
goto :EOF

:AppendLanguageFileDel
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  echo IfFileExists "$INSTDIR\wdsr%langno%.dll" 0 +2 >> %langfile%
  echo Delete "$INSTDIR\wdsr%langno%.dll" >> %langfile%
  if EXIST "%install%\wdsh%langno%.chm" (
    echo IfFileExists "$INSTDIR\wdsh%langno%.chm" 0 +2 >> %langfile%
    echo Delete "$INSTDIR\wdsh%langno%.chm" >> %langfile%
  )
)
endlocal
goto :EOF

:: ----------------------------------------------------------------------------
:: END of functions
:: ----------------------------------------------------------------------------
:END
if EXIST %install% rd /q /s %install%
pause

