import pathlib
import subprocess
import sys
import tempfile


def check_copy(driver, extension, mode, directory):
    source = directory / ("rex_copy_input." + extension)
    output = directory / ("rex_copy_output." + extension)
    text = ""
    if mode != "empty":
        text = (
            "program rex_copy\n  integer :: value\n  value = 1\n"
            "end program rex_copy\n"
            if extension == "f90"
            else "int rex_copy_value = 1;\n"
        )
    if mode == "commented":
        text = (
            "! REX_KEEP_COMMENT\n"
            if extension == "f90"
            else "// REX_KEEP_COMMENT\n#define REX_COPY_MACRO 1\n"
        ) + text
    source.write_text(text)
    subprocess.run(
        [driver, str(source), str(output)], cwd=directory, check=True, timeout=90
    )
    generated = output.read_text()
    if generated.count("REX_KEEP_COMMENT") != int(mode == "commented"):
        raise RuntimeError("copied source changed comment multiplicity")
    if extension != "f90" and generated.count("#define REX_COPY_MACRO") != int(
        mode == "commented"
    ):
        raise RuntimeError("copied source changed directive multiplicity")


if len(sys.argv) != 3 or sys.argv[2] not in ("c", "cpp", "f90"):
    raise SystemExit("expected driver path and c, cpp, or f90")
for mode in ("empty", "plain", "commented"):
    with tempfile.TemporaryDirectory(prefix="rex_copied_preprocessing_") as directory:
        check_copy(sys.argv[1], sys.argv[2], mode, pathlib.Path(directory))
