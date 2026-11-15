# Shared discovery for the Qt Test module and the Visual Studio CRT.
#
# The debug test binary needs Qt6Testd.dll, ucrtbased.dll and the debug MSVC runtime,
# none of which the CMake build output copies next to TestNativeDesktop.exe. The
# sandbox payload builder copies them into the payload, and the host fallback prepends
# these directories to PATH. Both call Get-GuiTrialRuntimeSearchPaths so the two paths
# stay in sync.
#
# A machine can have several Qt installations under C:\Qt. Taking the first Qt bin that
# contains Qt6Testd.dll can pair it with a Qt6Core from another version, so the Qt bin
# is selected by matching the Qt6Core[d].dll file version already beside the test exe.

function Get-QtCoreLibraryName {
    param([string]$BinSource)
    foreach ($name in 'Qt6Cored.dll', 'Qt6Core.dll') {
        if (Test-Path -LiteralPath (Join-Path $BinSource $name)) { return $name }
    }
    return $null
}

function Get-QtRuntimeBinDirectories {
    [CmdletBinding()]
    param(
        [string]$BinSource,
        [string]$QtRoot
    )
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($QtRoot) {
        $qtBin = Join-Path $QtRoot 'bin'
        if (Test-Path -LiteralPath $qtBin) { $candidates.Add($qtBin) }
    } else {
        foreach ($candidate in @(Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Get-ChildItem $_.FullName -Directory -Filter 'msvc*_64' -ErrorAction SilentlyContinue } |
            ForEach-Object { Join-Path $_.FullName 'bin' } |
            Where-Object { Test-Path -LiteralPath $_ } |
            Sort-Object)) {
            $candidates.Add($candidate)
        }
    }

    $coreName = Get-QtCoreLibraryName -BinSource $BinSource
    if (-not $coreName) {
        foreach ($candidate in $candidates) { Write-Output $candidate }
        return
    }

    $sourceCore = Join-Path $BinSource $coreName
    $sourceVersion = (Get-Item -LiteralPath $sourceCore).VersionInfo.FileVersion
    foreach ($candidate in $candidates) {
        $candidateCore = Join-Path $candidate $coreName
        if (-not (Test-Path -LiteralPath $candidateCore)) { continue }
        if (-not $sourceVersion) {
            Write-Output $candidate
            continue
        }
        $candidateVersion = (Get-Item -LiteralPath $candidateCore).VersionInfo.FileVersion
        if ($candidateVersion -eq $sourceVersion) { Write-Output $candidate }
    }
}

function Get-VisualStudioInstallPaths {
    $installs = New-Object System.Collections.Generic.List[string]
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        foreach ($line in @(& $vswhere -all -products * -property installationPath 2>$null)) {
            $candidate = ([string]$line).Trim()
            if ($candidate -and (Test-Path -LiteralPath $candidate) -and -not $installs.Contains($candidate)) {
                $installs.Add($candidate)
            }
        }
    }
    if ($installs.Count -gt 0) { return $installs.ToArray() }

    # Fallback for machines without vswhere: the layout is <root>\<year>\<edition>,
    # but a few installs place the edition directory directly under <root>.
    foreach ($vsRoot in @('C:\Program Files\Microsoft Visual Studio', 'C:\Program Files (x86)\Microsoft Visual Studio')) {
        if (-not (Test-Path -LiteralPath $vsRoot)) { continue }
        foreach ($topLevel in @(Get-ChildItem -LiteralPath $vsRoot -Directory -ErrorAction SilentlyContinue)) {
            $candidates = @($topLevel.FullName) +
                @(Get-ChildItem -LiteralPath $topLevel.FullName -Directory -ErrorAction SilentlyContinue |
                    ForEach-Object { $_.FullName })
            foreach ($candidate in $candidates) {
                if ((Test-Path -LiteralPath (Join-Path $candidate 'VC\Redist\MSVC')) -or
                    (Test-Path -LiteralPath (Join-Path $candidate 'Common7\IDE'))) {
                    if (-not $installs.Contains($candidate)) { $installs.Add($candidate) }
                }
            }
        }
    }
    return $installs.ToArray()
}

function Get-GuiTrialRuntimeSearchPaths {
    [CmdletBinding()]
    param(
        [string]$BinSource,
        [string]$QtRoot
    )
    $paths = New-Object System.Collections.Generic.List[string]
    if ($BinSource) { $paths.Add([IO.Path]::GetFullPath($BinSource).TrimEnd('\')) }
    foreach ($qtBin in @(Get-QtRuntimeBinDirectories -BinSource $BinSource -QtRoot $QtRoot)) {
        $paths.Add($qtBin)
    }

    $installs = @(Get-VisualStudioInstallPaths)
    $crtCandidates = New-Object System.Collections.Generic.List[object]
    foreach ($installPath in $installs) {
        $redistRoot = Join-Path $installPath 'VC\Redist\MSVC'
        if (-not (Test-Path -LiteralPath $redistRoot)) { continue }
        foreach ($redistVersion in @(Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue)) {
            $parsedVersion = [version]'0.0.0'
            try { $parsedVersion = [version]$redistVersion.Name } catch { }
            # The toolset name changes with the Visual Studio generation, so match
            # Microsoft.VC*.DebugCRT instead of hard-coding one toolset.
            $debugRoot = Join-Path $redistVersion.FullName 'debug_nonredist\x64'
            foreach ($crtDirectory in @(Get-ChildItem -LiteralPath $debugRoot -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like 'Microsoft.VC*.DebugCRT' })) {
                $crtCandidates.Add([pscustomobject]@{ Version = $parsedVersion; Path = $crtDirectory.FullName })
            }
        }
    }
    # Newest first: MSVC runtimes are backward compatible within a major version, and
    # ordering by version keeps the pick deterministic when several are installed.
    foreach ($candidate in @($crtCandidates | Sort-Object -Property Version -Descending)) {
        $paths.Add($candidate.Path)
    }
    foreach ($installPath in $installs) {
        $ideDirectory = Join-Path $installPath 'Common7\IDE'
        if (Test-Path -LiteralPath $ideDirectory) { $paths.Add($ideDirectory) }
    }

    $system32 = Join-Path $env:SystemRoot 'System32'
    if (Test-Path -LiteralPath $system32) { $paths.Add($system32) }

    $kitsRoot = 'C:\Program Files (x86)\Windows Kits\10\bin'
    if (Test-Path -LiteralPath $kitsRoot) {
        foreach ($kitsVersion in @(Get-ChildItem -LiteralPath $kitsRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
            $ucrtDirectory = Join-Path $kitsVersion.FullName 'x64\ucrt'
            if (Test-Path -LiteralPath $ucrtDirectory) { $paths.Add($ucrtDirectory) }
        }
    }

    return $paths.ToArray()
}

function Find-GuiTrialRuntimeFile {
    param(
        [string]$Name,
        [string[]]$SearchPaths
    )
    foreach ($directory in $SearchPaths) {
        $candidate = Join-Path $directory $Name
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}
