#!/usr/bin/env python3
"""Make recorded memory defects fatal independently of CTest output matching."""

import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def check_results(build_directory):
    testing = build_directory / "Testing"
    tag = (testing / "TAG").read_text(encoding="utf-8").splitlines()[0]
    report = testing / tag / "DynamicAnalysis.xml"
    analysis = ET.parse(report).getroot().find("DynamicAnalysis")
    if analysis is None or analysis.find("EndTestTime") is None:
        raise ValueError(f"incomplete Memcheck report: {report}")
    failures = []
    for test in analysis.findall("Test"):
        for defect in test.findall("Results/Defect"):
            count = int(defect.text)
            if count < 0:
                raise ValueError(f"negative Memcheck defect count: {report}")
            if count:
                failures.append(
                    f"{test.findtext('Name')}: {count} {defect.get('type')}"
                )
    if failures:
        print("Memcheck defects remain:\n" + "\n".join(failures), file=sys.stderr)
        return 1
    # CTest also invokes the hook for a show-only inventory. Stay silent so its
    # JSON remains machine-readable; an empty inventory is not test acceptance.
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_directory", type=Path)
    args = parser.parse_args()
    try:
        return check_results(args.build_directory)
    except (OSError, ValueError, TypeError, IndexError, ET.ParseError) as error:
        print(f"Cannot validate Memcheck results: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
