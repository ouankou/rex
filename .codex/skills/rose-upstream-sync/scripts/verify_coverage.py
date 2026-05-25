#!/usr/bin/env python3
"""Verify that a sync CSV covers every upstream ROSE commit in a range."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import UPSTREAM_REF, commit_exists, existing_sync_logs, read_rows, run_git

ALLOWED_DECISIONS = {
    "already-present",
    "drop",
    "drop-superseded",
    "pick",
    "pick-partial",
    "version-tracker",
}
PICK_DECISIONS = {"pick", "pick-partial", "version-tracker"}


def latest_recorded_upstream_sha_excluding(repo: Path, log_dir: Path, excluded_csv: Path) -> str:
    latest = ""
    latest_date = ""
    excluded = excluded_csv.resolve()
    for csv_path in existing_sync_logs(log_dir):
        if csv_path.resolve() == excluded:
            continue
        for row in read_rows(csv_path):
            sha = row.get("Upstream commit", "").strip()
            date = (row.get("Upstream date", "") or row.get("Date", "")).strip()
            if sha and sha != "N/A" and date >= latest_date:
                latest = sha
                latest_date = date
    if latest and not commit_exists(repo, latest):
        raise SystemExit(f"latest recorded upstream SHA is not present locally: {latest}")
    return latest


def upstream_range(repo: Path, base: str, tip: str) -> list[str]:
    output = run_git(["log", f"{base}..{tip}", "--format=%H", "--reverse"], repo)
    return [line.strip() for line in output.splitlines() if line.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path("."), help="REX repository root")
    parser.add_argument("--log-dir", type=Path, default=Path("docs/upstream-sync"), help="sync CSV directory")
    parser.add_argument("--csv", type=Path, default=Path("docs/upstream-sync/rose-2026-commits.csv"), help="CSV to verify")
    parser.add_argument("--from-sha", help="base upstream SHA; defaults to latest prior sync log SHA")
    parser.add_argument("--to-ref", default=UPSTREAM_REF, help="upstream tip/ref to verify through")
    args = parser.parse_args()

    repo = args.repo.resolve()
    log_dir = args.log_dir if args.log_dir.is_absolute() else repo / args.log_dir
    csv_path = args.csv if args.csv.is_absolute() else repo / args.csv
    base = args.from_sha or latest_recorded_upstream_sha_excluding(repo, log_dir, csv_path)
    if not base:
        raise SystemExit("no base checkpoint found; pass --from-sha")

    expected = upstream_range(repo, base, args.to_ref)
    expected_set = set(expected)
    rows = read_rows(csv_path)
    by_sha = {row.get("Upstream commit", "").strip(): row for row in rows if row.get("Upstream commit", "").strip()}
    recorded_set = set(by_sha)

    missing = [sha for sha in expected if sha not in recorded_set]
    extra = sorted(recorded_set - expected_set)
    blank_decision = [sha for sha in expected if sha in by_sha and not by_sha[sha].get("Decision", "").strip()]
    invalid_decision = [
        sha
        for sha in expected
        if sha in by_sha
        and by_sha[sha].get("Decision", "").strip()
        and by_sha[sha].get("Decision", "").strip() not in ALLOWED_DECISIONS
    ]
    missing_rex_commit = [
        sha
        for sha in expected
        if sha in by_sha
        and by_sha[sha].get("Decision", "").strip() in PICK_DECISIONS
        and not by_sha[sha].get("REX commit", "").strip()
    ]

    failures = False
    if missing:
        failures = True
        print("missing upstream rows:")
        for sha in missing:
            print(sha)
    if extra:
        failures = True
        print("rows outside verified range:")
        for sha in extra:
            print(sha)
    if blank_decision:
        failures = True
        print("rows with blank Decision:")
        for sha in blank_decision:
            print(sha)
    if invalid_decision:
        failures = True
        print("rows with invalid Decision:")
        for sha in invalid_decision:
            print(f"{sha} {by_sha[sha].get('Decision', '').strip()}")
    if missing_rex_commit:
        failures = True
        print("picked rows without REX commit mapping:")
        for sha in missing_rex_commit:
            print(sha)

    if failures:
        return 1

    print(f"coverage ok: {len(expected)} upstream commits recorded in {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
