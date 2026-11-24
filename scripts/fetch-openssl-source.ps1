$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $repositoryRoot "cmake\dependencies.json"

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 |
    ConvertFrom-Json

$opensslVersion = [string]$manifest.openssl.version
$sourceUrl = [string]$manifest.openssl.source_url
$expectedHash = [string]$manifest.openssl.source_sha256

if ($opensslVersion -notmatch '^3\.5\.\d+$') {
    throw "Expected an OpenSSL 3.5.x version."
}

if ($expectedHash -notmatch '^[0-9a-fA-F]{64}$') {
    throw "Invalid SHA-256 in dependencies.json."
}

$downloadDirectory = Join-Path $repositoryRoot "build\deps"

New-Item -ItemType Directory -Path $downloadDirectory -Force |
    Out-Null

$archivePath = Join-Path $downloadDirectory (
    "openssl-" + $opensslVersion + ".tar.gz"
)

if (Test-Path -LiteralPath $archivePath -PathType Leaf) {
    $actualHash = (
        Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
    ).Hash

    if ($actualHash -ne $expectedHash) {
        throw "Cached archive SHA-256 mismatch: $archivePath"
    }
}
else {
    $partialPath = Join-Path $downloadDirectory (
        "openssl-download-" + [Guid]::NewGuid().ToString("N") + ".partial"
    )

    try {
        [Net.ServicePointManager]::SecurityProtocol =
            [Net.SecurityProtocolType]::Tls12

        Write-Host "Downloading OpenSSL $opensslVersion ..."

        Invoke-WebRequest `
            -Uri $sourceUrl `
            -OutFile $partialPath `
            -UseBasicParsing

        $actualHash = (
            Get-FileHash -LiteralPath $partialPath -Algorithm SHA256
        ).Hash

        if ($actualHash -ne $expectedHash) {
            throw "Downloaded archive SHA-256 mismatch."
        }

        Move-Item -LiteralPath $partialPath -Destination $archivePath
    }
    finally {
        # Remove only this script's temporary download.
        if (Test-Path -LiteralPath $partialPath) {
            Remove-Item -LiteralPath $partialPath -Force
        }
    }
}

Write-Host "Verified OpenSSL $opensslVersion"
Write-Host "Archive: $archivePath"
Write-Host "SHA-256: $expectedHash"
