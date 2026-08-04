param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDirectory,
    [string]$InstallerPath = ""
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
    $installer = [System.IO.Path]::GetFullPath($InstallerPath)
    if (-not (Test-Path -LiteralPath $installer)) {
        throw "Installer is missing: $installer"
    }

    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $installRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $tempRoot "KTK-installer-smoke-$PID"))
    if (-not $installRoot.StartsWith(
            $tempRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe installer smoke-test directory: $installRoot"
    }

    try {
        $installProcess = Start-Process -FilePath $installer `
            -ArgumentList @("/S", "/D=$installRoot") `
            -WindowStyle Hidden -Wait -PassThru
        if ($installProcess.ExitCode -ne 0) {
            throw "Silent installer failed with exit code $($installProcess.ExitCode)"
        }
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
    } finally {
        if (Test-Path -LiteralPath $installRoot) {
            Remove-Item -LiteralPath $installRoot -Recurse -Force
        }
    }
}

Write-Output "Windows package smoke test passed."
