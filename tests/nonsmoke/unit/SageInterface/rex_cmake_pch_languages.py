#!/usr/bin/env python3
"""Build and run C, C++, and mixed-language consumers of the public C++ PCH."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command!r}\n"
                           f"{result.stdout}")


def main():
    module, cmake, c_compiler, cxx_compiler = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="rex_pch_languages_") as name:
        root = Path(name)
        consumer = root / "consumer"
        consumer.mkdir()
        (root / "pch.hpp").write_text("#pragma once\nnamespace rex { constexpr int value = 17; }\n")
        (root / "provider.cpp").write_text("#include \"pch.hpp\"\n")
        (consumer / "value.c").write_text("int rex_value(void) { return 17; }\n")
        (consumer / "main.cpp").write_text(
            "#include \"pch.hpp\"\nextern \"C\" int rex_value(void);\n"
            "int main() { return rex_value() == rex::value ? 0 : 1; }\n")
        (consumer / "c_main.c").write_text("int main(void) { return 0; }\n")
        (consumer / "CMakeLists.txt").write_text("""add_executable(mixed main.cpp value.c)
target_include_directories(mixed PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(c_only c_main.c)
""")
        (root / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.16)
project(rex_pch_languages LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
include([=[{Path(module).resolve()}]=])
add_library(provider OBJECT provider.cpp)
rose_enable_private_pch(provider "${{CMAKE_CURRENT_SOURCE_DIR}}/pch.hpp")
add_subdirectory(consumer)
rose_reuse_pch(mixed provider)
rose_reuse_pch(c_only provider)
""")
        for mode in ("ON", "OFF"):
            build = root / mode
            run([cmake, "-S", str(root), "-B", str(build),
                 f"-DCMAKE_C_COMPILER={c_compiler}",
                 f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
                 f"-DROSE_ENABLE_PCH={mode}"])
            run([cmake, "--build", str(build), "--parallel", "2"])
            run([str(build / "consumer/mixed")])
            run([str(build / "consumer/c_only")])


if __name__ == "__main__":
    main()
