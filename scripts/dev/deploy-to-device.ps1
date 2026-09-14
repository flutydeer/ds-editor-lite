<#
.SYNOPSIS
    Builds DS Editor Lite, deploys it to a device over SSH, restarts it there,
    and streams the device's log output back to this machine.

.DESCRIPTION
    Replaces the manual "copy the build over a share, run it, then copy the log
    file back" loop used when debugging input handling on a touch device.

    The device only needs an OpenSSH server; everything else is driven from
    here. The editor is started with --log-udp pointing at this machine, so its
    log lines appear in this console as they happen instead of in a file that
    has to be collected afterwards.

    Prerequisites:
      * OpenSSH Server enabled and running on the device
        (Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0),
        with the firewall rule for port 22 and the LAN profile set to Private.
      * Key-based login recommended, otherwise every SSH/SCP call prompts.

.PARAMETER Target
    Device address as user@host, e.g. dev@192.168.1.42 or dev@surface.

.PARAMETER Preset
    CMake preset to build. Defaults to debug; release is a much smaller and
    faster deployment if you do not need assertions and debug symbols.

.PARAMETER RemoteDir
    Directory on the device that receives the deployment. Avoid spaces.

.PARAMETER LogPort
    UDP port used for log mirroring. Must be free on both machines.

.PARAMETER LogHost
    Address the device sends logs to. Defaults to the local address that routes
    to the device, which is normally the right one on a single LAN.

.PARAMETER BinDir
    Local directory holding the built, self-contained runtime (the executable,
    its DLLs, and the plugin tree). Defaults to the preset's out/bin directory.

.PARAMETER StageDir
    Local staging directory. The runtime is mirrored here with debug symbols and
    test executables filtered out, because those roughly double the payload and
    the device needs neither.

.PARAMETER SshKey
    Private key passed to ssh/scp with -i.

.PARAMETER ExtraArgs
    Additional editor command-line options, as one string.

.PARAMETER ExeOnly
    Deploy just the executable. Dependencies rarely change between code edits,
    so this is much faster for the normal edit-build-run loop. Use a full deploy
    after dependency, plugin, or CMake changes.

.PARAMETER SkipBuild
    Deploy the existing build output without building.

.PARAMETER NoStop
    Skip the pre-deploy taskkill on the device. The start script still stops any
    running instance, so this is only useful when inspecting a running process.

.PARAMETER NoStart
    Deploy without starting the editor.

.PARAMETER NoListen
    Do not attach the UDP log listener after starting the editor.

.PARAMETER DryRun
    Print the commands that would run, without running them.

.EXAMPLE
    .\scripts\dev\deploy-to-device.ps1 dev@192.168.1.42

.EXAMPLE
    .\scripts\dev\deploy-to-device.ps1 dev@surface -ExeOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Target,

    [string]$Preset = "package-dml-portable",
    [string]$RemoteDir = "C:\Data\Lite",
    [int]$LogPort = 9999,
    [string]$LogHost = "",
    [string]$BinDir = "",
    [string]$StageDir = "",
    [string]$SshKey = "",
    [string]$ExtraArgs = "",
    [string]$ExecutableName = "DsEditorLite.exe",
    # Scheduled task used to launch the editor in the device's interactive session.
    [string]$TaskName = "DsEditorLiteDeploy",

    [switch]$ExeOnly,
    [switch]$SkipBuild,
    [switch]$NoStop,
    [switch]$NoStart,
    [switch]$NoListen,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

# Preset name -> deployed runtime directory, mirroring CMAKE_INSTALL_PREFIX plus
# its bin/ subdirectory in CMakePresets.json.
#
# Only the package-* presets turn on LITE_INSTALL, which is what makes the
# install tree self-contained: it decides whether the Qt runtime, plugins/,
# Resources/ and configs/ are copied in or skipped. The debug and release
# presets skip them, so their install/bin ends up holding a handful of support
# DLLs and no usable runtime.
#
# The installed bin/ already carries the MSVC runtime DLLs, because they are
# deployed like any other dependency, so the device needs nothing preinstalled.
$PresetBinDirs = @{
    "package-dml-portable" = "dist/stage/portable/bin"
    "package-dml-release"  = "dist/stage/dml/bin"
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if (-not $BinDir) {
    if (-not $PresetBinDirs.ContainsKey($Preset)) {
        throw "Preset '$Preset' has no self-contained install tree (it needs LITE_INSTALL=ON). Use package-dml-portable, or pass -BinDir."
    }
    $BinDir = Join-Path $RepoRoot $PresetBinDirs[$Preset]
}
$BinDir = [System.IO.Path]::GetFullPath($BinDir)
if (-not $StageDir) {
    $StageDir = Join-Path (Split-Path -Parent $BinDir) "deploy-stage"
}
$StageDir = [System.IO.Path]::GetFullPath($StageDir)

$PresetScript = Join-Path $RepoRoot ".agents\skills\scripts\run-cmake-preset.ps1"
$StartScript = Join-Path $PSScriptRoot "device\start-editor.cmd"
$ListenerScript = Join-Path $PSScriptRoot "listen-remote-log.ps1"

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Title)
    Write-Host ""
    Write-Host "==> $Title" -ForegroundColor Cyan
}

