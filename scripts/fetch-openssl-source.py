#!/usr/bin/env python3
"""Fetch the pinned OpenSSL archive without changing generated libraries."""
import hashlib
import json
from pathlib import Path
import urllib.request


def main():
    repo = Path(__file__).resolve().parent.parent
    dep = json.loads((repo / "cmake/dependencies.json").read_text())["openssl"]
    archive = repo / "build/deps" / ("openssl-" + dep["version"] + ".tar.gz")
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        temporary = archive.with_suffix(".download")
        try:
            with urllib.request.urlopen(dep["source_url"], timeout=120) as response:
                with temporary.open("wb") as output:
                    while chunk := response.read(1024 * 1024):
                        output.write(chunk)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != dep["source_sha256"]:
                raise RuntimeError("OpenSSL source SHA-256 mismatch")
            temporary.replace(archive)
        finally:
            temporary.unlink(missing_ok=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != dep["source_sha256"]:
        raise RuntimeError("OpenSSL archive SHA-256 mismatch")
    print("Verified OpenSSL " + dep["version"])
    print(archive)


if __name__ == "__main__":
    main()
