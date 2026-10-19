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


ORIGINAL = {
    "version": "1.0.2", "core_mode": "single", "identity": "FM-1_900",
    "file": "mc-1-1.0.2.fwsc", "bytes": 610089,
    "sha256": "a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5",
}


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
    parser.add_argument("--alternative-package", type=Path, help="Use the original pinned 1.0.2 package offline")
    parser.add_argument("--out", type=Path, default=root / "build/site")
    args = parser.parse_args()
    info = json.loads(args.manifest.read_text())
    version = info["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version) or version != (root / "VERSION").read_text().strip():
        raise ValueError("Invalid release version")
    alternatives = info.get("alternatives", [])
    if (not isinstance(alternatives, list) or len(alternatives) > 1 or
            ("alternatives" in info and (len(alternatives) != 1 or info.get("core_mode") != "dual")) or
            any(record != ORIGINAL for record in alternatives) or
            (args.alternative_package is not None and not alternatives)):
        raise ValueError("Invalid pinned alternatives")
    records = [info, *alternatives]
    if (len({record["version"] for record in records}) != len(records) or
            len({record["file"] for record in records}) != len(records)):
        raise ValueError("Duplicate release version or file")
    for record in records:
        if (not re.fullmatch(r"\d+\.\d+\.\d+", record["version"]) or
                record["identity"] != "FM-1_900" or record["file"] != f"mc-1-{record['version']}.fwsc" or
                not re.fullmatch(r"[a-f0-9]{64}", record["sha256"]) or
                type(record["bytes"]) is not int or not 960 <= record["bytes"] <= 1048576 or
                record.get("core_mode") not in (None, "dual", "single")):
            raise ValueError("Invalid pinned release manifest")
    packages = []
    for record, local in zip(records, (args.package, args.alternative_package)):
        release_version, expected = record["version"], record["file"]
        if local is not None:
            data = local.read_bytes()
        else:
            release = json.loads(fetch(
                f"https://api.github.com/repos/dspaudio/mc-1/releases/tags/v{release_version}", 1048576))
            if release["draft"] or release["tag_name"] != f"v{release_version}":
                raise ValueError("Release is not published under the pinned tag")
            assets = [asset for asset in release["assets"] if asset["name"] == expected]
            if len(assets) != 1 or assets[0]["size"] != record["bytes"]:
                raise ValueError("Missing or mismatched release asset")
            url = assets[0]["browser_download_url"]
            if url != f"https://github.com/dspaudio/mc-1/releases/download/v{release_version}/{expected}":
                raise ValueError("Unexpected release asset URL")
            data = fetch(url, record["bytes"])
        if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
            raise ValueError("Release asset SHA256 or size mismatch")
        subprocess.run([
            "node", "--input-type=module", "-e",
            """import {loadPackage} from './site/firmware-ota.js';
const chunks = [];
for await (const chunk of process.stdin) chunks.push(chunk);
const pkg = await loadPackage(Buffer.concat(chunks), {sha256: process.argv[1]});
if (pkg.product !== process.argv[2]) throw new Error('Release identity mismatch');
""", record["sha256"], record["identity"],
        ], input=data, cwd=root, check=True)
        packages.append((expected, data))
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "styles.css", "install.html", "firmware-install.js", "firmware-ota.js",
                 "firmware-stock.js"):
        shutil.copyfile(root / "site" / name, out / name)
    shutil.copytree(root / "site/assets", out / "assets", dirs_exist_ok=True)
    shutil.copytree(root / "LICENSES", out / "LICENSES", dirs_exist_ok=True)
    (out / "audio").mkdir(exist_ok=True)
    (out / "licenses").mkdir(exist_ok=True)
    for name in ("mc-1-night-circuit-fx.mp3", "mc-1-neon-spiral-fx.mp3"):
        shutil.copyfile(root / "docs/audio" / name, out / "audio" / name)
    shutil.copyfile(root / "LICENSES/CC0-VSCO.txt", out / "licenses/CC0-VSCO.txt")
    shutil.copyfile(root / "assets/recorded/manifest.json", out / "recorded-samples.json")
    shutil.copyfile(root / "LICENSE", out / "LICENSE.txt")
    shutil.copyfile(root / "NOTICE.md", out / "NOTICE.md")
    for expected, data in packages:
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
