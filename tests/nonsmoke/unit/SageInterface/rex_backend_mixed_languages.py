#!/usr/bin/env python3
"""Mixed C/C++/Fortran inputs retain their runtimes and AST checkpoint identities."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

translator, cmake, c_compiler, cxx_compiler, fortran_compiler = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="rex-backend-mixed-") as directory:
    root = Path(directory)
    source = root / "inputs"
    source.mkdir()
    (source / "rex_mixed_c_helper.c").write_text(
        '_Static_assert(_Alignof(int) > 0, "integer alignment");\n'
        "int rex_c_increment(int value) { return value + 7; }\n"
        "int rex_c_identity(int value) { return value; }\n")
    (source / "rex_mixed_cpp_helper.cpp").write_text('''namespace rex_mixed {
struct Value { int value; };
enum Choice { delta = 7 };
}
static_assert(alignof(rex_mixed::Value) > 0, "record alignment");
extern "C" int rex_cpp_increment(int value) {
  rex_mixed::Value *pointer = new rex_mixed::Value{value + rex_mixed::delta};
  int answer = pointer->value;
  delete pointer;
  return answer;
}
extern "C" int rex_cpp_identity(int value) { return value; }
''')
    (source / "rex_mixed_main.f90").write_text('''program rex_mixed
use iso_c_binding, only: c_int
implicit none
interface
  function rex_c_increment(value) bind(c) result(answer)
    import c_int
    integer(c_int), value :: value
    integer(c_int) :: answer
  end function
  function rex_cpp_increment(value) bind(c) result(answer)
    import, only: c_int
    integer(c_int), value :: value
    integer(c_int) :: answer
  end function
  function rex_c_identity(value) bind(c) result(answer)
    import, all
    integer(c_int), value :: value
    integer(c_int) :: answer
  end function
  function rex_cpp_identity(value) bind(c) result(answer)
    import, none
    integer, value :: value
    integer :: answer
  end function
end interface
if (rex_cpp_identity(rex_c_identity(rex_cpp_increment(rex_c_increment(7_c_int)))) /= 21_c_int) stop 1
end program
''')
    # CMake independently selects its mixed-language link driver and runtimes.
    (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.15)
project(rex_mixed LANGUAGES C CXX Fortran)
add_executable(program rex_mixed_main.f90 rex_mixed_c_helper.c rex_mixed_cpp_helper.cpp)
''')
    stock = root / "stock"
    subprocess.run([cmake, "-S", str(source), "-B", str(stock),
                    "-DCMAKE_BUILD_TYPE=Debug", f"-DCMAKE_C_COMPILER={c_compiler}",
                    f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
                    f"-DCMAKE_Fortran_COMPILER={fortran_compiler}"], check=True)
    subprocess.run([cmake, "--build", str(stock)], check=True)
    subprocess.run([str(stock / "program")], check=True)
    for mode in ("rex", "json"):
        work = root / mode
        work.mkdir()
        env = os.environ.copy()
        if mode == "json":
            env.update(REX_AST_JSON_CHECKPOINT="pre-omp-construction",
                       REX_AST_JSON_DIR=str(work / "json"))
        subprocess.run([translator, "-O0", str(source / "rex_mixed_main.f90"),
                        str(source / "rex_mixed_c_helper.c"),
                        str(source / "rex_mixed_cpp_helper.cpp"),
                        "-o", str(work / "program")], cwd=work, env=env, check=True)
        subprocess.run([str(work / "program")], cwd=work, check=True)

        if mode == "json":
            checkpoints = sorted((work / "json").glob("*.json"))
            assert checkpoints, "No AST checkpoints were produced"
            for checkpoint in checkpoints:
                data = json.loads(checkpoint.read_text())
                imports = [node["properties"] for node in data["nodes"]
                           if node["kind"] == "SgImportStatement"]
                kinds = sorted(item["fortran_import_kind"] for item in imports)
                assert kinds == [0, 1, 2, 3], (checkpoint, imports)
