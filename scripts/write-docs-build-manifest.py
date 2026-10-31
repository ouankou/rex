import argparse
import importlib.metadata
import json
import os
import subprocess
import sys
import time
from pathlib import Path


def _run(cmd: list[str], *, multiline: bool = False) -> str:
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL, text=True)
        return out.strip() if multiline else out.splitlines()[0].strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def write_manifest(repo_root, site_dir):
    out = site_dir / "docs-build-manifest.json"

    git_commit = ""
    if (repo_root / ".git").exists():
        git_commit = _run(["git", "-C", str(repo_root), "rev-parse", "HEAD"])

    mrdocs_bin = os.environ.get("MRDOCS_BIN", "")
    mrdocs_version = _run([mrdocs_bin, "--version"], multiline=True) if mrdocs_bin else ""

    clang_version = _run(["clang++", "--version"])
    python_version = f"Python {sys.version.split()[0]}"

    data = {
        "generated_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "git_commit": git_commit,
        "mrdocs_version": mrdocs_version,
        "zensical_version": importlib.metadata.version("zensical"),
        "python_version": python_version,
        "clang_version": clang_version,
        "jobs": int(os.environ.get("DOCS_JOBS", "2")),
    }

    site_dir.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record the documentation build provenance.")
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--site-dir", type=Path, required=True)
    args = parser.parse_args()
    write_manifest(args.root.resolve(), args.site_dir.resolve())
