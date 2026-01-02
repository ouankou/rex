"""Sphinx configuration for the REX documentation site."""
from __future__ import annotations

import os
import sys
from pathlib import Path

import exhale.graph
import exhale.utils

project = "REX"
author = "REX contributors"

_HERE = Path(__file__).resolve().parent
_REPO_ROOT = _HERE.parent.parent
_DOXYGEN_XML = _REPO_ROOT / "docs" / "doxygen-xml" / "xml"

version = ""
release = ""
_version_file = _REPO_ROOT / "ROSE_VERSION"
if _version_file.exists():
    version = _version_file.read_text(encoding="utf-8").strip()
    release = version

extensions = [
    "myst_parser",
    "breathe",
    "exhale",
    "sphinx.ext.autosectionlabel",
    "sphinx.ext.githubpages",
    "sphinx.ext.todo",
    "sphinx.ext.napoleon",
    "sphinx.ext.doctest",
    "sphinx_copybutton",
]

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

source_suffix = {
    ".md": "markdown",
    ".rst": "restructuredtext",
}

myst_enable_extensions = [
    "colon_fence",
    "linkify",
    "substitution",
    "tasklist",
]
myst_heading_anchors = 3

html_theme = "sphinx_book_theme"
html_static_path = ["_static"]
html_title = "REX Documentation"
html_show_sourcelink = False
html_theme_options = {
    "path_to_docs": "docs/sphinx",
    "repository_url": "https://github.com/ouankou/rex",
    "use_repository_button": False,
    "use_issues_button": False,
    "use_download_button": False,
    "home_page_in_toc": False,
    "show_navbar_depth": 2,
}

primary_domain = "cpp"
highlight_language = "cpp"

breathe_projects = {
    "rex": str(_DOXYGEN_XML),
}
breathe_default_project = "rex"
# Removed breathe_default_members to prevent duplicate C++ declarations
# (see issue #70, item 4)

exhale_args = {
    "containmentFolder": str(_HERE / "api"),
    "rootFileName": "library_root.rst",
    "rootFileTitle": "C++ API Reference",
    "doxygenStripFromPath": str(_REPO_ROOT),
    "createTreeView": True,
    "minifyTreeView": False,  # Prevent line-length warnings (issue #70, 7C)
    # Show members on class/struct/namespace/file pages, but keep breathe_default_members
    # removed to prevent parent classes from duplicating nested class member docs (issue #70, item 4).
    # Include namespace and file to preserve free functions, variables, and typedefs in API docs.
    "customSpecificationsMapping": exhale.utils.makeCustomSpecificationsMapping(
        lambda kind: [":members:", ":undoc-members:"] if kind in ("class", "struct", "namespace", "file") else []
    ),
}

autosectionlabel_prefix_document = True
todo_include_todos = True

# Work around incomplete Doxygen XML entries; keep this local for easy removal later.
def _apply_exhale_xml_workarounds():
    """Apply temporary Exhale workarounds for missing/invalid Doxygen XML."""
    orig_file_post_process = exhale.graph.ExhaleRoot.filePostProcess
    orig_node_compound_xml_contents = exhale.utils.nodeCompoundXMLContents
    missing_refids = set()

    def _node_compound_xml_contents_with_placeholder(node):
        contents = orig_node_compound_xml_contents(node)
        if contents is None:
            if node.refid not in missing_refids:
                sys.stderr.write(
                    f"[exhale] Missing XML for refid {node.refid}; inserting placeholder.\n"
                )
                missing_refids.add(node.refid)
            kind = getattr(node, "kind", "file")
            return (
                "<doxygen>"
                f"<compounddef id=\"{node.refid}\" kind=\"{kind}\"></compounddef>"
                "</doxygen>"
            )
        return contents

    def _skip_files_without_soup(self):
        """Work around Doxygen XML entries that lack associated soup data."""
        missing = [f for f in self.files if getattr(f, "soup", None) is None]
        if missing:
            self.files = [f for f in self.files if getattr(f, "soup", None) is not None]
            sys.stderr.write(
                f"[exhale] Skipped {len(missing)} file entries missing XML; continuing.\n"
            )
        return orig_file_post_process(self)

    exhale.utils.nodeCompoundXMLContents = _node_compound_xml_contents_with_placeholder
    exhale.graph.ExhaleRoot.filePostProcess = _skip_files_without_soup


_apply_exhale_xml_workarounds()

if not (_DOXYGEN_XML / "index.xml").exists():
    raise FileNotFoundError(
        f"Doxygen XML not found at {_DOXYGEN_XML}. "
        "Run `doxygen docs/Doxyfile` before building the Sphinx docs."
    )
