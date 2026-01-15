#!/usr/bin/env python3
import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def extract_passed(junit_path: Path, output_path: Path) -> int:
    tree = ET.parse(junit_path)
    root = tree.getroot()
    passed = []
    for case in root.iter("testcase"):
        if case.find("failure") is not None:
            continue
        if case.find("error") is not None:
            continue
        if case.find("skipped") is not None:
            continue
        name = case.get("name")
        if name:
            passed.append(name)
    unique = sorted(set(passed))
    output_path.write_text("\n".join(unique) + ("\n" if unique else ""))
    return len(unique)


def indices_from_names(tests_json: Path, names_path: Path, output_path: Path) -> int:
    names = {line.strip() for line in names_path.read_text().splitlines() if line.strip()}
    if not names:
        raise ValueError("No test names provided.")

    data = json.loads(tests_json.read_text())
    tests = data.get("tests", [])
    indices = []
    missing = set(names)
    for index, test in enumerate(tests, start=1):
        name = test.get("name")
        if name in names:
            indices.append(index)
            missing.discard(name)

    if not indices:
        raise ValueError("No matching tests found in build test list.")

    if missing:
        sys.stderr.write(
            f"Warning: {len(missing)} tests not found in this build; skipping them.\n"
        )
    output_path.write_text("0,0,1," + ",".join(str(i) for i in indices) + "\n")
    return len(indices)


def main() -> int:
    parser = argparse.ArgumentParser(description="CTest selection helpers.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    passed_parser = subparsers.add_parser(
        "extract-passed", help="Extract passed tests from a CTest JUnit file."
    )
    passed_parser.add_argument("--junit", required=True, type=Path)
    passed_parser.add_argument("--out", required=True, type=Path)

    index_parser = subparsers.add_parser(
        "indices-from-names", help="Generate a ctest -I file from test names."
    )
    index_parser.add_argument("--tests-json", required=True, type=Path)
    index_parser.add_argument("--names", required=True, type=Path)
    index_parser.add_argument("--out", required=True, type=Path)

    args = parser.parse_args()
    if args.command == "extract-passed":
        count = extract_passed(args.junit, args.out)
        print(f"Wrote {count} passed tests to {args.out}")
        return 0
    if args.command == "indices-from-names":
        count = indices_from_names(args.tests_json, args.names, args.out)
        print(f"Wrote {count} test indices to {args.out}")
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
