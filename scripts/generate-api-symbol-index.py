#!/usr/bin/env python3
"""Index every prepared MrDocs symbol, including members and overloads."""
import json
import re
import sys
from pathlib import Path


def main():
    root = Path(__file__).resolve().parent.parent
    reference = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "docs/reference"
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "_site/assets/api/symbols.json"
    symbols = []
    for page in sorted(reference.rglob("*.md")):
        text = page.read_text(encoding="utf-8")
        kind = re.search(r'data-mrdocs-kind="([^"]+)"', text)
        if not kind:
            continue  # Authored category pages have no symbol identity.
        title = re.search(r'^title: (.+)$', text, re.M)
        if not title:
            raise ValueError(f"Missing prepared symbol title: {page}")
        name = json.loads(title[1]).replace("\u00ad", "")
        if not name or page == reference / "index.md":
            continue
        path = page.relative_to(reference).with_suffix("").as_posix()
        symbols.append({"name": name, "url": f"reference/{path}/", "kind": kind[1]})
    if not symbols:
        raise SystemExit(f"No prepared API symbols found: {reference}")
    symbols.sort(key=lambda symbol: (symbol["name"].casefold(), symbol["kind"], symbol["url"]))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"symbols": symbols}, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")


if __name__ == "__main__":
    main()
