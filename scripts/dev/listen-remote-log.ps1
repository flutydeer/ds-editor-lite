<#
.SYNOPSIS
    Receives log lines that a remote DS Editor Lite instance mirrors over UDP.

.DESCRIPTION
    Pairs with the editor's --log-udp host:port option. The editor sends exactly
    one datagram per log line, so this script only has to print what arrives.
    Run it on the build machine and point the device at this machine's address:

        .\scripts\dev\listen-remote-log.ps1 -Port 9999

    deploy-to-device.ps1 starts this listener for you unless -NoListen is given.

.PARAMETER Port
    UDP port to bind. Must match the port the editor was given.

.PARAMETER BindAddress
    Local address to bind. The default listens on every interface, which is what
    a device on the LAN needs. Use 127.0.0.1 to accept only local traffic.

.PARAMETER LogFile
    Optional path that also receives every line, for review after the session.

.PARAMETER Source
    Prefix each line with the sender's address. Useful when several devices or
    several editor instances log to the same port.

.EXAMPLE
    .\scripts\dev\listen-remote-log.ps1 -Port 9999 -LogFile logs\surface.log
#>
[CmdletBinding()]
param(
    [int]$Port = 9999,
    [string]$BindAddress = "0.0.0.0",
    [string]$LogFile = "",
    [switch]$Source
)

$ErrorActionPreference = "Stop"

# The editor emits UTF-8; without this, non-ASCII text (Chinese lyrics, paths)
# would be mangled by the console's default code page.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if ($LogFile) {
    $LogFile = [System.IO.Path]::GetFullPath($LogFile)
    $logDirectory = Split-Path -Parent $LogFile
    if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }
}

$localEndpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse($BindAddress), $Port)
$client = [System.Net.Sockets.UdpClient]::new($localEndpoint)
# Add-Content -Encoding utf8 writes a BOM on Windows PowerShell; a log file that
# other tools will parse is better off without one.
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

Write-Host "Listening for mirrored editor logs on udp://${BindAddress}:$Port" -ForegroundColor Cyan
Write-Host "Start the editor with --log-udp <this-machine-ip>:$Port, or use deploy-to-device.ps1." -ForegroundColor DarkGray
Write-Host "Press Ctrl+C to stop." -ForegroundColor DarkGray
Write-Host ""

$sender = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
$lineCount = 0
try {
    while ($true) {
        $datagram = $client.Receive([ref]$sender)
        if ($datagram.Length -eq 0) {
            continue
        }

        # One datagram carries one log line, without a trailing newline.
        $text = [System.Text.Encoding]::UTF8.GetString($datagram)
        if ($Source) {
            $text = "[$($sender.Address)] $text"
        }

        Write-Host $text
        if ($LogFile) {
            [System.IO.File]::AppendAllText($LogFile, $text + [Environment]::NewLine, $utf8NoBom)
        }
        $lineCount++
    }
}
finally {
    $client.Close()
    Write-Host ""
    Write-Host "Stopped after $lineCount log line(s)." -ForegroundColor Cyan
}
