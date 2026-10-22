#!/usr/bin/env python3
"""Execute a selected hidden-friend template followed by operator syntax."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    translator, compiler, source = [str(Path(arg).resolve()) for arg in sys.argv[1:]]
    with tempfile.TemporaryDirectory(prefix="rex-template-reference-") as directory:
        root = Path(directory)
        output = root / "translated.cpp"
        subprocess.run([translator, "-std=c++20", "-rose:verbose", "0", "-c", source,
                        "-rose:output", str(output)], check=True, cwd=root)
        for name, specimen in (("original", source), ("translated", str(output))):
            for optimization in ("-O0", "-O2"):
                executable = root / (name + optimization)
                subprocess.run([compiler, "-std=c++20", optimization, specimen,
                                "-o", str(executable)], check=True, cwd=root)
                subprocess.run([str(executable)], check=True, cwd=root)


if __name__ == "__main__":
    main()
