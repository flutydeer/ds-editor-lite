#Requires -Version 5.1
<#
.SYNOPSIS
    Exercise the real host runner with fake processes, without Qt, a build or GUI windows.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$runner = Join-Path $PSScriptRoot '..\scripts\Invoke-HostGuiTrial.ps1'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$testRoot = Join-Path $repo ('build\SandboxGuiTrial\environment-regression-' + [guid]::NewGuid().ToString('N'))
$bin = Join-Path $testRoot 'bin'
$voicebank = Join-Path $testRoot 'voicebank'
foreach ($directory in $bin, (Join-Path $bin 'plugins'), (Join-Path $bin 'configs'), $voicebank) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
# The runner's preflight needs these names, but Start-Process below never executes them.
foreach ($name in 'TestNativeDesktop.exe', 'Qt6Core.dll', 'Qt6Test.dll') {
    Set-Content -LiteralPath (Join-Path $bin $name) -Value ''
}
Set-Content -LiteralPath (Join-Path $voicebank 'desc.json') -Value '{}'

$expectedEnvironment = @{
    QT_QPA_PLATFORM = 'windows'
    QT_PLUGIN_PATH = Join-Path $bin 'plugins'
    QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $bin 'plugins\platforms'
    QT_FORCE_STDERR_LOGGING = '1'
    QT_DEBUG_PLUGINS = '1'
    QTEST_FUNCTION_TIMEOUT = '300000'
    DSEL_TEST_PLUGIN_ROOT = Join-Path $bin 'plugins'
    DSEL_TEST_VOICEBANK_ROOT = $voicebank
    DSEL_TEST_SINGER_ID = 'fixture'
    DSEL_TEST_LANGUAGE = 'cmn'
    DSEL_TEST_LYRIC = 'la'
    DSEL_TEST_GUI_LIFECYCLE = $null
}
$environmentNames = @('PATH') + @($expectedEnvironment.Keys)
$regression = [pscustomobject]@{
    LaunchCount = 0; ChildEnvironmentVerified = $false
    CallerEnvironment = @{}; SummaryLock = $null
}

function Get-TestEnvironment {
    $snapshot = @{}
    foreach ($name in $environmentNames) {
        $item = Get-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        $snapshot[$name] = if ($null -eq $item) { $null } else { $item.Value }
    }
    return $snapshot
}

# Windows can pass empty values even to PowerShell 5.1, whose Env: setter deletes them.
# Seed them through Win32 so the test does not silently skip inherited empty values.
if (-not ('GuiTrialRegression.NativeEnvironment' -as [type])) {
    Add-Type -Namespace GuiTrialRegression -Name NativeEnvironment -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
public static extern bool SetEnvironmentVariable(string name, string value);
'@
}
function Set-TestEnvironment {
    param([System.Collections.IDictionary]$Snapshot)
    foreach ($name in $Snapshot.Keys) {
        if ($null -eq $Snapshot[$name]) {
            Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        } elseif ($Snapshot[$name] -ceq '') {
            if (-not [GuiTrialRegression.NativeEnvironment]::SetEnvironmentVariable($name, '')) {
                throw "Unable to seed empty environment variable: $name"
            }
        } else {
            Set-Item -LiteralPath "Env:$name" -Value $Snapshot[$name]
        }
    }
}

# Force the fallback without inspecting or changing the machine's Sandbox installation.
function Get-CimInstance {
    [CmdletBinding()]
    param([string]$ClassName, [string]$Filter)
    switch ($ClassName) {
        'Win32_ComputerSystem' { [pscustomobject]@{ HypervisorPresent = $false; Manufacturer = 'Fake'; Model = 'Fake' } }
        'Win32_OptionalFeature' { [pscustomobject]@{ InstallState = 0 } }
        'Win32_OperatingSystem' { [pscustomobject]@{ Caption = 'Fake'; Version = '0'; BuildNumber = '0' } }
        'Win32_VideoController' { [pscustomobject]@{ Name = 'Fake'; DriverVersion = '0'; VideoProcessor = 'Fake' } }
        default { throw "Unexpected CIM query: $ClassName" }
    }
}

