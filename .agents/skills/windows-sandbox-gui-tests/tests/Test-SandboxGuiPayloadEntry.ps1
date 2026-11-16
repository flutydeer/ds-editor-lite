#Requires -Version 5.1
<#
.SYNOPSIS
    Check that a payload built for another test binary is described and staged consistently.
.DESCRIPTION
    Builds a real payload from the debug test output with -EntryExecutable and asserts that
    every generated field and the staged build tree follow that choice. The payload is
    discarded afterwards, and no Sandbox or guest entry point is started.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$payloadScript = Join-Path $PSScriptRoot '..\scripts\New-SandboxGuiPayload.ps1'
$binSource = Join-Path $repo 'build\debug\out\bin'
$results = [pscustomobject]@{ Passed = 0; Failures = @() }

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function Invoke-TestCase {
    param([string]$Name, [scriptblock]$Action)
    try {
        & $Action
        $results.Passed++
        Write-Host "PASS $Name"
    } catch {
        $results.Failures += "$Name`: $_"
        Write-Host "FAIL $Name`: $_"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $binSource 'TestApplicationGui.exe'))) {
    Write-Host "SKIP: build the debug test targets first; $binSource has no TestApplicationGui.exe."
    return
}

$outputRoot = Join-Path $repo ('build\SandboxGuiTrial\payload-entry-' + [guid]::NewGuid().ToString('N'))
Invoke-TestCase 'Every generated field and the staged tree follow the selected executable' {
    # A payload only has to name its entry point correctly, so one selector is enough.
    & $payloadScript -Preset debug -EntryExecutable 'TestApplicationGui.exe' `
        -FixtureExecutable 'TestApplicationWorkflows.exe' `
        -Selectors 'rhiPianoNavigationInputsReachTheActiveViewport' `
        -OutputRoot $outputRoot -UseHardLinks -Force | Out-Null
    $planPath = Join-Path $outputRoot 'payload\plan.json'
    $manifestPath = Join-Path $outputRoot 'payload\manifest.json'
    Assert-True (Test-Path -LiteralPath $planPath) 'The payload has no plan.json.'
    Assert-True (Test-Path -LiteralPath $manifestPath) 'The payload has no manifest.json.'
    $plan = Get-Content -LiteralPath $planPath -Raw | ConvertFrom-Json
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Assert-True ([string]$plan.executable -eq 'bin/TestApplicationGui.exe') `
        "plan.json names '$($plan.executable)' instead of the selected executable."
    Assert-True ([string]$manifest.entryPoint -eq 'bin/TestApplicationGui.exe') `
        "manifest.json names '$($manifest.entryPoint)' instead of the selected executable."
    $staged = Join-Path $outputRoot 'payload\bin'
    Assert-True (Test-Path -LiteralPath (Join-Path $staged 'TestApplicationGui.exe')) `
        'The selected executable was not staged.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $staged 'TestNativeDesktop.exe'))) `
        'An unrelated test executable was staged.'
    # TestApplicationGui runs this one as an external converter, so dropping it makes
    # the unfiltered suite unrunnable inside the guest.
    Assert-True ((@($plan.fixtureExecutables) -contains 'TestApplicationWorkflows.exe')) `
        "plan.json does not announce the staged fixture: $($plan.fixtureExecutables -join ', ')."
    Assert-True (Test-Path -LiteralPath (Join-Path $staged 'TestApplicationWorkflows.exe')) `
        'The subprocess fixture was not staged.'
}
Remove-Item -LiteralPath $outputRoot -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Payload entry point regression: $($results.Passed) passed, $($results.Failures.Count) failed. No Sandbox or GUI process was started."
if ($results.Failures.Count -gt 0) { throw ($results.Failures -join "`n") }
