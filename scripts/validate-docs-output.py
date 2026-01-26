#!/usr/bin/env python3
import argparse
import html
import re
import sys
from pathlib import Path


_DECLARED_RE = re.compile(r"Declared in `&lt;([^`]+)&gt;`")
_DASH_TRANSLATION = str.maketrans({
    "\u2010": "-",
    "\u2011": "-",
    "\u2012": "-",
    "\u2013": "-",
    "\u2212": "-",
})


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=None)
    ap.add_argument("--reference-dir", default=None)
    args = ap.parse_args()

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    reference_dir = (
        Path(args.reference_dir).resolve()
        if args.reference_dir
        else root / "docs" / "reference"
    )

    if not reference_dir.is_dir():
        print(f"reference dir not found: {reference_dir}", file=sys.stderr)
        return 2

    forbidden_prefixes = (
        "src/ROSETTA/",
        "src/frontend/SageIII/ompparser/",
        "src/frontend/SageIII/accparser/",
    )
    forbidden_build_prefixes = (
        "build/src/ROSETTA/",
        "build/src/frontend/SageIII/ompparser/",
        "build/src/frontend/SageIII/accparser/",
    )

    bad = []
    missing = []
    checked = 0

    for path in sorted(reference_dir.rglob("*.adoc")):
        text = path.read_text(encoding="utf-8")
        m = _DECLARED_RE.search(text)
        if not m:
            continue
        declared = html.unescape(m.group(1)).strip().replace("\\", "/")
        declared = declared.translate(_DASH_TRANSLATION)
        checked += 1

        candidates = []
        if declared.startswith("src/"):
            src_candidate = (root / declared).resolve()
            if src_candidate.exists():
                candidates = [src_candidate]
            else:
                build_candidate = (root / "build" / declared).resolve()
                if build_candidate.exists():
                    candidates = [build_candidate]
        else:
            candidates = [p.resolve() for p in (root / "src").rglob(declared)]
            if not candidates:
                candidates = [p.resolve() for p in (root / "build" / "src").rglob(declared)]

        if not candidates:
            missing.append((path, declared))
            continue
        if len(candidates) > 1:
            bad.append((path, f"{declared} (ambiguous: {len(candidates)} matches)"))
            continue

        abs_path = candidates[0]
        try:
            rel = abs_path.relative_to(root).as_posix()
        except ValueError:
            bad.append((path, declared))
            continue

        if any(rel.startswith(p) for p in forbidden_prefixes) or any(
            rel.startswith(p) for p in forbidden_build_prefixes
        ):
            bad.append((path, f"{declared} -> {rel}"))
            continue
        if not (rel.startswith("src/") or rel.startswith("build/src/")):
            bad.append((path, f"{declared} -> {rel}"))
            continue
        if not abs_path.exists():
            missing.append((path, f"{declared} -> {rel}"))

    if bad:
        print("invalid declared-in paths:", file=sys.stderr)
        for doc, declared in bad[:200]:
            print(f"- {doc.relative_to(root)} -> {declared}", file=sys.stderr)
        if len(bad) > 200:
            print(f"... and {len(bad) - 200} more", file=sys.stderr)
        return 1

    if missing:
        print("declared-in paths not found on disk:", file=sys.stderr)
        for doc, declared in missing[:200]:
            print(f"- {doc.relative_to(root)} -> {declared}", file=sys.stderr)
        if len(missing) > 200:
            print(f"... and {len(missing) - 200} more", file=sys.stderr)
        return 1

    print(f"validated declared-in paths: {checked}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
