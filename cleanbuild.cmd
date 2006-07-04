@echo off
:: #######################################################################
:: ## Small NT script to delete all intermediate files.
:: ##
:: ## $Header$
:: #######################################################################
:: Change to the make directory
pushd .\make
:: Set all files visible and read/write
attrib -r -h -s *.* /S /D
:: Delete solution intermediate files
del /f /q /s *.ncb
del /f /q /s *.suo
del /f /q /s *.aps
del /f /q /s *.bak
del /f /q /s *.pdb
del /f /q /s *.old
del /f /q /s *.user
del /f /q .\common\linkcounter.exe
:: Delete all release subdirectories (the release of the binaries is not touched)
for /d %%i in (*) do @(
  rd /s /q ".\%%i\UnicodeRelease"
  rd /s /q ".\%%i\UnicodeDebug"
  rd /s /q ".\%%i\Release"
  rd /s /q ".\%%i\Debug"
)
:: Change back to the previous dir
popd
pushd .\dev-docs
del /f /q /s *.bak
del /f /q /s *.aux
del /f /q /s *.dvi
del /f /q /s *.log
del /f /q /s *.out
del /f /q /s *.pdf
del /f /q /s *.toc
del /f /q .\common\linkcounter.exe
popd
del /f /q /s BuildLog.htm
pause
