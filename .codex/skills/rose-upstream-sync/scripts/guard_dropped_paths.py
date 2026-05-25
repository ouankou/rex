#!/usr/bin/env python3
"""Fail if a REX sync diff reintroduces hard-dropped upstream paths."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import dropped_paths, run_git


def changed_paths(repo: Path, staged: bool, ref_range: str | None, stdin: bool) -> list[str]:
    if stdin:
        return [line.strip() for line in sys.stdin if line.strip()]
    if ref_range:
        output = run_git(["diff", "--name-only", ref_range], repo)
    elif staged:
        output = run_git(["diff", "--cached", "--name-only"], repo)
    else:
        output = run_git(["diff", "--name-only"], repo)
    return [line.strip() for line in output.splitlines() if line.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path("."), help="REX repository root")
    parser.add_argument("--staged", action="store_true", help="check staged paths")
    parser.add_argument("--range", dest="ref_range", help="git diff range to check")
    parser.add_argument("--stdin", action="store_true", help="read paths from stdin")
    args = parser.parse_args()

    paths = changed_paths(args.repo.resolve(), args.staged, args.ref_range, args.stdin)
    denied = dropped_paths(paths)
    if denied:
        print("hard-dropped REX paths detected:", file=sys.stderr)
        for path in denied:
            print(path, file=sys.stderr)
        return 1
    print(f"checked {len(paths)} paths; no hard-dropped REX paths detected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
