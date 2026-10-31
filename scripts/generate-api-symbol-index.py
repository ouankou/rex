#!/usr/bin/env python3
import html
import json
import posixpath
import re
import sys
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit


_LINK_RE = re.compile(
    r"\[([^]\n]+)\]\(([^)\s]+)\)|<a\s+[^>]*href=[\"\']([^\"\']+)[\"\'][^>]*>(.*?)</a>",
    re.DOTALL,
)


def _normalize_label(text: str, *, html_markup: bool = False) -> str:
    if html_markup:
        text = re.sub(r"<[^>]+>", "", text)
    text = html.unescape(text)
    text = text.strip()
    if text.startswith("`") and text.endswith("`") and len(text) >= 2:
        text = text[1:-1].strip()
    text = text.replace("\u00ad", "")
    return text


def _to_url(target: str, section_slug: str = "") -> str:
    parts = urlsplit(html.unescape(target.strip()))
    if parts.scheme or parts.netloc:
        return ""
    # Markdown links resolve from the source file; raw HTML links have already
    # been prepared relative to MkDocs' sections/<name>/ output directory.
    markdown_target = parts.path.endswith(".md")
    base = "sections" if markdown_target else f"sections/{section_slug}"
    path = posixpath.normpath(posixpath.join(base, parts.path))
    if path == ".." or path.startswith("../") or path.startswith("/"):
        return ""
    if markdown_target:
        path = path[:-3]
    elif not parts.path.endswith("/"):
        return ""
    if path in ("index", "."):
        path = ""
    elif path.endswith("/index"):
        path = path[:-6]
    return urlunsplit(("", "", f"reference/{path}/" if path else "reference/", parts.query, parts.fragment))


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
    for section in sorted(sections_dir.glob("*.md")):
        kind = section.stem
        text = section.read_text(encoding="utf-8")
        for m in _LINK_RE.finditer(text):
            target = m.group(2) or m.group(3)
            label = _normalize_label(m.group(1) or m.group(4), html_markup=m.group(4) is not None)
            if not target or not label:
                continue
            url = _to_url(target, section.stem)
            if not url:
                continue
            key = (label, url)
            if key in entries:
                continue
            entries[key] = {"name": label, "url": url, "kind": kind}

    if not entries:
        raise SystemExit(f"no API symbol links found in Markdown sections: {sections_dir}")

    symbols = sorted(
        entries.values(),
        key=lambda x: (x["name"].casefold(), x["kind"], x["url"]),
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps({"symbols": symbols}, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
