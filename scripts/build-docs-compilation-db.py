#!/usr/bin/env python3
from __future__ import annotations

import argparse
import fnmatch
import json
import os
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path


def _parse_scalar(lines: list[str], key: str) -> str | None:
    for line in lines:
        stripped = line.lstrip()
        if stripped.startswith(f"{key}:"):
            value = stripped.split(":", 1)[1].strip()
            if value:
                return value.strip("\"'")
    return None


def _parse_list(lines: list[str], key: str) -> list[str]:
    items: list[str] = []
    in_block = False
    indent = None
    for line in lines:
        stripped = line.lstrip()
        if not in_block:
            if stripped.startswith(f"{key}:"):
                in_block = True
                indent = len(line) - len(stripped) + 2
            continue
        if not stripped:
            continue
        leading = len(line) - len(stripped)
        if leading < (indent or 0):
            break
        if stripped.startswith("- "):
            item = stripped[2:].strip().strip("\"'")
            if item:
                items.append(item)
    return items


def _resolve_config_paths(
    values: list[str], config_dir: Path, default: list[Path]
) -> list[Path]:
    if not values:
        return default
    paths: list[Path] = []
    for value in values:
        path = Path(value)
        paths.append(
            (config_dir / path).resolve() if not path.is_absolute() else path.resolve()
        )
    return paths


def _is_under(path: Path, base: Path) -> bool:
    try:
        path.relative_to(base)
        return True
    except ValueError:
        return False


def _arguments(entry: dict) -> list[str]:
    arguments = entry.get("arguments")
    if isinstance(arguments, list) and arguments:
        return [str(arg) for arg in arguments]
    command = entry.get("command")
    if isinstance(command, str) and command:
        return shlex.split(command)
    return []


def _canonical_command(command: str, directory: Path) -> Path | None:
    path = Path(command)
    if path.is_absolute():
        return path.resolve()
    if path.parent != Path("."):
        return (directory / path).resolve()
    resolved = shutil.which(command)
    return Path(resolved).resolve() if resolved else None


def _replace_compiler(
    arguments: list[str], compiler: str, directory: Path
) -> list[str]:
    selected_compiler = _canonical_command(compiler, directory)
    if selected_compiler is None:
        raise SystemExit(f"selected compiler is not resolvable: {compiler}")
    selected_name = Path(compiler).name
    compiler_indices = [
        index
        for index, argument in enumerate(arguments)
        if Path(argument).name == selected_name
        and _canonical_command(argument, directory) == selected_compiler
    ]
    if not compiler_indices:
        raise SystemExit(
            f"compile command does not contain selected compiler: {selected_compiler}"
        )
    if len(compiler_indices) != 1:
        raise SystemExit(
            "compile command contains selected compiler more than once: "
            f"{selected_compiler}"
        )
    return [compiler, *arguments[compiler_indices[0] + 1 :]]


def _cmake_bracket_argument(value: str) -> str:
    equals = ""
    while f"]{equals}]" in value:
        equals += "="
    return f"[{equals}[{value}]{equals}]"


