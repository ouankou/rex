#!/usr/bin/env python3
"""Classify one upstream ROSE commit for REX sync triage."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from rose_sync_common import (
    VERSION_TRACKER_PATHS,
    commit_paths,
    commit_subject,
    dropped_paths,
    is_merge_commit,
    is_release_subject,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("commit", help="upstream ROSE commit SHA")
    parser.add_argument("--repo", type=Path, default=Path("."), help="REX repository root")
    args = parser.parse_args()

    repo = args.repo.resolve()
    paths = commit_paths(repo, args.commit)
    subject = commit_subject(repo, args.commit)
    denied = dropped_paths(paths)
    path_set = set(paths)

    if is_merge_commit(repo, args.commit):
        decision = "drop"
        note = "merge commit"
    elif is_release_subject(subject) and path_set <= VERSION_TRACKER_PATHS.union({"configure.ac", "src/frontend/CxxFrontend/EDG_VERSION", "config/SCM_DATE"}):
        decision = "release-candidate"
        note = "apply only the latest version marker after the rest of the range is synced"
    elif paths and len(denied) == len(paths):
        decision = "drop"
        note = "all touched paths are abandoned or inventory-only in REX"
    elif denied:
        decision = "pick-partial-candidate"
        note = "mixed retained and dropped paths; remove dropped hunks before commit"
    else:
        decision = "pick-candidate"
        note = "no hard-dropped paths detected; still inspect semantics manually"

    print(f"commit\t{args.commit}")
    print(f"summary\t{subject}")
    print(f"decision\t{decision}")
    print(f"note\t{note}")
    if denied:
        print("dropped_paths")
        for path in denied:
            print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
