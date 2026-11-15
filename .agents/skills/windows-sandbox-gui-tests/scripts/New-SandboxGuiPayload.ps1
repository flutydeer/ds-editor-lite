#Requires -Version 5.1
<#
.SYNOPSIS
    Stage a read-only Windows Sandbox payload for DS Editor Lite GUI test cases.

.DESCRIPTION
    Copies the existing test build and the CI fixture voicebank into a trial payload,
    writes plan.json so the guest runner knows what to execute, and records a SHA-256
    manifest that both the host launcher and the guest runner verify before any test
    starts. Nothing here executes the product or the test binary.

.PARAMETER Preset
    CMake preset whose build output is staged (debug or release).

.PARAMETER OutputRoot
    Trial directory. Must live under the repository build directory.

.PARAMETER BinSource
    Directory that contains TestNativeDesktop.exe. Defaults to build/<Preset>/out/bin.

.PARAMETER VoicebankSource
    Fixture voicebank directory. Defaults to build/<Preset>/src/tests/resources/ci-fixture@1.0.0.

.PARAMETER Selectors
    QtTest selectors. An empty string runs the whole suite in one process.

.PARAMETER CaseTimeoutSeconds
    0 keeps the default: 1800 seconds for the full suite, 180 seconds per selector.

.PARAMETER UseHardLinks
    Stage payload files as hard links instead of copies. Only valid on one volume.

.PARAMETER Force
    Replace an existing payload directory.
#>
[CmdletBinding()]
param(
    [string]$Preset = 'debug',
    [string]$OutputRoot = 'build\SandboxGuiTrial',
    [string]$BinSource,
    [string]$VoicebankSource,
    [string]$QtRoot,
    [string[]]$Selectors = @(''),
    [int]$CaseTimeoutSeconds = 0,
    [switch]$UseHardLinks,
    [switch]$IncludeSymbols,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
if (-not (Test-Path -LiteralPath (Join-Path $repo 'AGENTS.md'))) {
    throw 'Cannot locate the repository root from this script location.'
}

if (-not $BinSource) { $BinSource = Join-Path $repo (Join-Path 'build' (Join-Path $Preset 'out\bin')) }
if (-not $VoicebankSource) {
    $VoicebankSource = Join-Path $repo (Join-Path 'build' (Join-Path $Preset 'src\tests\resources\ci-fixture@1.0.0'))
}
if (-not [IO.Path]::IsPathRooted($OutputRoot)) { $OutputRoot = Join-Path $repo $OutputRoot }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
$buildRoot = ([IO.Path]::GetFullPath((Join-Path $repo 'build'))).TrimEnd('\') + '\'
if (-not $OutputRoot.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must stay under $buildRoot so trial output never enters the repository."
}

$payload = Join-Path $OutputRoot 'payload'
$results = Join-Path $OutputRoot 'results'

$exeName = 'TestNativeDesktop.exe'
$exePath = Join-Path $BinSource $exeName
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Missing $exeName under $BinSource. Build the debug preset with tests enabled first."
}
if (-not (Test-Path -LiteralPath (Join-Path $BinSource 'plugins'))) {
    throw "Missing the Qt plugin directory under $BinSource."
}
if (-not (Test-Path -LiteralPath (Join-Path $BinSource 'configs'))) {
    throw "Missing the embedded config directory under $BinSource."
}
if (-not (Test-Path -LiteralPath (Join-Path $VoicebankSource 'desc.json'))) {
    throw "Missing the fixture voicebank at $VoicebankSource."
}

# The guest recreates the build-time config path with subst, so a C: build can never
# be staged. Point the caller at the host fallback instead of producing a payload
# that fails at run time.
$binSourceDrive = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($BinSource)).TrimEnd('\')
if ($binSourceDrive -eq 'C:') {
    throw 'The build output is on C:, so Windows Sandbox cannot recreate the embedded config path. Run Invoke-HostGuiTrial.ps1 on this machine instead.'
}

$cases = @()
if ($Selectors.Count -eq 1 -and [string]::IsNullOrEmpty($Selectors[0])) {
    $timeoutSeconds = if ($CaseTimeoutSeconds -gt 0) { $CaseTimeoutSeconds } else { 1800 }
    $cases += [ordered]@{ id = '00-full-suite'; selector = ''; timeoutSeconds = $timeoutSeconds }
} else {
    $timeoutSeconds = if ($CaseTimeoutSeconds -gt 0) { $CaseTimeoutSeconds } else { 180 }
    $index = 0
    foreach ($selector in $Selectors) {
        if ([string]::IsNullOrWhiteSpace($selector)) { continue }
        $index++
        $safeId = $selector -replace '[^A-Za-z0-9._-]', '-'
        $cases += [ordered]@{
            id = ('{0:D2}-{1}' -f $index, $safeId)
            selector = $selector.Trim()
            timeoutSeconds = $timeoutSeconds
        }
    }
    if ($cases.Count -eq 0) { throw 'No selector was supplied.' }
}

if (Test-Path -LiteralPath $payload) {
    if (-not $Force) { throw "Payload already exists at $payload. Pass -Force to rebuild it." }
    Remove-Item -LiteralPath $payload -Recurse -Force
}
New-Item -ItemType Directory -Path $payload -Force | Out-Null
New-Item -ItemType Directory -Path $results -Force | Out-Null

$symbolExtensions = @('.pdb', '.ilk', '.lib', '.exp', '.obj', '.iobj', '.ipdb', '.map', '.bsc', '.idb', '.tlog', '.res')

function Copy-Tree {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$KeepOnlyExecutable
    )
    $sourceRoot = [IO.Path]::GetFullPath($Source).TrimEnd('\')
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $skipped = 0
    foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
        $relative = $file.FullName.Substring($sourceRoot.Length + 1)
        $extension = [IO.Path]::GetExtension($file.Name).ToLowerInvariant()
        if (-not $IncludeSymbols -and ($symbolExtensions -contains $extension)) {
            $skipped++
            continue
        }
        # Top-level test executables other than the entry point are never loaded by
        # this run. Symbols and unrelated executables dominate the raw build output.
        if ($KeepOnlyExecutable -and $extension -eq '.exe' -and $relative -notmatch '\\' -and
            $file.Name -ine $KeepOnlyExecutable) {
            $skipped++
            continue
        }
        $target = Join-Path $Destination $relative
        $targetParent = Split-Path -Path $target -Parent
        if (-not (Test-Path -LiteralPath $targetParent)) {
            New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
        }
        if ($UseHardLinks) {
            New-Item -ItemType HardLink -Path $target -Target $file.FullName | Out-Null
        } else {
            Copy-Item -LiteralPath $file.FullName -Destination $target
        }
    }
    return $skipped
}

