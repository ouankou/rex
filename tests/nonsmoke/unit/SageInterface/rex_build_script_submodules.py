#!/usr/bin/env python3
"""Check submodule preflight failures before any compiler/build execution."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


build_script = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="rex-submodules-") as directory:
    root = Path(directory)
    (root / "CMakeLists.txt").touch()
    git = root / "git"
    git.write_text(f"#!{sys.executable}\n" + '''
import json, os, sys
args = sys.argv[1:]
with open(os.environ["REX_GIT_CALLS"], "a") as stream:
    stream.write(json.dumps(args) + "\\n")
if args == ["rev-parse", "--is-inside-work-tree"]:
    sys.exit(0)
if args[:2] == ["submodule", "status"]:
    print(os.environ["REX_GIT_STATUS"])
    sys.exit(int(os.environ["REX_GIT_STATUS_EXIT"]))
if args == ["submodule", "update", "--init", "--recursive"]:
    sys.exit(int(os.environ["REX_GIT_UPDATE_EXIT"]))
sys.exit(99)
''')
    git.chmod(0o755)
    calls = root / "calls.jsonl"
    env = dict(os.environ, PATH=f"{root}:{os.environ['PATH']}", NUM_JOBS="1",
               LLVM_REQUIRED_MAJOR="22", LLVM_CONFIG=str(root / "missing-llvm"),
               REX_GIT_CALLS=str(calls))
    for status, status_exit, update_exit, needs_update in (
        (" abc parent", 0, 0, False),
        ("-abc parent", 0, 0, True),
        (" abc parent\n-def parent/child", 0, 0, True),
        ("", 7, 0, False),
        ("-abc parent", 0, 8, True),
    ):
        calls.write_text("")
        result = subprocess.run(
            ["bash", str(build_script), str(root / "install")], cwd=root,
            env=dict(env, REX_GIT_STATUS=status, REX_GIT_STATUS_EXIT=str(status_exit),
                     REX_GIT_UPDATE_EXIT=str(update_exit)),
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
        recorded = [json.loads(line) for line in calls.read_text().splitlines()]
        expected = [["rev-parse", "--is-inside-work-tree"],
                    ["submodule", "status", "--recursive"]]
        if needs_update:
            expected.append(["submodule", "update", "--init", "--recursive"])
        assert recorded == expected, (recorded, result.stdout)
        assert result.returncode != 0, result.stdout
        reached_llvm = "[2/5]" in result.stdout
        assert reached_llvm == (status_exit == update_exit == 0), result.stdout
        if status_exit:
            assert "unable to inspect git submodules" in result.stdout, result.stdout
        if update_exit:
            assert "Submodules initialized." not in result.stdout, result.stdout
