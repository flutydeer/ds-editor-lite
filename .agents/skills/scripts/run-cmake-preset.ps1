param(
    [ValidateSet("Configure", "Build", "ConfigureAndBuild")]
    [string] $Mode = "Build",

    [string] $Preset = "debug",

    [string] $Target = "",

    [string] $QtDir = $env:QT_DIR
)

$ErrorActionPreference = "Stop"

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $requires = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
        $vs = & $vswhere -products * -requires $requires -version "[18.0,19.0)" -latest -property installationPath
        if (!$vs) {
            Write-Warning "Visual Studio 2026 / 18.x was not found by vswhere. Falling back to the latest VS with C++ tools."
            $vs = & $vswhere -products * -requires $requires -latest -property installationPath
        }
        if ($vs) {
            return $vs.Trim()
        }
    }

    $vsRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio"
    foreach ($major in @("18", "17")) {
        $majorRoot = Join-Path $vsRoot $major
        if (!(Test-Path $majorRoot)) {
            continue
        }

        $install = Get-ChildItem $majorRoot -Directory |
            Where-Object { Test-Path (Join-Path $_.FullName "VC\Auxiliary\Build\vcvarsall.bat") } |
            Select-Object -First 1

        if ($install) {
            Write-Warning "Using Visual Studio from filesystem fallback: $($install.FullName)"
            return $install.FullName
        }
    }

    throw "No Visual Studio installation with vcvarsall.bat was found."
}

function Resolve-QtDir {
    param([string] $RequestedQtDir)

    if ($RequestedQtDir -and (Test-Path $RequestedQtDir)) {
        return (Resolve-Path $RequestedQtDir).Path
    }

    $qtRoot = "C:\Qt"
    if (Test-Path $qtRoot) {
        $candidate = Get-ChildItem $qtRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "msvc2022_64" } |
            Where-Object { Test-Path $_ } |
            Select-Object -First 1

        if ($candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "QT_DIR is not set and no C:\Qt\*\msvc2022_64 directory was found."
}

function Invoke-CMakeCommand {
    param(
        [string] $VcVars,
        [string] $RepoRoot,
        [string] $QtPath,
        [string] $Command
    )

    $cmd = @(
        "call `"$VcVars`" x64",
        "set `"QT_DIR=$QtPath`"",
        "set `"Qt6_DIR=$QtPath`"",
        "set `"CMAKE_PREFIX_PATH=$QtPath`"",
        "set `"VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH`"",
        "cd /d `"$RepoRoot`"",
        $Command
    ) -join " && "

    Write-Host "Running: $Command"
    & cmd.exe /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$vsInstall = Find-VisualStudio
$vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvarsall.bat"
if (!(Test-Path $vcvars)) {
    throw "vcvarsall.bat was not found at: $vcvars"
}

$resolvedQtDir = Resolve-QtDir $QtDir

$configureCommand = "cmake --preset $Preset"
$buildCommand = "cmake --build --preset $Preset"
if ($Target) {
    $buildCommand = "$buildCommand --target $Target"
}

switch ($Mode) {
    "Configure" {
        Invoke-CMakeCommand $vcvars $repoRoot $resolvedQtDir $configureCommand
    }
    "Build" {
        Invoke-CMakeCommand $vcvars $repoRoot $resolvedQtDir $buildCommand
    }
    "ConfigureAndBuild" {
        Invoke-CMakeCommand $vcvars $repoRoot $resolvedQtDir "$configureCommand && $buildCommand"
    }
}
