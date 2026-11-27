[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$NdkRoot,
    [Parameter(Mandatory = $true)][string]$VcpkgRoot,
    [ValidateSet("arm64-v8a", "x86_64")][string]$Abi = "arm64-v8a",
    [ValidateRange(1, 64)][int]$Jobs = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Write-Host "NK Android OpenSSL builder v2"

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $Program"
    }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$NdkRoot = (Resolve-Path -LiteralPath $NdkRoot).Path
$VcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
if (($repo + $NdkRoot) -match '\s') {
    throw "OpenSSL build paths must not contain spaces."
}

$deps = Get-Content (Join-Path $repo "cmake/dependencies.json") -Raw |
    ConvertFrom-Json
$version = [string]$deps.openssl.version
$sourceHash = [string]$deps.openssl.source_sha256
if ($version -notmatch '^\d+\.\d+\.\d+$' -or
    $sourceHash -notmatch '^[0-9a-fA-F]{64}$') {
    throw "Invalid OpenSSL version or SHA-256 in dependencies.json."
}

$toolBin = Join-Path $NdkRoot "toolchains/llvm/prebuilt/windows-x86_64/bin"
if (-not (Test-Path (Join-Path $toolBin "clang.exe"))) {
    throw "The Windows Android NDK was not found: $toolBin"
}
$properties = Get-Content (Join-Path $NdkRoot "source.properties") -Raw
if ($properties -notmatch '(?m)^Pkg\.Revision\s*=\s*([0-9.]+)\s*$') {
    throw "Cannot read the NDK version."
}
$ndkVersion = $Matches[1]

$msysBase = Join-Path $VcpkgRoot "downloads/tools/msys2"
$msys = Get-ChildItem -LiteralPath $msysBase -Directory |
    Where-Object {
        (Test-Path (Join-Path $_.FullName "usr/bin/bash.exe")) -and
        (Test-Path (Join-Path $_.FullName "usr/bin/perl.exe")) -and
        (Test-Path (Join-Path $_.FullName "usr/bin/make.exe")) -and
        (Test-Path (Join-Path $_.FullName "usr/bin/cygpath.exe"))
    } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($null -eq $msys) {
    throw "MSYS2 with bash, perl and make is missing from the vcpkg tools."
}
$msysBin = Join-Path $msys.FullName "usr/bin"

Write-Host "[1/5] Verify OpenSSL source"
Invoke-Checked -Program "powershell.exe" -Arguments @(
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
    (Join-Path $PSScriptRoot "fetch-openssl-source.ps1")
)
$archive = Join-Path $repo "build/deps/openssl-$version.tar.gz"
if ((Get-FileHash $archive -Algorithm SHA256).Hash -ine $sourceHash) {
    throw "OpenSSL archive SHA-256 mismatch."
}

$work = Join-Path $repo "build/deps/openssl-$version-android-$Abi-api26-ndk$ndkVersion"
$source = Join-Path $work "source"
$build = Join-Path $work "build"
$prefix = Join-Path $repo "third_party/prebuilt/openssl/$version/android-$Abi/release"
foreach ($directory in @($source, $build, $prefix)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}
$infoPath = Join-Path $prefix "build-info.json"
if (Test-Path $infoPath) {
    Remove-Item -LiteralPath $infoPath
}

$tar = (Get-Command tar.exe -CommandType Application).Source
Write-Host "[2/5] Extract source"
Invoke-Checked -Program $tar -Arguments @(
    "-xzf", $archive, "-C", $source, "--strip-components=1"
)

# Keep MSYS2 archive commands below the Windows command-line limit.
$templatePath = Join-Path $source "Configurations/unix-Makefile.tmpl"
$template = [IO.File]::ReadAllText($templatePath)
$oldBatch = 'my $max_per_call = 500;'
if (-not $template.Contains($oldBatch)) {
    throw "Unsupported OpenSSL archive rule. Review the MSYS2 build adjustment."
}
[IO.File]::WriteAllText(
    $templatePath, $template.Replace($oldBatch, 'my $max_per_call = 80;'),
    (New-Object Text.UTF8Encoding($false))
)

