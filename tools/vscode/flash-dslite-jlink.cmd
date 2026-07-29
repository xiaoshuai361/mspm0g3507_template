@echo off
setlocal

set "WORKSPACE=%~1"
set "DSLITE=%~2"

if "%WORKSPACE%"=="" set "WORKSPACE=%CD%"
if "%DSLITE%"=="" set "DSLITE=D:\ti\CCS\ccs\ccs_base\DebugServer\bin\DSLite.exe"

set "CCXML=%WORKSPACE%\targetConfigs\MSPM0G3507.ccxml"
set "PROGRAM=%WORKSPACE%\Debug\cy_template.out"
set "MAPFILE=%WORKSPACE%\Debug\cy_template.map"

if not exist "%DSLITE%" (
    echo DSLite not found: %DSLITE%
    exit /b 1
)

if not exist "%CCXML%" (
    echo Target config not found: %CCXML%
    exit /b 1
)

if not exist "%PROGRAM%" (
    echo Program file not found: %PROGRAM%
    echo Run "Build: CCS Debug" first.
    exit /b 1
)

echo [flash] Probe: J-Link via CCS DSLite
echo [flash] Target config: %CCXML%
echo [flash] Program: %PROGRAM%
if exist "%MAPFILE%" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%WORKSPACE%\tools\vscode\show-memory-usage.ps1" -MapFile "%MAPFILE%"
)
echo.

"%DSLITE%" flash --config="%CCXML%" -e -r 2 -u "%PROGRAM%"

exit /b %ERRORLEVEL%
