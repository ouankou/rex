#!/usr/bin/env python3
"""Run a dependency-closed CTest name selection."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def ctest_records(ctest: str, test_dir: Path, options: list[str]) -> list[dict]:
    output = subprocess.check_output(
        [
            ctest,
            "--test-dir",
            str(test_dir),
            "-N",
            "--show-only=json-v1",
            *options,
        ],
        text=True,
    )
    return json.loads(output)["tests"]


def manifest_names(path: Path) -> set[str]:
    return {
        line
        for raw_line in path.read_text().splitlines()
        if (line := raw_line.strip()) and not line.startswith("#")
    }


def property_values(test: dict, name: str) -> set[str]:
    return {
        value
        for prop in test.get("properties", [])
        if prop.get("name") == name
        for value in prop.get("value", [])
    }


def dependency_closure(registry: list[dict], names: set[str]) -> set[str]:
    by_name = {test["name"]: test for test in registry}
    selected = set(names)

    while True:
        fixtures = set().union(
            *(property_values(by_name[name], "FIXTURES_REQUIRED") for name in selected)
        )
        support = {
            test["name"]
            for test in registry
            if fixtures
            & (
                property_values(test, "FIXTURES_SETUP")
                | property_values(test, "FIXTURES_CLEANUP")
            )
        }
        dependencies = set().union(
            *(property_values(by_name[name], "DEPENDS") for name in selected)
        )
        expanded = selected | support | dependencies
        if expanded == selected:
            return selected
        selected = expanded


def shard(name: str, count: int) -> int:
    digest = hashlib.sha256(name.encode()).digest()
    return int.from_bytes(digest[:8], "big") % count + 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--test-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--include-regex")
    parser.add_argument("--exclude-label")
    parser.add_argument("--shard-index", type=int, default=1)
    parser.add_argument("--shard-count", type=int, default=1)
    parser.add_argument("--jobs", type=int, required=True)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--memcheck", action="store_true")
    parser.add_argument("--timeout", type=int)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    test_dir = args.test_dir.resolve()
    inventory_options = ["-T", "memcheck"] if args.memcheck else []
    registry = ctest_records(args.ctest, test_dir, inventory_options)

    direct = set()
    if args.manifest:
        requested = manifest_names(args.manifest)
        missing = requested - {test["name"] for test in registry}
        if missing:
            raise SystemExit(
                "Manifest tests are not registered:\n  "
                + "\n  ".join(sorted(missing))
            )
        direct.update(requested)

    if args.include_regex:
        regex_options = [*inventory_options, "-R", args.include_regex]
        if args.exclude_label:
            regex_options.extend(["-LE", args.exclude_label])
        regex_options.extend(["-FA", ".*", "-FS", ".*", "-FC", ".*"])
        direct.update(
            test["name"]
            for test in ctest_records(args.ctest, test_dir, regex_options)
            if shard(test["name"], args.shard_count) == args.shard_index
        )

    selected = dependency_closure(registry, direct)
    print(f"Selected {len(selected)} CTest tests", flush=True)
    if args.dry_run:
        return 0

    indices = {test["name"]: index for index, test in enumerate(registry, 1)}
    selector = "0,0,1," + ",".join(
        str(indices[name]) for name in sorted(selected, key=indices.get)
    ) + "\n"
    with tempfile.NamedTemporaryFile(mode="w", prefix="rex-ctest-set-") as handle:
        handle.write(selector)
        handle.flush()
        command = [args.ctest, "--test-dir", str(test_dir)]
        if args.memcheck:
            command.extend(["-T", "memcheck"])
        command.extend(
            [
                "-I",
                handle.name,
                "--no-tests=error",
                "--output-on-failure",
                f"-j{args.jobs}",
            ]
        )
        if args.timeout:
            command.extend(["--timeout", str(args.timeout)])
        return subprocess.run(command).returncode


if __name__ == "__main__":
    raise SystemExit(main())
