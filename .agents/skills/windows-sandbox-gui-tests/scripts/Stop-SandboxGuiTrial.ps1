#Requires -Version 5.1
<#
.SYNOPSIS
    Close the Windows Sandbox used by a GUI trial and confirm it is gone.

.DESCRIPTION
    Sends a shutdown to the guest through the Sandbox CLI running as System, waits for
    the client processes to exit, and prints the remaining environment list. Windows
    Sandbox is disposable, so a run is only clean again after the whole guest is gone.

.PARAMETER TrialRoot
    Trial directory that holds current-sandbox.json.

.PARAMETER Id
    Explicit Sandbox environment id. Defaults to the id recorded at launch.
#>
[CmdletBinding()]
param(
    [string]$TrialRoot = 'build\SandboxGuiTrial',
    [string]$Id
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
if (-not [IO.Path]::IsPathRooted($TrialRoot)) { $TrialRoot = Join-Path $repo $TrialRoot }
$TrialRoot = [IO.Path]::GetFullPath($TrialRoot).TrimEnd('\')
$wsb = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\wsb.exe'
$recordPath = Join-Path $TrialRoot 'current-sandbox.json'
$activeNames = @('WindowsSandbox', 'WindowsSandboxClient', 'WindowsSandboxRemoteSession', 'vmmemWindowsSandbox')

if (-not (Test-Path -LiteralPath $wsb)) { throw "Missing the Sandbox CLI at $wsb." }

if (-not $Id -and (Test-Path -LiteralPath $recordPath)) {
    $record = Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
    if ($record.PSObject.Properties.Name -contains 'sandboxId' -and $record.sandboxId) { $Id = [string]$record.sandboxId }
}
if (-not $Id) {
    $raw = & $wsb list --raw 2>$null | Out-String
    if ($raw -match '\S') {
        try { $listed = $raw | ConvertFrom-Json } catch { $listed = $null }
        if ($null -ne $listed) {
            $environments = @($listed.WindowsSandboxEnvironments)
            if ($environments.Count -eq 1) {
                $Id = [string]$environments[0].Id
                Write-Host "Using the only Sandbox environment: $Id"
            }
        }
    }
}
if (-not $Id) {
    throw 'No Sandbox environment id was supplied or recorded. Pass -Id explicitly.'
}

Write-Host "Shutting down Sandbox $Id"
& $wsb exec --id $Id --run-as System --command 'shutdown.exe /s /t 0 /f' --raw 2>&1 | Out-String

for ($i = 0; $i -lt 45; $i++) {
    $active = @(Get-Process -Name $activeNames -ErrorAction SilentlyContinue)
    if ($active.Count -eq 0) { break }
    Start-Sleep -Seconds 2
}
$active = @(Get-Process -Name $activeNames -ErrorAction SilentlyContinue)
if ($active.Count -gt 0) {
    throw ('Sandbox client processes are still running: ' + (($active | ForEach-Object { "$($_.ProcessName)#$($_.Id)" }) -join ', '))
}

$remaining = & $wsb list --raw 2>$null | Out-String
Write-Host 'Sandbox closed.'
Write-Host $remaining
Remove-Item -LiteralPath $recordPath -Force -ErrorAction SilentlyContinue
