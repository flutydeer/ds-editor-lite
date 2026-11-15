#Requires -Version 5.1
<#
.SYNOPSIS
    Test drive policy and guest config placement without starting a guest or touching its paths.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$scripts = Join-Path $PSScriptRoot '..\scripts'

function Read-TestAst {
    param([string]$Name)
    $tokens = $null; $errors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $scripts $Name), [ref]$tokens, [ref]$errors)
    if ($errors.Count -gt 0) { throw "Cannot parse $Name`: $errors" }
    return $ast
}
function Get-TestAssignment {
    param($Ast, [string]$Name)
    $matches = @($Ast.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.AssignmentStatementAst] -and
            $node.Left -is [System.Management.Automation.Language.VariableExpressionAst] -and
            $node.Left.VariablePath.UserPath -eq $Name
    }, $true))
    if ($matches.Count -ne 1) { throw "Expected exactly one assignment to $Name." }
    return $matches[0].Extent.Text
}

# Execute only these bounded production policy statements, never a complete entry point.
$payloadAst = Read-TestAst 'New-SandboxGuiPayload.ps1'
$driveGuards = @($payloadAst.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.IfStatementAst] -and
        $node.Clauses[0].Item1.Extent.Text -match '\$binSourceRoot\b'
}, $true))
if ($driveGuards.Count -ne 1) { throw 'Expected exactly one payload drive guard.' }
$payloadPolicy = [scriptblock]::Create((Get-TestAssignment $payloadAst 'binSourceRoot') + "`n" + $driveGuards[0].Extent.Text)
$hostAst = Read-TestAst 'Invoke-HostGuiTrial.ps1'
$hostPolicy = [scriptblock]::Create((Get-TestAssignment $hostAst 'binSourceRoot') + "`n" +
    (Get-TestAssignment $hostAst 'sandboxDriveSupported') + "`n" + (Get-TestAssignment $hostAst 'sandboxReady') + "`n" + '$sandboxReady')
$guestAst = Read-TestAst 'Run-Guest.ps1'
$configFunctions = @($guestAst.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Initialize-GuestConfigPath'
}, $true))
if ($configFunctions.Count -ne 1) { throw 'Expected exactly one guest config function.' }
# All side-effect commands in the extracted function must use recording mocks below.
$allowedCommands = @('Test-Path', 'New-Item', 'Mount-GuestConfigDrive', 'Copy-Item', 'Join-Path', 'Out-Null')
foreach ($command in $configFunctions[0].FindAll({ param($node) $node -is [System.Management.Automation.Language.CommandAst] }, $true)) {
    if ($allowedCommands -notcontains $command.GetCommandName()) { throw 'Unexpected command in guest config helper.' }
}
. ([scriptblock]::Create($configFunctions[0].Extent.Text))

