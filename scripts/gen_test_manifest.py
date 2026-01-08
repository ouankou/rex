#!/usr/bin/env python3
from __future__ import annotations

import argparse
import functools
import glob
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional, Sequence


TEST_FILE_EXTS = {
    ".c",
    ".C",
    ".cc",
    ".cpp",
    ".cxx",
    ".cu",
    ".f",
    ".F",
    ".f90",
    ".F90",
    ".f95",
    ".F95",
    ".f03",
    ".F03",
    ".f08",
    ".F08",
    ".java",
    ".jov",
    ".cpl",
    ".caf",
    ".upc",
    ".x10",
    ".cl",
    ".py",
}

REQUIRED_ROSE_BRANCH = "develop"

DISABLED_HINTS = (
    "FAIL",
    "DISABLED",
    "XFAIL",
    "SKIP",
    "NOT_COMPILABLE",
    "CURRENTLY_FAILING",
    "KNOWN_FAIL",
    "BROKEN",
    "TO_FIX",
)

SCRIPT_TEST_EXTS = {
    ".sh",
    ".pl",
    ".t",
}

_GLOB_CACHE: dict[str, list[str]] = {}


def _cached_glob(pattern: str) -> list[str]:
    cached = _GLOB_CACHE.get(pattern)
    if cached is not None:
        return list(cached)
    matches = sorted(glob.glob(pattern))
    _GLOB_CACHE[pattern] = matches
    return list(matches)

PROGRAM_VAR_NAMES = (
    "bin_PROGRAMS",
    "noinst_PROGRAMS",
    "check_PROGRAMS",
    "TEST_PROGRAMS",
)

SCRIPT_TEST_NAMES = {
    "test_with_answer",
    "test_exit_status",
    "rth_run.pl",
    "rth_stats.pl",
    "timeout.sh",
}

KNOWN_SHELL_COMMANDS = {
    "bash",
    "sh",
    "python",
    "python3",
    "perl",
    "env",
    "diff",
    "cmp",
    "sed",
    "awk",
    "grep",
    "egrep",
    "fgrep",
    "head",
    "tail",
    "cat",
    "printf",
}

TRANSLATOR_EXECUTABLES = {
    "testTranslator",
    "translator",
}

AUTOTOOLS_IGNORE_VAR_HINTS = (
    "TRANSLATOR",
    "EXTRA_DIST",
    "CLEAN",
    "DISTCLEAN",
    "MAINTAINERCLEAN",
    "MOSTLYCLEAN",
    "SUBDIRS",
    "BUILT_SOURCES",
    "SOURCES",
    "HEADERS",
    "OBJECTS",
    "LDADD",
    "LIBS",
    "LDFLAGS",
    "CPPFLAGS",
    "CFLAGS",
    "CXXFLAGS",
    "FCFLAGS",
    "FFLAGS",
    "AM_CPPFLAGS",
    "AM_CFLAGS",
    "AM_CXXFLAGS",
    "AM_LDFLAGS",
)

AUTOTOOLS_RUN_VAR_HINTS = (
    "TESTS",
    "TESTSCRIPT",
    "TEST_SCRIPT",
    "TEST_TARGET",
    "XFAIL",
    "CHECK_PROGRAM",
    "CHECK_SCRIPTS",
    "TEST_PROGRAM",
    "RUN_TEST",
    "RUNTEST",
)

_AUTOTOOLS_TEST_VAR_RE = re.compile(
    r"(?:^|_)(TESTS?|TESTCODES?|TEST_TARGETS|TEST_FILES?|TESTFILE|PASSING_TEST|FAILING_TEST|XFAIL|KNOWN_FAIL|DISABLED|RUN_TEST|RUNTEST)",
    re.IGNORECASE,
)

DROP_PATH_PREFIXES = [
    Path("projects"),
    Path("tests/CompileTests"),
    Path("tests/roseTests"),
    Path("tests/nonsmoke/functional/BinaryAnalysis"),
    Path("tests/nonsmoke/functional/CompileTests/Java_tests"),
    Path("tests/nonsmoke/functional/CompileTests/MicrosoftWindows_C_tests"),
    Path("tests/nonsmoke/functional/CompileTests/MicrosoftWindows_Cxx_tests"),
    Path("tests/nonsmoke/functional/CompileTests/MicrosoftWindows_Java_tests"),
    Path("tests/nonsmoke/functional/CompileTests/MicrosoftWindows_tests"),
    Path("tests/nonsmoke/functional/CompileTests/NewEDGInterface_C_tests"),
    Path("tests/nonsmoke/functional/CompileTests/PythonExample_tests"),
    Path("tests/nonsmoke/functional/CompileTests/Python_tests"),
    Path("tests/nonsmoke/functional/CompileTests/UPC_tests"),
    Path("tests/nonsmoke/functional/CompileTests/x10_tests"),
    Path("tests/CompileTests/x10_tests"),
    Path("tests/nonsmoke/functional/CompileTests/boost_tests"),
    Path("tests/nonsmoke/functional/CompileTests/colorAST_tests"),
    Path("tests/nonsmoke/functional/CompileTests/experimental_csharp_tests"),
    Path("tests/nonsmoke/functional/CompileTests/experimental_ada_tests"),
    Path("tests/nonsmoke/functional/CompileTests/experimental_jovial_tests"),
    Path("tests/nonsmoke/functional/CompileTests/vxworks_tests"),
    Path("tests/nonsmoke/functional/CompilerOptionsTests/testWave"),
    Path("tests/nonsmoke/functional/RunTests/PythonTests"),
    Path("tests/nonsmoke/functional/roseTests/PHPTests"),
    Path("tests/nonsmoke/functional/roseTests/abstractMemoryObjectTests"),
    Path("tests/nonsmoke/functional/roseTests/astFileIOTests"),
    Path("tests/nonsmoke/functional/roseTests/astRewriteTests"),
    Path("tests/nonsmoke/functional/roseTests/astSnippetTests"),
    Path("tests/nonsmoke/functional/roseTests/graph_tests"),
    Path("tests/nonsmoke/functional/roseTests/roseHPCToolkitTests"),
    Path("tests/nonsmoke/functional/roseTests/roseCodeGen"),
    Path("tests/nonsmoke/functional/roseTests/roseTraits"),
    Path("tests/nonsmoke/functional/CompileTests/FailSafe_tests"),
    Path("tests/nonsmoke/functional/CompilerOptionsTests/tokenStream_tests"),
    Path("tests/nonsmoke/functional/roseTests/astMempoolTests"),
    Path("tests/nonsmoke/functional/roseTests/astNodeIdTests"),
    Path("tests/nonsmoke/functional/roseTests/loopProcessingTests"),
    Path("tests/nonsmoke/functional/roseTests/programAnalysisTests/ssa_UnfilteredCfg_Test"),
    Path("tests/nonsmoke/functional/roseTests/programAnalysisTests/staticSingleAssignmentTests"),
    Path("tests/nonsmoke/functional/roseTests/programAnalysisTests/systemDependenceGraphTests"),
    Path("tests/nonsmoke/specimens/binary"),
    Path("tests/nonsmoke/specimens/java"),
    Path("tests/smoke/unit/BinaryAnalysis"),
    Path("tests/smoke/unit/Boost"),
    Path("tests/smoke/functional/BinaryAnalysis"),
    Path("tests/smoke/unit/Sawyer"),
]

DROP_FILE_NAMES = {
    "astThreadedCreation",
    "astThreadedCreation.C",
    "buildJavaPackage",
    "buildJavaPackage.C",
    "bPTP",
    "bPTP.C",
    "binaryPaths",
    "binaryPaths.C",
    "BitFlags.h",
    "bitFlags",
    "bitFlags.C",
    "Diagnostics.h",
    "GraphUtility.h",
    "MatlabNodeBuildersUnitTests",
    "MatlabNodeBuildersUnitTests.C",
    "MatlabNodeBuildersUnitTests.test",
    "progressReports",
    "progressReports.C",
    "graphPerformance",
    "graphPerformance.C",
    "graphIO",
    "graphIO.C",
    "hash",
    "hash.C",
    "rangeMapTests.C",
    "rangeMapTests",
    "testDiagnostics.C",
    "testDiagnostics",
    "testRangeMap.C",
    "testRangeMap",
    "testSha256Builtin.C",
    "testSha256Builtin",
    "testSort.C",
    "testSort",
    "testYaml.C",
    "testYaml",
    "testJSONGeneration",
    "testJSONGeneration.C",
    "strictGraphTest",
    "strictGraphTest.C",
    "strictGraphTest2",
    "strictGraphTest2.C",
    "strictGraphTest3",
    "strictGraphTest3.C",
    "smtlibParser",
    "smtlibParser.C",
    "yicesParser",
    "yicesParser.C",
}

DROP_FILE_EXTS = {
    ".java",
    ".py",
    ".jov",
    ".cpl",
    ".upc",
    ".x10",
    ".adb",
    ".ads",
    ".class",
}

DROP_NAME_PREFIXES = (
    "bp_",
    "bptp_",
    "yp_",
    "smtlib_",
    "smtlibParser",
    "runAlgorithm_x86-64",
)

OUTPUT_FILE_PREFIXES = (
    "rose_",
)


@dataclass
class TestEntry:
    key: str
    name: str
    command: list[str]
    workdir: Optional[str]
    env: dict[str, str]
    labels: list[str]
    status: str
    disable_reason: Optional[str]
    origin: list[dict]
    needs_manual_followup: bool = False
    priority: int = 0
    depends: list[str] = field(default_factory=list)

    def as_manifest(self) -> dict:
        data = {
            "id": self.key,
            "name": self.name,
            "command": self.command,
            "labels": sorted(set(self.labels)),
            "status": self.status,
            "origin": self.origin,
            "env": self.env or {},
        }
        if self.workdir:
            data["workdir"] = self.workdir
        if self.disable_reason:
            data["disable_reason"] = self.disable_reason
        if self.depends:
            data["depends"] = sorted(set(self.depends))
        if self.needs_manual_followup:
            data["labels"].append("needs_manual_followup")
            data["labels"] = sorted(set(data["labels"]))
        return data


def _git_current_branch(repo: Path) -> str:
    try:
        output = subprocess.check_output(
            ["git", "-C", str(repo), "rev-parse", "--abbrev-ref", "HEAD"],
            text=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        raise RuntimeError(f"Failed to read git branch for {repo}") from exc
    return output.strip()


def _require_branch(repo: Path, expected: str, label: str) -> str:
    branch = _git_current_branch(repo)
    if branch != expected:
        raise RuntimeError(
            f"{label} must be on '{expected}' (found '{branch}'). "
            f"Run: git -C {repo} checkout {expected}"
        )
    return branch


def _annotate_origin_branch(entries: list[TestEntry], repo_label: str, branch: str) -> None:
    tag = f"branch={branch}"
    for entry in entries:
        for origin in entry.origin:
            if origin.get("repo") != repo_label:
                continue
            notes = (origin.get("notes") or "").strip()
            if tag in notes.split():
                continue
            origin["notes"] = f"{notes} {tag}".strip() if notes else tag


@dataclass
class CMakeCommand:
    name: str
    args: list[str]
    line: int


@dataclass
class CMakeForEach:
    var: str
    items: list[str]
    body: list[object]
    line: int


@dataclass
class CMakeIfBranch:
    condition: list[str]
    body: list[object]


@dataclass
class CMakeIfBlock:
    branches: list[CMakeIfBranch]
    line: int


@dataclass
class CMakeFunction:
    name: str
    params: list[str]
    body: list[object]
    line: int
    is_macro: bool = False


@dataclass
class MakefileRule:
    targets: list[str]
    deps: list[str]
    commands: list[tuple[str, bool]]
    origin: Path
    commented: bool = False


class ReturnSignal(Exception):
    pass


@dataclass
class EvalState:
    vars: dict[str, list[str]]
    parent: Optional["EvalState"] = None


@dataclass
class CMakeContext:
    repo_root: Path
    repo_label: str
    cmake_path: Path
    compile_test_mode: str
    cache_vars: set[str] = field(default_factory=set)
    tests: dict[str, TestEntry] = field(default_factory=dict)
    pending_props: dict[str, dict] = field(default_factory=dict)
    functions: dict[str, CMakeFunction] = field(default_factory=dict)
    targets: set[str] = field(default_factory=set)
    visited_subdirs: set[Path] = field(default_factory=set)
    cmake_glob_cache: dict[str, list[str]] = field(default_factory=dict)


def _split_comment(value: str) -> tuple[str, str]:
    if "#" not in value:
        return value, ""
    idx = value.find("#")
    return value[:idx], value[idx + 1 :]


def _tokenize(value: str) -> list[str]:
    try:
        return shlex.split(value, comments=False, posix=True)
    except ValueError:
        return value.split()


def _is_test_token(token: str) -> bool:
    token = token.strip()
    if not token:
        return False
    if ":" in token:
        return False
    if token.endswith(".passed"):
        return True
    return Path(token).suffix in TEST_FILE_EXTS


def _normalize_test_token(token: str) -> Optional[str]:
    token = token.strip()
    if not token:
        return None
    if ":" in token:
        return None
    if token.endswith(".passed"):
        base = token[: -len(".passed")]
        if Path(base).suffix in TEST_FILE_EXTS:
            return base
        return base
    return token


def _sanitize_test_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.+-]", "_", name)


def _rel_name_from_path(path: Path, repo_root: Path) -> str:
    try:
        rel_path = path.relative_to(repo_root)
    except ValueError:
        rel_path = path
    parts = rel_path.parts
    if parts and parts[0] == "tests":
        rel_path = Path(*parts[1:]) if len(parts) > 1 else Path(parts[0])
    return _sanitize_test_name(str(rel_path).replace("/", "_"))


def _infer_status(var_name: str, commented: bool) -> tuple[str, Optional[str]]:
    if commented:
        return "disabled", "commented-out in Makefile.am"
    upper = var_name.upper()
    for hint in DISABLED_HINTS:
        if hint in upper:
            return "disabled", f"autotools:{var_name}"
    return "enabled", None


def _looks_like_test_var(var_name: str) -> bool:
    return _AUTOTOOLS_TEST_VAR_RE.search(var_name) is not None


def _is_compile_tests_path(path: Path) -> bool:
    parts = path.parts
    if "CompileTests" not in parts and "CompilerOptionsTests" not in parts:
        return False
    disallowed = {
        "OpenMP_tests",
        "OpenACC_tests",
        "uninitializedField_tests",
        "unparseToString_tests",
        "UnparseHeadersTests",
        "UnparseHeadersUsingTokenStream_tests",
        "staticCFG_tests",
        "virtualCFG_tests",
        "sourcePosition_tests",
        "copyAST_tests",
        "mergeAST_tests",
        "moveDeclarationTool",
    }
    if any(part in disallowed for part in parts):
        return False
    return True


def _find_var_value(lines: list[str], name: str) -> Optional[str]:
    assign_re = re.compile(rf"^\s*{re.escape(name)}\s*[:+?]?=\s*(.*)$")
    for line in lines:
        match = assign_re.match(line)
        if match:
            return match.group(1).strip()
    return None


def _merge_line_continuations(lines: Iterable[str]) -> list[str]:
    merged = []
    buffer = ""
    for line in lines:
        if buffer:
            buffer += line
        else:
            buffer = line
        if buffer.rstrip().endswith("\\"):
            buffer = buffer.rstrip()[:-1] + " "
            continue
        merged.append(buffer)
        buffer = ""
    if buffer:
        merged.append(buffer)
    return merged


def _merge_line_continuations_with_origin(
    lines: Iterable[tuple[str, Path]],
) -> list[tuple[str, Path]]:
    merged: list[tuple[str, Path]] = []
    buffer = ""
    origin: Optional[Path] = None
    for line, line_origin in lines:
        if buffer:
            buffer += line
        else:
            buffer = line
            origin = line_origin
        if buffer.rstrip().endswith("\\"):
            buffer = buffer.rstrip()[:-1] + " "
            continue
        merged.append((buffer, origin or line_origin))
        buffer = ""
        origin = None
    if buffer:
        merged.append((buffer, origin or Path(".")))
    return merged


def _expand_makefile_include_path(include_path: str, repo_root: Path, base_dir: Path) -> list[Path]:
    expanded = include_path.strip().strip("\"'")
    replacements = {
        "$(top_srcdir)": str(repo_root),
        "${top_srcdir}": str(repo_root),
        "$(srcdir)": str(base_dir),
        "${srcdir}": str(base_dir),
        "$(top_builddir)": str(repo_root),
        "${top_builddir}": str(repo_root),
        "@top_srcdir@": str(repo_root),
    }
    for key, value in replacements.items():
        expanded = expanded.replace(key, value)
    candidates = []
    for path in _cached_glob(expanded):
        candidate = Path(path)
        if candidate.is_file():
            candidates.append(candidate)
    if not candidates:
        candidate = Path(expanded)
        if not candidate.is_absolute():
            candidate = base_dir / candidate
        if candidate.is_file():
            candidates.append(candidate)
    return candidates


def _expand_autotools_vars(value: str, repo_root: Path, base_dir: Path) -> str:
    build_root = repo_root / "build"
    replacements = {
        "$(top_srcdir)": str(repo_root),
        "${top_srcdir}": str(repo_root),
        "$(srcdir)": str(base_dir),
        "${srcdir}": str(base_dir),
        "$(top_builddir)": str(build_root),
        "${top_builddir}": str(build_root),
        "$(builddir)": str(build_root),
        "${builddir}": str(build_root),
        "@top_srcdir@": str(repo_root),
        "@top_builddir@": str(build_root),
    }
    for key, replacement in replacements.items():
        value = value.replace(key, replacement)
    return value


