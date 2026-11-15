#Requires -Version 5.1
<#
.SYNOPSIS
    Launch a Windows Sandbox run of the DS Editor Lite native GUI test binary.

.DESCRIPTION
    Inspection only unless -Launch is present. Verifies the Sandbox feature and
    hypervisor, refuses to start beside an existing Sandbox client, re-verifies every
    payload hash, writes the .wsb file from the template with absolute paths, and
    checks that the generated configuration keeps the payload read-only while giving
    the guest one writable results folder.

    This script never runs the test binary on the host.

.PARAMETER Launch
    Actually open the Sandbox. Without it the script only reports readiness.

.PARAMETER TrialRoot
    Trial directory prepared by New-SandboxGuiPayload.ps1.

.PARAMETER MemoryMB
    Guest memory. Keep 8192 unless the machine is short on memory.
#>
[CmdletBinding()]
param(
    [switch]$Launch,
    [string]$TrialRoot = 'build\SandboxGuiTrial',
    [int]$MemoryMB = 8192
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
if (-not [IO.Path]::IsPathRooted($TrialRoot)) { $TrialRoot = Join-Path $repo $TrialRoot }
$TrialRoot = [IO.Path]::GetFullPath($TrialRoot).TrimEnd('\')
$payload = Join-Path $TrialRoot 'payload'
$results = Join-Path $TrialRoot 'results'
$wsbPath = Join-Path $TrialRoot 'SandboxGuiTrial.wsb'
$templatePath = Join-Path $PSScriptRoot 'SandboxGuiTrial.wsb.template'
$manifestPath = Join-Path $payload 'manifest.json'
$planPath = Join-Path $payload 'plan.json'
$wsb = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\wsb.exe'
$activeNames = @('WindowsSandbox', 'WindowsSandboxClient', 'WindowsSandboxRemoteSession', 'vmmemWindowsSandbox')

if (-not (Test-Path -LiteralPath $templatePath)) { throw "Missing the .wsb template at $templatePath." }
if (-not (Test-Path -LiteralPath $wsb)) { throw "Missing the Sandbox CLI at $wsb." }
if (-not (Test-Path -LiteralPath $manifestPath)) { throw "Payload is not prepared. Run New-SandboxGuiPayload.ps1 first." }
if (-not (Test-Path -LiteralPath $planPath)) { throw "Payload has no plan.json. Run New-SandboxGuiPayload.ps1 first." }
if (-not (Test-Path -LiteralPath $results -PathType Container)) { throw "Missing the results folder at $results." }

$computer = Get-CimInstance Win32_ComputerSystem
$feature = Get-CimInstance Win32_OptionalFeature -Filter "Name='Containers-DisposableClientVM'" -ErrorAction SilentlyContinue
$sandboxInstallState = if ($null -ne $feature) { [int]$feature.InstallState } else { -1 }
$active = @(Get-Process -Name $activeNames -ErrorAction SilentlyContinue)
$plan = Get-Content -LiteralPath $planPath -Raw | ConvertFrom-Json

[pscustomobject]@{
    TrialRoot = $TrialRoot
    SandboxInstallState = $sandboxInstallState
    HypervisorPresent = [bool]$computer.HypervisorPresent
    ExistingSandboxClientCount = $active.Count
    Preset = $plan.preset
    Cases = (@(@($plan.cases) | ForEach-Object { if ($_.selector) { $_.selector } else { '<full suite>' } }) -join ', ')
} | Format-List

if ($sandboxInstallState -ne 1) {
    throw 'The Windows Sandbox feature is not installed. Enable Containers-DisposableClientVM, reboot, and recheck.'
}
if (-not $computer.HypervisorPresent) {
    throw 'The hypervisor is not running. Reboot after enabling Windows Sandbox and recheck.'
}
if ($active.Count -gt 0) {
    throw 'A Sandbox client already exists. Close it before starting another run.'
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$verifiedBytes = 0
$verifiedFiles = 0
foreach ($entry in $manifest.files) {
    $file = [IO.Path]::GetFullPath((Join-Path $payload $entry.path))
    if (-not $file.StartsWith($payload + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Invalid manifest path: $($entry.path)"
    }
    if (-not (Test-Path -LiteralPath $file)) { throw "Payload file is missing: $($entry.path)" }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry.sha256) {
        throw "Payload hash mismatch: $($entry.path). Rebuild the payload."
    }
    $verifiedFiles++
    $verifiedBytes += (Get-Item -LiteralPath $file).Length
}
Write-Host ("Payload verified: {0} files, {1:N1} MiB" -f $verifiedFiles, ($verifiedBytes / 1MB))

function ConvertTo-XmlText([string]$text) {
    return $text.Replace('&', '&amp;').Replace('<', '&lt;').Replace('>', '&gt;').Replace('"', '&quot;').Replace("'", '&apos;')
}

$template = Get-Content -LiteralPath $templatePath -Raw
$configuration = $template.Replace('{{MEMORY_MB}}', [string]$MemoryMB).
    Replace('{{PAYLOAD}}', (ConvertTo-XmlText $payload)).
    Replace('{{RESULTS}}', (ConvertTo-XmlText $results))
Set-Content -LiteralPath $wsbPath -Value $configuration -Encoding UTF8

[xml]$parsed = Get-Content -LiteralPath $wsbPath -Raw
$mapped = @($parsed.Configuration.MappedFolders.MappedFolder)
if ($mapped.Count -ne 2 -or
    $mapped[0].HostFolder -ine $payload -or $mapped[0].SandboxFolder -ine 'C:\SandboxTrialInput' -or $mapped[0].ReadOnly -ne 'true' -or
    $mapped[1].HostFolder -ine $results -or $mapped[1].SandboxFolder -ine 'C:\SandboxTrialResults' -or $mapped[1].ReadOnly -ne 'false') {
    throw 'Generated Sandbox configuration does not match the expected payload and results mappings.'
}
foreach ($setting in @('Networking', 'ClipboardRedirection', 'AudioInput', 'VideoInput', 'PrinterRedirection')) {
    if ($parsed.Configuration.$setting -ne 'Disable') { throw "Sandbox setting $setting must be Disable." }
}
if ($parsed.Configuration.LogonCommand.Command -notmatch 'C:\\SandboxTrialInput\\Run-Guest\.ps1$') {
    throw 'Unexpected Sandbox logon command.'
}
Write-Host "Wrote $wsbPath"

if (Test-Path -LiteralPath $results) {
    $latest = Get-ChildItem -LiteralPath $results -Filter 'summary.json' -File -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($null -ne $latest) {
        $summary = Get-Content -LiteralPath $latest.FullName -Raw | ConvertFrom-Json
        Write-Host "Latest run: $($latest.Directory.Name) -> $($summary.overall)"
    }
}

if (-not $Launch) {
    Write-Host 'Inspection only. Add -Launch to open the Sandbox.'
    exit 0
}

Write-Host 'Launching Windows Sandbox, not the test binary on the host.'
Write-Host 'The guest waits 20 seconds before starting. Minimize the Sandbox window, do not click inside it,'
Write-Host 'and check that host keyboard and mouse stay responsive.'
Start-Process -FilePath $wsbPath

$sandboxId = $null
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    $raw = & $wsb list --raw 2>$null | Out-String
    if ($raw -notmatch '\S') { continue }
    try { $listed = $raw | ConvertFrom-Json } catch { continue }
    $environments = @($listed.WindowsSandboxEnvironments)
    if ($environments.Count -ge 1) { $sandboxId = $environments[0].Id; break }
}
[ordered]@{
    startedUtc = (Get-Date).ToUniversalTime().ToString('o')
    sandboxId = $sandboxId
    trialRoot = $TrialRoot
    wsbPath = $wsbPath
    memoryMB = $MemoryMB
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TrialRoot 'current-sandbox.json') -Encoding UTF8

if ($null -eq $sandboxId) {
    Write-Host 'Sandbox window opened, but no environment id was reported yet. Check wsb list --raw shortly.'
} else {
    Write-Host "Sandbox environment: $sandboxId"
}
Write-Host "Results: $results"
Write-Host 'When the run finishes, close the guest with Stop-SandboxGuiTrial.ps1 before the next run.'
