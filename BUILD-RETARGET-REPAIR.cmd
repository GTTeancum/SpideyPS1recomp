@echo off
setlocal
cd /d "%~dp0"
call BUILD-ISSUES-REPAIR.cmd 1
exit /b %errorlevel%
