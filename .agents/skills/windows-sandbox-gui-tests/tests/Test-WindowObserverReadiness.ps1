#Requires -Version 5.1
<#
.SYNOPSIS
    Check observer startup coordination with console children and real cross-process events.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$helper = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\scripts\WindowObserverLifecycle.ps1'))
. $helper
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$testRoot = Join-Path $repo ('build\SandboxGuiTrial\observer-readiness-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$fakeObserver = Join-Path $testRoot 'fake observer.ps1'
$engine = (Get-Process -Id $PID).Path
@'
param([string]$Helper, [string]$Mode, [string]$EnteredName, [string]$ReleaseName,
    [string]$FinishName, [string]$InitializedName, [string]$ReadyEventName)
$ErrorActionPreference = 'Stop'
. $Helper
$entered = [Threading.EventWaitHandle]::OpenExisting($EnteredName)
$release = [Threading.EventWaitHandle]::OpenExisting($ReleaseName)
$finish = [Threading.EventWaitHandle]::OpenExisting($FinishName)
$initialized = [Threading.EventWaitHandle]::OpenExisting($InitializedName)
try {
    $null = $entered.Set()
    if ($Mode -eq 'ExitBeforeReady') { exit 0 }
    if ($Mode -eq 'NeverReady') { $null = $finish.WaitOne(15000); exit 0 }
    if ($Mode -eq 'Gated' -and -not $release.WaitOne(15000)) { throw 'Initialization was never released.' }
    $null = $initialized.Set()
    Send-WindowObserverReady -ReadyEventName $ReadyEventName
    if (-not $finish.WaitOne(15000)) { throw 'The fake observer was not stopped.' }
} finally {
    $entered.Dispose(); $release.Dispose(); $finish.Dispose(); $initialized.Dispose()
}
'@ | Set-Content -LiteralPath $fakeObserver -Encoding UTF8

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function New-Scenario {
    param([string]$Mode, [int]$TimeoutMs = 10000, [switch]$LaunchFailure)
    $scenario = [pscustomobject]@{
        Events = @{}; Names = @{}; Worker = $null; Invocation = $null; Completed = $false
        State = [hashtable]::Synchronized(@{ Process = $null; Error = $null; WorkSawInitialization = $false })
    }
    foreach ($name in 'Entered', 'Release', 'Finish', 'Initialized', 'Work') {
        $eventName = 'Local\DsEditorLite.ObserverTest.' + [guid]::NewGuid().ToString('N')
        $scenario.Names[$name] = $eventName
        $scenario.Events[$name] = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset, $eventName)
    }
    $childEngine = $engine
    if ($LaunchFailure) { $childEngine = Join-Path $testRoot 'missing-observer-engine.exe' }
    $scenario.Worker = [PowerShell]::Create()
    $null = $scenario.Worker.AddScript({
        param($Helper, $Engine, $ChildScript, $Mode, $TimeoutMs, $Names, $Events, $State)
        $ErrorActionPreference = 'Stop'
        . $Helper
        $watcher = $null
        try {
            Start-WindowObserver -FilePath $Engine -ArgumentList @(
                '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $ChildScript + '"'),
                '-Helper', ('"' + $Helper + '"'), '-Mode', $Mode,
                '-EnteredName', $Names.Entered, '-ReleaseName', $Names.Release,
                '-FinishName', $Names.Finish, '-InitializedName', $Names.Initialized
            ) -Process ([ref]$watcher) -ReadyTimeoutMs $TimeoutMs
            $State.WorkSawInitialization = $Events.Initialized.WaitOne(0)
            $null = $Events.Work.Set()
        } catch {
            $State.Error = $_
        } finally {
            $State.Process = $watcher
        }
    }.ToString()).AddArgument($helper).AddArgument($childEngine).AddArgument($fakeObserver).
        AddArgument($Mode).AddArgument($TimeoutMs).AddArgument($scenario.Names).
        AddArgument($scenario.Events).AddArgument($scenario.State)
    $scenario.Invocation = $scenario.Worker.BeginInvoke()
    return $scenario
}
function Complete-Scenario {
    param($Scenario)
    Assert-True ($Scenario.Invocation.AsyncWaitHandle.WaitOne(15000)) 'The parent startup operation exceeded its bound.'
    $null = $Scenario.Worker.EndInvoke($Scenario.Invocation)
    $Scenario.Completed = $true
}
function Close-Scenario {
    param($Scenario)
    if ($null -eq $Scenario) { return }
    try {
        $null = $Scenario.Events.Release.Set()
        $null = $Scenario.Events.Finish.Set()
        if (-not $Scenario.Completed) { Complete-Scenario $Scenario }
        $child = $Scenario.State.Process
        if ($null -ne $child) {
            try {
                if (-not $child.WaitForExit(5000)) { $child.Kill(); $null = $child.WaitForExit(5000) }
            } finally { $child.Dispose() }
        }
    } finally {
        $Scenario.Invocation.AsyncWaitHandle.Dispose()
        $Scenario.Worker.Dispose()
        foreach ($event in $Scenario.Events.Values) { $event.Dispose() }
    }
}
$passed = 0; $failures = @()
function Invoke-ReadinessCase {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action; $script:passed++; Write-Host "PASS $Name" }
    catch { $script:failures += "$Name`: $_" }
}
Invoke-ReadinessCase 'Work cannot begin while the observer is still initializing' {
    $scenario = New-Scenario -Mode 'Gated'
    try {
        Assert-True ($scenario.Events.Entered.WaitOne(10000)) 'The console observer did not reach initialization.'
        Assert-True (-not $scenario.Events.Work.WaitOne(200)) 'Work began before observer initialization was released.'
        $null = $scenario.Events.Release.Set()
        Complete-Scenario $scenario
        Assert-True ($null -eq $scenario.State.Error) "Startup failed: $($scenario.State.Error)"
        Assert-True ($scenario.Events.Work.WaitOne(0)) 'Work did not begin after readiness.'
        Assert-True $scenario.State.WorkSawInitialization 'Work started before the child completed initialization.'
        Assert-True ($null -ne $scenario.State.Process -and -not $scenario.State.Process.HasExited) 'The ready child process was not retained.'
    } finally { Close-Scenario $scenario }
}
Invoke-ReadinessCase 'A real child readiness signal permits work' {
    $scenario = New-Scenario -Mode 'Ready'
    try {
        Complete-Scenario $scenario
        Assert-True ($null -eq $scenario.State.Error) "Startup failed: $($scenario.State.Error)"
        Assert-True ($scenario.Events.Work.WaitOne(0) -and $scenario.State.WorkSawInitialization) 'The parent did not observe the real child signal.'
    } finally { Close-Scenario $scenario }
}
Invoke-ReadinessCase 'Missing readiness times out without starting work and retains the child' {
    $scenario = New-Scenario -Mode 'NeverReady' -TimeoutMs 300
    try {
        Complete-Scenario $scenario
        Assert-True ($null -ne $scenario.State.Error -and $scenario.State.Error.Exception -is [TimeoutException]) 'Startup did not report a readiness timeout.'
        Assert-True (-not $scenario.Events.Work.WaitOne(0)) 'Work began despite a readiness timeout.'
        Assert-True ($null -ne $scenario.State.Process -and -not $scenario.State.Process.HasExited) 'The unready process was lost instead of being left for cleanup.'
    } finally { Close-Scenario $scenario }
}
Invoke-ReadinessCase 'A child exit without readiness fails closed and preserves its process object' {
    $scenario = New-Scenario -Mode 'ExitBeforeReady' -TimeoutMs 3000
    try {
        Assert-True ($scenario.Events.Entered.WaitOne(10000)) 'The console observer did not enter before exiting.'
        Complete-Scenario $scenario
        Assert-True ($null -ne $scenario.State.Process) 'The exited process object was not retained.'
        Assert-True ($scenario.State.Process.WaitForExit(5000)) 'The console observer did not finish its requested exit.'
        Assert-True ($null -ne $scenario.State.Error) 'An exited child was accepted without readiness.'
        Assert-True (-not $scenario.Events.Work.WaitOne(0)) 'Work began after an observer exited before readiness.'
    } finally { Close-Scenario $scenario }
}
Invoke-ReadinessCase 'A launch failure never starts work or invents a child process' {
    $scenario = New-Scenario -Mode 'Ready' -LaunchFailure
    try {
        Complete-Scenario $scenario
        Assert-True ($null -ne $scenario.State.Error) 'The missing engine did not fail startup.'
        Assert-True (-not $scenario.Events.Work.WaitOne(0)) 'Work began after process creation failed.'
        Assert-True ($null -eq $scenario.State.Process) 'Launch failure unexpectedly supplied a process.'
    } finally { Close-Scenario $scenario }
}
Invoke-ReadinessCase 'A child cannot report readiness without a parent-owned event' {
    $caught = $null
    try { Send-WindowObserverReady -ReadyEventName ('Local\DsEditorLite.Missing.' + [guid]::NewGuid().ToString('N')) }
    catch { $caught = $_ }
    Assert-True ($null -ne $caught) 'The child silently created a readiness event with no waiting parent.'
}
Write-Host "Evidence: $testRoot"
if ($failures.Count -gt 0) { throw ($failures -join "`n") }
Write-Host "Observer readiness regression: $passed passed, 0 failed. Only console fixtures were launched."
