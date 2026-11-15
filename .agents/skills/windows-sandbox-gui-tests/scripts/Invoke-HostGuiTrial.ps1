#Requires -Version 5.1
<#
.SYNOPSIS
    Fallback runner: execute the native GUI test cases on the current desktop session.

.DESCRIPTION
    Use this only when Windows Sandbox is unavailable. The test windows appear on the
    user's desktop and can take focus. -AllowHostGui is a caller-side opt-in, not an
    interactive confirmation, so unattended automation can pass it directly without
    asking the user.

    Unlike the sandbox path, nothing is copied and no guest registry is touched. The
    built binary runs in place with the same environment the guest uses, and the run
    records the same JUnit, console, window and screenshot evidence under
    build/<trial>/host-results/<runId>.

    This is a fallback, not the default. Prefer Start-SandboxGuiTrial.ps1 whenever the
    Sandbox feature and the hypervisor are available.

.PARAMETER Preset
    CMake preset whose build output is executed.

.PARAMETER Selectors
    QtTest selectors. An empty string runs the whole suite in one process.

.PARAMETER CaseTimeoutSeconds
    0 keeps the default: 1800 seconds for the full suite, 180 seconds per selector.

.PARAMETER AllowHostGui
    Required to actually run. It is an intent marker for the caller, not a user
    confirmation, and automation may pass it without prompting anyone.

.PARAMETER NoWindowObserver
    Skip window sampling and screenshots. Only valid for filtered runs.

.PARAMETER RequireWindowEvidence
    Require window evidence even for a filtered run with one or more selectors.
