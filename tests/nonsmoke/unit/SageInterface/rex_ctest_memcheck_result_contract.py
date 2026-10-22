#!/usr/bin/env python3
"""Exercise CTest's memory-defect gate independently of command success rules."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("cmake", "ctest", "compiler", "wrapper", "source", "work"):
        parser.add_argument(name)
    args = parser.parse_args()
    source = Path(args.source)
    work = Path(args.work)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    probe = work / "rex_ctest_memcheck_child"
    subprocess.run(
        [args.compiler, "-std=c11", "-O0", str(Path(__file__).with_name(
            "rex_ctest_valgrind_child_report.c")), "-o", str(probe)], check=True
    )
    clean_source = work / "rex_ctest_memcheck_clean.c"
    clean_source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
    clean = work / "rex_ctest_memcheck_clean"
    subprocess.run([args.compiler, str(clean_source), "-o", str(clean)], check=True)
    project = work / "project"
    project.mkdir()
    (project / "CMakeLists.txt").write_text(
        """cmake_minimum_required(VERSION 3.20)
project(REXMemcheckResultContract NONE)
set(MEMORYCHECK_TYPE Valgrind CACHE STRING "Memory checker" FORCE)
set(MEMORYCHECK_COMMAND "${WRAPPER}" CACHE FILEPATH "Memory checker" FORCE)
set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --error-exitcode=79" CACHE STRING "Options" FORCE)
include(CTest)
add_test(NAME ignored_child_error COMMAND "${PROBE}")
set_tests_properties(ignored_child_error PROPERTIES PASS_REGULAR_EXPRESSION "^")
add_test(NAME clean_command COMMAND "${CLEAN}")
if(ENABLE_GATE)
  set(ROSE_CTEST_MEMCHECK_COMMAND "${WRAPPER}")
  set(REX_TEST_PYTHON_EXECUTABLE "${PYTHON}")
  set(ROSE_TOP_SRC_DIR "${REX_SOURCE}")
  configure_file("${REX_SOURCE}/cmake/CTestCustom.cmake.in"
    "${CMAKE_BINARY_DIR}/CTestCustom.cmake" @ONLY)
endif()
""", encoding="utf-8"
    )
    for mode in ("unhooked", "gated"):
        build = work / mode
        gate_options = []
        if mode == "gated":
            gate_options = ["-DPYTHON=" + sys.executable,
                            "-DREX_SOURCE=" + str(source)]
        subprocess.run([
            args.cmake, "-S", str(project), "-B", str(build),
            "-DWRAPPER=" + args.wrapper, "-DPROBE=" + str(probe),
            "-DCLEAN=" + str(clean), *gate_options,
            "-DENABLE_GATE=" + ("ON" if mode == "gated" else "OFF"),
        ], check=True, stdout=subprocess.DEVNULL)
        command = [args.ctest, "--test-dir", str(build), "-T", "memcheck",
                   "--no-tests=error", "--output-on-failure"]
        result = subprocess.run(command + ["-R", "^ignored_child_error$"],
                                capture_output=True, text=True)
        (build / "negative.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == (16 if mode == "gated" else 0), result
        tag = (build / "Testing/TAG").read_text().splitlines()[0]
        report = build / "Testing" / tag / "DynamicAnalysis.xml"
        analysis = ET.parse(report).getroot().find("DynamicAnalysis")
        test = analysis.find("Test")
        assert test.get("Status") == "passed", ET.tostring(test)
        assert sum(int(d.text) for d in test.findall("Results/Defect")) > 0
        failed_report = report.read_bytes()
        (build / "negative.DynamicAnalysis.xml").write_bytes(failed_report)
        if mode == "gated":
            assert "Memcheck defects remain" in result.stdout + result.stderr
            # The selection runner must reject inventory before CTest can
            # overwrite an existing failed dashboard.
            rejected = subprocess.run([
                sys.executable, str(source / "scripts/run_ctest_name_set.py"),
                "--test-dir", str(build), "--include-regex", "^clean_command$",
                "--jobs", "1", "--memcheck", "--dry-run",
            ], capture_output=True, text=True)
            assert rejected.returncode != 0, rejected
            assert "existing evidence" in rejected.stderr, rejected
            assert report.read_bytes() == failed_report
            subprocess.run(command + ["-R", "^clean_command$"], check=True,
                           stdout=subprocess.DEVNULL)
            (build / "clean.DynamicAnalysis.xml").write_bytes(report.read_bytes())
            inventory = subprocess.run(command + ["--show-only=json-v1"], check=True,
                                       capture_output=True, text=True)
            assert {t["name"] for t in json.loads(inventory.stdout)["tests"]} == {
                "ignored_child_error", "clean_command"
            }
            # Missing or incomplete evidence must fail without accepting an
            # older successful dashboard. These are private contract artifacts.
            for invalid in (None, "<Site><DynamicAnalysis/></Site>"):
                if invalid is None:
                    report.unlink()
                else:
                    report.write_text(invalid, encoding="utf-8")
                result = subprocess.run([sys.executable,
                    str(source / "scripts/check_memcheck_results.py"), str(build)],
                    capture_output=True, text=True)
                assert result.returncode == 1, result
                assert "Cannot validate Memcheck results" in result.stderr
    return 0


if __name__ == "__main__":
    sys.exit(main())
