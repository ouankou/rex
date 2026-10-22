#!/usr/bin/env python3
"""Compile and execute translated complex literals and typed operand edits."""

import argparse
from pathlib import Path
import subprocess
import tempfile


SOURCE = r"""
#ifndef REX_EXPECT_DELTA
#define REX_EXPECT_DELTA 0
#endif
#define REX_IMAGINARY_MACRO 6.0FJ
#define REX_ARITHMETIC_MACRO (8 + 9)
#define REX_CONDITIONAL_MACRO (8 < 9) ? 8 : 9
#define REX_BINARY_CONDITIONAL_MACRO (8 ?: 9)
#define REX_SIZEOF_MACRO sizeof(int)
#define REX_ALIGNOF_MACRO __alignof__(int)
#ifdef __cplusplus
int &rex_conditional_lvalue(bool choose, int &left, int &right) {
  return choose ? left : right;
}
namespace rex_literal_operator {
constexpr long double operator ""i(long double value) { return value + 10; }
long double rex_selected_operator(void) { return 1.5i; }
}
int rex_integer_kind(int value) {
  return value == -__INT_MAX__ - 1 ? 1 : 0;
}
int rex_integer_kind(long value) {
  return value == -__LONG_MAX__ - 1L ? 2 : 0;
}
int rex_integer_kind(long long value) {
  return value == -__LONG_LONG_MAX__ - 1LL ? 3 : 0;
}
int rex_integer_kind(signed char value) { return value == 2 ? 4 : 0; }
int rex_integer_kind(unsigned char value) { return value == 2 ? 5 : 0; }
int rex_integer_kind(short value) { return value == 2 ? 6 : 0; }
int rex_integer_kind(unsigned short value) { return value == 2 ? 7 : 0; }
int rex_generated_min_int(void) { return rex_integer_kind(0); }
int rex_generated_min_long(void) { return rex_integer_kind(0L); }
int rex_generated_min_long_long(void) { return rex_integer_kind(0LL); }
int rex_generated_narrow_signed_char(void) { return rex_integer_kind(0); }
int rex_generated_narrow_unsigned_char(void) { return rex_integer_kind(0); }
int rex_generated_narrow_short(void) { return rex_integer_kind(0); }
int rex_generated_narrow_unsigned_short(void) { return rex_integer_kind(0); }
#endif
_Complex float rex_float(void) { return 2.0fi; }
_Complex double rex_double(void) { return 3.0i; }
_Complex long double rex_long_double(void) { return 4.0Li; }
_Complex int rex_int(void) { return 5i; }
_Complex float rex_macro(void) { return REX_IMAGINARY_MACRO; }
_Complex double rex_hex(void) { return 0x1.Cp2J; }
int rex_arithmetic_macro(void) { return REX_ARITHMETIC_MACRO * 2; }
int rex_conditional_macro(void) { return REX_CONDITIONAL_MACRO; }
int rex_binary_conditional_macro(void) { return REX_BINARY_CONDITIONAL_MACRO; }
unsigned long rex_sizeof_macro(void) { return REX_SIZEOF_MACRO; }
unsigned long rex_alignof_macro(void) { return REX_ALIGNOF_MACRO; }
_Complex float rex_generated_infinity(void) { return 0; }
_Complex float rex_generated_nan(void) { return 0; }
float rex_generated_negative_float(void) { return 0; }
double rex_generated_negative_double(void) { return 0; }
int main(void) {
  if (__real__ rex_float() != 0 || __imag__ rex_float() != 2 + REX_EXPECT_DELTA) return 1;
  if (__real__ rex_double() != 0 || __imag__ rex_double() != 3 + REX_EXPECT_DELTA) return 2;
  if (__real__ rex_long_double() != 0 || __imag__ rex_long_double() != 4 + REX_EXPECT_DELTA) return 3;
  if (__real__ rex_int() != 0 || __imag__ rex_int() != 5 + REX_EXPECT_DELTA) return 4;
  if (__real__ rex_macro() != 0 || __imag__ rex_macro() != 6 + REX_EXPECT_DELTA) return 5;
  if (__real__ rex_hex() != 0 || __imag__ rex_hex() != 7 + REX_EXPECT_DELTA) return 6;
  if (!__builtin_signbit(__real__ rex_generated_infinity())) return 7;
  if (!__builtin_isinf(__imag__ rex_generated_infinity())) return 8;
  if (!__builtin_signbit(__real__ rex_generated_nan())) return 9;
  if (!__builtin_isnan(__imag__ rex_generated_nan())) return 10;
  if (!__builtin_isinf(rex_generated_negative_float()) ||
      !__builtin_signbit(rex_generated_negative_float())) return 18;
  if (!__builtin_isinf(rex_generated_negative_double()) ||
      !__builtin_signbit(rex_generated_negative_double())) return 19;
  if (rex_arithmetic_macro() != (17 + REX_EXPECT_DELTA) * 2) return 11;
  if (rex_conditional_macro() != 8 + REX_EXPECT_DELTA) return 13;
  if (rex_binary_conditional_macro() != 8 + REX_EXPECT_DELTA) return 14;
  if (rex_sizeof_macro() != sizeof(int)) return 15;
  if (rex_alignof_macro() != __alignof__(int)) return 16;
#ifdef __cplusplus
  if (rex_generated_min_int() != 1 || rex_generated_min_long() != 2 ||
      rex_generated_min_long_long() != 3) return 20;
  if (rex_generated_narrow_signed_char() != 4 ||
      rex_generated_narrow_unsigned_char() != 5 ||
      rex_generated_narrow_short() != 6 ||
      rex_generated_narrow_unsigned_short() != 7) return 21;
  if (rex_literal_operator::rex_selected_operator() != 11.5L) return 12;
  int left = 0, right = 0;
  rex_conditional_lvalue(true, left, right) = 3;
  rex_conditional_lvalue(false, left, right) = 4;
  if (left != 3 || right != 4) return 17;
#endif
  return 0;
}
"""


def run(command, directory):
    result = subprocess.run(command, cwd=directory, text=True, capture_output=True)
    assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--translator", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--cxx", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rex complex literals ") as temporary:
        root = Path(temporary)
        for extension, compiler, standard in (("c", args.cc, "gnu11"), ("cpp", args.cxx, "gnu++17")):
            for mode in ("unchanged", "mutate"):
                work = root / f"{extension}-{mode}"
                work.mkdir()
                source = work / f"rex_complex_literal_input.{extension}"
                source.write_text(SOURCE)
                delta = f"-DREX_EXPECT_DELTA={int(mode == 'mutate')}"
                run([args.translator, mode, f"-std={standard}", delta, "-c", str(source)], work)
                output = work / ("rose_" + source.name)
                assert output.is_file()
                program = work / "rex_complex_literal_output"
                run([compiler, f"-std={standard}", delta, str(output), "-o", str(program)], work)
                run([str(program)], work)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
