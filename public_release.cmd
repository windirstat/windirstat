@echo off
setlocal ENABLEEXTENSIONS & pushd .
"%~dp0common\premake4.exe" --release vs2005
call %~dp0common\setvcvars.cmd 2005
call %~dp0common\buildinc.cmd "%~dp0common"
echo %VCVER_FRIENDLY%
vcbuild.exe /time /rebuild /showenv /M1 /nologo ^
    "/htmllog:$(SolutionDir)build\buildlog.html" "%~dp0\wds_release.vs8.sln" "$ALL"
popd & endlocal
goto :EOF
