param(
    [string]$Preset = "windows-release",
    [string]$BuildDirectory = "",
    [string]$Version = "",
    [string]$SigningCertificateThumbprint = "",
    [string]$TimestampServer = "http://timestamp.digicert.com",
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

$signingCertificate = $null
if (-not [string]::IsNullOrWhiteSpace($SigningCertificateThumbprint)) {
    $normalizedThumbprint = $SigningCertificateThumbprint.Replace(" ", "").ToUpperInvariant()
    $certificatePath = "Cert:\CurrentUser\My\$normalizedThumbprint"
    $signingCertificate = Get-Item -LiteralPath $certificatePath -ErrorAction SilentlyContinue
    if (-not $signingCertificate) {
        throw "Code-signing certificate was not found: $normalizedThumbprint"
    }
    if (-not $signingCertificate.HasPrivateKey) {
        throw "Code-signing certificate has no private key: $normalizedThumbprint"
    }
    if ($signingCertificate.NotAfter -le (Get-Date)) {
        throw "Code-signing certificate has expired: $($signingCertificate.NotAfter)"
    }
    $codeSigningOid = "1.3.6.1.5.5.7.3.3"
    $hasCodeSigningUsage = @($signingCertificate.Extensions | Where-Object {
        $_ -is [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]
    } | ForEach-Object { $_.EnhancedKeyUsages } | Where-Object {
        $_.Value -eq $codeSigningOid
    }).Count -gt 0
    if (-not $hasCodeSigningUsage) {
        throw "Certificate is not valid for code signing: $normalizedThumbprint"
    }
}

function Set-KtkAuthenticodeSignature([string]$TargetPath) {
    if (-not $signingCertificate) { return }
    $signature = Set-AuthenticodeSignature `
        -LiteralPath $TargetPath `
        -Certificate $signingCertificate `
        -HashAlgorithm SHA256 `
        -TimestampServer $TimestampServer
    if (-not $signature.SignerCertificate -or
        $signature.SignerCertificate.Thumbprint -ne $signingCertificate.Thumbprint) {
        throw "Authenticode signing failed for: $TargetPath ($($signature.StatusMessage))"
    }
    if (-not $signature.TimeStamperCertificate) {
        throw "Authenticode timestamp is missing for: $TargetPath"
    }
}

function Test-KtkAuthenticodeSignature([string]$TargetPath) {
    if (-not $signingCertificate) { return }
    $signature = Get-AuthenticodeSignature -LiteralPath $TargetPath
    if (-not $signature.SignerCertificate -or
        $signature.SignerCertificate.Thumbprint -ne $signingCertificate.Thumbprint) {
        throw "Unexpected Authenticode signer for: $TargetPath"
    }
    if (-not $signature.TimeStamperCertificate) {
        throw "Authenticode timestamp verification failed for: $TargetPath"
    }
}

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
if ($signingCertificate) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "docs\early-access-signature.md") `
        -Destination $docsDirectory
}

$licenseDirectory = Join-Path $stageRoot "licenses"
New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "third_party\QXlsx\LICENSE") `
    -Destination (Join-Path $licenseDirectory "QXlsx-MIT.txt")

function Ensure-LicenseFile(
    [string]$DestinationName,
    [string[]]$Candidates
) {
    $destination = Join-Path $licenseDirectory $DestinationName
    if (Test-Path -LiteralPath $destination) { return }
    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            Copy-Item -LiteralPath $candidate -Destination $destination
            return
        }
    }
    throw "Missing required third-party license: $DestinationName"
}

Ensure-LicenseFile "Qt-LGPL-3.0.txt" @(
    (Join-Path $projectRoot "third_party\licenses\Qt-LGPL-3.0.txt")
)

$toolchainRoots = @("C:\Qt\Tools\mingw1310_64")
$compilerCommand = Get-Command "g++.exe" -ErrorAction SilentlyContinue
if ($compilerCommand) {
    $compilerBin = Split-Path -Parent $compilerCommand.Source
    $toolchainRoots += Split-Path -Parent $compilerBin
}
$toolchainRoots = @($toolchainRoots | Select-Object -Unique)

