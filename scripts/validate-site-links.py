#!/usr/bin/env python3
import argparse
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import posixpath


class _LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.links: list[str] = []
        self.anchors: set[str] = set()

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for k, v in attrs:
            if v is not None and (k == "id" or (tag == "a" and k == "name")):
                self.anchors.add(v)
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
    parsed = urlsplit(url)
    return bool(parsed.scheme or parsed.netloc)


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
    ap.add_argument("--base-path", default="/", help="URL path where this site is mounted")
    args = ap.parse_args()

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    site_dir = Path(args.site_dir).resolve() if args.site_dir else root / "_site"
    if not site_dir.is_dir():
        print(f"site dir not found: {site_dir}", file=sys.stderr)
        return 2

    base_path = "/" + args.base_path.strip("/")
    base_path = base_path.rstrip("/")
    site_bytes = 0
    existing: dict[str, Path] = {}
    documents: dict[Path, _LinkParser] = {}
    for p in site_dir.rglob("*"):
        if not p.is_file():
            continue
        site_bytes += p.stat().st_size
        if site_bytes > 1_000_000_000:
            print(f"site exceeds GitHub Pages 1 GB limit: {site_bytes} bytes", file=sys.stderr)
            return 1
        rel = base_path + "/" + p.relative_to(site_dir).as_posix()
        existing[rel] = p
        if rel.endswith("/index.html"):
            existing[rel[: -len("index.html")]] = p
        if rel == base_path + "/index.html":
            existing[base_path + "/"] = p

        if p.suffix == ".html":
            parser = _LinkParser()
            parser.feed(p.read_text(encoding="utf-8"))
            documents[p] = parser

    if not documents:
        print("no HTML pages found in site", file=sys.stderr)
        return 1

    bad: list[tuple[str, str, str]] = []
    checked = 0

    for html, parser in sorted(documents.items()):
        base_dir = base_path + _page_url_dir(site_dir, html)

        for raw in parser.links:
            if not raw or _is_external(raw):
                continue

            parsed = urlsplit(raw)
            path = unquote(parsed.path)
            if not path:
                resolved = base_path + "/" + html.relative_to(site_dir).as_posix()
            elif path.startswith("/"):
                resolved = _normalize_path(path)
            else:
                resolved = _normalize_path(posixpath.join(base_dir, path))

            checked += 1
            candidates = [resolved]
            if not resolved.endswith("/"):
                candidates.append(resolved + "/")

            target = next((existing[c] for c in candidates if c in existing), None)
            if target is None:
                bad.append((html.relative_to(site_dir).as_posix(), raw, resolved))
                continue
            fragment = unquote(parsed.fragment)
            if fragment and target in documents and fragment not in documents[target].anchors:
                bad.append((html.relative_to(site_dir).as_posix(), raw, f"{resolved}#{fragment} (missing anchor)"))

    if bad:
        print(f"dead links found: {len(bad)} (checked {checked})", file=sys.stderr)
        for page, raw, resolved in bad[: args.limit]:
            print(f"- {page}: {raw} -> {resolved}", file=sys.stderr)
        if len(bad) > args.limit:
            print(f"... and {len(bad) - args.limit} more", file=sys.stderr)
        return 1

    print(f"validated links: {checked}; site payload: {site_bytes} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

