param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDirectory,
    [string]$InstallerPath = "",
    [switch]$RequireAdminInstallerTest
)

$ErrorActionPreference = "Stop"
$packageRoot = [System.IO.Path]::GetFullPath($PackageDirectory)
$packageExecutable = Join-Path $packageRoot "KalkulatorTrasKablowych.exe"
if (-not (Test-Path -LiteralPath $packageExecutable)) {
    throw "Package executable is missing: $packageExecutable"
}

function Invoke-SmokeTest([string]$Executable) {
    $previousPlatform = $env:QT_QPA_PLATFORM
    $previousSmokeTest = $env:KTK_SMOKE_TEST
    try {
        # The portable package intentionally contains qwindows.dll, not the
        # development-only qoffscreen plugin used by CTest.
        $env:QT_QPA_PLATFORM = "windows"
        $env:KTK_SMOKE_TEST = "1"
        $process = Start-Process -FilePath $Executable `
            -ArgumentList "--smoke-test" -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(90000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "Smoke test timed out for $Executable"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) {
            throw "Smoke test failed with exit code $($process.ExitCode) for $Executable"
        }
    } finally {
        $env:QT_QPA_PLATFORM = $previousPlatform
        $env:KTK_SMOKE_TEST = $previousSmokeTest
    }
}

Invoke-SmokeTest $packageExecutable

if (-not [string]::IsNullOrWhiteSpace($InstallerPath)) {
    $isElevated = [Security.Principal.WindowsPrincipal]::new(
        [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isElevated) {
        if ($RequireAdminInstallerTest) {
            throw "Administrator installer smoke test is required, but the current process is not elevated."
        }
        Write-Warning "Administrator installer smoke test skipped in a non-elevated process. Run packaging from an elevated PowerShell; GitHub CI enforces the complete test."
        Write-Output "Windows package smoke test passed (portable package; admin installer deferred)."
        return
    }

    $installer = [System.IO.Path]::GetFullPath($InstallerPath)
    if (-not (Test-Path -LiteralPath $installer)) {
        throw "Installer is missing: $installer"
    }

    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $installRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $tempRoot "KTK installer smoke $PID"))
    $unsafeRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $tempRoot "KTK installer unsafe $PID"))
    if (-not $installRoot.StartsWith(
            $tempRoot,
            [System.StringComparison]::OrdinalIgnoreCase) -or
        -not $unsafeRoot.StartsWith(
            $tempRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe installer smoke-test directory."
    }

    try {
        # Regression: NSIS wildcard checks can report an existing empty
        # directory as non-empty because of the implicit . and .. entries.
        # Create the directory before installation so this exact case is tested.
        New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
        if (@(Get-ChildItem -LiteralPath $installRoot -Force).Count -ne 0) {
            throw "Installer smoke-test directory is not empty before installation."
        }
        $installProcess = Start-Process -FilePath $installer `
            -ArgumentList @("/S", "/NO_SHORTCUTS=1", "/D=$installRoot") `
            -WindowStyle Hidden -Wait -PassThru
        if ($installProcess.ExitCode -ne 0) {
            throw "Silent installer failed with exit code $($installProcess.ExitCode)"
        }
        $registeredInstallRoot = Get-ItemPropertyValue `
            -LiteralPath "HKLM:\Software\KubwojPrime\KalkulatorTrasKablowych" `
            -Name "InstallDir"
        if ([System.IO.Path]::GetFullPath($registeredInstallRoot) -ne $installRoot) {
            throw "Installer did not preserve the selected installation directory."
        }
        $uninstallRegistration = Get-ItemProperty `
            -LiteralPath "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\KalkulatorTrasKablowych"
        if ($uninstallRegistration.InstallLocation -ne $installRoot -or
            [string]::IsNullOrWhiteSpace($uninstallRegistration.UninstallString)) {
            throw "Installer did not create the expected all-users uninstall registration."
        }
        Write-Output "All-users HKLM registration test passed."
        Write-Output "Existing empty directory installation test passed."
        Invoke-SmokeTest (Join-Path $installRoot "KalkulatorTrasKablowych.exe")

        $uninstaller = Join-Path $installRoot "Uninstall.exe"
        if (-not (Test-Path -LiteralPath $uninstaller)) {
            throw "Installer did not create an uninstaller."
        }
        $uninstallProcess = Start-Process -FilePath $uninstaller `
            -ArgumentList @("/S", "_?=$installRoot") `
            -WindowStyle Hidden -Wait -PassThru
        if ($uninstallProcess.ExitCode -ne 0) {
            throw "Silent uninstaller failed with exit code $($uninstallProcess.ExitCode)"
        }

        New-Item -ItemType Directory -Path $unsafeRoot -Force | Out-Null
        $sentinel = Join-Path $unsafeRoot "do-not-delete.txt"
        Set-Content -LiteralPath $sentinel -Value "installer safety test"
        $unsafeInstallProcess = Start-Process -FilePath $installer `
            -ArgumentList @("/S", "/NO_SHORTCUTS=1", "/D=$unsafeRoot") `
            -WindowStyle Hidden -Wait -PassThru
        if ($unsafeInstallProcess.ExitCode -eq 0) {
            throw "Installer accepted a non-empty foreign directory."
        }
        if (-not (Test-Path -LiteralPath $sentinel) -or
            (Test-Path -LiteralPath (Join-Path $unsafeRoot "KalkulatorTrasKablowych.exe"))) {
            throw "Installer changed a protected foreign directory."
        }
        Write-Output "Non-empty foreign directory protection test passed."
    } finally {
        foreach ($testRoot in @($installRoot, $unsafeRoot)) {
            if (Test-Path -LiteralPath $testRoot) {
                Remove-Item -LiteralPath $testRoot -Recurse -Force
            }
        }
    }
}

Write-Output "Windows package smoke test passed."
