#!/usr/bin/env python3
"""Check nested module paths and Flang 23's KIND4 substring rejection."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


translator, compiler, specimens = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-flang-include-") as directory:
    root = Path(directory)
    for index, name in enumerate(('quote"path', "space path", "back\\slash")):
        include = root / name
        include.mkdir()
        work = root / f"run-{index}"
        work.mkdir()
        objects = []
        for module in ("base", "middle"):
            source = include / f"rex_flang_include_{module}.f90"
            shutil.copyfile(specimens / source.name, source)
            obj = work / (module + ".o")
            subprocess.run([str(compiler), "-I", str(include), "-J", str(include),
                            "-c", str(source), "-o", str(obj)], check=True)
            objects.append(str(obj))
        source = work / "rex_flang_include_main.f90"
        shutil.copyfile(specimens / source.name, source)
        subprocess.run([str(translator), "-rose:skipfinalCompileStep",
                        "-outputdir", str(work), "-I", str(include),
                        str(source)], cwd=work, check=True)

        # Stock Flang 23 parses KIND4 literal substrings but cannot lower their
        # StaticDataObject representation (ConvertExprToHLFIR.cpp). Require that
        # exact rejection for both the original and REX-emitted source. REX's
        # typed import, substring, copy and JSON checks above must still succeed.
        # Follow-up: https://github.com/ouankou/rex/issues/862.
        # Unexpected native success fails this test so a Flang fix prompts review
        # and restoration of the original/translated execution comparison.
        for native_source, executable in (
                (source, "original"),
                (work / ("rose_" + source.name), "translated")):
            if not native_source.is_file():
                raise AssertionError(f"Missing source: {native_source}")
            result = subprocess.run(
                [str(compiler), "-I", str(include), str(native_source),
                 *objects, "-o", executable], cwd=work, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            diagnostic = (
                "not yet implemented: StaticDataObject::Pointer substring "
                "with kind > 1")
            if result.returncode != 1 or result.stdout.count(diagnostic) != 1:
                raise AssertionError(
                    f"Expected Flang 23 KIND4 lowering rejection for "
                    f"{native_source}; exit={result.returncode}\n{result.stdout}")
            print(result.stdout, end="")
