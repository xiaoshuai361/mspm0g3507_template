param(
    [Parameter(Mandatory = $true)]
    [string]$WorkspaceFolder,

    [string]$Dslite = "D:\ti\CCS\ccs\ccs_base\DebugServer\bin\DSLite.exe",

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ccxml = Join-Path $WorkspaceFolder "targetConfigs\MSPM0G3507.ccxml"
$program = Join-Path $WorkspaceFolder "Debug\cy_template.out"
$mapFile = Join-Path $WorkspaceFolder "Debug\cy_template.map"

foreach ($requiredFile in @($Dslite, $ccxml, $program)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required file not found: $requiredFile"
    }
}

Write-Host "[flash] Probe: J-Link via CCS DSLite"
Write-Host "[flash] Target config: $ccxml"
Write-Host "[flash] Program: $program"
if (Test-Path -LiteralPath $mapFile -PathType Leaf) {
    & (Join-Path $WorkspaceFolder "tools\vscode\show-memory-usage.ps1") -MapFile $mapFile
}
Write-Host ""

$arguments = 'flash --config="{0}" -e -r 2 -u "{1}"' -f $ccxml, $program
$stdoutFile = New-TemporaryFile
$stderrFile = New-TemporaryFile
$process = Start-Process -FilePath $Dslite -ArgumentList $arguments `
    -NoNewWindow -PassThru `
    -RedirectStandardOutput $stdoutFile.FullName `
    -RedirectStandardError $stderrFile.FullName

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host "DSLite timed out after $TimeoutSeconds seconds. Terminating this flash process tree."
    & taskkill.exe /PID $process.Id /T /F | Out-Host
    Get-Content -LiteralPath $stdoutFile.FullName | Out-Host
    Get-Content -LiteralPath $stderrFile.FullName | Out-Host
    Remove-Item -LiteralPath $stdoutFile.FullName, $stderrFile.FullName -Force
    exit 124
}

$process.WaitForExit()
$process.Refresh()
$stdout = Get-Content -LiteralPath $stdoutFile.FullName -Raw
$stderr = Get-Content -LiteralPath $stderrFile.FullName -Raw
$stdout | Write-Host -NoNewline
$stderr | Write-Host -NoNewline
Remove-Item -LiteralPath $stdoutFile.FullName, $stderrFile.FullName -Force

if (($stdout -match '(?im)^(error:|Failed:)') -or
    ($stderr -match '(?im)^(error:|Failed:)')) {
    exit 1
}
if (($stdout -match '(?im)^Success\s*$') -or
    ($stderr -match '(?im)^Success\s*$')) {
    exit 0
}

exit $process.ExitCode
