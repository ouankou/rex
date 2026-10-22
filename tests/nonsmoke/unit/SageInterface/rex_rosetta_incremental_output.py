#!/usr/bin/env python3
"""Require unchanged generator output to retain timestamps and expose I/O errors."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

generator = Path(sys.argv[1]).resolve()
grammar = Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix="rex-rosetta-output-") as temporary:
    root = Path(temporary)
    rejected = subprocess.run(
        [str(generator), ".", str(root / "output")], cwd=root,
        capture_output=True, text=True)
    assert rejected.returncode != 0, "generator accepted two output directories"
    assert "only one output directory is allowed" in rejected.stderr
    assert not list(root.iterdir()), "invalid arguments wrote generator output"
    shutil.copytree(grammar, root / "Grammar")
    work = root / "src"
    output = root / "output"
    work.mkdir()
    output.mkdir()
    command = [str(generator), str(output)]
    subprocess.run(command, cwd=work, check=True)
    generated = sorted(output.iterdir()) + sorted(work.iterdir())
    assert (output / "Cxx_Grammar.h") in generated
    assert (output / "AstQueryMemoryPool.h") in generated
    assert all(path.is_file() for path in generated)
    expected = {}
    for path in generated:
        os.utime(path, ns=(1_000_000_000, 1_000_000_000))
        expected[path] = (hashlib.sha256(path.read_bytes()).hexdigest(),
                          path.stat().st_mtime_ns)
    subprocess.run(command, cwd=work, check=True)
    for path, snapshot in expected.items():
        actual = (hashlib.sha256(path.read_bytes()).hexdigest(),
                  path.stat().st_mtime_ns)
        assert actual == snapshot, f"unchanged generated file rewritten: {path}"

    header = output / "Cxx_Grammar.h"
    original = header.read_bytes()
    header.write_bytes(b"invalid generated header\n")
    subprocess.run(command, cwd=work, check=True)
    assert header.read_bytes() == original
    for path, snapshot in expected.items():
        if path != header:
            assert path.stat().st_mtime_ns == snapshot[1], path

    header.unlink()
    header.mkdir()
    rejected = subprocess.run(command, cwd=work, capture_output=True, text=True)
    assert rejected.returncode != 0, "generator accepted an unwritable output file"
print("ROSETTA stable output, changed output, and I/O failure checks passed")