function Start-Process {
    [CmdletBinding()]
    param([string]$FilePath, [string]$ArgumentList, [string]$WorkingDirectory,
        [switch]$PassThru, [switch]$NoNewWindow)
    $regression.LaunchCount++
    if ($FilePath -ne "$env:SystemRoot\System32\cmd.exe") { throw "Unexpected process: $FilePath" }
    foreach ($name in $expectedEnvironment.Keys) {
        $item = Get-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        $value = if ($null -eq $item) { $null } else { $item.Value }
        if ($value -cne $expectedEnvironment[$name]) { throw "Child environment mismatch: $name" }
    }
    if (($env:PATH -split ';')[0] -ne $bin -or $env:PATH -eq $regression.CallerEnvironment.PATH) {
        throw 'The child did not receive the isolated runtime PATH.'
    }
    $regression.ChildEnvironmentVerified = $true
    if ($scenario -eq 'LaunchFailure') { throw 'Injected process launch failure.' }
    if ($ArgumentList -notmatch '-o "([^"]+),junitxml"') { throw 'Missing JUnit output argument.' }
    Set-Content -LiteralPath $Matches[1] -Value '<testsuite><testcase name="environmentRegression"/></testsuite>'
    if ($scenario -eq 'SummaryWriteFailure') {
        # Make reporting fail after the process completes, including inside finally.
        $summaryPath = Join-Path (Split-Path -Parent $Matches[1]) 'summary.json'
        $regression.SummaryLock = [IO.File]::Open($summaryPath, 'Open', 'ReadWrite', 'None')
    }
    $process = [pscustomobject]@{ Id = $PID; Handle = 0; HasExited = $true; ExitCode = 0 }
    $process | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { param($Timeout) return $true }
    $process | Add-Member -MemberType ScriptMethod -Name Dispose -Value { }
    return $process
}

$originalEnvironment = Get-TestEnvironment
$failures = @()
$passed = 0
try {
    foreach ($state in 'Present', 'Absent', 'Empty') {
        foreach ($scenario in 'Success', 'LaunchFailure', 'SummaryWriteFailure') {
            $seed = @{}
            foreach ($name in $environmentNames) {
                $seed[$name] = switch ($state) {
                    'Present' { "caller-$name" }
                    'Absent' { $null }
                    'Empty' { '' }
                }
            }
            Set-TestEnvironment $seed
            $regression.CallerEnvironment = Get-TestEnvironment
            $regression.LaunchCount = 0
            $regression.ChildEnvironmentVerified = $false
            $regression.SummaryLock = $null
            $caught = $null
            try {
                & $runner -BinSource $bin -VoicebankSource $voicebank -QtRoot $testRoot `
                    -OutputRoot (Join-Path $testRoot "$state-$scenario") `
                    -Selectors 'environmentRegression' -NoWindowObserver -AllowHostGui 6>$null | Out-Null
                $exitCode = $LASTEXITCODE
            } catch {
                $caught = $_
            } finally {
                if ($null -ne $regression.SummaryLock) { $regression.SummaryLock.Dispose() }
            }
            $problems = @()
            if ($regression.LaunchCount -ne 1 -or -not $regression.ChildEnvironmentVerified) {
                $problems += 'Did not reach the fake child with the required environment.'
            }
            if ($scenario -eq 'SummaryWriteFailure') {
                if ($null -eq $caught -or $caught.ToString() -notmatch 'summary.json') {
                    $problems += 'Did not observe the injected summary write failure.'
                }
            } elseif ($null -ne $caught) {
                $problems += $caught.ToString()
            } else {
                $expectedExit = if ($scenario -eq 'Success') { 0 } else { 2 }
                if ($exitCode -ne $expectedExit) { $problems += "Exit code $exitCode, expected $expectedExit." }
            }
            $after = Get-TestEnvironment
            foreach ($name in $environmentNames) {
                if ($after[$name] -cne $regression.CallerEnvironment[$name]) {
                    $problems += "Leaked environment variable: $name"
                }
            }
            Set-TestEnvironment $originalEnvironment
            if ($problems.Count -gt 0) {
                $failures += "$state/$scenario`: $($problems -join ' | ')"
            } else {
                $passed++
                Write-Host "PASS $state/$scenario"
            }
        }
    }
} finally {
    Set-TestEnvironment $originalEnvironment
}
Write-Host "Evidence: $testRoot"
if ($failures.Count -gt 0) { throw ($failures -join "`n") }
Write-Host "Host environment regression: $passed passed, 0 failed."
