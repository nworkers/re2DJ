@echo off
setlocal

rem Configure and build the Win32 (x86) Debug runtime into build\windows-x86\bin\Debug.
rem Extra PowerShell parameters are passed through to build.ps1.
set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Configuration Debug %*
set "EXIT_CODE=%ERRORLEVEL%"

endlocal & exit /b %EXIT_CODE%
