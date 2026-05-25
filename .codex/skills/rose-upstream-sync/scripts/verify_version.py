#!/usr/bin/env python3
"""Verify ROSE_VERSION and config/SCM_DATE agree under REX encoding rules."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import encode_rose_version


def read_value(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", help="version string to verify instead of reading ROSE_VERSION")
    parser.add_argument("--scm-date", help="encoded integer to verify instead of reading config/SCM_DATE")
    parser.add_argument("--version-file", type=Path, default=Path("ROSE_VERSION"))
    parser.add_argument("--scm-date-file", type=Path, default=Path("config/SCM_DATE"))
    args = parser.parse_args()

    version = args.version if args.version is not None else read_value(args.version_file)
    scm_date = args.scm_date if args.scm_date is not None else read_value(args.scm_date_file)
    expected = encode_rose_version(version)
    actual = int(scm_date)
    if expected != actual:
        raise SystemExit(f"version mismatch: {version} expects {expected}, got {actual}")
    print(f"version ok: {version} -> {actual}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
