@echo off
:: #######################################################################
:: ## Small NT script build all the HTML help files at once or clean them
:: ##
:: ## $Id$
:: #######################################################################
pushd .\
setlocal
for /f "tokens=*" %%j in ('cd') do @set CURRDIR=%%j
set HELPDIR=..\..\help
set ACTION=%1
set HELPPROJECT=windirstat.hhp

if /i {%ACTION%}=={BUILD} goto :buildhelp
if /i {%ACTION%}=={CLEAN} goto :cleanhelp
goto :wrongparam

:buildhelp
echo Building help files
:: Create the directory for help files
if not exist "%HELPDIR%" md "%HELPDIR%"
:: Tell the HTML Help Compiler to do its job
for /d %%i in (*) do @(
  if exist "%CURRDIR%\%%i\%HELPPROJECT%" echo. & echo .\%%i\%HELPPROJECT% & hhc "%CURRDIR%\%%i\%HELPPROJECT%" || cd .
)
:: Return
set errorlevel=0
goto :endofscript

:cleanhelp
echo Cleaning help files
:: Delete the HTML help files
for /f %%i in ('dir /b "%HELPDIR%\*.chm"') do @(
  echo.
  echo Deleting %%i
  del /f /q "%HELPDIR%\%%i"
)
del /f /q "%HELPDIR%\BuildLog.htm"
:: Return
set errorlevel=0
goto :endofscript

:wrongparam
:: Tell the developer something went wrong
echo Please check the parameters. Something is wrong, valid options are ^"build^" and ^"clean^"
set errorlevel=1

:endofscript
endlocal
popd
