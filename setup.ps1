param(
    [switch]$InstallMissing,
    [switch]$SkipExtensions,
    [switch]$NoBuildVerify,
    [switch]$NoWrite
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$VscodeDir = Join-Path $ProjectRoot ".vscode"
$ScriptsDir = Join-Path $ProjectRoot "tools\vscode"
$LocalEnvPath = Join-Path $VscodeDir "local.env.json"
$InstallersDir = Join-Path $ProjectRoot "installers"

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "== $Text =="
}

function Write-Ok {
    param([string]$Text)
    Write-Host "[OK] $Text"
}

function Write-Warn {
    param([string]$Text)
    Write-Host "[WARN] $Text"
}

function Write-Fail {
    param([string]$Text)
    Write-Host "[MISS] $Text"
}

function Resolve-ExistingPath {
    param([string[]]$Patterns, [switch]$Directory)

    $found = New-Object System.Collections.Generic.List[string]
    foreach ($pattern in $Patterns) {
        if ([string]::IsNullOrWhiteSpace($pattern)) { continue }
        try {
            $items = Resolve-Path -Path $pattern -ErrorAction SilentlyContinue
            foreach ($item in $items) {
                $path = $item.Path
                if ($Directory) {
                    if (Test-Path -LiteralPath $path -PathType Container) {
                        $found.Add($path) | Out-Null
                    }
                } else {
                    if (Test-Path -LiteralPath $path -PathType Leaf) {
                        $found.Add($path) | Out-Null
                    }
                }
            }
        } catch {
            continue
        }
    }

    return $found | Select-Object -Unique
}

function First-ExistingPath {
    param([string[]]$Patterns, [switch]$Directory)
    $all = Resolve-ExistingPath -Patterns $Patterns -Directory:$Directory
    return ($all | Select-Object -First 1)
}

function To-VsPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    return ($Path -replace "\\", "/")
}

function Parent-Dir {
    param([string]$Path, [int]$Levels = 1)
    $p = $Path
    for ($i = 0; $i -lt $Levels; $i++) {
        if ([string]::IsNullOrWhiteSpace($p)) { return "" }
        $p = Split-Path -Parent $p
    }
    return $p
}

function Write-JsonFile {
    param([string]$Path, [object]$Object)
    $json = $Object | ConvertTo-Json -Depth 30
    Set-Content -LiteralPath $Path -Value $json -Encoding UTF8
}

function Get-CodeCommand {
    $cmd = Get-Command code.cmd -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $cmd = Get-Command code.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $cmd = Get-Command code -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return ""
}

function Show-Tool {
    param([string]$Name, [string]$Path, [switch]$Optional)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        if ($Optional) { Write-Warn "$Name not found (optional)" } else { Write-Fail "$Name not found" }
    } else {
        Write-Ok "${Name}: $Path"
    }
}

function Ask-YesNo {
    param([string]$Question, [bool]$Default = $false)
    if ($InstallMissing) { return $true }

    $suffix = if ($Default) { " [Y/n]" } else { " [y/N]" }
    while ($true) {
        $answer = Read-Host ($Question + $suffix)
        if ([string]::IsNullOrWhiteSpace($answer)) { return $Default }
        switch -Regex ($answer.Trim()) {
            '^(?i:y|yes)$' { return $true }
            '^(?i:n|no)$' { return $false }
            default { Write-Host "Please answer y or n." }
        }
    }
}

function Find-Installer {
    param([string[]]$Patterns)
    $searchRoots = @(
        $InstallersDir,
        (Join-Path $ProjectRoot "tools\installers"),
        "E:\code\vscode_for_m0\installer\DevEnv",
        "E:\code\vscode_for_m0\installer",
        "$env:USERPROFILE\Downloads"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Container) }

    foreach ($root in $searchRoots) {
        foreach ($pattern in $Patterns) {
            $hit = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue |
                Sort-Object Length -Descending |
                Select-Object -First 1
            if ($hit) { return $hit.FullName }
        }
    }
    return ""
}

