param(
    [Parameter(Mandatory = $true)]
    [string]$PerlPath,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedNative {
    param(
        [string]$Executable,
        [string[]]$Arguments = @()
    )

    & $Executable @Arguments

    if ($LASTEXITCODE -ne 0) {
        throw (
            "Command failed ({0}): {1}" -f
            $LASTEXITCODE, $Executable
        )
    }
}

if ($env:OS -ne "Windows_NT") {
    throw "This script supports Windows only."
}

if ($env:VSCMD_ARG_TGT_ARCH -notin @("x64", "amd64")) {
    throw "Use the x64 Visual Studio Developer Command Prompt."
}

$clExecutable = (
    Get-Command cl.exe -TotalCount 1 -CommandType Application -ErrorAction Stop
).Source

$nmakeExecutable = (
    Get-Command nmake.exe -TotalCount 1 -CommandType Application -ErrorAction Stop
).Source

$tarExecutable = (
    Get-Command tar.exe -TotalCount 1 -CommandType Application -ErrorAction Stop
).Source

$perlExecutable = (
    Resolve-Path -LiteralPath $PerlPath -ErrorAction Stop
).Path

$perlPlatform = (
    & $perlExecutable -e 'print $^O'
) -join ""

if ($LASTEXITCODE -ne 0 -or $perlPlatform -ne "MSWin32") {
    throw "Specify a native Windows Perl executable."
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "fetch-openssl-source.ps1")

$manifest = Get-Content -LiteralPath (
    Join-Path $repositoryRoot "cmake\dependencies.json"
) -Raw -Encoding UTF8 | ConvertFrom-Json

$opensslVersion = [string]$manifest.openssl.version

if ($opensslVersion -notmatch '^3\.5\.(\d+)$') {
    throw "Expected an OpenSSL 3.5.x version."
}

$expectedPatch = [int]$Matches[1]
$configurationName = $Configuration.ToLowerInvariant()

$runtimeFlag = if ($Configuration -eq "Debug") {
    "-MDd"
}
else {
    "-MD"
}

$archivePath = Join-Path $repositoryRoot (
    "build\deps\openssl-" + $opensslVersion + ".tar.gz"
)

$workDirectory = Join-Path $repositoryRoot (
    "build\deps\openssl-" + $opensslVersion +
    "-windows-x64-" + $configurationName
)

$sourceParent = Join-Path $workDirectory "source"
$sourceDirectory = Join-Path $sourceParent (
    "openssl-" + $opensslVersion
)

$buildDirectory = Join-Path $workDirectory "build"

$installDirectory = Join-Path $repositoryRoot (
    "third_party\prebuilt\openssl\" + $opensslVersion +
    "\windows-x64\" + $configurationName
)

foreach ($directory in @(
        $sourceParent,
        $buildDirectory,
        $installDirectory
    )) {
    New-Item -ItemType Directory -Path $directory -Force |
    Out-Null
}

Write-Host "Extracting OpenSSL $opensslVersion ..."

Invoke-CheckedNative -Executable $tarExecutable -Arguments @(
    "-xzf", $archivePath,
    "-C", $sourceParent
)

$configurePath = Join-Path $sourceDirectory "Configure"

if (-not (Test-Path -LiteralPath $configurePath -PathType Leaf)) {
    throw "OpenSSL Configure was not found after extraction."
}

$installForward = $installDirectory.Replace('\', '/')

$configureArguments = @(
    $configurePath,
    "VC-WIN64A",
    "no-shared",
    "no-module",
    "no-tests",
    "no-asm",
    "--prefix=$installForward",
    "--openssldir=$installForward/ssl",
    "--libdir=lib",
    $runtimeFlag
)

if ($Configuration -eq "Debug") {
    $configureArguments += "--debug"
}
else {
    $configureArguments += "--release"
}

$previousPath = $env:PATH
$locationPushed = $false

try {
    $perlDirectory = Split-Path -Parent $perlExecutable
    $env:PATH = "$perlDirectory;$previousPath"

    Push-Location -LiteralPath $buildDirectory
    $locationPushed = $true

    Invoke-CheckedNative `
        -Executable $perlExecutable `
        -Arguments $configureArguments

    Invoke-CheckedNative -Executable $nmakeExecutable -Arguments @(
        "/NOLOGO", "build_libs"
    )

    Invoke-CheckedNative -Executable $nmakeExecutable -Arguments @(
        "/NOLOGO", "install_dev"
    )

    $cryptoLibrary = Join-Path $installDirectory "lib\libcrypto.lib"
    $sslLibrary = Join-Path $installDirectory "lib\libssl.lib"
    $includeDirectory = Join-Path $installDirectory "include"

    foreach ($requiredFile in @(
            $cryptoLibrary,
            $sslLibrary,
            (Join-Path $includeDirectory "openssl\opensslv.h")
        )) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Required file was not produced: $requiredFile"
        }
    }

    $smokeSourcePath = Join-Path $buildDirectory "openssl-smoke.c"
    $smokeExecutable = Join-Path $buildDirectory "openssl-smoke.exe"
    $smokeObject = Join-Path $buildDirectory "openssl-smoke.obj"

    $smokeSource = @'
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifndef _M_X64
#error An x64 compiler is required.
#endif

int main(void)
{
    unsigned char bytes[32];

    if (OPENSSL_VERSION_MAJOR != 3
        || OPENSSL_VERSION_MINOR != 5
        || OPENSSL_VERSION_PATCH != NK_EXPECTED_PATCH) {
        return 1;
    }

    if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
        return 2;
    }

    if (RAND_bytes(bytes, (int)sizeof(bytes)) != 1) {
        ERR_print_errors_fp(stderr);
        return 3;
    }

    return puts(OpenSSL_version(OPENSSL_VERSION)) < 0 ? 4 : 0;
}
'@

    Set-Content `
        -LiteralPath $smokeSourcePath `
        -Value $smokeSource `
        -Encoding ASCII

    Invoke-CheckedNative -Executable $clExecutable -Arguments @(
        "/nologo",
        "/TC",
        $runtimeFlag,
        "/DNK_EXPECTED_PATCH=$expectedPatch",
        "/I$includeDirectory",
        $smokeSourcePath,
        "/Fo$smokeObject",
        "/Fe$smokeExecutable",
        "/link",
        $cryptoLibrary,
        "ws2_32.lib",
        "gdi32.lib",
        "advapi32.lib",
        "crypt32.lib",
        "user32.lib"
    )

    Invoke-CheckedNative -Executable $smokeExecutable

    Copy-Item -LiteralPath (
        Join-Path $sourceDirectory "LICENSE.txt"
    ) -Destination (
        Join-Path $installDirectory "LICENSE.txt"
    ) -Force

    $buildInfo = [ordered]@{
        schema_version  = 1
        openssl_version = $opensslVersion
        source_sha256   = [string]$manifest.openssl.source_sha256
        configuration   = $configurationName
        architecture    = "x64"
        runtime         = $runtimeFlag.Substring(1)
        static          = $true
        assembly        = $false
        crypto_sha256   = (
            Get-FileHash -LiteralPath $cryptoLibrary -Algorithm SHA256
        ).Hash.ToLowerInvariant()
        ssl_sha256      = (
            Get-FileHash -LiteralPath $sslLibrary -Algorithm SHA256
        ).Hash.ToLowerInvariant()
    }

    $buildInfo | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (
        Join-Path $installDirectory "build-info.json"
    ) -Encoding ASCII
}
finally {
    if ($locationPushed) {
        Pop-Location
    }

    $env:PATH = $previousPath
}

Write-Host "OpenSSL prepared: $opensslVersion $Configuration"
Write-Host "Install directory: $installDirectory"
