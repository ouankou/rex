#!/usr/bin/env python3
import argparse
import subprocess
from typing import Iterable, List, Set, Tuple

INVENTORY_PREFIXES = (
    "config/",
    "acmacros/",
)
INVENTORY_FILES = {
    "configure.ac",
    "Makefile.am",
    "Tupfile",
    "Tupfile.lua",
    "Tupfile.ini",
}

KEEP_TOP_PREFIXES = (
    "cmake/",
    "docs/",
    "scripts/",
    "tools/",
    "tutorial/",
    "exampleTranslators/",
    "LicenseInformation/",
    "src/",
    "tests/",
    ".github/",
)
KEEP_TOP_FILES = {
    "CMakeLists.txt",
    "build-rex.sh",
    "rose_config.h.in.cmake",
    "ROSE_VERSION",
    "README.md",
    "COPYRIGHT",
}

FRONTEND_KEEP = (
    "src/frontend/CxxFrontend/Clang/",
    "src/frontend/SageIII/",
    "src/frontend/OpenFortranParser_SAGE_Connection/",
)

DROP_FRONTEND_PREFIX = "src/frontend/"
DROP_SUBDIRS = (
    "src/frontend/BinaryAnalysis/",
    "src/frontend/EDG/",
    "src/frontend/ECJ/",
    "src/frontend/Java/",
    "src/frontend/UPC/",
    "src/frontend/PHP/",
    "src/frontend/JavaScript/",
)

PLATFORM_KEYWORDS = (
    "Windows",
    "Win32",
    "Darwin",
    "Mac",
    "macOS",
    "OSX",
)


def run_git_log(ref: str, since: str, until: str) -> Iterable[str]:
    cmd = [
        "git",
        "log",
        f"--since={since}",
        f"--until={until}",
        "--name-only",
        "--pretty=format:__COMMIT__%H\t%cs\t%s",
        ref,
    ]
    out = subprocess.check_output(cmd, text=True)
    return out.splitlines()


def is_inventory(path: str) -> bool:
    if path in INVENTORY_FILES:
        return True
    return path.startswith(INVENTORY_PREFIXES)


def is_keep(path: str) -> bool:
    if path in KEEP_TOP_FILES:
        return True
    return path.startswith(KEEP_TOP_PREFIXES)


def tag_for_path(path: str) -> Set[str]:
    tags: Set[str] = set()
    if is_inventory(path):
        tags.add("inventory:autotools")
    if path in KEEP_TOP_FILES or path.startswith("cmake/"):
        tags.add("area:cmake")
    if path.startswith("docs/") or path.endswith(".md"):
        tags.add("area:docs")
    if path.startswith("scripts/"):
        tags.add("area:scripts")
    if path.startswith("tools/"):
        tags.add("area:tools")
    if path.startswith("tutorial/"):
        tags.add("area:tutorial")
    if path.startswith("exampleTranslators/"):
        tags.add("area:exampleTranslators")
    if path.startswith("LicenseInformation/"):
        tags.add("area:license")
    if path.startswith("tests/"):
        tags.add("area:tests")
    if path.startswith(".github/"):
        tags.add("area:github")

    if path.startswith("src/"):
        tags.add("area:src")
        if path.startswith("src/util/"):
            tags.add("src:util")
        elif path.startswith("src/midend/"):
            tags.add("src:midend")
        elif path.startswith("src/backend/unparser/"):
            tags.add("src:backend:unparser")
        elif path.startswith("src/ROSETTA/"):
            tags.add("src:rosetta")
        elif path.startswith("src/3rdPartyLibraries/"):
            tags.add("src:thirdparty")
        elif path.startswith("src/Rose/"):
            tags.add("src:rose")
        elif path.startswith("src/frontend/"):
            tags.add("src:frontend")
            if path.startswith(FRONTEND_KEEP):
                if path.startswith("src/frontend/CxxFrontend/Clang/"):
                    tags.add("src:frontend:clang")
                elif path.startswith("src/frontend/SageIII/"):
                    tags.add("src:frontend:sage")
                elif path.startswith("src/frontend/OpenFortranParser_SAGE_Connection/"):
                    tags.add("src:frontend:fortran")
            else:
                tags.add("drop:frontend-other")

    if path.startswith(DROP_SUBDIRS):
        tags.add("drop:frontend-other")

    lower = path.lower()
    if "php" in lower:
        tags.add("drop:php")
    if "javascript" in lower or lower.endswith(".js"):
        tags.add("drop:javascript")

    for key in PLATFORM_KEYWORDS:
        if key in path:
            tags.add("drop:platform")
            break

    return tags


