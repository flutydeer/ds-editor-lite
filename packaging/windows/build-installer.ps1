[CmdletBinding()]
param(
    [string]$StageDir = "dist\stage\dml",
    [string]$OutputDir = "dist\installer",
    [string]$InnoSetupPath = "",
    [string]$VcRedistPath = "",

    # Directory of unpacked wolf language packages to stage: one subdirectory per package, each
    # containing its desc.json. This route installs a tree (LITE_INSTALL=ON), and CMake
    # configuration of such a tree fails if no source for the packages is set. If this parameter is
    # empty, the script passes no -D option and CMake resolves the packages from the
    # WOLF_LANG_PACKAGES_SOURCE environment variable, an installed wolf-lang-packages package or,
    # if LITE_WOLF_LANG_PACKAGES_SIBLING_FALLBACK is ON, a sibling wolf checkout. If this parameter
    # is set, LITE_WOLF_LANG_PACKAGES takes precedence over all of those sources.
    # The parameter controls only whether the -D option is passed. The script does not clear the
    # CMake cache of the build directory, so a cache that already contains LITE_WOLF_LANG_PACKAGES
    # keeps supplying the value in the binaryDir of this preset (build\PackageDmlRelease) even
    # on a default run. Deleting that cache, or configuring once with an explicit empty
    # -DLITE_WOLF_LANG_PACKAGES=, restores the resolution by convention.
    [string]$WolfLangPackages = "",

    [switch]$SkipVcpkgInstall,
    # Build the CUDA flavor: installs onnxruntime-builds[cuda12] and configures
    # LITE_ENABLE_CUDA=ON. Default is the DirectML (DML) flavor.
    [switch]$EnableCuda,
    [switch]$NoBuild,
    [switch]$CheckPrerequisites,

    [string]$SignToolPath = "",
    [string]$SignCertThumbprint = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

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

function Find-InnoSetup {
    if ($InnoSetupPath) {
        $candidate = Resolve-RepoPath $InnoSetupPath
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
        throw "Inno Setup compiler not found: $candidate"
    }

    $defaultPaths = @(
        "${env:ProgramFiles(x86)}\Inno Setup 7\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 7\ISCC.exe"
    )

    foreach ($path in $defaultPaths) {
        if ($path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
            return $path
        }
    }

    $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "ISCC.exe was not found. Install Inno Setup 7 or pass -InnoSetupPath."
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

function Convert-ToInnoPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return ([System.IO.Path]::GetFullPath($Path) -replace "/", "\")
}

function Escape-InnoValue {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)
    return $Value.Replace('"', '""')
}

function Write-InnoScript {
    param(
        [Parameter(Mandatory = $true)][string]$TemplatePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $values = @{
        APP_NAME             = $ProductMetadata.productName
        APP_VERSION          = $ProductMetadata.version
        APP_PUBLISHER        = $ProductMetadata.publisher
        APP_COPYRIGHT        = $ProductMetadata.copyright
        APP_URL              = $ProductMetadata.productUrl
        APP_EXE_NAME         = "$($ProductMetadata.executableBaseName).exe"
        APP_ID               = "{{$($ProductMetadata.windowsAppId)}"
        STAGE_DIR            = Convert-ToInnoPath $ResolvedStageDir
        OUTPUT_DIR           = Convert-ToInnoPath $ResolvedOutputDir
        OUTPUT_BASE_FILENAME = "$($ProductMetadata.executableBaseName)-$($ProductMetadata.version)-win-x64-$(if ($EnableCuda) { 'cuda' } else { 'dml' })-internal"
        VC_REDIST_PATH       = Convert-ToInnoPath $ResolvedVcRedistPath
        SIGN_TOOL_NAME       = $Script:SignToolName
    }

    $script = Get-Content -LiteralPath $TemplatePath -Raw
    foreach ($key in $values.Keys) {
        $script = $script.Replace("@$key@", (Escape-InnoValue ([string]$values[$key])))
    }

    Set-Content -LiteralPath $DestinationPath -Value $script -Encoding UTF8
}

# Reads the deployed layout from src/libs/SynthrtEngine/DeployLayout.h (cmake/LiteBuildApi.cmake).
function Get-DeployLayout {
    $header = Join-Path $RepoRoot "src\libs\SynthrtEngine\DeployLayout.h"
    $layout = @{}
    foreach ($line in Select-String -LiteralPath $header -Pattern 'inline constexpr char ([A-Z_]+)\[\] = "([^"]*)";') {
        $groups = $line.Matches[0].Groups
        $layout[$groups[1].Value] = $groups[2].Value -replace '/', '\'
    }
    foreach ($name in @("ONNX_RUNTIME_DIR", "CUDA_RUNTIME_SUBDIR", "LANGUAGE_PACKAGES_DIR",
        "PLUGIN_LIBRARIES")) {
        if (-not $layout.ContainsKey($name)) {
            throw "DeployLayout.h does not define $name in the form this script parses"
        }
    }
    return $layout
}

function Assert-StagingLayout {
    # The plugins\<library> entries are the host plugin trees. cmake/LiteBuildApi.cmake
    # copies each tree only if the vcpkg tree contains it and only warns if it is absent. Without
    # these entries an installer could be built and shipped without an entire plugin tree
    # (dsinfer, wolf or otter) while every step reports success. The library names are not repeated
    # here: they are read from DeployLayout.h (PLUGIN_LIBRARIES), the same definition
    # cmake/LiteBuildApi.cmake reads, so the deployed and the verified library sets cannot diverge.
    # build-portable.ps1 verifies the same list below its $AppDir.
    $pluginTreePaths = $DeployLayout.PLUGIN_LIBRARIES -split ' ' |
        ForEach-Object { "bin\plugins\$_" }
    # The ONNX Runtime payload is the second deployment input CMake reads from a package it cannot
    # verify itself: a missing payload only warns unless LITE_INSTALL is set, which turns it into a
    # configure error. The path below is the default flavor, which every build carries; the CUDA
    # flavor subdirectory is the additional one and is asserted against -EnableCuda further down.
    $requiredPaths = @(
        "bin\$($ProductMetadata.executableBaseName).exe",
        "bin\plugins",
        "bin\plugins\platforms"
    ) + $pluginTreePaths + @(
        "bin\Resources",
        "bin\configs",
        "bin\$($DeployLayout.LANGUAGE_PACKAGES_DIR)",
        "bin\$($DeployLayout.ONNX_RUNTIME_DIR)"
    )

    # Every directory that DeployLayout.h records below plugins\<library>\ must exist in the staged
    # tree. cmake/LiteBuildApi.cmake checks the same directories in the vcpkg source tree at
    # configure time and only warns, so this check covers the tree that is about to be shipped.
    # The names come from the parsed layout: a category constant already carries its
    # plugins\<library>\ prefix, so the library-to-category mapping is derived here rather than
    # restated.
    foreach ($layoutEntry in $DeployLayout.GetEnumerator()) {
        if ($layoutEntry.Value -match '^plugins\\[^\\]+\\[^\\]+$') {
            $requiredPaths += "bin\$($layoutEntry.Value)"
        }
    }

    foreach ($path in $requiredPaths) {
        $fullPath = Join-Path $ResolvedStageDir $path
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "Installer staging is missing required path: $path"
        }
    }

    $buildRuntimeDir = Join-Path $BuildDir "out\bin"
    $stageRuntimeDir = Join-Path $ResolvedStageDir "bin"
    $buildDlls = Get-ChildItem -LiteralPath $buildRuntimeDir -File -Filter "*.dll" |
        Select-Object -ExpandProperty Name
    $stageDlls = Get-ChildItem -LiteralPath $stageRuntimeDir -File -Filter "*.dll" |
        Select-Object -ExpandProperty Name
    $missingDlls = Compare-Object $stageDlls $buildDlls |
        Where-Object SideIndicator -eq "=>" |
        Select-Object -ExpandProperty InputObject
    if ($missingDlls) {
        throw "Installer staging is missing runtime DLLs: $($missingDlls -join ', ')"
    }

    # Flavor assertion: the staging tree must agree with -EnableCuda. The CMake
    # runtime gate already enforces this at build/install time; this is the
    # packaging-boundary net so a stale vcpkg tree can never slip through.
    $cudaRuntimeDir = Join-Path $ResolvedStageDir (
        "bin\$($DeployLayout.ONNX_RUNTIME_DIR)\$($DeployLayout.CUDA_RUNTIME_SUBDIR)")
    $cudaPresent = Test-Path -LiteralPath $cudaRuntimeDir
    if ($EnableCuda -and -not $cudaPresent) {
        throw ("CUDA staging is missing the ONNX Runtime cuda/ runtimes. " +
            "Run vcpkg install with --x-feature=cuda12, then rebuild.")
    }
    if (-not $EnableCuda -and $cudaPresent) {
        throw ("DML staging contains the ONNX Runtime cuda/ runtimes (vcpkg tree built " +
            "with --x-feature=cuda12?). " +
            "Rerun vcpkg install without the feature, then rebuild.")
    }
}

function Assert-InnoScript {
    param([Parameter(Mandatory)][string]$Path)

    $script = Get-Content -LiteralPath $Path -Raw
    if ($script -match '(?i)\bpostinstall\b') {
        throw "The installer must not launch the app from the elevated setup process: $Path"
    }
}

function Assert-SafeStageDirectory {
    $allowedRoot = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot "dist\stage"))
    $stage = [System.IO.Path]::GetFullPath($ResolvedStageDir)
    $comparison = [System.StringComparison]::OrdinalIgnoreCase

    if (-not ($stage.Equals($allowedRoot, $comparison) -or
            $stage.StartsWith($allowedRoot + [System.IO.Path]::DirectorySeparatorChar, $comparison))) {
        throw "Refusing to clean staging outside dist\stage: $stage"
    }
}

