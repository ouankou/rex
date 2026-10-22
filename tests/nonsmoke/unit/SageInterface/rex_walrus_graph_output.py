"""Check graph structure and repeatability from the real AST exporter."""

import pathlib
import re
import subprocess
import sys
import tempfile


def records(text, field, pattern):
    block = re.search(r"@" + field + r"=\[([^]]*)\]", text)
    assert block is not None, field
    content = block.group(1).strip()
    if not content:
        return []
    assert re.fullmatch(pattern + r"(?:\s*,\s*" + pattern + r")*", content), content
    return re.findall(pattern, content)


def check_graph(path, count, expected_edges):
    text = path.read_text()
    assert re.search(r"@numNodes=(\d+);", text).group(1) == str(count)
    assert re.search(r"@numLinks=(\d+);", text).group(1) == str(len(expected_edges))
    edges = records(text, "links", r"\{ @source=(\d+); @destination=(\d+); \}")
    assert [(int(a), int(b)) for a, b in edges] == expected_edges
    links = records(text, "linkValues", r"\{ @id=(\d+); @value=T; \}")
    assert [int(value) for value in links] == list(range(len(expected_edges)))
    roots = records(text, "nodeValues", r"\{ @id=(\d+); @value=T; \}")
    assert roots == ["0"]
    return text


with tempfile.TemporaryDirectory(prefix="rex_walrus_graph_") as directory:
    subprocess.run([sys.argv[1], directory], check=True)
    root = pathlib.Path(directory)
    first = check_graph(root / "first.graph", 3, [(0, 1), (0, 2)])
    check_graph(root / "single.graph", 1, [])
    assert (root / "repeated.graph").read_text() == first
