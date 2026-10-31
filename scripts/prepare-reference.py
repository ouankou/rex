#!/usr/bin/env python3
"""Prepare native MrDocs Markdown for MkDocs (navigation and pretty URLs)."""
import argparse
import html
import json
import posixpath
import re
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

import yaml

HREF = re.compile(r'href="([^"]+)"')
SIGNATURE = re.compile(r'<pre><code\b[^>]*>(.*?)</code></pre>', re.S)


def plain(text):
    return html.unescape(re.sub(r'<[^>]+>', '', text)).strip()


def page_url(path):
    return path[:-8] if posixpath.basename(path) == 'index.md' else path[:-3] + '/'


def prepare(reference, nav_config):
    pages = {p.relative_to(reference).as_posix(): p.read_text() for p in reference.rglob('*.md')}
    if 'index.md' not in pages or len(pages) < 2:
        raise ValueError('MrDocs must generate index.md and symbol pages')

    # MrDocs can emit Class.md and Class/index.md for a member named index.
    # MkDocs would silently write both to Class/index.html; preserve both pages.
    output_names = {
        name: posixpath.join(posixpath.dirname(name), 'index-symbol.md')
        if name != 'index.md' and posixpath.basename(name) == 'index.md' else name
        for name in pages
    }
    if len(set(output_names.values())) != len(pages):
        raise ValueError('generated symbol filename collides with index-symbol.md')

    # Keep the existing category landing pages, without a second MrDocs pass.
    nav = [next(iter(item.items())) for item in yaml.safe_load(nav_config.read_text())['nav']]
    categories = {title for title, target in nav if target.startswith('sections/')}
    sections = {title: [] for title in categories}
    namespace = ''
    for heading in re.finditer(r'^#{2,3} ([^\n]+)\n(.*?)(?=^#{2,3} |^---[ \t]*$|\Z)', pages['index.md'], re.M | re.S):
        title, body = heading.groups()
        if title not in categories:
            if title.endswith(' namespace'):
                namespace = plain(title[:-len(' namespace')])
            continue
        if namespace:
            body = f'## {namespace}\n\n' + body.lstrip()
        sections[title].append(body.strip())
    for title, target in nav:
        if not target.startswith('sections/'):
            continue
        if target in pages:
            raise ValueError(f'category page would overwrite a symbol: {target}')
        body = '\n\n'.join(sections[title]) or f'No {title.lower()} extracted.'
        body = HREF.sub(lambda m: f'href="../{m[1]}"' if not urlsplit(m[1]).scheme and not m[1].startswith('#') else m[0], body)
        pages[target] = f'# {title}\n\n{body.strip()}\n'
        output_names[target] = target

    # Preserve the useful compact first signature in function listings. This is
    # presentation only: MrDocs itself resolves all signature type links.
    for name, text in list(pages.items()):
        in_functions = name == 'sections/functions.md'
        lines = []
        for line in text.splitlines():
            if line.startswith('#') and name != 'sections/functions.md':
                in_functions = bool(re.match(r'^#+ .*Functions$', line))
            if in_functions and line.startswith('| '):
                link = HREF.search(line)
                if link:
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(name), urlsplit(link[1]).path))
                    match = SIGNATURE.search(pages.get(target, ''))
                    if match:
                        signature = ' '.join(plain(match[1]).split())
                        signature = html.escape(signature)
                        # Inline HTML inside Markdown tables still passes through
                        # Markdown's inline parser (notably operator[](arg)).
                        signature = re.sub(r'[\\_*`\[\]|]', lambda m: f'&#{ord(m[0])};', signature)
                        end = line.find('</a>', link.end()) + len('</a>')
                        line = line[:end] + f'<br><span class="ref-signature"><code>{signature}</code></span>' + line[end:]
            lines.append(line)
        pages[name] = '\n'.join(lines) + '\n'

    for name, text in pages.items():
        def pretty_link(match):
            url = urlsplit(html.unescape(match[1]))
            if url.scheme or url.netloc or not url.path.endswith('.md'):
                return match[0]
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), url.path))
            if target not in pages:
                raise ValueError(f'{name}: missing MrDocs target {target}')
            relative = posixpath.relpath(page_url(output_names[target]) or '.', page_url(output_names[name]) or '.') + '/'
            return 'href="' + html.escape(urlunsplit(('', '', relative, url.query, url.fragment)), quote=True) + '"'
        text = HREF.sub(pretty_link, text)
        heading = re.search(r'^# (.+)$', text, re.M)
        title = plain(heading[1]) if heading else Path(name).stem
        path = reference / output_names[name]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('---\ntitle: ' + json.dumps(title) + '\n---\n\n' + text)
        if output_names[name] != name:
            (reference / name).unlink()
    print(f'Prepared {len(pages)} Markdown reference pages')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-dir', type=Path, required=True)
    parser.add_argument('--nav-config', type=Path, default=Path(__file__).resolve().parent.parent / 'mkdocs-reference.yml')
    args = parser.parse_args()
    prepare(args.reference_dir, args.nav_config)
