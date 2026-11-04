#!/usr/bin/env python3
"""Compare REX's selected Fortran preprocessing mode with native LLVM Flang."""

import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


MODES = {
    "disabled": [],
    "fopenmp": ["-fopenmp"],
    "simd": ["-fopenmp-simd"],
    "libomp": ["-fopenmp=libomp"],
    "rex_ast": ["--rex-omp-ast-only"],
    "parse_only": ["-rose:openmp:parse_only"],
}
SPECIMENS = {
    "free": "rex_fortran_lexical_free.F90",
    "fixed": "rex_fortran_lexical_fixed.F",
    "macros": "rex_fortran_lexical_macros.F90",
    "keywords": "rex_fortran_lexical_keywords.F90",
}


def run(command, directory, log, environment):
    result = subprocess.run(
        [str(argument) for argument in command],
        cwd=directory,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=180,
    )
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(
            f"Command returned {result.returncode}; see {log}:\n"
            + result.stdout[-8000:]
        )
    return result.stdout


def is_conditional_line(line, form):
    if form == "fixed":
        # The sentinel occupies columns 1-2. Columns 3-5 can contain a
        # statement label, and column 6 can be a continuation character.
        return (
            len(line) >= 2
            and line[0].lower() in ("!", "c", "*")
            and line[1] == "$"
            and all(character.isspace() or character.isdigit()
                    for character in line[2:5])
        )
    text = line.lstrip()
    return text.startswith("!$") and (
        len(text) == 2 or text[2].isspace() or text[2] == "&"
    )


def real_conditional_comments(lines, form):
    marker = "! rex_fortran_lexical_real_comment"
    return {
        index: (text[text.index(marker):], text.index(marker) + 1)
        for index, text in enumerate(lines, 1)
        if is_conditional_line(text, form) and marker in text
    }


def directive_comments(lines):
    marker = "! rex_fortran_lexical_directive_"
    return {
        index: (text[text.index(marker):], text.index(marker) + 1)
        for index, text in enumerate(lines, 1)
        if marker in text
    }


def inactive_corpus_lines(lines):
    # These markers delimit literal #if0 specimens, not arbitrary user CPP.
    # Validate their shape before excluding any line from active ownership;
    # missing cooked text is deliberately not evidence of inactivity here.
    begin = "! rex_fortran_lexical_inactive_begin_"
    end = "! rex_fortran_lexical_inactive_end_"
    inactive = {}
    start = None
    for index, text in enumerate(lines, 1):
        if text.startswith(begin):
            assert start is None, (index, text)
            assert lines[index] == "#if 0", (index, text)
            start = (index, text[len(begin):])
        elif text.startswith(end):
            assert start is not None, (index, text)
            first, name = start
            assert text[len(end):] == name
            assert lines[index - 2] == "#endif", (index, text)
            for position in range(first + 2, index - 1):
                content = lines[position - 1]
                assert content and not content.lstrip().startswith("#")
                inactive[position] = content
            start = None
    assert start is None
    assert inactive, "Every lexical specimen must exercise an inactive block"
    return inactive


def directive_member_lines(lines, inactive):
    return {
        index: text
        for index, text in enumerate(lines, 1)
        if index not in inactive
        and (re.match(r"[!cC*]\$ompx?(?:\s|&|$)", text.lstrip())
             or text.lstrip().startswith("!REX_LEXICAL_"))
    }


