# Local development experiment. Runs only inside Windows Sandbox.
# Passive observer: enumerates top-level windows and captures desktop screenshots
# while the tests run. It never injects input and never changes window state.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [Parameter(Mandatory = $true)][string]$StopFile,
    [int]$SampleIntervalMs = 150,
    [int]$ScreenshotIntervalMs = 8000,
    [int]$MaxScreenshots = 30,
    [switch]$AllowHostDesktop
)
$ErrorActionPreference = 'Stop'

if (-not $AllowHostDesktop -and
    -not ($OutputDirectory.StartsWith('C:\SandboxTrialResults\', [StringComparison]::OrdinalIgnoreCase))) {
    throw 'The window observer writes only to the dedicated results mapping.'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$shotDirectory = Join-Path $OutputDirectory 'shots'
New-Item -ItemType Directory -Path $shotDirectory -Force | Out-Null
$csvPath = Join-Path $OutputDirectory 'windows.csv'
$summaryPath = Join-Path $OutputDirectory 'windows-summary.json'

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class TrialWindowSampler {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool IsIconic(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowTextW(IntPtr hWnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassNameW(IntPtr hWnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    public static List<string> Sample() {
        List<string> rows = new List<string>();
        EnumWindowsProc callback = delegate(IntPtr hWnd, IntPtr lParam) {
            if (!IsWindowVisible(hWnd)) { return true; }
            uint processId;
            GetWindowThreadProcessId(hWnd, out processId);
            StringBuilder title = new StringBuilder(512);
            GetWindowTextW(hWnd, title, title.Capacity);
            StringBuilder className = new StringBuilder(256);
            GetClassNameW(hWnd, className, className.Capacity);
            RECT rect;
            bool hasRect = GetWindowRect(hWnd, out rect);
            rows.Add(string.Join("\t", new string[] {
                processId.ToString(),
                hWnd.ToInt64().ToString(),
                title.ToString(),
                className.ToString(),
                IsIconic(hWnd) ? "1" : "0",
                hasRect ? (rect.Right - rect.Left).ToString() : "",
                hasRect ? (rect.Bottom - rect.Top).ToString() : ""
            }));
            return true;
        };
        EnumWindows(callback, IntPtr.Zero);
        return rows;
    }
}
'@

function Format-CsvField([string]$value) {
    if ($null -eq $value) { return '""' }
    return '"' + ($value -replace '"', '""') + '"'
}

$writer = New-Object System.IO.StreamWriter($csvPath, $false, (New-Object System.Text.UTF8Encoding($true)))
$writer.WriteLine('utc,pid,process,hwnd,title,class,iconic,width,height')

$processCache = @{}
$seenWindows = @{}
$sampleCount = 0
$ourWindowSamples = 0
$maxConcurrent = 0
$firstOurWindowUtc = $null
$lastOurWindowUtc = $null
$titles = @{}
$classes = @{}
$screenshots = 0
$screenshotErrors = 0
$lastScreenshotUtc = [DateTime]::MinValue
$startedUtc = (Get-Date).ToUniversalTime()
$errorText = $null

try {
    while (-not (Test-Path -LiteralPath $StopFile)) {
        $sampleUtc = (Get-Date).ToUniversalTime()
        $ourCount = 0
        $captureNow = $false
        try {
            $rows = [TrialWindowSampler]::Sample()
        } catch {
            $errorText = $_.Exception.Message
            $rows = @()
        }
        foreach ($row in $rows) {
            $parts = $row -split "`t", 7
            $windowPid = [int]$parts[0]
            if (-not $processCache.ContainsKey($windowPid)) {
                $name = ''
                try {
                    $name = (Get-Process -Id $windowPid -ErrorAction Stop).ProcessName
                } catch {
                    $name = '<exited>'
                }
                $processCache[$windowPid] = $name
            }
            $processName = $processCache[$windowPid]
            $isOurs = $processName -like 'TestNativeDesktop*'
            if ($isOurs) {
                $ourCount++
                if (-not $seenWindows.ContainsKey($parts[1])) {
                    $seenWindows[$parts[1]] = $true
                    $captureNow = $true
                }
                $titles[$parts[2]] = $true
                $classes[$parts[3]] = $true
                if ($null -eq $firstOurWindowUtc) { $firstOurWindowUtc = $sampleUtc }
                $lastOurWindowUtc = $sampleUtc
            }
            $null = $writer.WriteLine(
                $sampleUtc.ToString('o') + ',' +
                $windowPid + ',' +
                (Format-CsvField $processName) + ',' +
                $parts[1] + ',' +
                (Format-CsvField $parts[2]) + ',' +
                (Format-CsvField $parts[3]) + ',' +
                $parts[4] + ',' + $parts[5] + ',' + $parts[6])
        }
        $writer.Flush()
        $sampleCount++
        $ourWindowSamples += $ourCount
        if ($ourCount -gt $maxConcurrent) { $maxConcurrent = $ourCount }

        if ($ourCount -gt 0 -and $screenshots -lt $MaxScreenshots -and ($captureNow -or ($sampleUtc - $lastScreenshotUtc).TotalMilliseconds -ge $ScreenshotIntervalMs)) {
            $lastScreenshotUtc = $sampleUtc
            $shotPath = Join-Path $shotDirectory ('shot-' + $sampleUtc.ToString('yyyyMMddTHHmmssfffZ') + '-' +
                $screenshots.ToString('D3') + '.png')
            try {
                $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
                $bitmap = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                try {
                    $graphics.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bitmap.Size)
                    $bitmap.Save($shotPath, [System.Drawing.Imaging.ImageFormat]::Png)
                } finally {
                    $graphics.Dispose()
                    $bitmap.Dispose()
                }
                $screenshots++
            } catch {
                $screenshotErrors++
                $errorText = $_.Exception.Message
            }
        }

        Start-Sleep -Milliseconds $SampleIntervalMs
    }
} finally {
    $writer.Dispose()
    [ordered]@{
        startedUtc = $startedUtc.ToString('o')
        finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
        sampleIntervalMs = $SampleIntervalMs
        screenshotIntervalMs = $ScreenshotIntervalMs
        sampleCount = $sampleCount
        testWindowSamples = $ourWindowSamples
        maxConcurrentTestWindows = $maxConcurrent
        distinctTestWindowHandles = $seenWindows.Count
        firstTestWindowUtc = if ($null -ne $firstOurWindowUtc) { $firstOurWindowUtc.ToString('o') } else { $null }
        lastTestWindowUtc = if ($null -ne $lastOurWindowUtc) { $lastOurWindowUtc.ToString('o') } else { $null }
        distinctTestWindowTitles = @($titles.Keys | Sort-Object)
        distinctTestWindowClasses = @($classes.Keys | Sort-Object)
        screenshotCount = $screenshots
        screenshotCap = $MaxScreenshots
        screenshotErrorCount = $screenshotErrors
        lastError = $errorText
        hostInputInjected = $false
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
