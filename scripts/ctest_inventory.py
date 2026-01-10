#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
from pathlib import Path
from typing import Optional

from ctest_parse_utils import parse_ctest_output_records, should_override_ctest_command


def _tokenize(value: str) -> list[str]:
    try:
        return shlex.split(value, comments=False, posix=True)
    except ValueError:
        return value.split()


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

def _parse_bracket_quote(text: str, start: int) -> Optional[tuple[str, int]]:
    if start >= len(text) or text[start] != "[":
        return None
    i = start + 1
    while i < len(text) and text[i] == "=":
        i += 1
    if i >= len(text) or text[i] != "[":
        return None
    eq_count = i - start - 1
    closing = "]" + ("=" * eq_count) + "]"
    end = text.find(closing, i + 1)
    if end == -1:
        return None
    content = text[i + 1 : end]
    return content, end + len(closing)


def _tokenize_cmake_args(value: str) -> list[str]:
    tokens: list[str] = []
    current: list[str] = []
    in_quote = False
    i = 0
    while i < len(value):
        ch = value[i]
        if in_quote:
            if ch == '"' and (i == 0 or value[i - 1] != "\\"):
                in_quote = False
            else:
                current.append(ch)
            i += 1
            continue
        parsed = _parse_bracket_quote(value, i)
        if parsed:
            content, next_i = parsed
            if current:
                tokens.append("".join(current))
                current = []
            tokens.append(content)
            i = next_i
            continue
        if ch.isspace():
            if current:
                tokens.append("".join(current))
                current = []
            i += 1
            continue
        if ch == '"':
            in_quote = True
            i += 1
            continue
        current.append(ch)
        i += 1
    if current:
        tokens.append("".join(current))
    return tokens


def _parse_cmake_commands(text: str) -> list[tuple[str, list[str]]]:
    commands: list[tuple[str, list[str]]] = []
    pattern = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", re.MULTILINE)
    idx = 0
    while True:
        match = pattern.search(text, idx)
        if not match:
            break
        name = match.group(1).lower()
        args_start = match.end()
        depth = 1
        i = args_start
        in_quote = False
        while i < len(text) and depth > 0:
            ch = text[i]
            if ch == '"' and (i == 0 or text[i - 1] != "\\"):
                in_quote = not in_quote
            if not in_quote:
                parsed = _parse_bracket_quote(text, i)
                if parsed:
                    _, next_i = parsed
                    i = next_i
                    continue
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
        commands.append((name, args))
        idx = i
    return commands


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
        if prop == "LABELS" and _is_prop_key(prop_token):
            values: list[str] = []
            while idx < len(tokens) and not _is_prop_key(tokens[idx]):
                for item in tokens[idx].split(";"):
                    if item:
                        values.append(item)
                idx += 1
            props[prop] = values
            continue
        if prop == "DEPENDS" and _is_prop_key(prop_token):
            values = []
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


def _is_truthy(value: str) -> bool:
    return value.strip().lower() in {"1", "on", "true", "yes"}


def _parse_ctest_testfiles(build_dir: Path) -> dict[str, dict[str, object]]:
    tests: dict[str, dict[str, object]] = {}
    pending: dict[str, dict[str, list[str]]] = {}
    for testfile in build_dir.rglob("CTestTestfile.cmake"):
        text = _strip_cmake_comments(testfile.read_text(errors="ignore"))
        commands = _parse_cmake_commands(text)
        for name, args in commands:
            if name == "add_test":
                test_name, command, workdir = _parse_add_test_tokens(args)
                if not test_name:
                    continue
                tests.setdefault(test_name, {})
                tests[test_name]["command"] = command
                if workdir:
                    tests[test_name]["workdir"] = workdir
                if test_name in pending:
                    props = pending.pop(test_name)
                    tests[test_name].setdefault("labels", [])
                    for item in props.get("LABELS", []):
                        tests[test_name]["labels"].append(item)
                    if props.get("DEPENDS"):
                        tests[test_name].setdefault("depends", [])
                        tests[test_name]["depends"].extend(props.get("DEPENDS", []))
                    if props.get("DISABLED") and _is_truthy(props["DISABLED"][0]):
                        tests[test_name]["disabled"] = True
            elif name == "set_tests_properties":
                if "PROPERTIES" not in [arg.upper() for arg in args]:
                    continue
                idx = next(
                    (i for i, arg in enumerate(args) if arg.upper() == "PROPERTIES"),
                    None,
                )
                if idx is None:
                    continue
                test_names = args[:idx]
                props = _parse_properties(args[idx + 1 :])
                for test_name in test_names:
                    if test_name in tests:
                        tests[test_name].setdefault("labels", [])
                        for item in props.get("LABELS", []):
                            tests[test_name]["labels"].append(item)
                        if props.get("DEPENDS"):
                            tests[test_name].setdefault("depends", [])
                            tests[test_name]["depends"].extend(props.get("DEPENDS", []))
                        if props.get("DISABLED") and _is_truthy(props["DISABLED"][0]):
                            tests[test_name]["disabled"] = True
                    else:
                        pending[test_name] = props
    return tests


