#!/usr/bin/env python3
"""Validate cached zlib prefix changes and the imported header/link interface."""

from pathlib import Path
import subprocess
import sys
import tempfile


module, cmake, compiler, include, library = map(Path, sys.argv[1:6])
library_name = sys.argv[6]
with tempfile.TemporaryDirectory(prefix="rex-zlib-prefix-") as directory:
    root = Path(directory)
    (root / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.15)\n'
        'project(RexZlibPrefix C)\n'
        f'include("{module}")\n'
        '# LLVM may request the same dependency after REX selects its prefix.\n'
        'find_package(ZLIB REQUIRED)\n'
        'add_executable(probe probe.c)\n'
        'target_link_libraries(probe PRIVATE ZLIB::ZLIB)\n'
        'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/selected.txt"\n'
        '  CONTENT "${ZLIB_INCLUDE_DIR}\\n$<TARGET_FILE:ZLIB::ZLIB>\\n")\n'
    )
    (root / "probe.c").write_text(
        '#include <string.h>\n#include <zlib.h>\n'
        'int main(void) { return strcmp(zlibVersion(), ZLIB_VERSION) != 0; }\n'
    )
    build = root / "build"

    def configure(*options, expected=None, error=None):
        result = subprocess.run(
            [str(cmake), "-S", str(root), "-B", str(build),
             f"-DCMAKE_C_COMPILER={compiler}", *options],
            capture_output=True, text=True,
        )
        output = result.stdout + result.stderr
        if error is not None:
            assert result.returncode != 0 and error in " ".join(output.split()), output
            return
        assert result.returncode == 0, output
        selected = tuple(map(Path, (build / "selected.txt").read_text().splitlines()))
        assert selected == expected, (selected, expected)
        subprocess.run([str(cmake), "--build", str(build)], check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run([str(build / "probe")], check=True)

    configure(f"-DZLIB_INCLUDE_DIR={include}", f"-DZLIB_LIBRARY={library}",
              expected=(include, library))
    configure(f"-DZLIB_ROOT={root / 'missing'}", error="but not found")
    for name, headers, binary in (("headers only", True, False),
                                  ("library only", False, True),
                                  ("first prefix", True, True),
                                  ("second prefix", True, True)):
        prefix = root / name
        if headers:
            (prefix / "include").mkdir(parents=True)
            for header in ("zlib.h", "zconf.h"):
                (prefix / "include" / header).symlink_to(include / header)
        if binary:
            (prefix / "lib").mkdir(parents=True)
            (prefix / "lib" / library_name).symlink_to(library)
        configure(f"-DZLIB_ROOT={prefix}",
                  expected=(prefix / "include", prefix / "lib" / library_name),
                  error=None if headers and binary else "but not found")
    configure(f"-DZLIB_ROOT={root / 'still missing'}", error="but not found")
    configure("-DZLIB_ROOT=no", error="cannot be disabled")
    configure("-UZLIB_ROOT", f"-DZLIB_INCLUDE_DIR={include}",
              f"-DZLIB_LIBRARY={library}", expected=(include, library))
print("Zlib prefixes select matching headers/libraries and reject incomplete installations")
