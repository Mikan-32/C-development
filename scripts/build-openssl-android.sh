#!/usr/bin/env bash
# Linux/macOS counterpart to the Windows/MSYS2 builder. Produces the same layout.
set -euo pipefail
repo=$(cd -- "$(dirname -- "$0")/.." && pwd)
ndk=$(cd -- "${1:?Specify the Android NDK directory}" && pwd)
abi=${2:-arm64-v8a}
jobs=${NK_BUILD_JOBS:-4}
case "$abi" in
    arm64-v8a) target=android-arm64; triple=aarch64-linux-android26 ;;
    x86_64) target=android-x86_64; triple=x86_64-linux-android26 ;;
    *) echo "Unsupported ABI: $abi" >&2; exit 1 ;;
esac
case "$(uname -s)" in
    Linux) host=linux-x86_64 ;;
    Darwin) host=darwin-x86_64 ;;
    *) echo 'Use build-openssl-android.ps1 on Windows' >&2; exit 1 ;;
esac
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["openssl"]["version"])' "$repo/cmake/dependencies.json")
python3 "$repo/scripts/fetch-openssl-source.py"
work="$repo/build/deps/openssl-$version-android-$abi-posix"
prefix="$repo/third_party/prebuilt/openssl/$version/android-$abi/release"
mkdir -p "$work/source" "$work/build" "$prefix"
rm -f "$prefix/build-info.json"
tar -xzf "$repo/build/deps/openssl-$version.tar.gz" -C "$work/source" --strip-components=1
export ANDROID_NDK_ROOT="$ndk"
export PATH="$ndk/toolchains/llvm/prebuilt/$host/bin:$PATH"
unset CROSS_SYSROOT CROSS_COMPILE CC CXX AR RANLIB CFLAGS CPPFLAGS LDFLAGS
cd "$work/build"
perl "$work/source/Configure" "$target" -D__ANDROID_API__=26 \
    no-shared no-module no-tests no-asm -fPIC \
    "--prefix=$prefix" "--openssldir=$prefix/ssl" --libdir=lib
make -j"$jobs" "CC=clang --target=$triple" AR=llvm-ar RANLIB=llvm-ranlib build_libs
make "CC=clang --target=$triple" AR=llvm-ar RANLIB=llvm-ranlib install_dev
cp "$work/source/LICENSE.txt" "$prefix/LICENSE.txt"
python3 - "$repo/cmake/dependencies.json" "$ndk/source.properties" "$prefix" "$abi" <<'PY'
import hashlib
import json
from pathlib import Path
import re
import sys
dep = json.loads(Path(sys.argv[1]).read_text())["openssl"]
ndk = re.search(r"Pkg.Revision\s*=\s*(\S+)", Path(sys.argv[2]).read_text()).group(1)
if int(ndk.split(".")[0]) < 28:
    raise RuntimeError("NDK r28 or newer is required")
prefix = Path(sys.argv[3])
info = dict(schema_version=1, openssl_version=dep["version"], source_sha256=dep["source_sha256"],
            configuration="release", architecture=sys.argv[4], static=True, assembly=False,
            android_api=26, ndk_version=ndk)
for name in ("crypto", "ssl"):
    info[name + "_sha256"] = hashlib.sha256((prefix / ("lib/lib" + name + ".a")).read_bytes()).hexdigest()
(prefix / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")
PY
echo "Android OpenSSL prepared: $prefix"
