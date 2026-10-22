#!/usr/bin/env python3
"""Valgrind selection must reject missing, stale, and invalid executables."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile


def main():
    root, cmake = Path(sys.argv[1]), sys.argv[2]
    source = root.read_text()
    begin = source.index("if(VALGRIND_BIN_PATH)\n")
    end = source.index("if(VALGRIND_BINARY AND NOT VALGRIND_INCLUDE_PATH)", begin)
    selection = source[begin:end]
    with tempfile.TemporaryDirectory(prefix="rex-valgrind-selection-") as temp:
        work = Path(temp)
        available = work / "available"
        available.mkdir()
        checker = available / "valgrind"
        checker.write_text("#!/bin/sh\necho valgrind-3.25.1.GIT\n")
        checker.chmod(0o755)
        invalid_version = work / "invalid-version"
        invalid_version.mkdir()
        fake_checker = invalid_version / "valgrind"
        fake_checker.write_text("#!/bin/sh\necho not-valgrind\n")
        fake_checker.chmod(0o755)
        invalid = work / "directory-candidate"
        (invalid / "valgrind").mkdir(parents=True)
        nonexec = work / "nonexecutable"
        nonexec.mkdir()
        (nonexec / "valgrind").write_text("not executable\n")
        target_root = work / "target-root"
        target_root.mkdir()
        script = work / "probe.cmake"
        script.write_text(
            'cmake_minimum_required(VERSION 3.25)\n'
            'if(NOT DEFINED ROSE_VALGRIND_REQUESTED)\n'
            '  set(ROSE_VALGRIND_REQUESTED TRUE)\n'
            'endif()\n' + selection +
            'file(WRITE "${RESULT_FILE}" "${VALGRIND_BINARY}")\n'
        )
        for name, directory, cached, succeeds, requested in (
            ("explicit", available, None, True, True),
            ("explicit-over-cache", available, "/obsolete/valgrind", True, True),
            ("missing", work / "missing", None, False, True),
            ("missing-with-cache", work / "missing", checker, False, True),
            ("directory", invalid, None, False, True),
            ("nonexecutable", nonexec, None, False, True),
            ("invalid-version", invalid_version, None, False, True),
            ("implicit", None, None, True, True),
            ("auto-valid", None, None, True, False),
            ("auto-invalid-version", None, None, False, False),
        ):
            result_file = work / (name + ".result")
            search_dir = available
            if name == "auto-invalid-version":
                search_dir = invalid_version
            command = [cmake, f"-DCMAKE_PROGRAM_PATH={search_dir}",
                       f"-DROSE_VALGRIND_REQUESTED:BOOL={'ON' if requested else 'OFF'}",
                       f"-DRESULT_FILE={result_file}"]
            if name == "implicit":
                command.extend((f"-DCMAKE_FIND_ROOT_PATH={target_root}",
                                "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY"))
            if directory is not None:
                command.append(f"-DVALGRIND_BIN_PATH={directory}")
            if cached is not None:
                command.append(f"-DVALGRIND_BINARY:FILEPATH={cached}")
            result = subprocess.run(
                command + ["-P", str(script)],
                capture_output=True,
                text=True,
                timeout=20,
                env=dict(
                    os.environ,
                    PATH=str(search_dir) + os.pathsep + os.environ["PATH"],
                ),
            )
            assert (result.returncode == 0) == succeeds, (
                name, result.stdout, result.stderr
            )
            if succeeds:
                assert result_file.read_text() == str(checker), name
            else:
                assert not result_file.exists(), name
                if name in ("invalid-version", "auto-invalid-version"):
                    assert "valid --version result" in result.stderr, result.stderr
                elif name != "nonexecutable":
                    assert "explicitly selected Valgrind directory" in result.stderr, (
                        result.stderr
                    )
            print(name, "passed")


if __name__ == "__main__":
    main()
