@echo off
setlocal ENABLEEXTENSIONS & pushd .
call %~dp0common\setvcvars.cmd 2005
call %~dp0common\buildinc.cmd "%~dp0common"
echo %VCVER_FRIENDLY%
vcbuild.exe /time /rebuild /showenv /M4 /nologo ^
    "/htmllog:$(SolutionDir)build\buildlog32.html" "%~dp0\windirstat.vs8.sln" "Release|Win32"
vcbuild.exe /time /rebuild /showenv /M4 /nologo ^
    "/htmllog:$(SolutionDir)build\buildlog64.html" "%~dp0\windirstat.vs8.sln" "Release|x64"
popd & endlocal
goto :EOF
