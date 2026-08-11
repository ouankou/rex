#!/usr/bin/env python3
"""Run an exact, name-based CTest manifest against one configured build."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any, NoReturn


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


def ctest_records(
    ctest: str, test_dir: Path, extra: list[str]
) -> list[dict[str, Any]]:
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
    records: list[dict[str, Any]] = []
    for index, test in enumerate(tests, 1):
        name = test.get("name") if isinstance(test, dict) else None
        if not isinstance(name, str) or not name:
            fail(f"CTest JSON inventory has an invalid test at index {index}")
        records.append(test)
    return records


def ctest_inventory(ctest: str, test_dir: Path, extra: list[str]) -> list[str]:
    return [test["name"] for test in ctest_records(ctest, test_dir, extra)]


def fixture_property(test: dict[str, Any], name: str) -> set[str]:
    properties = test.get("properties", [])
    if not isinstance(properties, list):
        fail(f"CTest test {test['name']} has malformed properties")
    result: set[str] = set()
    for prop in properties:
        if not isinstance(prop, dict) or prop.get("name") != name:
            continue
        value = prop.get("value")
        if not isinstance(value, list) or not all(
            isinstance(item, str) and item for item in value
        ):
            fail(f"CTest test {test['name']} has malformed {name}")
        result.update(value)
    return result


def fixture_selection_closure(
    registry: list[dict[str, Any]], directly_selected: set[str]
) -> set[str]:
    """Return the tests CTest must add to satisfy selected fixtures exactly."""

    by_name = {test["name"]: test for test in registry}
    selected = set(directly_selected)
    while True:
        required: set[str] = set()
        for name in selected:
            test = by_name.get(name)
            if test is None:
                fail(f"fixture closure references absent CTest test: {name}")
            required.update(fixture_property(test, "FIXTURES_REQUIRED"))

        support = {
            test["name"]
            for test in registry
            if required
            & (
                fixture_property(test, "FIXTURES_SETUP")
                | fixture_property(test, "FIXTURES_CLEANUP")
            )
        }
        expanded = selected | support
        if expanded == selected:
            return selected
        selected = expanded


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

    registry_records = ctest_records(args.ctest, test_dir, [])
    registry_names = [test["name"] for test in registry_records]
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
            # Ask CTest for direct regex matches without its automatic fixture
            # expansion.  The exact expansion is computed and checked below.
            regex_names = ctest_inventory(
                args.ctest,
                test_dir,
                [
                    "-R",
                    args.include_regex,
                    "-FA",
                    ".*",
                    "-FS",
                    ".*",
                    "-FC",
                    ".*",
                ],
            )
            selection_options.extend(["-R", args.include_regex, "-U"])
        selected_names = ctest_inventory(
            args.ctest, test_dir, selection_options
        )
        requested_set = set(requested)
        directly_selected = requested_set | set(regex_names)
        expected_set = fixture_selection_closure(
            registry_records, directly_selected
        )
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
            f"fixture_support={len(expected_set - directly_selected)}, "
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
