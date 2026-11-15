# Shared discovery for the Qt Test module and the Visual Studio CRT.
#
# The debug test binary needs Qt6Testd.dll, ucrtbased.dll and the debug MSVC runtime,
# none of which the CMake build output copies next to TestNativeDesktop.exe. The
# sandbox payload builder copies them into the payload, and the host fallback prepends
# these directories to PATH. Both call Get-GuiTrialRuntimeSearchPaths so the two paths
# stay in sync.

function Get-GuiTrialRuntimeSearchPaths {
    [CmdletBinding()]
    param(
        [string]$BinSource,
        [string]$QtRoot
    )
    $paths = New-Object System.Collections.Generic.List[string]
    if ($BinSource) { $paths.Add([IO.Path]::GetFullPath($BinSource).TrimEnd('\')) }

    if ($QtRoot) {
        $paths.Add((Join-Path $QtRoot 'bin'))
    } else {
        foreach ($qtBin in @(Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Get-ChildItem $_.FullName -Directory -Filter 'msvc*_64' -ErrorAction SilentlyContinue } |
            ForEach-Object { Join-Path $_.FullName 'bin' } |
            Where-Object { Test-Path -LiteralPath $_ })) {
            $paths.Add($qtBin)
        }
    }

    foreach ($vsRoot in @('C:\Program Files\Microsoft Visual Studio', 'C:\Program Files (x86)\Microsoft Visual Studio')) {
        if (-not (Test-Path -LiteralPath $vsRoot)) { continue }
        foreach ($versionDirectory in @(Get-ChildItem -LiteralPath $vsRoot -Directory -ErrorAction SilentlyContinue)) {
            $redistRoot = Join-Path $versionDirectory.FullName 'VC\Redist\MSVC'
            if (Test-Path -LiteralPath $redistRoot) {
                foreach ($redistVersion in @(Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue)) {
                    $debugCrt = Join-Path $redistVersion.FullName 'debug_nonredist\x64\Microsoft.VC145.DebugCRT'
                    if (Test-Path -LiteralPath $debugCrt) { $paths.Add($debugCrt) }
                }
            }
            $ideDirectory = Join-Path $versionDirectory.FullName 'Common7\IDE'
            if (Test-Path -LiteralPath $ideDirectory) { $paths.Add($ideDirectory) }
        }
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
