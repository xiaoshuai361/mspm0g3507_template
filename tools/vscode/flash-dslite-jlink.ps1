param(
    [Parameter(Mandatory = $true)]
    [string]$WorkspaceFolder,

    [string]$Dslite = "D:\APPs\TI\Unflsh\dslite.bat",

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ccxml = Join-Path $WorkspaceFolder "tools\vscode\MSPM0G3507-jlink.ccxml"
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

$isBatchWrapper = [System.IO.Path]::GetExtension($Dslite) -ieq ".bat"
$dsliteArguments = @(
    "--config=`"$ccxml`"",
    "-e",
    "-r",
    "2",
    "-u",
    "`"$program`""
)
if (-not $isBatchWrapper) {
    # UniFlash 的 dslite.bat 已补上 flash；原生 DSLite.exe 则需要显式补上。
    $dsliteArguments = @("flash") + $dsliteArguments
}
$stdoutFile = [System.IO.Path]::GetTempFileName()
$stderrFile = [System.IO.Path]::GetTempFileName()
$process = Start-Process -FilePath $Dslite -ArgumentList $dsliteArguments `
    -NoNewWindow -PassThru `
    -RedirectStandardOutput $stdoutFile `
    -RedirectStandardError $stderrFile

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host "DSLite timed out after $TimeoutSeconds seconds. Terminating this flash process tree."
    & taskkill.exe /PID $process.Id /T /F | Out-Host
    Get-Content -LiteralPath $stdoutFile | Out-Host
    Get-Content -LiteralPath $stderrFile | Out-Host
    Remove-Item -LiteralPath $stdoutFile, $stderrFile -Force
    exit 124
}

$process.WaitForExit()
$process.Refresh()
$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$stdout | Write-Host -NoNewline
$stderr | Write-Host -NoNewline
Remove-Item -LiteralPath $stdoutFile, $stderrFile -Force

if ((($stdout -match '(?im)^Success\s*$') -or
     ($stderr -match '(?im)^Success\s*$')) -and
    ($process.ExitCode -eq 0)) {
    exit 0
}
if (($stdout -match '(?im)^(error:|Failed:)') -or
    ($stderr -match '(?im)^(error:|Failed:)')) {
    exit 1
}

exit $process.ExitCode
