@echo off
REM Starts DS Editor Lite on the debugging device, replacing any running instance.
REM
REM deploy-to-device.ps1 copies this file next to the deployed binaries and runs
REM it through a scheduled task, so it executes inside the signed-in user's
REM interactive session. That is the whole point: a process started directly
REM over SSH is a child of the sshd service in session 0, and its window can
REM never appear on the device's desktop.
REM
REM Two further details:
REM
REM   * The editor enforces a single instance: a second launch forwards its
REM     request to the running process and exits, so without the taskkill below a
REM     redeploy would silently keep the old binary and the old log target alive.
REM
REM   * The editor is started with "start", never through
REM     Win32_Process::Create: WMI would create it under WmiPrvSE in session 0
REM     again, undoing what running from a scheduled task just achieved.
REM
REM Usage: start-editor.cmd [--log-udp host:port] [other editor options]

setlocal

set "EDITOR_DIR=%~dp0"
set "EDITOR_EXE=%EDITOR_DIR%DsEditorLite.exe"

if not exist "%EDITOR_EXE%" (
    echo start-editor: cannot find "%EDITOR_EXE%" 1>&2
    exit /b 1
)

taskkill /IM DsEditorLite.exe /F >nul 2>&1

REM "start" detaches the editor so it outlives this task.
start "" /D "%EDITOR_DIR%" "%EDITOR_EXE%" %*

exit /b 0
