---
name: rose-upstream-sync
description: Use when syncing REX with upstream ROSE commits, reviewing upstream ROSE history, updating docs/upstream-sync records, or applying useful LLNL ROSE changes into REX. Enforces read-only upstream access, dropped-subsystem guards, per-commit records, version marker handling, and full CTest validation.
---

# ROSE Upstream Sync For REX

Use this skill for any request mentioning ROSE upstream sync, LLNL ROSE sync, `rose/weekly`, or updating REX from upstream ROSE.

## Hard Rules

- Upstream ROSE is read-only evidence. Allowed: `git fetch`, `git log`, `git show`, `git diff`, local comparisons. Forbidden: pushing, opening PRs/issues/comments, review requests, or mentioning upstream maintainers.
- PRs are only for `ouankou/rex`.
- Never restore abandoned REX components: EDG, OFP/old Fortran parser, binary analysis, Sawyer, CodeThorn, Rosebud source tree, Ada, Jovial, Java/ECJ, PHP, JavaScript, Python, UPC, X10, Csharp, Matlab, YAML/mini-yaml, legacy autotools/Tup paths, or dropped OpenMP designs.
- Process upstream commits in chronological order and record every upstream SHA before moving on.
- The sync is not complete until the build, focused gates, and full CTest pass without regressions.
- Helper scripts disable normal `.pyc` writes. If you run `py_compile`, use a temp `pycache_prefix` outside the source tree.

## Required Files

- Guide: `docs/upstream-sync/rose-upstream-sync-guide.md`
- Current log: newest non-empty `docs/upstream-sync/rose-*-commits.csv`
- Helper scripts: `.codex/skills/rose-upstream-sync/scripts/`

## Standard Workflow

1. Start from a clean REX branch based on `origin/main`.
2. Ensure the read-only upstream remote exists:
   ```bash
   git remote add rose https://github.com/llnl/rose.git 2>/dev/null || true
   git remote set-url --push rose DISABLED_READ_ONLY_UPSTREAM
   git fetch rose weekly --prune
   ```
3. Inspect pending commits:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/list_pending.py
   ```
4. For each upstream commit, classify it:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/classify_commit.py <upstream-sha>
   ```
5. Apply exactly one upstream commit or tightly related series at a time:
   - Clean useful commit: `git cherry-pick -x <sha>`.
   - Partially useful commit: `git cherry-pick -n <sha>`, remove dropped hunks, adapt to REX, then commit.
   - Dropped commit: do not edit code; record the drop reason.
6. Before every local commit and before PR, run:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/guard_dropped_paths.py --staged
   python3 .codex/skills/rose-upstream-sync/scripts/verify_version.py
   ```
7. Record each decision in the current year sync CSV. Picked commits must map to the REX commit SHA.
8. For release/version commits:
   - record intermediate version-only commits as `drop-superseded`;
   - after the synced range is otherwise complete, apply only the latest upstream version marker to `ROSE_VERSION` and `config/SCM_DATE`;
   - never restore upstream `configure.ac` or `src/frontend/CxxFrontend/EDG_VERSION`.
9. Before final validation, prove that no upstream commit in the sync range was overlooked:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/verify_coverage.py --csv docs/upstream-sync/rose-2026-commits.csv
   ```
10. Validate before PR:
   ```bash
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -j32
   ```

## Commit Trailers

Use these trailers on every REX commit that applies upstream content:

```text
Upstream-ROSE: <sha>
Sync-Decision: pick|pick-partial
Sync-Log: docs/upstream-sync/rose-2026-commits.csv
```

If one REX commit adapts multiple upstream commits, include one `Upstream-ROSE:` trailer per upstream SHA and record each upstream row with the same REX commit SHA.
