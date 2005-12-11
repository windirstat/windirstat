@echo off
:: ----------------------------------------------------------------------------
:: Script to create the NSIS installer from a prepared NSIS script
::
:: Requirements:
:: - NSIS must be installed in %ProgramFiles%\NSIS
:: - WDS.nsi must exist in the current directory (which has to be writable!)
::
:: This script will do this:
:: 0. Create temporary NSI scripts WDS_GetVersion.nsi and WDS_Languages.nsi
:: 1. Copy required files into %install%
:: 2. Create %chkver(A|U)% from WDS_GetVersion.nsi
:: 3. Call the created EXE file
:: 4. Parse the output in %verfile(A|U)% into %vernsi%
:: 5. Search for wdsr* subdirectories in the ..\make directory
:: 6. Extract the names from the .<language>.txt files located there
:: 7. Putting all language information into WDS_LanguagesTemp.nsi
:: ----------------------------------------------------------------------------
set install=.\Installer
set nsis=%ProgramFiles%\NSIS\makensis.exe
set verfileA=VersionA.txt
set verfileU=VersionU.txt
set vernsi=WDS_LangAnd%verfileA:A.txt=.nsi%
set chkverA=GetVersionWDS_A.exe
set chkverU=GetVersionWDS_U.exe
set chkfileA=%install%\windirstatA.exe
set chkfileU=%install%\windirstatU.exe
set langfile=WDS_LanguagesTemp.nsi
set langs=langs.txt
set langsexe=langs.exe
set getvernsi=WDS_GetVersion.nsi
set langsnsi=WDS_Languages.nsi
set wdsmain=WDS_Main.nsi
set err=Unknown error
if NOT EXIST %nsis% @(
  set err=Could not find NSIS compiler at "%nsis%"
  goto ERROR
)
:: ----------------------------------------------------------------------------
echo Creation of temporary scripts
echo ------------------------------------------------------------------------------
echo !define WDSMAIN_NSI > %wdsmain%
echo !define LANGANDVER ^"%vernsi%^" >> %wdsmain%
echo !define LANGTEMP ^"%langfile%^" >> %wdsmain%
type WDS.nsi >> %wdsmain%
echo !define LANGUAGES_NSI > %langsnsi%
type WDS.nsi >> %langsnsi%
echo !define GETVERSION_NSI > %getvernsi%
type WDS.nsi >> %getvernsi%
:: ----------------------------------------------------------------------------
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
echo Now retrieving the current (ANSI) version
echo ^(from "%install%\windirstatA.exe"^)
echo ------------------------------------------------------------------------------
if NOT EXIST %getvernsi% @(
  set err=The NSIS script file which retrieves the version could not be found. Make sure it is in the current directory and accessible!
  goto ERROR
)
"%nsis%" "/DInstallerOut=%chkverA%" "/DVersionOut=%verfileA%" "/DVersionOfFile=%chkfileA%" %getvernsi% > NUL
"%nsis%" "/DInstallerOut=%chkverU%" "/DVersionOut=%verfileU%" "/DVersionOfFile=%chkfileU%" %getvernsi% > NUL
:: Delete the old version file, if it exists
if EXIST "%verfileA%" del /f /q "%verfileA%"
if EXIST "%verfileU%" del /f /q "%verfileU%"
:: Try to call the EXE file created from the script
if EXIST "%chkverA%" @(
  %chkverA%
) else (
  set err=The program to retrieve the WDS ANSI version could not be found. Is the current directory read-only? Make sure it's not and try again.
  goto ERROR
)
if EXIST "%chkverU%" @(
  %chkverU%
) else (
  set err=The program to retrieve the WDS Unicode version could not be found. Is the current directory read-only? Make sure it's not and try again.
  goto ERROR
)
:: Check for existance of the version file
if EXIST "%verfileA%" @(
  echo Successfully retrieved ANSI file version:
) else (
  set err=Could not retrieve the file version of "%chkfileA%", please make sure the file is at its place and accessible.
  goto ERROR
)
if EXIST "%verfileU%" @(
  echo Successfully retrieved Unicode file version:
) else (
  set err=Could not retrieve the file version of "%chkfileU%", please make sure the file is at its place and accessible.
  goto ERROR
)
:: Delete the temporary EXE file
if EXIST "%chkverA%" del /f /q "%chkverA%"
if EXIST "%chkverU%" del /f /q "%chkverU%"
:: Split the version string and then create several defines
for /f "tokens=1,2,3,4 delims=." %%i in (%verfileA%) do (
  echo ANSI Version %%i.%%j.%%k ^(Build %%l^)
  echo ^!define sVersion_Ansi "%%i.%%j.%%k.%%l" > %vernsi%
  echo ^!define sVersionBuild_Ansi "%%l" >> %vernsi%
  echo ^!define dVersionBuild_Ansi %%l >> %vernsi%
  echo ^!define sVersionS "%%i.%%j.%%k" >> %vernsi%
  echo ^!define dVersionMajor %%i >> %vernsi%
  echo ^!define dVersionMinor %%j >> %vernsi%
  echo ^!define dVersionRev %%k >> %vernsi%
  echo ^!define sVersionFile "%%i_%%j_%%k" >> %vernsi%
  echo ^!define sVersionFull "WinDirStat %%i.%%j.%%k" >> %vernsi%
  echo ^!define Installer "%install%" >> %vernsi%
)
:: The same for the unicode executable
for /f "tokens=1,2,3,4 delims=." %%i in (%verfileU%) do (
  echo Unicode Version %%i.%%j.%%k ^(Build %%l^)
  echo ^!define sVersion_Unicode "%%i.%%j.%%k.%%l" >> %vernsi%
  echo ^!define sVersionBuild_Unicode "%%l" >> %vernsi%
  echo ^!define dVersionBuild_Unicode %%l >> %vernsi%
)
:: Delete the Version.txt
if EXIST "%verfileA%" del /f /q "%verfileA%"
if EXIST "%verfileU%" del /f /q "%verfileU%"
echo.
echo Going to collect information about available languages ...
echo ------------------------------------------------------------------------------
if EXIST "%langs%" del /f /q "%langs%"
:: Prepare language strings
for /f %%i in ('dir /b /A:D ..\make\wdsr*') do (
  for /f %%j in ('dir /b ..\make\%%i\.*.txt') do (
    call :GetLanguage %%i %%j "%langfile%" PrepareLanguageStrings
  )
)
:: Now the actual parsing of the languages
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
echo ^!ifdef LANGCURRLANG >> "%langfile%"
for /f %%i in ('dir /b /A:D ..\make\wdsr*') do (
  for /f %%j in ('dir /b ..\make\%%i\.*.txt') do (
    call :GetLanguage %%i %%j "%langfile%" AppendCurrentLangChoice
  )
)
echo ^!endif >> "%langfile%"
echo. >> "%langfile%"
:: Cleanup language strings
if EXIST "%langs%" del /f /q "%langs%"
echo.
echo Now creating the installer. Please wait, this may take a while ...
echo ------------------------------------------------------------------------------
"%nsis%" %wdsmain% > NUL
if EXIST "%langfile%" del /f /q "%langfile%"
if EXIST "%vernsi%" del /f /q "%vernsi%"
:: Cleanup temporary scripts
if EXIST "%getvernsi%" del /f /q "%getvernsi%"
if EXIST "%langsnsi%" del /f /q "%langsnsi%"
if EXIST "%wdsmain%" del /f /q "%wdsmain%"
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

