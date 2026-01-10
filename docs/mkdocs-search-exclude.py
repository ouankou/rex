import json
import re
from pathlib import Path

_PRE_TAG_RE = re.compile(r"<pre(?![^>]*\bdata-search-exclude\b)([^>]*)>", re.IGNORECASE)
_LEAF_URLS = set()
_MAX_SEARCH_TEXT = 100_000


def on_files(files, config):
    urls = []
    for entry in files:
        src_path = getattr(entry, "src_path", "")
        is_doc = False
        if hasattr(entry, "is_documentation_page") and callable(entry.is_documentation_page):
            is_doc = entry.is_documentation_page()
        if not is_doc and not src_path.endswith((".md", ".adoc")):
            continue
        url = getattr(entry, "url", None)
        if url is None:
            continue
        urls.append(url)

    urls = sorted(set(urls))
    has_children = set()
    for i, url in enumerate(urls[:-1]):
        if urls[i + 1].startswith(url):
            has_children.add(url)

    global _LEAF_URLS
    _LEAF_URLS = set(urls) - has_children
    return files


def _should_keep_leaf(page):
    src_path = getattr(page.file, "src_path", "")
    if src_path == "index.adoc":
        return True
    if src_path.startswith("sections/"):
        return True
    return False


def on_page_context(context, page, config, nav):
    if page.url in _LEAF_URLS and not _should_keep_leaf(page):
        search = page.meta.setdefault("search", {})
        search["exclude"] = True
    return context


on_page_context.mkdocs_priority = 100


def on_page_content(html, page, config, files):
    # Exclude code blocks from search while keeping inline code searchable.
    return _PRE_TAG_RE.sub(r"<pre\1 data-search-exclude>", html)


def on_post_build(config):
    path = Path(config.site_dir) / "search" / "search_index.json"
    if not path.is_file():
        return
    data = json.loads(path.read_text(encoding="utf-8"))
    docs = data.get("docs")
    if not isinstance(docs, list):
        return
    changed = False
    for doc in docs:
        if not isinstance(doc, dict):
            continue
        text = doc.get("text")
        if isinstance(text, str) and len(text) > _MAX_SEARCH_TEXT:
            doc["text"] = text[:_MAX_SEARCH_TEXT]
            changed = True
    if changed:
        json_str = json.dumps(data, separators=(",", ":"), ensure_ascii=False)
        temp_path = path.with_name(path.name + ".tmp")
        try:
            temp_path.write_text(json_str, encoding="utf-8")
            temp_path.replace(path)
        except OSError:
            if temp_path.exists():
                try:
                    temp_path.unlink()
                except OSError:
                    pass
            raise


on_post_build.mkdocs_priority = -100
