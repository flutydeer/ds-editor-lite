#Requires -Version 5.1
<#
.SYNOPSIS
    Exercise the public config-path contract with isolated drives and real file copies.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot '..\scripts\SandboxConfigPaths.ps1')

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('ds-editor-config-paths-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$results = [pscustomobject]@{ Passed = 0; Failures = @() }

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function Assert-FileText {
    param([string]$Path, [string]$Expected)
    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "Missing expected config file: $Path"
    Assert-True ([IO.File]::ReadAllText($Path) -ceq $Expected) "Unexpected file contents: $Path"
}
function Assert-Rejected {
    param([scriptblock]$Action, [string]$MessagePattern = '.')
    $caught = $null
    try { & $Action } catch { $caught = $_ }
    Assert-True ($null -ne $caught) 'The operation unexpectedly succeeded.'
    Assert-True ($caught.Exception.Message -match $MessagePattern) "Unexpected failure: $caught"
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
function New-TestFixture {
    $root = Join-Path $testRoot ([guid]::NewGuid().ToString('N'))
    $source = Join-Path $root 'input\configs'
    $work = Join-Path $root 'work'
    $systemDrive = Join-Path $root 'system drive'
    foreach ($directory in $source, (Join-Path $source 'nested'), $work, $systemDrive) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    [IO.File]::WriteAllText((Join-Path $source 'marker.json'), '{"setting":"from payload"}')
    [IO.File]::WriteAllText((Join-Path $source 'nested\child.ini'), 'nested=true')
    $state = [pscustomobject]@{
        Roots = @{ 'C:' = $systemDrive; 'D:' = (Join-Path $root 'unmounted drive') }
        MountFails = $false
    }
    $resolve = {
        param($DriveSpec)
        return $state.Roots[$DriveSpec]
    }.GetNewClosure()
    # Only drive access is substituted. Directory creation and copying remain real.
    $mount = {
        param($DriveSpec, $DriveRoot)
        if ($DriveSpec -eq 'C:') { throw 'The system drive must never be remapped.' }
        if ($state.MountFails) { throw 'Injected drive mapping failure.' }
        $state.Roots[$DriveSpec] = $DriveRoot
    }.GetNewClosure()
    return [pscustomobject]@{
        Root = $root
        Source = $source
        Work = $work
        SystemDrive = $systemDrive
        State = $state
        Resolve = $resolve
        Mount = $mount
    }
}
function Invoke-FixtureInitialization {
    param($Fixture, [string]$EmbeddedPath)
    Initialize-SandboxConfigPath -EmbeddedPath $EmbeddedPath -ConfigSource $Fixture.Source `
        -WorkRoot $Fixture.Work -ResolveDriveRoot $Fixture.Resolve -MountDrive $Fixture.Mount
}
function Assert-ConfigLayout {
    param([string]$Parent)
    Assert-FileText (Join-Path $Parent 'configs\marker.json') '{"setting":"from payload"}'
    Assert-FileText (Join-Path $Parent 'configs\nested\child.ini') 'nested=true'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Parent 'marker.json'))) 'Configs must not be flattened into the bin directory.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Parent 'configs\configs'))) 'Configs must not acquire an extra container level.'
}

foreach ($drive in 'C', 'c', 'D') {
    Invoke-TestCase "Local $drive drive supports an embedded path containing spaces" {
        $fixture = New-TestFixture
        $path = $drive + ':\projects\editor with spaces\build\out\bin'
        Assert-True (Test-SandboxConfigPathSupported -Path $path) 'Local drive paths must remain Sandbox eligible.'
        Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath $path
        $accessibleDrive = $fixture.State.Roots[($drive.ToUpperInvariant() + ':')]
        Assert-ConfigLayout (Join-Path $accessibleDrive 'projects\editor with spaces\build\out\bin')
        Assert-FileText (Join-Path $fixture.Source 'marker.json') '{"setting":"from payload"}'
    }
}

Invoke-TestCase 'Subprocess fixtures land next to the recreated configs' {
    $fixture = New-TestFixture
    $staged = Join-Path $fixture.Root 'staged bin'
    New-Item -ItemType Directory -Path $staged -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $staged 'TestApplicationWorkflows.exe'), 'fixture binary')
    Initialize-SandboxConfigPath -EmbeddedPath 'C:\projects\editor\build\out\bin' -ConfigSource $fixture.Source `
        -WorkRoot $fixture.Work -ResolveDriveRoot $fixture.Resolve -MountDrive $fixture.Mount `
        -FixtureSource $staged -FixtureName 'TestApplicationWorkflows.exe'
    $parent = Join-Path $fixture.SystemDrive 'projects\editor\build\out\bin'
    Assert-ConfigLayout $parent
    # A suite that spawns this binary holds its compiled path, not the payload copy.
    Assert-FileText (Join-Path $parent 'TestApplicationWorkflows.exe') 'fixture binary'
}
Invoke-TestCase 'A fixture missing from the staged payload is rejected' {
    $fixture = New-TestFixture
    $staged = Join-Path $fixture.Root 'staged bin'
    New-Item -ItemType Directory -Path $staged -Force | Out-Null
    Assert-Rejected {
        Initialize-SandboxConfigPath -EmbeddedPath 'C:\projects\editor\build\out\bin' -ConfigSource $fixture.Source `
            -WorkRoot $fixture.Work -ResolveDriveRoot $fixture.Resolve -MountDrive $fixture.Mount `
            -FixtureSource $staged -FixtureName 'MissingFixture.exe'
    } 'Missing staged fixture'
}

foreach ($path in '\\server\share\project\bin', 'relative\project\bin', 'C:project\bin', '\project\bin') {
    Invoke-TestCase "Non-local or relative path is rejected: $path" {
        $fixture = New-TestFixture
        Assert-True (-not (Test-SandboxConfigPathSupported -Path $path)) 'Unsupported paths must not select the Sandbox route.'
        Assert-Rejected { Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath $path }
        Assert-True (@(Get-ChildItem -LiteralPath $fixture.SystemDrive -Force).Count -eq 0) 'A rejected path changed the system-drive fixture.'
        Assert-True (@(Get-ChildItem -LiteralPath $fixture.Work -Force).Count -eq 0) 'A rejected path changed the work fixture.'
    }
}

Invoke-TestCase 'Existing bin parent keeps unrelated contents while receiving configs' {
    $fixture = New-TestFixture
    $parent = Join-Path $fixture.SystemDrive 'project\bin'
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $parent 'unrelated.txt'), 'keep this file')
    Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath 'C:\project\bin'
    Assert-ConfigLayout $parent
    Assert-FileText (Join-Path $parent 'unrelated.txt') 'keep this file'
}
Invoke-TestCase 'Existing configs are rejected without merging or overwriting' {
    $fixture = New-TestFixture
    $parent = Join-Path $fixture.SystemDrive 'project\bin'
    New-Item -ItemType Directory -Path (Join-Path $parent 'configs') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $parent 'configs\marker.json'), 'existing config')
    Assert-Rejected { Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath 'C:\project\bin' }
    Assert-FileText (Join-Path $parent 'configs\marker.json') 'existing config'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $parent 'configs\nested'))) 'A refused destination must not receive partial config contents.'
}
Invoke-TestCase 'An occupied non-system drive is preserved without preparing files' {
    $fixture = New-TestFixture
    $occupied = $fixture.State.Roots['D:']
    New-Item -ItemType Directory -Path $occupied | Out-Null
    [IO.File]::WriteAllText((Join-Path $occupied 'keep.txt'), 'existing drive')
    Assert-Rejected { Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath 'D:\project\bin' }
    Assert-FileText (Join-Path $occupied 'keep.txt') 'existing drive'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $occupied 'project'))) 'An occupied drive received config directories.'
    Assert-True (@(Get-ChildItem -LiteralPath $fixture.Work -Force).Count -eq 0) 'An occupied drive caused work-directory changes.'
}
Invoke-TestCase 'A mapping error propagates before configs are copied' {
    $fixture = New-TestFixture
    $fixture.State.MountFails = $true
    Assert-Rejected { Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath 'D:\project\bin' } 'Injected drive mapping failure'
    Assert-True (@(Get-ChildItem -LiteralPath $fixture.Work -Recurse -Filter 'marker.json').Count -eq 0) 'Config files were copied after a failed mount.'
    Assert-FileText (Join-Path $fixture.Source 'marker.json') '{"setting":"from payload"}'
}
Invoke-TestCase 'A real copy error propagates instead of reporting successful preparation' {
    $fixture = New-TestFixture
    $marker = Join-Path $fixture.Source 'marker.json'
    $locked = [IO.File]::Open($marker, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        Assert-Rejected { Invoke-FixtureInitialization -Fixture $fixture -EmbeddedPath 'C:\project\bin' }
    } finally {
        $locked.Dispose()
    }
    Assert-FileText $marker '{"setting":"from payload"}'
}

Write-Host "Fixture evidence: $testRoot"
if ($results.Failures.Count -gt 0) { throw ($results.Failures -join "`n") }
Write-Host "Sandbox config path regression: $($results.Passed) passed, 0 failed. No Sandbox, GUI, real drive mapping or guest entry point was started."
