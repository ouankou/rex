import argparse
import os
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--outline", required=True)
parser.add_argument("--compiler", required=True)
parser.add_argument("--nm", required=True)
parser.add_argument("--runtime", required=True)
parser.add_argument("--include", required=True)
parser.add_argument("--language", choices=("c", "cpp"), required=True)
parser.add_argument("--mode", choices=("default", "explicit", "link", "static"), required=True)
parser.add_argument("--copy-source", action="store_true")
args = parser.parse_args()

# Build and execute artifacts inside the CTest build directory; system temporary
# filesystems may prohibit execution. Each test owns an isolated subdirectory.
with tempfile.TemporaryDirectory(
    prefix="rex_outliner_output_", dir=pathlib.Path.cwd()
) as directory:
    work = pathlib.Path(directory)
    source = work / ("rex_outliner_object." + args.language)
    source.write_text(
        "int main(void) {\n int value = 0;\n#pragma rose_outline\n"
        " { value = 7; }\n return value != 7;\n}\n"
    )
    env = dict(os.environ, ROSE_TEST_OUTPUT_DIR=directory)

    def run(command):
        subprocess.run(command, cwd=work, env=env, check=True, timeout=90)

    flags = ["-rose:outline:output_path", "."]
    flags += ["-rose:outline:new_file" if args.mode == "static" else "-rose:outline:use_dlopen"]
    if args.copy_source:
        flags += ["-rose:outline:copy_orig_file"]
    if args.mode == "default":
        flags += ["-c"]
    elif args.mode == "explicit":
        flags += ["-c", "-o", "primary.o"]
    else:
        flags += ["-o", "primary.exe"]
        if args.mode == "link":
            flags += [args.runtime, "-ldl"]
    run([args.outline, "-I" + args.include] + flags + [str(source)])
    obj = work / ("primary.o" if args.mode == "explicit" else "rex_outliner_object.o")
    symbols = subprocess.check_output([args.nm, "--defined-only", str(obj)], text=True)
    if not any(line.split()[-1:] == ["main"] for line in symbols.splitlines()):
        raise RuntimeError("original program object lost its main definition")
    if args.mode != "static":
        if list(work.glob("*.o")) != [obj]:
            raise RuntimeError("dlopen library was compiled as a main-program object")
        libraries = [
            file for file in work.glob("*." + args.language)
            if file.name not in (source.name, "rose_" + source.name)
        ]
        if len(libraries) != 1:
            raise RuntimeError("expected exactly one separate library source")
        library = libraries[0]
        run([args.compiler, "-shared", "-fPIC", "-I" + args.include,
             str(library), "-o", str(library.with_suffix(".so"))])
    if args.mode in ("link", "static"):
        run([str(work / "primary.exe")])