function Get-DeviceHost {
    param([Parameter(Mandatory = $true)][string]$TargetSpec)

    $separator = $TargetSpec.LastIndexOf("@")
    if ($separator -ge 0) {
        return $TargetSpec.Substring($separator + 1)
    }
    return $TargetSpec
}

function Get-LogHostAddress {
    param([Parameter(Mandatory = $true)][string]$DeviceHost)

    # Connecting a UDP socket transmits nothing; it only asks the routing table
    # which local address reaches the device, which is exactly the address the
    # device must send its log datagrams back to.
    $probe = [System.Net.Sockets.UdpClient]::new()
    try {
        $probe.Connect($DeviceHost, 9)
        $address = $probe.Client.LocalEndPoint.Address
        if ($address -and -not [System.Net.IPAddress]::IsLoopback($address) -and
            $address.IPAddressToString -ne "0.0.0.0") {
            return $address.IPAddressToString
        }
    }
    catch {
        # Fall through to the error below.
    }
    finally {
        $probe.Close()
    }

    throw "Cannot determine which local address reaches '$DeviceHost'. Pass -LogHost explicitly."
}

function Get-SshArguments {
    $arguments = @()
    if ($SshKey) {
        $arguments += @("-i", $SshKey)
    }
    return $arguments
}

function Invoke-Tool {
    <#
        Runs an external tool and returns its exit code.

        Two things make a bare `& tool` unreliable here: OpenSSH 10.2 prints a
        post-quantum key-exchange warning to stderr on every connection, and
        under $ErrorActionPreference = 'Stop' any native command that writes to
        stderr is promoted to a terminating error. Relax the preference for the
        call so those warnings stay warnings, and let the caller judge the exit
        code.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        # Discards all output; used for probes whose failure is expected.
        [switch]$Silent
    )

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        if ($Silent) {
            & $FilePath @Arguments 2>&1 | Out-Null
        }
        else {
            # Out-Host keeps the tool's own stdout out of this function's return
            # value. Without it, a caller capturing the result would receive the
            # output lines followed by the exit code, and every exit-code test
            # would compare against an array.
            & $FilePath @Arguments | Out-Host
        }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

function Invoke-RemoteCommand {
    param([Parameter(Mandatory = $true)][string]$Command)

    if ($DryRun) {
        Write-Host "ssh $Target `"$Command`"" -ForegroundColor DarkGray
        return
    }

    $argumentList = Get-SshArguments
    $argumentList += @($Target, $Command)
    $exitCode = Invoke-Tool -FilePath "ssh" -Arguments $argumentList
    if ($exitCode -ne 0) {
        throw "Remote command failed with exit code $exitCode`: $Command"
    }
}

function Invoke-Copy {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [switch]$Recurse
    )

    $arguments = Get-SshArguments
    # -C compresses; the payload is mostly DLLs, which still shrink usefully.
    $arguments += @("-C")
    if ($Recurse) {
        $arguments += @("-r")
    }
    $arguments += @($Source, $Destination)

    if ($DryRun) {
        Write-Host "scp $($arguments -join ' ')" -ForegroundColor DarkGray
        return
    }

    $exitCode = Invoke-Tool -FilePath "scp" -Arguments $arguments
    if ($exitCode -ne 0) {
        throw "scp failed with exit code $exitCode`: $Source -> $Destination"
    }
}

