#!/usr/bin/env python3
from __future__ import annotations

import re
from typing import Callable, Optional


def should_override_ctest_command(entry_cmd: list[str], data_cmd: list[str]) -> bool:
    if not data_cmd:
        return False
    return not entry_cmd or entry_cmd != data_cmd


def _identity(value: str) -> str:
    return value


def parse_ctest_output_records(
    output: str,
    tokenize: Callable[[str], list[str]],
    name_transform: Callable[[str], str] = _identity,
) -> list[dict[str, object]]:
    records: dict[int, dict[str, object]] = {}
    current_num: Optional[int] = None
    header_re = re.compile(r"^Test\s+#(\d+)\s*:\s*(.+)$")
    prefix_re = re.compile(r"^\s*(\d+):\s*(.+)$")
    for line in output.splitlines():
        raw = line.rstrip()
        num: Optional[int] = None
        content = raw
        prefix = prefix_re.match(raw)
        if prefix:
            num = int(prefix.group(1))
            content = prefix.group(2)
            current_num = num
        stripped = content.strip()

        header = header_re.match(stripped)
        if header:
            num = int(header.group(1))
            raw_name = header.group(2).strip()
            record = records.setdefault(num, {})
            if any(token in raw_name for token in ("(Disabled)", "(Not Run)", "(Skipped)")):
                record["disabled"] = True
            record["name"] = name_transform(raw_name)
            current_num = num
            continue

        if stripped.startswith(
            ("Test command:", "Working Directory:", "Labels:", "Disabled:")
        ):
            record_num = num if num is not None else current_num
            if record_num is None:
                continue
            record = records.setdefault(record_num, {})
            if stripped.startswith("Test command:"):
                cmd = stripped.split("Test command:", 1)[1].strip()
                record["command"] = tokenize(cmd)
            elif stripped.startswith("Working Directory:"):
                record["workdir"] = stripped.split("Working Directory:", 1)[1].strip()
            elif stripped.startswith("Labels:"):
                labels = stripped.split("Labels:", 1)[1].strip()
                record["labels"] = [
                    label for label in re.split(r"[;\s]+", labels) if label
                ]
            elif stripped.startswith("Disabled:"):
                value = stripped.split("Disabled:", 1)[1].strip()
                record["disabled"] = value.lower() == "true"
            continue

    ordered: list[dict[str, object]] = []
    for record in (records[key] for key in sorted(records)):
        if record.get("name"):
            ordered.append(record)
    return ordered
