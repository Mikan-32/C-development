#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "$0")/.." && pwd)
pin=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["sqlcipher"]["commit"])' "$repo/cmake/dependencies.json")
actual=$(git -C "$repo/third_party/sqlcipher" rev-parse HEAD)
if [[ "$actual" != "$pin" ]]; then
    echo 'SQLCipher submodule does not match cmake/dependencies.json' >&2
    exit 1
fi
git -C "$repo/third_party/sqlcipher" diff --quiet HEAD --
work="$repo/build/deps/sqlcipher-$pin"
output="$repo/third_party/prebuilt/sqlcipher"
mkdir -p "$work" "$output"
cd "$work"
"$repo/third_party/sqlcipher/configure" --with-tempstore=yes --disable-shared --disable-readline
make -j2 sqlite3.c sqlite3.h
cp sqlite3.c sqlite3.h "$repo/third_party/sqlcipher/LICENSE.md" "$output/"
printf '%s\n' "$pin" > "$output/source-commit.txt"
echo "SQLCipher amalgamation prepared: $output"
