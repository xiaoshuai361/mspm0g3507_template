param(
    [Parameter(Mandatory = $true)]
    [string]$WorkspaceFolder,

    [string]$SdkProduct = "",

    [string]$SysConfigRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$syscfg = Get-ChildItem -LiteralPath $WorkspaceFolder -Filter "*.syscfg" -File |
    Sort-Object Name |
    Select-Object -First 1

if ($null -eq $syscfg) {
    throw "No .syscfg file was found in workspace root: $WorkspaceFolder"
}

$sdkProduct = $SdkProduct
if ([string]::IsNullOrWhiteSpace($sdkProduct)) {
    $sdkProduct = "D:\APPs\TI\CCS\mspm0_sdk_2_07_00_05\.metadata\product.json"
}
if (-not (Test-Path -LiteralPath $sdkProduct -PathType Leaf)) {
    throw "MSPM0 SDK product.json was not found: $sdkProduct"
}

$candidateRoots = @(
    $SysConfigRoot,
    $env:SYSCONFIG_ROOT,
    "D:\APPs\TI\sysconfig_1.27.1",
    "D:\APPs\TI\CCS\ccs\utils\sysconfig_1.27.0",
    "D:\APPs\TI\CCS\ccs\utils\sysconfig_1.25.0",
    "C:\ti\sysconfig_1.27.1",
    "C:\ti\sysconfig_1.27.0",
    "D:\ti\sysconfig_1.27.1",
    "D:\ti\sysconfig_1.27.0"
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique

foreach ($root in $candidateRoots) {
    $nw = Join-Path $root "nw\nw.exe"
    $app = Join-Path $root "app"
    if ((Test-Path -LiteralPath $nw -PathType Leaf) -and
        (Test-Path -LiteralPath $app -PathType Container)) {
        Start-Process -FilePath $nw -ArgumentList @(
            $app,
            "--product",
            $sdkProduct,
            "--script",
            $syscfg.FullName
        ) -WorkingDirectory $root
        Write-Host "SysConfig GUI started: $nw"
        Write-Host "SysConfig file: $($syscfg.FullName)"
        exit 0
    }
}

throw @"
SysConfig GUI was not found.

This CCS installation provides sysconfig_cli.bat, but not the standalone SysConfig GUI entry:
  nw\nw.exe

To open SysConfig from VSCode, install standalone TI SysConfig or set user environment variable:
  SYSCONFIG_ROOT=<SysConfig install directory>

Expected standalone layout example:
  <DevEnv>\TI\sysconfig_1.27.1\nw\nw.exe
"@
