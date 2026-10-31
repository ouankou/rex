#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
import json
from html.parser import HTMLParser
import re
import sys
from pathlib import Path


# Only generated synopsis blocks, not user-provided documentation examples.
_CODE_BLOCK = re.compile(
    r'<pre\b[^>]*><code class="source-code cpp">(.*?)</code></pre>',
    re.DOTALL | re.MULTILINE,
)
_TOKEN = re.compile(r"[A-Za-z_]\w*|::|[^\s]")
_REAL_AMP = re.compile(r"(?<!&)&(?![A-Za-z]+;)")
_SKIP = {
    "void",
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "signed",
    "unsigned",
    "const",
    "volatile",
    "static",
    "inline",
    "virtual",
    "explicit",
    "constexpr",
    "mutable",
    "typename",
    "class",
    "struct",
    "enum",
    "template",
    "friend",
    "auto",
    "decltype",
    "operator",
}


def _symbol_name(text: str, path: Path) -> str:
    title = re.search(r'^title: (".*")$', text, re.MULTILINE)
    if title:
        name = json.loads(title.group(1))
    else:
        heading = re.search(r"^# (.+)$", text, re.MULTILINE)
        name = html.unescape(re.sub(r"<[^>]*>", "", heading.group(1))) if heading else path.stem
        if "::" not in name:
            name = "::".join((*path.parts[:-1], name))
    # Template arguments do not change the lexical scope name.
    depth = 0
    result = []
    for char in name:
        if char == "<":
            depth += 1
        elif char == ">" and depth:
            depth -= 1
        elif not depth:
            result.append(char)
    return re.sub(r"\s*::\s*", "::", "".join(result)).strip().removeprefix("::")


def _build_symbol_map(reference_dir: Path) -> dict[str, str]:
    mapping = {}
    for path in reference_dir.rglob("*.md"):
        rel = path.relative_to(reference_dir)
        if rel.as_posix() == "index.md" or rel.parts[0] == "sections":
            continue
        text = path.read_text(encoding="utf-8")
        marker = re.search(r'\bdata-mrdocs-kind="([^"]+)"', text)
        if not marker:
            raise ValueError(f"{rel}: missing data-mrdocs-kind marker; regenerate the reference")
        name = _symbol_name(text, rel)
        mapping[name] = marker.group(1)
    return mapping


class _SignatureText(HTMLParser):
    """Keep qualification intact while recording which characters are linked."""
    def __init__(self, markup: str):
        super().__init__(convert_charrefs=True)
        self.text = ""
        self.linked: list[bool] = []
        self.in_link = False
        self.feed(markup)

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.in_link = any(key == "href" and value for key, value in attrs)

    def handle_endtag(self, tag):
        if tag == "a":
            self.in_link = False

    def handle_data(self, data):
        self.text += data
        self.linked.extend([self.in_link] * len(data))


def _template_parameters(text: str) -> set[str]:
    """Read type parameter declarations from a symbol's own first synopsis."""
    match = _CODE_BLOCK.search(text)
    if not match:
        return set()
    signature = html.unescape(re.sub(r"<[^>]*>", "", match.group(1)))
    parameters = set()
    for template in re.finditer(r"\btemplate\s*<", signature):
        depth = 1
        end = template.end()
        while end < len(signature) and depth:
            depth += (signature[end] == "<") - (signature[end] == ">")
            end += 1
        declaration = signature[template.end():end - 1]
        parameters.update(re.findall(r"\b(?:class|typename)\s+(?:\.\.\.\s*)?([A-Za-z_]\w*)", declaration))
    return parameters


