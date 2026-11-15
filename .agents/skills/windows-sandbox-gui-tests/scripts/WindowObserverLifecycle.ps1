#Requires -Version 5.1

function Start-WindowObserver {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$ArgumentList,
        [Parameter(Mandatory = $true)][ref]$Process,
        [ValidateRange(1, 2147483647)][int]$ReadyTimeoutMs = 30000
    )
    if ($null -ne $Process.Value) { throw 'The observer process reference must be empty before startup.' }
    $eventName = 'Local\DsEditorLite.WindowObserverReady.' + [guid]::NewGuid().ToString('N')
    $ready = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset, $eventName)
    try {
        # Publish ownership before waiting so a failed handshake cannot orphan the child.
        $Process.Value = Start-Process -FilePath $FilePath `
            -ArgumentList ($ArgumentList + @('-ReadyEventName', $eventName)) `
            -PassThru -WindowStyle Hidden -ErrorAction Stop
        if (-not $ready.WaitOne($ReadyTimeoutMs)) {
            throw [TimeoutException]::new("The window observer did not become ready within $ReadyTimeoutMs ms.")
        }
        if ($Process.Value.HasExited) {
            throw 'The window observer exited before readiness could be confirmed.'
        }
    } finally {
        $ready.Dispose()
    }
}

function Send-WindowObserverReady {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$ReadyEventName)
    $ready = [Threading.EventWaitHandle]::OpenExisting($ReadyEventName)
    try { $null = $ready.Set() } finally { $ready.Dispose() }
}