function Invoke-Stage {
    # Mirrors the self-contained runtime into the staging directory, dropping
    # files the device cannot use: debug symbols, and the test executables that
    # the build tree produces alongside the editor.
    $excludes = @("*.pdb", "*.ilk", "Test*.exe", "*Tests.exe")

    if ($DryRun) {
        Write-Host "robocopy `"$BinDir`" `"$StageDir`" /MIR /XF $($excludes -join ' ')" -ForegroundColor DarkGray
        return
    }

    $arguments = @($BinDir, $StageDir, "/MIR", "/XF") + $excludes +
        @("/NFL", "/NDL", "/NJH", "/NJS", "/NP")
    # robocopy reports success through exit codes 0-7; 8 and above mean failure.
    $exitCode = Invoke-Tool -FilePath "robocopy" -Arguments $arguments -Silent
    if ($exitCode -ge 8) {
        throw "robocopy failed with exit code $exitCode while staging the deployment."
    }
}

function Get-VisualStudioRoot {
    # vswhere skips Insider builds unless -prerelease is passed, so fall back to
    # the filesystem before giving up.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -products * -prerelease -latest -property installationPath
        if ($install) {
            return $install.Trim()
        }
    }

    $vsRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio"
    foreach ($major in @("18", "17")) {
        $majorRoot = Join-Path $vsRoot $major
        if (-not (Test-Path -LiteralPath $majorRoot)) {
            continue
        }
        $install = Get-ChildItem -Path $majorRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "VC\Redist\MSVC") } |
            Select-Object -First 1
        if ($install) {
            return $install.FullName
        }
    }

    throw "Visual Studio was not found, so the MSVC runtime cannot be located."
}

function Add-CompilerRuntime {
    <#
        Copies the MSVC runtime DLLs into the staging directory, but only when
        the installed tree is missing them.

        The install tree normally ships them already, because they are deployed
        like any other dependency. A Debug build is the exception: its runtime
        lives under debug_nonredist on the build machine and is not copied, so a
        deployment without this step would die on startup with 0xC0000135
        (STATUS_DLL_NOT_FOUND).
    #>
    param([Parameter(Mandatory = $true)][string]$Destination)

    if (Test-Path -LiteralPath (Join-Path $Destination "vcruntime140.dll")) {
        return
    }

    $isDebugBuild = $Preset -match 'debug'
    $relativePattern = if ($isDebugBuild) {
        "debug_nonredist\x64\Microsoft.VC*.DebugCRT"
    }
    else {
        "x64\Microsoft.VC*.CRT"
    }

    $redistRoot = Join-Path (Get-VisualStudioRoot) "VC\Redist\MSVC"
    # Version directories are numeric ("14.51.36231"); "v145" style entries are
    # aliases and must not win the sort.
    $versionDirs = Get-ChildItem -Path $redistRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
        Sort-Object { [version]$_.Name } -Descending

    $runtimeDir = $null
    foreach ($versionDir in $versionDirs) {
        # Get-ChildItem expands the wildcard in the flavor name; Test-Path with
        # -LiteralPath would treat the asterisk as a literal character.
        $match = Get-ChildItem -Path (Join-Path $versionDir.FullName $relativePattern) -Directory -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) {
            $runtimeDir = $match.FullName
            break
        }
    }

    if (-not $runtimeDir) {
        throw "The install tree has no MSVC runtime and none matched '$relativePattern' under '$redistRoot'."
    }

    $dlls = Get-ChildItem -Path $runtimeDir -Filter *.dll
    if ($DryRun) {
        Write-Host "copy $($dlls.Count) MSVC runtime DLL(s) from $runtimeDir" -ForegroundColor DarkGray
        return
    }

    foreach ($dll in $dlls) {
        Copy-Item -LiteralPath $dll.FullName -Destination $Destination -Force
    }
    Write-Host "    staged $($dlls.Count) MSVC runtime DLL(s)" -ForegroundColor DarkGray
}

if (-not (Get-Command ssh -ErrorAction SilentlyContinue)) {
    throw "ssh was not found on PATH. Install the OpenSSH Client optional feature."
}
if (-not (Get-Command scp -ErrorAction SilentlyContinue)) {
    throw "scp was not found on PATH. Install the OpenSSH Client optional feature."
}