function Get-ProjectSdkProduct {
    $projectHints = @(
        (Join-Path $ProjectRoot "Debug\ccsIncludes.opt"),
        (Join-Path $ProjectRoot ".cproject"),
        (Join-Path $ProjectRoot "empty.syscfg")
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }

    foreach ($file in $projectHints) {
        $text = Get-Content -Raw -LiteralPath $file -ErrorAction SilentlyContinue
        if ($text -match '([A-Za-z]:[\\/][^"`\s<>]*mspm0_sdk_\d+_\d+_\d+_\d+)') {
            $sdkRoot = ($matches[1] -replace '/', '\')
            $product = Join-Path $sdkRoot ".metadata\product.json"
            if (Test-Path -LiteralPath $product -PathType Leaf) {
                return $product
            }
        }
        if ($text -match 'MSPM0-SDK:(\d+)\.(\d+)\.(\d+)\.(\d+)') {
            $sdkDir = "mspm0_sdk_{0}_{1}_{2}_{3}" -f $matches[1], $matches[2].PadLeft(2, '0'), $matches[3].PadLeft(2, '0'), $matches[4].PadLeft(2, '0')
            $candidates = Resolve-ExistingPath -Patterns @(
                "D:\APPs\TI\CCS\$sdkDir\.metadata\product.json",
                "D:\APPs\TI\$sdkDir\.metadata\product.json",
                "D:\ti\$sdkDir\.metadata\product.json",
                "C:\ti\$sdkDir\.metadata\product.json"
            )
            $hit = $candidates | Select-Object -First 1
            if ($hit) { return $hit }
        }
    }
    return ""
}

function Start-DownloadPage {
    param([string]$Url)
    try {
        Start-Process $Url | Out-Null
    } catch {
        Write-Warn "Could not open browser: $Url"
    }
}

function Install-SysConfigIfMissing {
    param([object]$Tools)
    if ($Tools.sysconfigGuiRoot -and $Tools.sysconfigCli) { return $false }

    if (-not (Ask-YesNo "SysConfig is missing or incomplete. Install standalone SysConfig automatically?")) {
        return $false
    }

    $installer = Find-Installer -Patterns @("sysconfig-*-setup.exe", "sysconfig_*setup*.exe")
    if (-not $installer) {
        Write-Warn "SysConfig installer not found."
        Write-Host "Put sysconfig-*-setup.exe under: $InstallersDir"
        Start-DownloadPage "https://www.ti.com/tool/SYSCONFIG"
        return $false
    }

    $prefix = "D:\APPs\TI\sysconfig_1.27.1"
    Write-Host "[proc] Installing SysConfig: $installer"
    Write-Host "[proc] Target: $prefix"
    & $installer --mode unattended --unattendedmodeui none --prefix $prefix
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "SysConfig installer exited with code $LASTEXITCODE"
        return $false
    }
    Write-Ok "SysConfig installed."
    return $true
}

function Install-DsliteIfMissing {
    param([object]$Tools)
    if ($Tools.dslite) { return $false }

    if (-not (Ask-YesNo "UniFlash/DSLite is missing. Open/install UniFlash now?")) {
        return $false
    }

    $installer = Find-Installer -Patterns @("*uniflash*.exe", "*UniFlash*.exe", "*uniflash*.msi")
    if ($installer) {
        Write-Host "[proc] Starting UniFlash installer: $installer"
        Start-Process -FilePath $installer -Wait
        return $true
    }

    Write-Warn "UniFlash installer not found."
    Write-Host "Put UniFlash installer under: $InstallersDir"
    Start-DownloadPage "https://www.ti.com/tool/UNIFLASH"
    return $false
}

function Install-CcsOrSdkIfMissing {
    param([object]$Tools)
    if ($Tools.gmake -and $Tools.compiler -and $Tools.sdkProduct) { return $false }

    if (-not (Ask-YesNo "CCS / TI Arm Clang / MSPM0 SDK is missing. Open/install TI tools now?")) {
        return $false
    }

    $ccsInstaller = Find-Installer -Patterns @("ccs_setup*.exe", "*CCS*setup*.exe", "Code_Composer_Studio*.exe")
    if ($ccsInstaller) {
        Write-Host "[proc] Starting CCS installer: $ccsInstaller"
        Start-Process -FilePath $ccsInstaller -Wait
    } else {
        Write-Warn "CCS installer not found."
        Write-Host "Put CCS installer under: $InstallersDir"
        Start-DownloadPage "https://www.ti.com/tool/CCSTUDIO"
    }

    $sdkInstaller = Find-Installer -Patterns @("mspm0_sdk*.exe", "MSPM0_SDK*.exe", "*mspm0*setup*.exe")
    if ($sdkInstaller) {
        Write-Host "[proc] Starting MSPM0 SDK installer: $sdkInstaller"
        Start-Process -FilePath $sdkInstaller -Wait
    } else {
        Write-Warn "MSPM0 SDK installer not found."
        Write-Host "Put MSPM0 SDK installer under: $InstallersDir"
        Start-DownloadPage "https://www.ti.com/tool/MSPM0-SDK"
    }

    return $true
}