function Get-ToolchainLicenseCandidates([string]$RelativePath) {
    @($toolchainRoots | ForEach-Object { Join-Path $_ $RelativePath })
}

Ensure-LicenseFile "GNU-GPL-3.0.txt" @(
    Get-ToolchainLicenseCandidates "licenses\gcc\COPYING3"
)
Ensure-LicenseFile "GCC-Runtime-Library-Exception.txt" @(
    Get-ToolchainLicenseCandidates "licenses\gcc\COPYING.RUNTIME"
)
Ensure-LicenseFile "MinGW-w64-runtime.txt" @(
    Get-ToolchainLicenseCandidates "licenses\mingw-w64\COPYING.MinGW-w64-runtime.txt"
)
Ensure-LicenseFile "winpthreads-COPYING.txt" @(
    Get-ToolchainLicenseCandidates "licenses\winpthreads\COPYING"
)

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

$publicCertificatePath = $null
$certificateInfoPath = $null
if ($signingCertificate) {
    $publicCertificatePath = Join-Path $releaseRoot `
        "KalkulatorTrasKablowych-$Version-early-access-code-signing.cer"
    $certificateInfoPath = Join-Path $releaseRoot `
        "KalkulatorTrasKablowych-$Version-early-access-code-signing.txt"
    Export-Certificate -Cert $signingCertificate -FilePath $publicCertificatePath -Force | Out-Null

    $certificateLines = @(
        "Kalkulator Tras Kablowych - Early Access self-signed code-signing certificate"
        "Subject: $($signingCertificate.Subject)"
        "Issuer: $($signingCertificate.Issuer)"
        "SHA-1 thumbprint: $($signingCertificate.Thumbprint)"
        "SHA-256 certificate fingerprint: $((Get-FileHash -Algorithm SHA256 -LiteralPath $publicCertificatePath).Hash)"
        "Valid from: $($signingCertificate.NotBefore.ToUniversalTime().ToString('u'))"
        "Valid until: $($signingCertificate.NotAfter.ToUniversalTime().ToString('u'))"
        "Trust scope: Early Access only; not issued by a public certification authority."
    )
    Set-Content -LiteralPath $certificateInfoPath -Value $certificateLines -Encoding utf8
    Copy-Item -LiteralPath $publicCertificatePath `
        -Destination (Join-Path $stageRoot "EarlyAccess-CodeSigning.cer")
    Copy-Item -LiteralPath $certificateInfoPath `
        -Destination (Join-Path $stageRoot "EarlyAccess-CodeSigning.txt")

    Set-KtkAuthenticodeSignature `
        (Join-Path $stageRoot "KalkulatorTrasKablowych.exe")
    Test-KtkAuthenticodeSignature `
        (Join-Path $stageRoot "KalkulatorTrasKablowych.exe")
}

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
    Set-KtkAuthenticodeSignature $installerPath
    Test-KtkAuthenticodeSignature $installerPath
    & (Join-Path $PSScriptRoot "test-windows-package.ps1") `
        -PackageDirectory $stageRoot -InstallerPath $installerPath
    if ($LASTEXITCODE -ne 0) { throw "Installer smoke test failed." }
    $artifacts += $installerPath
}

if ($publicCertificatePath) {
    $artifacts += $publicCertificatePath
    $artifacts += $certificateInfoPath
}

$hashPath = Join-Path $releaseRoot "SHA256SUMS-$Version.txt"
$hashLines = foreach ($artifact in $artifacts) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact).Hash
    "$hash  $([System.IO.Path]::GetFileName($artifact))"
}
Set-Content -LiteralPath $hashPath -Value $hashLines -Encoding ascii

$artifacts + $hashPath | ForEach-Object { Write-Output $_ }
