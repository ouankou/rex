"""Verify typed ABI matching and nonzero status for unverified interfaces."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


checker = sys.argv[1]
cases = [
    ("no_arguments", "void rex_entry(void);",
     'subroutine rex_entry() bind(c)\nend subroutine\n', 0),
    ("scalar_reference", "void rex_entry(int, int *, float);",
     '''subroutine rex_entry(a, b, c) bind(c)
integer, value :: a
integer :: b
real, value :: c
end subroutine
''', 0),
    ("renamed_kinds", "double rex_Mixed(short *, long long, double);",
     '''function rex_local(a, b, c) bind(c, name="  rex_" // "Mixed  ") result(r)
use iso_c_binding, only: small=>c_short, wide=>c_long_long, real_kind=>c_double
integer(small) :: a
integer(wide), value :: b
real(real_kind), value :: c
real(real_kind) :: r
r = c
end function
''', 0),
    ("size_kind", "#include <stddef.h>\nvoid rex_entry(size_t);",
     '''subroutine rex_entry(a) bind(c)
use iso_c_binding, only: c_size_t
integer(c_size_t), value :: a
end subroutine
''', 0),
    ("array_address", "void rex_entry(int [100], float *);",
     '''subroutine rex_entry(a, b) bind(c)
integer :: a(100)
real :: b(*)
end subroutine
''', 0),
    ("logical_character", "void rex_entry(_Bool, char, char *);",
     '''subroutine rex_entry(a, b, c) bind(c)
use iso_c_binding, only: c_bool, c_char
logical(c_bool), value :: a
character(kind=c_char), value :: b
character(kind=c_char) :: c
end subroutine
''', 0),
    ("complex", "double _Complex rex_entry(double _Complex);",
     '''function rex_entry(a) bind(c) result(r)
use iso_c_binding, only: c_double_complex
complex(c_double_complex), value :: a
complex(c_double_complex) :: r
r = a
end function
''', 0),
    ("type_mismatch", "void rex_entry(float);",
     'subroutine rex_entry(a) bind(c)\ninteger, value :: a\nend subroutine\n', 1),
    ("passing_mismatch", "void rex_entry(int);",
     'subroutine rex_entry(a) bind(c)\ninteger :: a\nend subroutine\n', 1),
    ("result_mismatch", "float rex_entry(void);",
     '''function rex_entry() bind(c) result(r)
use iso_c_binding, only: c_double
real(c_double) :: r
r = 1
end function
''', 1),
    ("missing_interface", "void rex_other(void);",
     'subroutine rex_entry() bind(c)\nend subroutine\n', 1),
    ("cxx_linkage", 'extern "C" void rex_entry(void);',
     'subroutine rex_entry() bind(c)\nend subroutine\n', 0),
    ("cxx_unbound", 'void rex_entry(void);',
     'subroutine rex_entry() bind(c)\nend subroutine\n', 1),
    ("later_mismatch", 'void rex_a(void);\nvoid rex_b(float);',
     '''subroutine rex_a() bind(c)
end subroutine
subroutine rex_b(value) bind(c)
integer, value :: value
end subroutine
''', 1),
    ("empty_binding", "void rex_entry(void);",
     'subroutine rex_entry() bind(c, name="")\nend subroutine\n', 2),
    ("descriptor_unverified", "void rex_entry(float *);",
     'subroutine rex_entry(a) bind(c)\nreal :: a(:)\nend subroutine\n', 2),
]

with tempfile.TemporaryDirectory(prefix="rex_interface_") as temporary:
    root = Path(temporary)
    for name, c_source, fortran_source, expected in cases:
        for checkpoint in (False, True):
            work = root / (name + ("_json" if checkpoint else "_native"))
            work.mkdir()
            c = work / ("rex_interface_cpp.cpp" if name.startswith("cxx_")
                        else "rex_interface_c.c")
            f = work / "rex_interface_fortran.f90"
            c.write_text(c_source)
            f.write_text(fortran_source)
            env = os.environ.copy()
            if checkpoint:
                env["REX_AST_JSON_CHECKPOINT"] = "pre-omp-construction"
                env["REX_AST_JSON_DIR"] = str(work / "checkpoint")
            result = subprocess.run([checker, "-c", str(c), str(f)], cwd=work,
                                    env=env, text=True, capture_output=True)
            if result.returncode != expected:
                raise AssertionError(
                    f"{name} checkpoint={checkpoint}: expected exit {expected}, "
                    f"got {result.returncode}\n{result.stdout}\n{result.stderr}")
            if expected == 0 and "Compatible ABI" not in result.stdout:
                raise AssertionError(f"{name}: no verified interface was reported")
            if expected == 2 and not (
                    "Cannot verify binding" in result.stderr or
                    "No externally bound" in result.stderr):
                raise AssertionError(f"{name}: missing explicit unverified diagnostic")
