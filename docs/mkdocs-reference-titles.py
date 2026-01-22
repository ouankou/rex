import html
import os
import re


_TITLE_RE = re.compile(r"^=\s+(.*)$")
_XREF_RE = re.compile(r"xref:[^\[]+\[([^\]]*)\]")


def _extract_adoc_title(src_path: str) -> str | None:
    try:
        with open(src_path, "r", encoding="utf-8", errors="ignore") as f:
            for _ in range(80):
                line = f.readline()
                if not line:
                    break
                line = line.strip()
                if not line:
                    continue
                m = _TITLE_RE.match(line)
                if not m:
                    continue
                title = m.group(1).strip()
                title = _XREF_RE.sub(lambda mm: mm.group(1), title)
                title = html.unescape(title)
                title = re.sub(r"\s+", " ", title).strip()
                return title or None
    except OSError:
        return None
    return None


def on_page_markdown(markdown, page, config, files):
    src_path = getattr(page.file, "abs_src_path", None)
    if not src_path or not src_path.endswith(".adoc"):
        return markdown
    norm = src_path.replace(os.sep, "/")
    if "/docs/reference/" not in norm:
        return markdown

    title = _extract_adoc_title(src_path)
    if title:
        page.title = title
    return markdown

