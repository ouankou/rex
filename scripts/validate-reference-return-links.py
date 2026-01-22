#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


_CODE_DELIM = "----"
_IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
_REAL_AMP = re.compile(r"(?<!&)&(?![A-Za-z]+;)")
_SKIP = {
    "void",
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "signed",
    "unsigned",
    "const",
    "volatile",
    "static",
    "inline",
    "virtual",
    "explicit",
    "constexpr",
    "mutable",
    "typename",
    "class",
    "struct",
    "enum",
    "template",
    "friend",
    "auto",
    "decltype",
}


def _build_symbol_map(reference_dir: Path) -> dict[str, str]:
    candidates: dict[str, list[str]] = {}
    for path in reference_dir.rglob("*.adoc"):
        rel = path.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue
        candidates.setdefault(path.stem, []).append(rel)

    mapping: dict[str, str] = {}
    for stem, rels in candidates.items():
        if f"{stem}.adoc" in rels:
            mapping[stem] = f"{stem}.adoc"
        elif len(rels) == 1:
            mapping[stem] = rels[0]
    return mapping


def _line_has_unlinked_return_type(line: str, mapping: dict[str, str]) -> str | None:
    if "xref:" in line:
        return None
    if "*" not in line and not _REAL_AMP.search(line):
        return None
    for m in _IDENT.finditer(line):
        tok = m.group(0)
        if tok in _SKIP:
            continue
        if (tok[:1].isupper() or tok.startswith("Sg")) and tok in mapping:
            return tok
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference-dir", default=None)
    ap.add_argument("--limit", type=int, default=200)
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

    mapping = _build_symbol_map(reference_dir)

    bad: list[tuple[str, str, str]] = []
    checked = 0

    for path in sorted(reference_dir.rglob("*.adoc")):
        rel = path.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue
        base = path.name
        if base.startswith("2constructor") or base.startswith("2destructor"):
            continue

        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        i = 0
        while i < len(lines):
            line = lines[i]
            if line.startswith("[source,cpp") and i + 1 < len(lines) and lines[i + 1].strip() == _CODE_DELIM:
                i += 2
                block: list[str] = []
                while i < len(lines) and lines[i].strip() != _CODE_DELIM:
                    block.append(lines[i])
                    i += 1

                for j, bline in enumerate(block):
                    if "(" not in bline:
                        continue
                    checked += 1
                    prev = None
                    for k in range(j - 1, -1, -1):
                        if block[k].strip():
                            prev = block[k].strip()
                            break
                    if prev:
                        tok = _line_has_unlinked_return_type(prev, mapping)
                        if tok:
                            bad.append((rel, tok, prev))
                    else:
                        left = bline.split("(", 1)[0]
                        tok = _line_has_unlinked_return_type(left.strip(), mapping)
                        if tok:
                            bad.append((rel, tok, left.strip()))
            i += 1

    if bad:
        print(f"unlinked return types found: {len(bad)}", file=sys.stderr)
        for rel, tok, ctx in bad[: args.limit]:
            print(f"- {rel}: {tok} in '{ctx}'", file=sys.stderr)
        if len(bad) > args.limit:
            print(f"... and {len(bad) - args.limit} more", file=sys.stderr)
        return 1

    print(f"validated return type linking: {checked} signatures checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
