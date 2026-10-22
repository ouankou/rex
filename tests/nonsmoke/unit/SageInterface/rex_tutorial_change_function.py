#!/usr/bin/env python3
"""Typed callee matching must preserve overloads and indirect calls."""
from pathlib import Path
import subprocess
import sys
import tempfile

translator, *flags = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="rex-tutorial-change-function-") as name:
    root = Path(name)
    for language in ("c", "cpp"):
        source = root / f"rex_tutorial_change_function.{language}"
        cpp = language == "cpp"
        source.write_text("""typedef long ssize_t;
typedef unsigned long size_t;
typedef ssize_t rex_result;
typedef size_t rex_count;
rex_result send(int sock, const void *message, rex_count length, int flags) {
  return 7;
}
""" + ("""ssize_t send() { return 17; }
ssize_t send(int value) { return 19; }
ssize_t send(int sock, void *message, size_t length, int flags) { return 23; }
namespace rex_nested {
ssize_t send(int sock, const void *message, size_t length, int flags) { return 29; }
}
extern "C"
""" if cpp else "") + """ssize_t contest_call(int sock, const void *message, size_t length, int flags) {
  return sock == 3 && message != 0 && length == 5 && flags == 9 ? 111 : 99;
}
int main(void) {
  const char *message = "hello";
  rex_result (*indirect)(int, const void *, rex_count, int) = &send;
  if (send(3, message, 5, 9) != 111) return 1;
  if (send(3, message, 5, 9) != 111) return 2;
  if (indirect(3, message, 5, 9) != 7) return 3;
""" + ("""  char mutable_message[6] = "hello";
  if (send() != 17 || send(2) != 19) return 4;
  if (send(3, mutable_message, 5, 9) != 23) return 5;
  if (rex_nested::send(3, message, 5, 9) != 29) return 6;
""" if cpp else "") + "  return 0;\n}\n")
        executable = root / f"rex_tutorial_change_function_{language}"
        subprocess.run([translator, *flags, str(source), "-o", str(executable)],
                       cwd=root, check=True)
        subprocess.run([str(executable)], cwd=root, check=True)