function Find-Vcpkg {
    $candidate = Join-Path $RepoRoot "vcpkg\vcpkg.exe"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return $candidate
    }

    throw "vcpkg.exe was not found at $candidate. Bootstrap the local vcpkg checkout first."
}

function Initialize-Signing {
    $Script:SignToolName = ""
    $Script:SignToolCommand = ""

    if (-not ($SignToolPath -or $SignCertThumbprint)) {
        return
    }

    if (-not $SignToolPath -or -not $SignCertThumbprint) {
        throw "Signing requires both -SignToolPath and -SignCertThumbprint."
    }

    $resolvedSignTool = Resolve-RepoPath $SignToolPath
    if (-not (Test-Path -LiteralPath $resolvedSignTool -PathType Leaf)) {
        throw "SignTool not found: $resolvedSignTool"
    }

    $Script:SignToolName = "ds-editor-lite"
    $Script:SignToolCommand = "`"$resolvedSignTool`" sign /fd SHA256 /sha1 $SignCertThumbprint /tr $TimestampUrl /td SHA256 `$f"
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\.."))
Set-Location $RepoRoot

Invoke-Step "Initialize Visual Studio and Qt environment" {
    Initialize-BuildEnvironment
}

$ResolvedStageDir = Resolve-RepoPath $StageDir
$ResolvedOutputDir = Resolve-RepoPath $OutputDir
$WorkDir = Join-Path $ResolvedOutputDir "work"
New-Item -ItemType Directory -Force -Path $ResolvedOutputDir, $WorkDir | Out-Null

$MetadataPath = Join-Path $WorkDir "product-metadata.json"
$ProductMetadata = Get-ProductMetadata -DestinationPath $MetadataPath
$DeployLayout = Get-DeployLayout

if (-not $VcRedistPath) {
    $VcRedistPath = $env:VC_REDIST_X64
}
if (-not $VcRedistPath) {
    throw "VC++ Redistributable path is required. Pass -VcRedistPath or set VC_REDIST_X64."
}
$ResolvedVcRedistPath = Resolve-RepoPath $VcRedistPath
if (-not (Test-Path -LiteralPath $ResolvedVcRedistPath -PathType Leaf)) {
    throw "VC++ Redistributable not found: $ResolvedVcRedistPath"
}

$ResolvedInnoSetupPath = Find-InnoSetup
Initialize-Signing
$PresetName = "package-dml-release"
$BuildDir = Join-Path $RepoRoot "build\PackageDmlRelease"
$TemplatePath = Join-Path $ScriptDir "DsEditorLite.iss.in"
$GeneratedScriptPath = Join-Path $WorkDir "DsEditorLite.iss"

if ($CheckPrerequisites) {
    $vcpkg = Find-Vcpkg
    Write-Host "Packaging prerequisites are available." -ForegroundColor Green
    Write-Host "vcpkg: $vcpkg"
    Write-Host "Inno Setup: $ResolvedInnoSetupPath"
    Write-Host "VC++ Redistributable: $ResolvedVcRedistPath"
    exit 0
}

if (-not $NoBuild) {
    $flavor = if ($EnableCuda) { "CUDA" } else { "DML" }
    if (-not $SkipVcpkgInstall) {
        Invoke-Step "Install vcpkg dependencies for $flavor package" {
            $vcpkg = Find-Vcpkg
            $vcpkgArgs = @(
                "install",
                "--x-manifest-root=$RepoRoot\scripts\vcpkg-manifest",
                "--x-install-root=$RepoRoot\vcpkg\installed",
                "--triplet=x64-windows"
            )
            if ($EnableCuda) {
                $vcpkgArgs += "--x-feature=cuda12"
            }
            Invoke-Process $vcpkg $vcpkgArgs
        }
    }

    Invoke-Step "Configure CMake preset $PresetName" {
        $configureArgs = @("--preset", $PresetName)
        if ($EnableCuda) {
            # The preset pins LITE_ENABLE_CUDA=OFF for the DML flavor; the
            # switch overrides it so this script and the CMake runtime gate
            # agree on one source of truth.
            $configureArgs += "-DLITE_ENABLE_CUDA=ON"
        }
        if ($WolfLangPackages) {
            # Passed only if the caller sets a value. Omitting the option leaves the CMake
            # resolution chain (the environment variable, the port, the sibling checkout) in
            # effect. It does not clear a cache that already contains the value (see the
            # parameter help).
            $configureArgs += "-DLITE_WOLF_LANG_PACKAGES=$WolfLangPackages"
        }
        Invoke-Process "cmake" $configureArgs
    }

    Invoke-Step "Build $PresetName" {
        Invoke-Process "cmake" @("--build", "--preset", $PresetName)
    }

    Invoke-Step "Install to staging" {
        Assert-SafeStageDirectory
        if (Test-Path -LiteralPath $ResolvedStageDir) {
            Remove-Item -LiteralPath $ResolvedStageDir -Recurse -Force
        }
        Invoke-Process "cmake" @("--install", $BuildDir, "--prefix", $ResolvedStageDir)
    }
}

Invoke-Step "Validate staging layout" {
    Assert-StagingLayout
}

Invoke-Step "Generate Inno Setup script" {
    Write-InnoScript -TemplatePath $TemplatePath -DestinationPath $GeneratedScriptPath
    Assert-InnoScript -Path $GeneratedScriptPath
    Write-Host "Generated $GeneratedScriptPath"
}

Invoke-Step "Build installer" {
    $isccArgs = @()
    if ($Script:SignToolCommand) {
        $isccArgs += @("/S$Script:SignToolName=$Script:SignToolCommand")
    }
    $isccArgs += $GeneratedScriptPath
    Invoke-Process $ResolvedInnoSetupPath $isccArgs
}

$InstallerPath = Join-Path $ResolvedOutputDir "$($ProductMetadata.executableBaseName)-$($ProductMetadata.version)-win-x64-$(if ($EnableCuda) { 'cuda' } else { 'dml' })-internal.exe"
$InstallerHash = (Get-FileHash -LiteralPath $InstallerPath -Algorithm SHA256).Hash
Write-Host ""
Write-Host "Installer created: $InstallerPath" -ForegroundColor Green
Write-Host "SHA-256: $InstallerHash"