function Install-JLinkIfRequested {
    $jlinkPresent = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object { $_.FriendlyName -match 'J-Link|SEGGER' -or $_.InstanceId -match 'VID_1366|SEGGER|JLINK' } |
        Select-Object -First 1
    if ($jlinkPresent) {
        Write-Ok "J-Link driver/device detected: $($jlinkPresent.FriendlyName)"
        return $false
    }

    $jlinkSoftware = First-ExistingPath -Directory -Patterns @(
        "C:\Program Files\SEGGER\JLink",
        "C:\Program Files (x86)\SEGGER\JLink",
        "D:\APPs\SEGGER\JLink",
        "F:\APPS\SEGGER\JLink"
    )
    if ($jlinkSoftware) {
        Write-Ok "J-Link software detected: $jlinkSoftware"
        return $false
    }

    $uninstallRoots = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    $jlinkPackage = Get-ItemProperty -Path $uninstallRoots -ErrorAction SilentlyContinue |
        Where-Object {
            $displayNameProperty = $_.PSObject.Properties["DisplayName"]
            $installLocationProperty = $_.PSObject.Properties["InstallLocation"]
            if (-not $displayNameProperty -or -not $installLocationProperty) {
                return $false
            }

            $displayName = [string]$displayNameProperty.Value
            $installLocation = [string]$installLocationProperty.Value
            $displayName -match '^(SEGGER\s+)?J-Link\b' -and
            -not [string]::IsNullOrWhiteSpace($installLocation) -and
            (Test-Path -LiteralPath $installLocation -PathType Container)
        } |
        Select-Object -First 1
    if ($jlinkPackage) {
        Write-Ok "J-Link software detected: $($jlinkPackage.InstallLocation)"
        return $false
    }

    if (-not (Ask-YesNo "J-Link driver/device was not detected. Open/install SEGGER J-Link software now?")) {
        return $false
    }

    $installer = Find-Installer -Patterns @("JLink*_Windows*.exe", "JLink*.exe", "*SEGGER*JLink*.exe")
    if ($installer) {
        Write-Host "[proc] Starting J-Link installer: $installer"
        Start-Process -FilePath $installer -Wait
        return $true
    }

    Write-Warn "J-Link installer not found."
    Write-Host "Put SEGGER J-Link installer under: $InstallersDir"
    Start-DownloadPage "https://www.segger.com/downloads/jlink/"
    return $false
}

function Offer-MissingSoftwareInstall {
    param([object]$Tools)
    $changed = $false
    if (Install-CcsOrSdkIfMissing -Tools $Tools) { $changed = $true }
    if (Install-SysConfigIfMissing -Tools $Tools) { $changed = $true }
    if (Install-DsliteIfMissing -Tools $Tools) { $changed = $true }
    if (Install-JLinkIfRequested) { $changed = $true }
    return $changed
}

