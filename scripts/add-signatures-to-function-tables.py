#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


_SECTION_FUNCTIONS = re.compile(r"^==\s+Functions\s*$")
_TABLE_START = re.compile(r"^\|===\s*$")
_TABLE_END = re.compile(r"^\|===\s*$")
_XREF_TARGET = re.compile(r"\bxref:([^\[]+)\[")
_XREF_INLINE = re.compile(r"\bxref:[^\[]+\[([^\]]*)\]")


def _extract_first_signature(target_adoc: Path) -> str | None:
    try:
        lines = target_adoc.read_text(encoding="utf-8").splitlines()
    except OSError:
        return None

    i = 0
    while i < len(lines):
        if lines[i].startswith("[source,cpp") and i + 1 < len(lines) and lines[i + 1].strip() == "----":
            i += 2
            block: list[str] = []
            while i < len(lines) and lines[i].strip() != "----":
                block.append(lines[i].rstrip())
                i += 1

            cleaned: list[str] = []
            for ln in block:
                s = ln.strip()
                if not s:
                    continue
                if s in ("&lsqb;&lsqb;visibility&rsqb;&rsqb;", "[[visibility]]"):
                    continue
                cleaned.append(s)

            if not cleaned:
                return None

            sig = " ".join(cleaned)
            sig = re.sub(r"\s+", " ", sig).strip()
            sig = sig.replace(" (", "(")
            sig = re.sub(r"\s*,\s*", ", ", sig)
            sig = re.sub(r"\(\s+", "(", sig)
            sig = re.sub(r"\s+\)", ")", sig)
            sig = _XREF_INLINE.sub(lambda m: m.group(1), sig)
            return sig
        i += 1
    return None


def _process_file(path: Path, reference_dir: Path) -> int:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    out: list[str] = []
    changed = 0
    i = 0

    while i < len(lines):
        line = lines[i]
        out.append(line)

        if not _SECTION_FUNCTIONS.match(line.strip()):
            i += 1
            continue

        i += 1
        while i < len(lines) and not _TABLE_START.match(lines[i].strip()):
            out.append(lines[i])
            i += 1
        if i >= len(lines):
            break

        out.append(lines[i])
        i += 1

        while i < len(lines):
            cur = lines[i]
            if _TABLE_END.match(cur.strip()):
                out.append(cur)
                i += 1
                break

            m = _XREF_TARGET.search(cur)
            if not m or cur.lstrip().startswith("| Name"):
                out.append(cur)
                i += 1
                continue

            if cur.rstrip().endswith(" +"):
                out.append(cur)
                i += 1
                continue

            if i + 1 < len(lines) and lines[i + 1].lstrip().startswith("[.ref-signature]#"):
                out.append(cur)
                i += 1
                continue

            target = m.group(1)
            if target.startswith("http:") or target.startswith("https:"):
                out.append(cur)
                i += 1
                continue

            target_path = reference_dir / target
            if not target_path.exists():
                out.append(cur)
                i += 1
                continue

            sig = _extract_first_signature(target_path)
            if not sig:
                out.append(cur)
                i += 1
                continue

            updated = cur.rstrip("\n")
            if not updated.endswith(" +"):
                updated += " +\n"
            out.append(updated)
            out.append(f"[.ref-signature]#`{sig}`#\n")
            changed += 1
            i += 1
            continue

        continue

    if changed:
        path.write_text("".join(out), encoding="utf-8")
    return changed


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference-dir", default=None)
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    reference_dir = (
        Path(args.reference_dir).resolve()
        if args.reference_dir
        else root / "docs" / "reference"
    )
    if not reference_dir.is_dir():
        print(f"reference dir not found: {reference_dir}", file=sys.stderr)
        return 2

    total = 0
    for path in sorted(reference_dir.glob("*.adoc")):
        if path.name in ("index.adoc",):
            continue
        total += _process_file(path, reference_dir)

    print(f"added signatures to {total} function entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
