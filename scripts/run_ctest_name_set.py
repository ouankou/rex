#!/usr/bin/env python3
"""Run an exact, dependency-closed CTest selection."""

from __future__ import annotations

import argparse
import hashlib
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


def shard_for_name(name: str, shard_count: int) -> int:
    """Return a stable one-based shard for a CTest name."""

    digest = hashlib.sha256(name.encode("utf-8")).digest()
    return int.from_bytes(digest[:8], byteorder="big") % shard_count + 1


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


def selection_dependency_closure(
    registry: list[dict[str, Any]], directly_selected: set[str]
) -> tuple[set[str], set[str], set[str]]:
    """Return the exact transitive CTest dependency and fixture closure."""

    by_name = {test["name"]: test for test in registry}
    selected = set(directly_selected)
    dependency_support: set[str] = set()
    fixture_support: set[str] = set()
    while True:
        required: set[str] = set()
        for name in selected:
            test = by_name.get(name)
            if test is None:
                fail(f"selection closure references absent CTest test: {name}")
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
        new_fixture_support = support - selected
        fixture_support.update(new_fixture_support)

        dependencies: set[str] = set()
        for name in selected:
            dependencies.update(fixture_property(by_name[name], "DEPENDS"))
        missing_dependencies = sorted(dependencies - by_name.keys())
        if missing_dependencies:
            fail(
                "CTest dependency closure references absent tests:\n  "
                + "\n  ".join(missing_dependencies)
            )
        # CTest's JSON inventory may materialize fixture setup/cleanup tests in
        # DEPENDS as well as publishing the fixture properties.  Classify those
        # tests once as fixture support while retaining every dependency.
        new_dependencies = dependencies - selected - support
        dependency_support.update(new_dependencies)
        if not new_dependencies and not new_fixture_support:
            return selected, dependency_support, fixture_support
        selected.update(new_dependencies | new_fixture_support)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run an exact, dependency-closed CTest selection"
    )
    parser.add_argument("--test-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--expected-absent-manifest",
        type=Path,
        help=(
            "declare manifest tests that must be absent from this configured "
            "CTest registry"
        ),
    )
    parser.add_argument("--jobs", type=int, required=True)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument(
        "--include-regex",
        help="union the exact manifest with CTest's own -R selection",
    )
    parser.add_argument(
        "--exclude-label",
        help="apply CTest's -LE filter to the regex selection",
    )
    parser.add_argument(
        "--shard-index",
        type=int,
        help="select this one-based residue from the regex selection",
    )
    parser.add_argument(
        "--shard-count",
        type=int,
        help="partition the regex selection into this many residues",
    )
    parser.add_argument(
        "--memcheck",
        action="store_true",
        help="run the exact selection through CTest's MemCheck dashboard step",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        help="set CTest's per-test timeout in seconds",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="validate and print the exact selected test count without executing",
    )
    args = parser.parse_args()

    if args.jobs <= 0:
        fail("--jobs must be a positive integer")
    if args.timeout is not None and args.timeout <= 0:
        fail("--timeout must be a positive integer")
    if (args.shard_index is None) != (args.shard_count is None):
        fail("--shard-index and --shard-count must be specified together")
    if args.shard_count is not None:
        if args.shard_count <= 0:
            fail("--shard-count must be a positive integer")
        if args.shard_index <= 0 or args.shard_index > args.shard_count:
            fail("--shard-index must be in the range 1..--shard-count")
        if not args.include_regex:
            fail("sharding requires --include-regex")
    if args.exclude_label and not args.include_regex:
        fail("--exclude-label requires --include-regex")
    if args.expected_absent_manifest is not None and args.manifest is None:
        fail("--expected-absent-manifest requires --manifest")
    if args.manifest is None and not args.include_regex:
        fail("at least one of --manifest or --include-regex is required")

    test_dir = args.test_dir.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True) if args.manifest else None
    requested = read_manifest(manifest) if manifest else []
    expected_absent_manifest = (
        args.expected_absent_manifest.resolve(strict=True)
        if args.expected_absent_manifest
        else None
    )
    expected_absent = (
        read_manifest(expected_absent_manifest)
        if expected_absent_manifest
        else []
    )
    requested_set = set(requested)
    expected_absent_set = set(expected_absent)
    undeclared_absences = sorted(expected_absent_set - requested_set)
    if undeclared_absences:
        fail(
            "expected-absent tests are not present in the primary manifest:\n  "
            + "\n  ".join(undeclared_absences)
        )
    # CTest applies CTEST_CUSTOM_MEMCHECK_IGNORE before interpreting numeric
    # -I selectors in dashboard MemCheck mode.  Build and validate those
    # selectors against that same filtered registry; normal test indices can
    # otherwise address different tests after the ignored entries disappear.
    inventory_mode_options = ["-T", "memcheck"] if args.memcheck else []
    registry_records = ctest_records(
        args.ctest, test_dir, inventory_mode_options
    )
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

    unexpectedly_present = [
        name for name in expected_absent if name in registry
    ]
    if unexpectedly_present:
        fail(
            f"{len(unexpectedly_present)} tests declared absent are present "
            "in the configured CTest registry:\n  "
            + "\n  ".join(unexpectedly_present)
        )

    selected_manifest = [
        name for name in requested if name not in expected_absent_set
    ]
    missing = [name for name in selected_manifest if name not in registry]
    if missing:
        fail(
            f"{len(missing)} manifest tests are absent from the configured "
            "CTest registry:\n  "
            + "\n  ".join(missing)
        )

    regex_names: list[str] = []
    if args.include_regex:
        # Resolve the regex against the unexpanded registry, then construct one
        # exact numeric selector below.  Do not delegate the union to CTest:
        # released CTest versions have rejected otherwise documented -I/-R/-U
        # combinations before running any tests.
        regex_options = [*inventory_mode_options, "-R", args.include_regex]
        if args.exclude_label:
            regex_options.extend(["-LE", args.exclude_label])
        regex_options.extend(["-FA", ".*", "-FS", ".*", "-FC", ".*"])
        regex_names = ctest_inventory(args.ctest, test_dir, regex_options)
        if args.shard_index is not None:
            # Registry-index residues align generated test families that are
            # registered in equally sized blocks.  That clustered every
            # parser-heavy variant in one MemCheck job and made its wall time
            # depend on unrelated CMake registration order.  A name hash is
            # stable across registry insertions and distributes those families
            # without knowing or special-casing any test name.
            regex_names = [
                name
                for name in regex_names
                if shard_for_name(name, args.shard_count) == args.shard_index
            ]

    selected_manifest_set = set(selected_manifest)
    directly_selected = selected_manifest_set | set(regex_names)
    expected_set, dependency_support, fixture_support = (
        selection_dependency_closure(registry_records, directly_selected)
    )
    indices = sorted(registry[name] for name in expected_set)
    selector = "0,0,1," + ",".join(str(index) for index in indices) + "\n"
    with tempfile.NamedTemporaryFile(
        mode="w", prefix="rex-ctest-set-", delete=True
    ) as handle:
        handle.write(selector)
        handle.flush()
        selection_options = ["-I", handle.name]
        selected_names = ctest_inventory(
            args.ctest,
            test_dir,
            [*inventory_mode_options, *selection_options],
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
            f"manifest={len(selected_manifest)}, "
            f"expected_absent={len(expected_absent)}, "
            f"regex={len(regex_names)}, "
            f"dependency_support={len(dependency_support)}, "
            f"fixture_support={len(fixture_support)}, "
            f"union={len(expected_set)}, "
            f"source={manifest if manifest else '<none>'}, "
            "expected_absent_source="
            f"{expected_absent_manifest if expected_absent_manifest else '<none>'}",
            flush=True,
        )
        if args.dry_run:
            return 0
        command = [
            args.ctest,
            "--test-dir",
            str(test_dir),
        ]
        if args.memcheck:
            command.extend(["-T", "memcheck"])
        command.extend(
            [
                *selection_options,
                "--no-tests=error",
                "--output-on-failure",
                f"-j{args.jobs}",
            ]
        )
        if args.timeout is not None:
            command.extend(["--timeout", str(args.timeout)])
        return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
