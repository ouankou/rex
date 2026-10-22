#!/usr/bin/env python3
"""Flang header selection requires files from a single complete directory."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    source = Path(sys.argv[1]).read_text()
    cmake = sys.argv[2]
    begin = source.index('  set(FLANG_INCLUDE_DIRS "")\n')
    end = source.index('  set(FLANG_INCLUDE_DIRS "${FLANG_INCLUDE_DIRS}" CACHE', begin)
    with tempfile.TemporaryDirectory(prefix="rex-flang-headers-") as directory:
        work = Path(directory)
        required = ("flang/Parser/parsing.h", "flang/Semantics/semantics.h")
        prefixes = {name: work / name for name in
                    ("complete", "missing", "directory", "dangling", "left", "right")}
        for prefix in prefixes.values():
            for header in required:
                path = prefix / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
        for name in ("missing", "directory", "dangling", "left"):
            (prefixes[name] / required[1]).unlink()
        (prefixes["right"] / required[0]).unlink()
        (prefixes["directory"] / required[1]).mkdir()
        (prefixes["dangling"] / required[1]).symlink_to(work / "absent.h")
        script = work / "probe.cmake"
        script.write_text(
            'cmake_minimum_required(VERSION 3.19)\n'
            'set(REX_FLANG_FRONTEND_HEADERS "' + ';'.join(required) + '")\n' +
            source[begin:end] +
            'file(WRITE "${RESULT_FILE}" "${FLANG_INCLUDE_DIRS}")\n')
        for name, candidates, selected in (
            ("complete", ["complete"], "complete"),
            ("missing", ["missing"], None),
            ("directory", ["directory"], None),
            ("dangling", ["dangling"], None),
            ("split", ["left", "right"], None),
            ("reject-first", ["directory", "complete"], "complete"),
        ):
            output = work / (name + ".result")
            result = subprocess.run(
                [cmake, '-D_flang_include_candidates=' +
                 ';'.join(str(prefixes[c]) for c in candidates),
                 f'-DRESULT_FILE={output}', '-P', str(script)],
                capture_output=True, text=True, timeout=20)
            assert (result.returncode == 0) == (selected is not None), (
                name, result.stdout, result.stderr)
            if selected is None:
                assert not output.exists(), name
                assert "No single Flang include directory" in result.stderr
            else:
                assert output.read_text() == str(prefixes[selected]), name
            print(name, "passed")


if __name__ == "__main__":
    main()
