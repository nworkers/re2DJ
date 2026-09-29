@echo off
setlocal

rem Configure, build, and test the Win32 (x86) Release runtime into
rem build\windows-x86\bin\Release. Pass -SkipTests to build without CTest.
set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build_release.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"

endlocal & exit /b %EXIT_CODE%