def check_checkpoints(directory, source, form, enabled, constructs_ast):
    files = list(directory.glob("*.json"))
    before = [path for path in files if "pre-omp-construction" in path.name]
    after = [path for path in files if "post-omp-construction" in path.name]
    assert len(before) == 1, [path.name for path in files]
    assert len(after) == int(constructs_ast), [path.name for path in files]
    original_lines = source.read_text().splitlines()
    inactive = inactive_corpus_lines(original_lines)
    conditional_lines = {
        index: text
        for index, text in enumerate(original_lines, 1)
        if index not in inactive and is_conditional_line(text, form)
    }
    conditional_comments = real_conditional_comments(original_lines, form)
    expected_directive_comments = directive_comments(original_lines)
    expected_directive_members = directive_member_lines(original_lines, inactive)
    pre = json.loads(before[0].read_text())
    nodes = pre["nodes"]
    assert not any(node["kind"].startswith("SgOmp") for node in nodes)

    groups = set()
    extensions = set()
    skipped_conditional = Counter()
    preserved_comments = Counter()
    preserved_directive_comments = Counter()
    observed_directive_members = Counter()
    preserved_inactive_lines = Counter()
    for node in nodes:
        properties = node["properties"]
        if node["kind"] == "SgPragmaDeclaration":
            family = properties["fortran_directive_family"]
            if family in (1, 2):  # OpenMP / OMPX source groups.
                assert enabled, properties
                position = node["location"]["start"]
                assert Path(position["physical_filename"]).resolve() == source
                line = position["physical_line"]
                # Intervening comments and blank lines have their own source
                # roles; they must not become physical pragma members.
                assert line in expected_directive_members, properties
                assert properties["fortran_directive_raw_text"] == (
                    expected_directive_members[line]
                )
                observed_directive_members[line] += 1
                assert properties["fortran_directive_group_id"]
                if properties["fortran_directive_primary"]:
                    assert properties["fortran_directive_logical_text"]
                    assert properties["fortran_directive_semantic_text"]
                    if family == 2:
                        extensions.add(properties["fortran_directive_semantic_text"])
                    if family == 1:
                        groups.add(properties["fortran_directive_group_id"])
                        record = properties["openmp_producer_semantic_records"][
                            "fortran_exact_semantic_bindings"
                        ]
                        assert record["producer"] == 1  # rex_typed_scope
                        assert not record["bindings"]
                        assert not record["expressions"]

        for record in properties.get("preprocessing", []):
            position = record["file_info"]
            if Path(position["physical_filename"]).resolve() != source:
                continue
            line = position["physical_line"]
            if line in inactive:
                assert record["directive"] == 22, record  # CSkippedToken
                assert record["text"].rstrip("\r\n") == inactive[line], record
                assert position["column"] == 1, record
                preserved_inactive_lines[line] += 1
            if line in conditional_lines:
                # The source corpus keeps these lines outside inactive CPP
                # branches. Its executable part belongs to the enabled AST;
                # a real trailing comment remains a separate source record.
                text = record["text"].rstrip("\r\n")
                if enabled:
                    assert not is_conditional_line(text, form), record
                    assert line in conditional_comments, record
                    comment, column = conditional_comments[line]
                    assert text == comment, record
                    assert position["column"] == column, record
                    preserved_comments[line] += 1
                else:
                    assert text == conditional_lines[line], record
                    skipped_conditional[line] += 1
            if line in expected_directive_comments:
                comment, column = expected_directive_comments[line]
                text = record["text"].rstrip("\r\n")
                if enabled or line not in expected_directive_members:
                    # Enabled directives expose a true trailing comment;
                    # ordinary interstitial comments use this role in either
                    # selected mode.
                    assert text == comment, record
                    assert position["column"] == column, record
                else:
                    # A disabled directive is one opaque source record. Its
                    # nested '!' is not a second comment in that mode, but the
                    # exact raw spelling still proves the marker's location.
                    assert text == original_lines[line - 1], record
                    assert text.count(comment) == 1, record
                    assert position["column"] + text.index(comment) == column
                preserved_directive_comments[line] += 1

    expected_groups = 1 if form == "keywords" else 2
    assert len(groups) == (expected_groups if enabled else 0), groups
    assert preserved_inactive_lines == Counter(
        {line: 1 for line in inactive}
    ), preserved_inactive_lines
    assert observed_directive_members == Counter(
        {line: 1 for line in expected_directive_members} if enabled else {}
    ), observed_directive_members
    assert preserved_directive_comments == Counter(
        {line: 1 for line in expected_directive_comments}
    ), preserved_directive_comments
    if form == "free" and enabled:
        assert "ompx rex_fortran_lexical_macro_opaque" in extensions, extensions
        assert (
            "ompx rex_fortran_lexical_prefix_opaque prefix_continued_payload"
            in extensions
        ), extensions
        assert (
            "ompx rex_fortran_lexical_opaque continued_payload" in extensions
        ), extensions
    if enabled:
        assert preserved_comments == Counter(
            {line: 1 for line in conditional_comments}
        ), preserved_comments
    else:
        assert skipped_conditional == Counter(
            {line: 1 for line in conditional_lines}
        ), skipped_conditional

    counts = Counter()
    if constructs_ast:
        post = json.loads(after[0].read_text())
        counts.update(node["kind"] for node in post["nodes"])
        assert counts["SgOmpParallelStatement"] == 1, counts
        if form != "macros":
            assert counts["SgOmpDoStatement"] == 1, counts
            assert counts["SgOmpReductionClause"] == 1, counts
            assert counts["SgOmpPrivateClause"] == 1, counts
    return {
        "generic_openmp_groups": len(groups),
        "skipped_conditional_lines": dict(skipped_conditional),
        "preserved_conditional_comments": dict(preserved_comments),
        "preserved_directive_comments": dict(preserved_directive_comments),
        "physical_directive_members": dict(observed_directive_members),
        "inactive_source_lines": dict(preserved_inactive_lines),
        "post_construction_kinds": {
            kind: count for kind, count in counts.items() if kind.startswith("SgOmp")
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("translator", type=Path)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("specimens", type=Path)
    parser.add_argument("workdir", type=Path)
    parser.add_argument("--form", choices=SPECIMENS, required=True)
    parser.add_argument("--mode", choices=MODES, required=True)
    args = parser.parse_args()
    source = (args.specimens / SPECIMENS[args.form]).resolve()
    compiler = args.compiler.resolve()
    translator = args.translator.resolve()
    args.workdir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.workdir.resolve()))
    print(f"Fortran lexical preservation evidence: {work}", flush=True)
    environment = os.environ.copy()
    environment.update(OMP_NUM_THREADS="2", OMP_DYNAMIC="FALSE")
    enabled = args.mode != "disabled"
    constructs_ast = enabled and args.mode != "parse_only"

    # Native Flang is only the oracle/backend compiler. REX mode spellings
    # select the same enabled preprocessing semantics through its own pipeline;
    # they are never forwarded to the oracle as REX-specific options.
    # The keyword specimen tests the generic parse-only source contract.
    # Its identity loop permutation has no base-language runtime effect.
    native_flags = ["-fopenmp"] if enabled and args.form != "keywords" else []
    include_flags = ["-I", source.parent]
    original = work / "original"
    run(
        [compiler, *native_flags, *include_flags, source, "-o", original],
        work,
        work / "original-build.log",
        environment,
    )
    expected = run(
        [original], work, work / "original-result.txt", environment
    )
    assert expected, "The native specimen must produce an observable result"
    reports = {}
    for variant in ("native_ast", "json_roundtrip"):
        directory = work / variant
        directory.mkdir()
        output = directory / ("rose_" + source.name)
        command = [
            translator,
            "-rose:skipfinalCompileStep",
            *MODES[args.mode],
            *include_flags,
            "-rose:output",
            output,
        ]
        if variant == "json_roundtrip":
            command.extend(
                [
                    "-rex:ast-json-checkpoint=all",
                    f"-rex:ast-json-dir={directory / 'json'}",
                ]
            )
        command.append(source)
        run(command, directory, directory / "translate.log", environment)
        assert output.is_file() and output.stat().st_size, output
        executable = directory / "translated"
        run(
            [compiler, *native_flags, *include_flags, output, "-o", executable],
            directory,
            directory / "translated-build.log",
            environment,
        )
        observed = run(
            [executable], directory, directory / "translated-result.txt", environment
        )
        assert observed == expected, (expected, observed, directory)
        source_lines = source.read_text().splitlines()
        preserved_source_comments = real_conditional_comments(source_lines, args.form)
        preserved_source_comments.update(directive_comments(source_lines))
        for comment, _ in preserved_source_comments.values():
            assert output.read_text().count(comment) == 1, (comment, output)
        if args.form == "free":
            assert output.read_text().count("rex_fortran_lexical_opaque") == 1
            # Active pragmas unparse their selected, expanded semantics;
            # checkpoint metadata separately verifies exact original spelling.
            extension = (
                "!$ompx rex_fortran_lexical_macro_opaque"
                if enabled else "!REX_LEXICAL_EXTENSION"
            )
            assert output.read_text().count(extension) == 1
            for payload in ("continued_payload", "prefix_continued_payload"):
                # These are separate Fortran identifiers. Substring counting
                # would count the first name again inside the second one.
                occurrences = re.findall(
                    rf"(?<![A-Za-z0-9_]){re.escape(payload)}(?![A-Za-z0-9_])",
                    output.read_text(),
                )
                assert len(occurrences) == 1, (payload, occurrences, output)
        if variant == "json_roundtrip":
            reports[variant] = check_checkpoints(
                directory / "json", source, args.form, enabled, constructs_ast
            )
    (work / "validation.json").write_text(
        json.dumps(
            {"mode": args.mode, "form": args.form, "runtime_output": expected,
             "checkpoints": reports},
            indent=2,
        ) + "\n"
    )


if __name__ == "__main__":
    main()
