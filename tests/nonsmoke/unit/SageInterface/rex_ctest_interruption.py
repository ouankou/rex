#!/usr/bin/env python3
"""Interrupt a real CTest run and require the selection runner to reject it."""

import argparse
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runner", "cmake", "ctest", "work"):
        parser.add_argument(name)
    args = parser.parse_args()
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    source = work / "source"
    source.mkdir(parents=True)
    ready = work / "ready.txt"
    (source / "rex_ctest_wait.py").write_text('''import os
from pathlib import Path
import sys
import time
if sys.argv[2] == "ON":
    ready = Path(sys.argv[1])
    pending = ready.with_suffix(".pending")
    pending.write_text(f"{os.getppid()} {os.getpid()}")
    pending.replace(ready)
    time.sleep(60)
''')
    (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.15)
project(REXCtestInterruption NONE)
enable_testing()
add_test(NAME rex_ctest_complete COMMAND "${CMAKE_COMMAND}" -E true)
add_test(NAME rex_ctest_interrupted COMMAND "${PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/rex_ctest_wait.py" "${READY}" "${WAIT_FOR_INTERRUPT}")
''')
    build = work / "build"
    configure = [args.cmake, "-S", str(source), "-B", str(build),
                 "-DPYTHON=" + sys.executable, "-DREADY=" + str(ready)]
    subprocess.run(configure + ["-DWAIT_FOR_INTERRUPT=ON"], check=True,
                   stdout=subprocess.DEVNULL)
    command = [sys.executable, args.runner, "--ctest", args.ctest,
               "--test-dir", str(build), "--include-regex", "^rex_ctest_",
               "--jobs", "1"]
    # A successful report from an older run must not rescue this interruption.
    testing = build / "Testing"
    testing.mkdir()
    (testing / "stale.junit.xml").write_text(
        '<testsuite><testcase name="rex_ctest_complete"/>'
        '<testcase name="rex_ctest_interrupted"/></testsuite>')
    adapter = work / "rex_ctest_zero_exit.py"
    adapter.write_text(
        "#!" + sys.executable + "\nimport os, subprocess, sys\n"
        + "ctest = " + repr(args.ctest) + "\n"
        + "if '--show-only=json-v1' in sys.argv:\n"
        + "    os.execv(ctest, [ctest, *sys.argv[1:]])\n"
        + "subprocess.run([ctest, *sys.argv[1:]])\n"
        + "raise SystemExit(0)\n")
    adapter.chmod(0o755)
    # Direct CTest cancellation must fail. The injected zero-exit adapter also
    # proves that command status cannot substitute for complete fresh results.
    for mode, ctest_command in (("direct", args.ctest),
                                ("zero-exit-adapter", str(adapter))):
        ready.unlink(missing_ok=True)
        log_path = work / (mode + ".log")
        with log_path.open("w") as log:
            process = subprocess.Popen(command + ["--ctest", ctest_command],
                                       stdout=log, stderr=subprocess.STDOUT)
            ctest_pid = child_pid = None
            try:
                deadline = time.monotonic() + 15
                while not ready.exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.05)
                assert ready.exists(), "CTest never started the interruption control"
                ctest_pid, child_pid = map(int, ready.read_text().split())
                os.kill(ctest_pid, signal.SIGTERM)
                status = process.wait(timeout=15)
            finally:
                if process.poll() is None:
                    if ctest_pid is not None:
                        try:
                            os.kill(ctest_pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    process.kill()
                    process.wait()
                if child_pid is not None:
                    try:
                        os.kill(child_pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
        (work / (mode + "-exit-code.txt")).write_text(str(status) + "\n")
        assert status != 0, log_path.read_text()
    subprocess.run(configure + ["-DWAIT_FOR_INTERRUPT=OFF"], check=True,
                   stdout=subprocess.DEVNULL)
    with (work / "complete.log").open("w") as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    with (work / "empty.log").open("w") as log:
        empty = subprocess.run(command + ["--include-regex", "^no_such_test$"],
                               stdout=log, stderr=subprocess.STDOUT)
    assert empty.returncode != 0, "An empty selection must not run the whole suite"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
