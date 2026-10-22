#!/usr/bin/env python3
"""Parse the original Fortran source bytes and preserve their semantics."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


translator, compiler, specimens = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-flang-prescan-") as directory:
    root = Path(directory)
    for name in ("rex_flang_prescan_source.F90", "rex_flang_prescan_empty.inc"):
        shutil.copyfile(specimens / name, root / name)
    for tool, executable in ((compiler, "original"), (translator, "translated")):
        subprocess.run([str(tool), str(root / "rex_flang_prescan_source.F90"),
                        "-I", str(root), "-o", executable], cwd=root, check=True)
        subprocess.run([str(root / executable)], check=True)
