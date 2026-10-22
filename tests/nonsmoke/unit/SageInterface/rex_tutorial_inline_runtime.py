"""Exercise inlining, symbol renaming, flattening, and generated execution."""
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


with tempfile.TemporaryDirectory(prefix="rex_tutorial_inline_") as name:
    directory = Path(name)
    source = directory / "rex_tutorial_inline_input.cpp"
    source.write_text("""inline int increment(int value) { return value + 1; }
int main() { return increment(3) == 4 ? 0 : 1; }
""")
    output = directory / "rex_tutorial_inline_output.cpp"
    executable = directory / "rex_tutorial_inline_runtime"
    run([sys.argv[1], "-std=c++17", "-rose:output", str(output),
         "-c", str(source)], directory)
    run([sys.argv[2], "-std=c++17", str(output), "-o", str(executable)], directory)
    run([str(executable)], directory)
