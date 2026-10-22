#!/usr/bin/env python3
"""Check original and translated execution as well as the translator's AST assertions."""

from pathlib import Path
import subprocess
import sys
import tempfile


translator, compiler, specimen = map(Path, sys.argv[1:4])
standard = sys.argv[4] if len(sys.argv) == 5 else "c++11"
with tempfile.TemporaryDirectory(prefix="rex-template-spelling-") as directory:
    work = Path(directory)
    for source in (specimen, work / ("rose_" + specimen.name)):
        if source != specimen:
            subprocess.run(
                [str(translator), f"-std={standard}", "-c", str(specimen)],
                cwd=work,
                check=True,
            )
        executable = work / (source.stem + ".exe")
        subprocess.run(
            [str(compiler), f"-std={standard}", str(source), "-o", str(executable)],
            cwd=work,
            check=True,
        )
        subprocess.run([str(executable)], cwd=work, check=True)