def _cmake_state_cxx_compiler(state: Path) -> str:
    cmake = shutil.which("cmake")
    if cmake is None:
        raise SystemExit(
            "cmake is required to read the generated C++ compiler state: "
            f"{state}"
        )
    with tempfile.TemporaryDirectory(prefix="rex-docs-cmake-state-") as temp_dir:
        script = Path(temp_dir) / "read-compiler.cmake"
        output = Path(temp_dir) / "compiler.txt"
        script.write_text(
            f"include({_cmake_bracket_argument(str(state))})\n"
            "if(NOT DEFINED CMAKE_CXX_COMPILER OR "
            'CMAKE_CXX_COMPILER STREQUAL "")\n'
            '  message(FATAL_ERROR "generated CMake C++ compiler state is empty")\n'
            "endif()\n"
            f"file(WRITE {_cmake_bracket_argument(str(output))} "
            '"${CMAKE_CXX_COMPILER}")\n',
            encoding="utf-8",
        )
        result = subprocess.run(
            [cmake, "-P", str(script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0 or not output.is_file():
            detail = result.stderr.strip() or result.stdout.strip()
            raise SystemExit(
                f"failed to read generated CMake C++ compiler state {state}: "
                f"{detail or f'cmake exited {result.returncode}'}"
            )
        compiler = output.read_text(encoding="utf-8")
    if not compiler:
        raise SystemExit(f"generated CMake C++ compiler state is empty: {state}")
    return compiler


def _cmake_cxx_compiler(compile_db: Path) -> str:
    compile_db = compile_db.resolve()
    if not compile_db.is_file():
        raise SystemExit(f"compilation database does not exist: {compile_db}")
    cache = compile_db.parent / "CMakeCache.txt"
    if not cache.is_file():
        raise SystemExit(
            "documentation compilation database has no adjacent CMakeCache.txt: "
            f"{compile_db}"
        )
    prefix = "CMAKE_CXX_COMPILER:"
    values = [
        line.split("=", 1)[1]
        for line in cache.read_text(encoding="utf-8").splitlines()
        if line.startswith(prefix) and "=" in line
    ]
    if len(values) > 1 or (values and not values[0]):
        raise SystemExit(
            "CMakeCache.txt contains an ambiguous or empty "
            "CMAKE_CXX_COMPILER entry"
        )
    cache_compiler = values[0] if values else None

    # CMake 4 can keep a compiler selected before project() exclusively in its
    # generated language state rather than duplicating it in CMakeCache.txt.
    # Both files are CMake-owned configuration state.  Accept either exact
    # representation and require equality whenever both are present.
    state = cache.parent / "CMakeFiles" / "CMakeCXXCompiler.cmake"
    state_compiler = _cmake_state_cxx_compiler(state) if state.is_file() else None
    if (
        cache_compiler is not None
        and state_compiler is not None
        and cache_compiler != state_compiler
    ):
        raise SystemExit(
            "CMake cache/generated C++ compiler state mismatch: "
            f"{cache_compiler} != {state_compiler}"
        )
    compiler = cache_compiler or state_compiler
    if compiler is None:
        raise SystemExit(
            "CMake configuration contains no exact CMAKE_CXX_COMPILER in "
            f"{cache} or {state}"
        )
    return compiler


def _compiler_include_search_paths(
    compiler: str,
    language: str,
    probe_arguments: tuple[str, ...],
    directory: Path | None,
) -> list[Path]:
    environment = os.environ.copy()
    for variable in (
        "CPATH",
        "CPLUS_INCLUDE_PATH",
        "C_INCLUDE_PATH",
        "OBJC_INCLUDE_PATH",
    ):
        environment.pop(variable, None)
    result = subprocess.run(
        [compiler, *probe_arguments, "-E", "-x", language, "-", "-v"],
        input=b"",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
        env=environment,
        cwd=directory,
    )

    paths: list[Path] = []
    seen: set[Path] = set()
    in_block = False
    found_end = False
    for line in result.stderr.decode("utf-8").splitlines():
        if line.strip() == "#include <...> search starts here:":
            in_block = True
            continue
        if not in_block:
            continue
        if line.strip() == "End of search list.":
            found_end = True
            break
        path_text = line.strip()
        if not path_text or path_text.startswith("("):
            continue
        path = Path(path_text)
        if not path.is_absolute() and directory is not None:
            path = directory / path
        path = path.resolve()
        if path not in seen:
            seen.add(path)
            paths.append(path)
    if not in_block or not found_end:
        raise SystemExit(
            f"compiler did not report a complete {language} include search path list"
        )
    return paths


def _compiler_probe_arguments(arguments: list[str]) -> tuple[str, ...]:
    with_value = {
        "--config",
        "--config-system-dir",
        "--config-user-dir",
        "--gcc-toolchain",
        "--resource-dir",
        "--sysroot",
        "--target",
        "-B",
        "-gcc-toolchain",
        "-isysroot",
        "-resource-dir",
        "-stdlib++-isystem",
        "-target",
    }
    joined_prefixes = (
        "--config=",
        "--config-system-dir=",
        "--config-user-dir=",
        "--driver-mode=",
        "--gcc-install-dir=",
        "--gcc-toolchain=",
        "--gcc-triple=",
        "--resource-dir=",
        "--sysroot=",
        "--target=",
        "-B",
        "-gcc-toolchain=",
        "-isysroot=",
        "-mabi=",
        "-resource-dir=",
        "-stdlib=",
        "-target=",
    )
    standalone = {"-m32", "-m64"}
    result: list[str] = []
    index = 1
    while index < len(arguments):
        argument = arguments[index]
        if argument in with_value:
            if index + 1 >= len(arguments):
                raise SystemExit(
                    f"toolchain-selecting compiler option lacks a value: {argument}"
                )
            result.extend((argument, arguments[index + 1]))
            index += 2
            continue
        if argument in standalone or any(
            argument.startswith(prefix) and argument != prefix
            for prefix in joined_prefixes
        ):
            result.append(argument)
        index += 1
    return tuple(result)


def _compiler_probe_configurations(
    compiler: str, compile_db: Path | None
) -> list[tuple[Path | None, tuple[str, ...]]]:
    if compile_db is None:
        return [(None, ())]
    compile_db = compile_db.resolve()
    if not compile_db.is_file():
        raise SystemExit(f"compilation database does not exist: {compile_db}")
    database = json.loads(compile_db.read_text(encoding="utf-8"))
    if not isinstance(database, list):
        raise SystemExit("compile_commands.json is not a list")

    configurations: list[tuple[Path, tuple[str, ...]]] = []
    seen: set[tuple[Path, tuple[str, ...]]] = set()
    for entry in database:
        if not isinstance(entry, dict) or not entry.get("file"):
            continue
        directory = Path(entry.get("directory") or compile_db.parent).resolve()
        source = Path(entry["file"])
        source = (
            (directory / source).resolve()
            if not source.is_absolute()
            else source.resolve()
        )
        arguments = _arguments(entry)
        if not arguments or not _is_cpp_translation_unit(source, arguments):
            continue
        canonical = _replace_compiler(arguments, compiler, directory)
        configuration = (
            directory,
            _compiler_probe_arguments(canonical),
        )
        if configuration not in seen:
            seen.add(configuration)
            configurations.append(configuration)
    if not configurations:
        raise SystemExit(
            "compilation database has no C++ command owned by the selected compiler"
        )
    return configurations


def _classified_compiler_include_paths(
    compiler: str, compile_db: Path | None
) -> list[tuple[str, Path]]:
    classified: dict[str, list[Path]] = {
        "libc": [],
        "stdlib": [],
        "system": [],
    }
    assigned: dict[Path, str] = {}
    for directory, probe_arguments in _compiler_probe_configurations(
        compiler, compile_db
    ):
        cxx_paths = _compiler_include_search_paths(
            compiler, "c++", probe_arguments, directory
        )
        c_paths = set(
            _compiler_include_search_paths(
                compiler, "c", probe_arguments, directory
            )
        )
        for path in cxx_paths:
            path_text = str(path)
            if path not in c_paths:
                # The language-search-path difference is the compiler-owned
                # C++ standard library. Its installation prefix is irrelevant.
                kind = "stdlib"
            elif "/lib/clang/" in path_text or "/llvm" in path_text:
                kind = "system"
            else:
                kind = "libc"
            previous = assigned.get(path)
            if previous is not None and previous != kind:
                raise SystemExit(
                    "compiler include path has conflicting classifications "
                    f"across C++ commands: {path}: {previous} versus {kind}"
                )
            if previous is None:
                assigned[path] = kind
                classified[kind].append(path)
    return [
        (kind, path)
        for kind in ("libc", "stdlib", "system")
        for path in classified[kind]
    ]


def _explicit_language(arguments: list[str]) -> str | None:
    language = None
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument == "-x" and index + 1 < len(arguments):
            language = arguments[index + 1]
            index += 2
            continue
        if argument.startswith("-x") and len(argument) > 2:
            language = argument[2:]
        index += 1
    return language


def _is_cpp_translation_unit(path: Path, arguments: list[str]) -> bool:
    language = _explicit_language(arguments)
    if language is not None:
        return "c++" in language
    if path.suffix == ".C":
        return True
    return path.suffix.lower() in {".cc", ".cpp", ".cxx", ".c++", ".cp"}


def _is_cmake_pch_argument(argument: str) -> bool:
    return argument.rsplit("/", 1)[-1].startswith("cmake_pch.")


def _strip_cmake_pch_arguments(arguments: list[str]) -> list[str]:
    cleaned: list[str] = []
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in {"-Winvalid-pch", "-fpch-instantiate-templates"}:
            index += 1
            continue
        if argument in {"-include-pch", "-include"} and index + 1 < len(arguments):
            if _is_cmake_pch_argument(arguments[index + 1]):
                index += 2
                continue
        if argument == "-Xclang" and index + 1 < len(arguments):
            clang_argument = arguments[index + 1]
            if clang_argument in {"-include-pch", "-include"}:
                if (
                    index + 3 < len(arguments)
                    and arguments[index + 2] == "-Xclang"
                    and _is_cmake_pch_argument(arguments[index + 3])
                ):
                    index += 4
                    continue
                if index + 2 < len(arguments) and _is_cmake_pch_argument(
                    arguments[index + 2]
                ):
                    index += 3
                    continue
        if _is_cmake_pch_argument(argument):
            index += 1
            continue
        cleaned.append(argument)
        index += 1
    return cleaned


def _resolve_include_path(path: str, directory: Path) -> Path:
    include_path = Path(path)
    if not include_path.is_absolute():
        include_path = directory / include_path
    return include_path.resolve()


def _strip_stdlib_includes(
    arguments: list[str], directory: Path, stdlib_includes: set[Path]
) -> list[str]:
    cleaned: list[str] = []
    index = 0
    prefixes = ("-I", "-isystem", "-iquote", "-idirafter")
    while index < len(arguments):
        argument = arguments[index]
        if (
            argument in prefixes
            and index + 1 < len(arguments)
            and _resolve_include_path(arguments[index + 1], directory)
            in stdlib_includes
        ):
            index += 2
            continue
        if any(
            argument.startswith(prefix)
            and argument != prefix
            and _resolve_include_path(argument[len(prefix) :], directory)
            in stdlib_includes
            for prefix in prefixes
        ):
            index += 1
            continue
        cleaned.append(argument)
        index += 1
    return cleaned


def _insert_frontend_argument(arguments: list[str], value: str) -> None:
    if value in arguments:
        return
    arguments.insert(1, value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config")
    parser.add_argument("--out-dir")
    parser.add_argument("--compiler")
    parser.add_argument("--compile-db")
    query = parser.add_mutually_exclusive_group()
    query.add_argument("--print-cmake-cxx-compiler")
    query.add_argument("--print-compiler-include-paths")
    parser.add_argument("--overlay-dir", default="")
    parser.add_argument("--stdlib-include", action="append", default=[])
    args = parser.parse_args()

    if args.print_cmake_cxx_compiler:
        print(_cmake_cxx_compiler(Path(args.print_cmake_cxx_compiler)))
        return 0
    if args.print_compiler_include_paths:
        for kind, path in _classified_compiler_include_paths(
            args.print_compiler_include_paths,
            Path(args.compile_db) if args.compile_db else None,
        ):
            print(f"{kind}:{path}")
        return 0
    missing = [
        option
        for option, value in {
            "--config": args.config,
            "--out-dir": args.out_dir,
            "--compiler": args.compiler,
            "--compile-db": args.compile_db,
        }.items()
        if not value
    ]
    if missing:
        parser.error("the following arguments are required: " + ", ".join(missing))

    config_path = Path(args.config).resolve()
    out_dir = Path(args.out_dir).resolve()
    compile_db_path = Path(args.compile_db).resolve()
    overlay_dir = Path(args.overlay_dir).resolve() if args.overlay_dir else None
    if not compile_db_path.is_file():
        raise SystemExit(f"compilation database does not exist: {compile_db_path}")

    lines = config_path.read_text(encoding="utf-8").splitlines()
    config_dir = config_path.parent
    source_root_value = _parse_scalar(lines, "source-root")
    source_root = (
        (config_dir / source_root_value).resolve()
        if source_root_value and not Path(source_root_value).is_absolute()
        else Path(source_root_value).resolve()
        if source_root_value
        else config_dir.resolve()
    )
    input_paths = _resolve_config_paths(
        _parse_list(lines, "input"), config_dir, [source_root / "src"]
    )
    exclude_paths = _resolve_config_paths(
        _parse_list(lines, "exclude"), config_dir, []
    )
    exclude_patterns = _parse_list(lines, "exclude-patterns")
    use_system_stdlib = (
        (_parse_scalar(lines, "use-system-stdlib") or "").lower() == "true"
    )
    stdlib_includes = {Path(path).resolve() for path in args.stdlib_include}
    if not use_system_stdlib and not stdlib_includes:
        raise SystemExit(
            "controlled standard library mode requires compiler-owned include paths"
        )

    def selected(path: Path) -> bool:
        if not any(_is_under(path, input_path) for input_path in input_paths):
            return False
        if any(_is_under(path, exclude_path) for exclude_path in exclude_paths):
            return False
        absolute = path.as_posix()
        try:
            relative = path.relative_to(source_root).as_posix()
        except ValueError:
            relative = absolute
        return not any(
            fnmatch.fnmatch(absolute, pattern)
            or fnmatch.fnmatch(relative, pattern)
            for pattern in exclude_patterns
        )

    prelude = source_root / "src" / "docs" / "mrdocs" / "doc_prelude.h"
    if not prelude.is_file():
        prelude = source_root / "src" / "docs" / "mrdocs" / "ast_node_docs.h"

    database = json.loads(compile_db_path.read_text(encoding="utf-8"))
    if not isinstance(database, list):
        raise SystemExit("compile_commands.json is not a list")

    output: list[dict] = []
    seen: set[tuple[str, tuple[str, ...]]] = set()
    for entry in database:
        if not isinstance(entry, dict) or not entry.get("file"):
            continue
        directory = Path(entry.get("directory") or source_root).resolve()
        source = Path(entry["file"])
        source = (
            (directory / source).resolve()
            if not source.is_absolute()
            else source.resolve()
        )
        arguments = _arguments(entry)
        if (
            not arguments
            or source.name.startswith("cmake_pch.")
            or not _is_cpp_translation_unit(source, arguments)
            or not selected(source)
        ):
            continue

        arguments = _strip_cmake_pch_arguments(arguments)
        arguments = _replace_compiler(arguments, args.compiler, directory)
        if not use_system_stdlib:
            arguments = _strip_stdlib_includes(
                arguments, directory, stdlib_includes
            )
            _insert_frontend_argument(arguments, "-nostdinc++")
        if overlay_dir and overlay_dir.is_dir():
            overlay_pair = ["-I", str(overlay_dir)]
            if not any(
                arguments[index : index + 2] == overlay_pair
                for index in range(len(arguments) - 1)
            ):
                arguments[1:1] = overlay_pair
        if prelude.is_file():
            prelude_pair = ["-include", str(prelude)]
            if not any(
                arguments[index : index + 2] == prelude_pair
                for index in range(len(arguments) - 1)
            ):
                arguments[1:1] = prelude_pair
        _insert_frontend_argument(arguments, "-DROSE_DOCGEN")

        key = (str(source), tuple(arguments))
        if key in seen:
            continue
        seen.add(key)
        output.append(
            {"directory": str(directory), "file": str(source), "arguments": arguments}
        )

    if not output:
        raise SystemExit("no selected C++ translation units in compilation database")
    out_dir.mkdir(parents=True, exist_ok=True)
    output_path = out_dir / "compile_commands.json"
    output_path.write_text(json.dumps(output, indent=2), encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
