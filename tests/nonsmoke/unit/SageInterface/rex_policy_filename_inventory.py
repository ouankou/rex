#!/usr/bin/env python3
"""Require complete current-source inventories and fail on inventory errors."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    policy = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="rex-filename-policy-") as temporary:
        root = Path(temporary)
        archive = root / "archive"
        archive.mkdir()

        def check(directory, expected, diagnostic="", env=None):
            result = subprocess.run(
                ["perl", str(policy)], cwd=directory, env=env,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            if result.returncode != expected or diagnostic not in result.stdout:
                raise AssertionError((directory, expected, result.returncode, result.stdout))

        (archive / "Source.c").touch()
        check(archive, 0)
        (archive / "source.c").touch()
        check(archive, 1, "Source.c source.c")
        (archive / "source.c").unlink()
        nested = archive / "_build_source"
        nested.mkdir()
        (nested / "Header.h").touch()
        (nested / "header.h").touch()
        check(archive, 1, "Header.h header.h")
        (nested / "header.h").unlink()
        # Symlink targets are not part of this tree; a loop must not recurse.
        (archive / "loop").symlink_to(archive, target_is_directory=True)
        check(archive, 0)

        worktree = root / "worktree"
        subprocess.run(["git", "init", "-q", str(worktree)], check=True)
        (worktree / "Tracked.c").touch()
        subprocess.run(["git", "-C", str(worktree), "add", "Tracked.c"], check=True)
        check(worktree, 0)  # No HEAD exists yet.
        (worktree / "tracked.c").touch()
        check(worktree, 1, "Tracked.c tracked.c")
        (worktree / "Tracked.c").unlink()
        check(worktree, 0)  # An unstaged deletion is absent from current source.
        (worktree / ".gitignore").write_text("ignored/\n")
        ignored = worktree / "ignored"
        ignored.mkdir()
        (ignored / "Foo").touch()
        (ignored / "foo").touch()
        check(worktree, 0)
        unusual = worktree / "spaces\nand\ttabs"
        unusual.mkdir()
        (unusual / "Value").touch()
        (unusual / "value").touch()
        check(worktree, 1, "Value value")

        broken = root / "broken-worktree"
        broken.mkdir()
        (broken / ".git").write_text("gitdir: missing-git-directory\n")
        check(broken, 2, "Git source inventory failed")

        # Exercise the actual harness: only status 128 is a documented warning,
        # so the policy must report Git inventory failure as a hard error.
        harness = root / "policy-harness"
        (harness / "policies").mkdir(parents=True)
        shutil.copy2(policy.parent.parent / "policies-checker.sh", harness)
        shutil.copy2(policy, harness / "policies")
        result = subprocess.run(["bash", str(harness / "policies-checker.sh")],
                                cwd=broken, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        if result.returncode != 1 or "Git source inventory failed" not in result.stdout:
            raise AssertionError((result.returncode, result.stdout))

        # A child command that produces an empty stream and exits nonzero must
        # not be interpreted as an empty, successful inventory.
        commands = root / "commands"
        commands.mkdir()
        fake_git = commands / "git"
        fake_git.write_text("#!/bin/sh\nexit 23\n")
        fake_git.chmod(0o755)
        environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"])
        check(worktree, 2, "Git source inventory failed", environment)
        print("filename policy archive/worktree/error contracts passed")


if __name__ == "__main__":
    main()
