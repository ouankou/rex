#!/usr/bin/env python3
"""Require the installed copy example to preserve complete source ownership."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--translator", required=True)
    parser.add_argument("--fortran", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rex copy replacement ") as temp:
        root = Path(temp)
        header = root / "rex_copy_replacement_header.h"
        header.write_text("struct item { int value; };\n"
                          "int helper(struct item declaration_value);\n")
        source = root / "rex_copy_replacement_input.c"
        source.write_text('#include "rex_copy_replacement_header.h"\n'
                          '// Preserve this source comment.\n'
                          'int main(void) { struct item value = {41};\n'
                          '  return helper(value) != 42; }\n')
        second = root / "rex_copy_replacement_helper.c"
        second.write_text('#include "rex_copy_replacement_header.h"\n'
                          'int helper(struct item value) { return value.value + 1; }\n')
        template = root / "rex_copy_replacement_template.cpp"
        template.write_text('template<class T> struct box { T value; };\n'
                            'int helper(box<int> declaration_value);\n'
                            'int helper(box<int> redeclared_value);\n'
                            'int helper(box<int> value) { return value.value + 1; }\n'
                            'int main() { return helper(box<int>{41}) != 42; }\n')
        cases = [("c", [str(source), str(second)]),
                 ("c-tokens", ["-rose:unparse_tokens", str(source), str(second)]),
                 ("cxx", [str(template)]),
                 ("cxx-tokens", ["-rose:unparse_tokens", str(template)])]
        if args.fortran:
            fortran = root / "rex_copy_replacement_module.f90"
            fortran.write_text("module values\n  integer, parameter :: expected = 42\n"
                               "contains\n  integer function answer()\n"
                               "    answer = expected\n  end function\nend module\n"
                               "program main\n  use values\n"
                               "  if (answer() /= 42) stop 1\nend program\n")
            cases.append(("fortran", [str(fortran)]))
        for name, inputs in cases:
            directory = root / name
            directory.mkdir()
            result = subprocess.run([args.translator, "-c", *inputs], cwd=directory,
                                    text=True, capture_output=True)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            assert "successful test" in result.stderr, (name, result.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