def group_for_path(path: str) -> str:
    if is_inventory(path):
        return "autotools"
    if path in KEEP_TOP_FILES:
        return "topfiles"
    for prefix in KEEP_TOP_PREFIXES:
        if path.startswith(prefix):
            if prefix == "src/":
                break
            return prefix.rstrip("/")
    if path.startswith("src/frontend/CxxFrontend/Clang/"):
        return "src/frontend/CxxFrontend/Clang"
    if path.startswith("src/frontend/OpenFortranParser_SAGE_Connection/"):
        return "src/frontend/OpenFortranParser_SAGE_Connection"
    if path.startswith("src/frontend/SageIII/"):
        return "src/frontend/SageIII"
    if path.startswith("src/frontend/"):
        return "src/frontend/OTHER"
    if path.startswith("src/midend/"):
        return "src/midend"
    if path.startswith("src/backend/unparser/"):
        return "src/backend/unparser"
    if path.startswith("src/util/"):
        return "src/util"
    if path.startswith("src/ROSETTA/"):
        return "src/ROSETTA"
    if path.startswith("src/3rdPartyLibraries/"):
        return "src/3rdPartyLibraries"
    if path.startswith("src/Rose/"):
        return "src/Rose"
    if path.startswith("tests/"):
        return "tests"
    if path.startswith("docs/"):
        return "docs"
    if path.startswith("tools/"):
        return "tools"
    if path.startswith("scripts/"):
        return "scripts"
    if path.startswith("tutorial/"):
        return "tutorial"
    if path.startswith("exampleTranslators/"):
        return "exampleTranslators"
    if path.startswith("LicenseInformation/"):
        return "LicenseInformation"
    if path.startswith(".github/"):
        return ".github"
    return path.split("/")[0]


def emit(commit: Tuple[str, str, str], paths: List[str]) -> str:
    tags: Set[str] = set()
    groups: Set[str] = set()
    for path in paths:
        tags.update(tag_for_path(path))
        groups.add(group_for_path(path))
    hash_, date, subject = commit
    tag_str = ";".join(sorted(tags))
    group_str = ";".join(sorted(groups))
    return f"{hash_}\t{date}\t{subject}\t{tag_str}\t{group_str}\t{len(paths)}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Triage rose-archive commits by path.")
    parser.add_argument("--ref", default="rose-archive/develop")
    parser.add_argument("--since", default="2019-01-01")
    parser.add_argument("--until", default="2023-10-26")
    parser.add_argument("--output", default="-")
    args = parser.parse_args()

    lines = run_git_log(args.ref, args.since, args.until)
    out_lines: List[str] = []
    out_lines.append("commit\tdate\tsubject\ttags\tgroups\tfile_count")

    commit = None
    paths: List[str] = []
    for line in lines:
        if line.startswith("__COMMIT__"):
            if commit is not None:
                out_lines.append(emit(commit, paths))
            header = line[len("__COMMIT__") :]
            parts = header.split("\t", 2)
            if len(parts) < 3:
                parts += [""] * (3 - len(parts))
            commit = (parts[0], parts[1], parts[2])
            paths = []
        elif line.strip():
            paths.append(line.strip())
    if commit is not None:
        out_lines.append(emit(commit, paths))

    output = "\n".join(out_lines) + "\n"
    if args.output == "-":
        print(output, end="")
    else:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
