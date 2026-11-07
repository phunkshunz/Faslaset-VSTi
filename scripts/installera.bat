@echo off
rem Installerar MOTSTAND pa Windows. Dubbelklicka pa filen.
rem Installs MOTSTAND on Windows. Double-click this file.
rem Plugin-mappen for VST3 kraver administrator, sa filen ber om det sjalv.

net session >nul 2>&1
if errorlevel 1 (
  powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
  exit /b
)

cd /d "%~dp0"
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\MOTSTAND.vst3" rmdir /s /q "%DEST%\MOTSTAND.vst3"
xcopy "MOTSTAND.vst3" "%DEST%\MOTSTAND.vst3\" /e /i /y /q >nul

if errorlevel 1 (
  echo.
  echo Nagot gick fel. Packade du upp zip-filen forst? Hogerklicka och valj "Extrahera alla".
  echo Something went wrong. Did you unzip first? Right-click the zip and choose "Extract All".
  pause
  exit /b 1
)

echo.
echo Klart! / Done! MOTSTAND:
echo   %DEST%
echo Starta om din DAW, eller sok efter nya plugins.
echo Restart your DAW, or rescan plugins.
echo.
pause
