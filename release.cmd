@echo off
echo.
echo Did you copy the 7z.exe into the current directory?
echo The version given at command line was: %1
echo If you believe everything is okay, hit ENTER now.
pause
7z a -tzip -r -y windirstat%1-exe-ansi.zip .\release\*.*
7z a -t7z -r -y windirstat%1-exe-ansi.7z .\release\*.*
7z a -tzip -r -y windirstat%1-exe-unicode.zip .\urelease\*.*
7z a -t7z -r -y windirstat%1-exe-unicode.7z .\urelease\*.*