function Find-Tools {
    $ccsRoots = Resolve-ExistingPath -Directory -Patterns @(
        $env:CCS_ROOT,
        "D:\APPs\TI\CCS\ccs",
        "D:\APPs\TI\CCS*\ccs",
        "D:\ti\ccs*\ccs",
        "C:\ti\ccs*\ccs",
        "C:\Program Files\Texas Instruments\ccs*\ccs",
        "C:\Program Files (x86)\Texas Instruments\ccs*\ccs"
    )

    $gmakeCandidates = New-Object System.Collections.Generic.List[string]
    foreach ($root in $ccsRoots) { $gmakeCandidates.Add((Join-Path $root "utils\bin\gmake.exe")) | Out-Null }
    $gmakeCandidates.Add("D:\APPs\TI\CCS\ccs\utils\bin\gmake.exe") | Out-Null
    $gmake = First-ExistingPath -Patterns $gmakeCandidates.ToArray()

    if (-not $ccsRoots -and $gmake) {
        $ccsRoots = @((Parent-Dir $gmake 3))
    }

    $compilerCandidates = New-Object System.Collections.Generic.List[string]
    $compilerCandidates.Add("D:\APPs\TI\CCS\ccs\tools\compiler\ti-cgt-armllvm_4.0.4.LTS\bin\tiarmclang.exe") | Out-Null
    foreach ($root in $ccsRoots) {
        $compilerCandidates.Add((Join-Path $root "tools\compiler\ti-cgt-armllvm_*\bin\tiarmclang.exe")) | Out-Null
    }
    $compiler = (Resolve-ExistingPath -Patterns $compilerCandidates.ToArray() | Sort-Object -Descending | Select-Object -First 1)

    $projectSdkProduct = Get-ProjectSdkProduct
    $sdkProductCandidates = New-Object System.Collections.Generic.List[string]
    if ($projectSdkProduct) { $sdkProductCandidates.Add($projectSdkProduct) | Out-Null }
    $sdkProductCandidates.Add("D:\APPs\TI\CCS\mspm0_sdk_2_07_00_05\.metadata\product.json") | Out-Null
    $sdkProductCandidates.Add("D:\APPs\TI\CCS\mspm0_sdk_*\.metadata\product.json") | Out-Null
    $sdkProductCandidates.Add("D:\APPs\TI\mspm0_sdk_*\.metadata\product.json") | Out-Null
    $sdkProductCandidates.Add("D:\ti\mspm0_sdk_*\.metadata\product.json") | Out-Null
    $sdkProductCandidates.Add("C:\ti\mspm0_sdk_*\.metadata\product.json") | Out-Null
    foreach ($root in $ccsRoots) {
        $tiRoot = Parent-Dir $root 1
        if ($tiRoot) { $sdkProductCandidates.Add((Join-Path $tiRoot "mspm0_sdk_*\.metadata\product.json")) | Out-Null }
    }
    if ($projectSdkProduct) {
        $sdkProduct = $projectSdkProduct
    } else {
        $sdkProduct = (Resolve-ExistingPath -Patterns $sdkProductCandidates.ToArray() | Sort-Object -Descending | Select-Object -First 1)
    }
    $sdkRoot = ""
    if ($sdkProduct) { $sdkRoot = Parent-Dir $sdkProduct 2 }

    $sysconfigRoots = Resolve-ExistingPath -Directory -Patterns @(
        $env:SYSCONFIG_ROOT,
        "D:\APPs\TI\sysconfig_1.27.1",
        "D:\APPs\TI\sysconfig_*",
        "D:\APPs\TI\CCS\ccs\utils\sysconfig_*",
        "D:\ti\sysconfig",
        "D:\ti\sysconfig_*",
        "C:\ti\sysconfig_*"
    )
    $sysconfigGuiRoot = ""
    foreach ($root in $sysconfigRoots) {
        if ((Test-Path -LiteralPath (Join-Path $root "nw\nw.exe") -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $root "app") -PathType Container)) {
            $sysconfigGuiRoot = $root
            break
        }
    }
    $sysconfigCli = ""
    foreach ($root in $sysconfigRoots) {
        $candidate = Join-Path $root "sysconfig_cli.bat"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $sysconfigCli = $candidate
            break
        }
    }

    $dslite = First-ExistingPath -Patterns @(
        "D:\APPs\TI\Unflsh\dslite.bat",
        "D:\APPs\TI\Uniflash\dslite.bat",
        "D:\APPs\TI\UniFlash\dslite.bat",
        "D:\ti\uniflash*\dslite.bat",
        "C:\ti\uniflash*\dslite.bat",
        "C:\Program Files\Texas Instruments\UniFlash*\dslite.bat",
        "C:\Program Files (x86)\Texas Instruments\UniFlash*\dslite.bat"
    )

    $openocd = First-ExistingPath -Patterns @(
        "F:\APPS\openocd\xpack-openocd-0.12.0-7\bin\openocd.exe",
        "F:\APPS\openocd\*\bin\openocd.exe",
        "D:\APPS\openocd\*\bin\openocd.exe",
        "D:\APPs\openocd\*\bin\openocd.exe",
        "C:\Program Files\OpenOCD\bin\openocd.exe"
    )
    if (-not $openocd) {
        $cmd = Get-Command openocd.exe -ErrorAction SilentlyContinue
        if ($cmd) { $openocd = $cmd.Source }
    }

    $openocdScripts = ""
    if ($openocd) {
        $openocdRoot = Parent-Dir $openocd 2
        $scriptCandidates = @(
            (Join-Path $openocdRoot "openocd\scripts"),
            (Join-Path $openocdRoot "scripts")
        )
        $openocdScripts = First-ExistingPath -Directory -Patterns $scriptCandidates
    }

    $gdb = First-ExistingPath -Patterns @(
        "F:\APPS\arm-none-eabi-gcc\xpack-arm-none-eabi-gcc-15.2.1-1.1\bin\arm-none-eabi-gdb.exe",
        "F:\APPS\arm-none-eabi-gcc\*\bin\arm-none-eabi-gdb.exe",
        "D:\APPS\arm-none-eabi-gcc\*\bin\arm-none-eabi-gdb.exe",
        "D:\APPs\arm-none-eabi-gcc\*\bin\arm-none-eabi-gdb.exe",
        "C:\Program Files (x86)\Arm GNU Toolchain*\bin\arm-none-eabi-gdb.exe",
        "C:\Program Files\Arm GNU Toolchain*\bin\arm-none-eabi-gdb.exe"
    )
    if (-not $gdb) {
        $cmd = Get-Command arm-none-eabi-gdb.exe -ErrorAction SilentlyContinue
        if ($cmd) { $gdb = $cmd.Source }
    }

    $objdump = ""
    if ($gdb) {
        $objdumpCandidate = Join-Path (Split-Path -Parent $gdb) "arm-none-eabi-objdump.exe"
        if (Test-Path -LiteralPath $objdumpCandidate -PathType Leaf) { $objdump = $objdumpCandidate }
    }

    $armToolchain = ""
    if ($gdb) { $armToolchain = Split-Path -Parent $gdb }

    [PSCustomObject]@{
        projectRoot      = $ProjectRoot
        code             = Get-CodeCommand
        ccsRoot          = ($ccsRoots | Select-Object -First 1)
        gmake            = $gmake
        compiler         = $compiler
        sdkRoot          = $sdkRoot
        sdkProduct       = $sdkProduct
        sysconfigGuiRoot = $sysconfigGuiRoot
        sysconfigCli     = $sysconfigCli
        dslite           = $dslite
        openocd          = $openocd
        openocdScripts   = $openocdScripts
        gdb              = $gdb
        armToolchain     = $armToolchain
        objdump          = $objdump
    }
}

