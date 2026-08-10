#!/usr/bin/env python3
"""Run the three local Cxx_Grammar performance contracts sequentially."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


TESTS = (
    "rose_example_src_frontend_SageIII_Cxx_Grammar_C",
    "astQuery_test3_cxx_grammar",
    "merge_traversal_cxx_grammar",
)


@dataclass(frozen=True)
class RegisteredContract:
    command: tuple[str, ...]
    working_directory: Path
    environment: dict[str, str]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the representative, AST-query, and merge Cxx_Grammar tests "
            "without concurrent resource contention. This harness is local-only "
            "and is intentionally not registered in hosted CI."
        )
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "build",
        help="configured CMake build directory (default: %(default)s)",
    )
    parser.add_argument(
        "--limit-seconds",
        type=float,
        default=180.0,
        help="strict per-test wall-clock upper bound (default: %(default)s)",
    )
    parser.add_argument(
        "--test",
        choices=TESTS,
        action="append",
        dest="selected_tests",
        help="run only this contract; repeat to select more than one",
    )
    return parser.parse_args()


def require_registered(build_dir: Path, test_name: str) -> RegisteredContract:
    result = subprocess.run(
        [
            "ctest",
            "--test-dir",
            str(build_dir),
            "--show-only=json-v1",
            "-R",
            f"^{test_name}$",
        ],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    report = json.loads(result.stdout)
    tests = report.get("tests", [])
    if len(tests) != 1 or tests[0].get("name") != test_name:
        raise RuntimeError(
            f"performance contract {test_name!r} is not registered exactly once:\n"
            f"{result.stdout}"
        )
    test = tests[0]
    command = tuple(test.get("command", ()))
    properties = {
        property_entry["name"]: property_entry["value"]
        for property_entry in test.get("properties", ())
    }
    working_directory = Path(properties.get("WORKING_DIRECTORY", build_dir))
    if not command or not working_directory.is_dir():
        raise RuntimeError(
            f"performance contract {test_name!r} has an incomplete command "
            "or working directory"
        )
    if "ENVIRONMENT_MODIFICATION" in properties:
        raise RuntimeError(
            f"performance contract {test_name!r} uses unsupported CTest "
            "environment modifications"
        )
    environment = os.environ.copy()
    for assignment in properties.get("ENVIRONMENT", ()):
        if "=" not in assignment:
            raise RuntimeError(
                f"performance contract {test_name!r} has malformed CTest "
                f"environment entry {assignment!r}"
            )
        key, value = assignment.split("=", 1)
        environment[key] = value
    return RegisteredContract(command, working_directory, environment)


def run_contract(
    test_name: str, registration: RegisteredContract, limit_seconds: float
) -> float:
    started = time.monotonic()
    process = subprocess.Popen(
        registration.command,
        cwd=registration.working_directory,
        env=registration.environment,
        start_new_session=True,
    )
    try:
        status = process.wait(timeout=limit_seconds)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=15.0)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        raise RuntimeError(
            f"{test_name} reached the strict {limit_seconds:.2f}s limit"
        )
    elapsed = time.monotonic() - started
    if status != 0:
        raise RuntimeError(f"{test_name} failed with status {status}")
    if elapsed >= limit_seconds:
        raise RuntimeError(
            f"{test_name} took {elapsed:.2f}s; required < {limit_seconds:.2f}s"
        )
    return elapsed


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    if args.limit_seconds <= 0:
        raise ValueError("--limit-seconds must be positive")
    if not (build_dir / "CTestTestfile.cmake").is_file():
        raise RuntimeError(f"{build_dir} is not a configured CTest build tree")

    selected_tests = tuple(args.selected_tests or TESTS)
    registrations = {
        test_name: require_registered(build_dir, test_name)
        for test_name in selected_tests
    }

    results: list[tuple[str, float]] = []
    for test_name in selected_tests:
        print(
            f"RUN {test_name} (strict limit {args.limit_seconds:.2f}s)",
            flush=True,
        )
        elapsed = run_contract(
            test_name, registrations[test_name], args.limit_seconds
        )
        results.append((test_name, elapsed))
        print(f"PASS {test_name} {elapsed:.2f}s", flush=True)

    worst_name, worst_elapsed = max(results, key=lambda result: result[1])
    print("SUMMARY")
    for test_name, elapsed in results:
        print(f"  {test_name}: {elapsed:.2f}s")
    print(
        f"  worst: {worst_name} {worst_elapsed:.2f}s "
        f"(< {args.limit_seconds:.2f}s)"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
