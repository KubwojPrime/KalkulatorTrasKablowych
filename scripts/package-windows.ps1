param(
    [string]$Preset = "windows-release",
    [string]$Version = "0.1.1"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "build\release"))
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "release"))
$stageRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $releaseRoot "KalkulatorTrasKablowych-$Version-win64"))
$archivePath = [System.IO.Path]::GetFullPath("$stageRoot.zip")
$qtBin = "C:\Qt\6.11.0\mingw_64\bin"
$mingwBin = "C:\Qt\Tools\mingw1310_64\bin"
$env:PATH = "$mingwBin;$qtBin;" + $env:PATH

if (-not $stageRoot.StartsWith(
        $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Nieprawidłowy katalog staging."
}

cmake --preset $Preset
cmake --build --preset $Preset --parallel 4
ctest --test-dir $buildRoot --output-on-failure

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

$executable = Join-Path $buildRoot "KalkulatorTrasKablowych.exe"
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Nie znaleziono pliku wykonywalnego: $executable"
}

Copy-Item -LiteralPath $executable -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot "LICENSE.md") -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot "THIRD_PARTY_NOTICES.md") -Destination $stageRoot

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

$windeployqt = Join-Path $qtBin "windeployqt.exe"
if (-not (Test-Path -LiteralPath $windeployqt)) {
    throw "Nie znaleziono windeployqt: $windeployqt"
}

& $windeployqt --release --compiler-runtime --no-translations `
    (Join-Path $stageRoot "KalkulatorTrasKablowych.exe")

Compress-Archive -LiteralPath $stageRoot -DestinationPath $archivePath -CompressionLevel Optimal
Write-Output $archivePath
