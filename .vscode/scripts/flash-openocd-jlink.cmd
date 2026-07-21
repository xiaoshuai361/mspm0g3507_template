@echo off
setlocal

set "WORKSPACE=%~1"
set "OPENOCD=%~2"
set "OPENOCD_SCRIPTS=%~3"
set "ADAPTER_SPEED=%~4"

if "%WORKSPACE%"=="" set "WORKSPACE=%CD%"
if "%OPENOCD%"=="" set "OPENOCD=F:\APPS\openocd\xpack-openocd-0.12.0-7\bin\openocd.exe"
if "%OPENOCD_SCRIPTS%"=="" set "OPENOCD_SCRIPTS=F:\APPS\openocd\xpack-openocd-0.12.0-7\openocd\scripts"
if "%ADAPTER_SPEED%"=="" set "ADAPTER_SPEED=40000"

set "PROGRAM=%WORKSPACE%\Debug\cy_template.out"

if not exist "%OPENOCD%" (
    echo OpenOCD not found: %OPENOCD%
    exit /b 1
)

if not exist "%PROGRAM%" (
    echo Program file not found: %PROGRAM%
    echo Run "Build: CCS Debug" first.
    exit /b 1
)

"%OPENOCD%" ^
    -s "%OPENOCD_SCRIPTS%" ^
    -f interface/jlink.cfg ^
    -c "transport select swd" ^
    -c "adapter speed %ADAPTER_SPEED%" ^
    -f target/ti_mspm0.cfg ^
    -c "init; reset init; flash write_image erase {%PROGRAM%}; verify_image {%PROGRAM%}; reset run; shutdown"

exit /b %ERRORLEVEL%
