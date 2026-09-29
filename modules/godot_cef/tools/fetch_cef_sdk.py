#!/usr/bin/env python3
"""Download the CEF binary distribution used by the godot_cef module.

Usage:
    python modules/godot_cef/tools/fetch_cef_sdk.py [--platform macosarm64|macosx64|windows64|all] [--dest DIR]

By default the SDK for the host platform is installed into the Godot build
dependencies folder (`bin/build_deps/cef/<platform>`, or
`%LOCALAPPDATA%/Godot/build_deps/cef/<platform>` on Windows), which is where
the module looks for it when `cef_sdk_path` is not given to SCons.
"""

if __name__ != "__main__":
    raise SystemExit(f'Utility script "{__file__}" should not be used as a module!')

import argparse
import hashlib
import os
import platform
import shutil
import sys
import tarfile
import urllib.request

CEF_VERSION = "148.0.10+g7ee53f5+chromium-148.0.7778.218"
CEF_CDN = "https://cef-builds.spotifycdn.com"

# SHA-1 of the "minimal" distributions, from https://cef-builds.spotifycdn.com/index.json.
CEF_SHA1 = {
    "macosarm64": "f071b9ce7629434c5315d283e8ff1834503fc740",
    "macosx64": "56594d7af160124dcea406cda2c8a8b98b49b7f5",
    "windows64": "2974c0c195343a1452c59a2687583eaceb98839b",
}

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../.."))


def default_deps_folder():
    local_app_data = os.getenv("LOCALAPPDATA")
    if local_app_data:
        return os.path.join(local_app_data, "Godot", "build_deps", "cef")
    return os.path.join(REPO_ROOT, "bin", "build_deps", "cef")


def host_platform():
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Darwin":
        return "macosarm64" if machine in ("arm64", "aarch64") else "macosx64"
    if system == "Windows":
        return "windows64"
    raise SystemExit(f"Unsupported host platform for godot_cef: {system} {machine}")


def sha1_of(path):
    digest = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url, dest):
    print(f"Downloading {url} ...")

    def report(blocks, block_size, total):
        if total > 0:
            done = min(blocks * block_size, total)
            sys.stdout.write(f"\r  {done * 100 // total:3d}% ({done // (1 << 20)} / {total // (1 << 20)} MiB)")
            sys.stdout.flush()

    urllib.request.urlretrieve(url, dest, report)
    sys.stdout.write("\n")


def install(cef_platform, deps_folder):
    name = f"cef_binary_{CEF_VERSION}_{cef_platform}_minimal"
    archive = os.path.join(deps_folder, name + ".tar.bz2")
    target = os.path.join(deps_folder, cef_platform)

    os.makedirs(deps_folder, exist_ok=True)
    if not os.path.isfile(archive) or sha1_of(archive) != CEF_SHA1[cef_platform]:
        download(f"{CEF_CDN}/{urllib.request.quote(name)}.tar.bz2", archive)
    actual = sha1_of(archive)
    if actual != CEF_SHA1[cef_platform]:
        os.remove(archive)
        raise SystemExit(f"Checksum mismatch for {archive}: expected {CEF_SHA1[cef_platform]}, got {actual}")

    if os.path.exists(target):
        print(f"Removing existing CEF SDK in {target} ...")
        shutil.rmtree(target)
    print(f"Extracting to {target} ...")
    with tarfile.open(archive, "r:bz2") as tar:
        tar.extractall(deps_folder, filter="tar") if hasattr(tarfile, "tar_filter") else tar.extractall(deps_folder)
    os.rename(os.path.join(deps_folder, name), target)
    os.remove(archive)

    with open(os.path.join(target, "godot_cef_version.txt"), "w", encoding="utf-8") as f:
        f.write(CEF_VERSION + "\n")
    print(f"CEF SDK {CEF_VERSION} ({cef_platform}) installed to {target}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", default=None, choices=sorted(CEF_SHA1) + ["all"])
    parser.add_argument("--dest", default=default_deps_folder())
    args = parser.parse_args()

    platforms = sorted(CEF_SHA1) if args.platform == "all" else [args.platform or host_platform()]
    for cef_platform in platforms:
        install(cef_platform, os.path.abspath(args.dest))


main()