$record = [pscustomobject]@{ Directories = @(); Copies = @(); Mounts = @(); DriveExists = $false; MountFails = $false; CopyFails = $false }
function New-Item {
    param([string]$ItemType, [string]$Path, [switch]$Force)
    if ($ItemType -ne 'Directory') { throw 'Only directory creation is expected.' }
    $record.Directories += $Path
}
function Copy-Item {
    param([string]$LiteralPath, [string]$Destination, [switch]$Recurse)
    if (-not $Recurse) { throw 'Config directory must be copied recursively.' }
    $record.Copies += [pscustomobject]@{ Source = $LiteralPath; Destination = $Destination }
    if ($record.CopyFails) { throw 'Injected config copy failure.' }
}
function Test-Path {
    param([string]$LiteralPath)
    if ($LiteralPath -notmatch '^[A-Za-z]:\\$') { throw "Unexpected existence probe: $LiteralPath" }
    return $record.DriveExists
}
function Mount-GuestConfigDrive {
    param([string]$DriveSpec, [string]$DriveRoot)
    $record.Mounts += [pscustomobject]@{ DriveSpec = $DriveSpec; DriveRoot = $DriveRoot }
    if ($record.MountFails) { throw 'Injected subst failure.' }
}
function Assert-Equal {
    param($Actual, $Expected, [string]$Message)
    if ($Actual -cne $Expected) { throw "$Message`: expected '$Expected', got '$Actual'." }
}
function Assert-Rejected {
    param([scriptblock]$Action, [string]$ExpectedMessage)
    $caught = $null
    try { & $Action } catch { $caught = $_ }
    if ($null -eq $caught -or $caught.ToString() -notmatch $ExpectedMessage) { throw "Expected rejection matching $ExpectedMessage." }
}
$failures = @(); $passed = 0
function Invoke-TestCase {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action; $script:passed++; Write-Host "PASS $Name" }
    catch { $script:failures += "$Name`: $_" }
}
foreach ($drive in 'C', 'c', 'D') {
    Invoke-TestCase "Payload accepts $drive drive" {
        $BinSource = $drive + ':\projects\editor with spaces\build\out\bin'
        & $payloadPolicy
    }
    Invoke-TestCase "Host refuses fallback for sandbox-ready $drive drive" {
        $BinSource = $drive + ':\projects\editor with spaces\build\out\bin'
        $sandboxFeature = [pscustomobject]@{ InstallState = 1 }
        $computer = [pscustomobject]@{ HypervisorPresent = $true }
        $sandboxCliReady = $true
        Assert-Equal (& $hostPolicy) $true 'Sandbox readiness'
        $sandboxCliReady = $false
        Assert-Equal (& $hostPolicy) $false 'Missing CLI still permits fallback'
    }
    Invoke-TestCase "Guest config placement on $drive drive" {
        $record.Directories = @(); $record.Copies = @(); $record.Mounts = @()
        $record.DriveExists = $true; $record.MountFails = $false
        if ($drive -eq 'D') { $record.DriveExists = $false }
        $embedded = $drive + ':\projects\editor with spaces\build\out\bin'
        $source = 'C:\SandboxTrialInput\bin\configs'
        Initialize-GuestConfigPath -EmbeddedPath $embedded -ConfigSource $source -WorkRoot 'C:\SandboxTrialWork'
        Assert-Equal $record.Copies.Count 1 'Config copy count'
        Assert-Equal $record.Copies[0].Source $source 'Copied source'
        Assert-Equal $record.Copies[0].Destination ($drive.ToUpperInvariant() + ':\projects\editor with spaces\build\out\bin') 'Config parent destination'
        if ($drive -eq 'D') {
            Assert-Equal $record.Mounts.Count 1 'Non-system drive mount count'
            Assert-Equal $record.Mounts[0].DriveSpec 'D:' 'Mounted drive'
            Assert-Equal $record.Mounts[0].DriveRoot 'C:\SandboxTrialWork\driveD' 'Private mount root'
            Assert-Equal $record.Directories.Count 2 'Mount root and config parent created'
        } else {
            Assert-Equal $record.Mounts.Count 0 'C drive must not use subst'
            Assert-Equal $record.Directories.Count 1 'Only config parent created'
        }
        Assert-Equal $record.Directories[-1] $record.Copies[0].Destination 'Config parent created before copy'
    }
}
Invoke-TestCase 'Payload rejects UNC source' {
    $BinSource = '\\server\share\bin'
    Assert-Rejected { & $payloadPolicy } 'drive letter'
}
Invoke-TestCase 'Host still permits fallback for UNC source' {
    $BinSource = '\\server\share\bin'
    $sandboxFeature = [pscustomobject]@{ InstallState = 1 }
    $computer = [pscustomobject]@{ HypervisorPresent = $true }
    $sandboxCliReady = $true
    Assert-Equal (& $hostPolicy) $false 'Unsupported source drive'
}
foreach ($invalidPath in '\\server\share\bin', 'relative\bin') {
    Invoke-TestCase "Guest rejects $invalidPath" {
        Assert-Rejected { Initialize-GuestConfigPath -EmbeddedPath $invalidPath } 'unusable embeddedConfigPath'
    }
}
Invoke-TestCase 'Guest refuses occupied non-system drive' {
    $record.DriveExists = $true; $record.Directories = @(); $record.Copies = @(); $record.Mounts = @()
    Assert-Rejected { Initialize-GuestConfigPath -EmbeddedPath 'D:\project\bin' } 'already has drive D:'
    Assert-Equal $record.Directories.Count 0 'No directory creation on occupied drive'
    Assert-Equal $record.Copies.Count 0 'No config copy on occupied drive'
    Assert-Equal $record.Mounts.Count 0 'No mount on occupied drive'
}
Invoke-TestCase 'Guest does not copy configs after subst failure' {
    $record.DriveExists = $false; $record.MountFails = $true; $record.Copies = @()
    Assert-Rejected { Initialize-GuestConfigPath -EmbeddedPath 'D:\project\bin' -WorkRoot 'C:\SandboxTrialWork' } 'Injected subst failure'
    Assert-Equal $record.Copies.Count 0 'No copy after failed drive mount'
}
Invoke-TestCase 'Guest propagates config copy failure on C drive' {
    $record.CopyFails = $true; $record.Mounts = @()
    Assert-Rejected { Initialize-GuestConfigPath -EmbeddedPath 'C:\project\bin' -ConfigSource 'C:\SandboxTrialInput\bin\configs' } 'Injected config copy failure'
    Assert-Equal $record.Mounts.Count 0 'No C drive mount even on copy failure'
}
if ($failures.Count -gt 0) { throw ($failures -join "`n") }
Write-Host "Sandbox config path regression: $passed passed, 0 failed. No guest, drive mapping or GUI was started."
