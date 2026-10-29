#!/usr/bin/env python3
"""Evaluate LLVM version metadata without importing native build targets."""

from pathlib import Path
import subprocess
import sys
import tempfile


module = Path(sys.argv[1]).resolve()
cmake = sys.argv[2]
with tempfile.TemporaryDirectory(prefix="rex-llvm-version-") as temp:
    root = Path(temp) / "native package"
    package = root / "lib/cmake/llvm"
    package.mkdir(parents=True)
    config = package / "LLVMConfig.cmake"
    version = package / "LLVMConfigVersion.cmake"
    config.write_text('message(FATAL_ERROR "Native targets were imported")\n')
    script = root / "check.cmake"
    script.write_text(f'''
include("{module}")
set(PACKAGE_VERSION "unrelated parent metadata")
rose_read_llvm_package_version("{root}" actual)
if(NOT actual STREQUAL "23.1.8git")
  message(FATAL_ERROR "Incorrect version: '${{actual}}'")
endif()
if(NOT PACKAGE_VERSION STREQUAL "unrelated parent metadata")
  message(FATAL_ERROR "Package variables leaked into the caller")
endif()
''')
    cases = [
        ("quoted", 'set(PACKAGE_VERSION "23.1.8git")\n', None),
        ("computed", 'set(patch 8)\nset(PACKAGE_VERSION\n "23.1.${patch}git")\n', None),
        ("empty", 'set(PACKAGE_VERSION "")\n', "valid PACKAGE_VERSION"),
        ("absent", '# no version value\n', "valid PACKAGE_VERSION"),
        ("malformed", 'set(PACKAGE_VERSION "23.1.8 garbage")\n', "valid PACKAGE_VERSION"),
        ("missing_version_file", None, "LLVM package is missing"),
        ("missing_config", 'set(PACKAGE_VERSION "23.1.8git")\n', "LLVM package is missing"),
        ("directory_config", 'set(PACKAGE_VERSION "23.1.8git")\n', "LLVM package is missing"),
    ]
    for name, metadata, error in cases:
        if metadata is None:
            version.unlink()
        else:
            version.write_text(metadata)
        if name == "missing_config":
            config.unlink()
        elif name == "directory_config":
            config.mkdir()
        result = subprocess.run([cmake, "-P", str(script)], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if error is None:
            if result.returncode:
                raise SystemExit(f"{name}: unexpected failure\n{result.stdout}")
        elif result.returncode == 0 or error not in result.stdout:
            raise SystemExit(f"{name}: expected '{error}'\n{result.stdout}")
print("LLVM version metadata is evaluated in isolation; invalid packages fail")
