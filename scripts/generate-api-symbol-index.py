#!/usr/bin/env python3
import html
import json
import re
import sys
from pathlib import Path


_XREF_RE = re.compile(r"xref:([^\[]+)\[([^\]]*)\]")


def _normalize_label(text: str) -> str:
    text = html.unescape(text)
    text = text.strip()
    if text.startswith("`") and text.endswith("`") and len(text) >= 2:
        text = text[1:-1].strip()
    text = text.replace("\u00ad", "")
    return text


def _to_url(target: str) -> str:
    target = target.strip()
    if target.endswith(".adoc"):
        target = target[: -len(".adoc")]
    target = target.strip("/")
    if not target:
        return "reference/"
    return f"reference/{target}/"


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    sections_dir = root / "docs" / "reference" / "sections"
    out_path = root / "_site" / "assets" / "api" / "symbols.json"

    if len(sys.argv) >= 2:
        sections_dir = Path(sys.argv[1]).resolve()
    if len(sys.argv) >= 3:
        out_path = Path(sys.argv[2]).resolve()

    if not sections_dir.is_dir():
        raise SystemExit(f"sections directory not found: {sections_dir}")

    entries: dict[tuple[str, str], dict] = {}
    for section in sorted(sections_dir.glob("*.adoc")):
        kind = section.stem
        text = section.read_text(encoding="utf-8")
        for m in _XREF_RE.finditer(text):
            target = m.group(1).strip()
            label = _normalize_label(m.group(2))
            if not target or not label:
                continue
            url = _to_url(target)
            key = (label, url)
            if key in entries:
                continue
            entries[key] = {"name": label, "url": url, "kind": kind}

    symbols = sorted(
        entries.values(),
        key=lambda x: (x["name"].casefold(), x["kind"], x["url"]),
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps({"symbols": symbols}, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
