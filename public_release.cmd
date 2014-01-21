@echo off
setlocal ENABLEEXTENSIONS & pushd .
set HGTIPFILE="%~dp0common\hgtip.h"
for /f %%i in ('hg id -i -r tip') do @echo #define HG_TIP_ID "%%i" > %HGTIPFILE%
if exist %HGTIPFILE% type %HGTIPFILE%
"%~dp0common\premake4.exe" --release vs2005
call %~dp0common\setvcvars.cmd 2005
call %~dp0common\buildinc.cmd "%~dp0common"
echo %VCVER_FRIENDLY%
::popd & endlocal & goto :EOF
vcbuild.exe /time /rebuild /showenv /M1 /nologo ^
    "/htmllog:$(SolutionDir)wds_release\buildlog.html" "%~dp0\wds_release.vs8.sln" "$ALL"
:: msbuild.exe "%~dp0\setup\wds_setup.wixproj"
:: Sign the MSIs
:: Create NSIS wrapper
:: Create ZIP file?
popd & endlocal & goto :EOF
