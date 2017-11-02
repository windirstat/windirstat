@echo off
setlocal ENABLEEXTENSIONS & pushd .
set HGTIPFILE="%~dp0hgid.h"
echo.> %HGTIPFILE%
for /f %%i in ('hg id -i') do @echo #define HG_REV_ID "%%i">> %HGTIPFILE%
for /f %%i in ('hg id -n') do @echo #define HG_REV_NO %%i>>   %HGTIPFILE%
for /f %%i in ('hg id -n') do @echo #define HG_REV_NO_NUMERIC %%i -0>>   %HGTIPFILE%
for /f %%i in ('hg log -l 1 -r . -T {node}') do @echo #define HG_FULLID "%%i">> %HGTIPFILE%
if exist %HGTIPFILE% type %HGTIPFILE%
popd & endlocal & goto :EOF
