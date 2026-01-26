#!/usr/bin/env python3
from __future__ import annotations

import argparse
import fnmatch
import json
import os
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


def _is_under(path: Path, base: Path | None) -> bool:
    if base is None:
        return False
    try:
        path.relative_to(base)
        return True
    except ValueError:
        return False


def _collect_local_includes(
    compile_db_path: Path | None, source_root: Path, build_dir: Path | None
) -> list[Path]:
    if not compile_db_path or not compile_db_path.is_file():
        return []
    data = json.loads(compile_db_path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        return []
    includes: list[Path] = []
    for entry in data:
        directory = entry.get("directory")
        base_dir = Path(directory).resolve() if directory else source_root
        args = entry.get("arguments")
        if not isinstance(args, list) or not args:
            cmd = entry.get("command")
            if not isinstance(cmd, str):
                continue
            import shlex

            args = shlex.split(cmd)
        i = 0
        while i < len(args):
            arg = args[i]
            next_val = args[i + 1] if i + 1 < len(args) else None
            path = None
            if arg in ("-I", "-isystem") and next_val:
                path = next_val
                i += 1
            elif arg.startswith("-I") and len(arg) > 2:
                path = arg[2:]
            elif arg.startswith("-isystem") and len(arg) > len("-isystem"):
                path = arg[len("-isystem") :]
            if path:
                inc_path = Path(path)
                if not inc_path.is_absolute():
                    inc_path = (base_dir / inc_path).resolve()
                else:
                    inc_path = inc_path.resolve()
                if _is_under(inc_path, source_root) or _is_under(inc_path, build_dir):
                    includes.append(inc_path)
            i += 1
    return includes


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--compiler", required=True)
    ap.add_argument("--build-dir", default="")
    ap.add_argument("--compile-db", default="")
    ap.add_argument("--overlay-dir", default="")
    args = ap.parse_args()

    config_path = Path(args.config).resolve()
    out_dir = Path(args.out_dir).resolve()
    compiler = args.compiler
    build_dir = Path(args.build_dir).resolve() if args.build_dir else None
    compile_db_path = Path(args.compile_db).resolve() if args.compile_db else None
    overlay_dir = Path(args.overlay_dir).resolve() if args.overlay_dir else None

    lines = config_path.read_text(encoding="utf-8").splitlines()
    config_dir = config_path.parent

    source_root_value = _parse_scalar(lines, "source-root")
    if source_root_value:
        source_root_path = Path(source_root_value)
        if not source_root_path.is_absolute():
            source_root_path = (config_dir / source_root_path).resolve()
        else:
            source_root_path = source_root_path.resolve()
    else:
        source_root_path = config_dir.resolve()

    input_items = _parse_list(lines, "input") or [str(source_root_path / "src")]
    input_paths: list[Path] = []
    for item in input_items:
        path = Path(item)
        if not path.is_absolute():
            path = (config_dir / path).resolve()
        else:
            path = path.resolve()
        input_paths.append(path)

    exclude_items = _parse_list(lines, "exclude")
    exclude_paths: list[Path] = []
    for item in exclude_items:
        path = Path(item)
        if not path.is_absolute():
            path = (config_dir / path).resolve()
        else:
            path = path.resolve()
        exclude_paths.append(path)

    patterns = _parse_list(lines, "file-patterns")
    if not patterns:
        patterns = ["*.h", "*.hh", "*.hpp", "*.hxx", "*.H", "*.inl", "*.ipp", "*.tpp", "*.tcc"]
    exclude_patterns = _parse_list(lines, "exclude-patterns")

    def matches_exclude_patterns(path: Path) -> bool:
        if not exclude_patterns:
            return False
        path_posix = path.as_posix()
        for pat in exclude_patterns:
            if fnmatch.fnmatch(path_posix, pat):
                return True
        try:
            rel = path.resolve().relative_to(source_root_path)
        except ValueError:
            return False
        rel_posix = rel.as_posix()
        for pat in exclude_patterns:
            if fnmatch.fnmatch(rel_posix, pat):
                return True
        return False

    def is_excluded(path: Path) -> bool:
        for ex in exclude_paths:
            try:
                path.relative_to(ex)
                return True
            except ValueError:
                continue
        return matches_exclude_patterns(path)

    headers: list[Path] = []
    for base in input_paths:
        if not base.is_dir():
            continue
        for root_dir, _, files in os.walk(base):
            root_path = Path(root_dir)
            if is_excluded(root_path):
                continue
            for name in files:
                if any(fnmatch.fnmatch(name, pat) for pat in patterns):
                    file_path = root_path / name
                    if is_excluded(file_path):
                        continue
                    headers.append(file_path)

    if not headers:
        raise SystemExit("no headers found for documentation input")

    headers = sorted(set(headers))
    src_root = source_root_path / "src"

    include_dirs: list[Path] = []
    if src_root.is_dir():
        include_dirs.append(src_root)
    if build_dir and build_dir.is_dir():
        include_dirs.append(build_dir)
    include_dirs.extend([p for p in input_paths if p.is_dir()])
    util_dir = source_root_path / "src" / "util"
    if util_dir.is_dir():
        include_dirs.append(util_dir)
    sage_dir = source_root_path / "src" / "frontend" / "SageIII"
    if sage_dir.is_dir():
        include_dirs.append(sage_dir)

    include_dirs.extend(_collect_local_includes(compile_db_path, source_root_path, build_dir))

    seen: set[Path] = set()
    include_args: list[str] = []
    for path in include_dirs:
        path = path.resolve()
        if path in seen:
            continue
        seen.add(path)
        include_args.extend(["-I", str(path)])
    if overlay_dir and overlay_dir.is_dir():
        include_args = ["-I", str(overlay_dir), *include_args]

    out_dir.mkdir(parents=True, exist_ok=True)
    entries: list[dict] = []
    for idx, path in enumerate(headers):
        try:
            rel = path.relative_to(src_root)
        except ValueError:
            rel = path.relative_to(source_root_path)
        cpp_path = out_dir / f"mrdocs_{idx}.cpp"
        doc_prelude = source_root_path / "src" / "docs" / "mrdocs" / "doc_prelude.h"
        fallback_prelude = source_root_path / "src" / "docs" / "mrdocs" / "ast_node_docs.h"
        prelude_include = ""
        if doc_prelude.is_file():
            prelude_include = "#include \"docs/mrdocs/doc_prelude.h\"\n"
        elif fallback_prelude.is_file():
            prelude_include = "#include \"docs/mrdocs/ast_node_docs.h\"\n"
        cpp_path.write_text(
            "// Generated by build-docs-compilation-db to include a public header.\n"
            + "#include \"sage3basic.h\"\n"
            + prelude_include
            + f"#include \"{rel.as_posix()}\"\n",
            encoding="utf-8",
        )
        obj_path = out_dir / f"mrdocs_{idx}.o"
        args_list = [
            compiler,
            "-std=gnu++17",
            "-DROSE_DOCGEN",
            *include_args,
            "-c",
            str(cpp_path),
            "-o",
            str(obj_path),
        ]
        entries.append(
            {
                "directory": str(out_dir),
                "file": str(cpp_path),
                "arguments": args_list,
            }
        )

    compile_db = out_dir / "compile_commands.json"
    compile_db.write_text(json.dumps(entries, indent=2), encoding="utf-8")
    print(compile_db)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
