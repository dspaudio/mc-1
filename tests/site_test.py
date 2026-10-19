#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the actual product/installer Pages artifact and rejected build inputs."""
import hashlib
import argparse
from html.parser import HTMLParser
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import runpy
from unittest.mock import patch
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


class PageLinks(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.ids: set[str] = set()
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for name, value in attrs:
            if value is None:
                continue
            if name == "id":
                self.ids.add(value)
            if name in ("src", "href"):
                self.links.append(value)


def check_site(root: Path, manifest: Path = ROOT / "FIRMWARE.json") -> None:
    root = root.resolve()
    pages: dict[Path, PageLinks] = {}
    for path in root.rglob("*.html"):
        page = PageLinks()
        page.feed(path.read_text(encoding="utf-8"))
        pages[path] = page
    assert {path.name for path in pages} == {"index.html", "install.html"}
    assert "install.html" in pages[root / "index.html"].links
    installer_url = f"firmware-install.js?v={(ROOT / 'VERSION').read_text().strip()}"
    assert installer_url in pages[root / "install.html"].links
    checked = 0
    for path, page in pages.items():
        for link in page.links:
            url = urlsplit(link)
            if url.scheme or url.netloc:
                continue
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            assert target.is_relative_to(root), f"Path outside site: {link}"
            if target.is_dir():
                target = target / "index.html"
            assert target.is_file() and target.stat().st_size, f"Missing resource: {link}"
            if url.fragment:
                assert target in pages and unquote(url.fragment) in pages[target].ids
            checked += 1
    pinned = json.loads(manifest.read_text())
    assert pinned["file"] == f"mc-1-{pinned['version']}.fwsc"
    assert json.loads((root / "firmware-release.json").read_text()) == pinned
    package = root / pinned["file"]
    for record in [pinned, *pinned.get("alternatives", [])]:
        asset = root / record["file"]
        assert asset.stat().st_size == record["bytes"]
        assert hashlib.sha256(asset.read_bytes()).hexdigest() == record["sha256"]
    files = set()
    for line in (root / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        path = (root / name).resolve()
        assert path.is_relative_to(root) and name not in files
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
        files.add(name)
    assert files == {
        path.relative_to(root).as_posix()
        for path in root.rglob("*") if path.is_file() and path.name != "SHA256SUMS"
    }
    assert {"firmware-stock.js", "firmware-ota.js", "firmware-install.js"} <= files
    assert {"audio/mc-1-night-circuit-fx.mp3", "audio/mc-1-neon-spiral-fx.mp3"} <= files
    with tempfile.TemporaryDirectory(prefix="mc1-pages-test-") as temporary:
        temp = Path(temporary)
        for field, value in [("version", "../1"), ("identity", "OTHER_001"),
                             ("file", f"mc-1-{pinned['version']}-EXPERIMENTAL.fwsc"),
                             ("bytes", pinned["bytes"] + 1), ("sha256", "0" * 64)]:
            manifest = temp / "FIRMWARE.json"
            manifest.write_text(json.dumps({**pinned, field: value}))
            output = temp / field
            result = subprocess.run([
                sys.executable, str(ROOT / "tools/make_site.py"),
                "--manifest", str(manifest), "--package", str(package), "--out", str(output),
                *(["--alternative-package", str(root / pinned["alternatives"][0]["file"])]
                  if pinned.get("alternatives") else []),
            ], capture_output=True, check=False)
            assert result.returncode != 0 and not output.exists(), field
    print(f"PRODUCT_INSTALLER_SITE_PASS pages={len(pages)} links={checked} rejected=5")


def check_versions(original_package: Path) -> None:
    original = {
        "version": "1.0.2", "core_mode": "single", "identity": "FM-1_900",
        "file": "mc-1-1.0.2.fwsc", "bytes": 610089,
        "sha256": "a09e763e6000f25dec63eb5feeb6251389dce33bf5abe2eb4746f1f1c7091ab5",
    }
    raw = original_package.read_bytes()
    assert len(raw) == original["bytes"] and hashlib.sha256(raw).hexdigest() == original["sha256"]
    # Reuse the known valid image as a packaging fixture, not a new hardware build.
    current = {**original, "version": (ROOT / "VERSION").read_text().strip(), "core_mode": "dual"}
    current["file"] = f"mc-1-{current['version']}.fwsc"
    assert current["version"] != original["version"]
    pinned = {**current, "alternatives": [original]}
    with tempfile.TemporaryDirectory(prefix="mc1-two-versions-") as temporary:
        temp = Path(temporary)
        manifest, output = temp / "release.json", temp / "site"
        manifest.write_text(json.dumps(pinned))
        command = [sys.executable, str(ROOT / "tools/make_site.py"), "--manifest", str(manifest),
                   "--package", str(original_package.resolve()),
                   "--alternative-package", str(original_package.resolve())]
        subprocess.run([*command, "--out", str(output)], check=True)
        check_site(output, manifest)
        invalid = [None, {}, [], [original, original], [None],
                   *[[{**original, key: None}] for key in original],
                   [{**original, "file": "../outside.fwsc"}],
                   [{**original, "bytes": 959}], [{**original, "bytes": 1048577}],
                   [{**original, "bytes": 610089.5}], [{**original, "sha256": "b" * 64}]]
        for index, alternatives in enumerate(invalid):
            manifest.write_text(json.dumps({**pinned, "alternatives": alternatives}))
            target = temp / f"invalid-{index}"
            result = subprocess.run([*command, "--out", str(target)], capture_output=True, check=False)
            assert result.returncode != 0 and not target.exists(), alternatives
        manifest.write_text(json.dumps({**pinned, "version": "1.0.2", "file": original["file"]}))
        target = temp / "duplicate"
        result = subprocess.run([*command, "--out", str(target)], capture_output=True, check=False)
        assert result.returncode != 0 and not target.exists()
        manifest.write_text(json.dumps(pinned))
        bad_package = temp / "bad.fwsc"
        bad_package.write_bytes(raw[:-1])
        target = temp / "bad-package"
        result = subprocess.run([*command, "--alternative-package", str(bad_package),
                                 "--out", str(target)], capture_output=True, check=False)
        assert result.returncode != 0 and not target.exists()

        module = runpy.run_path(str(ROOT / "tools/make_site.py"))
        calls = []
        defect = None

        def fake_fetch(url: str, maximum: int) -> bytes:
            calls.append(url)
            for record in [current, original]:
                version, name = record["version"], record["file"]
                tag_url = f"https://api.github.com/repos/dspaudio/mc-1/releases/tags/v{version}"
                asset_url = f"https://github.com/dspaudio/mc-1/releases/download/v{version}/{name}"
                if url == tag_url:
                    asset = {"name": name, "size": record["bytes"], "browser_download_url": asset_url}
                    release = {"draft": False, "tag_name": f"v{version}", "assets": [asset]}
                    if record is original and defect:
                        release, asset = defect(release, asset)
                    return json.dumps(release).encode()
                if url == asset_url:
                    assert maximum == record["bytes"]
                    return raw
            raise AssertionError(f"Unexpected fetch: {url}")

        with patch.dict(module["main"].__globals__, fetch=fake_fetch):
            target = temp / "fetched"
            with patch.object(sys, "argv", ["make_site.py", "--manifest", str(manifest),
                                          "--out", str(target)]):
                module["main"]()
            assert len(calls) == 4 and len(set(calls)) == 4
            check_site(target, manifest)
            defects = [
                lambda release, asset: ({**release, "draft": True}, asset),
                lambda release, asset: ({**release, "tag_name": "v0.0.0"}, asset),
                lambda release, asset: ({**release, "assets": []}, asset),
                lambda release, asset: ({**release, "assets": [asset, asset]}, asset),
                lambda release, asset: ({**release, "assets": [{**asset, "size": 610088}]}, asset),
                lambda release, asset: ({**release, "assets": [{**asset,
                    "browser_download_url": "https://example.invalid/firmware.fwsc"}]}, asset),
            ]
            for index, defect in enumerate(defects):
                target = temp / f"fetch-invalid-{index}"
                with patch.object(sys, "argv", ["make_site.py", "--manifest", str(manifest),
                                              "--out", str(target)]):
                    try:
                        module["main"]()
                    except ValueError:
                        pass
                    else:
                        raise AssertionError(f"Accepted invalid release: {index}")
                assert not target.exists()
    print("PRODUCT_TWO_VERSIONS_PASS offline=2 fetched=2 alternatives_rejected=18 fetch_rejected=6")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path, nargs="?")
    parser.add_argument("--original-package", type=Path)
    args = parser.parse_args()
    if args.site is not None or args.original_package is None:
        check_site(args.site or ROOT / "build/site")
    if args.original_package is not None:
        check_versions(args.original_package)
