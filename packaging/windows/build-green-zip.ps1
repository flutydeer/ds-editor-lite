[CmdletBinding()]
param(
    [string]$OutputDir = "dist\green",
    [switch]$SkipVcpkgInstall,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)][scriptblock]$Body
    )

    Write-Host ""
    Write-Host "==> $Title" -ForegroundColor Cyan
    & $Body
}

function Invoke-Process {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$WorkingDirectory = $RepoRoot
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code $LASTEXITCODE`: $FilePath $($Arguments -join ' ')"
        }
    } finally {
        Pop-Location
    }
}

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $requires = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
        $install = & $vswhere -products * -requires $requires -version "[18.0,19.0)" -latest -property installationPath
        if (-not $install) {
            $install = & $vswhere -products * -requires $requires -latest -property installationPath
        }
        if ($install) {
            return $install.Trim()
        }
    }

    $vsRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio"
    foreach ($major in @("18", "17")) {
        $majorRoot = Join-Path $vsRoot $major
        if (-not (Test-Path -LiteralPath $majorRoot -PathType Container)) {
            continue
        }
        $install = Get-ChildItem -LiteralPath $majorRoot -Directory |
            Where-Object {
                Test-Path -LiteralPath (Join-Path $_.FullName "VC\Auxiliary\Build\vcvarsall.bat")
            } |
            Select-Object -First 1
        if ($install) {
            return $install.FullName
        }
    }

    throw "Visual Studio with the x64 C++ toolchain was not found."
}

function Find-QtDirectory {
    foreach ($candidate in @($env:QT_DIR, $env:Qt6_DIR, $env:CMAKE_PREFIX_PATH)) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $qtRoot = "C:\Qt"
    if (Test-Path -LiteralPath $qtRoot -PathType Container) {
        $candidate = Get-ChildItem -LiteralPath $qtRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "msvc2022_64" } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
            Select-Object -First 1
        if ($candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Qt was not found. Set QT_DIR or install C:\Qt\<version>\msvc2022_64."
}

function Initialize-BuildEnvironment {
    $vsInstall = Find-VisualStudio
    $vcVars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvarsall.bat"
    $qtDir = Find-QtDirectory

    $environmentCommand = "call `"$vcVars`" x64 >nul && set"
    $environmentLines = & cmd.exe /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize the Visual Studio x64 developer environment."
    }
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }

    $env:QT_DIR = $qtDir
    $env:Qt6_DIR = $qtDir
    $env:CMAKE_PREFIX_PATH = $qtDir
    $env:VCPKG_KEEP_ENV_VARS = "QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH"

    Write-Host "Visual Studio: $vsInstall"
    Write-Host "Qt: $qtDir"
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\.."))
Set-Location $RepoRoot

if (-not $NoBuild) {
    Invoke-Step "Initialize Visual Studio and Qt environment" {
        Initialize-BuildEnvironment
    }

    Invoke-Step "Configure CMake preset package-dml-green" {
        Invoke-Process "cmake" @("--preset", "package-dml-green")
    }

    Invoke-Step "Build package-dml-green" {
        Invoke-Process "cmake" @("--build", "--preset", "package-dml-green")
    }
}

# The green zip is packaged directly from the build output directory, which
# already contains the deployed Qt runtime, plugins, resources and PDB symbols
# (RelWithDebInfo). This keeps the PDBs next to the binaries so crashes can be
# traced after the user unzips and runs the app.
$OutBinDir = Join-Path $RepoRoot "build\GreenDmlRelease\out\bin"
if (-not (Test-Path -LiteralPath $OutBinDir)) {
    throw "Build output directory not found: $OutBinDir"
}
if (-not (Test-Path -LiteralPath (Join-Path $OutBinDir "DsEditorLite.exe"))) {
    throw "DsEditorLite.exe not found in $OutBinDir"
}
$PdbCount = (Get-ChildItem -LiteralPath $OutBinDir -Filter "*.pdb").Count
if ($PdbCount -eq 0) {
    throw "No PDB symbols found in $OutBinDir - build was not RelWithDebInfo?"
}

$ResolvedOutputDir = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $OutputDir))
New-Item -ItemType Directory -Force -Path $ResolvedOutputDir | Out-Null

$Timestamp = Get-Date -Format "yyyyMMdd-HHmm"
$ZipBaseName = "DsEditorLite-$Timestamp-win-x64-dml-green"
$ZipPath = Join-Path $ResolvedOutputDir "$ZipBaseName.zip"

Invoke-Step "Package green zip" {
    $tempZip = Join-Path $ResolvedOutputDir ".$ZipBaseName.tmp.zip"
    if (Test-Path -LiteralPath $tempZip) {
        Remove-Item -LiteralPath $tempZip -Force
    }
    Compress-Archive -Path (Join-Path $OutBinDir "*\") -DestinationPath $tempZip -CompressionLevel Optimal
    Move-Item -LiteralPath $tempZip -Destination $ZipPath -Force
}

$ZipSize = (Get-Item -LiteralPath $ZipPath).Length
$ZipHash = (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash
Write-Host ""
Write-Host "Green zip created: $ZipPath" -ForegroundColor Green
Write-Host "Size: $([math]::Round($ZipSize / 1MB, 1)) MB"
Write-Host "SHA-256: $ZipHash"