def _key_from_command(name: str, command: list[str], repo_root: Path) -> str:
    return f"name:{name}"


def _parse_ctest_output(output: str, repo_root: Path, build_dir: Path) -> list[dict]:
    entries: list[dict] = []
    records = parse_ctest_output_records(output, _tokenize, _strip_ctest_suffix)
    for record in records:
        entries.append(_finalize_entry(record, repo_root, build_dir))
    return entries


def _apply_ctest_testfile_data(entries: list[dict], testfile_data: dict[str, dict[str, object]]) -> None:
    for entry in entries:
        data = testfile_data.get(entry.get("name", ""))
        if not data:
            continue
        if data.get("command"):
            current_cmd = entry.get("command", [])
            data_cmd = data.get("command", [])
            if should_override_ctest_command(current_cmd, data_cmd):
                entry["command"] = data_cmd
                labels = set(entry.get("labels", []))
                labels.discard("needs_manual_followup")
                entry["labels"] = sorted(labels)
        if data.get("workdir") and "workdir" not in entry:
            entry["workdir"] = data.get("workdir")
        if data.get("labels"):
            entry["labels"] = sorted(set(entry.get("labels", []) + data.get("labels", [])))
        if data.get("depends"):
            entry["depends"] = sorted(
                set(entry.get("depends", []) + data.get("depends", []))
            )
        if data.get("disabled"):
            entry["status"] = "disabled"
            entry["disable_reason"] = entry.get("disable_reason") or "ctest:disabled"
            if "disabled" not in entry["labels"]:
                entry["labels"].append("disabled")


def _merge_inventory_entries(entries: list[dict]) -> list[dict]:
    merged: dict[str, dict] = {}
    for entry in entries:
        name = entry.get("name", "")
        if not name:
            continue
        existing = merged.get(name)
        if not existing:
            merged[name] = entry
            continue
        labels = set(existing.get("labels", [])) | set(entry.get("labels", []))
        if len(entry.get("command", [])) > len(existing.get("command", [])):
            existing["command"] = entry.get("command", [])
        if entry.get("workdir") and not existing.get("workdir"):
            existing["workdir"] = entry.get("workdir")
        if entry.get("env"):
            env = dict(existing.get("env", {}))
            env.update(entry.get("env", {}))
            existing["env"] = env
        if entry.get("depends"):
            depends = set(existing.get("depends", []))
            depends.update(entry.get("depends", []))
            if depends:
                existing["depends"] = sorted(depends)
        if existing.get("status") == "disabled" and entry.get("status") == "enabled":
            existing["status"] = "enabled"
            existing.pop("disable_reason", None)
        elif existing.get("status") == "disabled" and entry.get("status") == "disabled":
            existing.setdefault("disable_reason", entry.get("disable_reason"))
        if existing.get("status") == "enabled":
            labels.discard("disabled")
        else:
            labels.add("disabled")
        existing["labels"] = sorted(labels)
    return list(merged.values())


def _finalize_entry(data: dict, repo_root: Path, build_dir: Path) -> dict:
    name = data.get("name", "")
    command = list(data.get("command") or [])
    workdir = data.get("workdir")
    labels = list(data.get("labels") or [])
    disabled = bool(data.get("disabled"))
    status = "disabled" if disabled or "disabled" in labels else "enabled"
    disable_reason = "ctest:disabled" if status == "disabled" else None
    if status == "disabled" and "disabled" not in labels:
        labels.append("disabled")
    entry = {
        "id": _key_from_command(name, command, repo_root),
        "name": name,
        "command": command,
        "labels": sorted(set(labels)),
        "status": status,
        "origin": [
            {
                "repo": "rex",
                "buildsys": "ctest",
                "path": str(build_dir),
                "notes": "ctest -N -V",
            }
        ],
        "env": {},
    }
    if workdir:
        entry["workdir"] = workdir
    depends = sorted(set(data.get("depends", []) or []))
    if depends:
        entry["depends"] = depends
    if disable_reason:
        entry["disable_reason"] = disable_reason
    if not command:
        entry["labels"].append("needs_manual_followup")
        entry["labels"] = sorted(set(entry["labels"]))
    return entry


def _strip_ctest_suffix(name: str) -> str:
    return re.sub(r"\s+\((Disabled|Not Run|Skipped)\)$", "", name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit CTest inventory as JSON.")
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    repo_root = args.repo_root.resolve()
    output = subprocess.check_output(
        ["ctest", "--test-dir", str(build_dir), "-N", "-V"],
        text=True,
    )
    entries = _parse_ctest_output(output, repo_root, build_dir)
    testfile_data = _parse_ctest_testfiles(build_dir)
    _apply_ctest_testfile_data(entries, testfile_data)
    entries = _merge_inventory_entries(entries)
    args.output.write_text(json.dumps(entries, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
