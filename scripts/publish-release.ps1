param(
    [Parameter(Mandatory=$true)][string]$Tag,
    [Parameter(Mandatory=$true)][string]$NotesFile,
    [string]$Repository = 'KubwojPrime/KalkulatorTrasKablowych',
    [string]$SignerThumbprint = 'C5BE5238453A8A9A93DB9AC6C755560147A00320',
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
if ($Tag -notmatch '^v(\d+\.\d+\.\d+)(-(rc|early-access)\.[1-9][0-9]*)?$') { throw 'Invalid release tag.' }
$version = $Matches[1]
$prerelease = $Tag.Contains('-')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$release = Join-Path $root 'release'
$hashFile = Join-Path $release "SHA256SUMS-$version.txt"
$expected = @("KalkulatorTrasKablowych-$version-win64.zip", "KalkulatorTrasKablowych-$version-win64-setup.exe",
    "KalkulatorTrasKablowych-$version-early-access-code-signing.cer", "KalkulatorTrasKablowych-$version-early-access-code-signing.txt")
$assets = @()
$seen = @()
foreach ($line in Get-Content -LiteralPath $hashFile) {
    if ($line -notmatch '^([A-Fa-f0-9]{64})  ([^/\\]+)$') { throw 'Invalid checksum manifest.' }
    $hash = $Matches[1]; $name = $Matches[2]
    if ($name -notin $expected -or $name -in $seen) { throw "Unexpected or duplicate asset: $name" }
    $path = Join-Path $release $name
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $name" }
    $assets += $path; $seen += $name
}
if ($seen.Count -ne $expected.Count) { throw 'Incomplete signed release package.' }
function Assert-Signed([string]$Path) {
    $s = Get-AuthenticodeSignature -LiteralPath $Path
    if (-not $s.SignerCertificate -or $s.SignerCertificate.Thumbprint -ne $SignerThumbprint -or
        -not $s.TimeStamperCertificate -or $s.Status -notin @('Valid','UnknownError','NotTrusted')) {
        throw "Missing, damaged or unexpected Authenticode signature: $Path ($($s.Status))"
    }
}
Assert-Signed (Join-Path $release "KalkulatorTrasKablowych-$version-win64-setup.exe")
$verify = Join-Path $release ('verify-publish-' + [guid]::NewGuid().ToString('N'))
try {
    Expand-Archive -LiteralPath (Join-Path $release "KalkulatorTrasKablowych-$version-win64.zip") -DestinationPath $verify
    Assert-Signed (Join-Path $verify 'KalkulatorTrasKablowych.exe')
    $info = Get-Content -Raw -LiteralPath (Join-Path $verify 'build-info.json') | ConvertFrom-Json
    if ($info.Version -ne $version -or $info.Dirty) { throw 'Package version mismatch or uncommitted source.' }
    $tagCommit = & git -c "safe.directory=$root" -C $root rev-list -n 1 $Tag
    if ($LASTEXITCODE -ne 0 -or $tagCommit -ne $info.Commit) { throw 'Package commit does not match tag.' }
    $runsJson = & gh run list --repo $Repository --branch $Tag --commit $tagCommit --limit 20 --json status,conclusion,workflowName
    if ($LASTEXITCODE -ne 0) { throw 'Cannot verify release CI.' }
    $runs = $runsJson | ConvertFrom-Json
    if (-not ($runs | Where-Object { $_.workflowName -eq 'Windows' -and $_.status -eq 'completed' -and $_.conclusion -eq 'success' })) {
        throw 'No successful Windows CI for this tag and commit.'
    }
} finally {
    $resolved = [IO.Path]::GetFullPath($verify)
    if ($resolved.StartsWith($release + [IO.Path]::DirectorySeparatorChar) -and (Test-Path -LiteralPath $resolved)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
if (-not (Test-Path -LiteralPath $NotesFile)) { throw 'Release notes missing.' }
if ($ValidateOnly) { Write-Output 'Signed release validation passed.'; return }
$arguments = @('release','create',$Tag,'--repo',$Repository,'--verify-tag','--draft','--title',"Kalkulator Tras Kablowych $Tag",'--notes-file',$NotesFile)
if ($prerelease) { $arguments += '--prerelease' }
& gh @arguments @assets $hashFile
if ($LASTEXITCODE -ne 0) { throw 'Draft release creation failed.' }
& gh release edit $Tag --repo $Repository --draft=false
if ($LASTEXITCODE -ne 0) { throw 'Release publication failed; inspect existing draft.' }
