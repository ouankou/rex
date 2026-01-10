import re

_PRE_TAG_RE = re.compile(r"<pre(?![^>]*\\bdata-search-exclude\\b)([^>]*)>", re.IGNORECASE)


def on_page_content(html, page, config, files):
    # Exclude code blocks from search while keeping inline code searchable.
    return _PRE_TAG_RE.sub(r"<pre\1 data-search-exclude>", html)
