@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "tools\porting\Build-Windows-Repair.ps1" -Game "%~1"
set "result=%errorlevel%"
echo Processing complete. Press any key to close the application.
pause >nul
exit /b %result%