function Install-VscodeExtensions {
    param([string]$CodeCommand)

    $required = @(
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "marus25.cortex-debug",
        "mcu-debug.peripheral-viewer",
        "eclipse-cdt.memory-inspector",
        "actboy168.tasks"
    )

    if ([string]::IsNullOrWhiteSpace($CodeCommand)) {
        Write-Warn "VSCode command 'code' not found. Please install recommended extensions manually."
        return
    }

    $installed = & $CodeCommand --list-extensions
    foreach ($ext in $required) {
        if ($installed -contains $ext) {
            Write-Ok "VSCode extension installed: $ext"
        } else {
            Write-Warn "VSCode extension missing: $ext"
            if ($InstallMissing -and -not $NoWrite) {
                Write-Host "[proc] code --install-extension $ext --force"
                & $CodeCommand --install-extension $ext --force
                if ($LASTEXITCODE -ne 0) {
                    Write-Warn "Extension install failed: $ext"
                } else {
                    Write-Ok "VSCode extension installed: $ext"
                }
            }
        }
    }
}

function New-SettingsJson {
    param([object]$Tools)

    $includePath = New-Object System.Collections.Generic.List[string]
    @(
        '${workspaceFolder}',
        '${workspaceFolder}/App',
        '${workspaceFolder}/BSP/**',
        '${workspaceFolder}/Module/**',
        '${workspaceFolder}/tests',
        '${workspaceFolder}/Debug'
    ) | ForEach-Object { $includePath.Add($_) | Out-Null }

    if ($Tools.sdkRoot) {
        $includePath.Add((To-VsPath (Join-Path $Tools.sdkRoot "source\third_party\CMSIS\Core\Include"))) | Out-Null
        $includePath.Add((To-VsPath (Join-Path $Tools.sdkRoot "source"))) | Out-Null
    }
    if ($Tools.compiler) {
        $compilerRoot = Parent-Dir $Tools.compiler 2
        $includePath.Add((To-VsPath (Join-Path $compilerRoot "include"))) | Out-Null
        $includePath.Add((To-VsPath (Join-Path $compilerRoot "include\c"))) | Out-Null
    }

    [ordered]@{
        "files.encoding" = "utf8"
        "files.autoGuessEncoding" = $false
        "files.associations" = [ordered]@{
            "*.h" = "c"
            "*.c" = "c"
            "ti_msp_dl_config.h" = "c"
        }
        "C_Cpp.intelliSenseEngine" = "default"
        "C_Cpp.errorSquiggles" = "enabled"
        "C_Cpp.default.cStandard" = "c11"
        "C_Cpp.default.compilerPath" = (To-VsPath $Tools.compiler)
        "C_Cpp.default.intelliSenseMode" = "windows-clang-arm"
        "C_Cpp.default.compileCommands" = '${workspaceFolder}/Debug/.clangd/compile_commands.json'
        "C_Cpp.default.defines" = @("__MSPM0G3507__", "__USE_SYSCONFIG__")
        "C_Cpp.default.includePath" = $includePath.ToArray()
        "cortex-debug.openocdPath" = (To-VsPath $Tools.openocd)
        "cortex-debug.gdbPath" = (To-VsPath $Tools.gdb)
        "cortex-debug.armToolchainPath" = (To-VsPath $Tools.armToolchain)
        "cortex-debug.objdumpPath" = (To-VsPath $Tools.objdump)
        "tasks.statusbar.default.hide" = $true
        "tasks.statusbar.limit" = 8
        "tasks.statusbar.select.label" = '$(list-selection) Task'
        "[c]" = [ordered]@{
            "editor.suggestOnTriggerCharacters" = $true
            "editor.quickSuggestions" = [ordered]@{
                "other" = "on"
                "comments" = "off"
                "strings" = "off"
            }
        }
    }
}

function New-CppPropertiesJson {
    param([object]$Tools)
    $settings = New-SettingsJson -Tools $Tools
    [ordered]@{
        "configurations" = @(
            [ordered]@{
                "name" = "MSPM0G3507-TI-Clang"
                "compilerPath" = $settings["C_Cpp.default.compilerPath"]
                "intelliSenseMode" = "windows-clang-arm"
                "cStandard" = "c11"
                "defines" = @("__MSPM0G3507__", "__USE_SYSCONFIG__")
                "includePath" = $settings["C_Cpp.default.includePath"]
                "forcedInclude" = @()
                "compileCommands" = '${workspaceFolder}/Debug/.clangd/compile_commands.json'
                "browse" = [ordered]@{
                    "path" = @('${workspaceFolder}', '${workspaceFolder}/App', '${workspaceFolder}/BSP', '${workspaceFolder}/Module', (To-VsPath (Join-Path $Tools.sdkRoot "source")))
                    "limitSymbolsToIncludedHeaders" = $true
                }
            }
        )
        "version" = 4
    }
}

