#!/usr/bin/env python3
"""Execute edited cooked/raw/string/template calls after copying and JSON."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    translator, compiler, source = [str(Path(arg).resolve()) for arg in sys.argv[1:]]
    with tempfile.TemporaryDirectory(prefix="rex-udl-semantic-edit-") as directory:
        root = Path(directory)
        output = root / "translated.cpp"
        subprocess.run([translator, "-std=c++20", "-rose:verbose", "0", "-c", source,
                        "-rose:output", str(output)], check=True, cwd=root)
        names = ("cooked", "raw", "text", "template", "other_template")
        for name, specimen, expected in (
            ("original", source, (42, 123, 67, 5, 2)),
            ("edited", str(output), (43, 456, 68, 2, 2)),
        ):
            wrapper = root / (name + "_main.cpp")
            declarations = "".join(f"extern unsigned long long rex_udl_{n};\n" for n in names)
            condition = " || ".join(f"rex_udl_{n} != {v}" for n, v in zip(names, expected))
            wrapper.write_text(declarations + f"int main() {{ return {condition}; }}\n")
            for optimization in ("-O0", "-O2"):
                executable = root / (name + optimization)
                subprocess.run([compiler, "-std=c++20", optimization, specimen,
                                str(wrapper), "-o", str(executable)], check=True, cwd=root)
                subprocess.run([str(executable)], check=True, cwd=root)


if __name__ == "__main__":
    main()
