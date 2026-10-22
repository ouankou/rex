#!/usr/bin/env python3
"""Require the exact rejection of preserved invalid Fortran grammar specimens."""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

# Independently checked with stock LLVM 22; see legacy-test-contracts.md.
EXPECTED = {
    "R511-F2015-N2007.f90": [
        (7, 15, "expected end of statement"),
        (8, 15, "expected end of statement"),
    ],
    "R1164-F2018.f90": [
        (3, 19, "Must have INTEGER type, but is REAL(4)"),
        (3, 36, "Must have CHARACTER type, but is REAL(4)"),
    ],
    "R1166-F2018.f90": [
        (2, 25, "Must have INTEGER type, but is REAL(4)"),
        (2, 42, "Must have CHARACTER type, but is REAL(4)"),
    ],
    "R1168-F2018.f90": [
        (3, 22, "Must have INTEGER type, but is REAL(4)"),
        (3, 39, "Must have CHARACTER type, but is REAL(4)"),
    ],
    "R1169-F2018.f90": [
        (1, 15, "Team value must be of type TEAM_TYPE from module ISO_FORTRAN_ENV"),
        (2, 15, "Team value must be of type TEAM_TYPE from module ISO_FORTRAN_ENV"),
        (2, 31, "Must have INTEGER type, but is REAL(4)"),
        (2, 48, "Must have CHARACTER type, but is REAL(4)"),
    ],
    "R1179-F2018.f90": [
        (7, 10, "Lock variable must have type LOCK_TYPE from ISO_FORTRAN_ENV"),
        (8, 10, "Lock variable must have type LOCK_TYPE from ISO_FORTRAN_ENV"),
    ],
    "R1181-F2018.f90": [
        (8, 13, "Lock variable must have type LOCK_TYPE from ISO_FORTRAN_ENV"),
        (9, 12, "Lock variable must have type LOCK_TYPE from ISO_FORTRAN_ENV"),
    ],
}


def main():
    translator, specimen, *flags = sys.argv[1:]
    source = Path(specimen).resolve(strict=True)
    expected = EXPECTED[source.name]
    with tempfile.TemporaryDirectory(prefix="rex-fortran-rejection-") as name:
        work = Path(name)
        environment = dict(os.environ, ROSE_TEST_OUTPUT_DIR=str(work))
        # A sanitizer failure must never collide with the expected compiler
        # diagnostic exit 1. Memcheck independently gates all child reports.
        for variable in ("ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"):
            options = environment.get(variable, "")
            environment[variable] = options + (":" if options else "") + "exitcode=86:halt_on_error=1"
        result = subprocess.run([translator, *flags, "-c", str(source)],
                                cwd=work, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end="")
        if result.returncode != 1:
            raise RuntimeError(f"expected frontend rejection 1, got {result.returncode}")
        actual = []
        header = ("error: Could not parse " if source.name.startswith("R511-")
                  else "error: Semantic errors in ") + str(source)
        for line in result.stdout.splitlines():
            diagnostic = re.fullmatch(r"(.+):(\d+):(\d+): error: (.*)", line)
            if diagnostic:
                path, row, column, message = diagnostic.groups()
                if path != str(source):
                    raise RuntimeError(f"unexpected diagnostic source: {line}")
                actual.append((int(row), int(column), message))
            elif "error:" in line.lower() and line != header:
                raise RuntimeError(f"unexpected failure: {line}")
        if actual != expected:
            raise RuntimeError(f"expected {expected!r}, got {actual!r}")
        artifacts = list(work.iterdir())
        if artifacts:
            raise RuntimeError(f"failed frontend produced files: {artifacts!r}")


if __name__ == "__main__":
    main()