$TargetHost = Get-DeviceHost -TargetSpec $Target
if (-not $LogHost) {
    $LogHost = Get-LogHostAddress -DeviceHost $TargetHost
}
$LogEndpoint = "${LogHost}:$LogPort"
# scp and the remote shell both accept forward slashes, avoiding the backslash
# escaping rules of the remote command line.
$RemoteDirForScp = $RemoteDir.Replace("\", "/")

if (-not $SkipBuild) {
    Write-Step "Building preset '$Preset' and staging the install tree"
    # The install target builds the editor and then lays out the self-contained
    # tree that the device receives.
    if ($DryRun) {
        Write-Host "run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset $Preset -Target install" -ForegroundColor DarkGray
    }
    else {
        $exitCode = Invoke-Tool -FilePath "powershell" -Arguments @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PresetScript,
            "-Mode", "ConfigureAndBuild", "-Preset", $Preset, "-Target", "install")
        if ($exitCode -ne 0) {
            throw "The build failed; fix the errors above and re-run."
        }
    }
}

if (-not $DryRun -and -not (Test-Path -LiteralPath (Join-Path $BinDir $ExecutableName))) {
    throw "'$BinDir' does not contain $ExecutableName. Build first, or pass -BinDir."
}

if (-not $NoStop) {
    Write-Step "Stopping any running editor on the device"
    # A failure here just means nothing was running, so the exit code is ignored.
    if ($DryRun) {
        Write-Host "ssh $Target `"taskkill /IM $ExecutableName /F`"" -ForegroundColor DarkGray
    }
    else {
        $argumentList = Get-SshArguments
        $argumentList += @($Target, "taskkill /IM $ExecutableName /F")
        Invoke-Tool -FilePath "ssh" -Arguments $argumentList -Silent | Out-Null
    }
}

Write-Step "Deploying to ${Target}:$RemoteDir"
Invoke-RemoteCommand -Command "if not exist $RemoteDir mkdir $RemoteDir"

if ($ExeOnly) {
    Invoke-Copy -Source (Join-Path $BinDir $ExecutableName) -Destination "${Target}:$RemoteDirForScp/"
}
else {
    Invoke-Stage
    Add-CompilerRuntime -Destination $StageDir
    # Trailing slash-separated wildcard copies the staging contents, not the directory.
    $stagedContents = $StageDir.Replace("\", "/") + "/*"
    Invoke-Copy -Source $stagedContents -Destination "${Target}:$RemoteDirForScp/" -Recurse
}
Invoke-Copy -Source $StartScript -Destination "${Target}:$RemoteDirForScp/"

if (-not $NoStart) {
    Write-Step "Starting the editor with --log-udp $LogEndpoint"
    $startArguments = "--log-udp $LogEndpoint"
    if ($ExtraArgs) {
        $startArguments = "$startArguments $ExtraArgs"
    }

    # The task runs in the signed-in user's interactive session (/IT). Anything
    # launched straight from the SSH session is a child of the sshd service and
    # therefore lives in session 0, where no window can ever appear on the
    # device's desktop even though the process itself runs fine.
    #
    # schtasks wants the launch command as a single string, and quotes do not
    # survive the trip through PowerShell, ssh.exe and cmd: the editor options
    # would be parsed as schtasks options. So the task is pointed at a wrapper
    # that takes no arguments, and the options are baked into that file, where
    # no shell has to quote anything.
    $wrapperPath = Join-Path ([System.IO.Path]::GetTempPath()) "run-editor.cmd"
    $wrapperLines = @(
        '@echo off',
        'REM Generated by deploy-to-device.ps1: runs the editor with the log',
        'REM target of the deployment that produced this file.',
        ('call "%~dp0start-editor.cmd" ' + $startArguments)
    )
    if (-not $DryRun) {
        Set-Content -LiteralPath $wrapperPath -Value $wrapperLines -Encoding ASCII
    }
    Invoke-Copy -Source $wrapperPath -Destination "${Target}:$RemoteDirForScp/"

    # The schedule time is irrelevant: /run triggers the task immediately, and
    # 23:59 avoids the "start time is in the past" warning that 00:00 produces.
    Invoke-RemoteCommand -Command "schtasks /create /tn $TaskName /tr $RemoteDir\run-editor.cmd /sc once /st 23:59 /f /it"
    Invoke-RemoteCommand -Command "schtasks /run /tn $TaskName"
}

if ($DryRun) {
    Write-Host ""
    Write-Host "Dry run complete; nothing was built, copied, or started." -ForegroundColor Yellow
    exit 0
}

if (-not $NoStart -and -not $NoListen) {
    Write-Step "Streaming device logs (Ctrl+C to stop)"
    Write-Host "The editor writes its log file on the device too." -ForegroundColor DarkGray
    & powershell -NoProfile -ExecutionPolicy Bypass -File $ListenerScript -Port $LogPort -Source
}
