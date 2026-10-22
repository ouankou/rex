"""Keep exact compiler aliases outside Memcheck and produced programs inside."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("wrapper", "cc", "cxx", "suppressions", "work"):
        parser.add_argument(name)
    parser.add_argument("options", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    launcher = work / "rex_memcheck_compiler_launcher.sh"
    launcher.write_text('''#!/bin/bash
compiler="$1"
source_file="$2"
program="$3"
shift 3
"$compiler" -g -O0 "$source_file" -o "$program" || exit 2
"$program" "$@"
# The wrapper must retain a child's memory failure when its parent succeeds.
exit 0
''')
    source = '''#include <stdio.h>
#include <stdlib.h>
__attribute__((noinline)) static void control(int release) {
  char *memory = (char *)malloc(317);
  if (!memory) abort();
  memory[0] = 'R';
  putchar(memory[0]);
  if (release) free(memory);
}
int main(int argc, char **argv) {
  (void)argv;
  control(argc > 1);
  return 0;
}
'''
    for suffix, compiler in (("c", args.cc), ("cpp", args.cxx)):
        specimen = work / ("rex_memcheck_compiler_boundary." + suffix)
        specimen.write_text(source)
        for released in (False, True):
            label = suffix + ("-released" if released else "-leaked")
            log = work / (label + ".log")
            result = subprocess.run([
                args.wrapper, *args.options, "--track-origins=yes",
                "--error-exitcode=79", "--suppressions=" + args.suppressions,
                "--log-file=" + str(log), "/bin/bash", str(launcher),
                compiler, str(specimen), str(work / (suffix + ".exe")),
                *(["free"] if released else []),
            ], capture_output=True, text=True)
            log.with_suffix(".stdout").write_text(result.stdout + result.stderr)
            report = log.read_text()
            assert result.returncode == (0 if released else 79), (
                result.returncode, report, result.stdout, result.stderr)
            assert "Command: " + compiler + " " not in report, report
            if released:
                assert "REX_MEMCHECK_ERROR_BEGIN" not in report, report
            else:
                assert "317 bytes in 1 blocks are definitely lost" in report, report
                assert specimen.name in report, report
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
