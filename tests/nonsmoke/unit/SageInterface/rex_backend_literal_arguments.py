#!/usr/bin/env python3
"""Compiler and linker argv must preserve literal quotes and backslashes."""

from pathlib import Path
import subprocess
import sys
import tempfile


translator, c_compiler, cxx_compiler = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-backend-arguments-") as directory:
    root = Path(directory)
    for suffix, compiler, standard in (("c", c_compiler, "c11"),
                                       ("cpp", cxx_compiler, "c++20")):
        for index, name in enumerate(('single"quote', 'paired"quotes"tail',
                                      "space path", "back\\slash")):
            include = root / f"{suffix}-{name}"
            include.mkdir()
            work = root / f"run-{suffix}-{index}"
            work.mkdir()
            (include / "rex_cli_payload.h").write_text("#define REX_CLI_VALUE 19\n")
            helper = work / f"rex_cli_helper.{suffix}"
            helper.write_text("int rex_cli_helper(void) { return 19; }\n")
            obj = include / "rex_cli_helper.o"
            subprocess.run([str(compiler), f"-std={standard}", "-c", str(helper),
                            "-o", str(obj)], check=True)
            source = work / f"rex_cli_main.{suffix}"
            source.write_text(r'''#include "rex_cli_payload.h"
#include <string.h>
int rex_cli_helper(void);
int main(void) {
  const char expected[] = "quote\" and \\ slash";
  return rex_cli_helper() != REX_CLI_VALUE || strcmp(REX_CLI_TEXT, expected) != 0;
}
''')
            if suffix == "cpp":
                source.rename(source.with_suffix(".c"))
                source.symlink_to(source.with_suffix(".c").name)
            for tool, name in ((compiler, "original"), (translator, "translated")):
                executable = include / f"rex_cli_{name}"
                subprocess.run([str(tool), f"-std={standard}", "-I", str(include),
                                "-Iabsent-includes", "-Labsent-libraries",
                                r'-DREX_CLI_TEXT="quote\" and \\ slash"',
                                str(source), str(obj), "-o", str(executable)],
                               cwd=work, check=True)
                subprocess.run([str(executable)], check=True)

    # These inputs pass the frontend and compilation, then fail at the final
    # project-level link. A packed wait status of 256 must not escape as exit 0.
    for suffix, compiler in (("c", c_compiler), ("cpp", cxx_compiler)):
        work = root / f"failed-link-{suffix}"
        work.mkdir()
        main = work / f"rex_backend_missing_symbol.{suffix}"
        helper = work / f"rex_backend_other_source.{suffix}"
        main.write_text("int rex_unresolved(void); int main(void) { return rex_unresolved(); }\n")
        helper.write_text("int rex_other(void) { return 0; }\n")
        obj = work / "rex_backend_missing_symbol.o"
        subprocess.run([str(compiler), "-c", str(main), "-o", str(obj)], check=True)
        for inputs in ((main, helper), (obj,)):
            for tool in (compiler, translator):
                output = work / "program"
                result = subprocess.run([str(tool), *map(str, inputs), "-o", str(output)],
                                        cwd=work, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True)
                assert result.returncode != 0, result.stdout
                assert not output.exists(), result.stdout
