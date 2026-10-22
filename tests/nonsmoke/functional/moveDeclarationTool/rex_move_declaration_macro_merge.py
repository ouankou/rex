#!/usr/bin/env python3
"""Check declaration merging at a macro RHS through compilation and execution."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    tool, compiler, specimen = map(lambda arg: str(Path(arg).absolute()), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="rex-macro-merge-") as temporary:
        work = Path(temporary)

        def execute(source, name):
            for optimization in ("-O0", "-O2"):
                executable = work / (name + optimization)
                subprocess.run([compiler, "-std=gnu++17", optimization, source,
                                "-o", str(executable)], check=True, cwd=work)
                subprocess.run([str(executable)], check=True, cwd=work)

        execute(specimen, "original")
        for merge in (False, True):
            name = "merged" if merge else "separate"
            output = work / (name + ".C")
            options = ["-rose:merge_decl_assign"] if merge else []
            subprocess.run([tool, "-rose:verbose", "0", "-std=gnu++17",
                            "-rose:debug", "-rose:trans-tracking", *options,
                            "-c", specimen, "-rose:output", str(output)],
                           check=True, cwd=work)
            text = output.read_text()
            initialized = re.search(
                r"\bint\s+result\s*=\s*REX_ARRAY_VALUE\s*\(\s*position\s*\)\s*;", text
            )
            if bool(initialized) != merge:
                raise AssertionError(f"incorrect declaration merge in {name}:\n{text}")
            if not re.search(r"\bresult\s*=\s*REX_ARRAY_VALUE\s*\(\s*position\s*\)\s*;", text):
                raise AssertionError(f"macro RHS ownership was lost:\n{text}")
            conditional = re.search(
                r"\bint\s+minimum\s*=\s*REX_MINIMUM\s*\(\s*result\s*,\s*6\s*\)\s*;", text
            )
            if bool(conditional) != merge:
                raise AssertionError(f"incorrect conditional macro merge in {name}:\n{text}")
            execute(str(output), name)
        print("macro RHS merge, source ownership and runtime contracts passed")


if __name__ == "__main__":
    main()