def _default_minimal_input(repo_root: Path) -> Optional[str]:
    candidates = [
        repo_root
        / "tests/nonsmoke/functional/input_codes/minimal/minimal.cpp",
        repo_root
        / "tests/nonsmoke/functional/input_codes/minimal/minimal.f90",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return None

def _find_matching_delim(text: str, start: int, open_ch: str, close_ch: str) -> int:
    depth = 1
    idx = start
    while idx < len(text):
        ch = text[idx]
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return idx
        idx += 1
    return -1


def _split_make_function_args(arg_str: str) -> Optional[tuple[str, str]]:
    depth = 0
    for idx, ch in enumerate(arg_str):
        if ch in "({":
            depth += 1
        elif ch in ")}":
            depth -= 1
        elif ch == "," and depth == 0:
            return arg_str[:idx], arg_str[idx + 1 :]
    return None


def _merge_split_ext_tokens(tokens: list[str]) -> list[str]:
    if not tokens:
        return tokens
    merged: list[str] = []
    for token in tokens:
        if merged:
            is_suffix = token in TEST_FILE_EXTS or (
                token.startswith(".")
                and token not in {".", ".."}
                and not token.startswith(("./", "../"))
                and len(token) > 1
            )
            if is_suffix:
                prev = merged[-1]
                if prev and Path(prev).suffix == "" and not prev.endswith(("/", "\\")):
                    merged[-1] = f"{prev}{token}"
                    continue
        merged.append(token)
    return merged


def _is_make_conditional_text(text: str) -> bool:
    lowered = text.strip().lower()
    if not lowered:
        return False
    if lowered in {"else", "endif"}:
        return True
    return lowered.startswith(("if ", "ifdef ", "ifndef ", "ifeq ", "ifneq "))


def _eval_make_inner(
    inner: str,
    var_map: dict[str, str],
    repo_root: Path,
    base_dir: Path,
    depth: int,
    cond_vars: Optional[set[str]] = None,
) -> str:
    stripped = inner.strip()
    if not stripped:
        return ""
    parts = stripped.split(None, 1)
    func = parts[0]
    rest = parts[1] if len(parts) > 1 else ""
    if any(sym in rest for sym in ("$<", "$@", "$*", "$(@:")):
        return f"$({inner})"
    if func in {"addprefix", "addsuffix"}:
        args = _split_make_function_args(rest)
        if not args:
            return f"$({inner})"
        prefix_expr, list_expr = args
        prefix_val = _expand_make_vars_value(
            prefix_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        list_val = _expand_make_vars_value(
            list_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        tokens = _merge_split_ext_tokens(_tokenize(list_val))
        tokens = [t for t in tokens if not _is_make_control_token(t, cond_vars)]
        if func == "addprefix":
            tokens = [f"{prefix_val}{token}" for token in tokens]
        else:
            tokens = [f"{token}{prefix_val}" for token in tokens]
        return " ".join(tokens)
    if func == "strip":
        value = _expand_make_vars_value(
            rest, var_map, repo_root, base_dir, depth + 1, cond_vars=cond_vars
        )
        tokens = _tokenize(value)
        tokens = [t for t in tokens if not _is_make_control_token(t, cond_vars)]
        return " ".join(tokens)
    if func == "notdir":
        value = _expand_make_vars_value(
            rest, var_map, repo_root, base_dir, depth + 1, cond_vars=cond_vars
        )
        tokens = _merge_split_ext_tokens(_tokenize(value))
        tokens = [t for t in tokens if not _is_make_control_token(t, cond_vars)]
        return " ".join(Path(token).name for token in tokens)
    if func == "basename":
        value = _expand_make_vars_value(
            rest, var_map, repo_root, base_dir, depth + 1, cond_vars=cond_vars
        )
        tokens = _merge_split_ext_tokens(_tokenize(value))
        tokens = [t for t in tokens if not _is_make_control_token(t, cond_vars)]
        return " ".join(Path(token).stem for token in tokens)
    if func == "abspath":
        value = _expand_make_vars_value(
            rest, var_map, repo_root, base_dir, depth + 1, cond_vars=cond_vars
        )
        tokens = _merge_split_ext_tokens(_tokenize(value))
        tokens = [t for t in tokens if not _is_make_control_token(t, cond_vars)]
        abs_tokens = []
        for token in tokens:
            path = Path(token)
            if not path.is_absolute():
                path = (base_dir / path).resolve()
            abs_tokens.append(str(path))
        return " ".join(abs_tokens)
    if func == "filter-out":
        args = _split_make_function_args(rest)
        if not args:
            return f"$({inner})"
        pattern_expr, list_expr = args
        pattern_val = _expand_make_vars_value(
            pattern_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        list_val = _expand_make_vars_value(
            list_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        patterns = _tokenize(pattern_val)
        items = _merge_split_ext_tokens(_tokenize(list_val))
        items = [t for t in items if not _is_make_control_token(t, cond_vars)]

        def _matches(item: str) -> bool:
            for pat in patterns:
                if "%" in pat:
                    regex = re.escape(pat).replace("\\%", ".*")
                    if re.fullmatch(regex, item):
                        return True
                elif item == pat:
                    return True
            return False

        filtered = [item for item in items if not _matches(item)]
        return " ".join(filtered)
    if func == "shell":
        cmd_tokens = _tokenize(rest)
        if not cmd_tokens:
            return ""
        if cmd_tokens[0] == "seq":
            seq_args = cmd_tokens[1:]
            try:
                nums = [int(arg) for arg in seq_args]
            except ValueError:
                return ""
            if len(nums) == 2:
                start, end = nums
                step = 1 if start <= end else -1
            elif len(nums) == 3:
                start, step, end = nums
                if step == 0:
                    return ""
            else:
                return ""
            values: list[str] = []
            current = start
            if step > 0:
                while current <= end:
                    values.append(str(current))
                    current += step
            else:
                while current >= end:
                    values.append(str(current))
                    current += step
            return " ".join(values)
        return ""
    if func == "wildcard":
        value = _expand_make_vars_value(
            rest, var_map, repo_root, base_dir, depth + 1, cond_vars=cond_vars
        )
        patterns = _tokenize(value)
        matches: list[str] = []
        for pattern in patterns:
            if not pattern:
                continue
            candidate = Path(pattern)
            if not candidate.is_absolute():
                candidate = (base_dir / candidate).resolve()
            matches.extend(_cached_glob(str(candidate)))
        return " ".join(matches)
    if func == "patsubst":
        first = _split_make_function_args(rest)
        if not first:
            return f"$({inner})"
        pattern_expr, rest_expr = first
        second = _split_make_function_args(rest_expr)
        if not second:
            return f"$({inner})"
        replacement_expr, list_expr = second
        pattern_val = _expand_make_vars_value(
            pattern_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        replacement_val = _expand_make_vars_value(
            replacement_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        list_val = _expand_make_vars_value(
            list_expr.strip(),
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
        patterns = _tokenize(pattern_val)
        items = _tokenize(list_val)
        if len(patterns) != 1:
            return " ".join(items)
        pattern = patterns[0]
        output: list[str] = []
        for item in items:
            if "%" in pattern:
                regex = re.escape(pattern).replace("\\%", "(.+)")
                match = re.fullmatch(regex, item)
                if match:
                    output.append(replacement_val.replace("%", match.group(1)))
                else:
                    output.append(item)
            elif item == pattern:
                output.append(replacement_val)
            else:
                output.append(item)
        return " ".join(output)
    if ":" in stripped and "=" in stripped:
        var_part, subst = stripped.split(":", 1)
        if var_part in var_map:
            pattern, replacement = subst.split("=", 1)
            base_val = _expand_make_vars_value(
                var_map[var_part],
                var_map,
                repo_root,
                base_dir,
                depth + 1,
                cond_vars=cond_vars,
            )
            pattern_val = _expand_make_vars_value(
                pattern.strip(),
                var_map,
                repo_root,
                base_dir,
                depth + 1,
                cond_vars=cond_vars,
            )
            replacement_val = _expand_make_vars_value(
                replacement.strip(),
                var_map,
                repo_root,
                base_dir,
                depth + 1,
                cond_vars=cond_vars,
            )
            tokens = _tokenize(base_val)
            mapped: list[str] = []
            for token in tokens:
                replaced = _apply_make_subst(token, pattern_val, replacement_val)
                mapped.append(replaced if replaced is not None else token)
            return " ".join(mapped)
    if stripped in var_map:
        return _expand_make_vars_value(
            var_map[stripped],
            var_map,
            repo_root,
            base_dir,
            depth + 1,
            cond_vars=cond_vars,
        )
    return f"$({inner})"


def _expand_make_vars_value(
    value: str,
    var_map: dict[str, str],
    repo_root: Path,
    base_dir: Path,
    depth: int = 0,
    *,
    cond_vars: Optional[set[str]] = None,
) -> str:
    if depth > 32:
        return value
    expanded = _expand_autotools_vars(value, repo_root, base_dir)
    result = []
    idx = 0
    while idx < len(expanded):
        if expanded.startswith("$(", idx) or expanded.startswith("${", idx):
            open_ch = expanded[idx + 1]
            close_ch = ")" if open_ch == "(" else "}"
            end = _find_matching_delim(expanded, idx + 2, open_ch, close_ch)
            if end == -1:
                result.append(expanded[idx:])
                break
            inner = expanded[idx + 2 : end]
            result.append(
                _eval_make_inner(
                    inner,
                    var_map,
                    repo_root,
                    base_dir,
                    depth + 1,
                    cond_vars=cond_vars,
                )
            )
            idx = end + 1
            continue
        result.append(expanded[idx])
        idx += 1
    return "".join(result)


_MAKE_CONTROL_TOKENS = {
    "if",
    "ifdef",
    "ifndef",
    "ifeq",
    "ifneq",
    "else",
    "endif",
    "then",
    "fi",
    "do",
    "done",
}


def _is_make_control_token(token: str, cond_vars: Optional[set[str]]) -> bool:
    lowered = token.lower()
    if lowered in _MAKE_CONTROL_TOKENS:
        return True
    if cond_vars and token in cond_vars:
        return True
    return False


def _expand_make_vars_tokens(
    value: str,
    var_map: dict[str, str],
    repo_root: Path,
    base_dir: Path,
    *,
    allow_pattern: bool = False,
    cond_vars: Optional[set[str]] = None,
) -> list[str]:
    expanded = _expand_make_vars_value(
        value, var_map, repo_root, base_dir, cond_vars=cond_vars
    )
    tokens = _merge_split_ext_tokens(_tokenize(expanded))
    filtered: list[str] = []
    for token in tokens:
        if not token:
            continue
        if any(ch in token for ch in ("$", "@")):
            continue
        if not allow_pattern and "%" in token:
            continue
        if _is_make_control_token(token, cond_vars):
            continue
        filtered.append(token)
    return filtered


def _collect_makefile_var_map(
    lines_with_origin: list[tuple[str, Path]], repo_root: Path
) -> tuple[dict[str, str], set[str]]:
    var_map: dict[str, str] = {}
    cond_vars: set[str] = set()
    assign_re = re.compile(r"^(\s*#\s*)?\s*([A-Za-z0-9_]+)\s*([:+?]?=)\s*(.*)$")
    cond_var_re = re.compile(r"\b[A-Z][A-Z0-9_]*\b")

    for raw_line, _ in lines_with_origin:
        stripped = _split_comment(raw_line.strip())[0].strip()
        if not stripped:
            continue
        lowered = stripped.lower()
        if not lowered.startswith(("if ", "ifdef ", "ifndef ", "ifeq ", "ifneq ")):
            continue
        for token in cond_var_re.findall(stripped):
            cond_vars.add(token)

    def _needs_continuation(value: str) -> bool:
        if not value:
            return True
        if value.endswith(("$( ", "$(", "${")):
            return True
        if value.count("$(") > value.count(")"):
            return True
        if value.count("${") > value.count("}"):
            return True
        return False

    idx = 0
    while idx < len(lines_with_origin):
        line, origin = lines_with_origin[idx]
        match = assign_re.match(line)
        if not match:
            idx += 1
            continue
        if match.group(1):
            idx += 1
            continue
        var = match.group(2)
        op = match.group(3)
        value = _split_comment(match.group(4))[0].strip()
        if _needs_continuation(value):
            continuation: list[str] = []
            lookahead = idx + 1
            while lookahead < len(lines_with_origin):
                next_line = lines_with_origin[lookahead][0]
                stripped = next_line.strip()
                if not stripped:
                    lookahead += 1
                    continue
                if assign_re.match(next_line):
                    break
                if next_line.startswith("\t"):
                    break
                if not next_line[:1].isspace():
                    break
                if stripped.startswith("#"):
                    lookahead += 1
                    continue
                if _is_make_conditional_text(_split_comment(stripped)[0]):
                    lookahead += 1
                    continue
                continuation.append(_split_comment(stripped)[0].strip())
                lookahead += 1
            if continuation:
                if value:
                    value = f"{value} {' '.join(continuation)}".strip()
                else:
                    value = " ".join(continuation).strip()
                idx = lookahead
            else:
                idx += 1
        else:
            idx += 1

        if not value and op != "+=":
            var_map.setdefault(var, "")
            continue
        value = _expand_autotools_vars(value, repo_root, origin.parent)
        if op == "+=":
            if var in var_map and var_map[var]:
                var_map[var] = f"{var_map[var]} {value}".strip()
            else:
                var_map[var] = value
        elif op == "?=":
            if var not in var_map:
                var_map[var] = value
        else:
            var_map[var] = value
    return var_map, cond_vars


def _prefer_long_make_check_list(var_map: dict[str, str]) -> None:
    def _token_count(value: str) -> int:
        return len(_tokenize(value)) if value else 0

    def _prefer(long_key: str, short_key: str) -> None:
        long_val = var_map.get(long_key)
        if not long_val:
            return
        long_count = _token_count(long_val)
        if long_count == 0:
            return

        testcodes_required = var_map.get("TESTCODES_REQUIRED_TO_PASS", "")
        testcodes_required_count = _token_count(testcodes_required)
        if short_key and short_key in testcodes_required:
            var_map["TESTCODES_REQUIRED_TO_PASS"] = long_val
        elif testcodes_required_count and long_count > testcodes_required_count:
            var_map["TESTCODES_REQUIRED_TO_PASS"] = long_val

        testcodes = var_map.get("TESTCODES", "")
        testcodes_count = _token_count(testcodes)
        if short_key and short_key in testcodes:
            var_map["TESTCODES"] = var_map.get("TESTCODES_REQUIRED_TO_PASS", long_val)
        elif testcodes_count and long_count > testcodes_count:
            var_map["TESTCODES"] = var_map.get("TESTCODES_REQUIRED_TO_PASS", long_val)
        elif not testcodes:
            var_map["TESTCODES"] = var_map.get("TESTCODES_REQUIRED_TO_PASS", long_val)

    _prefer("EXAMPLE_TESTCODES_REQUIRED_TO_PASS", "EXAMPLE_TESTCODES_REQUIRED_TO_PASS_SHORT")
    _prefer("EXAMPLE_TESTCODES", "EXAMPLE_TESTCODES_SHORT")


def _parse_makefile_rules(
    lines_with_origin: list[tuple[str, Path]],
    repo_root: Path,
    var_map: dict[str, str],
    cond_vars: Optional[set[str]] = None,
) -> list[MakefileRule]:
    rules: list[MakefileRule] = []
    assign_re = re.compile(r"^(\s*#\s*)?\s*([A-Za-z0-9_]+)\s*([:+?]?=)\s*(.*)$")
    current_rules: list[MakefileRule] = []

    def _is_make_conditional_line(text: str) -> bool:
        lowered = text.strip().lower()
        if not lowered:
            return False
        if lowered in {"else", "endif"}:
            return True
        return lowered.startswith(("if ", "ifdef ", "ifndef ", "ifeq ", "ifneq "))

    for raw_line, origin in lines_with_origin:
        line = raw_line.rstrip("\n")
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#") and raw_line.lstrip().startswith("#\t"):
            continue
        is_recipe = line.startswith("\t") or (line[:1].isspace() and not line.lstrip().startswith("#"))
        if current_rules and is_recipe:
            cmd = line.lstrip()
            cmd_commented = cmd.startswith("#") or any(rule.commented for rule in current_rules)
            cmd = cmd.lstrip("#").strip()
            if cmd:
                for rule in current_rules:
                    rule.commands.append((cmd, cmd_commented))
            continue
        if current_rules and _is_make_conditional_line(stripped):
            continue

        current_rules = []
        if assign_re.match(stripped):
            continue
        commented = False
        target_line = stripped
        if target_line.startswith("#"):
            commented = True
            target_line = target_line.lstrip("#").strip()
        if ":" not in target_line:
            continue
        parts = target_line.split(":")
        if len(parts) >= 3:
            target_part = parts[0].strip()
            pattern_part = parts[1].strip()
            dep_part = ":".join(parts[2:]).strip()
            if target_part and "%" in pattern_part:
                targets = _expand_make_vars_tokens(
                    target_part,
                    var_map,
                    repo_root,
                    origin.parent,
                    allow_pattern=True,
                    cond_vars=cond_vars,
                )
                deps = _expand_make_vars_tokens(
                    dep_part,
                    var_map,
                    repo_root,
                    origin.parent,
                    allow_pattern=True,
                    cond_vars=cond_vars,
                )
                targets = [
                    token
                    for token in targets
                    if _is_concrete_make_token(token) or "%" in token
                ]
                deps = [
                    token for token in deps if _is_concrete_make_token(token) or "%" in token
                ]
                if targets:
                    for target in targets:
                        stem = _match_make_pattern(pattern_part, target)
                        if stem is None:
                            continue
                        resolved_deps = [dep.replace("%", stem) for dep in deps]
                        rule = MakefileRule(
                            targets=[target],
                            deps=resolved_deps,
                            commands=[],
                            origin=origin,
                            commented=commented,
                        )
                        rules.append(rule)
                        current_rules.append(rule)
                    if current_rules:
                        continue
        target_part, dep_part = target_line.split(":", 1)
        target_part = target_part.strip()
        if not target_part:
            continue
        if assign_re.match(target_part):
            continue
        targets = _expand_make_vars_tokens(
            target_part,
            var_map,
            repo_root,
            origin.parent,
            allow_pattern=True,
            cond_vars=cond_vars,
        )
        deps = _expand_make_vars_tokens(
            dep_part.strip(),
            var_map,
            repo_root,
            origin.parent,
            allow_pattern=True,
            cond_vars=cond_vars,
        )
        targets = [
            token for token in targets if _is_concrete_make_token(token) or "%" in token
        ]
        deps = [
            token for token in deps if _is_concrete_make_token(token) or "%" in token
        ]
        if not targets:
            continue
        rule = MakefileRule(
            targets=targets,
            deps=deps,
            commands=[],
            origin=origin,
            commented=commented,
        )
        rules.append(rule)
        current_rules = [rule]

    return rules


def _collect_program_targets(
    var_map: dict[str, str],
    repo_root: Path,
    base_dir: Path,
    cond_vars: Optional[set[str]] = None,
) -> set[str]:
    programs: set[str] = set()
    for name in PROGRAM_VAR_NAMES:
        value = var_map.get(name)
        if not value:
            continue
        for token in _expand_make_vars_tokens(
            value, var_map, repo_root, base_dir, cond_vars=cond_vars
        ):
            if token:
                programs.add(token)
    return programs


def _apply_make_subst(target: str, pattern: str, replacement: str) -> Optional[str]:
    if pattern == "":
        return f"{target}{replacement}"
    if "%" in pattern:
        regex = re.escape(pattern).replace("%", "(.+)")
        match = re.match(rf"^{regex}$", target)
        if not match:
            return None
        return replacement.replace("%", match.group(1))
    if target.endswith(pattern):
        return target[: -len(pattern)] + replacement
    return None


def _match_make_pattern(pattern: str, target: str) -> Optional[str]:
    if "%" not in pattern:
        return None
    regex = re.escape(pattern).replace("%", "(.+)")
    match = re.match(rf"^{regex}$", target)
    if not match:
        return None
    if match.groups():
        return match.group(1)
    return None


def _match_pattern_rules(
    target: str, pattern_rules: list[MakefileRule]
) -> list[tuple[MakefileRule, str, list[str]]]:
    matches: list[tuple[MakefileRule, str, list[str]]] = []
    for rule in pattern_rules:
        for pattern in rule.targets:
            stem = _match_make_pattern(pattern, target)
            if stem is None:
                continue
            resolved_deps = [dep.replace("%", stem) for dep in rule.deps]
            matches.append((rule, stem, resolved_deps))
            break
    return matches


def _replace_make_auto_vars(
    command: str,
    target: Optional[str],
    dep: Optional[str],
    stem: Optional[str] = None,
) -> str:
    if not target:
        return command
    result = command.replace("$@", target).replace("${@}", target)
    if dep:
        result = result.replace("$<", dep).replace("${<}", dep)
    stem_value = stem or target
    if stem_value and "." in stem_value:
        stem_value = stem_value.rsplit(".", 1)[0]
    if stem_value:
        result = result.replace("$*", stem_value).replace("${*}", stem_value)

    def repl(match: re.Match[str]) -> str:
        pattern = match.group(1).strip()
        replacement = match.group(2).strip()
        replaced = _apply_make_subst(target, pattern, replacement)
        return replaced if replaced is not None else match.group(0)

    return re.sub(r"\$\(@:([^=]+)=([^)]*)\)", repl, result)


def _is_concrete_make_token(token: str) -> bool:
    if not token:
        return False
    if token.startswith("-"):
        return False
    return not any(ch in token for ch in ("$", "@", "%"))


def _is_shell_control(tokens: list[str]) -> bool:
    if not tokens:
        return True
    return tokens[0] in {
        "if",
        "then",
        "fi",
        "for",
        "while",
        "do",
        "done",
        "case",
        "esac",
        "in",
        "select",
        "{",
        "}",
    }


def _is_make_check_target(target: str) -> bool:
    return bool(re.search(r"(?:^|[-_])check(?:$|[-_])", target.lower()))


def _is_default_check_target(target: str) -> bool:
    return bool(re.fullmatch(r"(make-)?check", target)) or bool(
        re.fullmatch(r"(make-)?check-local", target)
    )


def _root_target_status(target: str) -> tuple[str, Optional[str]]:
    if _is_default_check_target(target):
        return "enabled", None
    return "disabled", f"autotools:non-default make target {target}"


def _make_target_name(target: str, base_dir: Path, repo_root: Path) -> str:
    base_name = _rel_name_from_path(base_dir, repo_root)
    target_name = _sanitize_test_name(target)
    if base_name:
        return f"{base_name}_{target_name}"
    return target_name


def _should_record_make_target(target: str, base_dir: Path, repo_root: Path) -> bool:
    if not _is_make_check_target(target):
        return False
    if not _is_concrete_make_token(target):
        return False
    if _resolve_make_target_to_source(target, base_dir):
        return False
    if Path(target).suffix in SCRIPT_TEST_EXTS:
        return False
    candidate = Path(target)
    if not candidate.is_absolute():
        candidate = (base_dir / candidate).resolve()
    if candidate.exists() and candidate.is_file():
        return False
    if _drop_reason(candidate, repo_root):
        return False
    return True


def _looks_like_executable(token: str, program_targets: set[str]) -> bool:
    if not token:
        return False
    if any(ch in token for ch in ("$", "@", ",")):
        return False
    base = Path(token).name
    if token.startswith(("./", "../", "/")):
        return True
    if base in program_targets:
        return True
    if base in SCRIPT_TEST_NAMES or base in KNOWN_SHELL_COMMANDS:
        return True
    if Path(base).suffix in SCRIPT_TEST_EXTS:
        return True
    return False


def _strip_passed_suffix(target: str) -> str:
    if target.endswith(".passed"):
        return target[: -len(".passed")]
    return target


def _read_config_cmd(config_path: Path) -> Optional[str]:
    for raw_line in config_path.read_text(errors="ignore").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.match(r"^cmd\s*=\s*(.+)$", line)
        if match:
            return match.group(1).strip()
    return None


def _expand_config_vars(value: str, var_map: dict[str, str]) -> str:
    def repl(match: re.Match[str]) -> str:
        name = match.group(1)
        return var_map.get(name, match.group(0))

    value = re.sub(r"\$\{([^}]+)\}", repl, value)
    value = re.sub(r"\$([A-Za-z_][A-Za-z0-9_]*)", repl, value)
    return value


def _extract_rth_config_command(
    tokens: list[str],
    base_dir: Path,
    cmd_target: Optional[str],
    rth_kv: dict[str, str],
) -> tuple[list[str], Optional[str]]:
    config_path: Optional[Path] = None
    for token in tokens:
        if not token:
            continue
        if not (token.endswith("config") or token.endswith(".conf")):
            continue
        candidate = Path(token)
        if not candidate.is_absolute():
            candidate = (base_dir / candidate).resolve()
        if candidate.is_file():
            config_path = candidate
            break
    if not config_path and cmd_target:
        target_dir = _strip_passed_suffix(Path(str(cmd_target)).name)
        candidate = (base_dir / target_dir / "config").resolve()
        if candidate.is_file():
            config_path = candidate
    if not config_path:
        return [], None
    cmd_line = _read_config_cmd(config_path)
    if not cmd_line:
        return [], None
    var_map = {key: _sanitize_rth_value(value) for key, value in rth_kv.items() if value}
    var_map.setdefault("TARGET", config_path.parent.name)
    var_map.setdefault("srcdir", str(base_dir))
    var_map.setdefault("blddir", str(base_dir))
    var_map.setdefault("VALGRIND", "")
    var_map.setdefault("OUTPUT", "")
    cmd_line = _expand_config_vars(cmd_line, var_map)
    cmd_line = _sanitize_rth_value(cmd_line)
    workdir, cmd_line = _split_cd_prefix(cmd_line, base_dir)
    cmd_tokens = _tokenize(cmd_line)
    return cmd_tokens, workdir


def _is_translator_command(tokens: list[str]) -> bool:
    if not tokens:
        return False
    exe = Path(tokens[0]).name
    return exe in TRANSLATOR_EXECUTABLES


def _parse_rth_run_kv(tokens: list[str]) -> dict[str, str]:
    kv: dict[str, str] = {}
    for token in tokens[1:]:
        if "=" not in token or token.startswith("-"):
            continue
        key, value = token.split("=", 1)
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", key):
            kv[key] = value
    return kv


def _extract_rth_input_paths(
    kv: dict[str, str], base_dir: Path, repo_root: Path
) -> list[Path]:
    paths: list[Path] = []
    for key in ("INPUT", "INPUTS", "INPUT_FILE", "INPUT_FILES"):
        value = kv.get(key)
        if not value:
            continue
        for token in _tokenize(value):
            candidate = _resolve_test_file_token(token, base_dir)
            if candidate and not _drop_reason(candidate, repo_root):
                paths.append(candidate)
    return paths


def _extract_rth_cmd_tokens(kv: dict[str, str]) -> list[str]:
    cmd = kv.get("CMD", "")
    if cmd:
        cmd = _sanitize_rth_value(cmd)
        return _tokenize(cmd) if cmd else []
    exe = kv.get("EXE", "")
    if exe:
        exe = _sanitize_rth_value(exe)
        args = _tokenize(_sanitize_rth_value(kv.get("ARGS", "")))
        if exe:
            return [exe] + args
    return []


def _extract_test_paths(tokens: list[str], base_dir: Path) -> list[Path]:
    test_paths: list[Path] = []
    idx = 0
    while idx < len(tokens):
        if tokens[idx] == "-c":
            idx += 1
            while idx < len(tokens):
                tok = tokens[idx]
                if tok in {"&&", "||", ";"}:
                    break
                candidate = _resolve_test_file_token(tok, base_dir)
                if candidate and candidate not in test_paths:
                    test_paths.append(candidate)
                idx += 1
            continue
        idx += 1
    if not test_paths:
        for tok in tokens:
            candidate = _resolve_test_file_token(tok, base_dir)
            if candidate and candidate not in test_paths:
                test_paths.append(candidate)
    return test_paths


def _normalize_recipe_command(
    command: str, var_map: dict[str, str], repo_root: Path, base_dir: Path
) -> str:
    normalized = command.strip()
    while normalized and normalized[0] in {"@", "-", "+"}:
        normalized = normalized[1:].lstrip()
    normalized = _expand_make_vars_value(normalized, var_map, repo_root, base_dir)
    return normalized.strip()


def _split_cd_prefix(
    command: str, base_dir: Path
) -> tuple[Optional[str], str]:
    match = re.match(r"^cd\s+([^&;]+)\s*(?:&&|;)\s*(.+)$", command)
    if not match:
        return None, command
    raw_dir = match.group(1).strip()
    new_cmd = match.group(2).strip()
    path = Path(raw_dir)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    return str(path), new_cmd


def _split_env_tokens(tokens: list[str]) -> tuple[dict[str, str], list[str]]:
    env: dict[str, str] = {}
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if "=" in token and not token.startswith("-"):
            key, value = token.split("=", 1)
            if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", key):
                if value.startswith("`") and not value.endswith("`"):
                    next_idx = idx + 1
                    while next_idx < len(tokens):
                        value += " " + tokens[next_idx]
                        if tokens[next_idx].endswith("`"):
                            next_idx += 1
                            break
                        next_idx += 1
                    env[key] = value
                    idx = next_idx
                    continue
                env[key] = value
                idx += 1
                continue
        break
    return env, tokens[idx:]


def _is_make_invocation(tokens: list[str]) -> bool:
    if not tokens:
        return False
    head = tokens[0]
    return head == "make" or head.endswith("/make")


def _resolve_make_target_to_source(
    target: str, base_dir: Path
) -> Optional[Path]:
    path = Path(target)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    if path.name.startswith(OUTPUT_FILE_PREFIXES):
        return None
    if path.suffix in TEST_FILE_EXTS:
        return path if path.exists() else None
    if path.suffix == ".passed":
        stem = path.with_suffix("")
        for ext in TEST_FILE_EXTS:
            candidate = stem.with_suffix(ext)
            if candidate.exists():
                return candidate
        return None
    if path.suffix == ".o":
        stem = path.with_suffix("")
        for ext in TEST_FILE_EXTS:
            candidate = stem.with_suffix(ext)
            if candidate.exists():
                return candidate
        return None
    return None


def _resolve_script_token(token: str, base_dir: Path) -> Optional[Path]:
    path = Path(token)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    else:
        path = path.resolve()
    if not path.exists() or not path.is_file():
        return None
    if path.suffix in SCRIPT_TEST_EXTS:
        return path
    if path.name in SCRIPT_TEST_NAMES:
        return path
    if path.parent.name == "scripts":
        return path
    return None


def _resolve_test_file_token(token: str, base_dir: Path) -> Optional[Path]:
    if (
        token.startswith("-")
        or "$" in token
        or "@" in token
        or "=" in token
        or "," in token
        or ";" in token
    ):
        return None
    path = Path(token)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    else:
        path = path.resolve()
    if path.name.startswith(OUTPUT_FILE_PREFIXES):
        return None
    if path.suffix in TEST_FILE_EXTS and path.exists():
        return path
    if path.suffix == "" and path.exists() and path.is_file():
        return path
    return None


def _command_has_dropped_token(
    tokens: list[str], base_dir: Path, repo_root: Path
) -> bool:
    for token in tokens:
        if not token or token.startswith("-") or "$" in token or "@" in token:
            continue
        if any(token.endswith(ext) for ext in DROP_FILE_EXTS):
            return True
        path = Path(token)
        if not path.is_absolute():
            path = (base_dir / path).resolve()
        if _drop_reason(path, repo_root):
            return True
    return False


def _autotools_entry_from_command(
    *,
    name: str,
    command: list[str],
    workdir: Optional[str],
    env: dict[str, str],
    repo_label: str,
    status: str,
    disable_reason: Optional[str],
    origin_path: Path,
    repo_root: Path,
    origin_notes: str,
) -> Optional[TestEntry]:
    if _drop_name_reason(name):
        return None
    labels = [repo_label, "autotools"]
    if status == "disabled" and "disabled" not in labels:
        labels.append("disabled")
    try:
        origin_rel = origin_path.relative_to(repo_root)
    except ValueError:
        origin_rel = origin_path
    return TestEntry(
        key=f"name:{name}",
        name=name,
        command=command,
        workdir=workdir,
        env=env,
        labels=labels,
        status=status,
        disable_reason=disable_reason,
        origin=[
            {
                "repo": repo_label,
                "buildsys": "autotools",
                "path": str(origin_rel),
                "notes": origin_notes,
            }
        ],
        needs_manual_followup=False,
        priority=_priority(repo_label, "autotools"),
    )


def _load_makefile_lines(
    makefile: Path, repo_root: Path, visited: Optional[set[Path]] = None
) -> list[tuple[str, Path]]:
    if visited is None:
        visited = set()
    if makefile in visited:
        return []
    visited.add(makefile)

    lines: list[tuple[str, Path]] = []
    for raw_line in makefile.read_text(errors="ignore").splitlines():
        stripped = raw_line.strip()
        if stripped.startswith("#"):
            lines.append((raw_line, makefile))
            continue
        if stripped.startswith("include "):
            include_path = stripped[len("include ") :].strip()
            for include_file in _expand_makefile_include_path(
                include_path, repo_root, makefile.parent
            ):
                lines.extend(_load_makefile_lines(include_file, repo_root, visited))
            continue
        lines.append((raw_line, makefile))
    return _merge_line_continuations_with_origin(lines)


def _load_cmake_cache(build_dir: Path) -> dict[str, list[str]]:
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.exists():
        return {}
    values: dict[str, list[str]] = {}
    for line in cache_path.read_text(errors="ignore").splitlines():
        line = line.strip()
        if not line or line.startswith(("//", "#")):
            continue
        if ":" not in line or "=" not in line:
            continue
        name, rest = line.split(":", 1)
        _, value = rest.split("=", 1)
        values[name] = _normalize_list_items([value])
    return values


def _extract_autotools_tests(repo: Path, repo_label: str) -> list[TestEntry]:
    entries: list[TestEntry] = []
    tests_dir = repo / "tests"
    makefiles: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(tests_dir):
        dir_path = Path(dirpath)
        if _drop_reason(dir_path, repo):
            dirnames[:] = []
            continue
        if "Makefile.am" in filenames:
            makefile = dir_path / "Makefile.am"
            if not _drop_reason(makefile, repo):
                makefiles.append(makefile)
    makefiles.sort()
    makefile_by_dir = {
        makefile.parent.resolve(): makefile.resolve() for makefile in makefiles
    }
    target_deps: dict[tuple[Path, str], set[tuple[Path, str]]] = {}
    for makefile in makefiles:
        lines_with_origin = _load_makefile_lines(makefile, repo)
        lines = [line for line, _ in lines_with_origin]

        translator_value = _find_var_value(lines, "TEST_TRANSLATOR")
        if translator_value:
            translator_value = _expand_autotools_vars(
                translator_value, repo, makefile.parent
            )
            translator = _tokenize(translator_value)[-1]
        else:
            translator_value = _find_var_value(lines, "TRANSLATOR_EXECUTABLE")
            if translator_value:
                translator_value = _expand_autotools_vars(
                    translator_value, repo, makefile.parent
                )
                translator = _tokenize(translator_value)[0]
            else:
                translator = "testTranslator"
        if "$" in translator or "@" in translator:
            translator = "testTranslator"

        lang_flags = _expand_autotools_vars(
            _split_comment(_find_var_value(lines, "LANG_FLAGS") or "")[0],
            repo,
            makefile.parent,
        )
        rose_flags = _expand_autotools_vars(
            _split_comment(_find_var_value(lines, "ROSE_FLAGS") or "")[0],
            repo,
            makefile.parent,
        )
        test_cxxflags = _expand_autotools_vars(
            _split_comment(_find_var_value(lines, "TEST_CXXFLAGS") or "")[0],
            repo,
            makefile.parent,
        )
        test_cflags = _expand_autotools_vars(
            _split_comment(_find_var_value(lines, "TEST_CFLAGS") or "")[0],
            repo,
            makefile.parent,
        )

        flags = _tokenize(
            " ".join([lang_flags, rose_flags, test_cxxflags, test_cflags]).strip()
        )
        flags = [
            token
            for token in flags
            if token not in {"(", ")"} and "$" not in token and "@" not in token
        ]

        var_map, cond_vars = _collect_makefile_var_map(lines_with_origin, repo)
        make_var = var_map.get("MAKE", "")
        if not make_var or any(ch in make_var for ch in ("@", "$")):
            var_map["MAKE"] = "make"
        var_map.setdefault("AM_V_GEN", "")
        var_map.setdefault("AM_V_at", "")
        var_map.setdefault("TESTS_ENVIRONMENT", "")
        var_map.setdefault("RTH_RUN", "rth_run.pl")
        var_map.setdefault("RTH_RUN_FLAGS", "")
        var_map.setdefault("RTH_RUN_FLAGS_V_0", "")
        var_map.setdefault("RTH_RUN_FLAGS_V_1", "")
        var_map.setdefault("LIBTOOL", "libtool")
        var_map.setdefault("VALGRIND_BINARY", "valgrind")
        var_map.setdefault("CC", "cc")
        var_map.setdefault("CXX", "c++")
        var_map.setdefault("F77", "f77")
        var_map.setdefault("FC", "fc")
        _prefer_long_make_check_list(var_map)
        minimal_input = _default_minimal_input(repo)
        if minimal_input:
            current_minimal = var_map.get("__minimal_input_code")
            if not current_minimal or "$" in current_minimal or "@" in current_minimal:
                var_map["__minimal_input_code"] = minimal_input
        program_targets = _collect_program_targets(
            var_map, repo, makefile.parent, cond_vars
        )
        seed_targets: list[tuple[str, str, Optional[str], Path, str, Path]] = []

        assign_re = re.compile(r"^(\s*#\s*)?([A-Za-z0-9_]+)\s*([:+?]?=)\s*(.*)$")
        for line, origin in lines_with_origin:
            match = assign_re.match(line)
            if not match:
                continue
            commented = bool(match.group(1))
            var = match.group(2)
            var_upper = var.upper()
            if any(hint in var_upper for hint in AUTOTOOLS_IGNORE_VAR_HINTS):
                continue
            value = match.group(4)
            active, commented_part = _split_comment(value)
            for part, is_commented in ((active, commented), (commented_part, True)):
                if not part.strip():
                    continue
                tokens = _tokenize(part)
                for token in tokens:
                    token = token.strip().strip("\"'")
                    if not token:
                        continue
                    token = _expand_autotools_vars(token, repo, origin.parent)
                    if "$" in token or "@" in token or token.startswith("-"):
                        continue
                    test_token = _normalize_test_token(token)
                    if not test_token:
                        continue
                    ext = Path(test_token).suffix
                    status, reason = _infer_status(var, is_commented)
                    labels = [repo_label, "autotools"]
                    if status == "disabled":
                        labels.append("disabled")

                    if _looks_like_test_var(var) and _should_record_make_target(
                        test_token, origin.parent, repo
                    ):
                        record_status = status
                        record_reason = reason
                        if record_status == "enabled":
                            root_status, root_reason = _root_target_status(test_token)
                            if root_status == "disabled":
                                record_status = root_status
                                record_reason = root_reason
                        seed_targets.append(
                            (
                                test_token,
                                record_status,
                                record_reason,
                                origin,
                                var,
                                origin.parent,
                            )
                        )

                    if ext in TEST_FILE_EXTS:
                        if not _looks_like_test_var(var):
                            continue
                        if not _is_compile_tests_path(origin):
                            continue
                        test_path = Path(test_token)
                        if not test_path.is_absolute():
                            test_path = (origin.parent / test_path).resolve()
                        if test_path.name.startswith(OUTPUT_FILE_PREFIXES):
                            continue
                        if not test_path.exists():
                            continue
                        if _drop_reason(test_path, repo):
                            continue
                        name = _rel_name_from_path(test_path, repo)
                        key = f"name:{name}"
                        command = [translator] + flags + ["-c", str(test_path)]
                        try:
                            origin_rel = origin.relative_to(repo)
                        except ValueError:
                            origin_rel = origin
                        entry = TestEntry(
                            key=key,
                            name=name,
                            command=command,
                            workdir=None,
                            env={},
                            labels=labels,
                            status=status,
                            disable_reason=reason,
                            origin=[
                                {
                                    "repo": repo_label,
                                    "buildsys": "autotools",
                                    "path": str(origin_rel),
                                    "notes": var,
                                }
                            ],
                            needs_manual_followup=False,
                            priority=_priority(repo_label, "autotools"),
                        )
                        entries.append(entry)
                        continue

                    if ext in SCRIPT_TEST_EXTS or ext == "":
                        if not any(hint in var_upper for hint in AUTOTOOLS_RUN_VAR_HINTS):
                            continue
                        if Path(test_token).name in TRANSLATOR_EXECUTABLES:
                            continue
                        script_path = None
                        if ext:
                            script_path = Path(test_token)
                            if not script_path.is_absolute():
                                script_path = (origin.parent / script_path).resolve()
                            if not script_path.exists() or not script_path.is_file():
                                continue
                            if _drop_reason(script_path, repo):
                                continue
                            command = [str(script_path)]
                        elif "/" in test_token or "\\" in test_token:
                            script_path = Path(test_token)
                            if not script_path.is_absolute():
                                script_path = (origin.parent / script_path).resolve()
                            if not script_path.exists() or not script_path.is_file():
                                continue
                            command = [str(script_path)]
                        else:
                            candidate = origin.parent / test_token
                            if candidate.exists() and candidate.is_file():
                                script_path = candidate.resolve()
                                command = [str(script_path)]
                            else:
                                if candidate.exists():
                                    continue
                                if test_token not in program_targets:
                                    continue
                                command = [test_token]
                        if script_path:
                            name = _rel_name_from_path(script_path, repo)
                        else:
                            name = _sanitize_test_name(test_token)
                        if _drop_name_reason(name):
                            continue
                        key = f"name:{name}"
                        try:
                            origin_rel = origin.relative_to(repo)
                        except ValueError:
                            origin_rel = origin
                        entry = TestEntry(
                            key=key,
                            name=name,
                            command=command,
                            workdir=str(origin.parent),
                            env={},
                            labels=labels,
                            status=status,
                            disable_reason=reason,
                            origin=[
                                {
                                    "repo": repo_label,
                                    "buildsys": "autotools",
                                    "path": str(origin_rel),
                                    "notes": var,
                                }
                            ],
                            needs_manual_followup=False,
                            priority=_priority(repo_label, "autotools"),
                        )
                        entries.append(entry)

        rules = _parse_makefile_rules(lines_with_origin, repo, var_map, cond_vars)
        rule_map: dict[str, list[MakefileRule]] = {}
        pattern_rules: list[MakefileRule] = []
        for rule in rules:
            if any("%" in target for target in rule.targets):
                pattern_rules.append(rule)
                continue
            for target in rule.targets:
                rule_map.setdefault(target, []).append(rule)

        if rule_map:
            root_targets = [
                target for target in rule_map if _is_make_check_target(target)
            ]
            extra_targets: list[str] = []
            for rule in rules:
                for target in rule.targets:
                    if target.endswith(
                        (".unfoldedConstants-o", ".foldedConstants-o", ".unnormalized-o")
                    ):
                        extra_targets.append(target)
            queue: list[tuple[str, str, Optional[str]]] = []
            for target in root_targets:
                status, reason = _root_target_status(target)
                queue.append((target, status, reason))
            for target in extra_targets:
                status, reason = _root_target_status(target)
                queue.append((target, status, reason))
            for seed_target, seed_status, seed_reason, _, _, _ in seed_targets:
                if seed_target in rule_map or _match_pattern_rules(
                    seed_target, pattern_rules
                ):
                    queue.append((seed_target, seed_status, seed_reason))
            visited_targets: set[tuple[str, str]] = set()
            processed_rules: set[tuple] = set()
            make_target_entries_seen: set[tuple[str, Path, str]] = set()

            while queue:
                target, root_status, root_reason = queue.pop()
                if (target, root_status) in visited_targets:
                    continue
                visited_targets.add((target, root_status))
                rule_instances: list[tuple[MakefileRule, Optional[str], list[str]]] = []
                direct_rules = rule_map.get(target, [])
                if direct_rules:
                    for rule in direct_rules:
                        rule_instances.append((rule, None, list(rule.deps)))
                else:
                    for rule, stem, deps in _match_pattern_rules(target, pattern_rules):
                        rule_instances.append((rule, stem, deps))

                for rule, stem, deps in rule_instances:
                    rule_id = (rule.origin, tuple(rule.targets), target, root_status)
                    if rule_id in processed_rules:
                        continue
                    processed_rules.add(rule_id)
                    target_key = (rule.origin.resolve(), target)
                    if _should_record_make_target(target, rule.origin.parent, repo):
                        entry_key = (target, rule.origin, root_status)
                        if entry_key not in make_target_entries_seen:
                            entry_status = root_status
                            entry_reason = root_reason
                            if rule.commented:
                                entry_status = "disabled"
                                entry_reason = (
                                    entry_reason or "commented-out in Makefile.am"
                                )
                            name = _make_target_name(target, rule.origin.parent, repo)
                            entry = _autotools_entry_from_command(
                                name=name,
                                command=[var_map.get("MAKE", "make"), target],
                                workdir=str(rule.origin.parent),
                                env={},
                                repo_label=repo_label,
                                status=entry_status,
                                disable_reason=entry_reason,
                                origin_path=rule.origin,
                                repo_root=repo,
                                origin_notes=f"target:{target}",
                            )
                            if entry:
                                entries.append(entry)
                            make_target_entries_seen.add(entry_key)
                    dep_token: Optional[str] = None
                    for dep in deps:
                        source_path = _resolve_make_target_to_source(
                            dep, rule.origin.parent
                        )
                        if source_path:
                            dep_token = str(source_path)
                            break
                        dep_path = Path(dep)
                        if not dep_path.is_absolute():
                            dep_path = (rule.origin.parent / dep_path).resolve()
                        if dep_path.exists():
                            dep_token = str(dep_path)
                            break
                    if not dep_token and deps:
                        dep_token = deps[0]

                    for dep in deps:
                        if dep in rule_map or _match_pattern_rules(dep, pattern_rules):
                            dep_key = (rule.origin.resolve(), dep)
                            target_deps.setdefault(target_key, set()).add(dep_key)
                            queue.append((dep, root_status, root_reason))
                        elif not rule.commands:
                            source_path = _resolve_make_target_to_source(
                                dep, rule.origin.parent
                            )
                            if source_path and not _drop_reason(source_path, repo):
                                name = _rel_name_from_path(source_path, repo)
                                status = (
                                    "enabled" if not rule.commented else "disabled"
                                )
                                reason = (
                                    "commented-out in Makefile.am"
                                    if rule.commented
                                    else None
                                )
                                if status == "enabled" and root_status == "disabled":
                                    status = "disabled"
                                    reason = reason or root_reason
                                entry = _autotools_entry_from_command(
                                    name=name,
                                    command=[translator] + flags + ["-c", str(source_path)],
                                    workdir=None,
                                    env={},
                                    repo_label=repo_label,
                                    status=status,
                                    disable_reason=reason,
                                    origin_path=rule.origin,
                                    repo_root=repo,
                                    origin_notes=f"dep:{target}",
                                )
                                if entry:
                                    entries.append(entry)

                    for cmd_idx, (cmd_line, cmd_commented) in enumerate(
                        rule.commands, start=1
                    ):
                        base_status = (
                            "disabled" if cmd_commented or rule.commented else "enabled"
                        )
                        base_reason = (
                            "commented-out in Makefile.am"
                            if base_status == "disabled"
                            else None
                        )
                        status = base_status
                        reason = base_reason
                        if status == "enabled" and root_status == "disabled":
                            status = "disabled"
                            reason = reason or root_reason
                        raw_command = _normalize_recipe_command(
                            cmd_line, var_map, repo, rule.origin.parent
                        )
                        if not raw_command:
                            continue
                        command_targets = [None]
                        if any(sym in raw_command for sym in ("$@", "$<", "$*", "$(@:")):
                            command_targets = [target]

                        for cmd_target in command_targets:
                            command = _replace_make_auto_vars(
                                raw_command, cmd_target, dep_token, stem
                            )
                            command = _expand_make_vars_value(
                                command, var_map, repo, rule.origin.parent
                            ).strip()
                            workdir, command = _split_cd_prefix(
                                command, rule.origin.parent
                            )
                            tokens = _tokenize(command)
                            if not tokens:
                                continue
                            if _is_shell_control(tokens):
                                continue
                            if tokens[0] in {"echo", "true", ":"}:
                                continue
                            if tokens[0] in {
                                "rm",
                                "rmdir",
                                "mkdir",
                                "touch",
                                "cp",
                                "mv",
                                "ln",
                                "install",
                                "chmod",
                                "chown",
                            }:
                                continue

                            base_dir = (
                                Path(workdir).resolve()
                                if workdir
                                else rule.origin.parent.resolve()
                            )
                            env: dict[str, str] = {}
                            cmd_tokens = list(tokens)
                            rth_kv: dict[str, str] = {}
                            rth_input_paths: list[Path] = []

                            rth_index = None
                            for idx, token in enumerate(tokens):
                                if Path(token).name == "rth_run.pl":
                                    rth_index = idx
                                    break
                            if rth_index is not None:
                                rth_tokens = tokens[rth_index:]
                                rth_kv = _parse_rth_run_kv(rth_tokens)
                                rth_kv = {
                                    key: _sanitize_rth_value(value)
                                    for key, value in rth_kv.items()
                                    if value
                                }
                                target_name = cmd_target or (
                                    rule.targets[0] if rule.targets else None
                                )
                                cfg_tokens, cfg_workdir = _extract_rth_config_command(
                                    rth_tokens, base_dir, target_name, rth_kv
                                )
                                if cfg_tokens:
                                    cmd_tokens = cfg_tokens
                                    if cfg_workdir:
                                        workdir = cfg_workdir
                                        base_dir = Path(workdir).resolve()
                                else:
                                    cmd_tokens = _extract_rth_cmd_tokens(rth_kv)
                                rth_input_paths = _extract_rth_input_paths(
                                    rth_kv, base_dir, repo
                                )
                                disabled_msg = rth_kv.get("DISABLED")
                                if disabled_msg:
                                    status = "disabled"
                                    reason = reason or f"rth_run:{disabled_msg}"
                                if cmd_tokens and cmd_tokens[0] in {"false", "/bin/false"}:
                                    status = "disabled"
                                    reason = reason or "rth_run:CMD=false"
                            else:
                                env, cmd_tokens = _split_env_tokens(tokens)
                                if _is_make_invocation(cmd_tokens):
                                    make_workdir = (
                                        Path(workdir).resolve()
                                        if workdir
                                        else rule.origin.parent.resolve()
                                    )
                                    idx = 1
                                    while idx < len(cmd_tokens):
                                        if cmd_tokens[idx] == "-C" and idx + 1 < len(cmd_tokens):
                                            make_workdir = (
                                                make_workdir / cmd_tokens[idx + 1]
                                            ).resolve()
                                            idx += 2
                                            continue
                                        if cmd_tokens[idx].startswith("-"):
                                            idx += 1
                                            continue
                                        break
                                    make_targets = cmd_tokens[idx:]
                                    for make_target in make_targets:
                                        if not make_target or make_target.startswith("-"):
                                            continue
                                        if "=" in make_target:
                                            continue
                                        dep_makefile = makefile_by_dir.get(
                                            make_workdir.resolve()
                                        )
                                        if dep_makefile:
                                            dep_key = (dep_makefile, make_target)
                                            target_deps.setdefault(target_key, set()).add(
                                                dep_key
                                            )
                                        if make_target in rule_map or _match_pattern_rules(
                                            make_target, pattern_rules
                                        ):
                                            queue.append((make_target, root_status, root_reason))
                                        if _should_record_make_target(
                                            make_target, make_workdir, repo
                                        ):
                                            seed_targets.append(
                                                (
                                                    make_target,
                                                    status,
                                                    reason,
                                                    rule.origin,
                                                    f"make:{make_target}",
                                                    make_workdir,
                                                )
                                            )
                                        source_path = _resolve_make_target_to_source(
                                            make_target, make_workdir
                                        )
                                        if source_path and not _drop_reason(
                                            source_path, repo
                                        ):
                                            name = _rel_name_from_path(source_path, repo)
                                            entry = _autotools_entry_from_command(
                                                name=name,
                                                command=[translator]
                                                + flags
                                                + ["-c", str(source_path)],
                                                workdir=None,
                                                env={},
                                                repo_label=repo_label,
                                                status=status,
                                                disable_reason=reason,
                                                origin_path=rule.origin,
                                                repo_root=repo,
                                                origin_notes=f"make:{make_target}",
                                            )
                                            if entry:
                                                entries.append(entry)
                                    continue

                            cmd_tokens = [tok for tok in cmd_tokens if "$" not in tok and "@" not in tok]
                            search_tokens = [
                                tok
                                for tok in (cmd_tokens or tokens)
                                if "$" not in tok and "@" not in tok
                            ]
                            script_path = None
                            if _command_has_dropped_token(
                                search_tokens or tokens, base_dir, repo
                            ):
                                continue
                            for tok in search_tokens:
                                candidate = _resolve_script_token(tok, base_dir)
                                if candidate and not _drop_reason(candidate, repo):
                                    script_path = candidate
                                    break

                            test_paths = _extract_test_paths(search_tokens, base_dir)
                            if rth_input_paths:
                                seen_paths: set[Path] = set()
                                merged_paths: list[Path] = []
                                for path in test_paths + rth_input_paths:
                                    if path not in seen_paths:
                                        merged_paths.append(path)
                                        seen_paths.add(path)
                                test_paths = merged_paths

                            is_translator = _is_translator_command(search_tokens)
                            prefer_target_name = bool(
                                rth_kv and cmd_target and not is_translator
                            )
                            if not test_paths and prefer_target_name:
                                name = _sanitize_test_name(
                                    _strip_passed_suffix(
                                        cmd_target
                                        or (rule.targets[0] if rule.targets else target)
                                    )
                                )
                                command_tokens = _unwrap_command_tokens(
                                    cmd_tokens or tokens
                                )
                                entry = _autotools_entry_from_command(
                                    name=name,
                                    command=command_tokens,
                                    workdir=workdir or str(rule.origin.parent),
                                    env=env,
                                    repo_label=repo_label,
                                    status=status,
                                    disable_reason=reason,
                                    origin_path=rule.origin,
                                    repo_root=repo,
                                    origin_notes=f"rule:{cmd_target or target}",
                                )
                                if entry:
                                    entries.append(entry)
                                continue

                            if test_paths:
                                tool_name = ""
                                if cmd_tokens or search_tokens:
                                    tool_name = Path(
                                        (cmd_tokens or search_tokens)[0]
                                    ).name
                                use_target_name = tool_name in {
                                    "testTranslatorUnfoldedConstants",
                                    "testTranslatorFoldedConstants",
                                }
                                if len(test_paths) > 1:
                                    prefer_target_name = True
                                if use_target_name or prefer_target_name:
                                    name = _sanitize_test_name(
                                        _strip_passed_suffix(
                                            cmd_target
                                            or (rule.targets[0] if rule.targets else target)
                                        )
                                    )
                                    command_tokens = _unwrap_command_tokens(
                                        cmd_tokens or tokens
                                    )
                                    entry = _autotools_entry_from_command(
                                        name=name,
                                        command=command_tokens,
                                        workdir=workdir or str(rule.origin.parent),
                                        env=env,
                                        repo_label=repo_label,
                                        status=status,
                                        disable_reason=reason,
                                        origin_path=rule.origin,
                                        repo_root=repo,
                                        origin_notes=f"rule:{cmd_target or target}",
                                    )
                                    if entry:
                                        entries.append(entry)
                                    continue
                                for test_path in test_paths:
                                    if _drop_reason(test_path, repo):
                                        continue
                                    name = _rel_name_from_path(test_path, repo)
                                    if is_translator:
                                        if cmd_tokens:
                                            command_tokens = _unwrap_command_tokens(
                                                cmd_tokens
                                            )
                                            entry_workdir = workdir or str(rule.origin.parent)
                                        else:
                                            command_tokens = (
                                                [translator]
                                                + flags
                                                + ["-c", str(test_path)]
                                            )
                                            entry_workdir = None
                                    else:
                                        command_tokens = _unwrap_command_tokens(
                                            cmd_tokens or tokens
                                        )
                                        entry_workdir = workdir or str(rule.origin.parent)
                                    entry = _autotools_entry_from_command(
                                        name=name,
                                        command=command_tokens,
                                        workdir=entry_workdir,
                                        env=env,
                                        repo_label=repo_label,
                                        status=status,
                                        disable_reason=reason,
                                        origin_path=rule.origin,
                                        repo_root=repo,
                                        origin_notes=f"rule:{cmd_target or target}",
                                    )
                                    if entry:
                                        entries.append(entry)
                                continue

                            if script_path:
                                name = _rel_name_from_path(script_path, repo)
                                command_tokens = _unwrap_command_tokens(
                                    cmd_tokens or tokens
                                )
                                entry = _autotools_entry_from_command(
                                    name=name,
                                    command=command_tokens,
                                    workdir=workdir or str(rule.origin.parent),
                                    env=env,
                                    repo_label=repo_label,
                                    status=status,
                                    disable_reason=reason,
                                    origin_path=rule.origin,
                                    repo_root=repo,
                                    origin_notes=f"rule:{cmd_target or target}",
                                )
                                if entry:
                                    entries.append(entry)
                                continue

                            exec_token = search_tokens[0] if search_tokens else ""
                            if not _looks_like_executable(exec_token, program_targets):
                                continue
                            name_target = cmd_target or (rule.targets[0] if rule.targets else None)
                            if name_target:
                                base_name_target = Path(str(name_target)).name
                                if base_name_target.startswith(OUTPUT_FILE_PREFIXES):
                                    continue
                            if name_target and not name_target.startswith("-"):
                                name = _sanitize_test_name(
                                    _strip_passed_suffix(name_target)
                                )
                            else:
                                origin_rel = rule.origin
                                try:
                                    origin_rel = rule.origin.relative_to(repo)
                                except ValueError:
                                    pass
                                name = _sanitize_test_name(
                                    f"{str(origin_rel).replace('/', '_')}_{cmd_idx}"
                                )
                            entry = _autotools_entry_from_command(
                                name=name,
                                command=tokens,
                                workdir=workdir or str(rule.origin.parent),
                                env=env,
                                repo_label=repo_label,
                                status=status,
                                disable_reason=reason,
                                origin_path=rule.origin,
                                repo_root=repo,
                                origin_notes=f"rule:{cmd_target or target}",
                            )
                            if entry:
                                entries.append(entry)
        for seed_target, seed_status, seed_reason, seed_origin, seed_notes, seed_dir in seed_targets:
            if not _should_record_make_target(seed_target, seed_dir, repo):
                continue
            if seed_target in rule_map or _match_pattern_rules(
                seed_target, pattern_rules
            ):
                continue
            name = _make_target_name(seed_target, seed_dir, repo)
            entry = _autotools_entry_from_command(
                name=name,
                command=[var_map.get("MAKE", "make"), seed_target],
                workdir=str(seed_dir),
                env={},
                repo_label=repo_label,
                status=seed_status,
                disable_reason=seed_reason,
                origin_path=seed_origin,
                repo_root=repo,
                origin_notes=f"target:{seed_target}",
            )
            if entry:
                entries.append(entry)
    target_to_entries: dict[tuple[Path, str], set[str]] = {}
    dir_to_tests: dict[Path, set[str]] = {}
    for entry in entries:
        for origin in entry.origin:
            if (
                origin.get("repo") != repo_label
                or origin.get("buildsys") != "autotools"
            ):
                continue
            notes = origin.get("notes", "")
            if not notes.startswith(("rule:", "dep:", "make:")):
                if not notes.startswith("target:"):
                    origin_path = (repo / origin["path"]).resolve()
                    dir_to_tests.setdefault(origin_path.parent, set()).add(entry.name)
                continue
            target = notes.split(":", 1)[1]
            origin_path = (repo / origin["path"]).resolve()
            target_to_entries.setdefault((origin_path, target), set()).add(entry.name)

    dep_cache: dict[tuple[Path, str], set[str]] = {}

    def _collect_dep_tests(
        key: tuple[Path, str], stack: set[tuple[Path, str]]
    ) -> set[str]:
        cached = dep_cache.get(key)
        if cached is not None:
            return set(cached)
        if key in stack:
            return set()
        stack.add(key)
        tests = set(target_to_entries.get(key, set()))
        for dep in target_deps.get(key, set()):
            tests.update(_collect_dep_tests(dep, stack))
        stack.remove(key)
        dep_cache[key] = set(tests)
        return tests

    for entry in entries:
        make_target = None
        origin_path = None
        for origin in entry.origin:
            if (
                origin.get("repo") != repo_label
                or origin.get("buildsys") != "autotools"
            ):
                continue
            notes = origin.get("notes", "")
            if notes.startswith("target:"):
                make_target = notes.split(":", 1)[1]
                origin_path = (repo / origin["path"]).resolve()
                break
        if make_target and origin_path:
            key = (origin_path, make_target)
            depends = sorted(_collect_dep_tests(key, set()))
            if not depends:
                fallback = sorted(dir_to_tests.get(origin_path.parent, set()))
                depends = fallback
            if entry.name in depends:
                depends.remove(entry.name)
            entry.depends = depends
    return entries


def _priority(repo_label: str, buildsys: str) -> int:
    order = {
        ("rex", "ctest"): 0,
        ("rex", "cmake"): 1,
        ("rex", "autotools"): 2,
        ("rose", "ctest"): 3,
        ("rose", "cmake"): 4,
        ("rose", "autotools"): 5,
    }
    return order.get((repo_label, buildsys), 9)


def _strip_cmake_comments(text: str) -> str:
    lines = []
    in_quote = False
    for line in text.splitlines():
        cleaned = []
        i = 0
        while i < len(line):
            ch = line[i]
            if ch == '"' and (i == 0 or line[i - 1] != "\\"):
                in_quote = not in_quote
                cleaned.append(ch)
                i += 1
                continue
            if ch == "#" and not in_quote:
                break
            cleaned.append(ch)
            i += 1
        lines.append("".join(cleaned))
    return "\n".join(lines)


def _comment_only_text(text: str) -> str:
    lines = []
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("#"):
            stripped = stripped[1:]
            if stripped.startswith(" "):
                stripped = stripped[1:]
            lines.append(stripped)
        else:
            lines.append("")
    return "\n".join(lines)


def _tokenize_cmake_args(value: str) -> list[str]:
    tokens: list[str] = []
    current: list[str] = []
    in_quote = False
    quote_had_prefix = False
    i = 0
    while i < len(value):
        ch = value[i]
        if in_quote:
            if ch == '"' and (i == 0 or value[i - 1] != "\\"):
                in_quote = False
                if not current and not quote_had_prefix:
                    tokens.append("")
                quote_had_prefix = False
            else:
                current.append(ch)
            i += 1
            continue
        if ch.isspace():
            if current:
                tokens.append("".join(current))
                current = []
            i += 1
            continue
        if ch == '"':
            in_quote = True
            quote_had_prefix = bool(current)
            i += 1
            continue
        current.append(ch)
        i += 1
    if current:
        tokens.append("".join(current))
    return tokens


def _parse_cmake_commands(text: str) -> list[CMakeCommand]:
    commands: list[CMakeCommand] = []
    pattern = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.MULTILINE)
    idx = 0
    while True:
        match = pattern.search(text, idx)
        if not match:
            break
        name = match.group(1)
        args_start = match.end()
        depth = 1
        i = args_start
        in_quote = False
        while i < len(text) and depth > 0:
            ch = text[i]
            if ch == '"' and (i == 0 or text[i - 1] != "\\"):
                in_quote = not in_quote
            elif not in_quote:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            i += 1
        if depth != 0:
            idx = match.end()
            continue
        arg_str = text[args_start : i - 1]
        args = _tokenize_cmake_args(arg_str)
        commands.append(CMakeCommand(name.lower(), args, 0))
        idx = i
    return commands


def _parse_cmake_nodes(
    commands: Sequence[CMakeCommand],
    start: int = 0,
    end_tokens: Optional[set[str]] = None,
) -> tuple[list[object], int]:
    if end_tokens is None:
        end_tokens = set()
    nodes: list[object] = []
    idx = start
    while idx < len(commands):
        cmd = commands[idx]
        if cmd.name in end_tokens:
            return nodes, idx
        if cmd.name == "foreach":
            var = cmd.args[0] if cmd.args else ""
            items = cmd.args[1:] if len(cmd.args) > 1 else []
            body, next_idx = _parse_cmake_nodes(commands, idx + 1, {"endforeach"})
            nodes.append(CMakeForEach(var, items, body, cmd.line))
            idx = next_idx + 1
            continue
        if cmd.name == "if":
            branches: list[CMakeIfBranch] = []
            cond = cmd.args
            body, next_idx = _parse_cmake_nodes(
                commands, idx + 1, {"elseif", "else", "endif"}
            )
            branches.append(CMakeIfBranch(cond, body))
            idx = next_idx
            while idx < len(commands) and commands[idx].name in ("elseif", "else"):
                branch_cmd = commands[idx]
                branch_cond = branch_cmd.args if branch_cmd.name == "elseif" else []
                body, next_idx = _parse_cmake_nodes(
                    commands, idx + 1, {"elseif", "else", "endif"}
                )
                branches.append(CMakeIfBranch(branch_cond, body))
                idx = next_idx
            if idx < len(commands) and commands[idx].name == "endif":
                idx += 1
            nodes.append(CMakeIfBlock(branches, cmd.line))
            continue
        if cmd.name in ("function", "macro"):
            func_name = cmd.args[0].lower() if cmd.args else ""
            params = cmd.args[1:] if len(cmd.args) > 1 else []
            end_token = "endfunction" if cmd.name == "function" else "endmacro"
            body, next_idx = _parse_cmake_nodes(commands, idx + 1, {end_token})
            nodes.append(
                CMakeFunction(func_name, params, body, cmd.line, cmd.name == "macro")
            )
            idx = next_idx + 1
            continue
        if cmd.name in ("endif", "endforeach", "endfunction", "endmacro"):
            return nodes, idx
        nodes.append(cmd)
        idx += 1
    return nodes, idx


def _clone_state(state: EvalState, parent: Optional[EvalState] = None) -> EvalState:
    return EvalState({k: list(v) for k, v in state.vars.items()}, parent=parent)


def _get_var(state: EvalState, name: str) -> list[str]:
    if name in state.vars:
        return state.vars[name]
    if state.parent is not None:
        return _get_var(state.parent, name)
    return []


def _set_var(state: EvalState, name: str, values: list[str], parent_scope: bool) -> None:
    target = state.parent if parent_scope and state.parent is not None else state
    target.vars[name] = values


def _set_cache_var(
    state: EvalState, context: CMakeContext, name: str, values: list[str]
) -> None:
    root = state
    while root.parent is not None:
        root = root.parent
    root.vars[name] = list(values)
    state.vars[name] = list(values)
    context.cache_vars.add(name)


def _append_var(state: EvalState, name: str, values: list[str]) -> None:
    if name in state.vars:
        current = state.vars[name]
    else:
        current = list(_get_var(state, name))
        state.vars[name] = current
    current.extend(values)


def _remove_var_items(state: EvalState, name: str, values: list[str]) -> None:
    if name in state.vars:
        current = state.vars.get(name, [])
    else:
        current = list(_get_var(state, name))
        state.vars[name] = current
    if not current:
        return
    state.vars[name] = [item for item in current if item not in values]


def _normalize_list_items(values: list[str]) -> list[str]:
    normalized: list[str] = []
    for value in values:
        if ";" in value:
            if re.search(r"\s", value):
                normalized.append(value)
            else:
                normalized.extend([part for part in value.split(";") if part])
        elif value:
            normalized.append(value)
    return normalized


_VAR_PATTERN = re.compile(r"\$\{([^}]+)\}")


def _expand_token(token: str, state: EvalState) -> list[str]:
    full_match = re.fullmatch(r"\$\{([^}]+)\}", token)
    if full_match:
        values = _get_var(state, full_match.group(1))
        return values or [""]

    def _replace(match: re.Match) -> str:
        values = _get_var(state, match.group(1))
        return values[0] if values else ""

    return [_VAR_PATTERN.sub(_replace, token)]


def _expand_tokens(tokens: Sequence[str], state: EvalState) -> list[str]:
    expanded: list[str] = []
    for token in tokens:
        expanded.extend(_expand_token(token, state))
    return _normalize_list_items(expanded)


def _expand_range_tokens(tokens: Sequence[str]) -> list[str]:
    if not tokens:
        return []
    try:
        values = [int(token) for token in tokens]
    except ValueError:
        return []
    if len(values) == 1:
        start, end, step = 0, values[0], 1
    elif len(values) == 2:
        start, end, step = values[0], values[1], 1
    else:
        start, end, step = values[0], values[1], values[2]
    if step == 0:
        return []
    if start <= end:
        return [str(value) for value in range(start, end + 1, step)]
    return [str(value) for value in range(start, end - 1, -abs(step))]


def _is_defined(state: EvalState, name: str) -> bool:
    if name in state.vars:
        return True
    if state.parent is not None:
        return _is_defined(state.parent, name)
    return False


def _truthy_value(value: Optional[str]) -> Optional[bool]:
    if value is None:
        return None
    stripped = value.strip()
    if stripped == "":
        return False
    upper = stripped.upper()
    if upper in {"0", "FALSE", "OFF", "NO", "N", "IGNORE", "NOTFOUND"}:
        return False
    if upper.endswith("-NOTFOUND"):
        return False
    if upper in {"1", "TRUE", "ON", "YES", "Y"}:
        return True
    return True


def _split_condition(tokens: list[str], keyword: str) -> list[list[str]]:
    parts: list[list[str]] = []
    current: list[str] = []
    for token in tokens:
        if token.upper() == keyword:
            parts.append(current)
            current = []
        else:
            current.append(token)
    parts.append(current)
    return parts


def _resolve_condition_token(token: str, state: EvalState) -> str:
    if _is_defined(state, token):
        values = _get_var(state, token)
        return values[0] if values else ""
    expanded = _expand_tokens([token], state)
    return expanded[0] if expanded else ""


def _eval_simple_condition(
    tokens: list[str], state: EvalState, context: CMakeContext
) -> Optional[bool]:
    if not tokens:
        return None
    head = tokens[0].upper()
    if head == "NOT":
        result = _eval_simple_condition(tokens[1:], state, context)
        return None if result is None else not result
    if head == "DEFINED" and len(tokens) > 1:
        return _is_defined(state, tokens[1])
    if head == "EXISTS" and len(tokens) > 1:
        paths = _expand_tokens(tokens[1:], state)
        if not paths:
            return None
        return any(Path(path).exists() for path in paths)
    if head == "TARGET" and len(tokens) > 1:
        target_tokens = _expand_tokens([tokens[1]], state)
        if not target_tokens:
            return None
        return target_tokens[0] in context.targets
    if len(tokens) >= 3 and tokens[1].upper() == "IN_LIST":
        var_values = [_resolve_condition_token(tokens[0], state)]
        list_values = _get_var(state, tokens[2])
        if not var_values or not list_values:
            return None
        return var_values[0] in list_values
    if len(tokens) >= 3 and tokens[1].upper() == "STREQUAL":
        left = _resolve_condition_token(tokens[0], state)
        right = _resolve_condition_token(tokens[2], state)
        return left == right
    if len(tokens) >= 3 and tokens[1].upper() == "EQUAL":
        left = _resolve_condition_token(tokens[0], state)
        right = _resolve_condition_token(tokens[2], state)
        try:
            return int(left) == int(right)
        except ValueError:
            return None
    if len(tokens) >= 3 and tokens[1].upper() in {"GREATER", "LESS", "GREATER_EQUAL", "LESS_EQUAL"}:
        try:
            left_val = int(_resolve_condition_token(tokens[0], state))
            right_val = int(_resolve_condition_token(tokens[2], state))
        except ValueError:
            return None
        op = tokens[1].upper()
        if op == "GREATER":
            return left_val > right_val
        if op == "LESS":
            return left_val < right_val
        if op == "GREATER_EQUAL":
            return left_val >= right_val
        if op == "LESS_EQUAL":
            return left_val <= right_val
    if len(tokens) == 1:
        token = tokens[0]
        if _is_defined(state, token):
            value = _get_var(state, token)
            return _truthy_value(value[0] if value else "")
        expanded = _expand_tokens([token], state)
        if expanded and expanded[0] != token:
            return _truthy_value(expanded[0])
        return _truthy_value(token)
    return None


def _eval_condition(
    tokens: list[str], state: EvalState, context: CMakeContext
) -> Optional[bool]:
    if not tokens:
        return None
    or_parts = _split_condition(tokens, "OR")
    if len(or_parts) > 1:
        results = [
            _eval_condition(part, state, context) for part in or_parts if part
        ]
        if any(result is True for result in results):
            return True
        if all(result is False for result in results):
            return False
        return None
    and_parts = _split_condition(tokens, "AND")
    if len(and_parts) > 1:
        results = [
            _eval_simple_condition(part, state, context)
            for part in and_parts
            if part
        ]
        if any(result is False for result in results):
            return False
        if all(result is True for result in results):
            return True
        return None
    return _eval_simple_condition(tokens, state, context)


def _merge_states(base_state: EvalState, branch_states: list[EvalState]) -> None:
    merged: dict[str, list[str]] = {}
    keys = set(base_state.vars.keys())
    for branch in branch_states:
        keys.update(branch.vars.keys())
    for key in keys:
        values: list[str] = []
        for state in [base_state] + branch_states:
            for value in state.vars.get(key, []):
                if value not in values:
                    values.append(value)
        if values:
            merged[key] = values
    base_state.vars.update(merged)


def _parse_add_test_tokens(tokens: list[str]) -> tuple[str, list[str], Optional[str]]:
    name: Optional[str] = None
    command: list[str] = []
    workdir: Optional[str] = None
    if any(token.upper() == "NAME" for token in tokens):
        idx = 0
        while idx < len(tokens):
            token = tokens[idx]
            upper = token.upper()
            if upper == "NAME" and idx + 1 < len(tokens):
                name = tokens[idx + 1]
                idx += 2
                continue
            if upper == "COMMAND":
                idx += 1
                while idx < len(tokens):
                    upper_next = tokens[idx].upper()
                    if upper_next in {"NAME", "COMMAND", "WORKING_DIRECTORY", "CONFIGURATIONS"}:
                        break
                    command.append(tokens[idx])
                    idx += 1
                continue
            if upper == "WORKING_DIRECTORY" and idx + 1 < len(tokens):
                workdir = tokens[idx + 1]
                idx += 2
                continue
            idx += 1
    else:
        if tokens:
            name = tokens[0]
            command = tokens[1:]
    return name or "", command, workdir


def _parse_properties(tokens: list[str]) -> dict[str, list[str]]:
    prop_keys = {
        "LABELS",
        "DISABLED",
        "WORKING_DIRECTORY",
        "ENVIRONMENT",
        "RESOURCE_LOCK",
        "DEPENDS",
        "TIMEOUT",
        "_BACKTRACE_TRIPLES",
    }
    def _is_prop_key(token: str) -> bool:
        return token == token.upper() and token.upper() in prop_keys

    props: dict[str, list[str]] = {}
    idx = 0
    while idx < len(tokens):
        prop_token = tokens[idx]
        prop = prop_token.upper()
        idx += 1
        if prop in {"LABELS", "ENVIRONMENT", "DEPENDS"} and _is_prop_key(prop_token):
            values: list[str] = []
            while idx < len(tokens) and not _is_prop_key(tokens[idx]):
                for item in tokens[idx].split(";"):
                    if item:
                        values.append(item)
                idx += 1
            props[prop] = values
            continue
        value = tokens[idx] if idx < len(tokens) else ""
        if idx < len(tokens):
            idx += 1
        props[prop] = [value] if value else []
    return props


def _apply_test_properties(
    name: str,
    props: dict[str, list[str]],
    context: CMakeContext,
) -> None:
    entry = context.tests.get(name)
    if not entry:
        pending = context.pending_props.setdefault(name, {})
        for key, value in props.items():
            pending[key] = (pending.get(key, []) or []) + value
        return
    labels = set(entry.labels)
    for key, value in props.items():
        if key == "LABELS":
            for item in _normalize_list_items(value):
                labels.add(item)
        elif key == "DISABLED":
            if any(item.upper() in {"1", "ON", "TRUE", "YES"} for item in value):
                entry.status = "disabled"
                entry.disable_reason = entry.disable_reason or "cmake:disabled"
        elif key == "WORKING_DIRECTORY" and value:
            entry.workdir = value[0]
        elif key == "ENVIRONMENT" and value:
            env_text = ";".join(value)
            for part in [p for p in env_text.split(";") if p]:
                if "=" in part:
                    key_name, val = part.split("=", 1)
                    entry.env[key_name] = val
        elif key == "DEPENDS":
            for item in _normalize_list_items(value):
                entry.depends.append(item)
    entry.labels = sorted(labels)
    if entry.depends:
        entry.depends = sorted(set(entry.depends))
    if entry.status == "disabled" and "disabled" not in entry.labels:
        entry.labels.append("disabled")


def _string_command(state: EvalState, args: list[str]) -> None:
    if not args:
        return
    op = args[0].upper()
    if op == "REPLACE" and len(args) >= 4:
        pattern = args[1]
        repl = args[2]
        out_var = args[3]
        input_tokens = _expand_tokens(args[4:], state)
        input_text = input_tokens[0] if input_tokens else ""
        _set_var(state, out_var, [input_text.replace(pattern, repl)], False)
        return
    if op == "JOIN" and len(args) >= 4:
        glue = args[1]
        out_var = args[2]
        items = _expand_tokens(args[3:], state)
        _set_var(state, out_var, [glue.join(items)], False)
        return
    if op == "CONCAT" and len(args) >= 3:
        out_var = args[1]
        items = _expand_tokens(args[2:], state)
        _set_var(state, out_var, ["".join(items)], False)
        return
    if op == "APPEND" and len(args) >= 3:
        out_var = args[1]
        items = _expand_tokens(args[2:], state)
        current = _get_var(state, out_var)
        base = current[0] if current else ""
        _set_var(state, out_var, [base + "".join(items)], False)
        return
    if op == "STRIP" and len(args) >= 3:
        input_tokens = _expand_tokens([args[1]], state)
        out_var = args[2]
        text = input_tokens[0].strip() if input_tokens else ""
        _set_var(state, out_var, [text], False)
        return
    if op == "FIND" and len(args) >= 4:
        input_tokens = _expand_tokens([args[1]], state)
        substring_tokens = _expand_tokens([args[2]], state)
        out_var = args[3]
        input_text = input_tokens[0] if input_tokens else ""
        substring = substring_tokens[0] if substring_tokens else ""
        _set_var(state, out_var, [str(input_text.find(substring))], False)


def _eval_math_expr(expr: str) -> Optional[int]:
    cleaned = expr.strip()
    if not cleaned:
        return None
    if not re.fullmatch(r"[0-9+\-*/%()\s]+", cleaned):
        return None
    try:
        return int(eval(cleaned, {"__builtins__": {}}, {}))
    except Exception:
        return None


def _is_config_var_name(name: str, context: CMakeContext) -> bool:
    if name in context.cache_vars:
        return True
    upper = name.upper()
    return upper.startswith(
        (
            "CMAKE_",
            "ENABLE-",
            "ENABLE_",
            "ROSE_",
            "HAVE_",
            "WITH_",
            "USE_",
            "BUILD_",
        )
    )


def _condition_is_config_gated(
    tokens: list[str], state: EvalState, context: CMakeContext
) -> bool:
    operators = {
        "AND",
        "OR",
        "NOT",
        "STREQUAL",
        "EQUAL",
        "GREATER",
        "LESS",
        "GREATER_EQUAL",
        "LESS_EQUAL",
        "IN_LIST",
        "VERSION_LESS",
        "VERSION_GREATER",
        "VERSION_EQUAL",
        "VERSION_GREATER_EQUAL",
        "VERSION_LESS_EQUAL",
    }
    for token in tokens:
        upper = token.upper()
        if upper in operators:
            continue
        if upper in {"DEFINED", "EXISTS", "TARGET"}:
            return True
        for var in _VAR_PATTERN.findall(token):
            if _is_config_var_name(var, context):
                return True
        if _is_config_var_name(token, context):
            return True
    return False


def _sanitize_command_tokens(tokens: list[str]) -> list[str]:
    sanitized: list[str] = []
    for token in tokens:
        if "$<TARGET_FILE:" in token:
            token = re.sub(r"\$<TARGET_FILE:([^>]+)>", r"\1", token)
        sanitized.append(token)
    return sanitized


def _handle_compile_test_builtin(
    args: list[str],
    state: EvalState,
    context: CMakeContext,
    forced_disable_reason: Optional[str],
) -> None:
    if len(args) < 2:
        return
    input_tokens = _expand_tokens([args[0]], state)
    label_tokens = _expand_tokens([args[1]], state)
    if not input_tokens or not label_tokens:
        return
    input_file = input_tokens[0]
    label = label_tokens[0]
    omit_includes = len(args) > 2
    if context.compile_test_mode == "rex":
        test_key = input_file.replace("/", "_").replace("\\", "_").replace(".", "_")
        test_name = f"{label}_{test_key}"
    else:
        test_name = f"{label}_{Path(input_file).stem}"

    translator = _get_var(state, "translator") or ["testTranslator"]
    rose_flags = _get_var(state, "ROSE_FLAGS")
    includes = _get_var(state, "ROSE_INCLUDE_FLAGS")
    source_dir = _get_var(state, "CMAKE_CURRENT_SOURCE_DIR")
    source_dir_value = source_dir[0] if source_dir else "."
    command = list(translator) + list(rose_flags)
    if not omit_includes:
        command += list(includes) + [f"-I{source_dir_value}"]
    command += ["-c", f"{source_dir_value}/{input_file}"]
    if forced_disable_reason:
        command = _sanitize_command_tokens(command)

    labels = [context.repo_label, "cmake", label]
    status = "disabled" if forced_disable_reason else "enabled"
    disable_reason = forced_disable_reason
    if "fail" in label.lower():
        status = "disabled"
        disable_reason = disable_reason or "cmake:known-failing"
    disabled_tests = _get_var(state, "ROSE_DISABLED_TESTCODES")
    if disabled_tests and input_file in disabled_tests:
        status = "disabled"
        disable_reason = disable_reason or "cmake:ROSE_DISABLED_TESTCODES"

    entry = TestEntry(
        key=_key_from_command(test_name, command, context.repo_root),
        name=test_name,
        command=command,
        workdir=None,
        env={},
        labels=labels,
        status=status,
        disable_reason=disable_reason,
        origin=[
            {
                "repo": context.repo_label,
                "buildsys": "cmake",
                "path": str(context.cmake_path.relative_to(context.repo_root)),
                "notes": "compile_test",
            }
        ],
        needs_manual_followup=False,
        priority=_priority(context.repo_label, "cmake"),
    )
    if entry.status == "disabled" and "disabled" not in entry.labels:
        entry.labels.append("disabled")
    context.tests[entry.name] = entry


def _bind_function_args(func: CMakeFunction, args: list[str], state: EvalState) -> None:
    state.vars["ARGC"] = [str(len(args))]
    state.vars["ARGV"] = list(args)
    for idx, value in enumerate(args):
        state.vars[f"ARGV{idx}"] = [value]
    for idx, param in enumerate(func.params):
        state.vars[param] = [args[idx]] if idx < len(args) else []
    state.vars["ARGN"] = list(args[len(func.params) :])


def _eval_function(
    func: CMakeFunction,
    args: list[str],
    state: EvalState,
    context: CMakeContext,
    forced_disable_reason: Optional[str],
) -> None:
    expanded_args = _expand_tokens(args, state)
    if func.is_macro:
        local_state = state
    else:
        local_state = _clone_state(state, parent=state)
    _bind_function_args(func, expanded_args, local_state)
    try:
        _eval_nodes(func.body, local_state, context, forced_disable_reason)
    except ReturnSignal:
        return


def _eval_command(
    cmd: CMakeCommand,
    state: EvalState,
    context: CMakeContext,
    forced_disable_reason: Optional[str],
) -> None:
    name = cmd.name
    if name == "set":
        if not cmd.args:
            return
        var = cmd.args[0]
        var_tokens = _expand_tokens([var], state)
        if var_tokens and var_tokens[0]:
            var = var_tokens[0]
        values = cmd.args[1:]
        parent_scope = False
        if values and values[-1].upper() == "PARENT_SCOPE":
            parent_scope = True
            values = values[:-1]
        has_cache = "CACHE" in values
        if has_cache:
            cache_idx = values.index("CACHE")
            values = values[:cache_idx]
        expanded = _expand_tokens(values, state)
        _set_var(state, var, expanded, parent_scope)
        if has_cache:
            _set_cache_var(state, context, var, expanded)
        return
    if name == "list":
        if not cmd.args:
            return
        action = cmd.args[0].upper()
        if len(cmd.args) < 2:
            return
        var = cmd.args[1]
        var_tokens = _expand_tokens([var], state)
        if var_tokens and var_tokens[0]:
            var = var_tokens[0]
        if action == "APPEND":
            items = _expand_tokens(cmd.args[2:], state)
            _append_var(state, var, items)
        elif action == "REMOVE_ITEM":
            items = _expand_tokens(cmd.args[2:], state)
            _remove_var_items(state, var, items)
        elif action == "REMOVE_DUPLICATES":
            values = _get_var(state, var)
            seen: set[str] = set()
            deduped: list[str] = []
            for value in values:
                if value in seen:
                    continue
                seen.add(value)
                deduped.append(value)
            _set_var(state, var, deduped, False)
        elif action == "FILTER":
            mode = None
            regex = None
            idx = 2
            while idx < len(cmd.args):
                token = cmd.args[idx].upper()
                if token in {"INCLUDE", "EXCLUDE"}:
                    mode = token
                    idx += 1
                    continue
                if token == "REGEX" and idx + 1 < len(cmd.args):
                    regex_tokens = _expand_tokens([cmd.args[idx + 1]], state)
                    regex = " ".join(regex_tokens) if regex_tokens else ""
                    idx += 2
                    continue
                idx += 1
            if mode and regex is not None:
                pattern = re.compile(regex)
                values = _get_var(state, var)
                if mode == "INCLUDE":
                    filtered = [value for value in values if pattern.search(value)]
                else:
                    filtered = [value for value in values if not pattern.search(value)]
                _set_var(state, var, filtered, False)
        elif action == "LENGTH" and len(cmd.args) >= 3:
            out_var = cmd.args[2]
            values = _get_var(state, var)
            _set_var(state, out_var, [str(len(values))], False)
        elif action == "GET" and len(cmd.args) >= 4:
            out_var = cmd.args[-1]
            values = _get_var(state, var)
            index_tokens = _expand_tokens(cmd.args[2:-1], state)
            indices: list[int] = []
            for token in index_tokens:
                try:
                    indices.append(int(token))
                except ValueError:
                    return
            results: list[str] = []
            for idx in indices:
                if idx < 0:
                    idx = len(values) + idx
                if 0 <= idx < len(values):
                    results.append(values[idx])
            _set_var(state, out_var, results, False)
        return
    if name == "get_filename_component" and len(cmd.args) >= 3:
        out_var = cmd.args[0]
        input_tokens = _expand_tokens([cmd.args[1]], state)
        mode = cmd.args[2].upper()
        input_value = input_tokens[0] if input_tokens else ""
        if mode == "NAME_WE":
            value = Path(input_value).stem
        elif mode == "NAME":
            value = Path(input_value).name
        else:
            value = input_value
        _set_var(state, out_var, [value], False)
        return
    if name == "string":
        _string_command(state, cmd.args)
        return
    if name == "math" and cmd.args:
        if cmd.args[0].upper() == "EXPR" and len(cmd.args) >= 3:
            out_var = cmd.args[1]
            expr = " ".join(_expand_tokens(cmd.args[2:], state))
            result = _eval_math_expr(expr)
            if result is not None:
                _set_var(state, out_var, [str(result)], False)
        return
    if name == "file" and cmd.args:
        op = cmd.args[0].upper()
        if op == "READ" and len(cmd.args) >= 3:
            path_tokens = _expand_tokens([cmd.args[1]], state)
            out_var = cmd.args[2]
            if not path_tokens:
                return
            path = Path(path_tokens[0])
            if not path.is_absolute():
                current_dir = _get_var(state, "CMAKE_CURRENT_LIST_DIR")
                base_dir = current_dir[0] if current_dir else str(context.cmake_path.parent)
                path = Path(base_dir) / path
            text = ""
            if path.exists():
                text = path.read_text(errors="ignore")
            _set_var(state, out_var, [text], False)
        elif op == "GLOB" and len(cmd.args) >= 3:
            out_var = cmd.args[1]
            patterns = _expand_tokens(cmd.args[2:], state)
            current_dir = _get_var(state, "CMAKE_CURRENT_SOURCE_DIR")
            base_dir = current_dir[0] if current_dir else str(context.cmake_path.parent)
            matches: list[str] = []
            for pattern in patterns:
                pattern_path = Path(pattern)
                if not pattern_path.is_absolute():
                    pattern_path = Path(base_dir) / pattern
                cache_key = f"{base_dir}::{pattern_path}"
                cached = context.cmake_glob_cache.get(cache_key)
                if cached is None:
                    cached = _cached_glob(str(pattern_path))
                    context.cmake_glob_cache[cache_key] = cached
                matches.extend(cached)
            _set_var(state, out_var, matches, False)
        return
    if name in {"add_executable", "add_library"}:
        if cmd.args and not forced_disable_reason:
            context.targets.add(cmd.args[0])
        return
    if name == "find_program" and cmd.args:
        var = cmd.args[0]
        if _get_var(state, var):
            return
        names: list[str] = []
        args_iter = iter(cmd.args[1:])
        for token in args_iter:
            upper = token.upper()
            if upper == "NAMES":
                continue
            if upper in {"PATHS", "HINTS", "PATH_SUFFIXES", "NO_DEFAULT_PATH", "REQUIRED"}:
                break
            names.append(token)
        found = None
        for name_token in names:
            candidate = shutil.which(name_token)
            if candidate:
                found = candidate
                break
        _set_var(state, var, [found or f"{var}-NOTFOUND"], False)
        return
    if name == "find_path" and cmd.args:
        var = cmd.args[0]
        if _get_var(state, var):
            return
        names: list[str] = []
        for token in cmd.args[1:]:
            upper = token.upper()
            if upper in {"PATHS", "HINTS", "PATH_SUFFIXES", "NO_DEFAULT_PATH", "REQUIRED"}:
                break
            names.append(token)
        search_dirs = [
            Path("/usr/include"),
            Path("/usr/local/include"),
        ]
        found = None
        for name_token in names:
            for base in search_dirs:
                candidate = base / name_token
                if candidate.exists():
                    found = str(base)
                    break
            if found:
                break
        _set_var(state, var, [found or f"{var}-NOTFOUND"], False)
        return
    if name == "include" and cmd.args:
        include_tokens = _expand_tokens([cmd.args[0]], state)
        if not include_tokens:
            return
        include_path = Path(include_tokens[0])
        if include_path.suffix == "":
            include_path = include_path.with_suffix(".cmake")
        if not include_path.is_absolute():
            current_dir = _get_var(state, "CMAKE_CURRENT_LIST_DIR")
            base_dir = current_dir[0] if current_dir else str(context.cmake_path.parent)
            include_path = Path(base_dir) / include_path
        if include_path.name == "GeneratedTests.cmake":
            return
        if include_path.exists():
            _eval_cmake_file(include_path, state, context, forced_disable_reason)
        return
    if name == "add_subdirectory" and cmd.args:
        subdir_tokens = _expand_tokens([cmd.args[0]], state)
        if not subdir_tokens:
            return
        subdir = Path(subdir_tokens[0])
        if not subdir.is_absolute():
            current_dir = _get_var(state, "CMAKE_CURRENT_SOURCE_DIR")
            base_dir = current_dir[0] if current_dir else str(context.cmake_path.parent)
            subdir = Path(base_dir) / subdir
        cmake_path = subdir / "CMakeLists.txt"
        if not cmake_path.exists():
            return
        if _drop_reason(cmake_path, context.repo_root):
            return
        if cmake_path.resolve() in context.visited_subdirs:
            return
        context.visited_subdirs.add(cmake_path.resolve())
        child_state = _clone_state(state, parent=state)
        child_state.vars["CMAKE_CURRENT_SOURCE_DIR"] = [str(subdir.resolve())]
        child_state.vars["CMAKE_CURRENT_LIST_DIR"] = [str(subdir.resolve())]
        build_dir = context.repo_root / "build"
        try:
            rel_dir = subdir.resolve().relative_to(context.repo_root)
            build_dir = build_dir / rel_dir
        except ValueError:
            pass
        child_state.vars["CMAKE_CURRENT_BINARY_DIR"] = [str(build_dir)]
        _eval_cmake_file(cmake_path, child_state, context, forced_disable_reason)
        return
    if name == "return":
        if forced_disable_reason:
            return
        raise ReturnSignal()
    if name == "add_test":
        tokens = _expand_tokens(cmd.args, state)
        name_token, command, workdir = _parse_add_test_tokens(tokens)
        if not name_token:
            return
        if _drop_name_reason(name_token):
            return
        if forced_disable_reason and name_token in context.tests:
            return
        if forced_disable_reason:
            command = _sanitize_command_tokens(command)
        current_dir = _get_var(state, "CMAKE_CURRENT_SOURCE_DIR")
        base_dir = Path(
            workdir or (current_dir[0] if current_dir else str(context.cmake_path.parent))
        )
        if not base_dir.is_absolute():
            base_dir = (context.cmake_path.parent / base_dir).resolve()
        if _command_has_dropped_token(command, base_dir, context.repo_root):
            return
        labels = [context.repo_label, "cmake"]
        status = "disabled" if forced_disable_reason else "enabled"
        disable_reason = forced_disable_reason
        if status == "disabled" and "disabled" not in labels:
            labels.append("disabled")
        entry = TestEntry(
            key=_key_from_command(name_token, command, context.repo_root),
            name=name_token,
            command=command,
            workdir=workdir,
            env={},
            labels=labels,
            status=status,
            disable_reason=disable_reason,
            origin=[
                {
                    "repo": context.repo_label,
                    "buildsys": "cmake",
                    "path": str(context.cmake_path.relative_to(context.repo_root)),
                    "notes": "add_test",
                }
            ],
            needs_manual_followup=False,
            priority=_priority(context.repo_label, "cmake"),
        )
        context.tests[name_token] = entry
        pending = context.pending_props.pop(name_token, None)
        if pending:
            _apply_test_properties(name_token, pending, context)
        return
    if name == "set_tests_properties":
        if "PROPERTIES" not in [arg.upper() for arg in cmd.args]:
            return
        idx = next(
            (i for i, arg in enumerate(cmd.args) if arg.upper() == "PROPERTIES"),
            None,
        )
        if idx is None:
            return
        test_names = _expand_tokens(cmd.args[:idx], state)
        props = _parse_properties(_expand_tokens(cmd.args[idx + 1 :], state))
        for test_name in test_names:
            if forced_disable_reason and test_name in context.tests:
                continue
            _apply_test_properties(test_name, props, context)
        return
    if name == "set_property":
        if not cmd.args:
            return
        if cmd.args[0].upper() not in {"TEST", "TESTS"}:
            return
        prop_idx = next(
            (i for i, arg in enumerate(cmd.args) if arg.upper() == "PROPERTY"),
            None,
        )
        if prop_idx is None:
            return
        test_names = _expand_tokens(cmd.args[1:prop_idx], state)
        props = _parse_properties(_expand_tokens(cmd.args[prop_idx + 1 :], state))
        for test_name in test_names:
            if forced_disable_reason and test_name in context.tests:
                continue
            _apply_test_properties(test_name, props, context)
        return
    if name in context.functions:
        _eval_function(context.functions[name], cmd.args, state, context, forced_disable_reason)
        return
    if name == "compile_test":
        _handle_compile_test_builtin(cmd.args, state, context, forced_disable_reason)


def _eval_nodes(
    nodes: list[object],
    state: EvalState,
    context: CMakeContext,
    forced_disable_reason: Optional[str],
) -> None:
    for node in nodes:
        if isinstance(node, CMakeFunction):
            context.functions[node.name] = node
            continue
        if isinstance(node, CMakeForEach):
            items: list[str] = []
            raw = node.items
            if raw and raw[0].upper() == "IN":
                mode = raw[1].upper() if len(raw) > 1 else ""
                if mode == "LISTS":
                    list_names = _expand_tokens(raw[2:], state)
                    for list_name in list_names:
                        items.extend(_get_var(state, list_name))
                elif mode == "ITEMS":
                    items = _expand_tokens(raw[2:], state)
                elif mode == "RANGE":
                    range_tokens = _expand_tokens(raw[2:], state)
                    items = _expand_range_tokens(range_tokens)
                else:
                    items = _expand_tokens(raw[1:], state)
            elif raw and raw[0].upper() == "RANGE":
                range_tokens = _expand_tokens(raw[1:], state)
                items = _expand_range_tokens(range_tokens)
            else:
                items = _expand_tokens(raw, state)
            previous = state.vars.get(node.var)
            for item in items:
                state.vars[node.var] = [item]
                _eval_nodes(node.body, state, context, forced_disable_reason)
            if previous is None:
                state.vars.pop(node.var, None)
            else:
                state.vars[node.var] = previous
            continue
        if isinstance(node, CMakeIfBlock):
            evaluated = _eval_condition(node.branches[0].condition, state, context)
            if evaluated is True:
                _eval_nodes(node.branches[0].body, state, context, forced_disable_reason)
                if _condition_is_config_gated(node.branches[0].condition, state, context):
                    for branch in node.branches[1:]:
                        disable_reason = forced_disable_reason or "cmake:condition false"
                        branch_state = _clone_state(state, parent=state.parent)
                        try:
                            _eval_nodes(branch.body, branch_state, context, disable_reason)
                        except ReturnSignal:
                            pass
                continue
            if evaluated is False:
                include_false_branches = _condition_is_config_gated(
                    node.branches[0].condition, state, context
                )
                if include_false_branches:
                    disable_reason = forced_disable_reason or "cmake:condition false"
                    branch_state = _clone_state(state, parent=state.parent)
                    try:
                        _eval_nodes(node.branches[0].body, branch_state, context, disable_reason)
                    except ReturnSignal:
                        pass
                active_branch_found = False
                unknown_branch = False
                for idx, branch in enumerate(node.branches[1:], start=1):
                    if not branch.condition:
                        if unknown_branch:
                            branch_state = _clone_state(state, parent=state.parent)
                            try:
                                _eval_nodes(
                                    branch.body,
                                    branch_state,
                                    context,
                                    forced_disable_reason or "cmake:condition unknown",
                                )
                            except ReturnSignal:
                                pass
                        else:
                            _eval_nodes(branch.body, state, context, forced_disable_reason)
                            active_branch_found = True
                            if include_false_branches:
                                for remaining in node.branches[idx + 1 :]:
                                    branch_state = _clone_state(state, parent=state.parent)
                                    try:
                                        _eval_nodes(
                                            remaining.body,
                                            branch_state,
                                            context,
                                            forced_disable_reason or "cmake:condition false",
                                        )
                                    except ReturnSignal:
                                        pass
                        break
                    branch_eval = _eval_condition(branch.condition, state, context)
                    if branch_eval is True:
                        _eval_nodes(branch.body, state, context, forced_disable_reason)
                        active_branch_found = True
                        if include_false_branches:
                            for remaining in node.branches[idx + 1 :]:
                                branch_state = _clone_state(state, parent=state.parent)
                                try:
                                    _eval_nodes(
                                        remaining.body,
                                        branch_state,
                                        context,
                                        forced_disable_reason or "cmake:condition false",
                                    )
                                except ReturnSignal:
                                    pass
                        break
                    if branch_eval is False:
                        if _condition_is_config_gated(branch.condition, state, context):
                            branch_state = _clone_state(state, parent=state.parent)
                            try:
                                _eval_nodes(
                                    branch.body,
                                    branch_state,
                                    context,
                                    forced_disable_reason or "cmake:condition false",
                                )
                            except ReturnSignal:
                                pass
                        continue
                    unknown_branch = True
                    branch_state = _clone_state(state, parent=state.parent)
                    try:
                        _eval_nodes(
                            branch.body,
                            branch_state,
                            context,
                            forced_disable_reason or "cmake:condition unknown",
                        )
                    except ReturnSignal:
                        pass
                if not active_branch_found and evaluated is False:
                    continue
                continue
            branch_states: list[EvalState] = []
            for branch in node.branches:
                branch_state = _clone_state(state, parent=state.parent)
                try:
                    _eval_nodes(
                        branch.body,
                        branch_state,
                        context,
                        forced_disable_reason or "cmake:condition unknown",
                    )
                except ReturnSignal:
                    pass
                branch_states.append(branch_state)
            _merge_states(state, branch_states)
            continue
        if isinstance(node, CMakeCommand):
            _eval_command(node, state, context, forced_disable_reason)


def _eval_cmake_file(
    cmake_path: Path,
    state: EvalState,
    context: CMakeContext,
    forced_disable_reason: Optional[str],
) -> None:
    if _drop_reason(cmake_path, context.repo_root):
        return
    context.cmake_path = cmake_path.resolve()
    prev_list_dir = state.vars.get("CMAKE_CURRENT_LIST_DIR")
    prev_source_dir = state.vars.get("CMAKE_CURRENT_SOURCE_DIR")
    state.vars["CMAKE_CURRENT_LIST_DIR"] = [str(cmake_path.parent.resolve())]
    state.vars["CMAKE_CURRENT_SOURCE_DIR"] = [str(cmake_path.parent.resolve())]

    text = cmake_path.read_text(errors="ignore")
    active_text = _strip_cmake_comments(text)
    commands = _parse_cmake_commands(active_text)
    nodes, _ = _parse_cmake_nodes(commands)
    returned = False
    try:
        _eval_nodes(nodes, state, context, forced_disable_reason)
    except ReturnSignal:
        returned = True
    if returned and not forced_disable_reason:
        return_state = _clone_state(state, parent=state.parent)
        try:
            _eval_nodes(nodes, return_state, context, "cmake:return")
        except ReturnSignal:
            pass

    commented_text = _comment_only_text(text)
    if commented_text.strip():
        comment_reason = forced_disable_reason or "commented-out in CMakeLists.txt"
        comment_commands = _parse_cmake_commands(commented_text)
        comment_nodes, _ = _parse_cmake_nodes(comment_commands)
        comment_state = _clone_state(state, parent=state.parent)
        try:
            _eval_nodes(comment_nodes, comment_state, context, comment_reason)
        except ReturnSignal:
            pass

    if prev_list_dir is None:
        state.vars.pop("CMAKE_CURRENT_LIST_DIR", None)
    else:
        state.vars["CMAKE_CURRENT_LIST_DIR"] = prev_list_dir
    if prev_source_dir is None:
        state.vars.pop("CMAKE_CURRENT_SOURCE_DIR", None)
    else:
        state.vars["CMAKE_CURRENT_SOURCE_DIR"] = prev_source_dir


def _collect_disabled_subdirs(tests_dir: Path) -> set[Path]:
    disabled: set[Path] = set()
    for cmake in tests_dir.rglob("CMakeLists.txt"):
        text = cmake.read_text(errors="ignore")
        commented_text = _comment_only_text(text)
        commands = _parse_cmake_commands(commented_text)
        for cmd in commands:
            if cmd.name != "add_subdirectory" or not cmd.args:
                continue
            subdir = cmd.args[0]
            if subdir.startswith("${"):
                continue
            disabled.add((cmake.parent / subdir).resolve())
    return disabled


def _path_is_relative(path: Path, base: Path) -> bool:
    try:
        path.relative_to(base)
        return True
    except ValueError:
        return False


def _drop_reason(path: Path, repo_root: Path) -> Optional[str]:
    if path.name in DROP_FILE_NAMES:
        return "dropped:file"
    if path.suffix in DROP_FILE_EXTS:
        return f"dropped:{path.suffix}"
    try:
        rel = path.resolve().relative_to(repo_root.resolve())
    except ValueError:
        return None
    for prefix in DROP_PATH_PREFIXES:
        if _path_is_relative(rel, prefix):
            return f"dropped:{prefix}"
    return None


def _drop_name_reason(name: str) -> Optional[str]:
    base = Path(name).name
    for prefix in DROP_NAME_PREFIXES:
        if base.startswith(prefix):
            return f"dropped:name-prefix:{prefix}"
    for prefix in OUTPUT_FILE_PREFIXES:
        if base.startswith(prefix):
            return f"dropped:output:{prefix}"
    if base in DROP_FILE_NAMES:
        return "dropped:file"
    if Path(base).suffix in DROP_FILE_EXTS:
        return f"dropped:{Path(base).suffix}"
    return None


def _disabled_reason_for_path(path: Path, disabled_dirs: set[Path]) -> Optional[str]:
    for disabled_dir in disabled_dirs:
        if _path_is_relative(path, disabled_dir):
            return f"commented-out add_subdirectory: {disabled_dir}"
    return None


def _extract_rose_includes(repo_root: Path, build_dir: Path) -> list[str]:
    cmake_path = repo_root / "CMakeLists.txt"
    if not cmake_path.exists():
        return []
    state = EvalState(
        vars={
            "CMAKE_CURRENT_SOURCE_DIR": [str(repo_root.resolve())],
            "CMAKE_CURRENT_BINARY_DIR": [str(build_dir.resolve())],
            "ROSE_TOP_SRC_DIR": [str(repo_root.resolve())],
            "ROSE_TOP_BINARY_DIR": [str(build_dir.resolve())],
        }
    )
    text = _strip_cmake_comments(cmake_path.read_text(errors="ignore"))
    commands = _parse_cmake_commands(text)
    for cmd in commands:
        if cmd.name != "set" or not cmd.args:
            continue
        if cmd.args[0] != "ROSE_INCLUDES":
            continue
        return _expand_tokens(cmd.args[1:], state)
    return []


def _parse_ctest_testfiles(build_dir: Path) -> dict[str, dict[str, object]]:
    tests: dict[str, dict[str, object]] = {}
    pending: dict[str, dict[str, list[str]]] = {}
    for testfile in build_dir.rglob("CTestTestfile.cmake"):
        text = _strip_cmake_comments(testfile.read_text(errors="ignore"))
        commands = _parse_cmake_commands(text)
        for cmd in commands:
            if cmd.name == "add_test":
                name, command, workdir = _parse_add_test_tokens(cmd.args)
                if not name:
                    continue
                tests.setdefault(name, {})
                tests[name]["command"] = command
                if workdir:
                    tests[name]["workdir"] = workdir
                if name in pending:
                    props = pending.pop(name)
                    tests[name].setdefault("labels", [])
                    for item in _normalize_list_items(props.get("LABELS", [])):
                        tests[name]["labels"].append(item)
                    if props.get("DISABLED") and _truthy_value(props["DISABLED"][0]):
                        tests[name]["disabled"] = True
                    if props.get("_BACKTRACE_TRIPLES"):
                        tests[name]["backtrace"] = list(props.get("_BACKTRACE_TRIPLES") or [])
                continue
            if cmd.name == "set_tests_properties":
                if "PROPERTIES" not in [arg.upper() for arg in cmd.args]:
                    continue
                idx = next(
                    (i for i, arg in enumerate(cmd.args) if arg.upper() == "PROPERTIES"),
                    None,
                )
                if idx is None:
                    continue
                test_names = cmd.args[:idx]
                props = _parse_properties(cmd.args[idx + 1 :])
                for test_name in test_names:
                    if test_name in tests:
                        tests[test_name].setdefault("labels", [])
                        for item in _normalize_list_items(props.get("LABELS", [])):
                            tests[test_name]["labels"].append(item)
                        if props.get("DISABLED") and _truthy_value(props["DISABLED"][0]):
                            tests[test_name]["disabled"] = True
                        if props.get("_BACKTRACE_TRIPLES"):
                            tests[test_name]["backtrace"] = list(props.get("_BACKTRACE_TRIPLES") or [])
                    else:
                        pending[test_name] = props
    return tests


def _should_override_ctest_command(entry_cmd: list[str], data_cmd: list[str]) -> bool:
    if not data_cmd:
        return False
    if not entry_cmd:
        return True
    if any(" " in tok for tok in data_cmd):
        return True
    if len(data_cmd) > len(entry_cmd):
        return True
    if entry_cmd and entry_cmd[0].startswith("-") and data_cmd and not data_cmd[0].startswith("-"):
        return True
    return False


def _extract_cmake_tests_from_ctest(
    repo_label: str, build_dir: Path, repo_root: Path
) -> list[TestEntry]:
    try:
        output = subprocess.check_output(
            ["ctest", "--test-dir", str(build_dir), "-N", "-V"],
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"ctest -N failed in {build_dir}") from exc

    entries: list[TestEntry] = []
    testfile_data = _parse_ctest_testfiles(build_dir)
    current: dict[str, object] = {}
    test_re = re.compile(r"^\s*Test\s+#\d+\s*:")
    for line in output.splitlines():
        stripped = re.sub(r"^\d+:\s*", "", line.strip())
        if test_re.match(stripped):
            if current.get("name"):
                entries.append(_ctest_entry(repo_label, current, repo_root))
            current = {}
            raw_name = stripped.split(":", 1)[1].strip()
            if any(token in raw_name for token in ("(Disabled)", "(Not Run)", "(Skipped)")):
                current["disabled"] = True
            current["name"] = _strip_ctest_suffix(raw_name)
        elif stripped.startswith("Test command:"):
            cmd = stripped.split("Test command:", 1)[1].strip()
            current["command"] = _tokenize(cmd)
        elif stripped.startswith("Working Directory:"):
            workdir = stripped.split("Working Directory:", 1)[1].strip()
            current["workdir"] = workdir
        elif stripped.startswith("Labels:"):
            labels = stripped.split("Labels:", 1)[1].strip()
            current["labels"] = [l for l in re.split(r"[;\s]+", labels) if l]
        elif stripped.startswith("Disabled:"):
            value = stripped.split("Disabled:", 1)[1].strip()
            current["disabled"] = value.lower() == "true"
    if current.get("name"):
        entries.append(_ctest_entry(repo_label, current, repo_root))
    filtered: list[TestEntry] = []
    for entry in entries:
        data = testfile_data.get(entry.name)
        if data and data.get("backtrace"):
            if any("GeneratedTests.cmake" in item for item in data.get("backtrace", [])):
                continue
        if data:
            if data.get("command"):
                data_cmd = list(data.get("command") or [])
                entry_cmd = list(entry.command or [])
                if _should_override_ctest_command(entry_cmd, data_cmd):
                    entry.command = data_cmd
            entry.workdir = entry.workdir or data.get("workdir")
            if data.get("labels"):
                entry.labels = sorted(set(entry.labels + data.get("labels", [])))
            if data.get("disabled"):
                entry.status = "disabled"
                entry.disable_reason = entry.disable_reason or "ctest:disabled"
            if entry.status == "disabled" and "disabled" not in entry.labels:
                entry.labels.append("disabled")
        filtered.append(entry)
    return filtered


def _ctest_entry(
    repo_label: str, data: dict[str, object], repo_root: Path
) -> TestEntry:
    name = str(data.get("name", "")).strip()
    command = list(data.get("command") or [])
    workdir = data.get("workdir")
    labels = list(data.get("labels") or [])
    status = "disabled" if data.get("disabled") else "enabled"
    disable_reason = "ctest:disabled" if status == "disabled" else None
    if status == "disabled" and "disabled" not in labels:
        labels.append("disabled")
    key = _key_from_command(name, command, repo_root)
    return TestEntry(
        key=key,
        name=name,
        command=command,
        workdir=str(workdir) if workdir else None,
        env={},
        labels=labels,
        status=status,
        disable_reason=disable_reason,
        origin=[
            {
                "repo": repo_label,
                "buildsys": "ctest",
                "path": str(Path("build") / "ctest"),
                "notes": "ctest -N -V",
            }
        ],
        needs_manual_followup=False,
        priority=_priority(repo_label, "ctest"),
    )


def _extract_cmake_tests_from_sources(
    repo: Path, repo_label: str, build_dir: Optional[Path] = None
) -> list[TestEntry]:
    compile_mode = "rex" if repo_label == "rex" else "rose"
    repo_root = repo.resolve()
    build_root = (build_dir or (repo_root / "build")).resolve()
    base_vars: dict[str, list[str]] = {}
    if build_dir and build_root.exists():
        base_vars.update(_load_cmake_cache(build_root))
    if "ENABLE-FORTRAN" in base_vars and "enable-fortran" not in base_vars:
        base_vars["enable-fortran"] = list(base_vars["ENABLE-FORTRAN"])
    if "ENABLE-C" in base_vars and "ROSE_BUILD_C_LANGUAGE_SUPPORT" not in base_vars:
        enable_c = base_vars.get("ENABLE-C", ["OFF"])[0].upper()
        base_vars["ROSE_BUILD_C_LANGUAGE_SUPPORT"] = ["1" if enable_c in {"ON", "TRUE", "YES", "1"} else "0"]
    if "ENABLE-FORTRAN" in base_vars and "ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT" not in base_vars:
        enable_fortran = base_vars.get("ENABLE-FORTRAN", ["OFF"])[0].upper()
        base_vars["ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT"] = [
            "1" if enable_fortran in {"ON", "TRUE", "YES", "1"} else "0"
        ]
    cache_vars = set(base_vars.keys())
    base_vars.setdefault("CMAKE_SOURCE_DIR", [str(repo_root)])
    base_vars.setdefault("CMAKE_BINARY_DIR", [str(build_root)])
    base_vars.setdefault("ROSE_TOP_SRC_DIR", [str(repo_root)])
    base_vars.setdefault("ROSE_TOP_BINARY_DIR", [str(build_root)])
    rose_includes = _extract_rose_includes(repo_root, build_root)
    if rose_includes:
        base_vars["ROSE_INCLUDES"] = rose_includes

    context = CMakeContext(
        repo_root=repo_root,
        repo_label=repo_label,
        cmake_path=repo_root / "CMakeLists.txt",
        compile_test_mode=compile_mode,
        cache_vars=cache_vars,
    )

    roots = [
        repo_root / "tests" / "smoke",
        repo_root / "tests" / "nonsmoke",
        repo_root / "src" / "frontend" / "SageIII" / "accparser" / "tests",
        repo_root / "src" / "frontend" / "SageIII" / "ompparser" / "tests",
    ]
    for root in roots:
        cmake_path = root / "CMakeLists.txt"
        if not cmake_path.exists():
            continue
        root_state = EvalState(vars={key: list(val) for key, val in base_vars.items()})
        root_state.vars["CMAKE_CURRENT_SOURCE_DIR"] = [str(root)]
        root_state.vars["CMAKE_CURRENT_LIST_DIR"] = [str(root)]
        try:
            rel_dir = root.relative_to(repo_root)
            root_build = build_root / rel_dir
        except ValueError:
            root_build = build_root
        root_state.vars["CMAKE_CURRENT_BINARY_DIR"] = [str(root_build)]
        _eval_cmake_file(cmake_path, root_state, context, None)

    return list(context.tests.values())


def _origin_fingerprint(entry: TestEntry) -> set[tuple[Optional[str], Optional[str]]]:
    return {
        (origin.get("repo"), origin.get("buildsys")) for origin in entry.origin
    }


def _looks_like_rth_kv_token(token: str) -> bool:
    if token.startswith("-") or "=" not in token:
        return False
    key = token.split("=", 1)[0]
    return re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", key) is not None


def _is_rth_boundary_token(token: str, base_dir: Optional[Path]) -> bool:
    if _looks_like_rth_kv_token(token):
        return True
    base = Path(token).name
    if base.endswith(".passed"):
        return True
    if base in SCRIPT_TEST_NAMES:
        return True
    if base_dir and _resolve_script_token(token, base_dir):
        return True
    return False


def _append_rth_cmd_args(
    cmd_tokens: list[str], command: list[str], workdir: Optional[str]
) -> list[str]:
    if not cmd_tokens:
        return cmd_tokens
    cmd_index = next((i for i, tok in enumerate(command) if tok.startswith("CMD=")), None)
    if cmd_index is None:
        return cmd_tokens
    base_dir = Path(workdir).resolve() if workdir else Path.cwd()
    extras: list[str] = []
    for extra in command[cmd_index + 1 :]:
        if _is_rth_boundary_token(extra, base_dir):
            break
        extras.append(extra)
    if extras:
        return cmd_tokens + extras
    return cmd_tokens


def _extract_signature_tokens(
    command: list[str], workdir: Optional[str] = None
) -> list[str]:
    if not command:
        return []
    head = Path(command[0]).name
    if head in {"sh", "bash"} and len(command) >= 3 and command[1] == "-c":
        script = command[2]
        extra = command[3:]
        if "$@" in script and extra:
            if extra[0] == "dummy":
                extra = extra[1:]
            if extra:
                script_tokens = _tokenize(script)
                expanded: list[str] = []
                replaced = False
                for tok in script_tokens:
                    stripped = tok.strip("'\"")
                    if stripped in {"$@", "${@}"}:
                        expanded.extend(extra)
                        replaced = True
                    else:
                        expanded.append(tok)
                if replaced:
                    return expanded
                return extra + script_tokens
        if extra:
            return [script] + extra
        return _tokenize(script)
    if head in {"rth_run.pl", "rth_run"}:
        rth_kv = _parse_rth_run_kv(command)
        rth_kv = {
            key: _sanitize_rth_value(value)
            for key, value in rth_kv.items()
            if value
        }
        base_dir = Path(workdir).resolve() if workdir else Path.cwd()
        cfg_tokens, _ = _extract_rth_config_command(
            command, base_dir, None, rth_kv
        )
        if cfg_tokens:
            return cfg_tokens
        cmd_tokens = _extract_rth_cmd_tokens(rth_kv)
        if cmd_tokens:
            return _append_rth_cmd_args(cmd_tokens, command, workdir)

    kv: dict[str, str] = {}
    for token in command:
        if "=" not in token or token.startswith("-"):
            continue
        key, value = token.split("=", 1)
        if key in {"CMD", "EXE", "ARGS"}:
            kv[key] = value
    if "CMD" in kv:
        return _tokenize(_sanitize_rth_value(kv["CMD"]))
    if "EXE" in kv:
        args = _tokenize(_sanitize_rth_value(kv.get("ARGS", "")))
        exe = _sanitize_rth_value(kv["EXE"])
        if exe:
            return [exe] + args
    return command


_SHELL_OPERATORS = {"&&", ";", "||", "|"}
_REDIRECT_RE = re.compile(r"^\d*[<>]")
_DURATION_RE = re.compile(r"^\d+[smhd]?$", re.IGNORECASE)


def _strip_shell_suffix(tokens: list[str]) -> list[str]:
    trimmed: list[str] = []
    for token in tokens:
        if token in _SHELL_OPERATORS:
            break
        if _REDIRECT_RE.match(token):
            continue
        if ("<" in token or ">" in token) and (
            token.startswith(("<", ">")) or re.match(r"^\d+[<>]", token)
        ):
            continue
        trimmed.append(token)
    return trimmed


def _drop_env_prefix(tokens: list[str]) -> list[str]:
    if tokens and tokens[0] == "env":
        tokens = tokens[1:]
    _, rest = _split_env_tokens(tokens)
    return rest


def _strip_wrapper_args(tokens: list[str], allow_duration: bool = False) -> list[str]:
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if token.startswith("-"):
            idx += 1
            continue
        if allow_duration and _DURATION_RE.match(token):
            idx += 1
            continue
        break
    return tokens[idx:]


def _unwrap_command_tokens(tokens: list[str]) -> list[str]:
    tokens = _strip_shell_suffix(tokens)
    tokens = _drop_env_prefix(tokens)
    changed = True
    while tokens and changed:
        changed = False
        head = tokens[0]
        head_lower = Path(head).name.lower()
        if head_lower == "env":
            tokens = _drop_env_prefix(tokens[1:])
            changed = True
            continue
        if "libtool" in head_lower or "libtool" in head.lower():
            tokens = _strip_wrapper_args(tokens[1:])
            tokens = _drop_env_prefix(tokens)
            changed = True
            continue
        if "valgrind" in head_lower:
            tokens = _strip_wrapper_args(tokens[1:])
            tokens = _drop_env_prefix(tokens)
            changed = True
            continue
        if head_lower in {"timeout", "timeout.sh", "gtimeout"}:
            tokens = _strip_wrapper_args(tokens[1:], allow_duration=True)
            tokens = _drop_env_prefix(tokens)
            changed = True
            continue
    return tokens


def _strip_variable_refs(token: str) -> str:
    token = token.replace("$$", "$")
    token = re.sub(r"@[^@]+@", "", token)
    token = re.sub(r"\$\([^)]+\)", "", token)
    token = re.sub(r"\$\{[^}]+\}", "", token)
    token = re.sub(r"\$[A-Za-z_][A-Za-z0-9_]*", "", token)
    token = token.replace("//", "/")
    return token


def _sanitize_rth_value(value: str) -> str:
    value = value.replace("$$", "$")
    value = re.sub(r"\$\(\s*abspath\s+([^)]+)\)", r"\1", value)
    value = re.sub(r"\$\(\s*notdir\s+([^)]+)\)", r"\1", value)
    value = re.sub(r"\$\(\s*pwd\s*\)", ".", value)
    value = _strip_variable_refs(value)
    value = value.replace("//", "/")
    return value.strip()


def _origin_has_path(entry: TestEntry, fragment: str) -> bool:
    for origin in entry.origin:
        path_str = origin.get("path", "")
        if fragment in path_str:
            return True
    return False


def _candidate_compile_dirs(entry: TestEntry, repo_root: Optional[Path]) -> list[Path]:
    if not repo_root:
        return []
    compile_root = repo_root / "tests/nonsmoke/functional/CompileTests"
    if not compile_root.exists():
        return []
    if any(
        _origin_has_path(entry, fragment)
        for fragment in (
            "staticCFG_tests",
            "virtualCFG_tests",
            "uninitializedField_tests",
        )
    ):
        return [
            compile_root / "Cxx_tests",
            compile_root / "C99_tests",
            compile_root / "Fortran_tests",
        ]
    return [
        compile_root / "Cxx_tests",
        compile_root / "C_subset_of_Cxx_tests",
        compile_root / "C_tests",
        compile_root / "C99_tests",
        compile_root / "C11_tests",
        compile_root / "C89_std_c89_tests",
        compile_root / "Fortran_tests",
        compile_root / "OpenMP_tests",
        compile_root / "OpenMP_tests/fortran",
        compile_root / "OpenACC_tests",
        compile_root / "OpenACC_tests/fortran",
    ]


def _strip_build_prefix(path: Path, repo_root: Optional[Path]) -> Path:
    if not repo_root:
        return path
    try:
        rel = path.relative_to(repo_root)
    except ValueError:
        return path
    if not rel.parts:
        return path
    if rel.parts[0].startswith("build") and len(rel.parts) > 1:
        return repo_root / Path(*rel.parts[1:])
    return path


def _find_candidate_in_dirs(
    filename: str, dirs: Iterable[Path]
) -> Optional[Path]:
    stem = Path(filename).stem
    for base_dir in dirs:
        candidate = base_dir / filename
        if candidate.exists():
            return candidate
        for ext in TEST_FILE_EXTS:
            candidate = base_dir / f"{stem}{ext}"
            if candidate.exists():
                return candidate
    return None


def _normalize_openmp_rename(
    path: Path, entry: TestEntry, repo_label: Optional[str], repo_roots: dict[str, Path]
) -> Path:
    if not repo_label or "OpenMP_tests" not in path.parts:
        return path
    other_label = "rex" if repo_label == "rose" else "rose"
    other_root = repo_roots.get(other_label)
    if not other_root:
        return path
    try:
        rel = path.relative_to(repo_roots[repo_label])
    except ValueError:
        return path
    if "CompileTests" not in rel.parts or "OpenMP_tests" not in rel.parts:
        return path
    normalized = re.sub(r"[_-]", "", path.name)
    if normalized == path.name:
        return path
    if (other_root / rel.parent / normalized).exists():
        return path.with_name(normalized)
    return path


def _normalize_signature_tokens(tokens: list[str], repo_root: Optional[Path]) -> list[str]:
    tokens = _unwrap_command_tokens(tokens)
    normalized: list[str] = []
    repo_root_str = str(repo_root.resolve()) if repo_root else ""
    for idx, token in enumerate(tokens):
        token = token.strip().replace("\\$", "$").replace("\\\"", "\"")
        token = token.strip("\"'")
        if not token:
            continue
        token = re.sub(r"\$<TARGET_FILE:([^>]+)>", r"\1", token)
        if "$" in token or "@" in token:
            token = _strip_variable_refs(token)
        if "$" in token or "@" in token:
            continue
        if repo_root_str and repo_root_str in token:
            token = token.replace(repo_root_str + "/", "")
            token = token.replace(repo_root_str, "")
        if idx == 0:
            token = Path(token).name
        if "/" in token and not token.startswith("-"):
            token = os.path.normpath(token)
            token = re.sub(r"\.temp\.int(?=\.)", ".temp", token)
        normalized.append(token)
    return normalized


def _command_signature(
    entry: TestEntry, repo_roots: dict[str, Path]
) -> Optional[str]:
    if not entry.command:
        return None
    tokens = _extract_signature_tokens(entry.command, entry.workdir)
    tokens = _unwrap_command_tokens(tokens)
    if not tokens:
        return None
    repo_label = entry.origin[0].get("repo") if entry.origin else ""
    repo_root = repo_roots.get(repo_label) if repo_label else None
    tool = Path(tokens[0]).name if tokens else ""
    tool_key = tool
    if tool == "unparseToString" and "--all" in tokens:
        tool_key = "unparseToString --all"
    if tool and tool not in KNOWN_SHELL_COMMANDS and tool != "cmake":
        input_tokens: list[str] = []
        for idx, tok in enumerate(tokens):
            if tok == "-c" and idx + 1 < len(tokens):
                input_tokens.append(tokens[idx + 1])
        if not input_tokens:
            input_tokens = list(tokens)
        inputs: list[str] = []
        for tok in input_tokens:
            tok = tok.strip().strip("\"'")
            if not tok:
                continue
            tok = _strip_variable_refs(tok)
            if not tok or tok.startswith("-") or "$" in tok or "@" in tok or "=" in tok:
                continue
            path = Path(tok.strip("\"'"))
            if path.name.startswith(OUTPUT_FILE_PREFIXES):
                continue
            if path.suffix not in TEST_FILE_EXTS:
                continue
            if not path.is_absolute():
                if entry.workdir:
                    path = (Path(entry.workdir) / path).resolve()
                elif repo_root:
                    path = (repo_root / path).resolve()
            if repo_root:
                try:
                    path = path.relative_to(repo_root)
                except ValueError:
                    pass
            inputs.append(str(path))
        if inputs:
            inputs = sorted(set(inputs))
            signature = f"{tool} | inputs={' '.join(inputs)}"
            return signature
    normalized = _normalize_signature_tokens(tokens, repo_root)
    if not normalized:
        return None
    if normalized[0] in {"true", "false", ":"}:
        return None
    if normalized[0] == "cmake" and len(normalized) > 1 and normalized[1] == "-E":
        return None
    signature = " ".join(normalized)
    if entry.workdir:
        workdir = entry.workdir
        if repo_root and str(repo_root.resolve()) in workdir:
            workdir = workdir.replace(str(repo_root.resolve()) + "/", "")
            workdir = workdir.replace(str(repo_root.resolve()), "")
        signature = f"{signature} | wd={workdir}"
    if entry.env:
        env_bits = [f"{key}={value}" for key, value in sorted(entry.env.items())]
        signature = f"{signature} | env={';'.join(env_bits)}"
    return signature


def _input_signature(entry: TestEntry, repo_roots: dict[str, Path]) -> Optional[str]:
    if not entry.command:
        return None
    repo_label = None
    if entry.origin:
        repo_label = entry.origin[0].get("repo")
    if not repo_label:
        for label in entry.labels:
            if label in repo_roots:
                repo_label = label
                break
    repo_root = repo_roots.get(repo_label) if repo_label else None
    tokens = _extract_signature_tokens(entry.command, entry.workdir)
    tokens = _unwrap_command_tokens(tokens)
    if not tokens:
        return None
    tool = Path(tokens[0]).name if tokens else ""
    tool_key = tool
    if tool == "unparseToString" and "--all" in tokens:
        tool_key = "unparseToString --all"
    if not tool or tool in KNOWN_SHELL_COMMANDS or tool == "cmake":
        return None
    input_tokens: list[str] = []
    for idx, tok in enumerate(tokens):
        if tok == "-c" and idx + 1 < len(tokens):
            input_tokens.append(tokens[idx + 1])
    if not input_tokens:
        input_tokens = list(tokens)
    base_dir: Optional[Path] = None
    if entry.workdir:
        base_dir = Path(entry.workdir)
    elif repo_root:
        base_dir = repo_root
    inputs: list[str] = []
    for tok in input_tokens:
        tok = tok.strip().strip("\"'")
        if not tok:
            continue
        tok = _strip_variable_refs(tok)
        if not tok or tok.startswith("-") or "$" in tok or "@" in tok or "=" in tok:
            continue
        candidate = _resolve_test_file_token(tok, base_dir) if base_dir else None
        if candidate:
            path = candidate
        else:
            path = Path(tok.strip("\"'"))
            if path.name.startswith(OUTPUT_FILE_PREFIXES):
                continue
            if path.suffix not in TEST_FILE_EXTS:
                continue
            if not path.is_absolute():
                if entry.workdir:
                    path = (Path(entry.workdir) / path).resolve()
                elif repo_root:
                    path = (repo_root / path).resolve()
            else:
                path = path.resolve()
        path = _strip_build_prefix(path, repo_root)
        path = _normalize_openmp_rename(path, entry, repo_label, repo_roots)
        if not path.exists():
            if path.suffix.lower() != ".caf":
                candidate = _find_candidate_in_dirs(
                    path.name, _candidate_compile_dirs(entry, repo_root)
                )
                if candidate:
                    path = candidate
        if repo_root:
            try:
                path = path.relative_to(repo_root)
            except ValueError:
                pass
        input_str = str(path)
        input_str = re.sub(r"\.temp\.int(?=\.)", ".temp", input_str)
        if ".temp." in input_str:
            input_str = input_str.replace(".temp.", ".")
        input_str = re.sub(r"\.c99$", ".c", input_str, flags=re.IGNORECASE)
        input_str = re.sub(r"\.f90$", ".f90", input_str, flags=re.IGNORECASE)
        input_str = re.sub(r"\.f03$", ".f03", input_str, flags=re.IGNORECASE)
        input_str = re.sub(r"\.f95$", ".f95", input_str, flags=re.IGNORECASE)
        input_str = re.sub(r"\.f08$", ".f08", input_str, flags=re.IGNORECASE)
        input_str = re.sub(r"\.f$", ".f", input_str, flags=re.IGNORECASE)
        inputs.append(input_str)
    if not inputs:
        return None
    inputs = sorted(set(inputs))
    return f"{tool_key} | inputs={' '.join(inputs)}"


def _input_only_signature(entry: TestEntry, repo_roots: dict[str, Path]) -> Optional[str]:
    signature = _input_signature(entry, repo_roots)
    if not signature or "inputs=" not in signature:
        return None
    return "inputs=" + signature.split("inputs=", 1)[1]


def _full_signature(entry: TestEntry, repo_roots: dict[str, Path]) -> Optional[str]:
    if not entry.command:
        return None
    repo_label = None
    if entry.origin:
        repo_label = entry.origin[0].get("repo")
    if not repo_label:
        for label in entry.labels:
            if label in repo_roots:
                repo_label = label
                break
    repo_root = repo_roots.get(repo_label) if repo_label else None
    tokens = _extract_signature_tokens(entry.command, entry.workdir)
    if not tokens:
        return None
    normalized = _normalize_signature_tokens(tokens, repo_root)
    if not normalized:
        return None
    return " ".join(normalized)


_GENERIC_LABELS = {
    "autotools",
    "cmake",
    "ctest",
    "rex",
    "rose",
    "disabled",
    "xfail",
    "known_fail",
    "known-fail",
    "expected_fail",
    "expected-fail",
    "needs_manual_followup",
}


def _label_key_set(labels: Sequence[str]) -> set[str]:
    keys: set[str] = set()
    for label in labels:
        if label in _GENERIC_LABELS:
            continue
        key = re.sub(r"[^a-z0-9]", "", label.lower())
        if not key:
            continue
        keys.add(key)
        if key.endswith("s") and len(key) > 1:
            keys.add(key[:-1])
        for suffix in ("failing", "fail", "disabled"):
            if key.endswith(suffix) and len(key) > len(suffix):
                base = key[: -len(suffix)]
                if base:
                    keys.add(base)
                    if base.endswith("s") and len(base) > 1:
                        keys.add(base[:-1])
    return keys


def _origin_hint_labels(entry: TestEntry) -> set[str]:
    hints: set[str] = set()
    for origin in entry.origin:
        path_str = origin.get("path")
        if not path_str:
            continue
        path = Path(path_str)
        parts = path.parts
        for anchor in (
            "CompileTests",
            "roseTests",
            "CompilerOptionsTests",
            "RunTests",
            "UnitTests",
        ):
            if anchor in parts:
                idx = parts.index(anchor)
                if idx + 1 < len(parts):
                    hints.add(parts[idx + 1])
        for anchor in ("functional", "unit", "smoke", "nonsmoke"):
            if anchor in parts:
                idx = parts.index(anchor)
                if idx + 1 < len(parts):
                    hints.add(parts[idx + 1])
        parent = path.parent.name
        if parent and parent not in {"tests", "nonsmoke", "smoke", "unit", "functional"}:
            if parent.endswith(("_tests", "Tests", "tests")):
                hints.add(parent)
        notes = origin.get("notes", "")
        if notes and any(tag in notes.upper() for tag in ("FAIL", "XFAIL")):
            hints.add("failing")
    return hints


def _entry_label_keys(entry: TestEntry) -> set[str]:
    keys = _label_key_set(entry.labels)
    hint_labels = _origin_hint_labels(entry)
    if hint_labels:
        keys |= _label_key_set(sorted(hint_labels))
    return keys


def _select_candidate_by_fail_hint(
    entry_keys: set[str], candidates: list[tuple[str, set[str]]]
) -> Optional[str]:
    if not candidates:
        return None
    prefer_fail = "failing" in entry_keys or "fail" in entry_keys
    if prefer_fail:
        fail_names = [
            name for name, _ in candidates if "fail" in name.lower()
        ]
        if len(fail_names) == 1:
            return fail_names[0]
        return None
    non_fail = [name for name, _ in candidates if "fail" not in name.lower()]
    if len(non_fail) == 1:
        return non_fail[0]
    return None


def _normalize_name_key(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


def _input_name_hints(entry: TestEntry, repo_roots: dict[str, Path]) -> set[str]:
    hints: set[str] = set()
    sig = _input_signature(entry, repo_roots)
    if not sig or "inputs=" not in sig:
        return hints
    input_part = sig.split("inputs=", 1)[1]
    for item in input_part.split():
        base = Path(item).name
        hints.add(_normalize_name_key(base))
        hints.add(_normalize_name_key(Path(base).stem))
    return {hint for hint in hints if hint}


def _cmake_sanitize_name(value: str) -> str:
    return value.replace("/", "_").replace(".", "_").replace("-", "_")


def _compile_test_key(value: str) -> str:
    return value.replace("/", "_").replace(".", "_")


def _load_cxx_failing_tests(repo_root: Path) -> set[str]:
    cmake_path = (
        repo_root / "tests/nonsmoke/functional/CompileTests/Cxx_tests/CMakeLists.txt"
    )
    if not cmake_path.exists():
        return set()
    lines = cmake_path.read_text(errors="ignore").splitlines()
    collecting = False
    items: list[str] = []
    for raw in lines:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if not collecting:
            if line.startswith("set(TESTCODE_CURRENTLY_FAILING"):
                collecting = True
                line = line[len("set(TESTCODE_CURRENTLY_FAILING"):].strip()
                if line.startswith("("):
                    line = line[1:].strip()
        if collecting:
            if ")" in line:
                before, _ = line.split(")", 1)
                if before.strip():
                    items.extend(before.split())
                break
            items.extend(line.split())
    return set(items)


def _load_c_tests_failing_tests(repo_root: Path) -> set[str]:
    cmake_path = (
        repo_root / "tests/nonsmoke/functional/CompileTests/C_tests/CMakeLists.txt"
    )
    if not cmake_path.exists():
        return set()
    lines = cmake_path.read_text(errors="ignore").splitlines()
    failing: list[str] = []
    removing: list[str] = []
    collecting = False
    remove_collecting = False
    for raw in lines:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if not collecting and line.startswith("set(TESTCODE_CURRENTLY_FAILING"):
            collecting = True
            line = line[len("set(TESTCODE_CURRENTLY_FAILING") :].strip()
            if line.startswith("("):
                line = line[1:].strip()
        if collecting:
            if ")" in line:
                before, _ = line.split(")", 1)
                if before.strip():
                    failing.extend(before.split())
                collecting = False
                continue
            failing.extend(line.split())
            continue
        if line.startswith("list(REMOVE_ITEM TESTCODE_CURRENTLY_FAILING"):
            rest = line[len("list(REMOVE_ITEM TESTCODE_CURRENTLY_FAILING") :].strip()
            if rest.startswith("("):
                rest = rest[1:].strip()
            if ")" in rest:
                before, _ = rest.split(")", 1)
                if before.strip():
                    removing.extend(before.split())
            else:
                remove_collecting = True
                if rest:
                    removing.extend(rest.split())
            continue
        if remove_collecting:
            if ")" in line:
                before, _ = line.split(")", 1)
                if before.strip():
                    removing.extend(before.split())
                remove_collecting = False
            else:
                removing.extend(line.split())
    if removing:
        return set(failing) - set(removing)
    return set(failing)


@functools.lru_cache(maxsize=None)
def _load_uninit_lists(repo_root: Optional[Path]) -> dict[str, set[str]]:
    if not repo_root:
        return {}
    cmake_path = (
        repo_root
        / "tests/nonsmoke/functional/CompileTests/uninitializedField_tests/uninitializedField_tests_lists.cmake"
    )
    if not cmake_path.exists():
        return {}
    lines = cmake_path.read_text(errors="ignore").splitlines()

    def _parse_list(var_name: str) -> set[str]:
        collecting = False
        items: list[str] = []
        for raw in lines:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if not collecting:
                if line.startswith(f"set({var_name}"):
                    collecting = True
                    line = line[len(f"set({var_name}") :].strip()
                    if line.startswith("("):
                        line = line[1:].strip()
                else:
                    continue
            if collecting:
                if ")" in line:
                    before, _ = line.split(")", 1)
                    if before.strip():
                        items.extend(before.split())
                    break
                items.extend(line.split())
        return set(items)

    return {
        "cxx": _parse_list("UNINIT_CXX_TEST_SOURCES"),
        "c": _parse_list("UNINIT_C_TEST_SOURCES"),
        "c99": _parse_list("UNINIT_C99_TEST_SOURCES"),
        "f90": _parse_list("UNINIT_F90_TEST_SOURCES"),
        "f77": _parse_list("UNINIT_F77_TEST_SOURCES"),
        "f03": _parse_list("UNINIT_F03_TEST_SOURCES"),
    }


def _apply_unparse_to_string_failing(
    entry: TestEntry, cxx_failing: set[str]
) -> None:
    if not entry.origin:
        return
    origin_path = str(entry.origin[0].get("path", ""))
    if not origin_path.endswith("CompileTests/unparseToString_tests/Makefile.am"):
        return
    if not entry.name.startswith(("ua_", "ut_")):
        return
    if entry.name.startswith(("ua_failing_", "ut_failing_")):
        return
    file_name = entry.name[3:]
    if file_name not in cxx_failing:
        return
    prefix = "ua_failing_" if entry.name.startswith("ua_") else "ut_failing_"
    entry.name = f"{prefix}{file_name}"
    entry.key = f"name:{entry.name}"
    if "known_fail" not in entry.labels:
        entry.labels.append("known_fail")
    if "disabled" not in entry.labels:
        entry.labels.append("disabled")
    entry.status = "disabled"
    entry.disable_reason = entry.disable_reason or "autotools:TESTCODE_CURRENTLY_FAILING"


def _explicit_name_mapping(
    entry: TestEntry,
    repo_roots: dict[str, Path],
    cxx_failing: set[str],
    c_tests_failing: set[str],
) -> Optional[str]:
    for origin in entry.origin:
        notes = origin.get("notes", "")
        if isinstance(notes, str) and notes.startswith("target:"):
            return None
    if not entry.origin:
        return None
    origin_path = str(entry.origin[0].get("path", ""))
    name = entry.name

    if origin_path.endswith("roseTests/astInterfaceTests/Makefile.am"):
        if "inputMovePreprocessingInfo" in name:
            return "astInterface_movePreprocessingInfo"
        if "inputAbiStuffTest" in name:
            return "astInterface_abiStuffTest"
        if name.startswith(("astInterface_", "rose_")):
            return None
        return f"astInterface_{name}"

    if origin_path.endswith("roseTests/varDeclNorm/Makefile.am"):
        prefix = "nonsmoke_functional_roseTests_varDeclNorm_"
        if name.startswith(prefix):
            return f"varDeclNorm_{name[len(prefix):]}"

    if origin_path.endswith("unit/SageInterface/Makefile.am"):
        if name.startswith("alias-") and name.endswith(".cpp"):
            return f"sage_performAliasAnalysis_{_cmake_sanitize_name(name)}"

    if origin_path.endswith("roseTests/programAnalysisTests/variableLivenessTests/Makefile.am"):
        match = re.match(r"runTest_(\d+)_?$", name)
        if match:
            return f"liveness_runTest_{match.group(1)}"

    if origin_path.endswith("roseTests/programAnalysisTests/variableRenamingTests/Makefile.am"):
        if name.startswith("tvr_"):
            specimen = name[len("tvr_") :]
            return f"variable_renaming_{_compile_test_key(specimen)}"

    if origin_path.endswith("roseTests/programTransformationTests/Makefile.am"):
        match = re.match(r"pre_(\d+)$", name)
        if match:
            return f"pre_pass{match.group(1)}.C"
        match = re.match(r"pr_(\d+)$", name)
        if match:
            return f"pr_rewrite_test{match.group(1)}.C"
        match = re.match(r"fd_(\d+)$", name)
        if match:
            return f"fd_finitediff_test{match.group(1)}.C"
        if name == "finiteDifferencingDemo":
            return "finiteDifferencingDemo_finitediff_test1.C"
        if name == "testFunctionNormalization":
            return "functionNormalization_functionNormalizationTest1.C"

    if origin_path.endswith("roseTests/astOutliningTests/Makefile.am"):
        if name == "complexStruct":
            return "outline_complexStruct"
        if name.startswith("outline_dlopen6_"):
            return None
        if name.startswith("outline_classic_"):
            if (
                "-rose:outline:select_omp_loop" in entry.command
                or any("OpenMP_tests" in token for token in entry.command)
            ):
                suffix = name[len("outline_classic_") :]
                return f"outline_classic_openmp_{suffix}"
            return None
        if name.startswith("classic_"):
            if (
                "-rose:outline:select_omp_loop" in entry.command
                or any("OpenMP_tests" in token for token in entry.command)
            ):
                specimen = name[len("classic_") :]
                return f"outline_classic_openmp_{_compile_test_key(specimen)}"
        if "-rose:outline:use_dlopen" in entry.command and "-rose:output" in entry.command:
            output_name = None
            for idx, token in enumerate(entry.command):
                if token == "-rose:output" and idx + 1 < len(entry.command):
                    output_name = Path(entry.command[idx + 1]).name
                    break
            if output_name and output_name.startswith("dlopen6_"):
                specimen = None
                for idx, token in enumerate(entry.command):
                    if token == "-c" and idx + 1 < len(entry.command):
                        specimen = Path(entry.command[idx + 1]).name
                        break
                if specimen:
                    return f"outline_dlopen6_{_compile_test_key(specimen)}"
        prefix_map = {
            "classic_": "outline_classic_",
            "inplace_": "outline_inplace_",
            "tofile_": "outline_tofile_",
            "dlopen_": "outline_dlopen_",
            "dlopen2_": "outline_dlopen2_",
            "dlopen3_": "outline_dlopen3_",
            "dlopensimple_": "outline_dlopensimple_",
            "dlopen4_": "outline_dlopen4_",
            "dlopen5_": "outline_dlopen5_",
            "default_": "outline_default_",
            "noswitch_": "outline_noswitch_",
            "seq7a_": "outline_seq7a_",
            "seq7b_": "outline_seq7b_",
            "failing_": "outline_failing_",
        }
        for prefix, mapped in prefix_map.items():
            if name.startswith(prefix):
                specimen = name[len(prefix) :]
                return f"{mapped}{_compile_test_key(specimen)}"

    if origin_path.endswith(
        "roseTests/programTransformationTests/extractFunctionArgumentsTest/Makefile.am"
    ):
        if name.startswith("normalizationTranslator_"):
            return None
        if name.startswith("nonsmoke_functional_CompileTests_Cxx_tests_"):
            name = name[len("nonsmoke_functional_CompileTests_Cxx_tests_") :]
        if Path(name).suffix in TEST_FILE_EXTS:
            return f"normalizationTranslator_{name}"

    if origin_path.endswith(
        "roseTests/programTransformationTests/singleStatementToBlockNormalization/Makefile.am"
    ):
        if name.startswith("singleStatementToBlockNormalization_"):
            return None
        prefix = "nonsmoke_functional_CompileTests_Cxx_tests_"
        if name.startswith(prefix):
            name = name[len(prefix) :]
        if Path(name).suffix in TEST_FILE_EXTS:
            return f"singleStatementToBlockNormalization_{name}"

    if origin_path.endswith("roseTests/fileLocation_tests/Makefile.am"):
        if name.startswith("fileLocation_"):
            return None
        prefix = "nonsmoke_functional_CompileTests_Cxx_tests_"
        if name.startswith(prefix):
            name = name[len(prefix) :]
        if Path(name).suffix in TEST_FILE_EXTS:
            return f"fileLocation_{name}"

    if origin_path.endswith("roseTests/astTokenStreamTests/CMakeLists.txt"):
        if name.startswith("tokenStreamMapping_") and name.endswith((".c", ".C")):
            return name.replace(".", "_")

    if origin_path.endswith("CompileTests/UnparseHeadersTests/CMakeLists.txt"):
        match = re.match(r"prepare_test(\d+)_Simple\d+\.C$", name)
        if match:
            return f"unparse_headers_prepare_test{match.group(1)}"
        match = re.match(r"test(\d+)_Simple\d+\.C$", name)
        if match:
            return f"unparse_headers_test{match.group(1)}"

    if origin_path.endswith("CompileTests/UnparseHeadersUsingTokenStream_tests/CMakeLists.txt"):
        match = re.match(r"UNPARSEHEADERTOKEN_prepare_test(\d+)_Simple\d+\.C$", name)
        if match:
            return f"unparse_tokens_prepare_test{match.group(1)}"

    if origin_path.endswith("CompileTests/UnparseHeadersUsingTokenStream_tests/Makefile.am"):
        match = re.match(r"test(\d+)$", name)
        if match:
            return f"unparse_tokens_test{match.group(1)}"

    if origin_path.endswith("CompileTests/unparseToString_tests/Makefile.am"):
        if name == "test_unparseProject":
            return "unparseProject_test2014_26"
        if name == "test_unparseProject_template":
            return "unparseProject_test2017_01"

    if origin_path.endswith("RunTests/FortranTests/LANL_POP/netcdf-4.1.1/Makefile.am"):
        if name == "check_nc_config":
            return "netcdf_check_nc_config"
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_test_prog."
        if name.startswith(prefix):
            ext = name[len(prefix):]
            return f"netcdf_test_prog_{ext.replace('.', '_')}"

    if "netcdf-4.1.1/nc_test4/Makefile.am" in origin_path:
        if name.startswith("netcdf_nc_test4_"):
            return None
        marker = "nc_test4_"
        if marker in name:
            name = name.split(marker, 1)[1]
        return f"netcdf_nc_test4_{_cmake_sanitize_name(name)}"

    if origin_path.endswith("CompileTests/staticCFG_tests/Makefile.am"):
        base_name = Path(name).name
        match = re.match(r"^(.*)\.CXX-INT(?:\.passed|-o)?$", base_name)
        if match:
            file_name = f"{match.group(1)}.C"
            prefix = (
                "testInterproceduralCFG_Cxx_failing_"
                if file_name in cxx_failing
                else "testInterproceduralCFG_Cxx_"
            )
            return f"{prefix}{file_name}"
        match = re.match(r"^(.*)\.CXX(?:\.passed|-o)?$", base_name)
        if match:
            file_name = f"{match.group(1)}.C"
            prefix = (
                "testStaticCFG_Cxx_failing_"
                if file_name in cxx_failing
                else "testStaticCFG_Cxx_"
            )
            return f"{prefix}{file_name}"

    if origin_path.endswith("CompileTests/uninitializedField_tests/Makefile.am"):
        rex_root = repo_roots.get("rex")
        uninit_lists = _load_uninit_lists(rex_root)
        if uninit_lists:
            input_file = None
            for token in reversed(entry.command):
                if Path(token).suffix in TEST_FILE_EXTS:
                    input_file = Path(token).name
                    break
            if not input_file:
                origin_notes = " ".join(origin.get("notes", "") for origin in entry.origin)
                match = re.search(r"rule:([^\s]+)", origin_notes)
                if match:
                    target_name = match.group(1)
                    if target_name.endswith(".passed"):
                        target_name = target_name[: -len(".passed")]
                    if target_name.endswith(".CXX-o"):
                        target_name = target_name[: -len(".CXX-o")] + ".C"
                    elif target_name.endswith(".CXX"):
                        target_name = target_name[: -len(".CXX")] + ".C"
                    if Path(target_name).suffix in TEST_FILE_EXTS:
                        input_file = Path(target_name).name
            if not input_file:
                prefix = "nonsmoke_functional_CompileTests_Cxx_tests_"
                if name.startswith(prefix):
                    input_file = Path(name[len(prefix):]).name
                elif name.endswith(tuple(TEST_FILE_EXTS)):
                    input_file = Path(name).name
            if input_file:
                if input_file.endswith(".temp.c"):
                    base = input_file[: -len(".temp.c")]
                    c_candidate = f"{base}.C"
                    c99_candidate = f"{base}.c"
                    if c_candidate in uninit_lists.get("c", set()):
                        return f"uninit_fields_c_{_compile_test_key(c_candidate)}"
                    if c99_candidate in uninit_lists.get("c99", set()):
                        return f"uninit_fields_c99_{_compile_test_key(c99_candidate)}"
                if input_file in uninit_lists.get("cxx", set()):
                    return f"uninit_fields_cxx_{_compile_test_key(input_file)}"
                if input_file in uninit_lists.get("c", set()):
                    return f"uninit_fields_c_{_compile_test_key(input_file)}"
                if input_file in uninit_lists.get("c99", set()):
                    return f"uninit_fields_c99_{_compile_test_key(input_file)}"
                if input_file in uninit_lists.get("f90", set()):
                    return f"uninit_fields_f90_{_compile_test_key(input_file)}"
                if input_file in uninit_lists.get("f77", set()):
                    return f"uninit_fields_f77_{_compile_test_key(input_file)}"
                if input_file in uninit_lists.get("f03", set()):
                    return f"uninit_fields_f03_{_compile_test_key(input_file)}"

    if origin_path.endswith("CompileTests/Cxx_tests/Makefile.am"):
        origin_notes = " ".join(origin.get("notes", "") for origin in entry.origin)
        if "test_pdf_generation" in origin_notes:
            return "Cxx_tests_test_pdf_generation"
        if "test_json_generation" in origin_notes:
            return "Cxx_tests_test_json_generation"
        if "test_common_configure_test_with_link_part_1" in origin_notes:
            return "Cxx_tests_common_configure_test_with_link_part_1"
        if "test_common_configure_test_with_link_part_2" in origin_notes:
            return "Cxx_tests_common_configure_test_with_link_part_2"
        if "test_common_configure_test_with_link_part_3" in origin_notes:
            return "Cxx_tests_common_configure_test_with_link_part_3"

        input_file = None
        for token in reversed(entry.command):
            if Path(token).suffix in (".C", ".cc", ".cpp", ".cxx"):
                input_file = Path(token).name
                break
        if not input_file:
            if name.endswith((".C", ".cc", ".cpp", ".cxx")):
                input_file = Path(name).name
            else:
                prefix = "nonsmoke_functional_CompileTests_Cxx_tests_"
                if name.startswith(prefix):
                    input_file = Path(name[len(prefix):]).name

        is_failing = False
        if entry.disable_reason and "TESTCODE_CURRENTLY_FAILING" in entry.disable_reason:
            is_failing = True
        for origin in entry.origin:
            if "TESTCODE_CURRENTLY_FAILING" in origin.get("notes", ""):
                is_failing = True
                break
        if input_file and input_file in cxx_failing:
            is_failing = True
        prefix_label = "Cxx_tests_failing_" if is_failing else "Cxx_tests_"
        if any(
            "testTranslatorUnfoldedConstants" in token
            for token in entry.command
        ):
            if name.endswith(".unfoldedConstants-o"):
                if ".C." not in name:
                    return name.replace(
                        ".unfoldedConstants-o", ".C.unfoldedConstants-o"
                    )
                return None
            if name.endswith(".C"):
                return f"{name}.unfoldedConstants-o"
        if name == "test2025_issue84_friend_template_in_class.cpp":
            return "Cxx_tests_rex_test2025_issue84_friend_template_in_class_cpp"
        prefix = "nonsmoke_functional_CompileTests_Cxx_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"{prefix_label}{_compile_test_key(suffix)}"
        if name.endswith((".C", ".cc", ".cpp", ".cxx")) and not name.startswith(
            ("Cxx_tests_", "Cxx_tests_failing_")
        ):
            return f"{prefix_label}{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/Cxx11_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_Cxx11_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"Cxx11_tests_{_compile_test_key(suffix)}"
        if "." not in name and (name.endswith("_ab") or name.endswith("ab")):
            return f"Cxx11_tests_{name}"
        if name.endswith((".C", ".cc", ".cpp", ".cxx")) and not name.startswith("Cxx11_tests_"):
            return f"Cxx11_tests_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/C11_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_C11_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"C11_tests_{_compile_test_key(suffix)}"
        if name.endswith(".c") and not name.startswith("C11_tests_"):
            return f"C11_tests_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/Cxx03_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_Cxx03_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"Cxx03_tests_{_compile_test_key(suffix)}"
        if name.endswith((".C", ".cc", ".cpp", ".cxx")) and not name.startswith("Cxx03_tests_"):
            return f"Cxx03_tests_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/Cxx20_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_Cxx20_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"Cxx20_tests_{_compile_test_key(suffix)}"
        if name.endswith((".C", ".cc", ".cpp", ".cxx")) and not name.startswith("Cxx20_tests_"):
            return f"Cxx20_tests_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/OpenClTests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_OpenClTests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            if suffix.endswith(".cl"):
                return f"OPENCLTEST_{_compile_test_key(suffix)}"
        if name.endswith(".cl") and not name.startswith("OPENCLTEST_"):
            return f"OPENCLTEST_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/C++Code/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_C__Code_"
        specimen = name[len(prefix):] if name.startswith(prefix) else name
        custom_map = {
            "test2004_19.C": "test2004_19",
            "test2004_21.c": "test2004_21",
            "test2004_30.C": "test2004_30",
            "test2004_41.C": "test2004_41",
            "test2005_56.C": "test2005_56",
            "test2005_64.C": "test2005_64",
            "test2005_168.c": "test2005_168",
            "test2005_172.c": "test2005_172",
            "test_CplusplusMacro_C.C": "test_CplusplusMacro_C",
            "test_CplusplusMacro_Cpp.C": "test_CplusplusMacro_Cpp",
        }
        if specimen in custom_map:
            return custom_map[specimen]
        if specimen.endswith("gconv_info.c"):
            if any("-rose:C99_only" in token for token in entry.command):
                return "gconv_info_C99"
            if any("-rose:C_only" in token for token in entry.command):
                return "gconv_info_C"
            return "gconv_info_Cpp"
        if specimen.endswith("stdio.c"):
            if any("-rose:C99_only" in token for token in entry.command):
                return "stdio_C99"
            if any(token in {"-rose:C", "-rose:C_only"} for token in entry.command):
                return "stdio_C"
            return "stdio_Cpp"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"CXXCODE_{_compile_test_key(suffix)}"

    if origin_path.endswith("CompileTests/C_tests/Makefile.am"):
        if name == "multiple_file_test_01":
            return "C_tests_multiple_file_test_01"
        if name.endswith("needs_reentrant.c") or name.endswith("needs_reentrant_c"):
            return "C_tests_check_pthread"
        if name == "test_else_case_disambiguation":
            return "C_tests_else_case_disambiguation"
        if name.endswith("conftest.c"):
            return "C_tests_common_configure_test_with_link"
        if name.endswith("test2005_168.c"):
            return "C_tests_test2005_168"
        if name.endswith("test2012_145.c"):
            return "C_tests_test2012_145"
        if name.endswith("test2015_162.c"):
            return "C_tests_compile_and_link_with_NDEBUG"
        if name.endswith("test2019_16b.c"):
            return "C_tests_multiple_file_test_01"
        is_failing = False
        if entry.disable_reason and "TESTCODE_CURRENTLY_FAILING" in entry.disable_reason:
            is_failing = True
        for origin in entry.origin:
            if "TESTCODE_CURRENTLY_FAILING" in origin.get("notes", ""):
                is_failing = True
                break
        input_file = None
        prefix = "nonsmoke_functional_CompileTests_C_tests_"
        if name.startswith(prefix):
            input_file = name[len(prefix) :]
        elif name.endswith(".c"):
            input_file = Path(name).name
        if input_file and input_file in c_tests_failing:
            is_failing = True
            entry.status = "disabled"
            entry.disable_reason = entry.disable_reason or "cmake:TESTCODE_CURRENTLY_FAILING"
            if "disabled" not in entry.labels:
                entry.labels.append("disabled")
        prefix_label = "C_tests_failing_" if is_failing else "C_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            if suffix.endswith(".c"):
                return f"{prefix_label}{_compile_test_key(suffix)}"
        if name.endswith(".c") and not name.startswith(("C_tests_", "C_tests_failing_")):
            return f"{prefix_label}{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/C89_std_c89_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_C89_std_c89_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            if suffix.endswith(".c"):
                return f"C89TEST_{_compile_test_key(suffix)}"
        if name.endswith(".c") and not name.startswith("C89TEST_"):
            return f"C89TEST_{_compile_test_key(name)}"

    if origin_path.endswith("CompileTests/OpenMP_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_OpenMP_tests_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            if "-rose:skipfinalCompileStep" in entry.command:
                return f"OMPACCTEST_{suffix}"
            return f"OMPTEST_{suffix}"

    if origin_path.endswith("CompileTests/OpenMP_tests/fortran/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_OpenMP_tests_fortran_"
        specimen = name[len(prefix):] if name.startswith(prefix) else name
        if Path(specimen).suffix in {".f", ".F", ".f90", ".F90"}:
            return f"OMPFORTRAN_{specimen}"

    if origin_path.endswith("CompileTests/virtualCFG_tests/Makefile.am"):
        base_name = Path(name).name
        match = re.match(r"^(.*)\.CXX(?:\.passed|-o)?$", base_name)
        if match:
            file_name = f"{match.group(1)}.C"
            prefix = (
                "testVirtualCFG_CXX_failing_"
                if file_name in cxx_failing
                else "testVirtualCFG_CXX_"
            )
            return f"{prefix}{file_name}"
        match = re.match(r"(.+)\.C99$", base_name)
        if match:
            return f"testVirtualCFG_C99_{match.group(1)}.c"

    if origin_path.endswith("CompileTests/OpenMP_tests/fortran/CMakeLists.txt"):
        if name == "macroIds":
            return "OMPTEST_macroIds"

    if origin_path.endswith("CompileTests/sizeofOperation_tests/Makefile.am"):
        if name.endswith("inputCode_SizeofTest.C"):
            return "SIZEOFOPTEST_inputCode_SizeofTest.C_GNU"

    if origin_path.endswith("CompileTests/STL_tests/Makefile.am"):
        if any("stl-eval.sh" in token for token in entry.command):
            if "no-cleanup" in entry.command:
                return "stl_eval_no_cleanup"
            if "only-cleanup" in entry.command:
                return "stl_eval_only_cleanup"
            if any(token.endswith("CPP11_STL_TESTS=yes") for token in entry.command):
                return "stl_eval_cpp11"
            if any(token.endswith("CPP11_STL_TESTS=no") for token in entry.command):
                return "stl_eval_no_cpp11"
            return "stl_eval_cpp11"

    if origin_path.endswith("moveDeclarationTool/Makefile.am"):
        prefix = "nonsmoke_functional_moveDeclarationTool_"
        specimen = name
        if specimen.startswith("rose_v1_"):
            specimen = specimen[len("rose_v1_") :]
            return f"moveDecl_v1_{specimen.replace('.', '_')}"
        if specimen.startswith("rose_v2_"):
            specimen = specimen[len("rose_v2_") :]
            return f"moveDecl_v2_{specimen.replace('.', '_')}"
        if specimen.startswith("rose_v3_"):
            specimen = specimen[len("rose_v3_") :]
            return f"moveDecl_v3_{specimen.replace('.', '_')}"
        if specimen.startswith(prefix):
            specimen = specimen[len(prefix) :]
        if specimen.startswith("inputmoveDeclarationToInnermostScope_"):
            return f"moveDecl_v1_{specimen.replace('.', '_')}"

    if origin_path.endswith("Utility/Makefile.am"):
        if name == "directorySupport":
            return "nonsmoke_utility_directorySupport"

    if origin_path.endswith("tests/nonsmoke/functional/Makefile.am"):
        origin_notes = " ".join(origin.get("notes", "") for origin in entry.origin)
        if "rule:testCppFileAnalysis" in origin_notes:
            return "functional_testCppFileAnalysis"
        if "rule:testCppFileCodeGeneration" in origin_notes:
            return "functional_testCppFileCodeGeneration"
        if "rule:testCppFileTranslator" in origin_notes:
            return "functional_testCppFileTranslator"
        functional_map = {
            "testObjectFileAnalysis": "functional_testObjectFileAnalysis",
            "testLinkFileAnalysis": "functional_testLinkFileAnalysis",
            "testCppFileAnalysis": "functional_testCppFileAnalysis",
            "testExecutableFileAnalysis": "functional_testExecutableFileAnalysis",
            "testObjectFileCodeGeneration": "functional_testObjectFileCodeGeneration",
            "testLinkFileCodeGeneration": "functional_testLinkFileCodeGeneration",
            "testCppFileCodeGeneration": "functional_testCppFileCodeGeneration",
            "testExecutableFileCodeGeneration": "functional_testExecutableFileCodeGeneration",
            "testObjectFileTranslator": "functional_testObjectFileTranslator",
            "testLinkFileTranslator": "functional_testLinkFileTranslator",
            "testCppFileTranslator": "functional_testCppFileTranslator",
            "testExecutableFileTranslator": "functional_testExecutableFileTranslator",
            "testSimpleLinkFileTranslator": "functional_testSimpleLinkFileTranslator",
            "testObjectFileTokenGeneration": "functional_testObjectFileTokenGeneration",
            "testGraphGeneration": "functional_testGraphGeneration",
            "testPDFGeneration": "functional_testPDFGeneration",
            "testTemplates": "functional_testTemplates",
            "testTranslatorFoldedConstants": "functional_testTranslatorFoldedConstants",
            "testTranslatorUnfoldedConstants": "functional_testTranslatorUnfoldedConstants",
            "testKeepGoingTranslator": "functional_testKeepGoingTranslator",
            "testExampleIdentityTranslator": "functional_testExampleIdentityTranslator",
            "testReadFileTwice": "functional_testReadFileTwice",
        }
        if name in functional_map:
            return functional_map[name]

    if origin_path.endswith("CompilerOptionsTests/testAnsiOption/Makefile.am"):
        return "compiler_options_ansi"

    if origin_path.endswith("CompilerOptionsTests/testFileNamesAndExtensions/Makefile.am"):
        if name.startswith(
            ("compiler_options_file_names_", "compiler_options_file_names_cxx_")
        ):
            return None
        specimen = name
        prefix = "nonsmoke_functional_CompilerOptionsTests_testFileNamesAndExtensions_"
        if specimen.startswith(prefix):
            specimen = specimen[len(prefix) :]
        cxx_only = {
            "suffix_02.c++",
            "suffix_05.cc",
            "suffix_07.cp",
            "suffix_09.cpp",
            "suffix_11.cxx",
            "suffix_03.C",
        }
        if specimen in cxx_only:
            return f"compiler_options_file_names_cxx_{specimen}"
        return f"compiler_options_file_names_{specimen}"

    if origin_path.endswith("CompilerOptionsTests/testCpreprocessorOption/Makefile.am"):
        if name.endswith("testSysIncludeOptionOrder.c"):
            return "compiler_options_cpreprocessor_sys_include_translator"
        if name.endswith("testIncludeOptionOrder.c"):
            return "compiler_options_cpreprocessor_include_order"

    if origin_path.endswith("CompilerOptionsTests/testGenerateSourceFileNames/Makefile.am"):
        if name == "test" or name.startswith(
            "nonsmoke_functional_CompilerOptionsTests_testGenerateSourceFileNames_"
        ):
            return "compiler_options_generate_source_file_names"

    if origin_path.endswith("CompilerOptionsTests/testGnuOptions/Makefile.am"):
        if name == "test_minus_x_option":
            return "compiler_options_gnu_minus_x"
        if name.endswith(("test-includeOption.C", "test-isystemOption.C")):
            return "compiler_options_gnu_options"

    if origin_path.endswith("CompilerOptionsTests/testHeaderFileOutput/Makefile.am"):
        if name.endswith("test2009_515.c"):
            return "compiler_options_header_file_output_include_order"

    if origin_path.endswith("CompilerOptionsTests/testNostdincOption/Makefile.am"):
        return "compiler_options_nostdinc"

    if origin_path.endswith("roseTests/astInterfaceTests/typeEquivalenceTests/Makefile.am"):
        if name.endswith("runVariableTypeTest.sh"):
            return "typeEquivalence_variableTypeTest"
        if name.endswith("runFunctionTypeTest.sh"):
            return "typeEquivalence_functionTypeTest"

    if origin_path.endswith("roseTests/programAnalysisTests/defUseAnalysisTests/Makefile.am"):
        match = re.match(r"runTest_(\d+)_?$", name)
        if match:
            return f"defuse_runTest_{match.group(1)}"

    if origin_path.endswith("roseTests/programAnalysisTests/typeTraitTests/Makefile.am"):
        match = re.match(r"nt_(test\d+)\.C$", name)
        if match:
            return f"type_trait_with_ret_{match.group(1)}_C"

    if origin_path.endswith("roseTests/programAnalysisTests/testCallGraphAnalysis/Makefile.am"):
        if name == "test01":
            return "callgraph_test01"
        match = re.match(r"test02-(\d+)$", name)
        if match:
            return f"callgraph_test02_{match.group(1)}"
        if name.startswith("t3_"):
            specimen = name[len("t3_") :]
            return f"callgraph_test03_{_compile_test_key(specimen)}"
        if name.startswith("callgraph_test04_") and name.endswith("_cg_dmp"):
            base = name[len("callgraph_test04_") : -len("_cg_dmp")]
            return f"callgraph_test04_{base}"
        if name.startswith("t4_"):
            specimen = name[len("t4_") :]
            if specimen.endswith(".cg.dmp"):
                specimen = specimen[: -len(".cg.dmp")]
            return f"callgraph_test04_{_compile_test_key(specimen)}"
        match = re.match(r"testNewCG_(\d+)$", name)
        if match:
            return f"callgraph_new_{int(match.group(1)):02d}"

    if origin_path.endswith("tests/nonsmoke/ExamplesForTestWriters/Makefile.am"):
        if name.startswith("parser_"):
            return f"nonsmoke_examples_{name}"
        if name == "noInputs":
            return "nonsmoke_examples_noInputs"

    if origin_path.endswith("tests/smoke/ExamplesForTestWriters/Makefile.am"):
        match = re.match(r"runAlgorithm_([a-d])$", name)
        if match:
            return f"smoke_runAlgorithm_{match.group(1)}"

    if origin_path.endswith("tests/smoke/unit/Utility/Makefile.am"):
        if name == "attributeTests":
            return "smoke_attributeTests"
        if name == "testMLog":
            return "smoke_testMLog"
        if name == "testStringUtility":
            return "smoke_testStringUtility"
        if name == "testSourceLocation":
            return "smoke_testSourceLocation"
        if name == "testBitOps":
            return "smoke_testBitOps"
        if name == "testToNumber":
            return "smoke_testToNumber"

    if origin_path.endswith("tests/nonsmoke/functional/RunTests/Makefile.am"):
        if name.endswith("inputtraverseCommonBlock.f"):
            return "RunTests_traverseCommonBlock"

    if origin_path.endswith("tests/nonsmoke/functional/translatorTests/Makefile.am"):
        if any(Path(token).name == "qualifiedName" for token in entry.command):
            return "testqualifiedName"
        prefix = "nonsmoke_functional_translatorTests_"
        specimen = name[len(prefix):] if name.startswith(prefix) else name
        if specimen in {"jacobi.c", "jacobi.C"}:
            return "testqualifiedName"
        if specimen == "inputbug125.C":
            return "testbug125"
        input_map = {
            "input_testConstDeclarations.C": "Translator_testConstDeclarations",
            "input_testPragmaInBody.c": "Translator_testPragmaInBody",
            "input_rex_genericPragma.c": "Translator_rex_genericPragma",
            "input_label_stmt_file_info_translator.C": "Translator_label_stmt_file_info",
            "input_querySubTree.C": "Translator_querySubTree",
            "input_ompVariableCollecting.C": "Translator_ompVariableCollecting",
            "input_testTranslator2010_2.C": "Translator_testTranslator2010_02",
            "input_testTranslator2010_3.C": "Translator_testTranslator2010_03",
            "input_moveStatementsBetweenBlocks.C": "Translator_moveStatementsBetweenBlocks",
            "input_testCopyAndDelete.c": "Translator_testCopyAndDelete_1",
            "input_testCopyAndDelete_2.C": "Translator_testCopyAndDelete_2",
            "input_testTraversalOfTemplateInstantiations.C": "Translator_testTraversalOfTemplateInstantiations",
            "input_testTranslator2013_01.C": "Translator_testTranslator2013_01",
            "input_testTranslator2013_02.C": "Translator_testTranslator2013_02",
            "input_testTranslator2018_01.C": "Translator_testTranslator2018_01",
            "input_testTranslator2018_02.C": "Translator_testTranslator2018_02",
            "input_testPlugins.C": "Translator_testPlugins",
        }
        if specimen in input_map:
            return input_map[specimen]
        if specimen.startswith("input_testTranslator2012_01"):
            return "Translator_testTranslator2012_01"

    if origin_path.endswith("CompileTests/Fortran_tests/experimental_frontend_tests/Makefile.am"):
        prefix = "nonsmoke_functional_CompileTests_Fortran_tests_experimental_frontend_tests_"
        suffix = name[len(prefix):] if name.startswith(prefix) else name
        suffix = _cmake_sanitize_name(suffix)
        origin_notes = " ".join(origin.get("notes", "") for origin in entry.origin)
        if "F90_TESTCODES_WORKING_ON" in origin_notes:
            return f"experimental_frontend_working_{suffix}"
        if "FAILING_TESTS" in origin_notes:
            return f"experimental_frontend_failing_{suffix}"
        if "NON_FORTRAN_FAILING_TESTS" in origin_notes:
            return f"experimental_frontend_non_fortran_{suffix}"
        if "F2018_TESTCODES" in origin_notes:
            return f"experimental_frontend_f2018_{suffix}"
        if "F90_TESTCODES" in origin_notes:
            return f"experimental_frontend_f90_{suffix}"

    if "netcdf-4.1.1/cxx/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_cxx_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_cxx_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/examples/CDL/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_examples_CDL_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_examples_cdl_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/examples/CXX/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_examples_CXX_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_examples_cxx_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/examples/CXX4/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_examples_CXX4_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_examples_cxx4_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/examples/F77/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_examples_F77_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_examples_f77_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/examples/F90/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_examples_F90_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_examples_f90_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/libcf/src/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_src_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_libcf_src_{_cmake_sanitize_name(suffix)}"
        if not name.startswith("netcdf_libcf_src_"):
            return f"netcdf_libcf_src_{_cmake_sanitize_name(name)}"

    if "netcdf-4.1.1/libcf/cfcheck/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_cfcheck_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_libcf_cfcheck_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/libcf/gridspec/tools/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libcf_gridspec_tools_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_gridspec_tools_{_cmake_sanitize_name(suffix)}"

    if "netcdf-4.1.1/libsrc4/Makefile.am" in origin_path:
        prefix = "nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_libsrc4_"
        if name.startswith(prefix):
            suffix = name[len(prefix):]
            return f"netcdf_libsrc4_{_cmake_sanitize_name(suffix)}"
        if not name.startswith("netcdf_libsrc4_"):
            return f"netcdf_libsrc4_{_cmake_sanitize_name(name)}"

    return None


def _merge_entries(entries: list[TestEntry], repo_roots: dict[str, Path]) -> list[dict]:
    rex_root = repo_roots.get("rex")
    cxx_failing = _load_cxx_failing_tests(rex_root) if rex_root else set()
    c_tests_failing = _load_c_tests_failing_tests(rex_root) if rex_root else set()
    rename_map: dict[str, str] = {}
    signature_to_names: dict[str, set[str]] = {}
    input_signature_to_names: dict[str, set[str]] = {}
    input_signature_to_candidates: dict[str, list[tuple[str, set[str]]]] = {}
    signature_to_candidates: dict[str, list[tuple[str, set[str]]]] = {}
    input_only_to_names: dict[str, set[str]] = {}
    input_only_to_candidates: dict[str, list[tuple[str, set[str]]]] = {}
    for entry in entries:
        if not _has_ctest_origin(entry):
            continue
        signature = _full_signature(entry, repo_roots)
        if not signature:
            continue
        signature_to_names.setdefault(signature, set()).add(entry.name)
        signature_to_candidates.setdefault(signature, []).append(
            (entry.name, _label_key_set(entry.labels))
        )
        input_signature = _input_signature(entry, repo_roots)
        if input_signature:
            input_signature_to_names.setdefault(input_signature, set()).add(entry.name)
            input_signature_to_candidates.setdefault(input_signature, []).append(
                (entry.name, _label_key_set(entry.labels))
            )
        input_only_signature = _input_only_signature(entry, repo_roots)
        if input_only_signature:
            input_only_to_names.setdefault(input_only_signature, set()).add(entry.name)
            input_only_to_candidates.setdefault(input_only_signature, []).append(
                (entry.name, _label_key_set(entry.labels))
            )
    signature_map = {
        sig: next(iter(names))
        for sig, names in signature_to_names.items()
        if len(names) == 1
    }
    input_signature_map = {
        sig: next(iter(names))
        for sig, names in input_signature_to_names.items()
        if len(names) == 1
    }
    input_only_map = {
        sig: next(iter(names))
        for sig, names in input_only_to_names.items()
        if len(names) == 1
    }
    for entry in entries:
        if _has_ctest_origin(entry):
            continue
        original_name = entry.name
        _apply_unparse_to_string_failing(entry, cxx_failing)
        if entry.name != original_name:
            rename_map[original_name] = entry.name
        explicit_name = _explicit_name_mapping(entry, repo_roots, cxx_failing, c_tests_failing)
        if explicit_name:
            if explicit_name != entry.name:
                rename_map[entry.name] = explicit_name
            entry.name = explicit_name
            entry.key = f"name:{explicit_name}"
            continue
        signature = _full_signature(entry, repo_roots)
        target_name = signature_map.get(signature) if signature else None
        if not target_name:
            if signature:
                sig_candidates = signature_to_candidates.get(signature, [])
                if sig_candidates:
                    entry_keys = _entry_label_keys(entry)
                    sig_matches = []
                    if entry_keys:
                        sig_matches = [
                            name
                            for name, keys in sig_candidates
                            if keys and keys.intersection(entry_keys)
                        ]
                        if len(sig_matches) == 1:
                            target_name = sig_matches[0]
                    if not target_name and sig_candidates:
                        target_name = _select_candidate_by_fail_hint(
                            entry_keys, sig_candidates
                        )
                    if not target_name:
                        hints = _input_name_hints(entry, repo_roots)
                        sig_hint_matches = []
                        candidates_to_check = (
                            [(name, keys) for name, keys in sig_candidates if name in sig_matches]
                            if sig_matches
                            else sig_candidates
                        )
                        if hints:
                            for name, _ in candidates_to_check:
                                name_key = _normalize_name_key(name)
                                if any(hint and hint in name_key for hint in hints):
                                    sig_hint_matches.append(name)
                        if len(sig_hint_matches) == 1:
                            target_name = sig_hint_matches[0]
        if not target_name:
            input_signature = _input_signature(entry, repo_roots)
            if input_signature:
                target_name = input_signature_map.get(input_signature)
                if not target_name:
                    candidates = input_signature_to_candidates.get(input_signature, [])
                    if candidates:
                        entry_keys = _entry_label_keys(entry)
                        matches = []
                        if entry_keys:
                            matches = [
                                name
                                for name, keys in candidates
                                if keys and keys.intersection(entry_keys)
                            ]
                            if len(matches) == 1:
                                target_name = matches[0]
                                matches = []
                        if not target_name and candidates:
                            target_name = _select_candidate_by_fail_hint(
                                entry_keys, candidates
                            )
                        if not target_name:
                            hint_matches = []
                            hints = _input_name_hints(entry, repo_roots)
                            candidates_to_check = (
                                [(name, keys) for name, keys in candidates if name in matches]
                                if matches
                                else candidates
                            )
                            if hints:
                                for name, _ in candidates_to_check:
                                    name_key = _normalize_name_key(name)
                                    if any(hint and hint in name_key for hint in hints):
                                        hint_matches.append(name)
                            if len(hint_matches) == 1:
                                target_name = hint_matches[0]
        if not target_name:
            input_only_signature = _input_only_signature(entry, repo_roots)
            if input_only_signature:
                target_name = input_only_map.get(input_only_signature)
                if not target_name:
                    candidates = input_only_to_candidates.get(input_only_signature, [])
                    if candidates:
                        entry_keys = _entry_label_keys(entry)
                        matches = []
                        if entry_keys:
                            matches = [
                                name
                                for name, keys in candidates
                                if keys and keys.intersection(entry_keys)
                            ]
                            if len(matches) == 1:
                                target_name = matches[0]
                                matches = []
                        if not target_name and candidates:
                            target_name = _select_candidate_by_fail_hint(
                                entry_keys, candidates
                            )
                        if not target_name:
                            hint_matches = []
                            hints = _input_name_hints(entry, repo_roots)
                            candidates_to_check = (
                                [(name, keys) for name, keys in candidates if name in matches]
                                if matches
                                else candidates
                            )
                            if hints:
                                for name, _ in candidates_to_check:
                                    name_key = _normalize_name_key(name)
                                    if any(hint and hint in name_key for hint in hints):
                                        hint_matches.append(name)
                            if len(hint_matches) == 1:
                                target_name = hint_matches[0]
        if target_name:
            if target_name != entry.name:
                rename_map[entry.name] = target_name
            entry.name = target_name
            entry.key = f"name:{target_name}"

    grouped: dict[str, list[TestEntry]] = {}
    for entry in entries:
        grouped.setdefault(entry.key, []).append(entry)

    merged: list[TestEntry] = []
    for key in sorted(grouped):
        group = grouped[key]
        min_priority = min(item.priority for item in group)
        candidates = [item for item in group if item.priority == min_priority]
        enabled = [item for item in candidates if item.status == "enabled"]
        primary_pool = enabled or candidates
        primary_pool.sort(
            key=lambda item: (
                0 if item.command else 1,
                -len(item.labels),
                -len(item.command),
                item.name,
            )
        )
        primary = primary_pool[0]
        for item in group:
            if item.name != primary.name:
                rename_map[item.name] = primary.name

        combined = TestEntry(
            key=primary.key,
            name=primary.name,
            command=list(primary.command),
            workdir=primary.workdir,
            env=dict(primary.env),
            labels=list(primary.labels),
            status=primary.status,
            disable_reason=primary.disable_reason,
            origin=[],
            needs_manual_followup=any(item.needs_manual_followup for item in group),
            priority=primary.priority,
            depends=list(primary.depends),
        )
        for item in group:
            combined.origin.extend(item.origin)
            if item.depends:
                combined.depends.extend(item.depends)
        if _is_make_target_entry(combined):
            preferred_depends: list[str] = []
            for item in sorted(group, key=lambda entry: entry.priority):
                if item.depends and _has_buildsys_origin(item, "cmake"):
                    preferred_depends = list(item.depends)
                    break
            if not preferred_depends:
                for item in sorted(group, key=lambda entry: entry.priority):
                    if item.depends:
                        preferred_depends = list(item.depends)
                        break
            if preferred_depends:
                combined.depends = preferred_depends
        if combined.depends:
            combined.depends = sorted(set(combined.depends))
        if combined.depends and not _is_make_target_entry(combined):
            combined.depends = []

        if combined.status == "enabled" and "disabled" in combined.labels:
            combined.labels = [label for label in combined.labels if label != "disabled"]
        if combined.status == "disabled" and "disabled" not in combined.labels:
            combined.labels.append("disabled")

        if not combined.command:
            combined.needs_manual_followup = True
            if _has_ctest_origin(combined):
                merged.append(combined)
                continue
            combined.command = ["cmake", "-E", "echo", "needs_manual_followup"]
            combined.status = "disabled"
            combined.disable_reason = combined.disable_reason or "missing command"
            if "disabled" not in combined.labels:
                combined.labels.append("disabled")
        merged.append(combined)

    name_set = {entry.name for entry in merged}

    def _resolve_renamed_name(name: str) -> str:
        if name in name_set:
            return name
        seen: set[str] = set()
        while name in rename_map and name not in seen:
            seen.add(name)
            name = rename_map[name]
        return name

    for entry in merged:
        if not entry.depends:
            continue
        updated: list[str] = []
        for dep in entry.depends:
            resolved = _resolve_renamed_name(dep)
            if resolved and resolved != entry.name:
                updated.append(resolved)
        entry.depends = sorted(set(updated))

    dir_to_tests: dict[str, set[str]] = {}
    for entry in merged:
        if _is_make_target_entry(entry):
            continue
        for origin in entry.origin:
            if origin.get("buildsys") == "ctest":
                continue
            path_str = origin.get("path")
            if not path_str:
                continue
            path = Path(path_str)
            dir_key = str(path.parent)
            dir_to_tests.setdefault(dir_key, set()).add(entry.name)

    for entry in merged:
        if not _is_make_target_entry(entry):
            continue
        if entry.depends:
            continue
        candidate_dirs: list[str] = []
        for origin in entry.origin:
            if origin.get("buildsys") not in {"autotools", "cmake"}:
                continue
            path_str = origin.get("path")
            if not path_str:
                continue
            candidate_dirs.append(str(Path(path_str).parent))
        fallback: set[str] = set()
        for dir_key in candidate_dirs:
            fallback.update(dir_to_tests.get(dir_key, set()))
        if entry.name in fallback:
            fallback.remove(entry.name)
        if fallback:
            entry.depends = sorted(fallback)

    return [entry.as_manifest() for entry in merged]


def _is_make_target_entry(entry: TestEntry) -> bool:
    for origin in entry.origin:
        if (
            origin.get("buildsys") == "autotools"
            and isinstance(origin.get("notes"), str)
            and origin["notes"].startswith("target:")
        ):
            return True
    return False


def _has_ctest_origin(entry: TestEntry) -> bool:
    for origin in entry.origin:
        if origin.get("notes") == "ctest -N -V":
            return True
    return False


def _has_buildsys_origin(entry: TestEntry, buildsys: str) -> bool:
    for origin in entry.origin:
        if origin.get("buildsys") == buildsys:
            return True
    return False


def _key_from_command(name: str, command: list[str], repo_root: Path) -> str:
    norm_name = name
    if name:
        norm_name = os.path.normpath(name)
        path = Path(norm_name)
        if path.is_absolute():
            try:
                norm_name = str(path.relative_to(repo_root))
            except ValueError:
                norm_name = name
    return f"name:{norm_name}"


def _strip_ctest_suffix(name: str) -> str:
    return re.sub(r"\s+\((Disabled|Not Run|Skipped)\)$", "", name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate unified test manifest JSON.")
    parser.add_argument("--rex", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--rex-build", type=Path, default=None)
    parser.add_argument("--rose-archive", type=Path, required=True)
    parser.add_argument("--rose-build", type=Path, default=None)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rex = args.rex.resolve()
    rose = args.rose_archive.resolve()
    rose_branch = _require_branch(rose, REQUIRED_ROSE_BRANCH, "rose-archive")

    entries: list[TestEntry] = []
    entries.extend(_extract_autotools_tests(rex, "rex"))
    entries.extend(_extract_cmake_tests_from_sources(rex, "rex", args.rex_build))
    if args.rex_build:
        entries.extend(
            _extract_cmake_tests_from_ctest("rex", args.rex_build.resolve(), rex)
        )

    entries.extend(_extract_autotools_tests(rose, "rose"))
    entries.extend(
        _extract_cmake_tests_from_sources(rose, "rose", args.rose_build)
    )
    if args.rose_build and args.rose_build.exists():
        entries.extend(
            _extract_cmake_tests_from_ctest(
                "rose", args.rose_build.resolve(), rose
            )
        )

    _annotate_origin_branch(entries, "rose", rose_branch)
    manifest = _merge_entries(entries, {"rex": rex, "rose": rose})
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
