@echo off
setlocal ENABLEEXTENSIONS & pushd .
call "%~dp0common\hgid.cmd"
"%~dp0common\premake4.exe" --release --resources vs2005
call %~dp0common\setvcvars.cmd 2005
echo %VCVER_FRIENDLY%
::popd & endlocal & goto :EOF
vcbuild.exe /time /rebuild /showenv /M1 /nologo ^
    "/htmllog:$(SolutionDir)wds_release\buildlog.html" "%~dp0\wds_release.vs8.sln" "$ALL"
:: msbuild.exe "%~dp0\setup\wds_setup.wixproj"
:: Sign the MSIs
:: Create NSIS wrapper
:: Create ZIP file?
popd & endlocal & goto :EOF
