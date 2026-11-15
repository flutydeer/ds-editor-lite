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

function Get-QtTestModuleName {
    param([string]$BinSource)
    # The Qt Test module must match the build flavor imported by the executable.
    $coreName = Get-QtCoreLibraryName -BinSource $BinSource
    if ($coreName -eq 'Qt6Cored.dll') { return 'Qt6Testd.dll' }
    if ($coreName -eq 'Qt6Core.dll') { return 'Qt6Test.dll' }
    return $null
}

function Get-GuiTrialVcpkgRuntimeDirectories {
    param(
        [string]$BinSource,
        [string]$RepoRoot
    )
    # CTest prepends these directories to PATH; the sandbox payload and the host PATH
    # must include them too, or dynamic vcpkg dependencies fail at load time.
    $dirs = New-Object System.Collections.Generic.List[string]
    $installed = $null
    $triplet = 'x64-windows'
    $presetDirectory = Split-Path -Path (Split-Path -Path $BinSource -Parent) -Parent
    $cachePath = Join-Path $presetDirectory 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cachePath) {
        foreach ($line in Get-Content -LiteralPath $cachePath) {
            if ($line -match '^VCPKG_INSTALLED_DIR:PATH=(.+)$') {
                $installed = ([string]$Matches[1]).Replace('/', '\').Trim()
            } elseif ($line -match '^VCPKG_TARGET_TRIPLET:STRING=(.+)$') {
                $triplet = ([string]$Matches[1]).Trim()
            }
        }
    }
    if (-not $installed -and $RepoRoot) { $installed = Join-Path $RepoRoot 'vcpkg\installed' }
    if (-not $installed) { return $dirs.ToArray() }
    foreach ($candidate in @(
        (Join-Path $installed (Join-Path $triplet 'bin')),
        (Join-Path $installed (Join-Path $triplet 'debug\bin')))) {
        if (Test-Path -LiteralPath $candidate) { $dirs.Add($candidate) }
    }
    return $dirs.ToArray()
}

