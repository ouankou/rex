#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
import shlex
import subprocess
from pathlib import Path
from typing import Any


def read_failure_names(path: Path) -> set[str]:
    names: set[str] = set()
    if not path.exists():
        return names
    for raw in path.read_text(errors="ignore").splitlines():
        line = raw.strip()
        if not line:
            continue
        if ":" in line:
            line = line.split(":", 1)[1]
        names.add(line.strip())
    return names


def load_ctest_metadata(build_dir: Path, metadata_json: Path | None) -> dict[str, dict[str, Any]]:
    if metadata_json is not None:
        data = json.loads(metadata_json.read_text(errors="ignore"))
    else:
        raw = subprocess.check_output(
            ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
            text=True,
        )
        data = json.loads(raw)

    tests: dict[str, dict[str, Any]] = {}
    for test in data.get("tests", []):
        name = test.get("name")
        if not name:
            continue
        props: dict[str, Any] = {}
        for prop in test.get("properties", []):
            props[prop.get("name", "")] = prop.get("value")
        labels = props.get("LABELS", [])
        if isinstance(labels, str):
            labels = [item for item in re.split(r"[;\s]+", labels) if item]
        tests[name] = {
            "command": test.get("command", []),
            "labels": labels,
            "workdir": props.get("WORKING_DIRECTORY", ""),
            "environment": props.get("ENVIRONMENT", []),
        }
    return tests


def parse_summary_statuses(path: Path | None) -> dict[str, dict[str, str]]:
    if path is None or not path.exists():
        return {}
    statuses: dict[str, dict[str, str]] = {}
    final_re = re.compile(r"^\s*(\d+)\s+-\s+(.+?)\s+\(([^)]+)\)")
    progress_re = re.compile(r"^\s*\d+/\d+\s+Test\s+#\s*(\d+):\s+(.+?)\s+\.{3,}\s*(.+?)\s+(?:\d|\*)")
    for raw in path.read_text(errors="ignore").splitlines():
        line = raw.rstrip()
        match = final_re.match(line)
        if match:
            number, name, status = match.groups()
            statuses[name] = {"number": number, "status": status}
            continue
        match = progress_re.match(line)
        if match:
            number, name, status = match.groups()
            if "***Timeout" in status:
                status = "Timeout"
            elif "Subprocess aborted" in status:
                status = "Subprocess aborted"
            elif "***Failed" in status:
                status = "Failed"
            elif "Not Run" in status:
                status = "Not Run"
            statuses.setdefault(name.strip(), {"number": number, "status": status.strip()})
    return statuses


def parse_failure_blocks(path: Path | None) -> dict[str, str]:
    if path is None or not path.exists():
        return {}
    blocks: dict[str, list[str]] = {}
    current: str | None = None
    header_re = re.compile(r"^\s*\d+/\d+\s+Test\s+#\s*\d+:\s+(.+?)\s+\.{3,}")
    start_re = re.compile(r"^\s*Start\s+\d+:")
    for raw in path.read_text(errors="ignore").splitlines():
        line = raw.rstrip()
        match = header_re.match(line)
        if match:
            current = match.group(1).strip()
            blocks.setdefault(current, []).append(line)
            continue
        if start_re.match(line):
            current = None
            continue
        if current is not None:
            blocks[current].append(line)
    return {name: "\n".join(lines) for name, lines in blocks.items()}


def source_from_command(command: list[str]) -> str:
    for arg in reversed(command):
        if arg.startswith("-"):
            continue
        suffix = Path(arg).suffix.lower()
        if suffix in {".c", ".cc", ".cpp", ".cxx", ".c++", ".h", ".hpp", ".f", ".f90", ".f95", ".f03", ".f08"}:
            return arg
    return ""


def generated_file_from_block(block: str) -> str:
    patterns = [
        r"(/[^\s:]+/rose_output/[^\s:]+)",
        r"(/[^\s:]+/test-output/[^\s:]+)",
        r"\b(rose_[A-Za-z0-9_.+-]+)\b",
    ]
    for pattern in patterns:
        match = re.search(pattern, block)
        if match:
            return match.group(1)
    return ""


def failure_signature(block: str, status: str) -> str:
    if status == "Timeout":
        return "Timeout"
    interesting = (
        "FAIL : ASSERTION",
        "Runtime Error:",
        "fatal error:",
        " error:",
        "undefined reference",
        "Clang found ",
        "command exited with value",
        "***Exception:",
    )
    for raw in block.splitlines():
        line = raw.strip()
        if any(token in line for token in interesting):
            return line[:240]
    for raw in block.splitlines():
        line = raw.strip()
        if line:
            return line[:240]
    return status or "unknown"


