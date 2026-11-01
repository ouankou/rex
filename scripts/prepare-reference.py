#!/usr/bin/env python3
"""Prepare native MrDocs Markdown for Zensical (navigation and search metadata)."""
import argparse
import html
import json
import posixpath
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote, urlsplit, urlunsplit

import tomllib

HREF = re.compile(r'href="([^"]+)"')
SIGNATURE = re.compile(r'<pre\b[^>]*><code\b[^>]*>(.*?)</code></pre>', re.S)


def plain(text):
    return html.unescape(re.sub(r'<[^>]+>', '', text)).strip()


def page_url(path):
    return path[:-8] if posixpath.basename(path) == 'index.md' else path[:-3] + '/'


def prepare(reference, nav_config, source_url='', source_root=None, source_revision='HEAD'):
    tracked = set()
    if source_url:
        tracked = set(subprocess.check_output(
            ['git', '-C', str(source_root), 'ls-tree', '-rz', '--name-only', source_revision],
            text=True).split('\0'))

    pages = {p.relative_to(reference).as_posix(): p.read_text() for p in reference.rglob('*.md')}
    if 'index.md' not in pages or len(pages) < 2:
        raise ValueError('MrDocs must generate index.md and symbol pages')

    # MrDocs can emit Class.md and Class/index.md for a member named index.
    # Directory URLs would write both to Class/index.html; preserve both pages.
    output_names = {
        name: posixpath.join(posixpath.dirname(name), 'index-symbol.md')
        if name != 'index.md' and posixpath.basename(name) == 'index.md' else name
        for name in pages
    }
    if len(set(output_names.values())) != len(pages):
        raise ValueError('generated symbol filename collides with index-symbol.md')

    # Keep the existing category landing pages, without a second MrDocs pass.
    config = tomllib.loads(nav_config.read_text())['project']
    api_nav = next(item['API'] for item in config['nav'] if 'API' in item)
    nav = [(title, target.removeprefix('reference/'))
           for item in api_nav for title, target in item.items()
           if target.startswith('reference/sections/')]
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

    # Generated headers exist locally but have no GitHub blob. Keep their
    # declaration locations without advertising a broken source link.
    if source_url:
        def source_location(match):
            generated = False
            def anchor(link):
                nonlocal generated
                url = html.unescape(link[1])
                if url.startswith(source_url) and unquote(urlsplit(url[len(source_url):]).path) not in tracked:
                    generated = True
                    return link[2]
                return link[0]
            location = re.sub(r'<a href="([^"]+)">(.*?)</a>', anchor, match[0])
            return location + (' (not stored in Git)' if generated else '')
        pages = {name: re.sub(r'Declared in <code[^>]*>.*?</code>', source_location, text)
                 for name, text in pages.items()}

    # Keep individual overloads connected to their owning overload set.
    for group, text in list(pages.items()):
        if 'data-mrdocs-kind="overloads"' not in text:
            continue
        heading = re.search(r'^# (.+)$', text, re.M)
        if not heading:
            raise ValueError(f'overload group has no heading: {group}')
        title = plain(heading[1])
        for href in HREF.findall(text):
            url = urlsplit(html.unescape(href))
            if url.scheme or url.netloc:
                continue
            target = posixpath.normpath(posixpath.join(posixpath.dirname(group), url.path))
            member = pages.get(target, '')
            member_heading = re.search(r'^# (.+)$', member, re.M)
            if ('data-mrdocs-kind="function"' not in member or not member_heading
                    or plain(member_heading[1]) != title):
                continue
            relative = posixpath.relpath(group, posixpath.dirname(target) or '.')
            link = f'<a href="{html.escape(relative, quote=True)}">All overloads of {html.escape(title)}</a>'
            if link not in member:
                pages[target] = member[:member_heading.end()] + '\n\n' + link + member[member_heading.end():]

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

    urls = sorted(page_url(name) for name in output_names.values())
    parents = {url for url, following in zip(urls, urls[1:]) if following.startswith(url)}
    for name, text in pages.items():
        def source_link(match):
            url = urlsplit(html.unescape(match[1]))
            if url.scheme or url.netloc or not url.path.endswith('.md'):
                return match[0]
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), url.path))
            if target not in pages:
                raise ValueError(f'{name}: missing MrDocs target {target}')
            relative = posixpath.relpath(output_names[target], posixpath.dirname(output_names[name]) or '.')
            return 'href="' + html.escape(urlunsplit(('', '', relative, url.query, url.fragment)), quote=True) + '"'
        text = HREF.sub(source_link, text)
        heading = re.search(r'^# (.+)$', text, re.M)
        title = plain(heading[1]) if heading else Path(name).stem
        path = reference / output_names[name]
        path.parent.mkdir(parents=True, exist_ok=True)
        metadata = 'title: ' + json.dumps(title) + '\n'
        if name != 'index.md' and not name.startswith('sections/') and page_url(output_names[name]) not in parents:
            metadata += 'search:\n  exclude: true\n'
        path.write_text('---\n' + metadata + '---\n\n' + text)
        if output_names[name] != name:
            (reference / name).unlink()
    print(f'Prepared {len(pages)} Markdown reference pages')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-dir', type=Path, required=True)
    parser.add_argument('--nav-config', type=Path, default=Path(__file__).resolve().parent.parent / 'zensical.toml')
    parser.add_argument('--source-url', default='')
    parser.add_argument('--source-revision', default='HEAD')
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    prepare(args.reference_dir, args.nav_config, args.source_url, args.source_root, args.source_revision)
