#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 dspaudio
"""Build MC-1 product and installer Pages from the pinned release package."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from urllib.request import Request, urlopen


def fetch(url: str, maximum: int) -> bytes:
    request = Request(url, headers={"User-Agent": "mc-1-web-release-builder"})
    with urlopen(request, timeout=60) as response:
        data = response.read(maximum + 1)
    if len(data) > maximum:
        raise ValueError("Release response exceeds its size limit")
    return data


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=root / "FIRMWARE.json")
    parser.add_argument("--package", type=Path, help="Use a local copy of the pinned release")
    parser.add_argument("--out", type=Path, default=root / "build/site")
    args = parser.parse_args()
    info = json.loads(args.manifest.read_text())
    version = info["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version) or version != (root / "VERSION").read_text().strip():
        raise ValueError("Invalid release version")
    expected = f"mc-1-{version}.fwsc"
    if (info["identity"] != "FM-1_900" or info["file"] != expected or
            not re.fullmatch(r"[a-f0-9]{64}", info["sha256"]) or
            type(info["bytes"]) is not int or not 960 <= info["bytes"] <= 1048576):
        raise ValueError("Invalid pinned release manifest")
    if args.package is not None:
        data = args.package.read_bytes()
    else:
        release = json.loads(fetch(
            f"https://api.github.com/repos/dspaudio/mc-1/releases/tags/v{version}", 1048576))
        if release["draft"] or release["tag_name"] != f"v{version}":
            raise ValueError("Release is not published under the pinned tag")
        assets = [asset for asset in release["assets"] if asset["name"] == expected]
        if len(assets) != 1 or assets[0]["size"] != info["bytes"]:
            raise ValueError("Missing or mismatched release asset")
        url = assets[0]["browser_download_url"]
        if url != f"https://github.com/dspaudio/mc-1/releases/download/v{version}/{expected}":
            raise ValueError("Unexpected release asset URL")
        data = fetch(url, info["bytes"])
    if len(data) != info["bytes"] or hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise ValueError("Release asset SHA256 or size mismatch")
    subprocess.run([
        "node", "--input-type=module", "-e",
        """import {loadPackage} from './site/firmware-ota.js';
const chunks = [];
for await (const chunk of process.stdin) chunks.push(chunk);
const pkg = await loadPackage(Buffer.concat(chunks), {sha256: process.argv[1]});
if (pkg.product !== process.argv[2]) throw new Error('Release identity mismatch');
""", info["sha256"], info["identity"],
    ], input=data, cwd=root, check=True)
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "styles.css", "install.html", "firmware-install.js", "firmware-ota.js"):
        shutil.copyfile(root / "site" / name, out / name)
    shutil.copytree(root / "site/assets", out / "assets", dirs_exist_ok=True)
    shutil.copytree(root / "LICENSES", out / "LICENSES", dirs_exist_ok=True)
    (out / "audio").mkdir(exist_ok=True)
    (out / "licenses").mkdir(exist_ok=True)
    shutil.copyfile(root / "docs/audio/mc-1-neon-spiral-fx.mp3", out / "audio/mc-1-neon-spiral-fx.mp3")
    shutil.copyfile(root / "LICENSES/CC0-VSCO.txt", out / "licenses/CC0-VSCO.txt")
    shutil.copyfile(root / "assets/recorded/manifest.json", out / "recorded-samples.json")
    shutil.copyfile(root / "LICENSE", out / "LICENSE.txt")
    shutil.copyfile(root / "NOTICE.md", out / "NOTICE.md")
    (out / expected).write_bytes(data)
    (out / "firmware-release.json").write_text(json.dumps(info, indent=2) + "\n")
    (out / ".nojekyll").touch()
    sums = "".join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(out).as_posix()}\n"
        for path in sorted(out.rglob("*")) if path.is_file() and path.name != "SHA256SUMS"
    )
    (out / "SHA256SUMS").write_text(sums)
    print(f"PRODUCT_PAGES_VERIFIED v{version} {info['sha256']}", flush=True)


if __name__ == "__main__":
    main()
