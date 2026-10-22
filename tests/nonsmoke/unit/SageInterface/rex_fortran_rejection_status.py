#!/usr/bin/env python3
"""A rejection test must reject crashes, extra errors, output, and memory failures."""

from pathlib import Path
import subprocess
import sys
import tempfile

contract, specimen, compiler = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-fortran-rejection-status-") as name:
    # Exercise real signal termination without abandoning a Python interpreter
    # and its allocations. This child is still fully instrumented by Memcheck.
    signal_probe = Path(name) / "rex_fortran_rejection_signal"
    subprocess.run([str(compiler), "-std=c11", str(Path(__file__).with_name(
        "rex_fortran_rejection_signal.c")), "-o", str(signal_probe)], check=True)
    probe = Path(name) / "rex_fortran_compiler_probe.py"
    probe.write_text('''#!/usr/bin/env python3
import os
from pathlib import Path
import sys
mode, source = sys.argv[1], sys.argv[-1]
messages = [(3, 19, "Must have INTEGER type, but is REAL(4)"),
            (3, 36, "Must have CHARACTER type, but is REAL(4)")]
if mode == "missing": messages.pop()
if mode == "extra": messages.append((4, 1, "Unexpected declaration"))
if mode == "wrong": messages[0] = (3, 19, "Invalid operands")
for line, column, message in messages:
    print(f"{source}:{line}:{column}: error: {message}")
if mode == "artifact": Path("rex_rejected.o").write_bytes(b"incorrect output")
if mode == "memory":
    for key in ("ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"):
        assert os.environ[key].endswith("exitcode=86:halt_on_error=1")
    sys.exit(86)
sys.exit(0 if mode == "success" else 1)
''')
    probe.chmod(0o755)
    for mode in ("valid", "missing", "extra", "wrong", "artifact", "memory", "success", "crash"):
        command = signal_probe if mode == "crash" else probe
        result = subprocess.run([sys.executable, str(contract), str(command),
                                 str(specimen), mode], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        expected_success = mode == "valid"
        if (result.returncode == 0) != expected_success:
            raise RuntimeError(f"contract mishandled {mode}: {result.returncode}\n{result.stdout}")
