# Local development experiment. Runs only inside Windows Sandbox.
#
# Reads plan.json from the read-only payload mapping, copies the test build into the
# guest work directory, applies the guest-only MIDI enumeration workaround, runs every
# planned QtTest case with a bounded outer timeout, records JUnit results, and keeps a
# passive window observer alive for the whole run.
[CmdletBinding()]
param([ValidateRange(0, 120)][int]$StartDelaySeconds = 20)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

# Fail closed before creating files or changing the environment.
$inputRoot = 'C:\SandboxTrialInput'
$resultsRoot = 'C:\SandboxTrialResults'
if ($PSScriptRoot.TrimEnd('\') -ine $inputRoot -or $env:USERNAME -ine 'WDAGUtilityAccount') {
    throw 'Guest-only entry point. Launch the generated .wsb file instead. No host tests were started.'
}
$computer = Get-CimInstance Win32_ComputerSystem
if ($computer.Manufacturer -ne 'Microsoft Corporation' -or $computer.Model -notmatch 'Virtual Machine') {
    throw 'Expected the Microsoft virtual machine used by Windows Sandbox. Refusing to proceed.'
}
if (-not (Test-Path -LiteralPath $resultsRoot -PathType Container)) {
    throw 'The dedicated results mapping is missing.'
}

$plan = Get-Content -LiteralPath (Join-Path $inputRoot 'plan.json') -Raw | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace([string]$plan.executable)) { throw 'plan.json has no executable.' }
$runCases = @($plan.cases)
if ($runCases.Count -eq 0) { throw 'plan.json has no cases.' }
$requireWindowEvidence = $false
if ($plan.PSObject.Properties.Name -contains 'requireWindowEvidence') {
    $requireWindowEvidence = [bool]$plan.requireWindowEvidence
}

$runId = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $resultsRoot $runId
New-Item -ItemType Directory -Path $runRoot | Out-Null
$summary = [ordered]@{
    schemaVersion = 1
    runId = $runId
    startedUtc = (Get-Date).ToUniversalTime().ToString('o')
    overall = 'Preparing'
    hostInputIsolation = 'NotMeasuredByGuest-UserMustObserveHost'
    backgroundRendering = 'NotAutomaticallyVerified'
    coverage = 'Native desktop QtTest cases executed inside Windows Sandbox. A guest-only MIDI enumeration workaround is applied because the Sandbox cannot start MidiSrv. RHI cases may prefer software rendering. Synthetic touch is not physical touch passthrough.'
    plan = [ordered]@{
        preset = [string]$plan.preset
        executable = [string]$plan.executable
        embeddedConfigPath = [string]$plan.embeddedConfigPath
        requireWindowEvidence = $requireWindowEvidence
        cases = @($runCases | ForEach-Object {
            [ordered]@{ id = [string]$_.id; selector = [string]$_.selector; timeoutSeconds = [int]$_.timeoutSeconds }
        })
    }
    cases = @()
}
$activeProcess = $null
$watcher = $null
$transcriptStarted = $false
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
        # An empty selector means the whole suite ran in one process.
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
            # Qt 6.11 JUnit names data rows as function(tag). Non-data cases may
            # appear as function or function(), depending on enterTestData().
            $expectedNames = @($function, ($function + '()'))
            if ($parts.Count -eq 2) { $expectedNames = @($function + '(' + $parts[1] + ')') }
            $requested = @($allCases | Where-Object { $expectedNames -ccontains $_.GetAttribute('name') })
            $Record['selectedTestcaseCount'] = $requested.Count
        }
        if (-not $Record.timedOut -and $Record.exitCode -eq 0) {
            if ($failures -gt 0) { $Record.status = 'Failed' }
            elseif ($fullSuite -and $executed.Count -eq 0) { $Record.status = 'NoTestcasesExecuted' }
            elseif ($fullSuite -and $skips -gt 0) { $Record.status = 'PassedWithSkips' }
            elseif ($fullSuite) { $Record.status = 'Passed' }
            elseif ($skips -gt 0) { $Record.status = 'Skipped-Unverified' }
            elseif ($requested.Count -ne 1 -or $executed.Count -ne 1) { $Record.status = 'RequestedCaseNotExecutedExactlyOnce' }
            else { $Record.status = 'Passed' }
        }
    } catch {
        $Record['junitParseError'] = $_.Exception.Message
        if ($Record.status -eq 'NoResults') { $Record.status = 'InvalidResults' }
    }
}

function Stop-OwnedTest {
    param($Process)
    if ($null -ne $Process -and -not $Process.HasExited) {
        # This function is reachable only after the guest guard above.
        & "$env:SystemRoot\System32\taskkill.exe" /PID $Process.Id /T /F 2>&1 |
            Out-File -LiteralPath (Join-Path $runRoot 'terminated-test-tree.txt') -Append -Encoding UTF8
        if (-not $Process.WaitForExit(10000)) {
            throw 'The timed-out guest test process did not exit. Refusing to start another case.'
        }
    }
}

