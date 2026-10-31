#!/usr/bin/env python3
"""Focused Markdown documentation checks; run directly with Python 3."""
import importlib.util
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[4] / "scripts"


class MarkdownContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.reference = self.root / "reference"
        self.reference.mkdir()
        (self.root / "src").mkdir()
        (self.root / "src" / "Node.h").touch()

    def run_script(self, name, *args):
        return subprocess.run(
            [sys.executable, str(SCRIPTS / name), *map(str, args)],
            capture_output=True, text=True,
        )

    def write_symbol(self, path, content, kind):
        path.write_text(content + f'\n<a data-mrdocs-kind="{kind}"></a>\n')

    def test_declared_in_markdown_and_html(self):
        for declaration in ("`<src/Node.h>`", "`&lt;src/Node.h&gt;`",
                            '<code>&lt;<a href="source">src/Node.h</a>&gt;</code>'):
            with self.subTest(declaration=declaration):
                (self.reference / "Node.md").write_text(f"# Node\nDeclared in {declaration}\n")
                result = self.run_script("validate-docs-output.py", "--root", self.root,
                                         "--reference-dir", self.reference)
                self.assertEqual(result.returncode, 0, result.stderr)
        (self.reference / "Node.md").write_text("Declared in `<missing.h>`")
        self.assertNotEqual(self.run_script("validate-docs-output.py", "--root", self.root,
                                           "--reference-dir", self.reference).returncode, 0)

    def test_return_links_and_unrelated_links(self):
        self.write_symbol(self.reference / "Node.md", "# Node\n", "record")
        function = self.reference / "get.md"
        for signature, success in (
            ('<pre><code class="source-code cpp"><a href="Node.md">Node</a>*\nget();</code></pre>', True),
            ('<pre><code class="source-code cpp">Node*\nget();</code></pre>', False),
            ('<pre><code class="source-code cpp">Node* <a href="Other.md">Other</a>\nget();</code></pre>', False),
            ('```cpp\nNode*\nget();\n```', False),
            ('```cpp\n[Node](Node.md)*\nget();\n```', False),
        ):
            with self.subTest(signature=signature):
                self.write_symbol(function, signature, "function")
                result = self.run_script("validate-reference-return-links.py",
                                         "--reference-dir", self.reference)
                self.assertEqual(result.returncode == 0, success, result.stderr)

    def test_return_links_ignore_documentation_examples(self):
        self.write_symbol(self.reference / "Node.md", "# Node\n", "record")
        self.write_symbol(self.reference / "get.md", '<pre><code class="source-code cpp"><a href="Node.md">Node</a>*\nget();</code></pre>\n'
            '<pre><code class="language-cpp">Node* example = get();</code></pre>\n'
            '```cpp\nNode* example = get();\n```', "function")
        result = self.run_script("validate-reference-return-links.py",
                                 "--reference-dir", self.reference)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_return_links_respect_template_parameter_scope(self):
        for name in ("Name", "Node"):
            self.write_symbol(self.reference / f"{name}.md", f"# {name}\n", "record")
        self.write_symbol(self.reference / "Box.md", '<pre><code class="source-code cpp">template&lt;class Name&gt;\nclass Box;</code></pre>', "record")
        (self.reference / "Box").mkdir()
        member = self.reference / "Box/get.md"
        for result_type, success in (("Name", True), ("Node", False)):
            self.write_symbol(member, '<pre><code class="source-code cpp">std::vector&lt;'
                              + result_type + '*&gt;\nget();</code></pre>', "function")
            result = self.run_script("validate-reference-return-links.py",
                                     "--reference-dir", self.reference)
            self.assertEqual(result.returncode == 0, success, result.stderr)
        member.unlink()
        self.write_symbol(self.reference / "get.md", '<pre><code class="source-code cpp">Name*\nget();</code></pre>', "function")
        result = self.run_script("validate-reference-return-links.py",
                                 "--reference-dir", self.reference)
        self.assertNotEqual(result.returncode, 0, "An unrelated template must not hide missing links")

    def test_return_links_resolve_qualified_identity(self):
        # Deliberately unrelated names collide with external dependency names.
        for filename, title in (
            ("Expr.md", "Expr"), ("Token.md", "Token"),
            ("Owned.md", "Owned"), ("Owned/Expr.md", "Owned::Expr"),
            ("Other.md", "Other"), ("Other/Expr.md", "Other::Expr"),
            ("Owned/Box.md", "Owned::Box"),
            ("Owned/Box/Nested.md", "Owned::Box::Nested"),
        ):
            page = self.reference / filename
            page.parent.mkdir(parents=True, exist_ok=True)
            self.write_symbol(page, '---\ntitle: ' + json.dumps(title) + '\n---\n# ' + title + '\n', "namespace" if title in {"Owned", "Other"} else "record")
        function = self.reference / "Owned/get.md"
        for result_type, success in (
            ("clang::Expr*", True), ("clang::Token*", True),
            ("Fortran::parser::Expr*", True),
            ('<a href="external/">clang</a>::Expr*', True),
            ("Owned::Expr*", False), ("Other::Expr*", False),
            ('<a href="../">Owned</a>::Expr*', False),
            ('Owned::<a href="../Expr/">Expr</a>*', True),
            ('<a href="../">Owned</a>::<a href="../Expr/">Expr</a>*', True),
            ("Expr*", False), ("::Expr*", False),
            ("Box&lt;int&gt;::Nested*", False),
            ('Box&lt;int&gt;::<a href="../Box/Nested/">Nested</a>*', True),
            ('std::vector&lt;Other::Expr*&gt;', False),
            ('std::vector&lt;clang::Expr*&gt;', True),
        ):
            with self.subTest(result_type=result_type):
                self.write_symbol(function, '---\ntitle: "Owned::get"\n---\n'
                                    '<pre><code class="source-code cpp">' + result_type
                                    + '\nget();</code></pre>', "function")
                result = self.run_script("validate-reference-return-links.py",
                                         "--reference-dir", self.reference)
                self.assertEqual(result.returncode == 0, success, result.stderr)
        # Removing the global symbol must not turn unrelated same-basename
        # namespace types into candidates for an unqualified global return.
        function.unlink()
        (self.reference / "Expr.md").unlink()
        self.write_symbol(self.reference / "get.md", '---\ntitle: "get"\n---\n'
            '<pre><code class="source-code cpp">Expr*\nget();</code></pre>', "function")
        result = self.run_script("validate-reference-return-links.py", "--reference-dir", self.reference)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_return_links_template_parameters_do_not_hide_qualified_types(self):
        (self.reference / "Owned").mkdir()
        self.write_symbol(self.reference / "Owned/Name.md", '---\ntitle: "Owned::Name"\n---\n', "record")
        function = self.reference / "get.md"
        for result_type, success in (("Name*", True), ("Name::Nested*", True),
                                     ("Owned::Name*", False), ("::Owned::Name*", False)):
            with self.subTest(result_type=result_type):
                self.write_symbol(function, '<pre><code class="source-code cpp">'
                                    'template&lt;typename Name&gt;\n' + result_type
                                    + '\nget();</code></pre>', "function")
                result = self.run_script("validate-reference-return-links.py",
                                         "--reference-dir", self.reference)
                self.assertEqual(result.returncode == 0, success, result.stderr)

    def test_return_links_use_native_symbol_kinds(self):
        (self.reference / "Owned").mkdir()
        for filename, title, kind in (
            ("vector.md", "vector", "record"),
            ("Owned.md", "Owned", "namespace"),
            ("Owned/vector.md", "Owned::vector", "using"),
            ("Owned/Node.md", "Owned::Node", "record"),
            ("Owned/Direction.md", "Owned::Direction", "enum"),
            ("Owned/Coordinate.md", "Owned::Coordinate", "typedef"),
            ("operator.md", "operator", "record"),
        ):
            self.write_symbol(self.reference / filename,
                              '---\ntitle: ' + json.dumps(title) + '\n---\n', kind)
        function = self.reference / "Owned/get.md"
        for signature, success in (
            ('vector&lt;<a href="../Node/">Node</a>*&gt;\nget();', True),
            ('vector&lt;Node*&gt;\nget();', False),
            ('::vector&lt;int&gt;*\nget();', False),
            ('Direction*\nget();', False),
            ('Coordinate*\nget();', False),
            ('operator <a href="../Node/">Node</a>*() const;', True),
            ('operator Node*() const;', False),
        ):
            with self.subTest(signature=signature):
                self.write_symbol(function, '---\ntitle: "Owned::get"\n---\n'
                                  '<pre><code class="source-code cpp">' + signature
                                  + '</code></pre>', "function")
                result = self.run_script("validate-reference-return-links.py",
                                         "--reference-dir", self.reference)
                self.assertEqual(result.returncode == 0, success, result.stderr)

    def test_return_links_require_symbol_kind_metadata(self):
        (self.reference / "Node.md").write_text('# Node\n')
        self.write_symbol(self.reference / "get.md",
                          '<pre><code class="source-code cpp">'
                          '<a href="Node/">Node</a>*\nget();</code></pre>', "function")
        result = self.run_script("validate-reference-return-links.py", "--reference-dir", self.reference)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Node.md: missing data-mrdocs-kind marker", result.stderr)

    def test_return_links_inner_qualifier_shadows_outer_namespace(self):
        (self.reference / "Owned").mkdir()
        (self.reference / "Other").mkdir()
        for filename, title, kind in (
            ("Other.md", "Other", "namespace"),
            ("Other/Expr.md", "Other::Expr", "record"),
            ("Owned/Other.md", "Owned::Other", "record"),
        ):
            self.write_symbol(self.reference / filename,
                              '---\ntitle: ' + json.dumps(title) + '\n---\n', kind)
        self.write_symbol(self.reference / "Owned/get.md", '---\ntitle: "Owned::get"\n---\n'
                          '<pre><code class="source-code cpp">Other::Expr*\nget();</code></pre>', "function")
        result = self.run_script("validate-reference-return-links.py", "--reference-dir", self.reference)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_symbol_urls_and_labels(self):
        sections = self.reference / "sections"
        sections.mkdir()
        (sections / "types.md").write_text(
            '| [`Node<T>`](../Node.md#overload) | description |\n'
            '| <a href="../../Scope/Node/"><code>Scope::Node</code></a> | |\n'
            '| [MrDocs](https://www.mrdocs.com) | |\n'
        )
        output = self.root / "symbols.json"
        result = self.run_script("generate-api-symbol-index.py", sections, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        symbols = json.loads(output.read_text())["symbols"]
        self.assertEqual(symbols, [
            {"name": "Node<T>", "url": "reference/Node/#overload", "kind": "types"},
            {"name": "Scope::Node", "url": "reference/Scope/Node/", "kind": "types"},
        ])

    def test_site_fragment_anchors(self):
        site = self.root / "site"
        site.mkdir()
        page = site / "index.html"
        (site / "target").mkdir()
        (site / "target" / "index.html").write_text('<h1 id="Node::get">Node</h1><a name="legacy"></a>')
        for link, success in (
            ("#local", True), ("#missing", False),
            ("target/#Node%3A%3Aget", True), ("target/#legacy", True),
            ("target/#missing", False), ("missing/#local", False),
            ("https://example.com/#missing", True), ("//example.com/#missing", True),
            ("ftp://example.com/#missing", True), ("#", True),
        ):
            with self.subTest(link=link):
                page.write_text(f'<h1 id="local">Title</h1><a href="{link}">Link</a>')
                result = self.run_script("validate-site-links.py", "--site-dir", site)
                self.assertEqual(result.returncode == 0, success, result.stderr)

    @unittest.skipUnless(importlib.util.find_spec("yaml"), "PyYAML is needed for reference preparation")
    def test_reference_preparation(self):
        (self.reference / "index.md").write_text(
            '# Reference\n\n## Types\n\n| Name | Description |\n| --- | --- |\n'
            '| <a href="./Node.md"><code>Node</code></a> | A node |\n\n'
            '## Functions\n\n| Name | Description |\n| --- | --- |\n'
            '| <a href="./get.md"><code>get</code></a> | Get a node |\n\n---\nFooter\n')
        (self.reference / "Node.md").write_text('# Node&amp;View\n')
        (self.reference / "get.md").write_text(
            '# get\n\n<pre><code><a href="./Node.md">Node</a>*\nget();</code></pre>\n')
        nav = self.root / "nav.yml"
        nav.write_text('nav:\n  - Types: sections/types.md\n  - Functions: sections/functions.md\n')
        result = self.run_script("prepare-reference.py", "--reference-dir", self.reference,
                                 "--nav-config", nav)
        self.assertEqual(result.returncode, 0, result.stderr)
        types = (self.reference / "sections" / "types.md").read_text()
        functions = (self.reference / "sections" / "functions.md").read_text()
        self.assertIn('title: "Types"', types)
        self.assertIn('href="../../Node/"', types)
        self.assertNotIn('Footer', types)
        self.assertIn('ref-signature', functions)
        self.assertIn('Node&#42; get();', functions)
        self.assertIn('href="../../get/"', functions)
        self.assertIn('title: "Node&View"', (self.reference / "Node.md").read_text())
        self.assertIn('href="../Node/"', (self.reference / "get.md").read_text())

    @unittest.skipUnless(importlib.util.find_spec("yaml"), "PyYAML is needed for reference preparation")
    def test_index_suffix_is_not_a_directory_index(self):
        spec = importlib.util.spec_from_file_location("prepare_reference", SCRIPTS / "prepare-reference.py")
        prepare_reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(prepare_reference)
        for source, expected in (
            ("index.md", ""), ("Widget/index.md", "Widget/"),
            ("Widget/get_index.md", "Widget/get_index/"),
            ("Widget/get_ref_index.md", "Widget/get_ref_index/"),
        ):
            with self.subTest(source=source):
                self.assertEqual(prepare_reference.page_url(source), expected)

    @unittest.skipUnless(importlib.util.find_spec("yaml"), "PyYAML is needed for reference preparation")
    def test_preparation_rejects_output_collisions(self):
        (self.reference / "index.md").write_text("# Reference\n")
        (self.reference / "Node").mkdir()
        (self.reference / "Node/index.md").write_text("# index\n")
        (self.reference / "Node/index-symbol.md").write_text("# existing\n")
        result = self.run_script("prepare-reference.py", "--reference-dir", self.reference)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("collides", result.stderr)
        self.assertEqual((self.reference / "Node/index-symbol.md").read_text(), "# existing\n")

    @unittest.skipUnless(os.environ.get("MRDOCS_BIN"), "set MRDOCS_BIN for real generator coverage")
    def test_real_generator(self):
        source = self.root / "src" / "fixture.cpp"
        shutil.copyfile(Path(__file__).with_name("rex_docs_markdown.cpp"), source)
        dependency = Path(__file__).with_name("rex_docs_markdown_dependency.hpp")
        shutil.copyfile(dependency, self.root / dependency.name)
        database = self.root / "compile_commands.json"
        database.write_text(json.dumps([{
            "directory": str(self.root), "file": str(source),
            "arguments": ["clang++", "-std=c++20", "-I", str(self.root), "-c", str(source)],
        }]))
        config = self.root / "mrdocs.yml"
        config.write_text(
            "generator: md\nmultipage: true\nconcurrency: 1\nsource-root: .\n"
            "input: [src]\noutput: reference\ncompilation-database: compile_commands.json\n"
            f"addons-supplemental: [{json.dumps(str(SCRIPTS.parent / 'docs/mrdocs-addons'))}]\n")
        import yaml
        policy = yaml.safe_load((SCRIPTS.parent / "docs/mrdocs.yml").read_text())
        with config.open("a") as stream:
            for key in ("inherit-base-members", "inherit-hidden-friends", "exclude-symbols"):
                stream.write(f"{key}: {json.dumps(policy[key])}\n")
        result = subprocess.run([os.environ["MRDOCS_BIN"], "--config", str(config)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = self.run_script("prepare-reference.py", "--reference-dir", self.reference)
        self.assertEqual(result.returncode, 0, result.stderr)
        for filename, kind in (("Docs/vector.md", "using"), ("Docs/Point.md", "record"),
                               ("Docs/Coordinate.md", "typedef"), ("Docs/Direction.md", "enum")):
            self.assertIn(f'data-mrdocs-kind="{kind}"', (self.reference / filename).read_text())
        imported = (self.reference / "Docs/imported_points.md").read_text()
        self.assertIn('href="../Point/">Point</a>', imported)
        self.assertFalse((self.reference / "llvm/APSInt.md").exists())
        self.assertIn("operator&#124;", (self.reference / "Docs/NamedPoint.md").read_text())
        for page in self.reference.rglob("*.md"):
            self.assertNotIn("Declared in <code>&lt;rex_docs_markdown_dependency.hpp", page.read_text())
        self.assertTrue((self.reference / "Docs/Point/index-symbol.md").is_file())
        self.assertFalse((self.reference / "Docs/Point/index.md").exists())
        nested = (self.reference / "Docs/nested_points.md").read_text()
        self.assertIn('href="../Holder/">Holder</a>', nested)
        self.assertIn('href="../Point/">Point</a>', nested)
        self.assertNotIn('>Container</a>', nested)
        point = (self.reference / "Docs/Point.md").read_text()
        self.assertIn('href="index-symbol/"', point)
        self.assertIn('href="get_index/"', point)
        functions = (self.reference / "sections/functions.md").read_text()
        self.assertIn("## Docs", functions)
        self.assertIn("operator&#124;", functions)
        self.assertIn("ref-signature", functions)
        self.assertIn("Holder", (self.reference / "sections/types.md").read_text())
        self.assertIn("Coordinate", (self.reference / "sections/type-aliases.md").read_text())
        # Inline template whitespace must not split Markdown table rows.
        holder = (self.reference / "Docs/Holder.md").read_text()
        self.assertIn("| **T** | Stored type. |", holder)
        if importlib.util.find_spec("markdown"):
            import markdown
            from html.parser import HTMLParser

            class Tables(HTMLParser):
                def __init__(self):
                    super().__init__()
                    self.rows = []
                    self.cell = None

                def handle_starttag(self, tag, attrs):
                    if tag == "tr":
                        self.rows.append([])
                    if tag in ("td", "th"):
                        self.cell = ""

                def handle_data(self, data):
                    if self.cell is not None:
                        self.cell += data

                def handle_endtag(self, tag):
                    if tag in ("td", "th"):
                        self.rows[-1].append(self.cell)
                        self.cell = None

            point_html = markdown.markdown(point, extensions=["tables"])
            self.assertNotIn('href="int i"', point_html)
            self.assertIn('operator&#91;&#93;(int i)', point_html)
            self.assertIn('<h2>Non-Member Functions</h2>', point_html)
            self.assertIn('<h2>Derived Classes</h2>', point_html)
            tables = Tables()
            rendered = markdown.markdown(holder, extensions=["tables", "admonition"])
            self.assertIn('class="admonition note"', rendered)
            self.assertNotIn("[!NOTE]", rendered)
            tables.feed(rendered)
            self.assertIn(["T", "Stored type."], tables.rows)
        for script in ("validate-docs-output.py", "validate-reference-return-links.py"):
            args = ["--reference-dir", self.reference]
            if script == "validate-docs-output.py":
                args += ["--root", self.root]
            result = self.run_script(script, *args)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_empty_output_never_passes(self):
        sections = self.reference / "sections"
        sections.mkdir()
        for name, args in (
            ("validate-docs-output.py", ("--root", self.root, "--reference-dir", self.reference)),
            ("validate-reference-return-links.py", ("--reference-dir", self.reference)),
            ("generate-api-symbol-index.py", (sections, self.root / "symbols.json")),
        ):
            with self.subTest(script=name):
                self.assertNotEqual(self.run_script(name, *args).returncode, 0)


if __name__ == "__main__":
    unittest.main()
