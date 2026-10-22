#!/usr/bin/env python3
"""Exercise fragment rejection through the real ROSETTA generator."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

generator = Path(sys.argv[1]).resolve()
grammar = Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix="rex-rosetta-fragments-") as temporary:
    root = Path(temporary)
    shutil.copytree(grammar, root / "Grammar")
    work = root / "src"
    output = root / "output"
    work.mkdir()
    output.mkdir()
    source = root / "Grammar" / "Type.code"
    original = source.read_text()
    start = "HEADER_TYPEDEF_TYPE_START"
    end = "HEADER_TYPEDEF_TYPE_END"
    assert original.count(start) == original.count(end) == 1
    command = [str(generator), str(output)]
    cases = {
        "duplicate-start": (original + "\n" + start + "\n",
                            "duplicate grammar marker " + start),
        "duplicate-end": (original + "\n" + end + "\n",
                          "duplicate grammar marker " + end),
        "missing-start": (original.replace(start, "// missing start"),
                          "missing or unordered grammar fragment " + start),
        "missing-end": (original.replace(end, "// missing end"),
                        "missing or unordered grammar fragment " + start),
        "reversed": (original.replace(start, "REX_TEMPORARY_MARKER")
                     .replace(end, start).replace("REX_TEMPORARY_MARKER", end),
                     "missing or unordered grammar fragment " + start),
    }
    for name, (text, diagnostic) in cases.items():
        source.write_text(text)
        result = subprocess.run(command, cwd=work, capture_output=True, text=True)
        assert result.returncode == 1, (name, result.returncode, result.stderr)
        assert diagnostic in result.stderr, (name, result.stderr)
        assert not list(output.iterdir()), f"{name}: published partial output"

    # Delimiter names in comments are ordinary text, not fragment boundaries.
    source.write_text("// " + start + "\n// " + end + "\n" + original)
    subprocess.run(command, cwd=work, check=True, capture_output=True, text=True)
    assert (output / "Cxx_Grammar.h").is_file()
print("ROSETTA exact fragment and early rejection checks passed")