function StatusBar {
    param([string]$Label, [string]$Detail, [string]$Color)
    [ordered]@{
        "hide" = $false
        "label" = $Label
        "detail" = $Detail
        "color" = $Color
        "running" = [ordered]@{
            "label" = '$(sync~spin) ' + (($Label -replace '^\$\([^)]+\)\s*', ''))
            "color" = "#FFD166"
        }
    }
}

function New-TasksJson {
    param([object]$Tools)

    $gmake = if ($Tools.gmake) { To-VsPath $Tools.gmake } else { "gmake.exe" }
    $sysconfigCli = if ($Tools.sysconfigCli) { To-VsPath $Tools.sysconfigCli } else { "" }
    $sdkProduct = if ($Tools.sdkProduct) { To-VsPath $Tools.sdkProduct } else { "" }
    $dslite = if ($Tools.dslite) { To-VsPath $Tools.dslite } else { "" }
    $openocd = if ($Tools.openocd) { To-VsPath $Tools.openocd } else { "openocd.exe" }
    $openocdScripts = if ($Tools.openocdScripts) { To-VsPath $Tools.openocdScripts } else { "" }
    $sysconfigGuiRoot = if ($Tools.sysconfigGuiRoot) { To-VsPath $Tools.sysconfigGuiRoot } else { "" }

    [ordered]@{
        "version" = "2.0.0"
        "tasks" = @(
            [ordered]@{
                "label" = "Build: CCS Debug"
                "type" = "process"
                "command" = "powershell.exe"
                "args" = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", '${workspaceFolder}/tools/vscode/build-ccs-debug.ps1', "-WorkspaceFolder", '${workspaceFolder}', "-Gmake", $gmake, "-Jobs", "4")
                "group" = [ordered]@{ "kind" = "build"; "isDefault" = $true }
                "problemMatcher" = @('$gcc')
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(tools) Build' "Build CCS Debug project" "#8FD694") }
                "detail" = "Build current CCS Debug project with TI Arm Clang."
            },
            [ordered]@{
                "label" = "Clean: CCS Debug"
                "type" = "process"
                "command" = $gmake
                "args" = @("-C", '${workspaceFolder}/Debug', "clean")
                "group" = "build"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(trash) Clean' "Clean CCS Debug output" "#C792EA") }
                "detail" = "Clean CCS Debug build output."
            },
            [ordered]@{
                "label" = "Rebuild: CCS Debug"
                "type" = "shell"
                "dependsOn" = @("Clean: CCS Debug", "Build: CCS Debug")
                "dependsOrder" = "sequence"
                "group" = "build"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(refresh) Rebuild' "Clean then build" "#82AAFF") }
                "detail" = "Clean and rebuild."
            },
            [ordered]@{
                "label" = "Open SysConfig"
                "type" = "process"
                "command" = "powershell.exe"
                "args" = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", '${workspaceFolder}/tools/vscode/open-sysconfig.ps1', "-WorkspaceFolder", '${workspaceFolder}', "-SdkProduct", $sdkProduct, "-SysConfigRoot", $sysconfigGuiRoot)
                "group" = "build"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(settings-gear) SysConfig' "Open empty.syscfg GUI" "#7FDBCA") }
                "detail" = "Open empty.syscfg in standalone SysConfig GUI."
            },
            [ordered]@{
                "label" = "SysConfig: Generate"
                "type" = "process"
                "command" = "cmd.exe"
                "args" = @("/d", "/c", '${workspaceFolder}/tools/vscode/sysconfig-generate.cmd', '${workspaceFolder}', $sysconfigCli, $sdkProduct, '${workspaceFolder}/Debug/syscfg')
                "group" = "build"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(file-code) Gen' "Generate Debug/syscfg files" "#B2CCD6") }
                "detail" = "Regenerate SysConfig output."
            },
            [ordered]@{
                "label" = "Flash: J-Link DSLite (Recommended)"
                "type" = "process"
                "command" = "cmd.exe"
                "args" = @("/d", "/c", '${workspaceFolder}/tools/vscode/flash-dslite-jlink.cmd', '${workspaceFolder}', $dslite)
                "dependsOn" = "Build: CCS Debug"
                "dependsOrder" = "sequence"
                "group" = "test"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = (StatusBar '$(zap) Flash' "J-Link + DSLite flash and run" "#FFCB6B") }
                "detail" = "Recommended flash path: CCS/UniFlash DSLite + J-Link."
            },
            [ordered]@{
                "label" = "Flash: J-Link OpenOCD"
                "type" = "process"
                "command" = "cmd.exe"
                "args" = @("/d", "/c", '${workspaceFolder}/tools/vscode/flash-openocd-jlink.cmd', '${workspaceFolder}', $openocd, $openocdScripts, "40000")
                "dependsOn" = "Build: CCS Debug"
                "dependsOrder" = "sequence"
                "group" = "test"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = [ordered]@{ "hide" = $true } }
                "detail" = "Experimental J-Link + OpenOCD flash."
            },
            [ordered]@{
                "label" = "Flash: J-Link OpenOCD Safe"
                "type" = "process"
                "command" = "cmd.exe"
                "args" = @("/d", "/c", '${workspaceFolder}/tools/vscode/flash-openocd-jlink.cmd', '${workspaceFolder}', $openocd, $openocdScripts, "8000")
                "dependsOn" = "Build: CCS Debug"
                "dependsOrder" = "sequence"
                "group" = "test"
                "problemMatcher" = @()
                "options" = [ordered]@{ "statusbar" = [ordered]@{ "hide" = $true } }
                "detail" = "Experimental J-Link + OpenOCD safe-speed flash."
            }
        )
    }
}

