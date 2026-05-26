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
- Freeze the upstream target SHA at sync start; process only that frozen range. Newer upstream commits wait for the next sync.
- Process upstream commits in chronological order and record every upstream SHA before moving on.
- Use one final REX commit for each applied upstream commit or tightly related upstream series. That commit includes both the useful upstream change and all required REX adaptation.
- Do not add later fixup commits for sync-caused build or test regressions. Identify the offending REX sync commit, amend it, rebase later sync commits, and refresh CSV mappings to the final SHAs.
- Never update tests, generated outputs, or reference files to mask a REX bug. Such files may change only when the synced semantics require it and the new output is verified as stable/correct.
- Let the REX pre-commit hook format staged code for applied sync commits. Do not run broad formatting sweeps, and do not format test/reference files unless a verified semantic sync requires it.
- The sync is not complete until the build, focused gates, and full CTest pass without regressions.
- Helper scripts disable normal `.pyc` writes. If you run `py_compile`, use a temp `pycache_prefix` outside the source tree.

## Required Files

- Guide: `docs/upstream-sync/rose-upstream-sync-guide.md`
- Current log: newest non-empty `docs/upstream-sync/rose-*-commits.csv`
- Helper scripts: `.codex/skills/rose-upstream-sync/scripts/`

## Standard Workflow

1. Start from a clean REX branch based on `origin/main`.
2. Ensure the read-only upstream remote exists, fetch it, and freeze the target tip:
   ```bash
   git remote add rose https://github.com/llnl/rose.git 2>/dev/null || true
   git remote set-url --push rose DISABLED_READ_ONLY_UPSTREAM
   git fetch rose weekly --prune
   UPSTREAM_TIP=$(git rev-parse rose/weekly)
   ```
3. Inspect pending commits:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/list_pending.py --upstream-ref "$UPSTREAM_TIP"
   ```
4. For each upstream commit, classify it:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/classify_commit.py <upstream-sha>
   ```
5. Apply exactly one upstream commit or tightly related series at a time, producing one final REX commit:
   - Start each item from a clean worktree. If cherry-pick conflicts occur, finish them into this one adapted commit or abort before moving on.
   - Clean useful commit: `git cherry-pick -n <sha>`, inspect the staged diff, then commit manually so normal REX hooks format staged code.
   - Partially useful commit: `git cherry-pick -n <sha>`, remove dropped hunks, adapt to REX, inspect the staged diff, then commit the upstream content and REX adaptation together.
   - Already present or empty change: record `already-present`, map the existing REX commit when known, and create no new commit.
   - Dropped commit: do not edit code; record the drop reason.
   - Merge commit: never cherry-pick blindly. Usually record integration-only merges as `drop`; manually port only proven unique retained changes.
6. Before every local commit and before PR, run:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/guard_dropped_paths.py --staged
   python3 .codex/skills/rose-upstream-sync/scripts/verify_version.py
   ```
   For code/build/test-affecting picks, also run `cmake --build build -j32` before advancing to unrelated upstream work. Use focused CTest gates after risky commits or small related batches.
7. Record each decision in an untracked scratch ledger under `build/upstream-sync/<sync-name>/` while the branch is still being rewritten. Picked commits must map to their current REX commit SHA. After all code decisions are complete and SHAs are stable, write the tracked `docs/upstream-sync/rose-YYYY-commits.csv` rows in one final ledger commit. If the frozen range crosses a year boundary, create/use the appropriate year CSV for each upstream row.
8. If a later build or test gate fails, identify the offending REX sync commit, amend that commit, rebase later sync commits, rerun the relevant gates, and update affected CSV `REX commit` entries. Do not create a separate regression-fix commit.
9. For release/version commits:
   - record intermediate version-only commits as `drop-superseded`;
   - after the synced range is otherwise complete, apply only the latest upstream version marker to `ROSE_VERSION` and `config/SCM_DATE`;
   - keep all REX version encoding and validation sites consistent with upstream ROSE's current version-numbering rule, without manual post-adjustment;
   - never restore upstream `configure.ac` or `src/frontend/CxxFrontend/EDG_VERSION`.
10. Before final validation, prove that no upstream commit in the sync range was overlooked:
   ```bash
   python3 .codex/skills/rose-upstream-sync/scripts/verify_coverage.py --csv <current-sync-csv> --to-ref "$UPSTREAM_TIP"
   ```
   If the frozen range spans multiple CSV files, verify each touched CSV with explicit range arguments.
11. Validate before PR:
   ```bash
   cmake --build build -j32
   ctest --test-dir build --output-on-failure -j32
   ```
12. If final full CTest finds regressions after all upstream commits have been processed:
   - save the exact failing test list from that full run;
   - identify the first offending REX sync commit using that frozen failing subset;
   - amend the offending commit instead of adding a new fixup commit;
   - rebase later sync commits, refresh affected CSV `REX commit` mappings, and rerun the frozen failing subset, focused/core gates, and final full CTest.
   Only proven unrelated environmental flakes may be documented without rewriting a sync commit.

## Commit Trailers

Use these trailers on every REX commit that applies upstream content:

```text
Upstream-ROSE: <sha>
Sync-Decision: pick|pick-partial|version-tracker
Sync-Log: docs/upstream-sync/rose-2026-commits.csv
```

If one REX commit adapts multiple upstream commits, include one `Upstream-ROSE:` trailer per upstream SHA and record each upstream row with the same REX commit SHA.

Do not add `fixup` or `follow-up` sync commits for problems caused by an earlier sync commit. Rewrite the responsible REX sync commit before the branch is published, then update the sync CSV to the rewritten SHA.

Final full-CTest regressions follow the same rule: freeze the failing tests, find the culprit sync commit, amend it, rebase later commits, and rerun validation until the final branch has no separate regression-fix commits.

## PR Review Rounds

Review comments on a sync PR must be mapped back to ownership before editing, even when the reviewer only names files or symptoms.

- For comments on synced code behavior, identify which REX sync commit introduced the affected lines or behavior using tools such as `git blame`, `git log --follow`, `git show <commit> -- <path>`, `git range-diff origin/main...HEAD`, and targeted reproduction.
- If one sync commit owns the problem, amend that commit, rebase later sync commits, refresh affected CSV `REX commit` mappings, rerun relevant validation, and force-push with lease.
- If several files in one review round map to different sync commits, split the fixes by culprit commit and amend each responsible commit. Do not collapse unrelated sync fixes into a generic review-fix commit.
- If a comment is cross-cutting and cannot honestly be assigned to one upstream sync commit, create a separate REX integration/review commit only when it is independent of any single upstream change.
- Comments about sync scripts, docs, ledger format, validation evidence, or PR text may use a separate review-fix commit.
- PR CI failures are handled like review comments: inspect logs, reproduce locally when possible, map the failure to the owning sync commit, amend/rebase/refresh mappings, and force-push with lease. Use a separate commit only for independent CI infrastructure or documentation fixes.
- Repeat this classification for every review round. New rounds do not weaken the one-commit policy, and repeated comments are not a reason to accumulate fixup commits for sync-caused issues.
