# Measure the G2P output of a voicebank for a fixed lyric set, so two runs can be diffed.
#
# Why: a language package may change its dictionary without changing any version number, and the
# package loader cannot detect that (see ../packaging-voicebank-essentials.md section 4). The only
# way to judge whether a dictionary update is incompatible is to compare what the G2P produces for
# the same words before and after. This tool produces that comparable output.
#
# Usage:
#   pwsh -File measure-g2p-output.ps1 -EditorExe <DsEditorLite.exe> -SingersRoot <Singers dir> `
#        -OutRoot <scratch dir> -Out <before.txt> -Words "你 好" -Speaker Yousa_Normal `
#        -SingerPackage yousa -SingerPackageVersion 1.65.1 -SingerId yousa
#
# Diff two runs:
#   Compare-Object (Get-Content before.txt) (Get-Content after.txt)
#
# Requires: PowerShell 7 (this file is UTF-8 and may carry non-ASCII lyrics), a built DsEditorLite,
# and wolf language packages staged inside that build tree (out/bin/wolf/packages). Swapping the
# provider package between runs is the caller's job: this tool only measures.
#
# The automation call shapes below are the ones that were verified end to end. Do not "simplify"
# them without re-verifying:
# documents.new requires unsaved_policy, ids must be read back through tracks.list / clips.list,
# and rendering is awaited by polling notes.list rather than by a fixed sleep.

param(
    [Parameter(Mandatory = $true)][string]$EditorExe,
    [Parameter(Mandatory = $true)][string]$SingersRoot,
    [Parameter(Mandatory = $true)][string]$OutRoot,
    [string]$Out = '',
    [string]$Words = 'ni hao',
    [string]$Language = 'cmn',
    [string]$Speaker = '',
    # The three identity fields below have no default: a default would name one package of the
    # author's machine, and a run that measures another package than the intended one looks the
    # same as a run that measured the right package.
    [Parameter(Mandatory = $true)][string]$SingerPackage,
    [Parameter(Mandatory = $true)][string]$SingerPackageVersion,
    [Parameter(Mandatory = $true)][string]$SingerId,
    [string]$ExtraPath = '',
    [int]$NoteTicks = 960,
    [int]$TimeoutSec = 240,
    [string]$Json = '',
    [switch]$KeepInstance
)

$ErrorActionPreference = 'Stop'

foreach ($duplicate in 'no_proxy', 'http_proxy', 'https_proxy') {
    Remove-Item -LiteralPath ("Env:" + $duplicate) -ErrorAction SilentlyContinue
}

if (Test-Path $OutRoot) { Remove-Item -Recurse -Force $OutRoot }
$roaming = Join-Path $OutRoot 'Roaming'
$local = Join-Path $OutRoot 'Local'
$editorData = Join-Path $roaming 'OpenVPI\DS Editor Lite'
New-Item -ItemType Directory -Force -Path $editorData, $local | Out-Null

$config = [ordered]@{
    automation = [ordered]@{ accessRoots = @(($OutRoot -replace '\\', '/')) }
    general    = [ordered]@{ packageSearchPaths = @($SingersRoot) }
}
$config | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $editorData 'appConfig.json') -Encoding UTF8

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()

$env:APPDATA = $roaming
$env:LOCALAPPDATA = $local
$env:XDG_DATA_HOME = $roaming
$env:XDG_CONFIG_HOME = $roaming
$env:QT_LOGGING_TO_CONSOLE = '1'
if ($ExtraPath) { $env:PATH = $ExtraPath + ';' + $env:PATH }

$proc = Start-Process -FilePath $EditorExe -ArgumentList @(
    '--headless', '--no-mcp', '--control-level', 'l3', '--control-port', "$port") -PassThru `
    -WindowStyle Hidden -RedirectStandardOutput (Join-Path $OutRoot 'editor.out.log') `
    -RedirectStandardError (Join-Path $OutRoot 'editor.err.log')

$endpoint = "http://127.0.0.1:$port/automation/v1"
$script:counter = 0

function Call([string]$Method, $Params) {
    $script:counter++
    $paramsJson = if ($null -eq $Params) { '{}' } else { $Params | ConvertTo-Json -Depth 24 -Compress }
    $body = '{"jsonrpc":"2.0","id":"m' + $script:counter + '","method":"' + $Method +
        '","params":' + $paramsJson + '}'
    $file = Join-Path $env:TEMP ("g2p-" + [guid]::NewGuid().ToString('N') + ".json")
    [System.IO.File]::WriteAllText($file, $body, (New-Object System.Text.UTF8Encoding($false)))
    $raw = & curl.exe --noproxy '*' -s --max-time 120 -X POST $endpoint `
        -H 'Content-Type: application/json' --data-binary "@$file" 2>&1
    $code = $LASTEXITCODE
    Remove-Item $file -Force -ErrorAction SilentlyContinue
    if ($code -ne 0) { throw ("curl exit " + $code + " for " + $Method) }
    $parsed = (($raw -join "`n") | ConvertFrom-Json)
    if ($parsed.PSObject.Properties.Name -contains 'error') {
        throw ($Method + " failed: " + $parsed.error.message + " " +
            ($parsed.error.data | ConvertTo-Json -Depth 6 -Compress))
    }
    return $parsed.result
}

