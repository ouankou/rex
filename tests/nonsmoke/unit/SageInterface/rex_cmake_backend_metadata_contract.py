#!/usr/bin/env python3
"""Exercise backend metadata publication and its hard configuration errors."""

from pathlib import Path
import subprocess
import sys
import tempfile


module = Path(sys.argv[1]).resolve()
cmake = sys.argv[2]
c_command_end = 4 + int(sys.argv[3])
native_compilers = (
    (sys.argv[4:c_command_end], "c", "c17"),
    (sys.argv[c_command_end:], "c++", "c++17"),
)

with tempfile.TemporaryDirectory(prefix="rex-cmake-metadata-") as temp:
    root = Path(temp)
    compiler = root / 'toolchain with "quotes"' / "clang-23"
    compiler.parent.mkdir()
    metadata_path = root / 'metadata with "quotes" and \\ slash??=value'
    metadata_path.mkdir()
    # Compiler discovery already happened. The metadata module must not execute
    # the compiler again, so this executable deliberately fails if invoked.
    compiler.write_text("#!/bin/sh\nexit 97\n")
    compiler.chmod(0o755)
    alias = root / "compiler-alias"
    alias.symlink_to(compiler)
    common = f'''
cmake_minimum_required(VERSION 3.15)
foreach(language C CXX Fortran)
  set(CMAKE_${{language}}_COMPILER [=[{compiler}]=])
  set(CMAKE_${{language}}_COMPILER_LOADED TRUE)
  set(CMAKE_${{language}}_COMPILER_ID Clang)
  set(CMAKE_${{language}}_COMPILER_VERSION 23.1.7)
endforeach()
set(ENABLE-FORTRAN ON)
foreach(field ROSE_TOP_BINARY_DIR ROSE_TOP_SRC_DIR
    ROSE_LLVM_OPENMP_RUNTIME_LIBRARY ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR
    LLVM_OPENMP_INCLUDE_PATH)
  set(${{field}} [=[{metadata_path}]=])
endforeach()
'''
    common += r'''
set(CMAKE_CXX_COMPILER_ARG1 [=[--target=x86_64-linux-gnu "-DREX_TEXT=two words" "-DREX_ESCAPE=quote\" and \\ slash"]=])
set(CMAKE_Fortran_COMPILER_ARG1 "-ffree-form")
'''
    verify = '''
foreach(language C CXX FORTRAN)
  if(NOT BACKEND_${language}_COMPILER_MAJOR_VERSION_NUMBER STREQUAL "23" OR
     NOT BACKEND_${language}_COMPILER_MINOR_VERSION_NUMBER STREQUAL "1" OR
     NOT BACKEND_${language}_COMPILER_NAME_WITHOUT_PATH STREQUAL "clang-23")
    message(FATAL_ERROR "Incorrect published metadata for ${language}")
  endif()
endforeach()
if(NOT BACKEND_CXX_IS_CLANG_COMPILER OR BACKEND_CXX_IS_GNU_COMPILER)
  message(FATAL_ERROR "Incorrect compiler vendor")
endif()
'''
    verify += r'''
set(expected "${CMAKE_CXX_COMPILER};--target=x86_64-linux-gnu;-DREX_TEXT=two words;-DREX_ESCAPE=quote\" and \\ slash")
if(NOT BACKEND_CXX_COMPILER_COMMAND STREQUAL expected OR
   NOT BACKEND_C_COMPILER_COMMAND STREQUAL CMAKE_C_COMPILER OR
   NOT BACKEND_FORTRAN_COMPILER_COMMAND STREQUAL "${CMAKE_Fortran_COMPILER};-ffree-form")
  message(FATAL_ERROR "Compiler command lost configured arguments")
endif()
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "/shared;/cxx runtime")
set(CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES "/shared;/fortran runtime")
set(CMAKE_CXX_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "/shared frameworks;/cxx frameworks")
set(CMAKE_Fortran_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "/shared frameworks")
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "c;stdc++;m;/runtime with spaces/libquoted\".a;-pthread;stdc++")
set(CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES "flang_rt.runtime;m;c")
rex_backend_runtime_options(CXX Fortran runtime_options)
set(expected "-L/cxx runtime;-F/cxx frameworks;-lstdc++;/runtime with spaces/libquoted\".a;-pthread;-lstdc++")
if(NOT runtime_options STREQUAL expected)
  message(FATAL_ERROR "Wrong runtime arguments: '${runtime_options}'")
endif()
rex_backend_runtime_options(CXX CXX runtime_options)
if(NOT "${runtime_options}" STREQUAL "")
  message(FATAL_ERROR "A driver must not add its own implicit runtimes twice")
endif()
'''
    cases = [
        ("selected", "", None),
        ("alias", f'set(BACKEND_CXX_COMPILER "{alias}")', None),
        ("different", 'set(BACKEND_CXX_COMPILER "/different/compiler")',
         "must identify the configured"),
        ("missing_identity", 'unset(CMAKE_CXX_COMPILER_ID)', "enabled, identified"),
        ("missing_version", 'unset(CMAKE_CXX_COMPILER_VERSION)', "complete numeric"),
        ("malformed_version", 'set(CMAKE_Fortran_COMPILER_VERSION "banner 23.1.7")',
         "complete numeric"),
    ]
    for name, override, error in cases:
        script = root / f"{name}.cmake"
        configure_header = f'''
configure_file("{module.parents[1] / 'rosePublicConfig.h.in.cmake'}"
  "{root / 'rosePublicConfig.h'}")
configure_file("{module.parents[2] / 'rose_config.h.in.cmake'}"
  "{root / 'rose_config.h'}")
'''
        script.write_text(common + override + f'\ninclude("{module}")\n' + verify
                          + configure_header)
        result = subprocess.run([cmake, "-P", str(script)], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if error is None:
            if result.returncode:
                raise SystemExit(f"{name}: configuration failed\n{result.stdout}")
            probe = root / "compiler_path.c"
            probe.write_text('''
#include "rose_config.h"
#include <string.h>
int main(int argc, char **argv) {
  return argc != 3 ||
    strcmp(BACKEND_C_COMPILER_NAME_WITH_PATH, argv[1]) ||
    strcmp(BACKEND_CXX_COMPILER_NAME_WITH_PATH, argv[1]) ||
    strcmp(BACKEND_FORTRAN_COMPILER_NAME_WITH_PATH, argv[1]) ||
    strcmp(ROSE_COMPILE_TREE_PATH, argv[2]) ||
    strcmp(ROSE_SOURCE_TREE_PATH, argv[2]) ||
    strcmp(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY, argv[2]) ||
    strcmp(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR, argv[2]) ||
    strcmp(LLVM_OPENMP_INCLUDE_PATH, argv[2]);
}
''')
            executable = root / "compiler_path"
            for command, language, standard in native_compilers:
                subprocess.run([*command, "-x", language, "-std=" + standard,
                                str(probe), "-o", str(executable)], check=True)
                subprocess.run([str(executable), str(compiler), str(metadata_path)],
                               check=True)
        elif result.returncode == 0 or error not in result.stdout:
            raise SystemExit(f"{name}: expected hard error '{error}'\n{result.stdout}")
print("Backend metadata and generated compiler paths preserve compiler identity")
