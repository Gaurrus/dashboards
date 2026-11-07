#!/usr/bin/env python3
"""Write dist/t3-dash-v<VER>.json with version, git commit and SHA-256."""
from __future__ import annotations

import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
DIST = ROOT / "dist" / f"v{VERSION}"
BIN = DIST / f"t3-dash-waveshare-7-16MB-v{VERSION}.bin"
ZIP = DIST / f"t3-dash-arduino-sketch-v{VERSION}.zip"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def file_info(path: Path, flash_offset: str | None = None) -> dict:
    if not path.exists():
        raise SystemExit(f"missing dist file: {path}")
    info = {
        "name": path.name,
        "size_bytes": path.stat().st_size,
        "sha256": sha256(path),
    }
    if flash_offset is not None:
        info["flash_offset"] = flash_offset
    return info


def main() -> None:
    git = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True
    ).strip()
    dirty = bool(
        subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()
    )
    data = {
        "version": VERSION,
        "git_commit": git,
        "git_dirty": dirty,
        "built_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "board": "Waveshare ESP32-S3-Touch-LCD-7",
        "lvgl": "8.4.0",
        "files": {
            "firmware": file_info(BIN, "0x0"),
            "arduino_sketch": file_info(ZIP),
        },
    }
    DIST.mkdir(parents=True, exist_ok=True)
    out = DIST / f"t3-dash-v{VERSION}.json"
    out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(out)


if __name__ == "__main__":
    main()
