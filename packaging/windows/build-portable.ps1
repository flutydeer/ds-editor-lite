[CmdletBinding()]
param(
    [string]$OutputDir = "dist\portable",
    # Build the CUDA flavor: configures LITE_ENABLE_CUDA=ON. Default is the
    # DirectML (DML) flavor. The vcpkg tree must already carry
    # onnxruntime-builds[cuda12] (run vcpkg install with --x-feature=cuda12).
    [switch]$EnableCuda,
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

function Get-ProductMetadata {
    param([Parameter(Mandatory = $true)][string]$DestinationPath)

    $generator = Join-Path $RepoRoot "cmake\GenerateProductMetadata.cmake"
    Invoke-Process "cmake" @(
        "-DOUTPUT_PATH=$DestinationPath",
        "-P",
        $generator
    )
    return Get-Content -LiteralPath $DestinationPath -Raw | ConvertFrom-Json
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

$ResolvedOutputDir = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $OutputDir))
New-Item -ItemType Directory -Force -Path $ResolvedOutputDir | Out-Null

$MetadataPath = Join-Path $ResolvedOutputDir "product-metadata.json"
$ProductMetadata = Get-ProductMetadata -DestinationPath $MetadataPath
$ExecutableName = "$($ProductMetadata.executableBaseName).exe"

if (-not $NoBuild) {
    Invoke-Step "Initialize Visual Studio and Qt environment" {
        Initialize-BuildEnvironment
    }

    Invoke-Step "Configure CMake preset package-dml-portable" {
        $configureArgs = @("--preset", "package-dml-portable")
        if ($EnableCuda) {
            # The preset pins LITE_ENABLE_CUDA=OFF for the DML flavor; the
            # switch overrides it so this script and the CMake runtime gate
            # agree on one source of truth.
            $configureArgs += "-DLITE_ENABLE_CUDA=ON"
        }
        Invoke-Process "cmake" $configureArgs
    }

    Invoke-Step "Build package-dml-portable" {
        Invoke-Process "cmake" @("--build", "--preset", "package-dml-portable")
    }
}

# The portable zip is produced from a real CMake install (not the raw build
# output): `cmake --install` lays down the app, the deployed Qt runtime,
# plugins, resources, and the app's + Qt's PDB symbols (windeployqt --pdb +
# LITE_INSTALL_PDB). We then add the vcpkg dependencies' PDBs, which vcpkg's
# applocal deploy does not copy, and zip the runnable `bin` tree.
$BuildDir = Join-Path $RepoRoot "build\PortableDmlRelease"
if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "out\bin\$ExecutableName"))) {
    throw "Build output not found in $BuildDir (run without -NoBuild first)."
}

$StageDir = Join-Path $ResolvedOutputDir "stage"
$AppDir = Join-Path $StageDir "bin"

Invoke-Step "Install to a clean staging tree" {
    if (Test-Path -LiteralPath $StageDir) {
        Remove-Item -LiteralPath $StageDir -Recurse -Force
    }
    Invoke-Process "cmake" @("--install", $BuildDir, "--prefix", $StageDir)
    if (-not (Test-Path -LiteralPath (Join-Path $AppDir "$ExecutableName"))) {
        throw "$ExecutableName not found after install in $AppDir"
    }
}

# windeployqt (--pdb) and LITE_INSTALL_PDB already installed the Qt and app
# PDBs. vcpkg ships each dependency's PDB next to its DLL but its applocal
# deploy copies only the DLLs, so for every installed DLL pull the same-named
# PDB from the vcpkg release bin. Kept here (packaging), not in CMake, so the
# build system stays vcpkg-agnostic.
Invoke-Step "Copy vcpkg dependency PDBs" {
    $vcpkgBin = Join-Path $RepoRoot "vcpkg\installed\x64-windows\bin"
    if (-not (Test-Path -LiteralPath $vcpkgBin -PathType Container)) {
        throw "vcpkg release bin not found: $vcpkgBin"
    }
    $copied = 0
    Get-ChildItem -LiteralPath $AppDir -Recurse -Filter "*.dll" | ForEach-Object {
        $pdbName = [System.IO.Path]::GetFileNameWithoutExtension($_.Name) + ".pdb"
        $src = Join-Path $vcpkgBin $pdbName
        $dst = Join-Path $_.DirectoryName $pdbName
        if ((Test-Path -LiteralPath $src -PathType Leaf) -and
            -not (Test-Path -LiteralPath $dst -PathType Leaf)) {
            Copy-Item -LiteralPath $src -Destination $dst
            $copied++
        }
    }
    Write-Host "Copied $copied vcpkg dependency PDB(s)."
}

$PdbCount = (Get-ChildItem -LiteralPath $AppDir -Recurse -Filter "*.pdb").Count
if ($PdbCount -eq 0) {
    throw "No PDB symbols in the installed tree - build was not RelWithDebInfo?"
}

# Flavor assertion: the staging tree must agree with -EnableCuda. The CMake
# runtime gate already enforces this at build/install time; this is the
# packaging-boundary net so a stale vcpkg tree can never slip through.
Invoke-Step "Validate staging flavor" {
    $cudaRuntimeDir = Join-Path $AppDir "plugins\srt-driver\inferencedrivers\srt-onnxdriver\runtimes\onnx\cuda"
    $cudaPresent = Test-Path -LiteralPath $cudaRuntimeDir
    if ($EnableCuda -and -not $cudaPresent) {
        throw ("CUDA staging is missing the ONNX Runtime cuda/ runtimes. " +
            "Run vcpkg install with --x-feature=cuda12, then rebuild with -EnableCuda.")
    }
    if (-not $EnableCuda -and $cudaPresent) {
        throw ("DML staging contains the ONNX Runtime cuda/ runtimes (vcpkg tree built " +
            "with --x-feature=cuda12?). " +
            "Rerun vcpkg install without the feature, then rebuild.")
    }
}

$Timestamp = Get-Date -Format "yyyyMMdd-HHmm"
$ZipBaseName = "DsEditorLite-$Timestamp-win-x64-$(if ($EnableCuda) { 'cuda' } else { 'dml' })-portable"
$ZipPath = Join-Path $ResolvedOutputDir "$ZipBaseName.zip"

Invoke-Step "Package portable zip" {
    $tempZip = Join-Path $ResolvedOutputDir ".$ZipBaseName.tmp.zip"
    if (Test-Path -LiteralPath $tempZip) {
        Remove-Item -LiteralPath $tempZip -Force
    }
    Compress-Archive -Path (Join-Path $AppDir "*") -DestinationPath $tempZip -CompressionLevel Optimal
    Move-Item -LiteralPath $tempZip -Destination $ZipPath -Force
}

$ZipSize = (Get-Item -LiteralPath $ZipPath).Length
$ZipHash = (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash
Write-Host ""
Write-Host "Portable zip created: $ZipPath" -ForegroundColor Green
Write-Host "Size: $([math]::Round($ZipSize / 1MB, 1)) MB"
Write-Host "SHA-256: $ZipHash"
