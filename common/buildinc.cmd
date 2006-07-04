@echo off
setlocal
set WORKPATH=%1
if {%WORKPATH%}=={} set WORKPATH=.
set BUILD=%WORKPATH%\BUILD
set outfile=%WORKPATH%\buildnumber.h
for /f "" %%i in ('type "%BUILD%"') do (
  set BUILDNR=%%i
  )
set /a BUILDNR=%BUILDNR%+1
echo New build number is %BUILDNR% ...
echo // Automatically created file! > %outfile%
echo #define LINKCOUNT %BUILDNR% >> %outfile%
echo. >> %outfile%
echo %BUILDNR% > %BUILD%
endlocal