def owning_bucket(name: str, labels: list[str], signature: str) -> str:
    text = " ".join([name, " ".join(labels), signature]).lower()
    if "timeout" in text:
        return "timeout"
    if any(token in text for token in ("unknown register name", "invalid output constraint", "invalid input constraint", "regparm", "default-calling-conv", "inline asm", "unrecognized instruction mnemonic")):
        return "cfe-target-options"
    if "tokenstream" in text or "unparse_tokens" in text or "token_" in name.lower():
        return "token-source-mapping"
    if "sourceposition" in text or "filelocation" in text:
        return "source-position"
    if any(token in text for token in ("movedecl", "outline_", "callgraph", "dataflow", "staticcfg", "virtualcfg", "normalizationtranslator")):
        return "midend-transform-analysis"
    if any(token in text for token in ("collectassociateddeclarationlistitems", "rose_output", "expected '}'", "incomplete type", "undeclared identifier", "unknown type name")):
        return "cxx-unparser-decl-order"
    if any(token in text for token in ("set_parent", "get_scope", "symbol", "firstnondefining", "nondefining", "defining declaration")):
        return "cfe-decl-scope-symbol"
    if any(label.startswith("OMP") or label.startswith("GFORTRAN") or label.startswith("FORTRAN") for label in labels):
        return "fortran-openmp-core"
    return "uncategorized"


def shell_join(command: list[str]) -> str:
    return " ".join(shlex.quote(item) for item in command)


def build_entries(
    frozen_names: set[str],
    current_names: set[str],
    metadata: dict[str, dict[str, Any]],
    statuses: dict[str, dict[str, str]],
    blocks: dict[str, str],
) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    for name in sorted(frozen_names):
        meta = metadata.get(name, {})
        labels = list(meta.get("labels") or [])
        command = list(meta.get("command") or [])
        status_info = statuses.get(name, {})
        status = status_info.get("status", "unresolved" if name in current_names else "resolved")
        block = blocks.get(name, "")
        signature = failure_signature(block, status)
        entries.append(
            {
                "name": name,
                "number": status_info.get("number", ""),
                "status": status,
                "labels": labels,
                "source_file": source_from_command(command),
                "generated_file": generated_file_from_block(block),
                "workdir": meta.get("workdir", ""),
                "command": shell_join(command),
                "signature": signature,
                "bucket": owning_bucket(name, labels, signature),
                "in_current_failure_file": name in current_names,
            }
        )
    return entries


def write_csv(path: Path, entries: list[dict[str, Any]]) -> None:
    fieldnames = [
        "name",
        "number",
        "status",
        "bucket",
        "labels",
        "source_file",
        "generated_file",
        "signature",
        "workdir",
        "command",
        "in_current_failure_file",
    ]
    with path.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        for entry in entries:
            row = dict(entry)
            row["labels"] = ";".join(entry.get("labels", []))
            writer.writerow(row)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a ledger for the frozen REX CTest failure set.")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--frozen-file", type=Path, default=Path("build/Testing/Temporary/frozen-full-failure-set.txt"))
    parser.add_argument("--current-failure-file", type=Path, default=Path("build/Testing/Temporary/LastTestsFailed.log"))
    parser.add_argument("--ctest-output", type=Path, default=None)
    parser.add_argument("--metadata-json", type=Path, default=None)
    parser.add_argument("--json-out", type=Path, default=None)
    parser.add_argument("--csv-out", type=Path, default=None)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    frozen_names = read_failure_names(args.frozen_file)
    current_names = read_failure_names(args.current_failure_file)
    metadata = load_ctest_metadata(build_dir, args.metadata_json)
    statuses = parse_summary_statuses(args.ctest_output)
    blocks = parse_failure_blocks(args.ctest_output)
    entries = build_entries(frozen_names, current_names, metadata, statuses, blocks)

    unresolved = sum(1 for entry in entries if entry["in_current_failure_file"])
    resolved = len(entries) - unresolved
    outside_frozen = sorted(current_names - frozen_names)
    buckets: dict[str, int] = {}
    for entry in entries:
        if entry["in_current_failure_file"]:
            buckets[entry["bucket"]] = buckets.get(entry["bucket"], 0) + 1

    report = {
        "frozen_total": len(frozen_names),
        "current_total": len(current_names),
        "unresolved_frozen": unresolved,
        "resolved_frozen": resolved,
        "outside_frozen_total": len(outside_frozen),
        "outside_frozen": outside_frozen,
        "buckets": dict(sorted(buckets.items(), key=lambda item: (-item[1], item[0]))),
        "entries": entries,
    }

    if args.json_out is not None:
        args.json_out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.csv_out is not None:
        write_csv(args.csv_out, entries)

    print(f"Frozen failures: {len(frozen_names)}")
    print(f"Current failures: {len(current_names)}")
    print(f"Unresolved frozen: {unresolved}")
    print(f"Resolved frozen: {resolved}")
    print(f"Outside frozen: {len(outside_frozen)}")
    print("Buckets:")
    for bucket, count in report["buckets"].items():
        print(f"  {bucket}: {count}")
    return 1 if outside_frozen else 0


if __name__ == "__main__":
    raise SystemExit(main())
