#!/usr/bin/env python3
"""Require exact extended AST values and execute emitted typed edits."""

import argparse
from pathlib import Path
import subprocess
import tempfile

SOURCE = r"""
#ifndef REX_EXPECT_DELTA
#define REX_EXPECT_DELTA 0
#endif
long double rex_extended_precision(void) { return 0x1.000000000000001p0L; }
long double rex_extended_exponent(void) { return 0x1p1024L; }
__float128 rex_quad_precision(void) { return 0x1.0000000000000000000000000001p0Q; }
__float128 rex_quad_infinity(void) { return 0; }
__float128 rex_quad_nan(void) { return 0; }
int main(void) {
  if (!__builtin_isinf(rex_quad_infinity()) || !__builtin_signbit(rex_quad_infinity())) return 4;
  __float128 nan = rex_quad_nan();
  unsigned long long words[2];
  __builtin_memcpy(words, &nan, sizeof(nan));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  if (words[0] != 0x123456789abcdef0ULL || words[1] != 0xffff800000005678ULL) return 5;
#else
  if (words[1] != 0x123456789abcdef0ULL || words[0] != 0xffff800000005678ULL) return 5;
#endif

  if (rex_extended_precision() != 0x1.000000000000001p0L + REX_EXPECT_DELTA) return 1;
  if (rex_extended_exponent() != 0x1p1024L + REX_EXPECT_DELTA) return 2;
  if (rex_quad_precision() != 0x1.0000000000000000000000000001p0Q + REX_EXPECT_DELTA) return 3;
  return 0;
}
"""


def run(command, directory):
    result = subprocess.run(command, cwd=directory, text=True, capture_output=True)
    assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("translator", "cc", "cxx"):
        parser.add_argument("--" + option, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rex extended literals ") as temporary:
        root = Path(temporary)
        for extension, compiler, standard in (("c", args.cc, "gnu11"), ("cpp", args.cxx, "gnu++17")):
            for mode in ("unchanged", "mutate"):
                work = root / f"{extension}-{mode}"
                work.mkdir()
                source = work / f"rex_extended_floating_input.{extension}"
                source.write_text(SOURCE)
                delta = f"-DREX_EXPECT_DELTA={int(mode == 'mutate')}"
                run([args.translator, mode, f"-std={standard}", delta, "-c", str(source)], work)
                output = work / ("rose_" + source.name)
                assert output.is_file()
                program = work / "rex_extended_floating_output"
                run([compiler, f"-std={standard}", delta, str(output), "-o", str(program)], work)
                run([str(program)], work)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
