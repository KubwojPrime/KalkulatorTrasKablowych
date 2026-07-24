param(
    [string]$ManifestPath = "source-materials\manifest.json"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$manifestFullPath = if ([System.IO.Path]::IsPathRooted($ManifestPath)) {
    [System.IO.Path]::GetFullPath($ManifestPath)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $projectRoot $ManifestPath))
}

if (-not $manifestFullPath.StartsWith(
        $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Manifest musi znajdowac sie w katalogu projektu."
}

if (-not (Test-Path -LiteralPath $manifestFullPath -PathType Leaf)) {
    throw "Nie znaleziono manifestu: $manifestFullPath"
}

$manifest = Get-Content -Raw -LiteralPath $manifestFullPath | ConvertFrom-Json
$failures = [System.Collections.Generic.List[string]]::new()
$verified = [System.Collections.Generic.List[object]]::new()
$totalBytes = [int64]0

foreach ($source in $manifest.sources) {
    $relativePath = [string]$source.file
    $targetPath = [System.IO.Path]::GetFullPath(
        (Join-Path $projectRoot ($relativePath -replace "/", "\")))

    if (-not $targetPath.StartsWith(
            $projectRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        $failures.Add("${relativePath}: sciezka wychodzi poza katalog projektu.")
        continue
    }

    if (-not (Test-Path -LiteralPath $targetPath -PathType Leaf)) {
        $failures.Add("${relativePath}: brak pliku.")
        continue
    }

    $file = Get-Item -LiteralPath $targetPath
    $bytes = [System.IO.File]::ReadAllBytes($targetPath)
    $signature = if ($bytes.Length -ge 5) {
        [System.Text.Encoding]::ASCII.GetString($bytes, 0, 5)
    } else {
        ""
    }
    $hash = (Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash.ToLowerInvariant()

    $contentType = [string]$source.contentType
    if ($contentType -eq "application/pdf" -and $signature -ne "%PDF-") {
        $failures.Add("${relativePath}: nieprawidlowa sygnatura PDF.")
    } elseif ($contentType -eq "text/html") {
        $prefixLength = [Math]::Min(256, $bytes.Length)
        $prefix = [System.Text.Encoding]::UTF8.GetString(
            $bytes,
            0,
            $prefixLength).ToLowerInvariant()
        if (-not $prefix.Contains("<!doctype html") -and
            -not $prefix.Contains("<html")) {
            $failures.Add("${relativePath}: nieprawidlowa sygnatura HTML.")
        }
    } elseif ($contentType -ne "application/pdf") {
        $failures.Add("${relativePath}: nieobslugiwany typ $contentType.")
    }
    if ($file.Length -ne [int64]$source.bytes) {
        $failures.Add(
            "${relativePath}: rozmiar $($file.Length), oczekiwano $($source.bytes).")
    }
    if ($hash -ne ([string]$source.sha256).ToLowerInvariant()) {
        $failures.Add("${relativePath}: skrot SHA-256 nie zgadza sie z manifestem.")
    }

    $totalBytes += $file.Length
    $verified.Add([pscustomobject]@{
        Manufacturer = [string]$source.manufacturer
        File = $relativePath
        Bytes = $file.Length
        SHA256 = $hash
    })
}

if ($verified.Count -ne [int]$manifest.sourceCount) {
    $failures.Add(
        "Zweryfikowano $($verified.Count) plikow, manifest deklaruje $($manifest.sourceCount).")
}
if ($totalBytes -ne [int64]$manifest.totalBytes) {
    $failures.Add(
        "Laczny rozmiar $totalBytes, manifest deklaruje $($manifest.totalBytes).")
}

$verified | Format-Table Manufacturer, File, Bytes -AutoSize

if ($failures.Count -gt 0) {
    $failureMessage = "Weryfikacja materialow zrodlowych nie powiodla sie:`n- " `
        + ($failures -join "`n- ")
    Write-Error $failureMessage
    exit 1
}

Write-Output ""
$successMessage = "Zweryfikowano {0} plikow zrodlowych, {1} bajtow. " `
    + "Wszystkie skroty SHA-256 sa zgodne."
Write-Output ($successMessage -f $verified.Count, $totalBytes)
