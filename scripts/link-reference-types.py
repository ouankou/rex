#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


_XREF_RE = re.compile(r"xref:[^\[]+\[[^\]]*\]")
_CODE_BLOCK_DELIM = "----"

_TOKEN_RE = re.compile(r"\b[A-Za-z_]\w*\b")
_SKIP_TOKENS = {
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
    "using",
    "namespace",
    "operator",
    "return",
    "public",
    "private",
    "protected",
    "this",
    "auto",
    "decltype",
    "noexcept",
    "override",
    "final",
}


def _build_symbol_map(reference_dir: Path) -> dict[str, str]:
    candidates: dict[str, list[str]] = {}
    for path in reference_dir.rglob("*.adoc"):
        rel = path.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue
        stem = path.stem
        candidates.setdefault(stem, []).append(rel)

    mapping: dict[str, str] = {}
    for stem, rels in candidates.items():
        if not stem or stem in mapping:
            continue
        if f"{stem}.adoc" in rels:
            mapping[stem] = f"{stem}.adoc"
            continue
        if len(rels) == 1:
            mapping[stem] = rels[0]
            continue
    return mapping


def _link_token(token: str, mapping: dict[str, str]) -> str | None:
    if token in _SKIP_TOKENS:
        return None
    if not token[:1].isupper() and not token.startswith("Sg"):
        return None
    target = mapping.get(token)
    if not target:
        return None
    return f"xref:{target}[{token}]"


def _process_code_block(text: str, mapping: dict[str, str]) -> tuple[str, int]:
    xref_spans: list[tuple[int, int]] = []
    for m in _XREF_RE.finditer(text):
        xref_spans.append((m.start(), m.end()))

    def _in_xref(i: int) -> bool:
        for a, b in xref_spans:
            if a <= i < b:
                return True
        return False

    out = []
    last = 0
    rewrites = 0
    for m in _TOKEN_RE.finditer(text):
        if _in_xref(m.start()):
            continue
        token = m.group(0)
        repl = _link_token(token, mapping)
        if not repl:
            continue
        out.append(text[last : m.start()])
        out.append(repl)
        last = m.end()
        rewrites += 1

    if rewrites == 0:
        return text, 0
    out.append(text[last:])
    return "".join(out), rewrites


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

    mapping = _build_symbol_map(reference_dir)
    changed_files = 0
    total = 0

    for path in sorted(reference_dir.rglob("*.adoc")):
        rel = path.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue

        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines(keepends=True)
        out_lines: list[str] = []
        i = 0
        rewrites = 0

        while i < len(lines):
            line = lines[i]
            out_lines.append(line)

            if line.startswith("[source,cpp") and i + 1 < len(lines) and lines[i + 1].strip() == _CODE_BLOCK_DELIM:
                i += 1
                out_lines.append(lines[i])
                i += 1
                block_start = i
                while i < len(lines) and lines[i].strip() != _CODE_BLOCK_DELIM:
                    i += 1
                block_text = "".join(lines[block_start:i])
                new_block, r = _process_code_block(block_text, mapping)
                out_lines.append(new_block)
                rewrites += r
                if i < len(lines):
                    out_lines.append(lines[i])
            i += 1

        if rewrites > 0:
            path.write_text("".join(out_lines), encoding="utf-8")
            changed_files += 1
            total += rewrites

    print(f"linked types: {total} replacements across {changed_files} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
