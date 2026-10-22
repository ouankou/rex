#!/usr/bin/env python3
"""Exercise production ROSETTA allocation operators with diagnostic fills."""

from pathlib import Path
import subprocess
import sys
import tempfile

compiler, allocator_path, fixture_path, *flags = sys.argv[1:]
allocator = Path(allocator_path).read_text().replace("$CLASSNAME", "RexPoolProbe")
fixture = Path(fixture_path).read_text()
with tempfile.TemporaryDirectory(prefix="rex-rosetta-pool-fill-",
                                 dir=Path.cwd()) as directory:
    root = Path(directory)
    source = root / "rex_rosetta_pool_fill.cpp"
    source.write_text(fixture.replace("@ALLOCATOR@", allocator))
    common = [compiler, "-std=c++17", "-pthread", *flags]
    for mode in (0, 2, 3):
        executable = root / f"mode-{mode}"
        subprocess.run([*common, f"-DROSE_ALLOC_MEMSET={mode}", str(source),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    for mode in (1, 4):
        result = subprocess.run([*common, f"-DROSE_ALLOC_MEMSET={mode}",
                                 "-fsyntax-only", str(source)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True)
        assert result.returncode != 0, result.stdout
        assert "ROSE_ALLOC_MEMSET must be 0, 2, or 3" in result.stdout, result.stdout