function Revision([string]$DocumentId) {
    return [int](Call 'documents.get' @{ document_id = $DocumentId }).document.revision
}

function LastId($Listing, [string]$Property) {
    $all = @($Listing.$Property)
    if ($all.Count -eq 0) { return $null }
    return $all[$all.Count - 1]
}

try {
    $deadline = (Get-Date).AddSeconds(90)
    $ready = $false
    while ((Get-Date) -lt $deadline) {
        if ($proc.HasExited) { throw ("editor exited with " + $proc.ExitCode) }
        try { Call 'application.get_status' $null | Out-Null; $ready = $true; break }
        catch { Start-Sleep -Milliseconds 700 }
    }
    if (-not $ready) { throw 'editor did not become ready within 90 seconds' }

    $doc = Call 'documents.new' @{ unsaved_policy = 'discard' }
    $documentId = $doc.current.document_id

    Call 'tracks.insert' @{ document_id = $documentId; expected_revision = (Revision $documentId)
        index = 0; tracks = @(@{ name = 'g2p-measure' }) } | Out-Null
    $trackId = (LastId (Call 'tracks.list' @{ document_id = $documentId }) 'tracks').track_id

    $speakerValue = if ([string]::IsNullOrEmpty($Speaker)) { $null } else { @{ speaker_id = $Speaker } }
    Call 'tracks.set_voice' @{ document_id = $documentId; expected_revision = (Revision $documentId)
        track_id = $trackId; voice = @{
            singer = @{ package_id = $SingerPackage; package_version = $SingerPackageVersion
                singer_id = $SingerId }
            speaker = $speakerValue } } | Out-Null

    Call 'clips.insert' @{ document_id = $documentId; expected_revision = (Revision $documentId)
        clips = @(@{ track_id = $trackId; start = 0; length = 7680 }) } | Out-Null
    $clipId = (LastId (Call 'clips.list' @{ document_id = $documentId; track_id = $trackId }) 'clips').clip_id

    $tokens = @($Words -split '\s+' | Where-Object { $_ })
    $notes = @()
    $cursor = $NoteTicks
    foreach ($token in $tokens) {
        $notes += @{ local_start = $cursor; length = $NoteTicks; key_index = 60; lyric = $token
            language = @{ mode = 'explicit'; language_id = $Language } }
        $cursor += $NoteTicks
    }
    Call 'notes.insert' @{ document_id = $documentId; expected_revision = (Revision $documentId)
        clip_id = $clipId; notes = $notes } | Out-Null

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $listing = $null
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 3
        try {
            $listing = Call 'notes.list' @{ document_id = $documentId; clip_id = $clipId }
            if (@(@($listing.notes)[0].phonemes).Count -gt 0) { break }
        } catch { Write-Host ("notes.list failed: " + $_.Exception.Message) }
    }

    $rows = @()
    foreach ($note in $listing.notes) {
        $symbols = @($note.phonemes | ForEach-Object { $_.symbol }) -join ' '
        $rows += [pscustomobject]@{ lyric = $note.lyric; phonemes = $symbols; note_id = $note.note_id }
        Write-Output ($note.lyric + "`t" + $symbols)
    }
    if ($Out) {
        # Keep the artifact inside the run directory: a relative -Out used to be resolved against
        # the caller's working directory, which littered the repository root.
        $outPath = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $OutRoot $Out }
        $rows | ForEach-Object { $_.lyric + "`t" + $_.phonemes } | Set-Content -Path $outPath -Encoding UTF8
    }
    if ($Json) {
        # Same rule as -Out: a relative -Json belongs to the run directory, never to the caller.
        $jsonPath = if ([System.IO.Path]::IsPathRooted($Json)) { $Json } else { Join-Path $OutRoot $Json }
        [ordered]@{ words = $tokens; language = $Language; speaker = $Speaker; note_ticks = $NoteTicks
            singer = @{ package_id = $SingerPackage; package_version = $SingerPackageVersion
                singer_id = $SingerId }; notes = $rows } |
            ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding UTF8
    }
} finally {
    if (-not $KeepInstance) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
}

