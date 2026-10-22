"""Check exact package ownership and invocation identity without a compiler build."""
from pathlib import Path
import subprocess
import sys
import tempfile

module = Path(sys.argv[1]).resolve()
cmake = sys.argv[2]


def executable(path, version="22.1.8", status=0):
    path.write_text(f"#!/bin/sh\nprintf '%s\\n' '{version}'\nexit {status}\n")
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix="rex_llvm_driver_", dir=Path.cwd()) as temp:
    root = Path(temp)
    prefixes = {name: root / (name + " package")
                for name in ("automatic", "explicit", "home")}
    for name, prefix in prefixes.items():
        (prefix / "bin").mkdir(parents=True)
        executable(prefix / "bin/clang-22")
        for package in ("LLVM", "Clang"):
            directory = prefix / "lib/cmake" / package.lower()
            directory.mkdir(parents=True)
            (directory / (package + "Config.cmake")).write_text(
                f'set(REX_SELECTED_{package} "{name}")\n')
    for case, home, selection, expected in (
        ("compiler_prefix", "", "", "automatic"),
        ("llvm_home", prefixes["home"], "", "home"),
        ("explicit_roots", prefixes["home"],
         f'set(LLVM_ROOT "{prefixes["explicit"]}")\n'
         f'set(Clang_ROOT "{prefixes["explicit"]}")', "explicit"),
        ("environment_roots", prefixes["home"],
         f'set(ENV{{LLVM_ROOT}} "{prefixes["explicit"]}")\n'
         f'set(ENV{{Clang_ROOT}} "{prefixes["explicit"]}")', "explicit"),
        ("explicit_prefix_path", prefixes["home"],
         f'set(CMAKE_PREFIX_PATH "{prefixes["explicit"]}")', "explicit"),
        ("toolchain_prefix", prefixes["home"],
         'set(CMAKE_TOOLCHAIN_FILE "external-toolchain.cmake")', "home"),
        ("wasm_prefix", prefixes["home"], 'set(REX_ENABLE_WASM TRUE)', "home"),
    ):
        script = root / (case + ".cmake")
        script.write_text(f'''
cmake_minimum_required(VERSION 3.19)
include("{module}")
foreach(package LLVM Clang)
  unset(ENV{{${{package}}_DIR}})
  unset(ENV{{${{package}}_ROOT}})
endforeach()
unset(ENV{{CLANG_ROOT}})
unset(ENV{{CMAKE_PREFIX_PATH}})
set(ENV{{LLVM_HOME}} "{home}")
{selection}
set(saved_llvm_root "${{LLVM_ROOT}}")
set(saved_clang_root "${{Clang_ROOT}}")
rex_llvm_package_hints("{prefixes["automatic"] / "bin/clang-22"}" hint_paths)
if(NOT "${{LLVM_ROOT}}" STREQUAL "${{saved_llvm_root}}" OR
   NOT "${{Clang_ROOT}}" STREQUAL "${{saved_clang_root}}")
  message(FATAL_ERROR "Package inference overwrote explicit roots")
endif()
find_package(LLVM REQUIRED CONFIG HINTS ${{hint_paths}})
find_package(Clang REQUIRED CONFIG HINTS ${{hint_paths}})
if(NOT REX_SELECTED_LLVM STREQUAL "{expected}" OR
   NOT REX_SELECTED_Clang STREQUAL "{expected}")
  message(FATAL_ERROR "Incorrect package selection")
endif()
''')
        result = subprocess.run([cmake, "-P", str(script)],
                                capture_output=True, text=True, timeout=20)
        if result.returncode != 0:
            raise RuntimeError(case + ": " + result.stdout + result.stderr)
        print(case + ": passed")

    cases = ["versioned", "unversioned", "cxx_symlink", "wrong_major",
             "failed_probe", "missing", "outside_package", "directory",
             "relative_tools", "nonexecutable", "no_version_fallback",
             "version_suffix", "trailing_dot", "empty_component",
             "extra_output", "empty_version", "major_only"]
    for case in cases:
        work = root / case
        tools = work / "LLVM tools"
        tools.mkdir(parents=True)
        name = "clang++" if case == "cxx_symlink" else "clang"
        selected = tools / (name + "-22")
        error = None
        if case == "unversioned":
            selected = tools / name
        if case == "missing":
            error = "has no clang 22 driver"
        elif case == "directory":
            selected.mkdir()
            executable(tools / name)
            error = "driver path is a directory"
        elif case == "outside_package":
            external = work / "external-clang"
            executable(external)
            selected.symlink_to(external)
            error = "resolves outside the loaded package"
        elif case == "cxx_symlink":
            executable(tools / "clang-22")
            selected.symlink_to(tools / "clang-22")
        else:
            version = {
                "wrong_major": "21.1.0",
                "no_version_fallback": "21.1.0",
                "version_suffix": "22garbage",
                "trailing_dot": "22.",
                "empty_component": "22..1",
                "extra_output": "22.1.8\nprobe error",
                "empty_version": "",
                "major_only": "22",
            }.get(case, "22.1.8")
            executable(selected, version,
                       7 if case == "failed_probe" else 0)
            if case in ("wrong_major", "failed_probe", "no_version_fallback",
                        "version_suffix", "trailing_dot", "empty_component",
                        "extra_output", "empty_version"):
                error = "must report Clang major 22"
            if case == "no_version_fallback":
                executable(tools / name)
            if case == "nonexecutable":
                selected.chmod(0o644)
                error = "must report Clang major 22"
        tools_argument = "relative/tools" if case == "relative_tools" else str(tools)
        if case == "relative_tools":
            error = "absolute LLVM_TOOLS_BINARY_DIR"
        script = work / "check.cmake"
        script.write_text(f'''
include("{module}")
set(LLVM_TOOLS_BINARY_DIR "{tools_argument}")
set(CLANG_VERSION_MAJOR 22)
set(driver "caller driver")
set(major "caller major")
rex_require_llvm_driver("{name}" actual)
if(NOT actual STREQUAL "{selected}")
  message(FATAL_ERROR "Incorrect invocation identity: '${{actual}}'")
endif()
if(NOT driver STREQUAL "caller driver" OR NOT major STREQUAL "caller major")
  message(FATAL_ERROR "Driver probe variables leaked into caller scope")
endif()
''')
        result = subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if error is None:
            if result.returncode != 0:
                raise RuntimeError(case + ": " + output)
        elif result.returncode == 0 or error not in output:
            raise RuntimeError(case + " did not reject the invalid package: " + output)
        print(case + ": passed")

    resource = root / "resource directory"
    resource.mkdir()
    alias = root / "resource alias"
    alias.symlink_to(resource, target_is_directory=True)
    for case, reported, status, valid in [
        ("resource", str(resource), 0, True),
        ("resource_symlink", str(alias), 0, True),
        ("resource_relative", "relative/resource", 0, False),
        ("resource_missing", str(root / "absent_resource"), 0, False),
        ("resource_probe_failure", str(resource), 7, False),
        ("resource_empty", "", 0, False),
    ]:
        driver = root / case
        executable(driver, reported, status)
        script = root / (case + ".cmake")
        script.write_text(f'''
include("{module}")
rex_require_clang_resource_directory("{driver}" actual)
if(NOT actual STREQUAL "{resource}")
  message(FATAL_ERROR "Incorrect resource identity: '${{actual}}'")
endif()
''')
        result = subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if valid:
            if result.returncode != 0:
                raise RuntimeError(case + ": " + output)
        elif result.returncode == 0 or "Clang resource directory resolution failed" not in output:
            raise RuntimeError(case + " did not reject the invalid resource: " + output)
        print(case + ": passed")