Write-Host "Staging $BinSource"
$skippedFiles = Copy-Tree -Source $BinSource -Destination (Join-Path $payload 'bin') -KeepOnlyExecutable $exeName
Write-Host "Staging $VoicebankSource"
$skippedFiles += Copy-Tree -Source $VoicebankSource -Destination (Join-Path $payload 'voicebank')
foreach ($scriptName in 'Run-Guest.ps1', 'Watch-Windows.ps1') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $scriptName) -Destination (Join-Path $payload $scriptName)
}

# The build output does not contain the Qt Test module or the debug CRT. Pull the
# runtime files the test binary needs from the Qt install and the Visual Studio CRT
# redistributables so the guest never fails with STATUS_DLL_NOT_FOUND (0xC0000135).
. (Join-Path $PSScriptRoot 'Resolve-GuiTrialRuntimePaths.ps1')
$searchDirectories = @(Get-GuiTrialRuntimeSearchPaths -BinSource $BinSource -QtRoot $QtRoot)

$binDestination = Join-Path $payload 'bin'
$runtimeFilesCopied = @()
$missingRuntimeFiles = @()
foreach ($runtimeName in @(
    'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2d.dll', 'msvcp140d.dll', 'msvcp140_1d.dll',
    'vcruntime140.dll', 'vcruntime140_1.dll', 'vcruntime140_1d.dll', 'vcruntime140d.dll',
    'ucrtbased.dll', 'Qt6Testd.dll', 'Qt6Test.dll')) {
    $target = Join-Path $binDestination $runtimeName
    if (Test-Path -LiteralPath $target) { continue }
    $found = $null
    foreach ($directory in $searchDirectories) {
        $candidate = Join-Path $directory $runtimeName
        if (Test-Path -LiteralPath $candidate) { $found = $candidate; break }
    }
    if ($null -eq $found) {
        $missingRuntimeFiles += $runtimeName
        continue
    }
    Copy-Item -LiteralPath $found -Destination $target
    $runtimeFilesCopied += $runtimeName
}
$qtTestPresent = (Test-Path -LiteralPath (Join-Path $binDestination 'Qt6Testd.dll')) -or
    (Test-Path -LiteralPath (Join-Path $binDestination 'Qt6Test.dll'))
