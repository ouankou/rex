"""Exercise numeric 128-bit template values, defaults and instantiated literals."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


translator, compiler = sys.argv[1:]
source = """
template<__int128 V = ((__int128)1 << 100) + 7> struct RexSigned {
  static constexpr __int128 value = V;
};
template<unsigned __int128 V> struct RexUnsigned {
  static constexpr unsigned __int128 value = V;
};
RexSigned<> rex_positive;
RexSigned<-(((__int128)1 << 100) + 7)> rex_negative;
RexUnsigned<~(unsigned __int128)0> rex_unsigned;
int main() {
  return rex_positive.value != (((__int128)1 << 100) + 7) ||
         rex_negative.value != -(((__int128)1 << 100) + 7) ||
         rex_unsigned.value != ~(unsigned __int128)0;
}
"""
with tempfile.TemporaryDirectory(prefix="rex_integer_wide_") as directory:
    root = Path(directory)
    original = root / "rex_integer_wide.cpp"
    original.write_text(source)
    for checkpoint in (False, True):
        work = root / str(checkpoint)
        work.mkdir()
        emitted = work / original.name
        env = os.environ.copy()
        if checkpoint:
            env["REX_AST_JSON_CHECKPOINT"] = "pre-omp-construction"
            env["REX_AST_JSON_DIR"] = str(work / "checkpoint")
        subprocess.run([translator, "-c", str(original), "-rose:output",
                        str(emitted)], cwd=work, env=env, check=True)
        for label, path in (("original", original), ("emitted", emitted)):
            for optimization in ("-O0", "-O2"):
                executable = work / (label + optimization)
                subprocess.run([compiler, "-std=c++17", optimization, str(path),
                                "-o", str(executable)], cwd=work, check=True)
                subprocess.run([str(executable)], cwd=work, check=True)