try {
    Start-Transcript -LiteralPath (Join-Path $runRoot 'runner.log') | Out-Null
    $transcriptStarted = $true
    Save-Summary
    Write-Host "Guest run: $runId"
    Write-Host "Tests begin after $StartDelaySeconds seconds. Minimize or leave the Sandbox window now."
    Write-Host 'Do not interact with the guest while the tests run. Host input isolation still requires observation.'
    Start-Sleep -Seconds $StartDelaySeconds

    $manifest = Get-Content -LiteralPath (Join-Path $inputRoot 'manifest.json') -Raw | ConvertFrom-Json
    if (@($manifest.files).Count -eq 0) { throw 'The payload manifest is empty.' }
    foreach ($entry in $manifest.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $inputRoot $entry.path))
        if (-not $path.StartsWith($inputRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Invalid manifest path: $($entry.path)"
        }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $entry.sha256) {
            throw "Payload hash mismatch: $($entry.path)"
        }
    }

    $dependencyReportPath = Join-Path $inputRoot 'dependency-report.json'
    if (Test-Path -LiteralPath $dependencyReportPath) {
        $dependencyReport = Get-Content -LiteralPath $dependencyReportPath -Raw | ConvertFrom-Json
        $missingSystem = @()
        foreach ($dependency in $dependencyReport.systemDependencies) {
            if ($dependency.name -match '^(api|ext)-ms-') { continue }
            if (-not (Test-Path -LiteralPath (Join-Path "$env:SystemRoot\System32" $dependency.name))) {
                $missingSystem += $dependency.name
            }
        }
        $summary['missingGuestSystemDlls'] = @($missingSystem | Sort-Object -Unique)
        $summary['staticMissingPayloadDependencies'] = @($dependencyReport.missing)
    } else {
        $summary['missingGuestSystemDlls'] = @()
        $summary['staticMissingPayloadDependencies'] = @()
    }

    $workRoot = 'C:\SandboxTrialWork'
    if (Test-Path -LiteralPath $workRoot) { throw 'Guest work directory already exists. Close and relaunch Sandbox for a clean run.' }
    New-Item -ItemType Directory -Path $workRoot | Out-Null
    $executableRelative = ([string]$plan.executable).Replace('/', '\')
    $voicebankRelative = ([string]$plan.voicebankRoot).Replace('/', '\')
    $copyRoots = New-Object System.Collections.Generic.List[string]
    foreach ($relativePath in @($executableRelative, $voicebankRelative)) {
        $topLevel = ($relativePath -split '\\')[0]
        if (-not $copyRoots.Contains($topLevel)) { $copyRoots.Add($topLevel) }
    }
    foreach ($copyRoot in $copyRoots) {
        Copy-Item -LiteralPath (Join-Path $inputRoot $copyRoot) -Destination $workRoot -Recurse
    }
    $exe = Join-Path $workRoot $executableRelative
    $exeDirectory = Split-Path -Path $exe -Parent
    if (-not (Test-Path -LiteralPath $exe)) { throw "The test executable is missing from the payload copy: $executableRelative" }

    # The existing test binary embeds the absolute build path for its config tree.
    # Recreate only that path inside this disposable guest.
    $embedded = [string]$plan.embeddedConfigPath
    $driveMatch = [regex]::Match($embedded, '^([A-Za-z]):\\(.+)$')
    if (-not $driveMatch.Success) { throw "plan.json has an unusable embeddedConfigPath: $embedded" }
    $driveLetter = $driveMatch.Groups[1].Value.ToUpperInvariant()
    if ($driveLetter -eq 'C') {
        throw 'The test binary embeds a path on C:, which cannot be recreated in the guest. Build from a non-system drive.'
    }
    $relativeAfterDrive = $driveMatch.Groups[2].Value
    $driveSpec = $driveLetter + ':'
    if (Test-Path -LiteralPath ($driveSpec + '\')) { throw "Guest already has drive $driveSpec. Refusing to overwrite it." }
    $driveRoot = Join-Path $workRoot ('drive' + $driveLetter)
    New-Item -ItemType Directory -Path $driveRoot | Out-Null
    & "$env:SystemRoot\System32\subst.exe" $driveSpec $driveRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not create the guest-only $driveSpec drive." }
    $compiledParent = $driveSpec + '\' + $relativeAfterDrive
    New-Item -ItemType Directory -Path $compiledParent -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $inputRoot $plan.configSource) -Destination $compiledParent -Recurse

    $env:PATH = "$exeDirectory;$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_PLUGIN_PATH = Join-Path $exeDirectory 'plugins'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $exeDirectory 'plugins\platforms'
    $env:QT_FORCE_STDERR_LOGGING = '1'
    $env:QT_DEBUG_PLUGINS = '1'
    $env:QTEST_FUNCTION_TIMEOUT = '300000'
    $env:DSEL_TEST_PLUGIN_ROOT = Join-Path $exeDirectory 'plugins'
    $env:DSEL_TEST_VOICEBANK_ROOT = Join-Path $workRoot $voicebankRelative
    if ($null -ne $plan.environment) {
        foreach ($property in $plan.environment.PSObject.Properties) {
            Set-Item -Path ('Env:' + $property.Name) -Value ([string]$property.Value)
        }
    }
    Remove-Item Env:DSEL_TEST_GUI_LIFECYCLE -ErrorAction SilentlyContinue

    if ($plan.midiWorkaround -eq $true) {
        # Windows Sandbox cannot start the Windows MIDI service: midisrv.exe exits
        # with 0x80073B01 during boot, and winmm's wdmaud2 shim then blocks forever
        # inside midiInGetNumDevs. The 64-bit Drivers32 midi1 value is what hands MIDI
        # enumeration to midisrv, so drop it in this disposable guest only. This
        # changes neither product code nor host state, and the guest has no MIDI
        # hardware, so no test depends on a MIDI port being present.
        $midiDriversKey = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32'
        # Strict mode rejects reading a value name that no longer exists, so read
        # through PSObject properties and treat a missing value as absent.
        $readMidiDriverValue = {
            param([string]$Name)
            $item = Get-ItemProperty -LiteralPath $midiDriversKey -Name $Name -ErrorAction SilentlyContinue
            if ($null -eq $item) { return $null }
            $property = $item.PSObject.Properties[$Name]
            if ($null -eq $property) { return $null }
            return [string]$property.Value
        }
        $originalMidi1 = & $readMidiDriverValue 'midi1'
        if ($null -ne $originalMidi1) {
            Remove-ItemProperty -LiteralPath $midiDriversKey -Name 'midi1' -ErrorAction Stop
            $midiAction = 'Removed the Drivers32 midi1 value that routes enumeration to midisrv.'
        } else {
            $midiAction = 'No midi1 value was present.'
        }
        $summary['midiEnumeration'] = [ordered]@{
            appliedUtc = (Get-Date).ToUniversalTime().ToString('o')
            reason = 'Guest MidiSrv fails to start, so winmm MIDI enumeration blocks forever.'
            originalMidi1 = $originalMidi1
            midi1After = & $readMidiDriverValue 'midi1'
            action = $midiAction
        }

        # Verify the workaround before spending a timeout window on the real tests.
        $midiProbeScript = Join-Path $workRoot 'midi-probe.ps1'
        @'
param([string]$ResultPath)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
namespace SandboxTrialMidiProbe {
    public static class Api {
        [DllImport("winmm.dll", ExactSpelling = true)]
        public static extern uint midiInGetNumDevs();
    }
}
"@
$clock = [Diagnostics.Stopwatch]::StartNew()
$count = [SandboxTrialMidiProbe.Api]::midiInGetNumDevs()
$clock.Stop()
[ordered]@{ count = [int]$count; elapsedMs = $clock.ElapsedMilliseconds } |
    ConvertTo-Json | Set-Content -LiteralPath $ResultPath -Encoding UTF8
'@ | Set-Content -LiteralPath $midiProbeScript -Encoding UTF8
        $midiProbeResult = Join-Path $workRoot 'midi-probe-result.json'
        Remove-Item -LiteralPath $midiProbeResult -ErrorAction SilentlyContinue
        $midiProbe = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
            -ArgumentList @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', ('"' + $midiProbeScript + '"'), '-ResultPath', ('"' + $midiProbeResult + '"')) `
            -PassThru -WindowStyle Hidden
        $midiProbeExited = $midiProbe.WaitForExit(20000)
        if (-not $midiProbeExited) {
            Stop-Process -Id $midiProbe.Id -Force -ErrorAction SilentlyContinue
        }
        $midiProbeCount = $null
        if (Test-Path -LiteralPath $midiProbeResult) {
            try { $midiProbeCount = (Get-Content -LiteralPath $midiProbeResult -Raw | ConvertFrom-Json).count } catch { $midiProbeCount = $null }
        }
        $summary.midiEnumeration['guestProbeExited'] = $midiProbeExited
        $summary.midiEnumeration['guestProbeInputDeviceCount'] = $midiProbeCount
        Save-Summary
        if (-not $midiProbeExited -or $null -eq $midiProbeCount) {
            $summary.overall = 'EnvironmentBlocked-MidiEnumerationUnavailable'
            $scriptExitCode = 4
            throw 'midiInGetNumDevs still blocks after removing the Drivers32 midi1 value. No test was started.'
        }
    }

    # Suppress Windows loader/crash modal dialogs in this guest process tree.
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
namespace SandboxTrial {
    public static class ErrorMode {
        [DllImport("kernel32.dll")]
        public static extern uint SetErrorMode(uint mode);
    }
}
'@
    [void][SandboxTrial.ErrorMode]::SetErrorMode(0x8003)

    [ordered]@{
        timestampUtc = (Get-Date).ToUniversalTime().ToString('o')
        os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber
        computer = $computer | Select-Object Manufacturer, Model
        video = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, VideoProcessor)
        powershell = $PSVersionTable.PSVersion.ToString()
        platform = $env:QT_QPA_PLATFORM
        pluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
        missingGuestSystemDlls = @($summary['missingGuestSystemDlls'])
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runRoot 'environment.json') -Encoding UTF8

    if ($plan.windowObserver -eq $true) {
        # Passive observer for real native windows: titles, sizes, and screenshots.
        $watcherScript = Join-Path $inputRoot 'Watch-Windows.ps1'
        if (-not (Test-Path -LiteralPath $watcherScript)) { throw 'The window observer script is missing from the payload.' }
        $watchDirectory = Join-Path $runRoot 'windows'
        New-Item -ItemType Directory -Path $watchDirectory | Out-Null
        $watchStopFile = Join-Path $workRoot 'window-watch.stop'
        Remove-Item -LiteralPath $watchStopFile -ErrorAction SilentlyContinue
        $watcher = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
            -ArgumentList @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', ('"' + $watcherScript + '"'),
                '-OutputDirectory', ('"' + $watchDirectory + '"'),
                '-StopFile', ('"' + $watchStopFile + '"')) `
            -PassThru -WindowStyle Hidden
    }

    $summary.overall = 'Running'
    Save-Summary
    foreach ($case in $runCases) {
        $caseId = [string]$case.id
        $selector = [string]$case.selector
        $timeoutSeconds = [int]$case.timeoutSeconds
        if ($timeoutSeconds -le 0) { $timeoutSeconds = 180 }
        $xmlPath = Join-Path $runRoot ($caseId + '.junit.xml')
        $stdout = Join-Path $runRoot ($caseId + '.stdout.txt')
        $stderr = Join-Path $runRoot ($caseId + '.stderr.txt')
        $record = [ordered]@{
            id = $caseId
            selector = $selector
            startedUtc = (Get-Date).ToUniversalTime().ToString('o')
            timeoutSeconds = $timeoutSeconds
            timedOut = $false
            status = 'Starting'
            exitCode = $null
            exitCodeHex = $null
        }
        Write-Host "Starting $caseId ($selector)"
        $clock = [Diagnostics.Stopwatch]::StartNew()
        # cmd redirects directly to files, so no asynchronous .NET pipe-drain
        # wait can outlive our watchdog. All command inputs come from plan.json,
        # which the host wrote and the manifest covered.
        if ([string]::IsNullOrEmpty($selector)) {
            $commandLine = '""{0}" -v1 -o "{1},junitxml" -o -,txt 1>"{2}" 2>"{3}""' -f $exe, $xmlPath, $stdout, $stderr
        } else {
            $commandLine = '""{0}" "{1}" -v1 -o "{2},junitxml" -o -,txt 1>"{3}" 2>"{4}""' -f $exe, $selector, $xmlPath, $stdout, $stderr
        }
        $activeProcess = Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" `
            -ArgumentList ('/d /s /c ' + $commandLine) -WorkingDirectory $exeDirectory -PassThru -NoNewWindow
        $null = $activeProcess.Handle
        $record['processId'] = $activeProcess.Id
        if (-not $activeProcess.WaitForExit($timeoutSeconds * 1000)) {
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

    # Stop the passive observer before deciding the result. A missing window record
    # means the GUI evidence this workflow exists to collect is absent, so a run that
    # requires it must not report success.
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
    if ($requireWindowEvidence) {
        if ($null -eq $windowSummary) { $windowEvidenceOk = $false }
        elseif ([int]$windowSummary.testWindowSamples -le 0) { $windowEvidenceOk = $false }
    }
    $summary['windowEvidenceRequired'] = $requireWindowEvidence
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
        $summary.overall = 'AllSelectedCasesPassed-IsolationStillRequiresObservation'
        $scriptExitCode = 0
    }
} catch {
    if ($summary.overall -eq 'Preparing' -or $summary.overall -eq 'Running') {
        $summary.overall = 'PreparationOrRunnerFailed'
    }
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
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
    "Run finished. Overall: $($summary.overall). See summary.json and per-case logs. Close Sandbox before the next run for a clean guest." |
        Set-Content -LiteralPath (Join-Path $runRoot 'DONE.txt') -Encoding UTF8
}
exit $scriptExitCode
