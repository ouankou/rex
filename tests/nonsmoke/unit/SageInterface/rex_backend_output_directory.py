#!/usr/bin/env python3
"""Generated object paths must agree between compilation and linking."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

translator = Path(sys.argv[1])
flags = sys.argv[2:]
with tempfile.TemporaryDirectory(prefix="rex-backend-output-") as directory:
    root = Path(directory)
    for suffix in ("c", "cpp"):
        for sources in (1, 2):
            for named in (False, True):
                work = root / f"{suffix}-{sources}-{named}"
                work.mkdir()
                output = work / 'object files "quoted"'
                env = dict(os.environ, ROSE_TEST_OUTPUT_DIR=str(output))
                main = work / f"rex_backend_main.{suffix}"
                inputs = [main]
                if sources == 1:
                    main.write_text("int main(void) { return 0; }\n")
                else:
                    main.write_text("int rex_helper(void); int main(void) { return rex_helper() != 19; }\n")
                    helper = work / f"rex_backend_helper.{suffix}"
                    helper.write_text("int rex_helper(void) { return 19; }\n")
                    inputs.append(helper)
                executable = work / ("rex_backend_program" if named else "a.out")
                command = [str(translator), *flags, *map(str, inputs)]
                if named:
                    command.extend(["-o", str(executable)])
                subprocess.run(command, cwd=work, env=env, check=True)
                for source in inputs:
                    assert (output / source.with_suffix(".o").name).is_file()
                    assert not source.with_suffix(".o").exists()
                subprocess.run([str(executable)], cwd=work, env=env, check=True)

    # Failure to create the requested output directory must be reported. The
    # compiler must not silently write the generated source beside the input.
    work = root / "invalid-output"
    work.mkdir()
    source = work / "rex_backend_invalid_output.c"
    source.write_text("int main(void) { return 0; }\n")
    output = work / "regular-file"
    output.write_text("occupied\n")
    result = subprocess.run(
        [str(translator), *flags, "-c", str(source)], cwd=work,
        env=dict(os.environ, ROSE_TEST_OUTPUT_DIR=str(output)),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert result.returncode == 1, (result.returncode, result.stdout)
    assert "REX_BACKEND_ERROR[filesystem]" in result.stdout, result.stdout
    assert output.read_text() == "occupied\n"
    assert not (work / "rose_rex_backend_invalid_output.c").exists()
    assert not source.with_suffix(".o").exists()
