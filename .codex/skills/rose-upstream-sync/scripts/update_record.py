#!/usr/bin/env python3
"""Append or update one row in a REX ROSE upstream sync CSV."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import CSV_HEADER, read_rows, write_rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, default=Path("docs/upstream-sync/rose-2026-commits.csv"))
    parser.add_argument("--upstream", required=True, help="upstream ROSE SHA")
    parser.add_argument("--date")
    parser.add_argument("--summary")
    parser.add_argument("--paths")
    parser.add_argument("--decision", required=True)
    parser.add_argument("--apply-method")
    parser.add_argument("--rex-commit")
    parser.add_argument("--validation")
    parser.add_argument("--notes")
    args = parser.parse_args()

    row = {
        "Upstream commit": args.upstream,
        "Upstream date": args.date,
        "Summary": args.summary,
        "Paths touched": args.paths,
        "Decision": args.decision,
        "Apply method": args.apply_method,
        "REX commit": args.rex_commit,
        "Validation": args.validation,
        "Notes": args.notes,
    }
    rows = read_rows(args.csv)
    replaced = False
    for index, existing in enumerate(rows):
        if existing.get("Upstream commit") == args.upstream:
            rows[index] = {
                key: (row[key] if row.get(key) is not None else existing.get(key, ""))
                for key in CSV_HEADER
            }
            replaced = True
            break
    if not replaced:
        rows.append({key: (row.get(key) or "") for key in CSV_HEADER})
    write_rows(args.csv, rows)
    print(("updated" if replaced else "appended") + f" {args.upstream} in {args.csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
