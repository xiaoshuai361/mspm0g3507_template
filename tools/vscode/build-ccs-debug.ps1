param(
    [Parameter(Mandatory = $true)]
    [string]$WorkspaceFolder,

    [string]$Gmake = "D:\ti\CCS\ccs\utils\bin\gmake.exe",

    [int]$Jobs = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

function Format-Size {
    param([UInt64]$Bytes)

    if ($Bytes -ge 1MB -and ($Bytes % 1MB) -eq 0) {
        return ("{0} MB" -f ($Bytes / 1MB))
    }
    if ($Bytes -ge 1KB -and ($Bytes % 1KB) -eq 0) {
        return ("{0} KB" -f ($Bytes / 1KB))
    }
    if ($Bytes -ge 1KB) {
        return ("{0:N1} KB" -f ($Bytes / 1KB))
    }
    return ("{0} B" -f $Bytes)
}

function Show-MemoryUsage {
    param([string]$MapFile)

    if (-not (Test-Path -LiteralPath $MapFile -PathType Leaf)) {
        Write-Host "[build] Memory usage skipped: map file not found: $MapFile"
        return
    }

    $regions = New-Object System.Collections.Generic.List[object]
    foreach ($line in Get-Content -LiteralPath $MapFile) {
        if ($line -match '^\s*(FLASH|SRAM|BCR_CONFIG|BSL_CONFIG)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)') {
            $name = $matches[1]
            $size = [Convert]::ToUInt64($matches[3], 16)
            $used = [Convert]::ToUInt64($matches[4], 16)
            $percent = 0.0
            if ($size -gt 0) {
                $percent = [Math]::Round(($used * 100.0) / $size, 2)
            }
            $regions.Add([PSCustomObject]@{
                Name    = $name
                Used    = $used
                Size    = $size
                Percent = $percent
            }) | Out-Null
        }
    }

    if ($regions.Count -eq 0) {
        Write-Host "[build] Memory usage skipped: no known memory regions found in map file."
        return
    }

    Write-Host ""
    Write-Host "Memory region       Used Size   Region Size   %age Used"
    foreach ($region in $regions) {
        Write-Host ("{0,-16} {1,10}   {2,11}   {3,8:N2}%" -f `
            ($region.Name + ":"), `
            (Format-Size $region.Used), `
            (Format-Size $region.Size), `
            $region.Percent)
    }
}

$projectName = Split-Path -Leaf $WorkspaceFolder
$projectFile = Join-Path $WorkspaceFolder ".project"
if (Test-Path -LiteralPath $projectFile -PathType Leaf) {
    [xml]$projectMetadata = Get-Content -LiteralPath $projectFile -Raw
    $configuredProjectName = [string]$projectMetadata.projectDescription.name
    if (-not [string]::IsNullOrWhiteSpace($configuredProjectName)) {
        $projectName = $configuredProjectName
    }
}
$debugDir = Join-Path $WorkspaceFolder "Debug"
$program = Join-Path $debugDir "$projectName.out"
$mapFile = Join-Path $debugDir "$projectName.map"

if (-not (Test-Path -LiteralPath $Gmake -PathType Leaf)) {
    throw "gmake not found: $Gmake"
}

if (-not (Test-Path -LiteralPath $debugDir -PathType Container)) {
    throw "Debug build directory not found: $debugDir"
}

Write-Host "[main] Project: $projectName"
Write-Host "[main] Build type: CCS Debug / TI Arm Clang"
Write-Host "[proc] Running command: $Gmake -C $debugDir all -j$Jobs"
Write-Host "[build] Output file: $program"
Write-Host ""

& $Gmake -C $debugDir all "-j$Jobs"
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Host ""
    Write-Host "[build] Build Failed. Exit code: $exitCode"
    exit $exitCode
}

Show-MemoryUsage -MapFile $mapFile

Write-Host ""
Write-Host "[build] Build Success: $program"
exit 0
