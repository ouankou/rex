import json
import os
import subprocess
import sys
import time
from pathlib import Path


def _run(cmd: list[str]) -> str:
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL, text=True)
        return out.splitlines()[0].strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def on_post_build(config):
    site_dir = Path(config["site_dir"]).resolve()
    out = site_dir / "docs-build-manifest.json"

    git_commit = ""
    repo_root = Path(config["config_file_path"]).resolve().parent
    if (repo_root / ".git").exists():
        git_commit = _run(["git", "-C", str(repo_root), "rev-parse", "HEAD"])

    mrdocs_bin = os.environ.get("MRDOCS_BIN", "")
    mrdocs_version = _run([mrdocs_bin, "--version"]) if mrdocs_bin else ""
    mkdocs_version = ""
    try:
        import mkdocs  # type: ignore

        mkdocs_version = f"mkdocs {mkdocs.__version__}"
    except (ImportError, AttributeError):
        mkdocs_version = _run(["mkdocs", "--version"])

    clang_version = _run(["clang++", "--version"])
    python_version = f"Python {sys.version.split()[0]}"

    data = {
        "generated_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "git_commit": git_commit,
        "mrdocs_version": mrdocs_version,
        "mkdocs_version": mkdocs_version,
        "python_version": python_version,
        "clang_version": clang_version,
        "jobs": int(os.environ.get("DOCS_JOBS", "32")),
    }

    site_dir.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
