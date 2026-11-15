#Requires -Version 5.1
# Safe to dot-source. These functions do not prepare paths until explicitly called.

function Test-SandboxConfigPathSupported {
    <#
    .SYNOPSIS
        Whether an absolute Windows directory can be recreated with a local drive letter.
    #>
    [CmdletBinding()]
    param([AllowNull()][AllowEmptyString()][string]$Path)

    return ($Path -match '^[A-Za-z]:\\.+$')
}

function Initialize-SandboxConfigPath {
    <#
    .SYNOPSIS
        Place a configs directory at the absolute parent embedded in a test binary.
    .DESCRIPTION
        C: uses the existing system drive. Other letters must be unoccupied and are
        mapped to a private directory under WorkRoot. Existing configs are rejected.
        ResolveDriveRoot receives a drive specifier such as C: and returns its
        accessible filesystem root. MountDrive receives the drive specifier and
        backing directory. Their defaults use real guest drives; an isolated caller
        can supply a drive adapter without replacing directory or copy operations.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$EmbeddedPath,
        [Parameter(Mandatory = $true)][string]$ConfigSource,
        [Parameter(Mandatory = $true)][string]$WorkRoot,
        [scriptblock]$ResolveDriveRoot = { param($DriveSpec) $DriveSpec + '\' },
        [scriptblock]$MountDrive = {
            param($DriveSpec, $DriveRoot)
            & "$env:SystemRoot\System32\subst.exe" $DriveSpec $DriveRoot
            if ($LASTEXITCODE -ne 0) { throw "Could not create the guest-only $DriveSpec drive." }
        }
    )
    $ErrorActionPreference = 'Stop'
    if (-not (Test-SandboxConfigPathSupported -Path $EmbeddedPath)) {
        throw "Unusable embedded config path: $EmbeddedPath"
    }
    $embedded = [IO.Path]::GetFullPath($EmbeddedPath).TrimEnd('\')
    if (-not (Test-SandboxConfigPathSupported -Path $embedded)) {
        throw "Unusable embedded config path: $EmbeddedPath"
    }
    $driveSpec = $embedded.Substring(0, 2).ToUpperInvariant()
    $relativePath = $embedded.Substring(3)
    $accessibleRoot = & $ResolveDriveRoot $driveSpec
    if ($driveSpec -ne 'C:') {
        if (Test-Path -LiteralPath $accessibleRoot) {
            throw "Guest already has drive $driveSpec. Refusing to overwrite it."
        }
        $driveRoot = Join-Path $WorkRoot ('drive' + $driveSpec.Substring(0, 1))
        New-Item -ItemType Directory -Path $driveRoot | Out-Null
        & $MountDrive $driveSpec $driveRoot | Out-Null
        $accessibleRoot = & $ResolveDriveRoot $driveSpec
    }
    if (-not (Test-Path -LiteralPath $accessibleRoot -PathType Container)) {
        throw "Guest drive $driveSpec is not accessible."
    }
    $compiledParent = Join-Path $accessibleRoot $relativePath
    $configDestination = Join-Path $compiledParent 'configs'
    if (Test-Path -LiteralPath $configDestination) {
        throw "Config destination already exists. Refusing to overwrite $configDestination."
    }
    # Reusing the parent preserves its other contents; the configs child must be new.
    New-Item -ItemType Directory -Path $compiledParent -Force | Out-Null
    Copy-Item -LiteralPath $ConfigSource -Destination $compiledParent -Recurse
}
