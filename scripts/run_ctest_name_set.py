#!/usr/bin/env python3
"""Run a dependency-closed CTest name selection."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def require_fresh_memcheck_dashboard(test_dir: Path) -> None:
    testing = test_dir / "Testing"
    if (testing / "TAG").exists() or any(testing.glob("*/DynamicAnalysis.xml")):
        raise ValueError(
            "Memcheck inventory requires a fresh dashboard directory; "
            f"existing evidence in {testing} must be preserved"
        )


def verify_results(registry: dict, junit_files: list[Path]) -> list[str]:
    tests = {test["name"]: test for test in registry["tests"]}
    if len(tests) != len(registry["tests"]) or not tests:
        raise ValueError("CTest selection has duplicate or no registered tests")
    seen, problems = set(), []
    for path in junit_files:
        root = ET.parse(path).getroot()
        for suite in root.iter():
            if suite.tag not in {"testsuite", "testsuites"}:
                continue
            for field in ("failures", "errors", "skipped", "disabled"):
                value = suite.get(field)
                if value is None:
                    continue
                try:
                    count = int(value)
                except ValueError:
                    count = -1
                if count < 0:
                    problems.append(f"Invalid JUnit {field} count in {path}: {value}")
                elif count:
                    problems.append(f"Unsuccessful JUnit suite in {path}: {field}={count}")
        cases = list(root.iter("testcase"))
        if not cases:
            problems.append(f"No test results in {path}")
        report_names = set()
        for case in cases:
            name = case.get("name")
            if name not in tests:
                problems.append(f"Unregistered result: {name}")
            if name in report_names:
                problems.append(f"Duplicate result in {path}: {name}")
            report_names.add(name)
            seen.add(name)
            if case.get("status") != "run" or any(
                case.find(tag) is not None for tag in ("failure", "error", "skipped")
            ):
                problems.append(f"Unsuccessful test in {path}: {name}")
    problems.extend(f"Missing result: {name}" for name in sorted(tests.keys() - seen))
    return problems


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
    if args.jobs < 1 or not 1 <= args.shard_index <= args.shard_count:
        parser.error("jobs and shard count must be positive; shard index must be in range")

    test_dir = args.test_dir.resolve()
    if args.memcheck:
        require_fresh_memcheck_dashboard(test_dir)
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
    if not selected:
        raise SystemExit("No CTest tests selected")
    if args.dry_run:
        return 0

    indices = {test["name"]: index for index, test in enumerate(registry, 1)}
    selector = "0,0,1," + ",".join(
        str(indices[name]) for name in sorted(selected, key=indices.get)
    ) + "\n"
    reports = test_dir / "Testing"
    reports.mkdir(parents=True, exist_ok=True)
    run_dir = Path(tempfile.mkdtemp(prefix="rex-name-set-", dir=reports))
    selected_registry = {"tests": [test for test in registry if test["name"] in selected]}
    (run_dir / "registry.json").write_text(json.dumps(selected_registry, indent=2) + "\n")
    junit = run_dir / "results.junit.xml"
    print(f"CTest selection and results: {run_dir}", flush=True)
    with (run_dir / "indices.txt").open("w") as handle:
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
                "--output-junit", str(junit),
                f"-j{args.jobs}",
            ]
        )
        if args.timeout:
            command.extend(["--timeout", str(args.timeout)])
        (run_dir / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        status = subprocess.run(command).returncode
    (run_dir / "ctest-exit-code.txt").write_text(str(status) + "\n")
    try:
        problems = verify_results(selected_registry, [junit])
    except (OSError, ET.ParseError) as error:
        problems = [f"Cannot read completed CTest results: {error}"]
    if problems:
        print("CTest selection did not complete successfully:\n  "
              + "\n  ".join(problems), file=sys.stderr)
    if status < 0:
        return 128 - status
    return status if status != 0 else (1 if problems else 0)


if __name__ == "__main__":
    raise SystemExit(main())
