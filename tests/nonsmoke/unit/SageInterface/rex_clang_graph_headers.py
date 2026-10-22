#!/usr/bin/env python3
"""Exercise Clang graphing with the primary frontend's headers and language flags."""

from pathlib import Path
import subprocess
import sys
import tempfile


translator, source_dir = map(Path, sys.argv[1:])
for suffix, standard in (("c", "c11"), ("cpp", "c++11")):
    with tempfile.TemporaryDirectory(prefix="rex-clang-graph-") as directory:
        work = Path(directory)
        source = source_dir / f"rex_clang_graph_headers.{suffix}"
        subprocess.run(
            [str(translator), "-rose:clang_il_to_graphviz", f"-std={standard}",
             "-DREX_GRAPH_VALUE=17", "-c", str(source)],
            cwd=work, check=True,
        )
        graph = (work / "clangGraph.dot").read_text()
        assert graph.startswith("digraph {") and graph.rstrip().endswith("}")
        assert "RexGraph" in graph and "rex_graph_count" in graph
        assert "RecordType" in graph and "PredefinedSugarType" in graph

        # Exercise actual compiler include selection. These paths previously
        # passed through a second CMake-to-C++ serialization for graphing.
        include_dirs = [work / name for name in
                        ('first path', 'quote"path', 'back\\slash')]
        for index, include_dir in enumerate(include_dirs):
            include_dir.mkdir()
            (include_dir / "rex_graph_selected.h").write_text(
                f"struct RexGraphSelected{index} {{ int value; }};\n"
            )
        for first in range(len(include_dirs)):
            ordered = include_dirs[first:] + include_dirs[:first]
            subprocess.run(
                [str(translator), "-rose:clang_il_to_graphviz", f"-std={standard}",
                 "-DREX_GRAPH_VALUE=17", "-include", "rex_graph_selected.h",
                 *(argument for path in ordered for argument in ("-I", str(path))),
                 "-c", str(source)],
                cwd=work, check=True,
            )
            graph = (work / "clangGraph.dot").read_text()
            for index in range(len(include_dirs)):
                assert (f"RexGraphSelected{index}" in graph) == (index == first)

with tempfile.TemporaryDirectory(prefix="rex-clang-graph-templates-") as directory:
    work = Path(directory)
    subprocess.run(
        [str(translator), "-rose:clang_il_to_graphviz", "-std=c++20", "-c",
         str(source_dir / "rex_clang_graph_templates.cpp")],
        cwd=work, check=True,
    )
    graph = (work / "clangGraph.dot").read_text()
    assert graph.startswith("digraph {") and graph.rstrip().endswith("}")
    for kind in ("ClassTemplateDecl", "ClassTemplateSpecializationDecl",
                 "ClassTemplatePartialSpecializationDecl", "TypeAliasTemplateDecl",
                 "FunctionTemplateDecl", "VarTemplateDecl", "ConceptDecl",
                 "LambdaExpr", "RecordType", "TemplateArgument"):
        assert kind in graph, kind
    for name in ("RexGraphIntegral", "RexGraphBox", "RexGraphAlias",
                 "rex_graph_initial", "rex_graph_twice", "rex_graph_quoted_text"):
        assert name in graph, name
    # Clang's displayed spelling contains escapes of its own. DOT must retain
    # them inside a single quoted label, including quotes and backslashes.
    assert r'graph \\\"quoted\\\" \\\\ path' in graph
