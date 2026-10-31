#!/usr/bin/env python3
"""Exercise real compiler links while changing REX linker policy in one tree."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(command, expected=0, env=None):
    result = subprocess.run(command, text=True, capture_output=True, env=env)
    assert (result.returncode == 0) == (expected == 0), (
        command, result.returncode, result.stdout, result.stderr)
    return result.stdout + result.stderr


def cache_flags(build):
    lines = (build / "CMakeCache.txt").read_text().splitlines()
    return {key: next(line.split("=", 1)[1] for line in lines
                      if line.startswith(key + ":STRING="))
            for key in ("CMAKE_EXE_LINKER_FLAGS", "CMAKE_SHARED_LINKER_FLAGS",
                        "CMAKE_MODULE_LINKER_FLAGS")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("cmake", "module", "cc", "cxx"):
        parser.add_argument("--" + argument, required=True)
    parser.add_argument("--fc")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rex linker policy ") as temporary:
        root = Path(temporary)
        source, build = root / "source", root / "build"
        source.mkdir()
        compilers = []
        for index, compiler in enumerate((args.cc, args.cxx, *([args.fc] if args.fc else []))):
            wrapper = root / f"rex_linker_driver_{index}"
            wrapper.write_text(f"""#!/usr/bin/env python3
import os
from pathlib import Path
import sys
arguments = sys.argv[1:]
if '-Wl,--disable-new-dtags' in arguments:
    expected = Path({str(build / 'expected-driver-option.txt')!r}).read_text().strip()
    actual = [argument for argument in arguments if argument.startswith('-fuse-ld=')]
    if actual != ([expected] if expected else []):
        sys.exit('capability probe did not use the selected linker: ' + repr(actual))
os.execv({os.path.abspath(compiler)!r}, [{os.path.abspath(compiler)!r}, *arguments])
""")
            wrapper.chmod(0o755)
            compilers.append(str(wrapper))
        (source / "rex_linker_input.c").write_text("int main(void) { return 0; }\n")
        (source / "rex_linker_input.cpp").write_text("int main() { return 0; }\n")
        (source / "rex_linker_input.f90").write_text("program rex_linker_input\nend program\n")
        fortran = "Fortran" if args.fc else ""
        (source / "CMakeLists.txt").write_text(f"""
cmake_minimum_required(VERSION 3.18)
project(RexLinkerPolicy LANGUAGES C CXX {fortran})
include("${{REX_LINKER_MODULE}}")
rex_configure_linker()
get_directory_property(options LINK_OPTIONS)
file(WRITE "${{CMAKE_BINARY_DIR}}/selected.txt" "${{REX_SELECTED_LINKER}}\n${{options}}\n")
file(WRITE "${{CMAKE_BINARY_DIR}}/expected-driver-option.txt" "${{options}}")
if(REX_TEST_C_SANITIZERS)
  add_link_options("$<$<LINK_LANGUAGE:C,CXX>:-fsanitize=address,undefined>")
endif()
if(NOT EMSCRIPTEN)
  rex_require_linker_option("-Wl,--disable-new-dtags")
endif()
if(DEFINED REX_REQUIRED_OPTION)
  rex_require_linker_option("${{REX_REQUIRED_OPTION}}")
endif()
add_executable(rex_linker_c rex_linker_input.c)
add_executable(rex_linker_cxx rex_linker_input.cpp)
if(CMAKE_Fortran_COMPILER_LOADED)
  add_executable(rex_linker_fortran rex_linker_input.f90)
endif()
""")
        library = root / "user library directory"
        library.mkdir()
        user_flags = f'-L"{library}" -Wl,--as-needed'
        configure = [args.cmake, "-S", str(source), "-B", str(build),
                     "-DREX_LINKER_MODULE=" + args.module,
                     "-DCMAKE_C_COMPILER=" + compilers[0],
                     "-DCMAKE_CXX_COMPILER=" + compilers[1]]
        if args.fc:
            configure += ["-DCMAKE_Fortran_COMPILER=" + compilers[2]]
        for kind in ("EXE", "SHARED", "MODULE"):
            configure += [f"-DCMAKE_{kind}_LINKER_FLAGS=" + user_flags]
        run([*configure, "-DREX_LINKER=auto"])
        expected_flags = cache_flags(build)
        assert all(value == user_flags for value in expected_flags.values())
        automatic = (build / "selected.txt").read_text().splitlines()[0]
        for policy in (automatic, "system", "auto", "system"):
            run([*configure, "-DREX_LINKER=" + policy])
            assert cache_flags(build) == expected_flags
            selected, options = (build / "selected.txt").read_text().splitlines()
            assert selected == (automatic if policy == "auto" else policy)
            assert options == ("" if selected == "system" else "-fuse-ld=" + selected)
            run([args.cmake, "--build", str(build), "--parallel", "1"])
            for executable in ("rex_linker_c", "rex_linker_cxx", *(["rex_linker_fortran"] if args.fc else [])):
                run([str(build / executable)])
        output = run([*configure, "-DREX_LINKER=invalid"], expected=1)
        assert "REX_LINKER must be" in output
        run([*configure, "-DREX_LINKER=auto", "-DREX_TEST_C_SANITIZERS=ON"])
        run([args.cmake, "--build", str(build), "--parallel", "1"])
        # The sanitizer runtimes must link and load with the selected linker.
        # Leak checking at exit is outside that contract, and its thread
        # suspension needs ptrace, which user-mode emulation cannot provide.
        options = os.environ.get("ASAN_OPTIONS")
        sanitized = dict(os.environ, ASAN_OPTIONS=(
            options + ":" if options else "") + "detect_leaks=0")
        for executable in ("rex_linker_c", "rex_linker_cxx", *(["rex_linker_fortran"] if args.fc else [])):
            run([str(build / executable)], env=sanitized)
        configure += ["-UREX_TEST_C_SANITIZERS"]
        configure += ["-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"]
        run([*configure, "-DREX_LINKER=system"])
        output = run([*configure, "-DREX_LINKER=system",
                      "-DREX_REQUIRED_OPTION=-Wl,--rex-invalid-linker-option"], expected=1)
        assert "Required linker option" in output and "unsupported by C" in " ".join(output.split()), output
        configure += ["-UREX_REQUIRED_OPTION"]
        output = run([*configure, "-DEMSCRIPTEN=TRUE", "-DREX_LINKER=lld"], expected=1)
        assert "Emscripten owns its linker" in output
        run([*configure, "-DEMSCRIPTEN=TRUE", "-DREX_LINKER=auto"])
        assert (build / "selected.txt").read_text() == "system\n\n"
        assert cache_flags(build) == expected_flags
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
