@echo off
setlocal ENABLEEXTENSIONS & pushd .
set TGTNAME=wds
set PRJNAME=%TGTNAME%_release
set SIGURL=https://windirstat.net
set SIGDESC=WinDirStat
premake4.exe --release vs2022
call "setvcvars.cmd" 2022
echo %VCVER_FRIENDLY%
vcbuild.exe /time /rebuild /showenv /M1 /nologo "/htmllog:$(SolutionDir)buildlog.html" "%~dp0%PRJNAME%.vs8.sln" "$ALL"
del /f %PRJNAME%\*.idb
rd /s /q "%~dp0%PRJNAME%_intermediate"
set SEVENZIP=%ProgramFiles%\7-Zip\7z.exe
if not exist "%SEVENZIP%" set SEVENZIP=%ProgramFiles(x86)%\7-Zip\7z.exe
for /f %%i in ('hg id -i') do @set RELEASE=%%i
for /f %%i in ('hg id -n') do @set RELEASE=%%i-%RELEASE%
set RELARCHIVE=%~dp0%TGTNAME%-%RELEASE%.7z
if exist "%SEVENZIP%" @(
    pushd "%~dp0%PRJNAME%"
    "%SEVENZIP%" a -y -t7z "%RELARCHIVE%" "*.exe" "*.pdb" "*.wdslng"
    gpg -ba %RELARCHIVE%
    popd
)
:: msbuild.exe "%~dp0\setup\wds_setup.wixproj"
:: Sign the MSIs
:: Create NSIS wrapper
popd & endlocal & goto :EOF