function New-LaunchJson {
    param([object]$Tools)
    $openocd = if ($Tools.openocd) { To-VsPath $Tools.openocd } else { "openocd.exe" }
    $openocdScripts = if ($Tools.openocdScripts) { To-VsPath $Tools.openocdScripts } else { "" }
    $gdb = if ($Tools.gdb) { To-VsPath $Tools.gdb } else { "arm-none-eabi-gdb.exe" }
    $armToolchain = if ($Tools.armToolchain) { To-VsPath $Tools.armToolchain } else { "" }
    $objdump = if ($Tools.objdump) { To-VsPath $Tools.objdump } else { "" }

    [ordered]@{
        "version" = "0.2.0"
        "configurations" = @(
            [ordered]@{
                "name" = "Debug: J-Link OpenOCD"
                "type" = "cortex-debug"
                "request" = "launch"
                "servertype" = "openocd"
                "cwd" = '${workspaceFolder}'
                "executable" = '${workspaceFolder}/Debug/cy_template.out'
                "device" = "MSPM0G3507"
                "runToEntryPoint" = "main"
                "preLaunchTask" = "Build: CCS Debug"
                "serverpath" = $openocd
                "gdbPath" = $gdb
                "armToolchainPath" = $armToolchain
                "objdumpPath" = $objdump
                "searchDir" = @($openocdScripts)
                "configFiles" = @("interface/jlink.cfg", "target/ti_mspm0.cfg")
                "serverArgs" = @("-c", "transport select swd", "-c", "adapter speed 40000")
                "showDevDebugOutput" = "none"
                "liveWatch" = [ordered]@{ "enabled" = $false; "samplesPerSecond" = 4 }
            },
            [ordered]@{
                "name" = "Debug: DAPLink OpenOCD (Fallback)"
                "type" = "cortex-debug"
                "request" = "launch"
                "servertype" = "openocd"
                "cwd" = '${workspaceFolder}'
                "executable" = '${workspaceFolder}/Debug/cy_template.out'
                "device" = "MSPM0G3507"
                "runToEntryPoint" = "main"
                "preLaunchTask" = "Build: CCS Debug"
                "serverpath" = $openocd
                "gdbPath" = $gdb
                "searchDir" = @($openocdScripts)
                "configFiles" = @("interface/cmsis-dap.cfg", "target/ti_mspm0.cfg")
                "serverArgs" = @("-c", "adapter speed 8000")
                "showDevDebugOutput" = "none"
            }
        )
    }
}

function Write-VscodeFiles {
    param([object]$Tools)
    if (-not (Test-Path -LiteralPath $VscodeDir -PathType Container)) {
        New-Item -ItemType Directory -Path $VscodeDir | Out-Null
    }

    $extensions = [ordered]@{
        "recommendations" = @(
            "ms-vscode.cpptools",
            "ms-vscode.cmake-tools",
            "marus25.cortex-debug",
            "mcu-debug.peripheral-viewer",
            "eclipse-cdt.memory-inspector",
            "actboy168.tasks"
        )
    }

    Write-JsonFile -Path (Join-Path $VscodeDir "extensions.json") -Object $extensions
    Write-JsonFile -Path (Join-Path $VscodeDir "settings.json") -Object (New-SettingsJson -Tools $Tools)
    Write-JsonFile -Path (Join-Path $VscodeDir "c_cpp_properties.json") -Object (New-CppPropertiesJson -Tools $Tools)
    Write-JsonFile -Path (Join-Path $VscodeDir "tasks.json") -Object (New-TasksJson -Tools $Tools)
    Write-JsonFile -Path (Join-Path $VscodeDir "launch.json") -Object (New-LaunchJson -Tools $Tools)

    $localEnv = [ordered]@{
        "generatedAt" = (Get-Date).ToString("s")
        "projectRoot" = $ProjectRoot
        "tools" = $Tools
    }
    Write-JsonFile -Path $LocalEnvPath -Object $localEnv

    $gitignore = Join-Path $ProjectRoot ".gitignore"
    $ignoreLine = ".vscode/local.env.json"
    if (Test-Path -LiteralPath $gitignore -PathType Leaf) {
        $content = Get-Content -LiteralPath $gitignore -ErrorAction SilentlyContinue
        if ($content -notcontains $ignoreLine) {
            Add-Content -LiteralPath $gitignore -Value $ignoreLine
        }
    } else {
        Set-Content -LiteralPath $gitignore -Value $ignoreLine -Encoding UTF8
    }
}