#>
[CmdletBinding()]
param(
    [string]$Preset = 'debug',
    [string]$OutputRoot = 'build\SandboxGuiTrial\host-results',
    [string]$BinSource,
    [string]$VoicebankSource,
    [string]$QtRoot,
    [string[]]$Selectors = @(''),
    [int]$CaseTimeoutSeconds = 0,
    [switch]$AllowHostGui,
    [switch]$NoWindowObserver,
    [switch]$RequireWindowEvidence
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

$exe = Join-Path $BinSource 'TestNativeDesktop.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing $exe. Build the tests first." }
if (-not (Test-Path -LiteralPath (Join-Path $BinSource 'plugins'))) { throw "Missing the Qt plugin directory under $BinSource." }
if (-not (Test-Path -LiteralPath (Join-Path $BinSource 'configs'))) { throw "Missing the embedded config directory under $BinSource." }
if (-not (Test-Path -LiteralPath (Join-Path $VoicebankSource 'desc.json'))) { throw "Missing the fixture voicebank at $VoicebankSource." }

# The build output does not contain the Qt Test module or the debug CRT, so the host
# run needs those directories on PATH just like the sandbox payload carries the files.
. (Join-Path $PSScriptRoot 'Resolve-GuiTrialRuntimePaths.ps1')
$runtimeSearchPaths = @(Get-GuiTrialRuntimeSearchPaths -BinSource $BinSource -QtRoot $QtRoot -RepoRoot $repo)
$requiredQtTest = Get-QtTestModuleName -BinSource $BinSource
if (-not $requiredQtTest) {
    throw 'Cannot tell whether the build is Debug or Release: neither Qt6Cored.dll nor Qt6Core.dll is next to the test executable.'
}
$qtTestModule = Find-GuiTrialRuntimeFile -Name $requiredQtTest -SearchPaths $runtimeSearchPaths
if (-not $qtTestModule) {
    throw "$requiredQtTest was not found on this machine. Pass -QtRoot so the Qt installation can be searched."
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

$computer = Get-CimInstance Win32_ComputerSystem
$sandboxFeature = Get-CimInstance Win32_OptionalFeature -Filter "Name='Containers-DisposableClientVM'" -ErrorAction SilentlyContinue
$sandboxCli = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\wsb.exe'
$sandboxCliReady = Test-Path -LiteralPath $sandboxCli
# The guest can only recreate the embedded config path on a substitutable drive.
# A build under C: cannot run in the sandbox at all, so the fallback must stay open.
$binSourceRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($BinSource))
# Only a real drive letter can be substed inside the guest. UNC shares and C: cannot.
$sandboxDriveSupported = ($binSourceRoot -match '^[A-Za-z]:\\') -and ($binSourceRoot.TrimEnd('\') -ne 'C:')
# The fallback is only allowed when the sandbox path is genuinely unusable, which
# includes a missing wsb.exe and a build path the guest cannot recreate.
$sandboxReady = ($null -ne $sandboxFeature -and [int]$sandboxFeature.InstallState -eq 1 -and
    [bool]$computer.HypervisorPresent -and $sandboxCliReady -and $sandboxDriveSupported)

$windowEvidenceRequired = ($cases.Count -eq 1 -and [string]::IsNullOrEmpty([string]$cases[0].selector))
if ($RequireWindowEvidence) { $windowEvidenceRequired = $true }
if ($NoWindowObserver -and $windowEvidenceRequired) {
    # The full suite is documented to open real windows. Without the observer the
    # run cannot prove that, so refuse instead of reporting an unverified pass.
    throw 'Window evidence is required for this run, so -NoWindowObserver cannot be used.'
}

[pscustomobject]@{
    Mode = 'HostDesktop'
    SandboxReady = $sandboxReady
    SandboxCliPresent = $sandboxCliReady
    SandboxDriveSupported = $sandboxDriveSupported
    Executable = $exe
    Voicebank = $VoicebankSource
    QtTestModule = $qtTestModule
    Cases = (@($cases | ForEach-Object { if ($_.selector) { $_.selector } else { '<full suite>' } }) -join ', ')
} | Format-List

if (-not $AllowHostGui) {
    Write-Host 'Inspection only. Pass -AllowHostGui to run these cases on the current desktop.'
    Write-Host 'Prefer the Windows Sandbox path when it is available.'
    exit 0
}
if ($sandboxReady) {
    throw 'Windows Sandbox is available. Use Start-SandboxGuiTrial.ps1 -Launch instead of running GUI tests on the desktop.'
}
Write-Host ''
Write-Host 'Running on the current desktop: GUI windows will open and may take focus.'
Write-Host 'No confirmation is required; the run does not wait for input.'

$runId = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $OutputRoot $runId
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$summary = [ordered]@{
    schemaVersion = 1
    runId = $runId
    mode = 'HostDesktop'
    startedUtc = (Get-Date).ToUniversalTime().ToString('o')
    overall = 'Running'
    hostInputIsolation = 'NotIsolated-UserDesktopInUse'
    coverage = 'Native desktop QtTest cases executed on the current desktop session because Windows Sandbox was unavailable. Windows may have taken focus. No registry change and no payload copy.'
    cases = @()
}
$watcher = $null
$watchDirectory = $null
$watchStopFile = $null
$activeProcess = $null
$scriptExitCode = 2

function Save-Summary {
    $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $runRoot 'summary.json') -Encoding UTF8
}

function Update-ResultFromJUnit {
    param([System.Collections.IDictionary]$Record, [string]$XmlPath, [string]$Selector)
    if (-not (Test-Path -LiteralPath $XmlPath)) { return }
    try {
        $settings = New-Object System.Xml.XmlReaderSettings
        $settings.DtdProcessing = [System.Xml.DtdProcessing]::Prohibit
        $settings.XmlResolver = $null
        $reader = [System.Xml.XmlReader]::Create($XmlPath, $settings)
        try {
            $document = New-Object System.Xml.XmlDocument
            $document.XmlResolver = $null
            $document.Load($reader)
        } finally { $reader.Dispose() }
        $fullSuite = [string]::IsNullOrEmpty($Selector)
        $allCases = @($document.SelectNodes('//testcase'))
        $executed = @($allCases | Where-Object { $_.GetAttribute('name') -notin @('initTestCase', 'initTestCase()', 'cleanupTestCase', 'cleanupTestCase()') })
        $failures = @($document.SelectNodes('//failure | //error')).Count
        $skips = @($document.SelectNodes('//skipped')).Count
        $Record['executedTestcaseCount'] = $executed.Count
        $Record['failureCount'] = $failures
        $Record['skipCount'] = $skips
        if (-not $fullSuite) {
            $parts = @($Selector -split ':', 2)
            $function = $parts[0]
            # Qt 6.11 JUnit names data rows as function(tag). A selector without a tag
            # runs every data row of that function, so all of them count as requested.
            if ($parts.Count -eq 2) {
                $requested = @($allCases | Where-Object {
                    $_.GetAttribute('name') -eq ($function + '(' + $parts[1] + ')')
                })
            } else {
                $requested = @($allCases | Where-Object {
                    $name = $_.GetAttribute('name')
                    $name -eq $function -or $name -eq ($function + '()') -or
                        $name.StartsWith($function + '(', [StringComparison]::Ordinal)
                })
            }
            $Record['selectedTestcaseCount'] = $requested.Count
        }
        if (-not $Record.timedOut -and $Record.exitCode -eq 0) {
            if ($failures -gt 0) { $Record.status = 'Failed' }
            elseif ($fullSuite -and $executed.Count -eq 0) { $Record.status = 'NoTestcasesExecuted' }
            elseif ($fullSuite -and $skips -gt 0) { $Record.status = 'PassedWithSkips' }
            elseif ($fullSuite) { $Record.status = 'Passed' }
            elseif ($skips -gt 0) { $Record.status = 'Skipped-Unverified' }
            elseif ($requested.Count -lt 1 -or $executed.Count -ne $requested.Count) {
                $Record.status = 'RequestedCaseNotExecutedExactlyOnce'
            } else { $Record.status = 'Passed' }
        }
    } catch {
        $Record['junitParseError'] = $_.Exception.Message
        if ($Record.status -eq 'NoResults') { $Record.status = 'InvalidResults' }
    }
}

function Stop-OwnedTest {
    param($Process)
    if ($null -ne $Process -and -not $Process.HasExited) {
        & "$env:SystemRoot\System32\taskkill.exe" /PID $Process.Id /T /F 2>&1 |
            Out-File -LiteralPath (Join-Path $runRoot 'terminated-test-tree.txt') -Append -Encoding UTF8
        if (-not $Process.WaitForExit(10000)) {
            throw 'The timed-out test process did not exit.'
        }
    }
}

try {
    $pathEntries = @($BinSource) + @($runtimeSearchPaths) + @($env:SystemRoot)
    $env:PATH = (@($pathEntries | Select-Object -Unique) -join ';')
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_PLUGIN_PATH = Join-Path $BinSource 'plugins'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $BinSource 'plugins\platforms'
    $env:QT_FORCE_STDERR_LOGGING = '1'
    $env:QT_DEBUG_PLUGINS = '1'
    $env:QTEST_FUNCTION_TIMEOUT = '300000'
    $env:DSEL_TEST_PLUGIN_ROOT = Join-Path $BinSource 'plugins'
    $env:DSEL_TEST_VOICEBANK_ROOT = $VoicebankSource
    $env:DSEL_TEST_SINGER_ID = 'fixture'
    $env:DSEL_TEST_LANGUAGE = 'cmn'
    $env:DSEL_TEST_LYRIC = 'la'
    Remove-Item Env:DSEL_TEST_GUI_LIFECYCLE -ErrorAction SilentlyContinue

    [ordered]@{
        timestampUtc = (Get-Date).ToUniversalTime().ToString('o')
        os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber
        computer = $computer | Select-Object Manufacturer, Model
        video = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, VideoProcessor)
        powershell = $PSVersionTable.PSVersion.ToString()
        platform = $env:QT_QPA_PLATFORM
        sandboxReady = $sandboxReady
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runRoot 'environment.json') -Encoding UTF8

    if (-not $NoWindowObserver) {
        $watcherScript = Join-Path $PSScriptRoot 'Watch-Windows.ps1'
        if (-not (Test-Path -LiteralPath $watcherScript)) { throw "Missing $watcherScript." }
        $watchDirectory = Join-Path $runRoot 'windows'
        New-Item -ItemType Directory -Path $watchDirectory -Force | Out-Null
        $watchStopFile = Join-Path ([IO.Path]::GetTempPath()) ('sandbox-gui-watch-' + $runId + '.stop')
        Remove-Item -LiteralPath $watchStopFile -ErrorAction SilentlyContinue
        $watchScopeStart = (Get-Date).AddSeconds(-1).ToString('o')
        $watcher = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
            -ArgumentList @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', ('"' + $watcherScript + '"'),
                '-OutputDirectory', ('"' + $watchDirectory + '"'),
                '-StopFile', ('"' + $watchStopFile + '"'),
                '-ScopeStartLocal', ('"' + $watchScopeStart + '"'), '-AllowHostDesktop') `
            -PassThru -WindowStyle Hidden
    }

    Save-Summary
    foreach ($case in $cases) {
        $caseId = [string]$case.id
        $selector = [string]$case.selector
        if ($selector -match '["%\r\n]') {
            throw "Selector contains unsupported characters: $selector"
        }
        $caseTimeout = [int]$case.timeoutSeconds
        $xmlPath = Join-Path $runRoot ($caseId + '.junit.xml')
        $stdout = Join-Path $runRoot ($caseId + '.stdout.txt')
        $stderr = Join-Path $runRoot ($caseId + '.stderr.txt')
        $record = [ordered]@{
            id = $caseId
            selector = $selector
            startedUtc = (Get-Date).ToUniversalTime().ToString('o')
            timeoutSeconds = $caseTimeout
            timedOut = $false
            status = 'Starting'
            exitCode = $null
            exitCodeHex = $null
        }
        Write-Host "Starting $caseId ($selector)"
        $clock = [Diagnostics.Stopwatch]::StartNew()
        if ([string]::IsNullOrEmpty($selector)) {
            $commandLine = '""{0}" -v1 -o "{1},junitxml" -o -,txt 1>"{2}" 2>"{3}""' -f $exe, $xmlPath, $stdout, $stderr
        } else {
            $commandLine = '""{0}" "{1}" -v1 -o "{2},junitxml" -o -,txt 1>"{3}" 2>"{4}""' -f $exe, $selector, $xmlPath, $stdout, $stderr
        }
        $activeProcess = Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" `
            -ArgumentList ('/d /s /c ' + $commandLine) -WorkingDirectory $BinSource -PassThru -NoNewWindow
        $null = $activeProcess.Handle
        $record['processId'] = $activeProcess.Id
        if (-not $activeProcess.WaitForExit($caseTimeout * 1000)) {
            $record.timedOut = $true
            $record.status = 'TimedOut'
            Stop-OwnedTest $activeProcess
        } else {
            $code = [int]$activeProcess.ExitCode
            $record.exitCode = $code
            $unsignedCode = [BitConverter]::ToUInt32([BitConverter]::GetBytes($code), 0)
            $record.exitCodeHex = '0x{0:X8}' -f $unsignedCode
            if ($unsignedCode -ge 2147483648) { $record.status = 'CrashOrLoaderError' }
            elseif ($code -ne 0) { $record.status = 'Failed' }
            else { $record.status = 'NoResults' }
        }
        $clock.Stop()
        $record['elapsedMs'] = $clock.ElapsedMilliseconds
        $record['finishedUtc'] = (Get-Date).ToUniversalTime().ToString('o')
        $activeProcess.Dispose()
        $activeProcess = $null

        Update-ResultFromJUnit -Record $record -XmlPath $xmlPath -Selector $selector
        $summary.cases += [pscustomobject]$record
        Save-Summary
        Write-Host "$($caseId): $($record.status)"
    }

    $windowSummary = $null
    if ($null -ne $watcher) {
        try {
            Set-Content -LiteralPath $watchStopFile -Value 'stop' -Encoding ASCII
            if (-not $watcher.WaitForExit(15000)) {
                Stop-Process -Id $watcher.Id -Force -ErrorAction SilentlyContinue
            }
            $watchSummaryPath = Join-Path $watchDirectory 'windows-summary.json'
            if (Test-Path -LiteralPath $watchSummaryPath) {
                $windowSummary = Get-Content -LiteralPath $watchSummaryPath -Raw | ConvertFrom-Json
                $summary['windowObservation'] = $windowSummary
            }
        } catch {
            $summary['windowObservationError'] = $_.Exception.Message
        }
        $watcher = $null
    }
    $windowEvidenceOk = $true
    if ($windowEvidenceRequired) {
        if ($null -eq $windowSummary) { $windowEvidenceOk = $false }
        elseif ([int]$windowSummary.testWindowSamples -le 0) { $windowEvidenceOk = $false }
    }
    $summary['windowEvidenceRequired'] = $windowEvidenceRequired
    $summary['windowEvidenceSatisfied'] = $windowEvidenceOk

    $unexpected = @($summary.cases | Where-Object { $_.status -notin @('Passed', 'PassedWithSkips', 'Skipped-Unverified') })
    $withSkips = @($summary.cases | Where-Object { $_.status -in @('PassedWithSkips', 'Skipped-Unverified') })
    if ($unexpected.Count -gt 0) {
        $summary.overall = 'Failed'
        $scriptExitCode = 1
    } elseif (-not $windowEvidenceOk) {
        $summary.overall = 'WindowEvidenceMissing'
        $scriptExitCode = 6
    } elseif ($withSkips.Count -gt 0) {
        $summary.overall = 'Incomplete-SkippedCoverage'
        $scriptExitCode = 3
    } else {
        $summary.overall = 'AllSelectedCasesPassed'
        $scriptExitCode = 0
    }
} catch {
    $summary.overall = 'PreparationOrRunnerFailed'
    $summary['fatalError'] = $_.Exception.ToString()
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $runRoot 'fatal-error.txt') -Encoding UTF8
    if ($null -ne $activeProcess) {
        try { Stop-OwnedTest $activeProcess } catch { $summary['cleanupError'] = $_.Exception.Message }
    }
} finally {
    if ($null -ne $watcher) {
        try {
            Set-Content -LiteralPath $watchStopFile -Value 'stop' -Encoding ASCII
            if (-not $watcher.WaitForExit(15000)) {
                Stop-Process -Id $watcher.Id -Force -ErrorAction SilentlyContinue
            }
            $watchSummaryPath = Join-Path $watchDirectory 'windows-summary.json'
            if (Test-Path -LiteralPath $watchSummaryPath) {
                $summary['windowObservation'] = Get-Content -LiteralPath $watchSummaryPath -Raw | ConvertFrom-Json
            }
        } catch {
            $summary['windowObservationError'] = $_.Exception.Message
        }
    }
    $summary['finishedUtc'] = (Get-Date).ToUniversalTime().ToString('o')
    $summary['runnerExitCode'] = $scriptExitCode
    Save-Summary
    "Run finished on the current desktop. Overall: $($summary.overall). See summary.json and the per-case logs." |
        Set-Content -LiteralPath (Join-Path $runRoot 'DONE.txt') -Encoding UTF8
}
exit $scriptExitCode
