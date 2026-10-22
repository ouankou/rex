#!/usr/bin/env python3
"""Check the declaration tutorials through compiled generated code."""
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, directory):
    result = subprocess.run(command, cwd=directory, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command!r}\n"
                           f"{result.stdout}")


def main():
    compiler, *tutorials = sys.argv[1:]
    if len(tutorials) != 3:
        raise ValueError("expected a C++ compiler and three tutorial executables")
    with tempfile.TemporaryDirectory(prefix="rex_tutorial_function_") as name:
        directory = Path(name)
        source = directory / "rex_tutorial_function_runtime.cpp"
        source.write_text("""int my_function(int &);
int main() {
  int value = 4;
  return my_function(value) == 5 && value == 5 ? 0 : 1;
}
""")
        for index, tutorial in enumerate(tutorials):
            output = directory / f"rex_tutorial_function_output_{index}.cpp"
            executable = directory / f"rex_tutorial_function_runtime_{index}"
            run([tutorial, "-std=c++17", "-rose:output", str(output),
                 "-c", str(source)], directory)
            run([compiler, "-std=c++17", "-Werror=return-type", str(output),
                 "-o", str(executable)], directory)
            run([str(executable)], directory)


if __name__ == "__main__":
    main()
