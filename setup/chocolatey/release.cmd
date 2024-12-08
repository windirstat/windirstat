@ECHO OFF
TITLE Publishing WinDirStat
CD /D "%~dp0"

:: ensure chocolatey is up to date
choco upgrade chocolatey -y

:: ask for api key
SET /p APIKEY=Enter API Key: 
IF /I "%APIKEY%" NEQ "" (
   choco apikey -k %APIKEY% -source https://push.chocolatey.org/
)

:: publish version
choco pack
choco push -s https://push.chocolatey.org/

PAUSE