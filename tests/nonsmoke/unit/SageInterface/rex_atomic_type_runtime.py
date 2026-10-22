"""Preserve atomic value and pointer types through AST copying and checkpoints."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

translator, cc, cxx = sys.argv[1:]
source = """
int rex_plain_value = 0;
_Atomic(int) rex_atomic_value;
_Atomic(int *) rex_atomic_pointer;
_Atomic(int) *rex_pointer_to_atomic = &rex_atomic_value;
int main(void) {
  __c11_atomic_store(&rex_atomic_value, 7, 5);
  __c11_atomic_store(&rex_atomic_pointer, &rex_plain_value, 5);
  return __c11_atomic_load(rex_pointer_to_atomic, 5) != 7 ||
         __c11_atomic_load(&rex_atomic_pointer, 5) != &rex_plain_value;
}
"""
with tempfile.TemporaryDirectory(prefix="rex_atomic_type_") as directory:
    root = Path(directory)
    for suffix, compiler, standard in (("c", cc, "c11"), ("cpp", cxx, "c++17")):
        original = root / ("rex_atomic_type." + suffix)
        original.write_text(source)
        for checkpoint in (False, True):
            work = root / (suffix + str(checkpoint))
            work.mkdir()
            emitted = work / original.name
            env = os.environ.copy()
            if checkpoint:
                env["REX_AST_JSON_CHECKPOINT"] = "pre-omp-construction"
                env["REX_AST_JSON_DIR"] = str(work / "checkpoint")
            subprocess.run([translator, "-std=" + standard, "-c", str(original),
                            "-rose:output", str(emitted)], cwd=work, env=env,
                           check=True)
            for label, path in (("original", original), ("emitted", emitted)):
                for optimization in ("-O0", "-O2"):
                    executable = work / (label + optimization)
                    subprocess.run([compiler, "-std=" + standard, optimization,
                                    str(path), "-o", str(executable)],
                                   cwd=work, check=True)
                    subprocess.run([str(executable)], cwd=work, check=True)
