#!/usr/bin/env python3
"""Shared module declarations must survive replacement of every project file."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


translator, compiler, inputs = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-ast-json-shared-modules-") as directory:
    root = Path(directory)
    objects = []
    for part in ("base", "middle"):
        source = root / f"rex_flang_include_{part}.f90"
        shutil.copyfile(inputs / source.name, source)
        obj = root / f"{part}.o"
        subprocess.run([str(compiler), "-I", str(root), "-J", str(root),
                        "-c", str(source), "-o", str(obj)], check=True)
        objects.append(str(obj))
    (root / "rex_flang_shared_compute.f90").write_text('subroutine rex_compute(number)\nuse rex_flang_include_middle\nimplicit none\ninteger :: number\ntype(rex_wrapper) :: value\nvalue = rex_make_value()\nnumber = value%value%number\nend subroutine\n')
    (root / "rex_flang_shared_main.f90").write_text('program rex_main\nuse rex_flang_include_middle\nimplicit none\ninteger :: number\ntype(rex_wrapper) :: value\ncall rex_compute(number)\nvalue = rex_make_value()\nif (number + value%value%number /= 14) stop 1\nend program\n')
    for mode in ("stock", "rex", "json"):
        work = root / mode
        work.mkdir()
        tool = compiler if mode == "stock" else translator
        env = os.environ.copy()
        if mode == "json":
            env.update(REX_AST_JSON_CHECKPOINT="pre-omp-construction",
                       REX_AST_JSON_DIR=str(work / "json"))
        result = subprocess.run([
            str(tool), "-I", str(root),
            str(root / "rex_flang_shared_compute.f90"),
            str(root / "rex_flang_shared_main.f90"), *objects,
            "-o", str(work / "program"),
        ], cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
           text=True)
        print(result.stdout, end="", flush=True)
        result.check_returncode()
        subprocess.run([str(work / "program")], cwd=work, check=True)
