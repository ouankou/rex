#!/usr/bin/env python3
import argparse
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit
import posixpath


class _LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for k, v in attrs:
            if k in {"href", "src"} and v:
                self.links.append(v)


def _page_url_dir(site_root: Path, html_file: Path) -> str:
    rel = html_file.relative_to(site_root).as_posix()
    if rel == "index.html":
        return "/"
    if rel.endswith("/index.html"):
        return "/" + rel[: -len("index.html")]
    parent = posixpath.dirname(rel)
    return "/" + (parent + "/" if parent else "")


def _is_external(url: str) -> bool:
    if url.startswith(("//", "http:", "https:", "mailto:", "tel:", "data:", "javascript:")):
        return True
    return False


def _normalize_path(path: str) -> str:
    if not path.startswith("/"):
        path = "/" + path
    norm = posixpath.normpath(path)
    if path.endswith("/") and not norm.endswith("/"):
        norm += "/"
    if norm == "/.":
        norm = "/"
    return norm


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--site-dir", default=None)
    ap.add_argument("--root", default=None)
    ap.add_argument("--limit", type=int, default=200)
    args = ap.parse_args()

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    site_dir = Path(args.site_dir).resolve() if args.site_dir else root / "_site"
    if not site_dir.is_dir():
        print(f"site dir not found: {site_dir}", file=sys.stderr)
        return 2

    existing: set[str] = set()
    for p in site_dir.rglob("*"):
        if not p.is_file():
            continue
        rel = "/" + p.relative_to(site_dir).as_posix()
        existing.add(rel)
        if rel.endswith("/index.html"):
            existing.add(rel[: -len("index.html")])
        if rel == "/index.html":
            existing.add("/")

    bad: list[tuple[str, str, str]] = []
    checked = 0

    for html in sorted(site_dir.rglob("*.html")):
        text = html.read_text(encoding="utf-8")
        parser = _LinkParser()
        parser.feed(text)
        base_dir = _page_url_dir(site_dir, html)

        for raw in parser.links:
            if not raw or raw.startswith("#") or _is_external(raw):
                continue

            parsed = urlsplit(raw)
            if not parsed.path:
                continue

            path = parsed.path
            if path.startswith("/"):
                resolved = _normalize_path(path)
            else:
                resolved = _normalize_path(posixpath.join(base_dir, path))

            checked += 1
            candidates = [resolved]
            if not resolved.endswith("/"):
                candidates.append(resolved + "/")

            if any(c in existing for c in candidates):
                continue

            bad.append((html.relative_to(site_dir).as_posix(), raw, resolved))

    if bad:
        print(f"dead links found: {len(bad)} (checked {checked})", file=sys.stderr)
        for page, raw, resolved in bad[: args.limit]:
            print(f"- {page}: {raw} -> {resolved}", file=sys.stderr)
        if len(bad) > args.limit:
            print(f"... and {len(bad) - args.limit} more", file=sys.stderr)
        return 1

    print(f"validated links: {checked}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

