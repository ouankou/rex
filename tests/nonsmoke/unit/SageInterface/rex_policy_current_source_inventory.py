#!/usr/bin/env python3
"""Policies must inspect current owned source and fail on unavailable inventory."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    policy = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="rex-policy-current-source-") as temporary:
        root = Path(temporary)
        repo = root / "checkout"
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        (repo / "new.c").write_text("int\tnew_source;\n")

        def check(directory, expected, diagnostic=""):
            result = subprocess.run(["perl", str(policy)], cwd=directory, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode != expected or diagnostic not in result.stdout:
                raise AssertionError((directory, result.returncode, result.stdout))

        check(repo, 1, "new.c")
        files = subprocess.check_output(
            ["perl", "-I", str(policy.parent), "-MFileLister", "-e",
             'print "$_\\n" for FileLister->new(@ARGV)->all_files()', "--", "--"],
            cwd=repo, text=True,
        ).splitlines()
        if "./new.c" not in files:
            raise AssertionError(("end-of-options omitted owned source", files))
        subprocess.run(["git", "-C", str(repo), "add", "new.c"], check=True)
        check(repo, 1, "new.c")
        (repo / "new.c").write_text("int new_source;\n")
        (repo / ".gitignore").write_text("ignored/\n")
        ignored = repo / "ignored"
        ignored.mkdir()
        (ignored / "invalid.c").write_text("int\tignored;\n")
        (ignored / "cycle").symlink_to(repo, target_is_directory=True)
        check(repo, 0)
        github = repo / ".github"
        github.mkdir()
        (github / "fixture.c").write_text("int\tgithub_source;\n")
        check(repo, 1, "fixture.c")
        (github / "fixture.c").unlink()
        for path in ("libltdl/invalid.c", "alt-mpi-headers/mpich-4.1/invalid.c"):
            source = repo / path
            source.parent.mkdir(parents=True)
            source.write_text("int\towned_source;\n")
            check(repo, 1, "invalid.c")
            source.unlink()
        # A snapshot nested in a worktree is a separate source inventory.
        archive = ignored / "snapshot"
        archive.mkdir()
        (archive / ".gitmodules").write_text("")
        (archive / "snapshot.c").write_text("int\tsnapshot_source;\n")
        check(archive, 1, "snapshot.c")

        harness = root / "harness"
        (harness / "policies").mkdir(parents=True)
        shutil.copy2(policy.parent.parent / "policies-checker.sh", harness)
        for filename in (policy.name, "FileLister.pm", "Policies.pm"):
            shutil.copy2(policy.parent / filename, harness / "policies")
            if filename.endswith(".pm"):
                (harness / "policies" / filename).chmod(0o644)
        broken = root / "broken"
        broken.mkdir()
        (broken / ".git").write_text("gitdir: missing\n")
        result = subprocess.run(["bash", str(harness / "policies-checker.sh")],
                                cwd=broken, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        if result.returncode != 1 or "Git source inventory failed" not in result.stdout:
            raise AssertionError((result.returncode, result.stdout))
        shutil.rmtree(harness / "policies")
        (harness / "policies").mkdir()
        probe = harness / "policies" / "probe"
        later = harness / "policies" / "zz_after_probe"
        later.write_text("#!/bin/sh\nprintf '%s\\n' rex_later_policy_ran\n")
        later.chmod(0o755)
        for status, expected in ((0, 0), (128, 0), (1, 1), (23, 1), (139, 1), (255, 1)):
            probe.write_text(f"#!/bin/sh\nexit {status}\n")
            probe.chmod(0o755)
            result = subprocess.run(["bash", str(harness / "policies-checker.sh")],
                                    cwd=root, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            if result.returncode != expected:
                raise AssertionError((status, expected, result.returncode, result.stdout))
            if ("rex_later_policy_ran" in result.stdout) != (expected == 0):
                raise AssertionError(("policy execution continued incorrectly", status,
                                      result.stdout))
        print("current-source inventory and policy error propagation passed")


if __name__ == "__main__":
    main()
