#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the actual product/installer Pages artifact and rejected build inputs."""
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import subprocess
import sys
import tempfile
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


def check_site(root: Path) -> None:
    root = root.resolve()
    pages: dict[Path, PageLinks] = {}
    for path in root.rglob("*.html"):
        page = PageLinks()
        page.feed(path.read_text(encoding="utf-8"))
        pages[path] = page
    assert {path.name for path in pages} == {"index.html", "install.html"}
    assert "install.html" in pages[root / "index.html"].links
    assert "firmware-install.js" in pages[root / "install.html"].links
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
    pinned = json.loads((ROOT / "FIRMWARE.json").read_text())
    assert json.loads((root / "firmware-release.json").read_text()) == pinned
    package = root / pinned["file"]
    assert package.stat().st_size == pinned["bytes"]
    assert hashlib.sha256(package.read_bytes()).hexdigest() == pinned["sha256"]
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
    with tempfile.TemporaryDirectory(prefix="mc1-pages-test-") as temporary:
        temp = Path(temporary)
        for field, value in [("version", "../1"), ("identity", "OTHER_001"),
                             ("bytes", pinned["bytes"] + 1), ("sha256", "0" * 64)]:
            manifest = temp / "FIRMWARE.json"
            manifest.write_text(json.dumps({**pinned, field: value}))
            output = temp / field
            result = subprocess.run([
                sys.executable, str(ROOT / "tools/make_site.py"),
                "--manifest", str(manifest), "--package", str(package), "--out", str(output),
            ], capture_output=True, check=False)
            assert result.returncode != 0 and not output.exists(), field
    print(f"PRODUCT_INSTALLER_SITE_PASS pages={len(pages)} links={checked} rejected=4")


if __name__ == "__main__":
    check_site(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build/site")
