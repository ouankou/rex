#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


_CODE_START = re.compile(r"^\[source,cpp")
_DELIM = "----"
_IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
_EXTERNAL = {"FILE", "JNIEnv"}
_FILTER = re.compile(r"^(Sg|Ast|Rose|Sage|OpenMP|OpenACC)[A-Za-z_]\w*$")


def _existing_pages(reference_dir: Path) -> set[str]:
    pages = set()
    for p in reference_dir.rglob("*.adoc"):
        rel = p.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue
        pages.add(p.stem)
    return pages


def _collect_missing_return_types(reference_dir: Path, pages: set[str]) -> set[str]:
    missing: set[str] = set()
    for p in reference_dir.rglob("*.adoc"):
        rel = p.relative_to(reference_dir).as_posix()
        if rel == "index.adoc" or rel.startswith("sections/"):
            continue
        lines = p.read_text(encoding="utf-8").splitlines()
        i = 0
        while i < len(lines):
            if _CODE_START.match(lines[i]) and i + 1 < len(lines) and lines[i + 1].strip() == _DELIM:
                i += 2
                block: list[str] = []
                while i < len(lines) and lines[i].strip() != _DELIM:
                    block.append(lines[i])
                    i += 1

                for j, line in enumerate(block):
                    if "(" not in line:
                        continue
                    prev = None
                    for k in range(j - 1, -1, -1):
                        if block[k].strip():
                            prev = block[k].strip()
                            break
                    if not prev or "xref:" in prev:
                        break
                    m = _IDENT.search(prev)
                    if not m:
                        break
                    tok = m.group(0)
                    if tok in pages or tok in _EXTERNAL:
                        break
                    if len(tok) <= 2:
                        break
                    if _FILTER.match(tok):
                        missing.add(tok)
                    break
            i += 1
    return missing


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

    pages = _existing_pages(reference_dir)
    missing = sorted(_collect_missing_return_types(reference_dir, pages))

    created = 0
    for tok in missing:
        out = reference_dir / f"{tok}.adoc"
        if out.exists():
            continue
        out.write_text(
            f"[#{tok}]\n= {tok}\n:relfileprefix:\n:mrdocs:\n\n",
            encoding="utf-8",
        )
        created += 1

    print(f"generated reference stubs: {created}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