$bashText = @'
set -euo pipefail
source_dir=$(cygpath -u "$NK_OSSL_SOURCE")
build_dir=$(cygpath -u "$NK_OSSL_BUILD")
prefix=$(cygpath -u "$NK_OSSL_PREFIX")
export ANDROID_NDK_ROOT=$(cygpath -u "$NK_OSSL_NDK")
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/windows-x86_64/bin:/usr/bin:$PATH"
unset CROSS_SYSROOT CROSS_COMPILE CC CXX AR RANLIB CFLAGS CPPFLAGS LDFLAGS
perl_bin=/usr/bin/perl.exe
"$perl_bin" -e 'use IPC::Cmd; print "Perl executable: $^X\nPerl platform: $^O\n"; die "MSYS2/Cygwin Perl is required\n" unless $^O =~ /^(msys|cygwin)$/;'

case "$NK_OSSL_ABI" in
    arm64-v8a) target=android-arm64; triple=aarch64-linux-android26 ;;
    x86_64) target=android-x86_64; triple=x86_64-linux-android26 ;;
    *) exit 2 ;;
esac

cd "$build_dir"
echo '[3/5] Configure Android OpenSSL'
"$perl_bin" "$source_dir/Configure" "$target" -D__ANDROID_API__=26 \
    no-shared no-module no-tests no-asm -fPIC \
    "--prefix=$prefix" "--openssldir=$prefix/ssl" --libdir=lib

# Use clang directly; the NDK's target-specific wrappers are .cmd files.
echo '[4/5] Build and install libraries'
make -j"$NK_OSSL_JOBS" "CC=clang --target=$triple" \
    AR=llvm-ar RANLIB=llvm-ranlib build_libs
make "CC=clang --target=$triple" AR=llvm-ar RANLIB=llvm-ranlib install_dev

cat > check-openssl.c <<'C'
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

int main(void) {
    unsigned char bytes[32];
    SSL_CTX *context;
    if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
        return 1;
    }
    context = SSL_CTX_new(TLS_method());
    if (context == NULL) {
        return 2;
    }
    SSL_CTX_free(context);
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        return 3;
    }
    printf("%s\nSSL_CTX: OK\nRAND_bytes: OK\n",
           OpenSSL_version(OPENSSL_VERSION));
    return 0;
}
C

echo '[5/5] Link Android check program'
clang --target="$triple" -fPIE -pie -Wl,-z,max-page-size=16384 \
    -I"$prefix/include" check-openssl.c \
    "$prefix/lib/libssl.a" "$prefix/lib/libcrypto.a" \
    -ldl -pthread -lm -o openssl_android_check
mkdir -p "$prefix/tools"
cp openssl_android_check "$prefix/tools/openssl_android_check"
cp "$source_dir/LICENSE.txt" "$prefix/LICENSE.txt"
'@

$bashFile = Join-Path $work "build-openssl.sh"
[IO.File]::WriteAllText(
    $bashFile, ($bashText -replace "`r`n", "`n"),
    (New-Object Text.UTF8Encoding($false))
)
$environment = @{
    PATH = "$msysBin;$env:PATH"
    NK_OSSL_SOURCE = $source
    NK_OSSL_BUILD = $build
    NK_OSSL_PREFIX = $prefix
    NK_OSSL_NDK = $NdkRoot
    NK_OSSL_ABI = $Abi
    NK_OSSL_JOBS = [string]$Jobs
}
$saved = @{}
foreach ($name in $environment.Keys) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
}
try {
    foreach ($name in $environment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $environment[$name], "Process")
    }
    $posixScript = & (Join-Path $msysBin "cygpath.exe") -u $bashFile
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot convert the build script path for MSYS2."
    }
    Invoke-Checked -Program (Join-Path $msysBin "bash.exe") -Arguments @(
        "--noprofile", "--norc", [string]$posixScript
    )
}
finally {
    foreach ($name in $environment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], "Process")
    }
}

$crypto = Join-Path $prefix "lib/libcrypto.a"
$ssl = Join-Path $prefix "lib/libssl.a"
$info = [ordered]@{
    schema_version = 1
    openssl_version = $version
    source_sha256 = $sourceHash.ToLowerInvariant()
    configuration = "release"
    architecture = $Abi
    static = $true
    assembly = $false
    msys_ar_batch_size = 80
    android_api = 26
    ndk_version = $ndkVersion
    crypto_sha256 = (Get-FileHash $crypto -Algorithm SHA256).Hash.ToLowerInvariant()
    ssl_sha256 = (Get-FileHash $ssl -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText(
    $infoPath, ($info | ConvertTo-Json), (New-Object Text.UTF8Encoding($false))
)
Write-Host "Android OpenSSL build and link check passed: $prefix"
Write-Host "Run tools/openssl_android_check on the Android device to verify execution."
# END NK_ANDROID_OPENSSL_BUILD