function Test-JsonFiles {
    foreach ($file in @("settings.json", "c_cpp_properties.json", "tasks.json", "launch.json", "extensions.json")) {
        $path = Join-Path $VscodeDir $file
        Get-Content -Raw -LiteralPath $path | ConvertFrom-Json | Out-Null
        Write-Ok "JSON valid: .vscode/$file"
    }
}

function Invoke-BuildVerify {
    param([object]$Tools)
    if (-not $Tools.gmake) {
        Write-Warn "Build verify skipped: gmake.exe not found."
        return
    }

    $debugDir = Join-Path $ProjectRoot "Debug"
    if (-not (Test-Path -LiteralPath $debugDir -PathType Container)) {
        Write-Warn "Build verify skipped: Debug build directory not found. Build the project once in CCS to generate it."
        return
    }

    $buildScript = Join-Path $ScriptsDir "build-ccs-debug.ps1"
    if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
        Write-Warn "Build verify skipped: $buildScript not found."
        return
    }

    Write-Host "[proc] Verify build"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $buildScript -WorkspaceFolder $ProjectRoot -Gmake $Tools.gmake -Jobs 4
    if ($LASTEXITCODE -ne 0) {
        throw "Build verify failed."
    }
}

Write-Step "MSPM0G3507 VSCode environment setup"
Write-Host "[main] Project root: $ProjectRoot"

Write-Step "Detect tools"
$tools = Find-Tools
Show-Tool "VSCode code command" $tools.code -Optional
Show-Tool "CCS root" $tools.ccsRoot
Show-Tool "CCS gmake" $tools.gmake
Show-Tool "TI Arm Clang" $tools.compiler
Show-Tool "MSPM0 SDK product.json" $tools.sdkProduct
Show-Tool "SysConfig GUI root" $tools.sysconfigGuiRoot -Optional
Show-Tool "SysConfig CLI" $tools.sysconfigCli
Show-Tool "UniFlash/DSLite" $tools.dslite
Show-Tool "OpenOCD" $tools.openocd -Optional
Show-Tool "OpenOCD scripts" $tools.openocdScripts -Optional
Show-Tool "Arm GDB" $tools.gdb -Optional

Write-Step "Install missing large software"
$installedLargeSoftware = $false
if ($NoWrite) {
    Write-Warn "Software installation skipped by -NoWrite."
} else {
    $installedLargeSoftware = Offer-MissingSoftwareInstall -Tools $tools
}
if ($installedLargeSoftware) {
    Write-Host "[main] Re-detect tools after installer step..."
    $tools = Find-Tools
    Show-Tool "CCS root" $tools.ccsRoot
    Show-Tool "CCS gmake" $tools.gmake
    Show-Tool "TI Arm Clang" $tools.compiler
    Show-Tool "MSPM0 SDK product.json" $tools.sdkProduct
    Show-Tool "SysConfig GUI root" $tools.sysconfigGuiRoot -Optional
    Show-Tool "SysConfig CLI" $tools.sysconfigCli
    Show-Tool "UniFlash/DSLite" $tools.dslite
}

Write-Step "VSCode extensions"
if ($SkipExtensions) {
    Write-Warn "Extension check skipped by -SkipExtensions."
} else {
    Install-VscodeExtensions -CodeCommand $tools.code
}

Write-Step "Generate VSCode project files"
if ($NoWrite) {
    Write-Warn "Write skipped by -NoWrite."
} else {
    Write-VscodeFiles -Tools $tools
    Write-Ok "Wrote .vscode configuration and .vscode/local.env.json"
    Test-JsonFiles
}

Write-Step "Verify build"
if ($NoBuildVerify -or $NoWrite) {
    Write-Warn "Build verify skipped by -NoBuildVerify or -NoWrite."
} else {
    Invoke-BuildVerify -Tools $tools
}

Write-Step "Summary"
$missingRequired = @()
if (-not $tools.gmake) { $missingRequired += "CCS gmake.exe" }
if (-not $tools.compiler) { $missingRequired += "TI Arm Clang tiarmclang.exe" }
if (-not $tools.sdkProduct) { $missingRequired += "MSPM0 SDK product.json" }
if (-not $tools.sysconfigCli) { $missingRequired += "SysConfig CLI" }
if (-not $tools.dslite) { $missingRequired += "UniFlash/DSLite dslite.bat" }

if ($missingRequired.Count -eq 0) {
    Write-Ok "Core environment is configured. Reload VSCode window to refresh status-bar buttons."
} else {
    Write-Warn "Core environment is not complete. Missing:"
    foreach ($item in $missingRequired) { Write-Host "  - $item" }
    Write-Host ""
    Write-Host "Install CCS/MSPM0 SDK/SysConfig/UniFlash, then run setup.ps1 again."
}

if (-not $InstallMissing) {
    Write-Host ""
    Write-Host "Tip: run with -InstallMissing to let the script install missing VSCode extensions automatically."
}
