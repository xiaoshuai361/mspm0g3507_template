param(
    [Parameter(Mandatory = $true)]
    [string]$MapFile
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

if (-not (Test-Path -LiteralPath $MapFile -PathType Leaf)) {
    Write-Host "[build] Memory usage skipped: map file not found: $MapFile"
    exit 0
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
    exit 0
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
