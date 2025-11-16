# System Header Mutation Safety

## Background

`tests/nonsmoke/functional/roseTests/astInterfaceTests/interfaceFunctionCoverage.C`
walks the entire AST and repeatedly calls SageInterface helpers. After the
parent/symbol-table repair work, those helpers now run on *every* declaration,
including the libstdc++ nodes that Clang/GCC synthesize inside headers such as
`/usr/include/c++/14/bits/basic_string.h`. Several of the helpers
(`removeStatement`, `movePreprocessingInfo`, etc.) remove the target statement
from its scope before re‑inserting it, assuming the scope “owns” the node. When
the visitor mutates a declaration that actually belongs to a system header, the
scope’s `StatementListInsertChild()` no longer finds the original node and the
test aborts.

## Short-Term Fix

We hardened the code in two places:

1. The coverage visitor now bails out immediately for nodes whose
   `Sg_File_Info` points at `/usr/include/...`. This keeps the test focused on
   user-owned code while still exercising the SageInterface entry points on
   everything else.
2. `StatementListInsertChild()` logs the missing-target details and simply
   returns when the target originated in a system header. That prevents the new
   parent/symbol cleanup from aborting the entire run when a third-party header
   (libstdc++, libc, etc.) contains local classes or helper declarations.

The combination restores CI parity between GCC 12 (ARM dev box) and GCC 14
(Ubuntu 24.04 CI) without touching the actual system headers.

## Long-Term Direction

The sustainable fix is to clone any declaration that a SageInterface mutator is
about to move/remove when that declaration lives outside the project.
Implementing that cleanly requires:

* a reusable “is this node user-owned?” predicate based on `Sg_File_Info` and
  the `SgProject` file list (no hard-coded `/usr/include` checks),
* a helper that deep-copies the node, relocates it into a project-owned scope,
  and rewires symbols/parents before the mutator runs, and
* an opt-out for advanced users who intentionally edit system headers.

That work is larger (roughly a week of design, implementation, and regression
testing) and will increase compile-time/memory because of the extra copies, but
once it lands we can drop the “skip system headers” guard entirely.

## Known Trade-offs

* **Coverage gap**: we no longer exercise SageInterface helpers on libstdc++
  nodes. The tests still provide the same coverage for user-facing code, but
  they intentionally avoid mutating system headers.
* **Intentional mutations**: projects that *want* to edit `/usr/include`
  declarations can still do so via their own translators. The guard only affects
  the generic coverage visitor and the low-level insert helper—it doesn’t stop
  user code from calling the APIs directly.
