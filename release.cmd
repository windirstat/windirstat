@echo off
setlocal ENABLEEXTENSIONS & pushd .
call %~dp0common\setvcvars.cmd 8.0
echo %VCVER_FRIENDLY%
vcbuild.exe /time /rebuild /nologo "/htmllog:$(SolutionDir)\buildlog32.html" "%~dp0\windirstat.vs2005.sln" "Release|Win32"
vcbuild.exe /time /rebuild /nologo "/htmllog:$(SolutionDir)\buildlog64.html" "%~dp0\windirstat.vs2005.sln" "Release|x64"
popd & endlocal
goto :EOF