:PrepareLanguageStrings
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  "%nsis%" "/DInstallerOut=%langsexe%" "/DLangOut=%vernsi%" /DLCID=0x%langno% /DLANGID=%langno% %langsnsi% > NUL
  %langsexe%
  if EXIST "%langsexe%" del /f /q "%langsexe%"
)
endlocal
goto :EOF

:AppendLanguage
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  echo %RET%
  echo ; ----------------------------------------------- >> %langfile%
  if EXIST "%install%\wdsh%langno%.chm" (
    echo   Section "${LangEngName%langno%} (${LangNatName%langno%}/${LangShoName%langno%}) + help" %lang% >> %langfile%
  ) else (
    echo   Section "${LangEngName%langno%} (${LangNatName%langno%}/${LangShoName%langno%})" %lang% >> %langfile%
  )
  echo     SectionIn 1 2 >> %langfile%
rem  echo     SetOutPath ^$INSTDIR >> %langfile%
  echo     File %install%\wdsr%langno%.dll >> %langfile%
  echo     ${SectionFlagIsSet} ${%lang%} ${SF_BOLD} 0 SkipRegSet%langno% >> %langfile%
  echo     WriteRegDWORD HKCU "${APPKEY}\options" "language" "0x%langno%" >> %langfile%
  echo    SkipRegSet%langno%: >> %langfile%
  if EXIST "%install%\wdsh%langno%.chm" (
    echo     File %install%\wdsh%langno%.chm >> %langfile%
    echo     CreateShortCut "$SMPROGRAMS\WinDirStat\Help (${LangEngName%langno%} - ${LangNatName%langno%}).lnk" "$INSTDIR\wdsh%langno%.chm" >> %langfile%
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
    echo ^!insertmacro MUI_DESCRIPTION_TEXT ^${%lang%} "Required for ${LangEngName%langno%} (${LangNatName%langno%}/${LangShoName%langno%}) user interface support (includes help file)" >> %langfile%
  ) else (
    echo ^!insertmacro MUI_DESCRIPTION_TEXT ^${%lang%} "Required for ${LangEngName%langno%} (${LangNatName%langno%}/${LangShoName%langno%}) user interface support" >> %langfile%
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

:AppendCurrentLangChoice
setlocal
if EXIST "%install%\wdsr%langno%.dll" (
  echo Push $0 >> %langfile%
  echo IntCmp $0 0x%langno% 0 Unselect%langno% Unselect%langno% >> %langfile%
  echo ${SetSectionFlag} ${%lang%} ${SF_BOLD} >> %langfile%
  echo ${SelectSection} ${%lang%} >> %langfile%
  echo Goto EndSel%langno% >> %langfile%
  echo Unselect%langno%: >> %langfile%
  echo ${ClearSectionFlag} ${%lang%} ${SF_BOLD} >> %langfile%
  echo ${UnselectSection} ${%lang%} >> %langfile%
  echo EndSel%langno%: >> %langfile%
  echo Pop $0 >> %langfile%

)
endlocal
goto :EOF

:: ----------------------------------------------------------------------------
:: END of functions
:: ----------------------------------------------------------------------------
:END
if EXIST %install% rd /q /s %install%
pause

