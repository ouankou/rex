#!/usr/bin/env python3
"""Run an exact, name-based CTest manifest against one configured build."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import NoReturn


def fail(message: str) -> NoReturn:
    raise SystemExit(message)


def read_manifest(path: Path) -> list[str]:
    names: list[str] = []
    seen: set[str] = set()
    duplicates: list[str] = []
    for line_number, raw_line in enumerate(path.read_text().splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line in seen:
            duplicates.append(f"{line} (line {line_number})")
        else:
            seen.add(line)
            names.append(line)
    if duplicates:
        fail("duplicate test names in manifest:\n  " + "\n  ".join(duplicates))
    if not names:
        fail(f"test manifest is empty: {path}")
    return names


def ctest_inventory(ctest: str, test_dir: Path, extra: list[str]) -> list[str]:
    command = [
        ctest,
        "--test-dir",
        str(test_dir),
        "-N",
        "--show-only=json-v1",
        *extra,
    ]
    completed = subprocess.run(
        command, check=True, stdout=subprocess.PIPE, text=True
    )
    try:
        document = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        fail(f"CTest returned malformed JSON inventory: {error}")
    tests = document.get("tests")
    if not isinstance(tests, list):
        fail("CTest JSON inventory has no test list")
    names: list[str] = []
    for index, test in enumerate(tests, 1):
        name = test.get("name") if isinstance(test, dict) else None
        if not isinstance(name, str) or not name:
            fail(f"CTest JSON inventory has an invalid test at index {index}")
        names.append(name)
    return names


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run every test in an exact name-based CTest manifest"
    )
    parser.add_argument("--test-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--jobs", type=int, required=True)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument(
        "--include-regex",
        help="union the exact manifest with CTest's own -R selection",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="validate and print the exact selected test count without executing",
    )
    args = parser.parse_args()

    if args.jobs <= 0:
        fail("--jobs must be a positive integer")
    test_dir = args.test_dir.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True)
    requested = read_manifest(manifest)

    registry_names = ctest_inventory(args.ctest, test_dir, [])
    registry: dict[str, int] = {}
    duplicates: list[str] = []
    for index, name in enumerate(registry_names, 1):
        if name in registry:
            duplicates.append(name)
        else:
            registry[name] = index
    if duplicates:
        fail("CTest registry has duplicate test names:\n  " + "\n  ".join(duplicates))

    missing = [name for name in requested if name not in registry]
    if missing:
        fail(
            f"{len(missing)} manifest tests are absent from the configured "
            "CTest registry:\n  "
            + "\n  ".join(missing)
        )

    indices = sorted(registry[name] for name in requested)
    selector = "0,0,1," + ",".join(str(index) for index in indices) + "\n"
    with tempfile.NamedTemporaryFile(
        mode="w", prefix="rex-ctest-set-", delete=True
    ) as handle:
        handle.write(selector)
        handle.flush()
        selection_options = ["-I", handle.name]
        regex_names: list[str] = []
        if args.include_regex:
            regex_names = ctest_inventory(
                args.ctest, test_dir, ["-R", args.include_regex]
            )
            selection_options.extend(["-R", args.include_regex, "-U"])
        selected_names = ctest_inventory(
            args.ctest, test_dir, selection_options
        )
        requested_set = set(requested)
        expected_set = requested_set | set(regex_names)
        selected_set = set(selected_names)
        if selected_set != expected_set or len(selected_names) != len(expected_set):
            missing_from_selection = sorted(expected_set - selected_set)
            unexpected = sorted(selected_set - expected_set)
            fail(
                "CTest selection diverged from the exact requested union"
                f"\nmissing: {missing_from_selection}"
                f"\nunexpected: {unexpected}"
            )

        print(
            "Validated exact CTest selection: "
            f"manifest={len(requested)}, regex={len(regex_names)}, "
            f"union={len(expected_set)}, source={manifest}",
            flush=True,
        )
        if args.dry_run:
            return 0
        command = [
            args.ctest,
            "--test-dir",
            str(test_dir),
            *selection_options,
            "--no-tests=error",
            "--output-on-failure",
            f"-j{args.jobs}",
        ]
        return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
