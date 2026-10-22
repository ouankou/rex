"""Check variable instrumentation callbacks and generated C/C++ semantics."""
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
    tutorial, c_compiler, cxx_compiler = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="rex_tutorial_access_") as name:
        directory = Path(name)
        for extension, compiler in (("c", c_compiler), ("cpp", cxx_compiler)):
            source = directory / f"rex_tutorial_access.{extension}"
            source.write_text("""int rex_access(void) {
  int x = 1, y = 3, z = 5;
  x = y;
  x = y + z;
  x += y;
  ++x;
  return x;
}
""")
            runner = directory / f"rex_tutorial_access_runner.{extension}"
            runner.write_text(
                ('extern "C" {\n' if extension == "cpp" else "") +
                "int reads, writes;\n"
                "void beforeRead(void) { ++reads; }\n"
                "void beforeWrite(void) { ++writes; }\n" +
                ("}\n" if extension == "cpp" else "") +
                "int rex_access(void);\n"
                "int main(void) { int value = rex_access();\n"
                "  return value == 12 && reads == 7 && writes == 4 ? 0 : 1; }\n")
            output = directory / f"rex_tutorial_access_output.{extension}"
            executable = directory / f"rex_tutorial_access_{extension}"
            run([tutorial, "-rose:output", str(output), "-c", str(source)], directory)
            run([compiler, "-Werror", str(output), str(runner),
                 "-o", str(executable)], directory)
            run([str(executable)], directory)


if __name__ == "__main__":
    main()
