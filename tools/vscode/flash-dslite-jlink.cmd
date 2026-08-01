@echo off
setlocal

set "WORKSPACE=%~1"
set "DSLITE=%~2"

if "%WORKSPACE%"=="" set "WORKSPACE=%CD%"
if "%DSLITE%"=="" set "DSLITE=D:\APPs\TI\Unflsh\dslite.bat"

powershell.exe -NoProfile -ExecutionPolicy Bypass ^
    -File "%WORKSPACE%\tools\vscode\flash-dslite-jlink.ps1" ^
    -WorkspaceFolder "%WORKSPACE%" ^
    -Dslite "%DSLITE%" ^
    -TimeoutSeconds 60

exit /b %ERRORLEVEL%
