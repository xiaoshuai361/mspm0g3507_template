@echo off
setlocal

set "WORKSPACE=%~1"
set "SYSCONFIG_CLI=%~2"
set "SDK_PRODUCT=%~3"
set "OUT_DIR=%~4"

if "%WORKSPACE%"=="" set "WORKSPACE=%CD%"
if "%SYSCONFIG_CLI%"=="" set "SYSCONFIG_CLI=D:\APPs\TI\CCS\ccs\utils\sysconfig_1.27.0\sysconfig_cli.bat"
if "%SDK_PRODUCT%"=="" set "SDK_PRODUCT=D:\APPs\TI\CCS\mspm0_sdk_2_07_00_05\.metadata\product.json"
if "%OUT_DIR%"=="" set "OUT_DIR=%WORKSPACE%\Debug\syscfg"

set "SYSCFG=%WORKSPACE%\empty.syscfg"

if not exist "%SYSCONFIG_CLI%" (
    echo SysConfig CLI not found: %SYSCONFIG_CLI%
    exit /b 1
)

if not exist "%SDK_PRODUCT%" (
    echo MSPM0 SDK product.json not found: %SDK_PRODUCT%
    exit /b 1
)

if not exist "%SYSCFG%" (
    echo SysConfig file not found: %SYSCFG%
    exit /b 1
)

"%SYSCONFIG_CLI%" --product "%SDK_PRODUCT%" --output "%OUT_DIR%" "%SYSCFG%"
exit /b %ERRORLEVEL%
