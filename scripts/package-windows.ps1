param(
    [string]$Preset = "windows-release",
    [string]$BuildDirectory = "",
    [string]$Version = "",
    [switch]$SkipBuild,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "build\release"))
} elseif ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $buildRoot = [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    $buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
}
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "release"))

foreach ($path in @($buildRoot, $releaseRoot)) {
    if (-not $path.StartsWith(
            $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Packaging path is outside the project: $path"
    }
}

if (-not $SkipBuild) {
    $localQtBin = "C:\Qt\6.11.0\mingw_64\bin"
    $localMingwBin = "C:\Qt\Tools\mingw1310_64\bin"
    if (Test-Path -LiteralPath $localQtBin) {
        $env:PATH = "$localQtBin;" + $env:PATH
    }
    if (Test-Path -LiteralPath $localMingwBin) {
        $env:PATH = "$localMingwBin;" + $env:PATH
    }

    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    cmake --build --preset $Preset --parallel 4
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
}

$versionFile = Join-Path $buildRoot "ktk-version.txt"
if (-not (Test-Path -LiteralPath $versionFile)) {
    throw "Missing configured version file: $versionFile"
}
$configuredVersion = (Get-Content -Raw -LiteralPath $versionFile).Trim()
if ($configuredVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "Invalid configured application version: $configuredVersion"
}
if (-not [string]::IsNullOrWhiteSpace($Version) -and $Version -ne $configuredVersion) {
    throw "Requested version $Version differs from CMake version $configuredVersion."
}
$Version = $configuredVersion

ctest --test-dir $buildRoot --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }

$stageRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $releaseRoot "KalkulatorTrasKablowych-$Version-win64"))
$archivePath = [System.IO.Path]::GetFullPath("$stageRoot.zip")

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

$executable = Join-Path $buildRoot "KalkulatorTrasKablowych.exe"
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Missing executable: $executable"
}
Copy-Item -LiteralPath $executable -Destination $stageRoot

foreach ($file in @("EULA.txt", "CHANGELOG.md", "LICENSE.md", "README.md", "THIRD_PARTY_NOTICES.md")) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $stageRoot
}

$docsDirectory = Join-Path $stageRoot "docs"
New-Item -ItemType Directory -Path $docsDirectory -Force | Out-Null
foreach ($file in @("fire-load-estimation.md", "baks-route-mass.md", "data-governance.md", "license-and-access.md")) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "docs\$file") -Destination $docsDirectory
}

$licenseDirectory = Join-Path $stageRoot "licenses"
New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "third_party\QXlsx\LICENSE") `
    -Destination (Join-Path $licenseDirectory "QXlsx-MIT.txt")

$licenseCopies = @{
    "C:\Qt\Tools\QtCreator\share\qtcreator\generic-highlighter\syntax\licenses\LICENSE.LGPLv3" = "Qt-LGPL-3.0.txt"
    "C:\Qt\Tools\mingw1310_64\licenses\gcc\COPYING3" = "GNU-GPL-3.0.txt"
    "C:\Qt\Tools\mingw1310_64\licenses\gcc\COPYING.RUNTIME" = "GCC-Runtime-Library-Exception.txt"
    "C:\Qt\Tools\mingw1310_64\licenses\mingw-w64\COPYING.MinGW-w64-runtime.txt" = "MinGW-w64-runtime.txt"
    "C:\Qt\Tools\mingw1310_64\licenses\winpthreads\COPYING" = "winpthreads-COPYING.txt"
}
foreach ($source in $licenseCopies.Keys) {
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source `
            -Destination (Join-Path $licenseDirectory $licenseCopies[$source])
    }
}

function Ensure-LicenseFile(
    [string]$DestinationName,
    [string[]]$Candidates,
    [string]$FallbackUrl
) {
    $destination = Join-Path $licenseDirectory $DestinationName
    if (Test-Path -LiteralPath $destination) { return }
    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            Copy-Item -LiteralPath $candidate -Destination $destination
            return
        }
    }
    Invoke-WebRequest -Uri $FallbackUrl -OutFile $destination
}

Ensure-LicenseFile "Qt-LGPL-3.0.txt" @(
    "C:\Qt\Tools\QtCreator\share\qtcreator\generic-highlighter\syntax\licenses\LICENSE.LGPLv3"
) "https://www.gnu.org/licenses/lgpl-3.0.txt"
Ensure-LicenseFile "GNU-GPL-3.0.txt" @(
    "C:\Qt\Tools\mingw1310_64\licenses\gcc\COPYING3"
) "https://www.gnu.org/licenses/gpl-3.0.txt"
Ensure-LicenseFile "GCC-Runtime-Library-Exception.txt" @(
    "C:\Qt\Tools\mingw1310_64\licenses\gcc\COPYING.RUNTIME"
) "https://www.gnu.org/licenses/gcc-exception-3.1.txt"

$windeployqtCommand = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
$windeployqt = if ($windeployqtCommand) {
    $windeployqtCommand.Source
} else {
    "C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe"
}
if (-not (Test-Path -LiteralPath $windeployqt)) {
    throw "Missing windeployqt: $windeployqt"
}
& $windeployqt --release --compiler-runtime --no-translations `
    --skip-plugin-types generic,networkinformation,tls `
    --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql `
    (Join-Path $stageRoot "KalkulatorTrasKablowych.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

& (Join-Path $PSScriptRoot "test-windows-package.ps1") -PackageDirectory $stageRoot
if ($LASTEXITCODE -ne 0) { throw "Portable package smoke test failed." }

Compress-Archive -Path (Join-Path $stageRoot "*") `
    -DestinationPath $archivePath -CompressionLevel Optimal

$artifacts = @($archivePath)
if (-not $SkipInstaller) {
    $makeNsisCommand = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
    $makeNsis = if ($makeNsisCommand) {
        $makeNsisCommand.Source
    } else {
        "C:\Program Files (x86)\NSIS\makensis.exe"
    }
    if (-not (Test-Path -LiteralPath $makeNsis)) {
        throw "Missing NSIS compiler. Install NSIS or use -SkipInstaller."
    }

    $installerScript = Join-Path $projectRoot "installer\KalkulatorTrasKablowych.nsi"
    & $makeNsis "/DAPP_VERSION=$Version" "/DSOURCE_DIR=$stageRoot" `
        "/DOUTPUT_DIR=$releaseRoot" $installerScript
    if ($LASTEXITCODE -ne 0) { throw "NSIS installer build failed." }

    $installerPath = Join-Path $releaseRoot `
        "KalkulatorTrasKablowych-$Version-win64-setup.exe"
    if (-not (Test-Path -LiteralPath $installerPath)) {
        throw "NSIS did not create expected installer: $installerPath"
    }
    & (Join-Path $PSScriptRoot "test-windows-package.ps1") `
        -PackageDirectory $stageRoot -InstallerPath $installerPath
    if ($LASTEXITCODE -ne 0) { throw "Installer smoke test failed." }
    $artifacts += $installerPath
}

$hashPath = Join-Path $releaseRoot "SHA256SUMS-$Version.txt"
$hashLines = foreach ($artifact in $artifacts) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact).Hash
    "$hash  $([System.IO.Path]::GetFileName($artifact))"
}
Set-Content -LiteralPath $hashPath -Value $hashLines -Encoding ascii

$artifacts + $hashPath | ForEach-Object { Write-Output $_ }