def _line_has_unlinked_return_type(line: str, mapping: dict[str, str],
                                  template_parameters: set[str], scope: str = "") -> str | None:
    signature = _SignatureText(line)
    if "*" not in signature.text and not _REAL_AMP.search(signature.text):
        return None
    tokens = list(_TOKEN.finditer(signature.text))
    consumed = set()
    for i, token in enumerate(tokens):
        first = token.group()
        if i in consumed or not re.fullmatch(r"[A-Za-z_]\w*", first) or first in _SKIP:
            continue
        components = [first]
        terminal = token
        j = i + 1
        # Traverse scope qualifiers, leaving template arguments to be checked
        # independently by the outer loop (e.g. Box<Owned*>::Nested*).
        while j < len(tokens):
            if tokens[j].group() == "<":
                depth = 1
                j += 1
                while j < len(tokens) and depth:
                    depth += (tokens[j].group() == "<") - (tokens[j].group() == ">")
                    j += 1
            if j + 1 >= len(tokens) or tokens[j].group() != "::":
                break
            next_token = tokens[j + 1]
            if not re.fullmatch(r"[A-Za-z_]\w*", next_token.group()):
                break
            consumed.add(j + 1)
            components.append(next_token.group())
            terminal = next_token
            j += 2
        absolute = i > 0 and tokens[i - 1].group() == "::"
        if not absolute and first in template_parameters:
            continue
        if all(signature.linked[terminal.start():terminal.end()]):
            continue
        name = "::".join(components)
        scopes = scope.split("::") if scope else []
        prefixes = [""] if absolute else ["::".join(scopes[:n]) for n in range(len(scopes), -1, -1)]
        for prefix in prefixes:
            candidate = f"{prefix}::{name}" if prefix else name
            if candidate in mapping:
                if mapping[candidate] in {"record", "enum", "typedef"}:
                    return candidate
                # A using declaration is an identity, not a locally documented
                # type. It still shadows same-named types in outer scopes.
                break
            # A known inner qualifier shadows an outer one even if the final
            # member is not part of the documented corpus.
            qualifier = f"{prefix}::{first}" if prefix else first
            if qualifier in mapping:
                break
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference-dir", default=None)
    ap.add_argument("--limit", type=int, default=200)
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    reference_dir = (
        Path(args.reference_dir).resolve()
        if args.reference_dir
        else root / "docs" / "reference"
    )
    if not reference_dir.is_dir():
        print(f"reference dir not found: {reference_dir}", file=sys.stderr)
        return 2

    try:
        mapping = _build_symbol_map(reference_dir)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    bad: list[tuple[str, str, str]] = []
    checked = 0

    for path in sorted(reference_dir.rglob("*.md")):
        rel = path.relative_to(reference_dir).as_posix()
        if rel == "index.md" or rel.startswith("sections/"):
            continue
        base = path.name
        if base.startswith("2constructor") or base.startswith("2destructor"):
            continue

        text = path.read_text(encoding="utf-8")
        scope = _symbol_name(text, Path(rel)).rpartition("::")[0]
        parameters = set()
        # Class template parameters are in scope on their member pages, even
        # when unrelated namespace/global symbols happen to have those names.
        for parent in path.relative_to(reference_dir).parents:
            owner = reference_dir / parent.with_suffix(".md") if parent != Path(".") else None
            if owner and owner.is_file():
                parameters.update(_template_parameters(owner.read_text(encoding="utf-8")))
        for match in _CODE_BLOCK.finditer(text):
            local_parameters = parameters | _template_parameters(match.group(0))
            block = match.group(1).splitlines()
            for j, bline in enumerate(block):
                if "(" not in html.unescape(re.sub(r"<[^>]*>", "", bline)):
                    continue
                checked += 1
                prev = next((line.strip() for line in reversed(block[:j]) if line.strip()), None)
                left = prev or bline.split("(", 1)[0].strip()
                tok = _line_has_unlinked_return_type(left, mapping, local_parameters, scope)
                if tok:
                    bad.append((rel, tok, left))

    if not mapping or not checked:
        print("no Markdown symbol pages or C++ signatures found", file=sys.stderr)
        return 1

    if bad:
        print(f"unlinked return types found: {len(bad)}", file=sys.stderr)
        for rel, tok, ctx in bad[: args.limit]:
            print(f"- {rel}: {tok} in '{ctx}'", file=sys.stderr)
        if len(bad) > args.limit:
            print(f"... and {len(bad) - args.limit} more", file=sys.stderr)
        return 1

    print(f"validated return type linking: {checked} signatures checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
