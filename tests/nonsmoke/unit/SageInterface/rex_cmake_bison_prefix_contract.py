#!/usr/bin/env python3
"""Changing an explicit Bison prefix must not reuse an unrelated cached tool."""

from pathlib import Path
import subprocess
import sys
import tempfile


module, cmake, bison = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-bison-prefix-") as directory:
    root = Path(directory)
    (root / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.15)\n'
        'project(RexBisonPrefix NONE)\n'
        f'include("{module}")\n'
        'if(NOT COMMAND BISON_TARGET OR NOT BISON_FOUND)\n'
        '  message(FATAL_ERROR "Bison generation API is missing")\n'
        'endif()\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${BISON_EXECUTABLE}")\n'
    )
    build = root / "build"

    def configure(*options, expected=None, error=None):
        result = subprocess.run([str(cmake), "-S", str(root), "-B", str(build),
                                 *options], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if error is not None:
            assert result.returncode != 0 and error in " ".join(output.split()), output
        else:
            assert result.returncode == 0, output
            assert Path((build / "selected.txt").read_text()) == expected

    configure("-DBISON_EXECUTABLE=" + str(bison), expected=bison)
    configure("-DBISON_ROOT=" + str(root / "missing"), error="but not found")
    for name in ("first prefix", "second prefix"):
        prefix = root / name
        (prefix / "bin").mkdir(parents=True)
        executable = prefix / "bin/bison"
        executable.symlink_to(bison)
        configure("-DBISON_ROOT=" + str(prefix), expected=executable)
    configure("-DBISON_ROOT=" + str(root / "still missing"), error="but not found")
    configure("-DBISON_ROOT=no", error="cannot be disabled")
    configure("-UBISON_ROOT", "-DBISON_EXECUTABLE=" + str(bison), expected=bison)
print("Bison prefix changes select the requested tool and reject missing prefixes")
