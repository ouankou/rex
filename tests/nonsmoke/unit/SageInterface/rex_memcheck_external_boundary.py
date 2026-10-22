#!/usr/bin/env python3
"""Require caller-owned undefined values and leaks to survive external filtering."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("compiler", "wrapper", "suppressions", "work"):
        parser.add_argument(name)
    parser.add_argument("memcheck_options", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    source = Path(__file__).resolve().parent
    # This isolated DSO deliberately matches the installed-library pattern.
    # Its two frames consume storage allocated by the caller. It is a filter
    # negative control, not a reproduction of a defect in Clang itself.
    library = work / "libclang-cpp.so.rex_audit_control"
    driver = work / "rex_memcheck_boundary_driver"
    subprocess.run([args.compiler, "-g", "-O0", "-fPIC", "-shared",
                    str(source / "rex_memcheck_boundary_library.c"),
                    "-o", str(library)], check=True)
    subprocess.run([args.compiler, "-g", "-O0",
                    str(source / "rex_memcheck_boundary_driver.c"),
                    str(library), "-o", str(driver)], check=True)
    for initialized in (False, True):
        log = work / ("defined.log" if initialized else "undefined.log")
        result = subprocess.run([
            args.wrapper, *args.memcheck_options,
            "--track-origins=yes", "--error-exitcode=79",
            "--suppressions=" + args.suppressions, "--log-file=" + str(log),
            str(driver), *(["defined"] if initialized else []),
        ], capture_output=True, text=True)
        log.with_suffix(".stdout").write_text(result.stdout + result.stderr)
        report = log.read_text()
        assert result.returncode == (0 if initialized else 79), (
            result.returncode, report, result.stdout, result.stderr)
        if initialized:
            assert "ERROR SUMMARY: 0 errors from 0 contexts" in report, report
        else:
            assert "Conditional jump or move depends on uninitialised value(s)" in report, report
            assert "Uninitialised value was created by a heap allocation" in report, report
            assert "rex_memcheck_boundary_driver.c" in report, report
            assert "rex_memcheck_boundary_library.c" in report, report
    leak_driver = work / "rex_memcheck_boundary_leak"
    subprocess.run([args.compiler, "-g", "-O0",
                    str(source / "rex_memcheck_boundary_leak.c"),
                    "-o", str(leak_driver)], check=True)
    # Executable names do not identify external allocation ownership. These
    # byte-identical callers used to disappear under wildcard utility masks.
    for name in ("rex_memcheck_boundary_leak", "bash", "dash", "grep", "cmp",
                 "mktemp", "gnumktemp", "awk", "mawk", "gawk", "find", "git",
                 "clang", "clang++"):
        program = work / name
        if program != leak_driver:
            shutil.copy2(leak_driver, program)
        for released in (False, True):
            log = work / ("leak-" + name + ("-released" if released else "") + ".log")
            result = subprocess.run([
                args.wrapper, *args.memcheck_options,
                "--leak-check=full", "--show-leak-kinds=all",
                "--errors-for-leak-kinds=definite,indirect,possible",
                "--error-exitcode=79", "--suppressions=" + args.suppressions,
                "--log-file=" + str(log), str(program),
                *(["free"] if released else []),
            ], capture_output=True, text=True)
            log.with_suffix(".stdout").write_text(result.stdout + result.stderr)
            report = log.read_text()
            assert result.returncode == (0 if released else 79), (
                result.returncode, report, result.stdout, result.stderr)
            if released:
                assert "ERROR SUMMARY: 0 errors from 0 contexts" in report, report
            else:
                assert "317 bytes in 1 blocks are definitely lost" in report, report
                assert "rex_memcheck_boundary_leak.c" in report, report
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
