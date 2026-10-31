# Final review: acceptance incomplete

The audit is incomplete. PR861 contains the checkpoint and subsequent CI fixes.
Coverage has38 scopes in progress and three retained.
Focused results establish only the tested behavior on their recorded source.

The actual PR workflows pass locally on frozen source794: core794 runs all10,763
selected tests, fragile795 runs all584, and WASM797 builds, runs all eight smoke
cases and assembles the site. All three act jobs exit0. Independent native JUnit
verification finds no missing, duplicate, skipped, disabled or failed result.
Source, submodule, stock-tool and dependency guards pass in the actual Docker
workspace before and after execution. Site artifacts match the tested module.

The current batch preserves brace/copy initialization in JSON (schema251), keeps
instantiated static assertions semantic, and distinguishes written specializations
from semantic copies before constructing source ranges. Isolated semantic
qualification acquires its source prefix only at the publication invariant; the
regex control passes783 in51.06 seconds versus its prior51.56 seconds. A redundant
literal bypass was removed so provenance errors retain their early hard rejection.
Verified stock-Clang diagnostics and typed wrapper-cache expectations are aligned
without weakening the rejection harness or allocation/lifetime checks.

The three user-approved negative contracts remain open defects, not compiler
fixes: [KIND4 substring lowering](https://github.com/ouankou/rex/issues/862),
[declaration-time template binding](https://github.com/ouankou/rex/issues/863),
and [coarray LOCK lowering](https://github.com/ouankou/rex/issues/864).
Their fixtures and positive typed assertions remain. Unexpected success and
unrelated errors fail the negatives. Stock LLVM22 is unchanged.

These results establish the required PR CI gates, not whole-audit acceptance.
Final packaging updates status documents and formats touched lines without
changing code tokens. The diagnostic specimen retains its exact bytes through
the existing formatting ignore list. No functional changes follow the CI runs.

| Required outcome | Current status |
| --- | --- |
| Whole-source cleanup and review | Open; see [coverage](coverage.json) and [remaining work](remaining-work.md). |
| Original class-type/finalization construction failure | Closed on AUD-1382 source: the positive and two intended owner-rejection controls pass. |
| Early typed frontend and reader construction | Open: recursive ownership, rollback, aggregate/constructor semantics, cleanup and property execution. |
| Memory correctness and external exceptions | Open: unclassified reports remain; see [external exceptions](external-exceptions.md). |
| Full Debug and optimized regular CTest | No passing frozen-current-source gates. |
| Full sanitizer and Memcheck gates | No passing frozen-current-source gates. |
| Installation and relocated consumers | Actual native core794 and fragile795 build/install steps pass. Relocated-consumer acceptance remains open. |
| Required PR CI jobs | Pass locally on source794: core10,763/10,763, fragile584/584, WASM8/8 and site assembly; act exits0 for each. |
| Supported configurations, architectures and WASM | PR WASM797 passes on source794 with stock LLVM22.1.8 and Emscripten4.0.22. Broader configuration and architecture gates remain open. |
| Current-source nightly CI | Open; recorded nightly excludes the audit changes. |
| Branch, commit, push and PR | PR861 contains the amended cleanup checkpoint and locally verified CI repairs. |

Source-specific fixes, failures and raw validation evidence belong in
[findings](findings.md). [Remaining work](remaining-work.md) is the open-contract
inventory; [migration](migration.md) records API/schema changes. This review
summarizes acceptance and does not duplicate their test history.

Before merge, freeze the final source/toolchain/configuration identities and
complete every required gate in [README](README.md). Focused passes apply only
to their recorded inputs; interrupted runs and green tests that do not exercise
the claimed behavior are not acceptance.

Resource accounting and active job details are in
`build-audit/RESOURCE_STOP.json`. Follow [AGENTS.md](https://github.com/ouankou/rex/blob/main/AGENTS.md): one serial
job in the existing bounded container, collective RAM below 48,000,000,000 bytes
and disk below 200,000,000,000 bytes. Do not repeat full gates while known blockers
remain.
