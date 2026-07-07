#!/usr/bin/env python3
"""List upstream ROSE commits pending for REX sync."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import UPSTREAM_REF, latest_recorded_upstream_sha, run_git


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path("."), help="REX repository root")
    parser.add_argument("--log-dir", type=Path, default=Path("docs/upstream-sync"), help="sync CSV directory")
    parser.add_argument("--from-sha", help="override checkpoint SHA")
    parser.add_argument("--upstream-ref", default=UPSTREAM_REF, help="upstream ref to inspect")
    parser.add_argument("--limit", type=int, default=0, help="maximum rows to print; 0 means all")
    args = parser.parse_args()

    repo = args.repo.resolve()
    log_dir = args.log_dir if args.log_dir.is_absolute() else repo / args.log_dir
    checkpoint = args.from_sha or latest_recorded_upstream_sha(repo, log_dir)
    if not checkpoint:
        raise SystemExit("no checkpoint found; pass --from-sha or add a sync CSV row")

    output = run_git(
        [
            "log",
            f"{checkpoint}..{args.upstream_ref}",
            "--format=%H\t%cd\t%s",
            "--date=short",
            "--reverse",
        ],
        repo,
    )
    all_rows = [line for line in output.splitlines() if line.strip()]
    rows = all_rows
    if args.limit:
        rows = rows[: args.limit]
    print(f"checkpoint\t{checkpoint}")
    print(f"pending_total\t{len(all_rows)}")
    if args.limit:
        print(f"showing\t{len(rows)}")
    for row in rows:
        print(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
