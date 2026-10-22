"""Check exact large array bounds without allocating any large objects."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


translator, cc, cxx = sys.argv[1:]
source = """
typedef unsigned char rex_small[4];
typedef unsigned char rex_wide[4294967300ULL];
typedef unsigned char rex_signed_boundary[2147483648ULL];
#ifdef __cplusplus
static_assert(sizeof(rex_wide) == 4294967300ULL, "exact wide extent");
static_assert(sizeof(rex_signed_boundary) == 2147483648ULL, "exact extent");
#else
_Static_assert(sizeof(rex_wide) == 4294967300ULL, "exact wide extent");
_Static_assert(sizeof(rex_signed_boundary) == 2147483648ULL, "exact extent");
#endif
int main(void) { return sizeof(rex_small) != 4; }
"""
with tempfile.TemporaryDirectory(prefix="rex_array_extent_") as directory:
    root = Path(directory)
    for suffix, compiler, standard in (("c", cc, "c11"), ("cpp", cxx, "c++17")):
        original = root / f"rex_array_extent.{suffix}"
        original.write_text(source)
        for checkpoint in (False, True):
            work = root / f"{suffix}-{checkpoint}"
            work.mkdir()
            emitted = work / original.name
            env = os.environ.copy()
            if checkpoint:
                env["REX_AST_JSON_CHECKPOINT"] = "pre-omp-construction"
                env["REX_AST_JSON_DIR"] = str(work / "checkpoint")
            subprocess.run([translator, "-c", str(original), "-rose:output",
                            str(emitted)], cwd=work, env=env, check=True)
            for label, path in (("original", original), ("emitted", emitted)):
                executable = work / label
                subprocess.run([compiler, f"-std={standard}", str(path),
                                "-o", str(executable)], cwd=work, check=True)
                subprocess.run([str(executable)], cwd=work, check=True)
