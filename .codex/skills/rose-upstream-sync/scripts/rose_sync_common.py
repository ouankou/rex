#!/usr/bin/env python3
"""Shared helpers for REX's read-only ROSE upstream sync workflow."""

from __future__ import annotations

import csv
import fnmatch
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Iterable

sys.dont_write_bytecode = True


CSV_HEADER = [
    "Upstream commit",
    "Upstream date",
    "Summary",
    "Paths touched",
    "Decision",
    "Apply method",
    "REX commit",
    "Validation",
    "Notes",
]

UPSTREAM_REMOTE_URL = "https://github.com/llnl/rose.git"
UPSTREAM_REF = "rose/weekly"

DROPPED_PATH_PATTERNS = [
    ".gitlab-ci.yml",
    "Makefile.am",
    "*/Makefile.am",
    "Tupfile",
    "*/Tupfile",
    "configure.ac",
    "acmacros/*",
    "config/support-rose.m4",
    "config/support-yamlcpp.m4",
    "src/AstNodes/BinaryAnalysis/*",
    "src/AstNodes/Jovial/*",
    "src/generated/BinaryAnalysis/*",
    "src/generated/Jovial/*",
    "src/Rose/BinaryAnalysis.h",
    "src/Rose/BinaryAnalysis/*",
    "src/Rose/Yaml.*",
    "src/Rose/SawyerGraphConnection.h",
    "src/Rosebud/*",
    "src/Sawyer/*",
    "src/util/Sawyer/*",
    "src/frontend/BinaryFormats/*",
    "src/frontend/Disassemblers/*",
    "src/frontend/CxxFrontend/EDG/*",
    "src/frontend/CxxFrontend/EDG_VERSION",
    "src/frontend/Experimental_Ada_ROSE_Connection/*",
    "src/frontend/Experimental_Csharp_ROSE_Connection/*",
    "src/frontend/Experimental_Jovial_ROSE_Connection/*",
    "src/frontend/Experimental_Matlab_ROSE_Connection/*",
    "src/frontend/ECJ_ROSE_Connection/*",
    "src/frontend/JavaFrontend/*",
    "src/frontend/PHPFrontend/*",
    "src/frontend/PythonFrontend/*",
    "src/frontend/X10_ROSE_Connection/*",
    "src/3rdPartyLibraries/fortran-parser/*",
    "src/3rdPartyLibraries/qrose/*",
    "tools/BinaryAnalysis/*",
    "tools/CodeThorn/*",
]

VERSION_TRACKER_PATHS = {"ROSE_VERSION", "config/SCM_DATE"}


def run_git(args: list[str], repo: Path = Path("."), check: bool = True) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        text=True,
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        msg = result.stderr.strip() or result.stdout.strip() or f"exit code {result.returncode}"
        raise SystemExit(f"error running git {shlex.join(args)}: {msg}")
    return result.stdout


def is_dropped_path(path: str) -> bool:
    normalized = path.strip().lstrip("./")
    return any(fnmatch.fnmatch(normalized, pattern) for pattern in DROPPED_PATH_PATTERNS)


def dropped_paths(paths: Iterable[str]) -> list[str]:
    return sorted({path for path in paths if path and is_dropped_path(path)})


def existing_sync_logs(log_dir: Path) -> list[Path]:
    return sorted(log_dir.glob("rose-*-commits.csv"))


def read_rows(csv_path: Path) -> list[dict[str, str]]:
    if not csv_path.exists():
        return []
    with csv_path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def write_rows(csv_path: Path, rows: list[dict[str, str]]) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=CSV_HEADER)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: (row.get(key) or "") for key in CSV_HEADER})


def commit_exists(repo: Path, sha: str) -> bool:
    if not sha or sha == "N/A":
        return False
    result = subprocess.run(
        ["git", "cat-file", "-e", f"{sha}^{{commit}}"],
        cwd=repo,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def latest_recorded_upstream_sha(repo: Path, log_dir: Path) -> str:
    latest = ""
    latest_date = ""
    for csv_path in existing_sync_logs(log_dir):
        for row in read_rows(csv_path):
            sha = (row.get("Upstream commit") or "").strip()
            date = (row.get("Upstream date") or row.get("Date") or "").strip()
            if sha and sha != "N/A":
                if not date and commit_exists(repo, sha):
                    date = commit_date(repo, sha)
                if date >= latest_date:
                    latest = sha
                    latest_date = date
    if latest and not commit_exists(repo, latest):
        raise SystemExit(f"latest recorded upstream SHA is not present locally: {latest}")
    return latest


def commit_paths(repo: Path, sha: str) -> list[str]:
    output = run_git(["show", "--name-only", "--format=", sha], repo)
    return [line.strip() for line in output.splitlines() if line.strip()]


def commit_subject(repo: Path, sha: str) -> str:
    return run_git(["show", "-s", "--format=%s", sha], repo).strip()


def commit_date(repo: Path, sha: str) -> str:
    return run_git(["show", "-s", "--format=%ad", "--date=short", sha], repo).strip()


def is_merge_commit(repo: Path, sha: str) -> bool:
    parents = run_git(["show", "-s", "--format=%P", sha], repo).split()
    return len(parents) > 1


def is_release_subject(subject: str) -> bool:
    lower = subject.lower()
    return "incremented version" in lower or lower.startswith("(release)") or lower.startswith("(rose release)")


def encode_rose_version(version: str) -> int:
    raw_parts = version.strip().split(".")
    if len(raw_parts) not in (3, 4) or not all(part.isascii() and part.isdigit() for part in raw_parts):
        raise ValueError(f"unsupported ROSE version format: {version!r}")

    parts = [int(part) for part in raw_parts]
    if len(parts) == 4:
        major, minor, patch, build = parts
        if major >= 10 or minor >= 100 or patch >= 1000 or build >= 10000:
            raise ValueError(f"ROSE 4-component version out of range: {version}")
        return (((major * 100 + minor) * 1000 + patch) * 10000 + build)
    if len(parts) == 3:
        major, minor, patch = parts
        if minor >= 10000 or patch >= 1000:
            raise ValueError(f"ROSE 3-component version out of range: {version}")
        return major * 10000000 + minor * 1000 + patch
