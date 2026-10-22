#!/usr/bin/env python3
"""Check example output against actual source ownership and the full-AST flag."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess


def run(command, work):
    work.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(command, cwd=work, text=True, capture_output=True)
    (work / "run.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr


def check_pdf(path):
    pdf = path.read_bytes()
    assert pdf.startswith(b"%PDF-"), pdf[:100]
    rectangles = re.findall(rb"/Rect\s*\[([^\]]+)\]", pdf)
    assert rectangles, path
    for rectangle in rectangles:
        left, bottom, right, top = map(float, rectangle.split())
        assert 0 <= left <= right <= 612, (path, rectangle)
        assert 0 <= bottom <= top <= 792, (path, rectangle)
    return len(re.findall(rb"/Type\s*/Page\b", pdf))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--term-generator", required=True)
    parser.add_argument("--pdf-generator", required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    included = work / "rex_example_included.h"
    included.write_text("struct rex_included_record { int field; };\n")
    source = work / "rex_example_term_input.h"
    source.write_text('#include "rex_example_included.h"\n'
                      'int rex_input_function(void) {\n'
                      '  struct rex_included_record value = {42};\n'
                      '  return value.field;\n'
                      '}\n')
    run([args.term_generator, "-x", "c", "-c", str(source)], work / "terms")
    terms = (work / "terms" / (source.name + ".astTerm.txt")).read_text()
    # AstTerm prints node structure and literal values, not declaration names.
    assert terms.count("SgFunctionDeclaration(") == 1, terms
    assert "SgIntVal:42" in terms, terms
    assert "SgClassDeclaration" not in terms, terms

    page_counts = []
    for mode in ("input", "full"):
        command = [args.pdf_generator, "-x", "c", "-c", str(source)]
        if mode == "full":
            command.insert(1, "-rose:convertFullAST")
        run(command, work / mode)
        page_counts.append(check_pdf(work / mode / (source.name + ".pdf")))
    assert 0 < page_counts[0] < page_counts[1], page_counts

    second_source = work / "rex_example_second_input.c"
    second_source.write_text("int rex_second_input(void) { return 17; }\n")
    # One generator instance renders both inputs. Its page handles must belong
    # to the current document, including when that document is smaller.
    for mode in ("input", "full"):
        command = [args.pdf_generator, "-x", "c", "-c",
                   str(source), str(second_source)]
        if mode == "full":
            command.insert(1, "-rose:convertFullAST")
        output = work / ("multiple-" + mode)
        run(command, output)
        for input_source in (source, second_source):
            assert check_pdf(output / (input_source.name + ".pdf")) > 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