function Get-PeImportNames {
    param([string]$Path)
    $names = New-Object System.Collections.Generic.List[string]
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object IO.BinaryReader($stream)
        if ($stream.Length -lt 0x40) { return $names.ToArray() }
        $stream.Position = 0x3C
        $peOffset = [int]$reader.ReadInt32()
        if ($peOffset -le 0 -or ($peOffset + 24) -ge $stream.Length) { return $names.ToArray() }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { return $names.ToArray() }
        $stream.Position = $peOffset + 4
        [void]$reader.ReadUInt16()
        $sectionCount = [int]$reader.ReadUInt16()
        [void]$reader.ReadUInt32()
        [void]$reader.ReadUInt32()
        [void]$reader.ReadUInt32()
        $optionalSize = [int]$reader.ReadUInt16()
        [void]$reader.ReadUInt16()

        $optionalOffset = $peOffset + 24
        $stream.Position = $optionalOffset
        $magic = $reader.ReadUInt16()
        if ($magic -eq 0x20B) { $directoryOffset = $optionalOffset + 112 }
        elseif ($magic -eq 0x10B) { $directoryOffset = $optionalOffset + 96 }
        else { return $names.ToArray() }

        $stream.Position = $directoryOffset + 8
        $importRva = [int]$reader.ReadUInt32()
        [void]$reader.ReadUInt32()
        if ($importRva -le 0) { return $names.ToArray() }

        $sections = @()
        $sectionOffset = $optionalOffset + $optionalSize
        for ($index = 0; $index -lt $sectionCount; $index++) {
            $stream.Position = $sectionOffset + ($index * 40)
            [void]$reader.ReadBytes(8)
            $virtualSize = [int]$reader.ReadUInt32()
            $virtualAddress = [int]$reader.ReadUInt32()
            $rawSize = [int]$reader.ReadUInt32()
            $rawPointer = [int]$reader.ReadUInt32()
            $sections += [pscustomobject]@{
                VirtualSize = $virtualSize; VirtualAddress = $virtualAddress
                RawSize = $rawSize; RawPointer = $rawPointer
            }
        }
        $toOffset = {
            param([int]$rva)
            foreach ($section in $sections) {
                $size = [Math]::Max($section.VirtualSize, $section.RawSize)
                if ($rva -ge $section.VirtualAddress -and $rva -lt ($section.VirtualAddress + $size)) {
                    return $section.RawPointer + ($rva - $section.VirtualAddress)
                }
            }
            return -1
        }

        $descriptorOffset = & $toOffset $importRva
        if ($descriptorOffset -lt 0) { return $names.ToArray() }
        for ($descriptor = 0; $descriptor -lt 512; $descriptor++) {
            $stream.Position = $descriptorOffset + ($descriptor * 20)
            [void]$reader.ReadUInt32()
            [void]$reader.ReadUInt32()
            [void]$reader.ReadUInt32()
            $nameRva = [int]$reader.ReadUInt32()
            [void]$reader.ReadUInt32()
            if ($nameRva -le 0) { break }
            $nameOffset = & $toOffset $nameRva
            if ($nameOffset -lt 0) { continue }
            $stream.Position = $nameOffset
            $bytes = New-Object System.Collections.Generic.List[byte]
            while ($stream.Position -lt $stream.Length) {
                $current = $reader.ReadByte()
                if ($current -eq 0) { break }
                $bytes.Add($current)
            }
            if ($bytes.Count -gt 0 -and $bytes.Count -lt 260) {
                $names.Add([Text.Encoding]::ASCII.GetString($bytes.ToArray()))
            }
        }
    } finally {
        $stream.Dispose()
    }
    return $names.ToArray()
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
        [string]$QtRoot,
        [string]$RepoRoot
    )
    $paths = New-Object System.Collections.Generic.List[string]
    if ($BinSource) { $paths.Add([IO.Path]::GetFullPath($BinSource).TrimEnd('\')) }
    foreach ($vcpkgDirectory in @(Get-GuiTrialVcpkgRuntimeDirectories -BinSource $BinSource -RepoRoot $RepoRoot)) {
        $paths.Add($vcpkgDirectory)
    }
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
                $crtCandidates.Add([pscustomobject]@{ Version = $parsedVersion; Order = 0; Path = $crtDirectory.FullName })
            }
            # Release CRT from the same redistributable covers third-party DLLs that
            # import msvcp140_2.dll and similar release-only runtimes.
            $releaseRoot = Join-Path $redistVersion.FullName 'x64'
            foreach ($crtDirectory in @(Get-ChildItem -LiteralPath $releaseRoot -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like 'Microsoft.VC*.CRT' })) {
                $crtCandidates.Add([pscustomobject]@{ Version = $parsedVersion; Order = 1; Path = $crtDirectory.FullName })
            }
        }
    }
    # Newest first: MSVC runtimes are backward compatible within a major version, and
    # ordering by version keeps the pick deterministic when several are installed.
    foreach ($candidate in @($crtCandidates | Sort-Object -Property @{Expression={$_.Version}; Descending=$true}, @{Expression={$_.Order}; Descending=$false})) {
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

function Copy-GuiTrialRuntimeClosure {
    [CmdletBinding()]
    param(
        [string]$StagedDirectory,
        [string[]]$SearchDirectories,
        [string]$SystemDirectory
    )
    # Walk the PE import graph so dynamically linked vcpkg dependencies are staged too.
    $copied = New-Object System.Collections.Generic.List[string]
    $missing = New-Object System.Collections.Generic.List[string]
    $queue = New-Object System.Collections.Generic.Queue[string]
    $visited = @{}
    foreach ($pattern in '*.exe', '*.dll') {
        foreach ($file in @(Get-ChildItem -LiteralPath $StagedDirectory -Filter $pattern -File -ErrorAction SilentlyContinue)) {
            $queue.Enqueue($file.FullName)
        }
    }
    while ($queue.Count -gt 0) {
        $file = $queue.Dequeue()
        if ($visited.ContainsKey($file)) { continue }
        $visited[$file] = $true
        foreach ($name in @(Get-PeImportNames -Path $file)) {
            if ($name -match '^(api|ext)-ms-') { continue }
            $staged = Join-Path $StagedDirectory $name
            if (Test-Path -LiteralPath $staged) { $queue.Enqueue($staged); continue }
            if (Test-Path -LiteralPath (Join-Path $SystemDirectory $name)) { continue }
            $found = $null
            foreach ($directory in $SearchDirectories) {
                $candidate = Join-Path $directory $name
                if (Test-Path -LiteralPath $candidate) { $found = $candidate; break }
            }
            if ($null -eq $found) { $missing.Add($name); continue }
            Copy-Item -LiteralPath $found -Destination $staged
            $copied.Add($name)
            $queue.Enqueue($staged)
        }
    }
    return [pscustomobject]@{
        Copied = @($copied | Sort-Object -Unique)
        Missing = @($missing | Sort-Object -Unique)
    }
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
