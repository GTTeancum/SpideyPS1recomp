@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\porting\Build-Linux.ps1" %*
set "result=%errorlevel%"
echo Processing complete. Press any key to close the application.
pause >nul
exit /b %result%