if (-not $qtTestPresent) {
    throw 'Qt6Test.dll was not found for the Qt version used by the build. Pass -QtRoot pointing at the matching Qt installation.'
}
$requiredMissing = @($missingRuntimeFiles | Where-Object { $_ -notin @('Qt6Testd.dll', 'Qt6Test.dll') })
if ($requiredMissing.Count -gt 0) {
    throw ('Missing required runtime DLLs: ' + ($requiredMissing -join ', ') + '. Install the Visual Studio debug CRT and the Windows SDK.')
}

$plan = [ordered]@{
    schemaVersion = 1
    createdUtc = (Get-Date).ToUniversalTime().ToString('o')
    preset = $Preset
    executable = 'bin/TestNativeDesktop.exe'
    workingDirectory = 'bin'
    embeddedConfigPath = [IO.Path]::GetFullPath($BinSource).TrimEnd('\')
    configSource = 'bin/configs'
    voicebankRoot = 'voicebank'
    environment = [ordered]@{
        DSEL_TEST_SINGER_ID = 'fixture'
        DSEL_TEST_LANGUAGE = 'cmn'
        DSEL_TEST_LYRIC = 'la'
    }
    midiWorkaround = $true
    windowObserver = $true
    # The whole suite is known to open real windows, so a run without window evidence
    # is a failure. A single selector may legitimately render offscreen, so it is not
    # required unless plan.json is edited by hand.
    requireWindowEvidence = ($cases.Count -eq 1 -and [string]::IsNullOrEmpty([string]$cases[0].selector))
    cases = $cases
}
$plan | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $payload 'plan.json') -Encoding UTF8

$files = @()
foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File) {
    if ($file.Name -eq 'manifest.json') { continue }
    $relative = $file.FullName.Substring($payload.Length + 1).Replace('\', '/')
    $files += [pscustomobject][ordered]@{
        path = $relative
        size = [int64]$file.Length
        sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
}
$totalBytes = [int64]0
foreach ($entry in $files) { $totalBytes += $entry.size }
$manifest = [ordered]@{
    schemaVersion = 1
    createdUtc = (Get-Date).ToUniversalTime().ToString('o')
    preset = $Preset
    localDevelopmentOnly = $true
    redistributable = $false
    binSource = $BinSource
    voicebankSource = $VoicebankSource
    entryPoint = 'bin/TestNativeDesktop.exe'
    fileCount = $files.Count
    totalBytes = $totalBytes
    totalMiB = [math]::Round($totalBytes / 1MB, 3)
    hardLinked = [bool]$UseHardLinks
    skippedBuildArtifacts = $skippedFiles
    runtimeFilesCopied = @($runtimeFilesCopied)
    files = $files
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $payload 'manifest.json') -Encoding UTF8

[pscustomobject]@{
    TrialRoot = $OutputRoot
    Payload = $payload
    Files = $files.Count
    TotalMiB = $manifest.totalMiB
    Cases = (@($cases | ForEach-Object { if ($_.selector) { $_.selector } else { '<full suite>' } }) -join ', ')
    HardLinked = [bool]$UseHardLinks
    SkippedBuildArtifacts = $skippedFiles
    RuntimeFilesCopied = $runtimeFilesCopied.Count
} | Format-List
Write-Host 'Next: .agents\skills\windows-sandbox-gui-tests\scripts\Start-SandboxGuiTrial.ps1 -Launch'
