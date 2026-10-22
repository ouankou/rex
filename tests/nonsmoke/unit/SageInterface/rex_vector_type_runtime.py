"""Preserve vector size and operations through typed AST copying and JSON."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

translator, cc, cxx = sys.argv[1:]
source = """
typedef int rex_vector __attribute__((vector_size(16)));
typedef int rex_ext_vector __attribute__((ext_vector_type(4)));
typedef int rex_ext_three __attribute__((ext_vector_type(3)));
typedef int rex_ext_wide __attribute__((ext_vector_type(32)));
int main(void) {
  rex_vector a = {1, 2, 3, 4};
  rex_vector b = {5, 6, 7, 8};
  rex_vector result = a + b;
  rex_ext_vector v = {1, 2, 3, 4};
  rex_ext_vector *p = &v;
  v.xy = v.yx;
  p->zw = v.xy;
  rex_ext_three three = {7, 8, 9};
  rex_ext_wide wide = {0};
  wide[31] = 11;
  return sizeof(result) != 16 || result[0] != 6 || result[3] != 12 ||
         v.s0 != 2 || v.hi.s1 != 1 || v.lo.s0 != 2 ||
         v.even.s1 != 2 || v.odd.s0 != 1 || v.xx.s1 != 2 ||
         three.hi.s0 != 9 || three.even.s1 != 9 || wide.hi.sf != 11;
}
"""
with tempfile.TemporaryDirectory(prefix="rex_vector_type_") as directory:
    root = Path(directory)
    for suffix, compiler, standard in (("c", cc, "c11"), ("cpp", cxx, "c++17")):
        original = root / ("rex_vector_type." + suffix)
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
