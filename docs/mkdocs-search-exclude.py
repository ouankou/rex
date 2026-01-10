import json
import re
import tempfile
from pathlib import Path

_PRE_TAG_RE = re.compile(r"<pre(?![^>]*\bdata-search-exclude\b)([^>]*)>", re.IGNORECASE)
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
        child = urls[i + 1]
        if not child.startswith(url):
            continue
        if len(child) <= len(url):
            continue
        if url.endswith("/"):
            has_children.add(url)
        else:
            if child[len(url)] == "/":
                has_children.add(url)

    setattr(config, "_search_exclude_leaf_urls", set(urls) - has_children)
    return files


def _should_keep_leaf(page):
    src_path = getattr(page.file, "src_path", "")
    if src_path == "index.adoc":
        return True
    if src_path.startswith("sections/"):
        return True
    return False


def on_page_context(context, page, config, nav):
    leaf_urls = getattr(config, "_search_exclude_leaf_urls", set())
    if page.url in leaf_urls and not _should_keep_leaf(page):
        search = page.meta.setdefault("search", {})
        search["exclude"] = True
    return context

# Run early so search exclusions are applied before the search plugin indexes pages.
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
            truncated = text[:_MAX_SEARCH_TEXT]
            last_space = max(
                truncated.rfind(" "),
                truncated.rfind("\n"),
                truncated.rfind("\t"),
                truncated.rfind("\r"),
            )
            if last_space > _MAX_SEARCH_TEXT * 0.8:
                truncated = truncated[:last_space]
            doc["text"] = truncated
            changed = True
    if changed:
        json_str = json.dumps(data, separators=(",", ":"), ensure_ascii=False)
        temp_path = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="w",
                encoding="utf-8",
                dir=path.parent,
                prefix=path.name + ".",
                suffix=".tmp",
                delete=False,
            ) as temp_file:
                temp_file.write(json_str)
                temp_path = Path(temp_file.name)
            try:
                temp_path.replace(path)
                temp_path = None
            except OSError as exc:
                raise OSError(
                    f"Failed to replace search index file '{path}' with temporary file '{temp_path}'."
                ) from exc
        finally:
            if temp_path is not None:
                try:
                    temp_path.unlink()
                except FileNotFoundError:
                    pass


# Ensure this runs after the search plugin writes search_index.json.
on_post_build.mkdocs_priority = -100
